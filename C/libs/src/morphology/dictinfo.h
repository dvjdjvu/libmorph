#ifndef __MORPHOLOGY_DICTINFO_H
#define __MORPHOLOGY_DICTINFO_H

#include "helpers.h"

typedef struct {
  char *name;
  Morphology *morphology;
  char *path;
  char *mrd_file_path;
  char *grammar_file_path;
  char *automat_file_path;
} Dictionary;

char *extract_dictionary_name(const char *folder_name);
void free_dictionary(Dictionary *dictionary);
Dictionary *make_dictionary(const char *folder_name, const char *all_dicts_root, size_t description_cache_size);
Dictionary **load_dictionaries(const char *all_dicts_root, size_t *length, size_t description_cache_size);
void free_dictionaries(Dictionary **dictionaries, size_t count);
Morphology *dictionary_morphology(Dictionary *dictionary);
char *dictionary_name(Dictionary *dictionary);

#endif

