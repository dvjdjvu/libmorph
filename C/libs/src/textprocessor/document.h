#ifndef __TEXTPROCESSOR_DOCUMENT_H_
#define __TEXTPROCESSOR_DOCUMENT_H_

#include <wchar.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include "../morphology/multilang.h"
#include "../morphology/helpers.h"

enum {DOC_PACKED = 1, DOC_NO_LOADED = 2};

typedef struct {
  int32_t word_index;
  int32_t start_position;
  int32_t end_position;
  int32_t original_start;
} WordRange;

typedef struct {
  uint16_t flags;
  int64_t created;
  uint64_t size;
  uint64_t text_length;
  uint64_t text_offset;
  uint64_t ranges_offset;
  uint64_t ranges_count;
} DocumentHeader;

#define MULTI_INTERSECTION_SPLITTER '\n'
#define EXACT_INTERSECTION_FLAG '!'
#define LANGUAGE_INTERSECTION_SPLITTER '|'

char *normalize_text(const char *text, size_t *text_length);
char *normalize_morph_form(const char *source_text, MultiMorphology *morphology, size_t text_size);

int32_t *build_suffix_array(const char *text, size_t text_size);
void *make_document(const char *text, uint16_t flags, MultiMorphology *morphology, size_t *document_data_size);
void free_document(void *document);
uint64_t document_size(void *document);
size_t document_text_length(const void *document);
uint16_t document_flags(void *document);
char *document_text(const void *document);
int32_t *document_suffix_array(const void *document);
WordRange *document_word_ranges(const void *document, size_t *ranges_count);
void document_find_intersection(const void *document, MultiMorphology *morphology,
                                Dictionary *suggested_language,
                                const char *phrase, int exact_match,
                                StringSet *result);
char *document_find_multi_intersection(const void *document, MultiMorphology *morphology,
                                       const char *phrase_lines,
                                       size_t *result_length);

char *_document_find_multi_intersection(const void *document, MultiMorphology *morphology,
                                       const char *phrase_lines,
                                       size_t *result_length);


#endif /* __TEXTPROCESSOR_DOCUMENT_H_ */
