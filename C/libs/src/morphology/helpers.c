/* Мелкие вспомогательные функции, упрощающие использование библиотеки
 * морфологии.
 *
 * Автор: Кирилл Маврешко <kimavr@gmail.com> */

#include "morphology/helpers.h"

#include <string.h>
#include <syslog.h>

#include "common/strict_alloc.h"
#include "common/datastruct.h"
#include "common/strtools.h"

/* Работа с кэшем описаний слов, содержащим леммы слова и исходный вариант в
 * форме, удобной для построения документов и поиска по ним */

static void free_cached_description(const void *key, size_t key_size, void *value, void *params) {
  strict_free(value);
}

static HashTable *make_description_cache(size_t cache_size) {
  HashTable *cache = make_hash_table(near_int_log2(cache_size));
  hash_table_fifo_limit(cache, cache_size, free_cached_description, NULL);
  return cache;
}

static void *put_description_to_cache(const void *key, size_t key_size,
                                      const char *description, size_t description_size,
                                      int8_t is_imitation,
                                      HashTable *cache) {
  void *data = strict_malloc(sizeof(description_size) + sizeof(is_imitation) + (description_size + 1)*sizeof(*description));
  void *description_offset = (int8_t *)data + sizeof(description_size) + sizeof(is_imitation);
  memcpy(data, &description_size, sizeof(description_size));
  memcpy((int8_t *)data + sizeof(description_size), &is_imitation, sizeof(is_imitation));
  memcpy(description_offset, description, (description_size + 1) * sizeof(*description));
  hash_table_chain_put(cache, key, key_size, data);
  return description_offset;
}

static char *get_description_from_cache(const void *key, size_t key_size,
                                        size_t *description_size, int8_t *is_imitation,
                                        HashTable *cache) {
  void *data = hash_table_chain_get(cache, key, key_size);
  if (data == NULL) return NULL;
  *description_size = *((size_t *)data);
  *is_imitation = *((int8_t *)data + sizeof(*description_size));
  return (char *)((int8_t *)data + sizeof(*description_size) + sizeof(*is_imitation));
}

static void free_description_cache(HashTable *cache) {
  hash_table_chain_foreach(cache, free_cached_description, NULL);
  free_hash_table(cache);
}

/* Загружает базы морфологии и автомат для анализа слов, объединяя всё в одном
 * объекте, для удобства.
 * dictionary_dir - путь до каталога, содержащего файлы morphs.mrd, gramtab.tab
 *   и automat.save
 * description_cache_size - размер кэша, используемого функцией
 *   make_word_description для кэширование лемм слов. Если число кэшированных лемм
 *   превысит указанное количество, самые старые из них начнут вытесняться.
 * Возвращает NULL если загрузка не удалась.
 */
Morphology *init_morphology_bases(const char *dictionary_dir, size_t description_cache_size) {
  char *mrd_file_name = join_path(2, dictionary_dir, DICTIONARY_MRD_FILE),
      *grammar_file_name = join_path(2, dictionary_dir, DICTIONARY_GRAMMAR_FILE),
      *automat_file_name = join_path(2, dictionary_dir, DICTIONARY_AUTOMAT_FILE);
  void *automat, *base;
  Morphology *morphology;
  base = init_morphology_base(mrd_file_name, grammar_file_name, 1);
  automat = load_mini_automat(automat_file_name);
  strict_free(mrd_file_name);
  strict_free(grammar_file_name);
  strict_free(automat_file_name);
  if (automat == NULL) {
    /* fprintf(stderr, "Automat loading failed.\n"); */
    return NULL;
  }
  if (base == NULL) {
    /* fprintf(stderr, "Morphology base loading failed.\n"); */
    return NULL;
  }
  morphology = strict_malloc(sizeof(*morphology));
  morphology->base = base;
  morphology->automat = automat;
  morphology->automat_output_generator = mini_possible_outputs;
  morphology->automat_common_prefix_size = mini_common_prefix_size;
  morphology->description_cache = make_description_cache(description_cache_size);
  if (pthread_mutex_init(&morphology->mutex, NULL) != 0) {
    return NULL;
  }
  return morphology;
}

/* Блокировка совместного доступа к одному и тому же объекту морфологии из
 * разных потоков. Не рекомендуется к использованию - лучше 
 * создать несколько копий морфологической базы. Создана для крайних случаев,
 * когда по какой-то причине держать несколько копий базы недопустимо.
 *
 * Цель блокировки - избегание конфликтов при параллельной записи в кэш
 * словоформ, а также преждевременного освобождения буферов, хранящих результаты
 * последнего вызова.
 */
static void lock_morphology(Morphology *morphology) {
  /*syslog(LOG_DEBUG, "Lock waiting");*/
  pthread_mutex_lock(&morphology->mutex);
  /*syslog(LOG_DEBUG, "...enter");*/
}

static void unlock_morphology(Morphology *morphology) {
  /*syslog(LOG_DEBUG, "Lock exit");*/
  pthread_mutex_unlock(&morphology->mutex);
}

/* Выгружает базы морфологии и автомат разбора слов из памяти */
void unload_morphology_bases(Morphology *morphology) {
    free_morphology_base(morphology->base);
    free_mini_automat(morphology->automat);
    free_description_cache(morphology->description_cache);
    pthread_mutex_destroy(&morphology->mutex);
    strict_free(morphology);
}

/* Ищет все леммы указанного слова, возвращая их в виде массива */

ArrayList *get_word_lemmas(const wchar_t *word, size_t word_size, Morphology *morphology) {
  return analyze_word(word, word_size, morphology->automat,
                      morphology->automat_output_generator,
                      morphology->base, 1, 0);
}

/* Освобождает память из под найденных get_word_lemmas лемм */
inline void free_word_lemmas(ArrayList *lemmas) { free_analyze_word_results(lemmas); }

/* Ищет все формы указанного слова, возвращая их в виде массива */
inline ArrayList *get_word_forms(const wchar_t *word, size_t word_size, Morphology *morphology) {
  return analyze_word(word, word_size, morphology->automat,
                      morphology->automat_output_generator,
                      morphology->base, 0, 0);
}

/* Освобождает память из под найденных get_word_lemmas лемм */
inline void free_word_forms(ArrayList *lemmas) { free_analyze_word_results(lemmas); }

/* Создаёт "описание слова" - строку, содержащую исходное слово, плюс все его
 * леммы, разделённые точками. Причём исходное слово всегда идёт последним.
 * Такое представление удобно для построения суффиксного массива, в котором
 * можно искать сразу все формы слова. Если слово не имеет лемм, в результат
 * попадает только исходный вариант. Результат работы кэшируется для повторного
 * использования.
 * 1. word и mb_word должны содержать одно и то же слово в "широкой строке" и в
 *    UTF-8 соответственно.
 * 2. Вместо word можно передать NULL, тогда преобразование будет выполнено внутри
 *    функции. mb_word - обязателен.
 * 3. mb_word может не завершаться терминатором '\0'. Размер строки берётся только
 *    из mb_word_length.
 * 4. Параметр dont_imitate задаёт поведение функции в случаях, если слово не
 *    лемматизируется, или вообще представляет собой мусор. В этом случае, можно
 *    потребовать не создавать описание, и вернуть NULL. Кэш при этом также не
 *    занимается.
 * 5. Через аргумент is_garbage возвращается флаг мусорности слова (т.е. оно
 *     вообще ни в один словарь точно не входит).
 *
 * Память, занимаемую возвращаемым значением, надо освобождать.
 */
char *make_word_description(const wchar_t *word, size_t word_length,
                            const char *mb_word, size_t mb_word_length,
                            Morphology *morphology,
                            int dont_imitate,
                            int *is_garbage,
                            size_t *result_length) {
  const char terminator[2] = {WORD_DESCRIPTION_TERMINATOR, '\0'};
  ArrayList *lemmas;
  char *mb_form, *result;
  size_t i, lemmas_count, mb_form_length;
  WordForm *form;
  StringBuffer *result_buffer;
  int8_t is_imitation;
  result = get_description_from_cache(mb_word, mb_word_length, result_length, &is_imitation, morphology->description_cache);
  if (result == NULL) {
    wchar_t *converted_word;
    if (word == NULL) {
      word = converted_word = to_wide_string_exact(mb_word, mb_word_length, &word_length);
    } else {
      converted_word = NULL;
    }
    *is_garbage = is_garbage_word(word, word_length);
    if (!*is_garbage) {
      /* Если слово - не мусор, можно провести лемматизацию. */
      lemmas = get_word_lemmas(word, word_length, morphology);
      lemmas_count = array_list_size(lemmas);
      is_imitation = (lemmas_count == 0);
      if (is_imitation && dont_imitate) {
        result = NULL;
        *result_length = 0;
      } else {
        result_buffer = create_string_buffer();
        for (i = 0; i < lemmas_count; ++i) {
          form = array_list_get(lemmas, i);
          if (wcscmp(form->word, word) != 0) {
            mb_form = to_multibyte_string(form->word, &mb_form_length);
            noclone_append_to_string_buffer(result_buffer, mb_form, mb_form_length);
            append_to_string_buffer(result_buffer, terminator);
          }
        }
        exact_append_to_string_buffer(result_buffer, mb_word, mb_word_length);
        append_to_string_buffer(result_buffer, terminator);
        result = join_string_buffer(result_buffer, result_length);
        free_string_buffer(result_buffer);
        /**original_word = result + *result_length - mb_word_length - 1;*/
        lock_morphology(morphology);
        put_description_to_cache(mb_word, mb_word_length,
                                 result, *result_length,
                                 is_imitation,
                                 morphology->description_cache);
        unlock_morphology(morphology);
      }
      free_word_lemmas(lemmas);
    } else {
      /* Слово - мусорное.
         Бесполезно лемматизировать всё что не похоже на на нормальное слово
         (числа, email'ы и прочее. Поэтому лемматизация просто
         имитируется, чтобы не тратить время и не забивать кэш понапрасну. */
      if (dont_imitate) {
        result = NULL;
        *result_length = 0;
      } else {
        size_t garbage_lemma_byte_size = sizeof(*result) * (mb_word_length + 2);
        result = strict_malloc(garbage_lemma_byte_size);
        memcpy(result, mb_word, garbage_lemma_byte_size - 2);
        result[garbage_lemma_byte_size - 2] = WORD_DESCRIPTION_TERMINATOR;
        result[garbage_lemma_byte_size - 1] = '\0';
        *result_length = garbage_lemma_byte_size - 1;
      }
    }
    strict_free(converted_word);
  } else if (dont_imitate) {
    /* Результат взят из кэша, но имитация запрещена */
    *is_garbage = 0; /* т.к. мусор в кэш просто не заносится */
    result_length = 0;
    result = NULL;
  } else {
    /* Результат взят из кэша */
    result = strict_strndup(result, *result_length);
    *is_garbage = 0;
  }
  return result;
}

/* Возвращает длину часть слова (с конца), на которую его узнаёт автомат анализа
 * данной  морфологии без использования предсказания. Применяется для
 * детектирования языка отдельных слов.  */
size_t known_part_of_word(Morphology *morphology, const wchar_t *word, size_t word_length) {
    wchar_t *reversed_word = strict_malloc(sizeof(wchar_t)*(word_length + 1));
    size_t result;
    wmemcpy(reversed_word, word, word_length);
    wcssubreverse(reversed_word, reversed_word + word_length);
    result = morphology->automat_common_prefix_size(morphology->automat, reversed_word, word_length);
    strict_free(reversed_word);
    return result;
}
