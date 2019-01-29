/* Построение суффиксного массива, используя линейный алгоритм
 * Каркайнена-Сандерса, описанный здесь:
 * http://algo2.iti.kit.edu/sanders/papers/suffix.ps.gz
 * Алгоритм умеет строить суффиксный массив за время O(n), благодаря разбиению
 * данных на триплеты и использованию для них поразрядной сортировки.
 
 * Код взят у автора (http://www.mpi-inf.mpg.de/~sanders/programs/suffix/)
 * он достаточно минималистичен, поэтому я обошёлся переписыванием на C,
 * добавлением комментариев к неясным частям и причёсыванием.
 *
 * Кирилл Маврешко <kimavr@gmail.com>
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "common/strict_alloc.h"

inline int leq_pairs(int a1, int a2, int b1, int b2) { /* lexic. order for pairs */
  return(a1 < b1 || (a1 == b1 && a2 <= b2)); 
}                                                   /* and triples */
inline int leq_triples(int a1, int a2, int a3,   int b1, int b2, int b3) {
  return(a1 < b1 || (a1 == b1 && leq_pairs(a2,a3, b2,b3))); 
}

/* Поразрядная сортировка массива source, которая выполняется не по самим
* значениям source, а по значениям массива keys, индексы элементов которого
* лежат в source. В данном случае, source хранит позиции в тексте, а keys - это
* сам текст.
* Параметр alphabet_range задаёт диапазон значений, которые могут принимать
* элементы keys. Он используется реализации поразрядной сортировки
* подсчётом. Результат работы сохраняется в target, который должен иметь длину
* не меньше source.
*/
static void radixPass(int32_t* source, int32_t* target, const int32_t* keys, int32_t keys_count, int32_t alphabet_range) 
{ /* count occurrences */
  int32_t* c = strict_calloc((size_t)alphabet_range + 1, sizeof(int32_t));                          /* counter array */
  int32_t *cursor, sum, i;
  cursor = source;
  for (i = 0;  i < keys_count;  i++, cursor++) c[keys[*cursor]]++;    /* count occurences */
  cursor = c;
  for (i = 0, sum = 0;  i <= alphabet_range;  i++, cursor++) { /* exclusive prefix sums */
     int32_t t = *cursor;  *cursor = sum;  sum += t;
  }
  cursor = source;
  for (i = 0;  i < keys_count;  i++, cursor++) target[c[keys[*cursor]]++] = source[i];      /* sort */
  strict_free(c);
}

/* Построение суффиксного массива для набора данных s (текст, например), длиной
 * n и с диапазоном значений каждого элемента данных K (размер алфавита).
 * Важно, чтобы массив s был размером n + 3 и содержал не менее n >= 2
 * элементов. Все значения s должны быть положительными.
 */
static void suffix_array(const int32_t* s, int32_t n, int32_t K, int32_t* SA) {
  const int32_t n0=(n+2)/3, n1=(n+1)/3, n2=n/3, n02=n0+n2; 
  int32_t* s12  = strict_malloc(sizeof(int32_t)*((size_t)n02 + 3));
  int32_t* SA12 = strict_malloc(sizeof(int32_t)*((size_t)n02 + 3)); 
  int32_t* s0   = strict_malloc(sizeof(int32_t)*(size_t)n0);
  int32_t* SA0  = strict_malloc(sizeof(int32_t)*(size_t)n0);
  int32_t i, j, p, t, k;
  int32_t name, c0, c1, c2;
  s12[n02] = s12[n02+1] = s12[n02+2]=0;
  SA12[n02] = SA12[n02+1] = SA12[n02+2]=0;
 
  /* generate positions of mod 1 and mod  2 suffixes */
  /* the "+(n0-n1)" adds a dummy mod 1 suffix if n%3 == 1 */
  for (i=0, j=0;  i < n+(n0-n1);  i++) if (i%3 != 0) s12[j++] = i;

  /* lsb radix sort the mod 1 and mod 2 triples */
  radixPass(s12 , SA12, s+2, n02, K);
  radixPass(SA12, s12 , s+1, n02, K);  
  radixPass(s12 , SA12, s  , n02, K);

  /* find lexicographic names of triples */
  name = 0, c0 = -1, c1 = -1, c2 = -1;
  for (i = 0;  i < n02;  i++) {
    if (s[SA12[i]] != c0 || s[SA12[i]+1] != c1 || s[SA12[i]+2] != c2) { 
      name++;  c0 = s[SA12[i]];  c1 = s[SA12[i]+1];  c2 = s[SA12[i]+2];
    }
    if (SA12[i] % 3 == 1) { s12[SA12[i]/3]      = name; } /* left half */
    else                  { s12[SA12[i]/3 + n0] = name; } /* right half */
  }

  /* recurse if names are not yet unique */
  if (name < n02) {
    suffix_array(s12, n02, name, SA12);
    /* store unique names in s12 using the suffix array  */
    for (i = 0;  i < n02;  i++) s12[SA12[i]] = i + 1;
  } else /* generate the suffix array of s12 directly */
    for (i = 0;  i < n02;  i++) SA12[s12[i] - 1] = i; 

  /* stably sort the mod 0 suffixes from SA12 by their first character */
  for (i=0, j=0;  i < n02;  i++) if (SA12[i] < n0) s0[j++] = 3*SA12[i];
  radixPass(s0, SA0, s, n0, K);

  /* merge sorted SA0 suffixes and sorted SA12 suffixes */
  for (p=0,  t=n0-n1,  k=0;  k < n;  k++) {
#define GetI() (SA12[t] < n0 ? SA12[t] * 3 + 1 : (SA12[t] - n0) * 3 + 2)
    int32_t i = GetI(); /* pos of current offset 12 suffix */
    int32_t j = SA0[p]; /* pos of current offset 0  suffix */
    if (SA12[t] < n0 ? 
        leq_pairs(s[i], s12[SA12[t] + n0], s[j], s12[j/3]) :
        leq_triples(s[i],s[i+1],s12[SA12[t]-n0+1], s[j],s[j+1],s12[j/3+n0]))
    { /* suffix from SA12 is smaller */
      SA[k] = i;  t++;
      if (t == n02) { /* done --- only SA0 suffixes left */
        for (k++;  p < n0;  p++, k++) SA[k] = SA0[p];
      }
    } else { 
      SA[k] = j;  p++; 
      if (p == n0)  { /* done --- only SA12 suffixes left */
        for (k++;  t < n02;  t++, k++) SA[k] = GetI(); 
      }
    }  
  } 
  strict_free(s12); strict_free(SA12); strict_free(SA0); strict_free(s0); 
}
#undef GetI

/* Преобразует массив типа char в массив типа int32_t с тремя дополнительными
 * нулевыми элементам. Он используется в качестве "текста" для алгоритма
 * построения суффиксного массива Каркайнена-Сандерса. */
static int32_t *int_text_for_suffix_array(const char *text, size_t text_size) {
  int32_t *int_cursor, i, *int_text;
  const unsigned char *cursor;
  int_text = strict_malloc((text_size + 3)*sizeof(*int_text));
  /* Копируем текст в массив целых */
  for (i=0, int_cursor=int_text, cursor=(const unsigned char *)text;
       i < (int32_t)text_size;
       ++i, ++int_cursor, ++cursor) { *int_cursor = *cursor; }
  /* Обнулим добавочные байты */
  for (i=0; i < 3; ++i, ++int_cursor) *int_cursor = 0;
  return int_text;
}

/* Для указанного текста text размером text_size возвращает суффиксный массив
 * длины text_size */
int32_t *text_to_suffix_array(const char *text, size_t text_size) {
  int32_t *result = strict_malloc(text_size*sizeof(*result));
  if (text_size < 2) {
    if (text_size > 0) *result = 0;
  } else {
    int32_t *int_text = int_text_for_suffix_array(text, text_size);
    suffix_array(int_text, (int32_t)text_size, 1 << 8*sizeof(char), result);
    strict_free(int_text);
  }
  return result;
}

/*int32_t *naive_text_to_suffix_array(const char *text, size_t text_size) {
  int comparer(const void *s1, const void *s2) {
    if (*((int32_t *)s1) == *((int32_t *)s2)) return 0;
    return strcmp(text + *((int32_t *)s1), text + *((int32_t *)s2));
  }
  int32_t i;
  int32_t *result = strict_malloc(text_size*sizeof(*result));
  for (i = 0; i < (int32_t)text_size; ++i) {
    result[i] = i;
  }
  qsort(result, text_size, sizeof(int32_t), comparer);
  return result;
  }*/


/* Ищет все вхождения строки sample в тексте text, используя суффиксный массив
 * suffix_array. Результатом являются указатели start_suffix и end_suffix,
 * указывающие на первый и последний элементы суффиксного массива, хранящие
 * позиции вхождения sample в текст text. */
void find_with_suffix_array(const char *sample, size_t sample_length,
                            const char *text, size_t text_length,
                            const int32_t *suffix_array,
                            const int32_t **start_suffix, const int32_t **end_suffix) {
  *start_suffix = NULL;
  *end_suffix = NULL;
  /* Двоичный поиск по суффиксному массиву. Ищем хотя бы одно вхождение. */
  if (text_length > 0) {
    const int32_t *left, *right, *item;
    int compare_result;
    left = suffix_array; right = suffix_array + text_length - 1;
    do {
      item = left + ((right - left) >> 1);
      compare_result = strncmp(sample, text + *item, sample_length);
      if (compare_result == 0) {
        /* Теперь движемся вперёд-назад чтобы собрать остальное */
        right = left = item;
        while (left - 1 >= suffix_array && strncmp(sample, text + *(left - 1), sample_length) == 0) --left;
        while (right + 1 < suffix_array + text_length && strncmp(sample, text + *(right + 1), sample_length) == 0) ++right;
        *start_suffix = left;
        *end_suffix = right;
        break;
      } else {
        if (compare_result < 0) {
          right = item - 1;
        } else {
          left = item + 1;
        }
      }
    } while (left <= right);
  }
}
