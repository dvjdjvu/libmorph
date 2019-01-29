/* Обёртка вокруг библиотеки морфологии, берущая на себя работу с отдельными
 * словарями и их представлением в файловой системе. Т.е. функции из helpers.c
 * работают с содержимым словаря, а функции из этого модуля отвечают за то, чтобы
 * найти сами словари, убедиться, что в них есть все файлы, и инициализировать
 * уже сами объекты морфоанализа из helpers.c.
 *
 * Автор: Кирилл Маврешко <kimavr@gmail.com> */

#include "morphology/dictinfo.h"

#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>

#include "common/strict_alloc.h"
#include "common/errors.h"
#include "common/strtools.h"

/* Полагая, что folder_name - имя каталога словаря, извлекает из имени название
 * самого словаря. Например, "01ru" преобразуется в "ru". В случае ошибки (не
 * удалось извлечь имя) возвращается NULL. */
char *extract_dictionary_name(const char *folder_name) {
  size_t i, folder_name_len = strlen(folder_name), order_number_len;
  enum {START, DIGIT, NAME} state = START;
  char *result;
  for (i = 0; i < folder_name_len; ++i) {
    char next_char = folder_name[i];
    switch (state) {
      case START:
        if (isdigit(next_char)) {
          state = DIGIT;
          order_number_len = 1;
        } else if (isalpha(next_char)) {
          state = NAME;
          order_number_len = 0;
        } else {
          return NULL;
        }
        break;
      case DIGIT:
        if (isdigit(next_char)) {
          state = DIGIT;
          ++order_number_len;
        } else if (isalpha(next_char)) {
          state = NAME;
        } else {
          return NULL;
        }
        break;
      case NAME:
        if (!isalpha(next_char)) {
          return NULL;
        }
        break;
    }
  }
  result = strict_malloc(sizeof(char)*(folder_name_len - order_number_len + 1));
  strcpy(result, folder_name + order_number_len);
  return result;
}

static int filter_dictionary_folder(const struct dirent *folder) {
  char *dict_name = extract_dictionary_name(folder->d_name);
  if (dict_name == NULL) {
    return 0;
  } else {
    strict_free(dict_name);
  }
  return 1;
}

/* Освобождает ресурсы, занятые make_dictionary */
void free_dictionary(Dictionary *dictionary) {
  strict_free(dictionary->name);
  strict_free(dictionary->path);
  strict_free(dictionary->mrd_file_path);
  strict_free(dictionary->grammar_file_path);
  strict_free(dictionary->automat_file_path);
  if (dictionary->morphology != NULL) {
    unload_morphology_bases(dictionary->morphology);
  }
  strict_free(dictionary);
}

/* Загружает словарь одного языка, находящегося в папке folder_name,
 * расположенной внутри базового каталога словарей all_dicts_root.
 * description_cache_size - размер кэша, используемого функцией для кэширования
 * лемм слов (подробнее см. функцию init_morphology_bases). */
Dictionary *make_dictionary(const char *folder_name, const char *all_dicts_root, size_t description_cache_size) {
  Dictionary *result = strict_calloc(1, sizeof(*result));
  int files_is_ready = 0;
  result->name = extract_dictionary_name(folder_name);
  result->mrd_file_path = join_path(3, all_dicts_root, folder_name, DICTIONARY_MRD_FILE);
  result->path = join_path(2, all_dicts_root, folder_name);
  if (access(result->mrd_file_path, R_OK) == 0) {
    result->grammar_file_path = join_path(3, all_dicts_root, folder_name, DICTIONARY_GRAMMAR_FILE);
    if (access(result->grammar_file_path, R_OK) == 0) {
      result->automat_file_path = join_path(3, all_dicts_root, folder_name, DICTIONARY_AUTOMAT_FILE);
      if (access(result->automat_file_path, R_OK) == 0) {
        /* Можно загружать морфологию */
        files_is_ready = 1;
      } else {
        if (access(result->path, W_OK) == 0) {
          /* Создаём новый автомат */
          fprintf(stderr,
                  "Generating new automat file: %s\n", result->automat_file_path);
          build_automat(result->mrd_file_path, result->grammar_file_path, result->automat_file_path);
          files_is_ready = 1;
        } else {
          fprintf(stderr,
                  "Automat file %s does not exists. And I"
                  " have no write permissions on %s to create one.\n",
                  result->automat_file_path, result->path);
        }
      }
    } else {
      fprintf(stderr, "Can't read grammar file %s.\n", result->grammar_file_path);
    }
  } else {
    fprintf(stderr, "Can't read morphology file %s.\n", result->mrd_file_path);
  }
  if (files_is_ready) {
    result->morphology = init_morphology_bases(result->path, description_cache_size);
    if (result->morphology == NULL) {
      fprintf(stderr, "Can't load dictionary %s. Possible one or more files are corrupted.\n", result->path);
      free_dictionary(result);
      result = NULL;
    }
  } else {
    free_dictionary(result);
    result = NULL;
  }
  return result;
}

/* Загружает все словари, расположенные в общем каталоге all_dicts_root, и
 * возвращает массив загруженных словарей. В переменную, на которую ссылается
 * length, записывается размер этого массива. description_cache_size - размер
 * кэша *каждого словаря*, используемого функцией для кэширования лемм слов
 * (подробнее см. функцию init_morphology_bases). */
Dictionary **load_dictionaries(const char *all_dicts_root, size_t *length, size_t description_cache_size) {
  struct dirent **dict_folder_names;
  size_t valid_dicts;
  int folders_count, i;
  Dictionary *dictionary, **result;
  folders_count = scandir(all_dicts_root, &dict_folder_names, filter_dictionary_folder, alphasort);
  
  errno_assert(folders_count >= 0);
  
  result = strict_malloc(sizeof(*result)*(size_t)folders_count);
  valid_dicts = 0;
  for (i = 0; i < folders_count; i++) {
    dictionary = make_dictionary(dict_folder_names[i]->d_name, all_dicts_root, description_cache_size);
    strict_free(dict_folder_names[i]);
    if (dictionary != NULL) {
      result[valid_dicts++] = dictionary;
    }
  }
  strict_free(dict_folder_names);
  result = strict_realloc(result, sizeof(*result)*valid_dicts);
  *length = valid_dicts;
  return result;
}

/* Освобождает массив словарей, созданный load_dictionaries */
void free_dictionaries(Dictionary **dictionaries, size_t count) {
  size_t i;
  for (i = 0; i < count; ++i) {
    free_dictionary(dictionaries[i]);
  }
  strict_free(dictionaries);
}

Morphology *dictionary_morphology(Dictionary *dictionary) {
  return dictionary->morphology;
}

char *dictionary_name(Dictionary *dictionary) {
  return dictionary->name;
}
