#ifndef __MORPHOLOGY_MULTILANG_H_
#define __MORPHOLOGY_MULTILANG_H_

#include "../common/datastruct.h"
#include "dictinfo.h"

typedef struct {
  Dictionary **languages;
  size_t languages_count;
} MultiMorphology;

MultiMorphology *init_multi_morphology(const char *all_dicts_root, size_t description_cache_size);
void free_multi_morphology(MultiMorphology *instance);
Dictionary *get_dictionary(MultiMorphology *multi_morpher, const char *language_name, size_t language_name_length);
Dictionary *detect_language(MultiMorphology *multi_morpher, const wchar_t *word, size_t word_length);
char *multilang_word_description(MultiMorphology *multi_morpher,
                                       Dictionary *suggested_language,
                                       const wchar_t *word, size_t word_length,
                                       const char *mb_word, size_t mb_word_length,
                                       size_t *result_length, Dictionary **detected_language);
ArrayList *multilang_word_forms(MultiMorphology *multi_morpher,
                                Dictionary *suggested_language,
                                const wchar_t *word, size_t word_length,
                                Dictionary **detected_language);
#endif
