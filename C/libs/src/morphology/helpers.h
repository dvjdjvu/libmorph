#ifndef __MORPHOLOGY_HELPERS_H_
#define __MORPHOLOGY_HELPERS_H_

#include <pthread.h>

#include "miniautomat.h"
#include "wordforms.h"
#include "../common/hashtable.h"

#define WORD_DESCRIPTION_TERMINATOR '.'

#ifdef __cplusplus
extern "C" {
#endif

/* Файлы одного языкового словаря морфологии */
#define DICTIONARY_MRD_FILE "morphs.mrd" /* Основы слов и правила словообразования */
#define DICTIONARY_GRAMMAR_FILE "gramtab.tab" /* Части речи */
#define DICTIONARY_AUTOMAT_FILE "automat.save" /* Автомат разбора и предсказания */
  
typedef struct {
  MiniAutomat *automat;
  MorphologyBase *base;
  AutomatOutputsGenerator automat_output_generator;
  AutomatCommonPrefixSize automat_common_prefix_size;
  HashTable *description_cache;
  pthread_mutex_t mutex;
} Morphology;

/* Загружает базы морфологии и автомат для анализа слов, объединяя всё в одном
 * объекте, для удобства.
 * dictionary_dir - путь до каталога, содержащего файлы morphs.mrd, rgramtab.tab
 *   и automat.save
 * description_cache_size - размер кэша, используемого функцией
 *   make_word_description для кэширование лемм слов. Если число кэшированных лемм
 *   превысит указанное количество, самые старые из них начнут вытесняться.
 * Возвращает NULL если загрузка не удалась.
 */
Morphology *init_morphology_bases(const char *dictionary_dir, size_t description_cache_size);

/* Выгружает базы морфологии и автомат разбора слов из памяти */
void unload_morphology_bases(Morphology *morphology);

/* Ищет все леммы указанного слова, возвращая их в виде массива. */
ArrayList *get_word_lemmas(const wchar_t *word, size_t word_size, Morphology *morphology);

/* Освобождает память из под найденных get_word_lemmas лемм */
void free_word_lemmas(ArrayList *lemmas);

/* Ищет все леммы указанного слова, возвращая их в виде массива. */
ArrayList *get_word_forms(const wchar_t *word, size_t word_size, Morphology *morphology);

/* Освобождает память из под найденных get_word_lemmas лемм */
void free_word_forms(ArrayList *lemmas);

/* Создаёт "описание слова" - строку, содержащую исходное слово, плюс все его
 * леммы, разделённые точками. Причём исходное слово всегда идёт последним.
 * Такое представление удобно для построения суффиксного массива, в котором
 * можно искать сразу все формы слова. Если слово не имеет лемм, в результат
 * попадает только исходный вариант. Результат работы кэшируется для повторного
 * использования.
 */
char *make_word_description(const wchar_t *word, size_t word_length,
                                  const char *mb_word, size_t mb_word_length,
                                  Morphology *morphology,
                                  int dont_imitate,
                                  int *is_garbage,
                                  size_t *result_length);

size_t known_part_of_word(Morphology *morphology, const wchar_t *word, size_t word_length);
  
#ifdef __cplusplus
}
#endif
  
#endif /* __MORPHOLOGY_HELPERS_H_ */
