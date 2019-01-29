/* Функции для работы со строками, используемые в разных частях морфологической библиотеки
 *
 * Особенность: Код может неправильно работать на платформах, где размер типа
 * wchar_t меньше 32 бит (Windows). В Linux проблем нет (во FreeBSD тоже не
 * должно быть).
 *
 *  Автор: Кирилл Маврешко <kimavr@gmail.com>
 */

#include "common/strtools.h"

#include <wchar.h>
#include <locale.h>
#include <stdlib.h>
#include <wctype.h>
#include <string.h>
#include <stdarg.h>

#include "common/strict_alloc.h"
#include "common/datastruct.h"

/* Преобразовывает UTF-8-строку text в UNICODE-строку (UTF-32), которая и возвращается.
Возвращённую строку можно использовать повторно, передав её в result при следующем
вызове функции. Её размер будет скорректирован, чтобы вместить результат. */
wchar_t *to_wide_string(const char *text, wchar_t *result, size_t *result_length) {
  char *old_locale = setlocale(LC_CTYPE, MORPHOLOGY_DEFAULT_LOCALE);
  size_t length, buf_size;
  mbstate_t state;
  memset(&state, 0, sizeof(state));
  buf_size = (strlen(text) + 1)*sizeof(wchar_t);
  result = strict_realloc(result, buf_size);
  length = mbsrtowcs(result, &text, buf_size, &state);
  if (length == (size_t)-1) {
    result = strict_realloc(result, sizeof(wchar_t));
    *result = L'\0';
  } else {
    result = strict_realloc(result, (length + 1)*sizeof(wchar_t));
  }
  *result_length = length;
  setlocale(LC_CTYPE, old_locale);
  return result;
}

/* Преобразовывает UTF-8-строку text в UNICODE-строку, которая и
 * возвращается. Отличается от to_wide_string возможностью явно указать длину
 * строки text, что позволяет конвертировать не завершённые терминатором '\0'
 * строки или части строк. */
wchar_t *to_wide_string_exact(const char *text, size_t length, size_t *result_length) {
  char *old_locale = setlocale(LC_CTYPE, MORPHOLOGY_DEFAULT_LOCALE);
  wchar_t *result = strict_malloc(sizeof(*result)*(length + 1));
  mbstate_t state;
  size_t converted;
  memset(&state, 0, sizeof(state));
  converted = mbsnrtowcs(result, &text, length, length, &state);
  if (converted == (size_t)-1) {
    converted = 0;
  }
  result[converted] = L'\0';
  *result_length = converted;
  if (converted < length) {
    result = strict_realloc(result, sizeof(*result)*(converted + 1));
  }
  setlocale(LC_CTYPE, old_locale);
  return result;
}

#include <stdio.h>
/* Преобразует широкосимвольную строку в байтовую (UTF-8). */
char *to_multibyte_string(const wchar_t *text, size_t *result_length) {
  const size_t temp_buffer_size = 1024;
  size_t converted;
  char *result, *old_locale = setlocale(LC_CTYPE, MORPHOLOGY_DEFAULT_LOCALE);
  StringBuffer *buffer = create_string_buffer();
  const wchar_t *text_cursor = text;
  mbstate_t ps;
  memset(&ps, 0, sizeof(ps));
  while (1) {
    char *temp_buffer = strict_malloc(temp_buffer_size * sizeof(char));
    if (temp_buffer == NULL) {
        return NULL;
    }
    converted = wcsrtombs(temp_buffer, &text_cursor, temp_buffer_size - 1, &ps);
    if (converted == 0 || converted == (size_t)-1) {
      strict_free(temp_buffer);
      break;
    } else {
      noclone_append_to_string_buffer(buffer, temp_buffer, converted);
      if (text_cursor == NULL) break;
    }
  }
  result = join_string_buffer(buffer, result_length);
  free_string_buffer(buffer);
  setlocale(LC_CTYPE, old_locale);
  return result;
}

/* Инвертирует подстроку, на начало и конец которой указывают указатели start и end. 
   end должен указывать на следующий за последним инвертируемым символом.
   Например, если инвертируется строка целиком, end должен указывать на символ '\0'.
*/
void wcssubreverse(wchar_t *start, wchar_t *end) {
  wchar_t buf;
  if (start != end) {
    end--;
    do {
      buf = *start;
      *start = *end;
      *end = buf;
      start++;
      end--;
    } while (start < end);
  }
}

/* Инвертирует строку word */
wchar_t *wcsreverse(wchar_t *word) {
  wcssubreverse(word, word + wcslen(word));
  return word;
}

/* Антипод функции wcstoul, приводящий число num в систему счисления
   с основой base, и записывая результат в строку result.
   максимально возможное значение base - 36. 
   Размер буфера должен быть не менее log<base>(2^(8*sizeof(unsigned long))) + 1 символов
*/
wchar_t *ultowcs(unsigned long num, unsigned char base, wchar_t *result) {
  const wchar_t *dictionary = L"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  wchar_t *cursor = result;
  while (num >= base) {
    *cursor = dictionary[num % base];
    num = num / base;
    cursor++;
  }
  *cursor = dictionary[num]; 
  cursor++;
  *cursor = L'\0';
  wcssubreverse(result, cursor);
  return result;
}

/* Преобразует строку к нижнему регистру */
wchar_t *wcslower(wchar_t *string) {
  char *old_locale = setlocale(LC_CTYPE, MORPHOLOGY_DEFAULT_LOCALE);
  wchar_t *cursor;
  for(cursor=string; *cursor; cursor++) {
    *cursor = (wchar_t)towlower((wint_t)*cursor);
  }
  setlocale(LC_CTYPE, old_locale);
  return string;
}

/* Сравнивает две строки, используя wcscmp. 
   Используется в сортировке массивов строк */
int wcs_simple_comparer(const void *data1, const void *data2) {
  return wcscmp(*(const wchar_t **)data1, *(const wchar_t **)data2);
}

/* Обрезает у текста text символы strip_chars в начале и конце. */
void strip_text(char *text, const char *strip_chars) {
  char *start_cursor, *end_cursor;
  size_t new_length;
  if (*text == '\0') return;
  for (start_cursor=text; *start_cursor != '\0' && strchr(strip_chars, *start_cursor) != NULL; start_cursor++);
  for (end_cursor=start_cursor; *end_cursor != '\0'; end_cursor++);
  if (end_cursor != start_cursor) {
    end_cursor--;
    while(strchr(strip_chars, *end_cursor) != NULL) {
      end_cursor--;
    }
  }
  new_length = (size_t)(end_cursor - start_cursor) + 1;
  memmove(text, start_cursor, new_length*sizeof(char));
  text[new_length] = '\0';
}

/* Обрезает у строки line пробельные символы в начале и конце.
   Используется при считывании морфологической базы. */
void strip_line(char *line) {
  strip_text(line, " \t\r\n");
}

/* Функция, проверяющая, может ли данное слово, хотя бы потенциально содержаться
 * в одном из словарей морфологии. Для этого принимается, что слово может
 * состоять только их букв алфавита и нескольких допустимых к вхождению в слово
 * знаков пунктуации (вроде дефисов */
int is_garbage_word(const wchar_t *word, size_t length) {
  /* L'-', L'\'', L'`',*/
  const wchar_t kExtraAllowedInWord[] = {L'-', L'\'', L'`', 0L};
  const wchar_t *next_char;
  size_t i;
  for (i = 0, next_char = word; i < length; ++i, ++next_char) {
    if (!(iswalpha((wint_t)*next_char) || wcsrchr(kExtraAllowedInWord, *next_char))) {
      return 1;
    }
  }
  return 0;
}

char *strict_strndup(const char *text, size_t length) {
  char *result = strict_malloc(length + 1);
  memcpy(result, text, length);
  result[length] = '\0';
  return result;
}

char *join_path(unsigned int chunks_count, ...) {
  const char *kPathDelimiter = "/";
  const size_t kPathDelimiterLength = strlen(kPathDelimiter);
  va_list argument;
  char *chunk, *result;
  size_t i, result_length;
  StringBuffer *buffer = create_string_buffer();
  va_start(argument, chunks_count);
  for (i = 0; i < chunks_count; i++) {
    chunk = strdup(va_arg(argument, char *));
    if (strncmp(chunk, kPathDelimiter, kPathDelimiterLength) == 0) {
      append_to_string_buffer(buffer, kPathDelimiter);
    }
    strip_text(chunk, kPathDelimiter);
    append_to_string_buffer(buffer, chunk);
    if (i < chunks_count - 1) {
      append_to_string_buffer(buffer, kPathDelimiter);
    }
    strict_free(chunk);
  }
  va_end(argument);
  result = join_string_buffer(buffer, &result_length);
  free_string_buffer(buffer);
  return result;
}
