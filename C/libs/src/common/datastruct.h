#ifndef __DATASTRUCT_H_
#define __DATASTRUCT_H_

#include <stdint.h>
#include <stdlib.h>
#include <wchar.h>
#include <stdio.h>

typedef struct string_chunk {
  struct string_chunk *next;
  char *string;
  const char *readonly_string;
  size_t length;
} StringChunk;

typedef struct {
  StringChunk *head;
  StringChunk *tail;
} StringBuffer;

typedef struct {
  int8_t *data;
  size_t block_size;
  size_t capacity;
  size_t initial_capacity;
  size_t size;
} ArrayList;

typedef struct {
  int8_t *data;
  size_t size;
  size_t used_space;
  FILE *file;
} MemBuffer;

typedef struct {
  ArrayList *strings;
  size_t full_length;
} StringSet;


typedef int (*EqFunction)(const void *item1, const void *item2);

StringBuffer *create_string_buffer(void);
void free_string_buffer(StringBuffer *buffer);
void complex_free_string_buffer(StringBuffer *buffer, int with_strings);
void exact_append_to_string_buffer(StringBuffer *buffer, const char *string, size_t length);
void append_to_string_buffer(StringBuffer *buffer, const char *string);
void noclone_append_to_string_buffer(StringBuffer *buffer, const char *string, size_t length);
char *join_string_buffer(StringBuffer *buffer, size_t *result_length);
size_t string_buffer_size(StringBuffer *buffer);

ArrayList *make_array_list(size_t block_size, size_t initial_capacity);
void free_array_list(ArrayList *list);
void free_array_list_without_data(ArrayList *list);
void *array_list_data(ArrayList *list);
void *array_list_put(ArrayList *list, size_t index, const void *data);
void array_list_delete(ArrayList *list, size_t index);
void *array_list_append(ArrayList *list, const void *data);
void *array_list_get(ArrayList *list, size_t index);
size_t array_list_size(ArrayList *list);
size_t array_list_capacity(ArrayList *list);
void array_list_minimize(ArrayList *list);
void array_list_foreach(ArrayList *list, void (*func)(void *item));
void array_list_shrink(ArrayList *list, size_t new_size);
void *array_list_iter(ArrayList *list, void **memo);
ssize_t array_list_index(ArrayList *list, void *data, EqFunction eqfunc);

MemBuffer *make_mem_buffer(size_t size, FILE *file);
void free_mem_buffer(MemBuffer *buffer);
size_t mem_buffer_free_space(MemBuffer *buffer);
int mem_buffer_enough(MemBuffer *buffer, size_t size);
int append_to_mem_buffer(MemBuffer *buffer, void *data, size_t size);
int flush_mem_buffer(MemBuffer *buffer);

StringSet *make_string_set(size_t initial_capacity);
int add_to_string_set(StringSet *set, char *string, size_t length);
char *join_string_set(StringSet *set,
                      const char *separator, int tail_separator,
                      size_t *result_length);
void free_string_set(StringSet *set, int with_strings);

#endif /* __DATASTRUCT_H_ */
