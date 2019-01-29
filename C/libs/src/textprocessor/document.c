/* Представление документа, удобное для быстрого поиска слов в нём и функции для
 * работы с таким представлением. Используется в поиске ключевиков по документу.
 *
 * Документ содержит оригинальные формы и леммы для каждого слова, суффиксный
 * массив для быстрого поиска и таблицу, сопоставляющего позиции лемм и позиции
 * оригинальных слов,  чтобы можно было определять оригинал и видеть порядок
 * следования слов (для сложных фраз).
 *
 * Общий принцип работы
 * --------------------
 * каждое слово в тексте лемматизируется, и превращается в
 * строку, содержащую все леммы + исходную форму, завершающиеся
 * символом-терминатором. Например: "стать.сталь.стали.". Из таких слов и
 * составляется конечный текст для поиска.
 * Чтобы не забыть, что кусок "стать.сталь.стали." на самом деле отражает одно
 * слово, строится список из объектов-диапазонов WordRange, хранящих начало
 * куска, его конец, порядковый номер слова в тексте и смещение, по которому
 * можно скопировать исходную форму, каковой она была в первоначальном тексте до
 * обработки.
 * 
 * Теперь мы можем искать по такому тексту целые фразы, игнорируя
 * морфологическую форму слов. Сначала мы находим все вхождения первого слова
 * фразы, потом вхождения второго слова, но уже только те, которые по порядку
 * следования идут за первым словом, потом вхождения третьего, но только те,
 * которые по порядку следования идут за вторым...
 *
 * Автор: Кирилл Маврешко <kimavr@gmail.com>
 */

#include "textprocessor/document.h"

#include <string.h>
#include <wchar.h>

#include "common/strict_alloc.h"
#include "common/strtools.h"
#include "common/datastruct.h"
#include "textprocessor/tokenizer.h"
#include "textprocessor/suffix.h"
#include "morphology/helpers.h"

/* Делает "нормализацию текста" в  UTF-8, приводя его к нижнему регистру, в той
 * же кодировке. Именно так он и будет потом обрабатываться и храниться, для
 * экономии памяти */
char *normalize_text(const char *text, size_t *text_length) {
  wchar_t *wide_text = to_wide_string(text, NULL, text_length);
  char *multibyte_text;
  wcslower(wide_text);
  multibyte_text = to_multibyte_string(wide_text, text_length);
  strict_free(wide_text);
  return multibyte_text;
}

/* 
 * Из текста source_text создаёт другой, приводит слова к нормальной морфологической форме.
 */
char *normalize_morph_form(const char *source_text,
                                    MultiMorphology *morphology,
                                    size_t text_size) {
  size_t text_length, description_size;
  ssize_t token_size;
  const char *token_start, *token_end;
  const wchar_t *wide_token;
  char *description = NULL;
  char *normal_text = NULL, *text_cursor = NULL, *first_description = NULL;
  int32_t cursor, words_counter;
  void *memo = NULL;
  Dictionary *suggest_language = NULL, *detected_language = NULL;
  //puts("norm_0");
  text_cursor = normal_text = normalize_text(source_text, &text_length);
  //puts("norm_1");
  words_counter = cursor = 0;
  first_description = NULL;
  suggest_language  = NULL;
  
  char *token = NULL;
  char *saveptr1 = NULL;
  char *norm_result_text = NULL;
  int  offset = 0;
  
  norm_result_text = (char *) calloc(2*text_size, sizeof(char));
  if (norm_result_text == NULL) {
     return NULL; 
  }
  //puts("norm_2");
  while ((token_size = tokenize(text_cursor, &token_start, &token_end, &wide_token, &memo)) > 0) {
    //puts("norm_3");
    description = multilang_word_description(morphology, suggest_language,
                                             wide_token, wcslen(wide_token),
                                             token_start, (size_t) token_size,
                                             &description_size,
                                             &detected_language);
    //puts("norm_4");
    token = strtok_r(description, ".", &saveptr1);
    if (token != NULL && (offset + strlen(token) + 1) < 2 * text_size) {
        memcpy(&norm_result_text[offset], token, strlen(token));
        offset += strlen(token);
        memcpy(&norm_result_text[offset],   " ", 1);
        offset++;
    }
    //puts("norm_5");
    //memcpy(&norm_result_text[offset],   "\0", 1);
    //puts("norm_6");
    if (detected_language != NULL && detected_language != suggest_language) {
        suggest_language = detected_language;
    }
    
    if (words_counter == 0) {
      // Перед первым словом надо поставить терминатор
      first_description = strict_malloc(description_size + 2);
      *first_description = WORD_DESCRIPTION_TERMINATOR;
      memcpy(first_description + 1, description, description_size + 1);
      ++description_size;
      strict_free(description);
      description = first_description;
    }
    
    strict_free(description);
    
    //++words_counter;
    //puts("norm_7");
    text_cursor = NULL;
    //puts("norm_7_1");
  }
  //puts("norm_8");
  strict_free(normal_text);
  //puts("norm_9");
  return norm_result_text;
}


/* Из текста source_text создаёт другой, по которому можно искать слова
 * во всех словоформах. Попутно создаётся массив диапазонов, каждый из
 * которых описывает, за какое исходное слово отвечает каждый блок нового
 * текста.
 */
static char *build_text_with_ranges(const char *source_text,
                                    MultiMorphology *morphology,
                                    size_t *text_size,
                                    WordRange **result_ranges,
                                    size_t *ranges_count) {
  size_t text_length, description_size;
  ssize_t token_size;
  const char *token_start, *token_end;
  const wchar_t *wide_token;
  char *description;
  char *normal_text, *text_cursor, *result_text, *first_description;
  int32_t cursor, words_counter;
  void *memo = NULL;
  StringBuffer *document_buffer = create_string_buffer();
  ArrayList *word_ranges = make_array_list(sizeof(WordRange), 10000);
  Dictionary *suggest_language, *detected_language;
  text_cursor = normal_text = normalize_text(source_text, &text_length);
  words_counter = cursor = 0;
  first_description = NULL;
  suggest_language = NULL;
  while ((token_size = tokenize(text_cursor, &token_start, &token_end, &wide_token, &memo)) > 0) {
    WordRange range;
    description = multilang_word_description(morphology, suggest_language,
                                             wide_token, wcslen(wide_token),
                                             token_start, (size_t)token_size,
                                             &description_size,
                                             &detected_language);
    //puts(description);
    if (detected_language != NULL && detected_language != suggest_language) {
      suggest_language = detected_language;
    }
    if (words_counter == 0) {
      /* Перед первым словом надо поставить терминатор */
      first_description = strict_malloc(description_size + 2);
      *first_description = WORD_DESCRIPTION_TERMINATOR;
      memcpy(first_description + 1, description, description_size + 1);
      ++description_size;
      strict_free(description);
      description = first_description;
      range.start_position = cursor;
    } else {
      range.start_position = cursor - 1;
    }
    exact_append_to_string_buffer(document_buffer, description, description_size);
    strict_free(description);
    range.end_position = cursor + (int32_t)description_size - 1;
    range.original_start = range.end_position - (int32_t)token_size - 1;
    range.word_index = words_counter;
    cursor += (int32_t)description_size;
    ++words_counter;
    array_list_append(word_ranges, &range);
    text_cursor = NULL;
  }
  result_text = join_string_buffer(document_buffer, text_size);
  array_list_minimize(word_ranges);
  *result_ranges = array_list_data(word_ranges);
  *ranges_count = array_list_size(word_ranges);
  /* Строки не удаляем, т.к. они из кэша и будут удалены при выгрузке морфологии */
  free_string_buffer(document_buffer);
  free_array_list_without_data(word_ranges);
  strict_free(normal_text);
  return result_text;
}

/* Создаёт новый "документ" - результат индексации отдельной страницы,
 * позволяющий проводить по ней быстрый поиск не зависящий от формы слов.
 * Возвращает ссылку на область памяти с готовым документом, а также записывает
 * размер этой области в переменную, на которую указывает document_data_size.
 */
void *make_document(const char *text, uint16_t flags, MultiMorphology *morphology, size_t *document_data_size) {
  size_t alt_text_size, ranges_count;
  WordRange *word_ranges;
  int32_t *suffix_array;
  void *document;
  int8_t *cursor;
  DocumentHeader *header;
  char *alt_text;
  size_t suffix_array_byte_size, text_byte_size, ranges_byte_size;
  alt_text = build_text_with_ranges(text, morphology, &alt_text_size,
                                    &word_ranges, &ranges_count);
  suffix_array = text_to_suffix_array(alt_text, alt_text_size);
  /* Документ хранится одним большим блоком памяти, имеющим следующую
   * структуру:
   +---------------------+
   | Заголовок документа |
   +---------------------+
   |  Суффиксный массив  |
   +---------------------+
   |  Текст для поиска   |
   +---------------------+
   |  Диапазоны слов     |
   +---------------------+
   В блоках данных не используются указатели, так что документ можно писать
   прямо на диск, архивировать и т.п., а потом просто восстанавливать.
   */
  suffix_array_byte_size = alt_text_size*sizeof(*suffix_array);
  text_byte_size = (alt_text_size + 1)*sizeof(*alt_text);
  ranges_byte_size = ranges_count*sizeof(*word_ranges);
  *document_data_size = sizeof(DocumentHeader) +
      suffix_array_byte_size +
      text_byte_size +
      ranges_byte_size;
  document = strict_malloc(*document_data_size);
  header = document;
  memset(header, 0, sizeof(*header)); /* Для Valgrind */
  header->size = *document_data_size;
  header->flags = flags;
  header->created = time(NULL);
  header->text_length = alt_text_size;
  header->text_offset = sizeof(*header) + suffix_array_byte_size;
  header->ranges_offset = header->text_offset + text_byte_size;
  header->ranges_count = ranges_count;
  cursor = document;
  cursor += sizeof(*header);
  memcpy(cursor, suffix_array, suffix_array_byte_size); cursor += suffix_array_byte_size;
  memcpy(cursor, alt_text, text_byte_size); cursor += text_byte_size;
  memcpy(cursor, word_ranges, ranges_byte_size);
  strict_free(suffix_array);
  strict_free(alt_text);
  strict_free(word_ranges);
  return document;
}

void free_document(void *document) {
  strict_free(document);
}

inline uint64_t document_size(void *document) {
  return ((DocumentHeader *)document)->size;
}

inline size_t document_text_length(const void *document) {
  return ((DocumentHeader *)document)->text_length;
}

uint16_t document_flags(void *document) {
  return ((DocumentHeader *)document)->flags;
}

#define DOCUMENT_DATA(cast, offset_field) \
  (cast *)(((DocumentHeader *)document)->size > ((DocumentHeader *)document)->offset_field ? \
           (int8_t *)document + ((DocumentHeader *)document)->offset_field : \
           NULL)

inline char *document_text(const void *document) {
  return DOCUMENT_DATA(char, text_offset);
}

inline int32_t *document_suffix_array(const void *document) {
  return (int32_t *)((int8_t *)document + sizeof(DocumentHeader));
}

inline WordRange *document_word_ranges(const void *document, size_t *ranges_count) {
  *ranges_count = ((DocumentHeader *)document)->ranges_count;
  return DOCUMENT_DATA(WordRange, ranges_offset);
}

#undef DOCUMENT_DATA

inline static int range_searcher(const void *position, const void *range) {
  if (*((const int32_t *)position) < ((const WordRange *)range)->start_position) return -1;
  if (*((const int32_t *)position) > ((const WordRange *)range)->end_position - 1) return 1;
  return 0;
}

inline static WordRange *find_word_range(const WordRange *ranges, size_t ranges_count, int32_t position) {
  return bsearch(&position, ranges, ranges_count, sizeof(*ranges), range_searcher);
}

WordRange *suffix_in_allowed_ranges(int32_t suffix, ArrayList *allowed_ranges,
                                    const WordRange *all_ranges, size_t all_ranges_count) {
  size_t ranges_count = array_list_size(allowed_ranges), i;
  WordRange *range;
  const WordRange *bound_range = all_ranges + all_ranges_count;
  for (i = 0; i < ranges_count; ++i) {
    range = *(WordRange **)array_list_get(allowed_ranges, i);
    if (range >= bound_range) continue; /* Игнорируем "липовые" диапазоны вне границ массива */
    if (suffix >= range->start_position && suffix < range->end_position) return range;
  }
  return NULL;
}

/* Ищет леммы, закодированные с помощью функции multi_word_description (строка
 * description) в тексте text, используя предварительно построенный суффиксный
 * массив.
 * Результатом работы является набор диапазонов слов (WordRange) в которых
 * должны встретиться следующие леммы, если они принадлежат одной
 * фразе. Например, для текста "гриб.стать.сталь.растить." и фразы "грибы стали
 * расти", за словом "грибы" следует слово "стали", т.е. слово обязательно
 * должно встретиться в диапазоне "[стать.сталь.]", следующим за словом "гриб".
 */
static void find_lemmas_in_document(const char *description, int exact_match, const char *text,
                                    size_t text_length, const int32_t *suffix_array,
                                    const WordRange *ranges, size_t ranges_count,
                                    ArrayList *allowed_ranges, ArrayList *result_ranges) {
  const char *lemma_start, *next_lemma;
  char *terminator_prefixed_lemma;
  const int32_t *start_suffix, *end_suffix, *current_suffix;
  size_t lemma_size;
  const WordRange *range, *next_range;
  int is_first_lemma = (array_list_size(allowed_ranges) == 0);
  lemma_start = description;
  do {
    next_lemma = strchr(lemma_start + 1, WORD_DESCRIPTION_TERMINATOR);
    lemma_size = (size_t)(next_lemma - lemma_start);
    terminator_prefixed_lemma = 0;
    if (lemma_start == description) {
      /* К первой лемме добавляется терминатор и поиск идёт вместе с ним. Так мы
       * можем точно отследить совпадение всего слова, а не только его части.
       * У последующих лемм терминатор в начале будет стоять "автоматически",
       * в силу формата записи "лемма1.лемма2.лемма3." */
      terminator_prefixed_lemma = strict_malloc(lemma_size + 3);
      *terminator_prefixed_lemma = WORD_DESCRIPTION_TERMINATOR;
      memcpy(terminator_prefixed_lemma + 1, lemma_start, lemma_size + 1);
      lemma_size = lemma_size + 2;
      terminator_prefixed_lemma[lemma_size] = '\0';
      lemma_start = terminator_prefixed_lemma;
    } else {
      ++lemma_size;
    }
    find_with_suffix_array(lemma_start, lemma_size, text, text_length, suffix_array, &start_suffix, &end_suffix);
    if (start_suffix != NULL) {
      current_suffix = start_suffix;
      do {
        if (!is_first_lemma) {
          range = suffix_in_allowed_ranges(*current_suffix, allowed_ranges, ranges, ranges_count);
        } else {
          range = find_word_range(ranges, ranges_count, *current_suffix);
          array_list_append(allowed_ranges, &range);
        }
        if (range != NULL) {
          next_range = range + 1; /* указатель может выйти за границы массива, но обращений по нему не будет */
          array_list_append(result_ranges, &next_range);
        }
      } while (++current_suffix <= end_suffix);
    }
    strict_free(terminator_prefixed_lemma);
    lemma_start = next_lemma;
  } while (*(lemma_start + 1) != '\0');
}


/* Ищет "пересечение" фразы phrase с документом document, возвращая список фраз,
 * эквивалентных указанной документе. Например, если мы передадим фразу "продажа
 * квартиры", то в результате может вернуться "продам квартиру".
 * Параметр exact_match заставляет искать точное вхождение фразы в документ */
void document_find_intersection(const void *document, MultiMorphology *morphology,
                                Dictionary *suggested_language,
                                const char *phrase, int exact_match,
                                StringSet *result) {
  const char *kSpace = " ";
  void *memo = NULL;
  const char *token_start, *token_end, *phrase_cursor = phrase, *text;
  ssize_t token_size;
  size_t description_size, text_length, ranges_count, results_count, tokens_count, i, k, line_length;
  WordRange *ranges, *range;
  ArrayList *allowed_ranges, *result_ranges, *prev_allowed_ranges;
  StringBuffer *result_buffer;
  char *line, *description, *original_description;
  const int32_t *suffix_array = document_suffix_array(document);
  Dictionary *detected_language;
  allowed_ranges = make_array_list(sizeof(WordRange *), 10);
  result_ranges  = make_array_list(sizeof(WordRange *), 10);
  text = document_text(document);
  text_length = document_text_length(document);
  ranges = document_word_ranges(document, &ranges_count);
  tokens_count = 0;
  prev_allowed_ranges = NULL;
  while ((token_size = tokenize(phrase_cursor, &token_start, &token_end, NULL, &memo)) > 0) {
    if (tokens_count > 0 && array_list_size(allowed_ranges) == 0) {
      prev_allowed_ranges = NULL;
      final_tokenize(memo);
      break;
    }
    phrase_cursor = NULL;
    array_list_shrink(result_ranges, 0);
    description = original_description = multilang_word_description(
        morphology, suggested_language,
        NULL, 0,
        token_start, (size_t)token_size,
        &description_size, &detected_language);
    if (detected_language != NULL && detected_language != suggested_language) {
      suggested_language = detected_language;
    }
    if (exact_match) description = description + description_size - token_size - 1;
    /* Перебираем все леммы и ищем их в тексте */
    find_lemmas_in_document(description, exact_match, text, text_length, suffix_array,
                            ranges, ranges_count, allowed_ranges, result_ranges);
    strict_free(original_description);
    prev_allowed_ranges = allowed_ranges;
    allowed_ranges = result_ranges;
    result_ranges = prev_allowed_ranges;
    ++tokens_count;
  }
  /* Собираем результаты в кучу */
  if (allowed_ranges != NULL) {
    results_count = array_list_size(allowed_ranges);
    for (i = 0; i < results_count; ++i) {
      result_buffer = create_string_buffer();
      range = *(WordRange **)array_list_get(allowed_ranges, i) - 1;
      range = range - tokens_count + 1;
      for (k = 0; k < tokens_count; ++k, ++range) {
        noclone_append_to_string_buffer(result_buffer, text + range->original_start + 1, (size_t)(range->end_position - range->original_start - 1));
        if (k < tokens_count - 1) noclone_append_to_string_buffer(result_buffer, kSpace, 1);
      }
      line = join_string_buffer(result_buffer, &line_length);
      complex_free_string_buffer(result_buffer, 0);
      if (!add_to_string_set(result, line, line_length)) {
        strict_free(line);
      }
    }
    
  }
  free_array_list(result_ranges);
  free_array_list(allowed_ranges);
}

/* Выполняет разбор ключевой фразы pharse, выделяя из неё (если указаны) код
 * языка, флаг поиска точного совпадения, а также основной текст самой фразы.
 * Возвращает текст фразы, плюс ссылку на словарь языка и флаг точного поиска
 * через аргументы exact_language и exact_match;
 */
static const char *parse_phrase(const char *phrase, MultiMorphology *morphology,
                         Dictionary **exact_language, int *exact_match) {
  const char *cursor = phrase;
  char *language_splitter = strchr(cursor, LANGUAGE_INTERSECTION_SPLITTER);
  if (language_splitter != NULL) {
    *exact_language = get_dictionary(morphology, phrase, (size_t)(language_splitter - phrase));
    cursor = language_splitter + 1;
  } else {
    *exact_language = NULL;
  }
  if (*cursor == EXACT_INTERSECTION_FLAG) {
    *exact_match = 1;
    return cursor + 1;
  } else {
    *exact_match = 0;
    return cursor;
  }
}

/* Делает то же самое, что и document_find_intersection, но на входе получает не
 * фразу, а набор фраз, разделённых переносом строки. Если требуется найти
 * точное вхождение фразы, она предваряется символом "!".
 * Также, каждая фраза может быть предварена служебным префиксом вида "ru:", "en:" и т.п.,
 * который принудительно указывает, в контексте какого языка должна быть интерпретирована фраза.
 * Примеры фраз:
 *   "en|oldest news"
 *   "ru|!текст фразы"
 */
char *document_find_multi_intersection(const void *document, MultiMorphology *morphology,
                                       const char *phrase_lines,
                                       size_t *result_length) {
  const char *kEndOfLine = "\n", *phrase;
  char *splitter, *orig_phrase, *result;
  const char *cursor = phrase_lines;
  size_t phrase_size;
  int exact_match;
  StringSet *result_buffer = make_string_set(20);
  Dictionary *exact_language;
  do {
    splitter = strchr(cursor, MULTI_INTERSECTION_SPLITTER);
    if (splitter != NULL) {
      phrase_size = sizeof(*phrase)*(size_t)(splitter - cursor);
      orig_phrase = strict_strndup(cursor, phrase_size);
      cursor = splitter + 1;
    } else {
      orig_phrase = strict_strndup(cursor, strlen(cursor));
    }
    strip_line(orig_phrase);
    phrase = parse_phrase(orig_phrase, morphology, &exact_language, &exact_match);
    if (strlen(phrase) > 0) {
      document_find_intersection(document, morphology,
                                 exact_language,
                                 phrase, exact_match,
                                 result_buffer);
    }
    strict_free(orig_phrase);
  } while (splitter != NULL);
  result = join_string_set(result_buffer, kEndOfLine, 1, result_length);
  free_string_set(result_buffer, 1);
  return result;
}

char *_document_find_multi_intersection(const void *document, MultiMorphology *morphology,
                                       const char *phrase_lines,
                                       size_t *result_length) {
    if (phrase_lines == NULL) {
        return NULL;
    }
    const char *kEndOfLine = "\n", *phrase;
    char *splitter, *orig_phrase, *result;
    const char *cursor = phrase_lines;
    size_t phrase_size;
    int exact_match;
    StringSet *result_buffer = make_string_set(20);
    Dictionary *exact_language;

    splitter = (char *) cursor;

    phrase = parse_phrase(splitter, morphology, &exact_language, &exact_match);
    if (strlen(phrase) > 0) {
        document_find_intersection(document, morphology,
                                 exact_language,
                                 phrase, exact_match,
                                 result_buffer);
    }
    strict_free(orig_phrase);

    result = join_string_set(result_buffer, kEndOfLine, 1, result_length);
    free_string_set(result_buffer, 1);
    return result;
}
