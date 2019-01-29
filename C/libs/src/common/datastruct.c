/* Базовые структуры данных (списки и т.п.) и функции для работы с ними */

#include "common/datastruct.h"

#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "common/strict_alloc.h"

/* Строковой буфер */

/* Создаёт строковой буфер, позволяющий накапливать строки-куски по частям,
а потом либо извлекать их, либо объединять в одну большую строку. */
StringBuffer *create_string_buffer(void) {
  StringBuffer *buffer = strict_malloc(sizeof(StringBuffer));
  buffer->tail = NULL;
  buffer->head = NULL;
  return buffer;
}

/* Возвращает число символов (без учёта терминальных), хранящееся во всех кусках буфера. */
size_t string_buffer_size(StringBuffer *buffer) {
  size_t result = 0;
  StringChunk *chunk;
  for(chunk=buffer->head; chunk != NULL; chunk=chunk->next) {
    result += chunk->length;
  }
  return result;
}

/* Освобождает всю память, занятую буфером, позволяя не освобождать использованные строки */
void complex_free_string_buffer(StringBuffer *buffer, int with_strings) {
  StringChunk *chunk, *next;
  for(chunk=buffer->head; chunk != NULL; chunk=next) {
    next = chunk->next;
    if (with_strings) {
      strict_free(chunk->string);
      strict_free((char *)chunk->readonly_string);
    }
    strict_free(chunk);
  }
  strict_free(buffer);
}

/* Освобождает всю память, занятую буфером */
void free_string_buffer(StringBuffer *buffer) {
  complex_free_string_buffer(buffer, 1);
}

void exact_append_to_string_buffer(StringBuffer *buffer, const char *string, size_t length) {
  StringChunk *chunk = strict_malloc(sizeof(StringChunk));
  const size_t bytes = sizeof(char) * (length + 1);
  chunk->readonly_string = NULL;
  chunk->string = strict_malloc(bytes);
  memcpy(chunk->string, string, bytes);
  chunk->string[length] = '\0';
  chunk->length = length;
  chunk->next = NULL;
  if (buffer->head == NULL) {
    buffer->head = buffer->tail = chunk;
  } else {
    buffer->tail->next = chunk;
    buffer->tail = chunk;
  }
}

/* Добавляет новую строку в буфер */
void append_to_string_buffer(StringBuffer *buffer, const char *string) {
  exact_append_to_string_buffer(buffer, string, strlen(string));
}

/* Добавляет новую строку в буфер, не копируя её содержимое, а сохраняя прямой
 * указатель на данные.
 * Важно: эта строка будет освобождена при удалении буфера.
 */
void noclone_append_to_string_buffer(StringBuffer *buffer, const char *string, size_t length) {
  StringChunk *chunk = strict_malloc(sizeof(StringChunk));
  chunk->readonly_string = string;
  chunk->string = NULL;
  chunk->length = length;
  chunk->next = NULL;
  if (buffer->head == NULL) {
    buffer->head = buffer->tail = chunk;
  } else {
    buffer->tail->next = chunk;
    buffer->tail = chunk;
  }
}

/* Объединяет все строки буфера в одну большую и возвращает её. */
char *join_string_buffer(StringBuffer *buffer, size_t *result_length) {
  char *result = strict_malloc(sizeof(char)*string_buffer_size(buffer) + 1);
  StringChunk *chunk;
  size_t offset = 0;
  for(chunk=buffer->head; chunk != NULL; chunk=chunk->next) {
    strncpy(result + offset,
            chunk->string != NULL ? chunk->string : chunk->readonly_string,
            chunk->length);
    offset += chunk->length;
  }
  result[offset] = '\0';
  *result_length = offset;
  return result;
}

/* Динамический массив */

ArrayList *make_array_list(size_t block_size, size_t initial_capacity) {
  ArrayList *list = strict_malloc(sizeof(ArrayList));
  list->data = strict_malloc(block_size * initial_capacity);
  list->capacity = list->initial_capacity = initial_capacity;
  list->block_size = block_size;
  list->size = 0;
  return list;
}

void free_array_list_without_data(ArrayList *list) {
  strict_free(list);
}

void free_array_list(ArrayList *list) {
  strict_free(list->data);
  free_array_list_without_data(list);
}

inline void *array_list_data(ArrayList *list) {
  return list->data;
}

void *array_list_put(ArrayList *list, size_t index, const void *data) {
  void *result;
  //puts("0 array_list_put");
  //assert(index < list->size);
  //puts("1 array_list_put");
  if (index < list->size) {
    result = list->data + (index * list->block_size);
    if (data != NULL) {
      memcpy(result, data, list->block_size);
    } else {
      memset(result, 0, list->block_size);
    }
  } else {
    result = NULL;
  }
  return result;
}

void array_list_delete(ArrayList *list, size_t index) {
  int8_t *removed;
  //puts("0 array_list_delete");
  //assert(index < list->size);
  //puts("1 array_list_delete");
  removed = list->data + (index * list->block_size);
  if (index < list->size - 1) {
    memmove(removed, removed + list->block_size, list->block_size * (list->size - index - 1));
  }
  list->size--;
}

void array_list_resize(ArrayList *list, size_t new_capacity) {
    list->capacity = new_capacity;
    list->data = strict_realloc(list->data, list->capacity * list->block_size);
}

void array_list_shrink(ArrayList *list, size_t new_size) {
    list->size = new_size;
    array_list_resize(list, new_size);
}

void *array_list_append(ArrayList *list, const void *data) {
  if (list->capacity <= list->size) {
    array_list_resize(list, list->capacity ? (list->capacity << 1) : list->initial_capacity);
  }
  list->size++;
  return array_list_put(list, list->size - 1, data);
}

void *array_list_get(ArrayList *list, size_t index) {
    //puts("0 array_list_get");
    //assert(index < list->size);
    //puts("1 array_list_get");
    if (index < list->size) {
        return list->data + (index * list->block_size);
    }
    return NULL;
}

inline size_t array_list_size(ArrayList *list) { 
  return list->size; 
}

inline size_t array_list_capacity(ArrayList *list) { 
  return list->capacity;
}

void array_list_minimize(ArrayList *list) {
  array_list_resize(list, list->size);
}

/* Вызывает для каждого элемента массива функцию func */
void array_list_foreach(ArrayList *list, void (*func)(void *item)) {
  int8_t *cursor = list->data;
  size_t i;
  for (i = 0; i < list->size; ++i) {
    func(cursor);
    cursor += list->block_size;
  }
}

/* Позволяет перебрать последовательно все элементы массива, вызовом вида 
   void *memo;
   while ((item = array_list_iter(list, &memo)) != NULL) {
   }
*/
void *array_list_iter(ArrayList *list, void **memo) {
  if (array_list_size(list) == 0) {
      return NULL;
  } else if (*memo == NULL) {
    *memo = list->data;
  } else if (*memo == array_list_get(list, array_list_size(list) - 1)) {
    return NULL;
  } else {
    *memo = (int8_t *)*memo + list->block_size;
  }
  return *memo;
}

/* Ищет элемент data в массиве и возвращает его индекс, 
   используя функцию эквивалентности eqfunc, которая должна 
   возвращать 0 если элементы не равны. Если eqfunc не задать,
   будет использоваться стандартная memcmp. */
ssize_t array_list_index(ArrayList *list, void *data, 
			 EqFunction eqfunc) {
  int8_t *cursor = list->data;
  size_t i;
  if (eqfunc == NULL) {
    for (i = 0; i < list->size; ++i) {
      if (memcmp(cursor, data, list->block_size) == 0) {
	return (ssize_t)i;
      }
      cursor += list->block_size;
    }
  } else {
    for (i = 0; i < list->size; ++i) {
      if (eqfunc(cursor, data)) {
	return (ssize_t)i;
      }
      cursor += list->block_size;
    }
  }
  return -1;
}

MemBuffer *make_mem_buffer(size_t size, FILE *file) {
  MemBuffer *buffer = strict_malloc(sizeof(*buffer));
  buffer->size = size;
  buffer->used_space = 0;
  buffer->data = strict_malloc(size);
  buffer->file = file;
  return buffer;
}

void free_mem_buffer(MemBuffer *buffer) {
  strict_free(buffer->data);
  strict_free(buffer);
}

size_t mem_buffer_free_space(MemBuffer *buffer) {
  return buffer->size - buffer->used_space;
}

int mem_buffer_enough(MemBuffer *buffer, size_t size) {
  return mem_buffer_free_space(buffer) >= size;
}

int flush_mem_buffer(MemBuffer *buffer) {
  size_t result = fwrite(buffer->data, 1, buffer->used_space, buffer->file);
  if (result == buffer->used_space) {
    buffer->used_space = 0;
    return 0;
  }
  return -1;
}

void resize_mem_buffer(MemBuffer *buffer, size_t new_size) {
  buffer->data = strict_realloc(buffer->data, new_size);
  buffer->size = new_size;
}

int mem_buffer_need_space(MemBuffer *buffer, size_t size) {
  int retcode = 0;
  if (!mem_buffer_enough(buffer, size)) {
    if (buffer->size >= size) {
      retcode = flush_mem_buffer(buffer);
    } else {
      resize_mem_buffer(buffer, buffer->used_space + size);
    }
  }
  return retcode;
}

int append_to_mem_buffer(MemBuffer *buffer, void *data, size_t size) {
  int rc = mem_buffer_need_space(buffer, size);
  if (rc == 0) {
    memcpy(buffer->data + buffer->used_space, data, size);
    buffer->used_space += size;
  }
  return 0;
}

/* Создаёт набор неповторяющихся строк. Добавление строк в такой набор делается
 * через функцию add_to_string_set. Затем готовый набор можно объеденить в одну
 * строку, используя какой-то разделитель. */
StringSet *make_string_set(size_t initial_capacity) {
  StringSet *set = strict_malloc(sizeof(*set));
  set->strings = make_array_list(sizeof(StringChunk), initial_capacity);
  set->full_length = 0;
  return set;
}

/* Служебная функция набора уникальных строк. Позволяет определить, есть ли
 * указанная строка в наборе, и если нет, то в какую позицию её вставить */
static ssize_t position_for_string(StringSet *set, const char *string, size_t length) {
  size_t strings_count = array_list_size(set->strings);
  StringChunk *item,
      *raw_data = array_list_data(set->strings),
      *left = raw_data,
      *right = raw_data + strings_count - 1;
  int compare_result;
  do {
    item = left + ((right - left) >> 1);
    compare_result = strncmp(string, item->string, length);
    if (compare_result == 0) {
      return -1;
      } else {
      if (compare_result < 0) {
        right = item - 1;
      } else {
        left = item + 1;
      }
    }
  } while (left <= right);
  return (compare_result < 0 ? item : left) - raw_data;
}

/* Добавляет строку к набору уникальных строк. Если строка не была добавлена,
 * возвращается 0. */
int add_to_string_set(StringSet *set, char *string, size_t length) {
  size_t strings_count = array_list_size(set->strings);
  StringChunk *added;
  ssize_t position;
  if (strings_count == 0) {
    added = array_list_append(set->strings, NULL);
    added->string = string;
    added->length = length;
    set->full_length += length;
  } else {
    position = position_for_string(set, string, length);
    if (position >= 0) {
      array_list_append(set->strings, NULL);
      added = array_list_get(set->strings, (size_t)position);
      if ((size_t)position < strings_count)
        memmove(added + 1, added, sizeof(*added)*(strings_count - (size_t)position));
      added->string = string;
      added->length = length;
      set->full_length += length;
    } else {
      return 0;
    }
  }
  return 1;
}

/* Объединяет все строки множества в одну большую строку, используя разделитель
 * separator. При tail_separator != 0, разделитель будет также завершать
 * получаемый результат. */
char *join_string_set(StringSet *set,
                      const char *separator, int tail_separator,
                      size_t *result_length) {
  size_t i,
      strings_count = array_list_size(set->strings),
      separator_size = strlen(separator),
      byte_size = set->full_length + 1 + \
        separator_size * (tail_separator || strings_count == 0 ? strings_count : strings_count - 1);
  char *result = strict_malloc(byte_size), *cursor;
  *result_length = byte_size - 1;
  cursor = result;
  for (i = 0; i < strings_count; ++i) {
    StringChunk *chunk = array_list_get(set->strings, i);
    memcpy(cursor, chunk->string, chunk->length);
    cursor += chunk->length;
    if (tail_separator || i < strings_count - 1) {
      memcpy(cursor, separator, separator_size);
      cursor += separator_size;
    }
  }
  result[byte_size - 1] = '\0';
  return result;
}

void free_string_set(StringSet *set, int with_strings) {
  if (with_strings) {
    size_t i, strings_count = array_list_size(set->strings);
    StringChunk *raw = array_list_data(set->strings);
    for (i = 0; i < strings_count; ++i) {
      strict_free(raw->string);
      ++raw;
    }
  }
  free_array_list(set->strings);
  strict_free(set);
}
