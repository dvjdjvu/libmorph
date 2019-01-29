/* Основной интерфейс для морфологического анализа сразу на нескольких
 * языках. Позволяет абстрагироваться от словарей (dictinfo.c) и функций
 * морфологического разбора для каждого языка.
 * Схематически, это можно изобразить так:
 *
 *    +--------------------------------------------+
 *    | Многоязычный морфоанализ (multilang.c)     |
 *    |                                            |
 *    | +------------------+  +------------------+ |
 *    | | Словарь языка    |  | Словарь языка    | |
 *    | | (dictinfo.c)     |  | (dictinfo.c)     | |
 *    | | +--------------+ |  | +--------------+ | |
 *    | | | Внутренности | |  | | Внутренности | | |
 *    | | | морфологии   | |  | | морфологии   | | |
 *    | | | (helpers.c)  | |  | | (helpers.c)  | | |
 *    | | +--------------+ |  | +--------------+ | |
 *    | +------------------+  +------------------+ |
 *    +--------------------------------------------+
 *
 * Абстракция позволяет вообще не указывать язык в ходе анализа - он будет
 * определяться автоматически. Но можно и указать его явно, если это имеет смысл
 * (например, ключевая фраза выглядит одинаково на двух родственных языках, но
 * лемматизируется на каждом из них по-своему.
 *
 * Автор: Кирилл Маврешко <kimavr@gmail.com>
 */

#include "morphology/multilang.h"

#include <string.h>

#include "common/strict_alloc.h"
#include "common/strtools.h"
#include "morphology/dictinfo.h"

MultiMorphology *init_multi_morphology(const char *all_dicts_root, size_t description_cache_size) {
  MultiMorphology *result = strict_malloc(sizeof(*result));
  result->languages = load_dictionaries(all_dicts_root, &result->languages_count, description_cache_size);
  return result;
}

void free_multi_morphology(MultiMorphology *instance) {
  free_dictionaries(instance->languages, instance->languages_count);
  strict_free(instance);
}

/* Возвращает словарь конкретного языка, если он был загружен, или
 * NULL. Используется в тестах, когда надо работать со словарём конкретного языка */
Dictionary *get_dictionary(MultiMorphology *multi_morpher, const char *language_name,
                           size_t language_name_length) {
  size_t i;
  Dictionary **language;
  for (i = 0, language = multi_morpher->languages; i < multi_morpher->languages_count; ++i, ++language) {
    if (strncmp(dictionary_name(*language), language_name, language_name_length) == 0) {
      return *language;
    }
  }
  return NULL;
}

/* Определяет язык слова word, возвращая ссылку на соответствующий ему
 * словарь. Если слово совершенно непохоже ни на один язык, возвращается NULL. */
Dictionary *detect_language(MultiMorphology *multi_morpher, const wchar_t *word, size_t word_length) {
  size_t i, known_length, max_known = 0;
  Dictionary **language, *result = NULL;
  if (!is_garbage_word(word, word_length)) {
    for (i = 0, language = multi_morpher->languages; i < multi_morpher->languages_count; ++i, ++language) {
      known_length = known_part_of_word(dictionary_morphology(multi_morpher->languages[i]), word, word_length);
      if (known_length == word_length) {
        return *language;
      }
      if (known_length > max_known) {
        result = *language;
        max_known = known_length;
      }
    }
  }
  return result;
}

static Dictionary *main_language(MultiMorphology *multi_morpher) {
  return (multi_morpher->languages_count > 0 ? multi_morpher->languages[0] : NULL);
}

/* Полный аналог функции get_word_forms (из helpers.c), но умеющий сам
 * определять язык передаваемого слова и искать начальные формы в его
 * контексте.
 * В отличии от get_word_forms, здесь нет аргумента morphology, но можно
 * указать "предпочтительный язык", в контексте которого будет происходить
 * анализ слова. Если слово не относится к этому языку, или предпочтительный
 * язык указан как NULL, будет предпринята попытка автоматического определения
 * языка.
 * Кроме описания слова, функция возвращает и настоящий язык слова, в контексте
 * которого, в результате, проходил анализ (может вернуть NULL, если слово не
 * относится ни к одному языку).
 */
ArrayList *multilang_word_forms(MultiMorphology *multi_morpher,
                                Dictionary *suggested_language,
                                const wchar_t *word, size_t word_length,
                                Dictionary **detected_language) {
  ArrayList *result;
  if (suggested_language == NULL) {
    *detected_language = detect_language(multi_morpher, word, word_length);
    result = get_word_forms(word, word_length,
                            dictionary_morphology(
                                (*detected_language != NULL) ?
                                *detected_language
                                : main_language(multi_morpher)));
    if (array_list_size(result) == 0) {
      *detected_language = NULL;
    }
  } else {
    result = get_word_forms(word, word_length,
                            dictionary_morphology(suggested_language));
    if (array_list_size(result) == 0) {
      *detected_language = detect_language(multi_morpher, word, word_length);
      result = get_word_forms(word, word_length,
                              dictionary_morphology((*detected_language != NULL) ?
                                                    *detected_language
                                                    : main_language(multi_morpher)));
      if (array_list_size(result) == 0) {
        *detected_language = NULL;
      }
    } else {
      *detected_language = suggested_language;
    }
  }
  return result;
}

/* Полный аналог функции make_word_description (из helpers.c), но умеющий сам
 * определять язык передаваемого слова и искать начальные формы в его
 * контексте.
 * В отличии от make_word_description, здесь нет аргумента morphology, но можно
 * указать "предпочтительный язык", в контексте которого будет происходить
 * анализ слова. Если слово не относится к этому языку, или предпочтительный
 * язык указан как NULL, будет предпринята попытка автоматического определения
 * языка.
 * Кроме описания слова, функция возвращает и настоящий язык слова, в контексте
 * которого, в результате, проходил анализ (может вернуть NULL, если слово не
 * относится ни к одному языку).
 */
char *multilang_word_description(MultiMorphology *multi_morpher,
                                 Dictionary *suggested_language,
                                 const wchar_t *word, size_t word_length,
                                 const char *mb_word, size_t mb_word_length,
                                 size_t *result_length, Dictionary **detected_language) {
  int is_garbage_word;
  wchar_t *converted_word;
  const char *result;
  if (word == NULL) {
    word = converted_word = to_wide_string_exact(mb_word, mb_word_length, &word_length);
  } else {
    converted_word = NULL;
  }
  if (suggested_language == NULL) {
    *detected_language = detect_language(multi_morpher, word, word_length);
    result = make_word_description(word, word_length, mb_word, mb_word_length,
                                   dictionary_morphology(
                                       (*detected_language != NULL) ?
                                       *detected_language
                                       : main_language(multi_morpher)),
                                   0,
                                   &is_garbage_word,
                                   result_length);
    if (is_garbage_word) {
      *detected_language = NULL;
    }
  } else {
    result = make_word_description(word, word_length, mb_word, mb_word_length,
                                   dictionary_morphology(suggested_language),
                                   1,
                                   &is_garbage_word,
                                   result_length);
    if (result == NULL) {
      if (!is_garbage_word) {
        *detected_language = detect_language(multi_morpher, word, word_length);
      }
      result = make_word_description(word, word_length, mb_word, mb_word_length,
                                     dictionary_morphology(
                                         is_garbage_word || *detected_language == NULL ?
                                         main_language(multi_morpher)
                                         : *detected_language),
                                     0,
                                     &is_garbage_word,
                                     result_length);
      if (!is_garbage_word) {
        if (*detected_language == suggested_language) {
          /* предсказание выдало уже рекомендованный язык, но слова в словаре
           * не было, и даже предсказание не сработало. Значит, формально, язык
           * остался неизвестным. */
          *detected_language = NULL;
        }
      }
    } else {
      *detected_language = suggested_language;
    }
  }
  strict_free(converted_word);
  return (char *) result;
}
