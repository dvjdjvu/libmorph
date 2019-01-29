/* 
Хэш-таблица, сделанная по классической схеме, с разрешением коллизий методом цепочек.
Из особенностей - поддержка быстрого перечисления всех элементов таблицы и возможность
сделать FIFO-внесение данных, при котором можно установить предельное число хранимых
элементов, так чтобы вновь добавляемые вытесняли старые.

Автор: Кирилл Маврешко <kimavr@gmail.com>
*/

#include "common/hashtable.h"

#include <stdlib.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <limits.h>

#include "common/strict_alloc.h"

#define NO_FIFO_LIMIT (size_t)-1

HASH_INT_TYPE hash_of_key(const void *key, size_t len) {
   const HASH_INT_TYPE seed = 19780211;
   register HASH_INT_TYPE hash = 0;
   size_t i = 0;
   const uint8_t *byte_key = key;
   const unsigned long int *longword_ptr;
   for(i = 0; i < len && ((unsigned long int) byte_key & (sizeof(*longword_ptr) - 1)) != 0; ++byte_key, ++i) {
      hash = (hash * seed) + (*byte_key);
   }
   longword_ptr = (unsigned long int *) byte_key;

   while (len - i >= sizeof(*longword_ptr)) {
     const unsigned char *cp = (const unsigned char *)longword_ptr;
     hash = (hash * seed) + cp[0];
     hash = (hash * seed) + cp[1];
     hash = (hash * seed) + cp[2];
     hash = (hash * seed) + cp[3];
#ifndef LONG_MAX
     abort();
#endif
#if LONG_MAX > 2147483647
     hash = (hash * seed) + cp[4];
     hash = (hash * seed) + cp[5];
     hash = (hash * seed) + cp[6];
     hash = (hash * seed) + cp[7];
#endif
     ++longword_ptr;
     i += sizeof (*longword_ptr);
   }
   
   byte_key = (uint8_t *) longword_ptr;
   while (i++ < len) {
      hash = (hash * seed) + (*byte_key);
      ++byte_key;
   }
   return hash;
}

/* Ближайший целый (в большую сторону) логарифм по основанию 2 числа n. */
uint8_t near_int_log2(size_t n) { 
  uint8_t i = 0;
  while ((n >> i) > 0) ++i;
  return i;
}

/* Создаёт хэш-таблицу размером 2^size_power */
HashTable *make_hash_table(uint8_t size_power) {
  HashTable *table = strict_malloc(sizeof(HashTable));
  table->bucket = (struct hash_table_chain_link **) strict_calloc(1U << size_power, sizeof(struct hash_table_chain_link *)); 
  table->size_power = size_power;
  table->size = (1U << size_power);
  table->last_added = table->first_added = NULL;
  table->total_stored = 0;
  table->load = 0;
  table->fifo_limit = (size_t)-1;
  table->fifo_limit_args = NULL;
  table->on_push_out = NULL;
  return table;
}

/* Устанавливает жёсткое ограничение на суммарное число хранящихся
 * элементов. Как только число элементов достигает указанный предел, добавление
 * новых будет приводить к "вытеснению" (и удалению) старых. Перед удалением,
 * будет вызвана callback-функция on_push_out, в которой можно освободить
 * память и т.п. с данными элемента и дополнительными данными args. */
void hash_table_fifo_limit(HashTable *table, size_t limit, ElementProcessor on_push_out, void *args) {
    //puts("hash_table_fifo_limit 0");
    //assert(limit > 0);
    //puts("hash_table_fifo_limit 1");
    table->fifo_limit = limit;
    table->fifo_limit_args = args;
    table->on_push_out = on_push_out;
}

/* Помещает значение value с ключом key и хэшем hash в цепочку head таблицы. */
static struct hash_table_chain_link *make_chain_link(struct hash_table_chain_link **head, const void *key, size_t key_size, HASH_INT_TYPE hash, void *value) {
  struct hash_table_chain_link *link = strict_malloc(sizeof(struct hash_table_chain_link));
  link->key = strict_malloc(key_size);
  memcpy(link->key, key, key_size);
  link->key_size = key_size;
  link->hash = hash;
  link->value = value;
  if (*head == NULL) {
    link->next = link;
    link->prev = link;
  } else {
    (*head)->prev->next = link;
    link->prev = (*head)->prev;
    (*head)->prev = link;
    link->next = *head;
  }
  *head = link;
  return link;
}

static void free_chain_link(struct hash_table_chain_link *link) {
  strict_free(link->key);
  strict_free(link);
}

static void free_chain(struct hash_table_chain_link *head) {
  struct hash_table_chain_link *next, *link = head;
  if (head != NULL) {
    do {
      next = link->next;
      free_chain_link(link);
      link = next;
    } while (link != head);
  }
}

/* Освобождает хэш-таблицу */
void free_hash_table(HashTable *table) {
  size_t i, bucket_size = table->size;
  struct hash_table_chain_link **chain;
  for (chain=table->bucket, i=0; i < bucket_size; i++, chain++) {
    free_chain(*chain);
  }
  strict_free(table->bucket);
  strict_free(table);
}

/* Удаляет отдельный элемент хэш таблицы. Возвращает указатель на хранившееся
 * там прежде значение. */
static void *drop_table_chain_link(HashTable *table,
                                  struct hash_table_chain_link *link,
                                  struct hash_table_chain_link **bucket_item) {
  void *link_value;
  if (link == *bucket_item) {
    if (link->next == link) {
      *bucket_item = NULL;
      table->load--;
    } else {
      link->next->prev = link->prev;
      link->prev->next = link->next;
      *bucket_item = link->next;
    }
  } else {
    if (link->next != NULL) link->next->prev = link->prev;
    if (link->prev != NULL) link->prev->next = link->next;
  }
  
  if (link->early_added != NULL) link->early_added->later_added = link->later_added;
  if (link->later_added != NULL) link->later_added->early_added = link->early_added;
  if (table->last_added == link) table->last_added = link->early_added;
  if (table->first_added == link) table->first_added = link->later_added;
  link_value = link->value;
  free_chain_link(link);
  table->total_stored--;
  return link_value;
}

/* Возвращает индекс ключа key в хэш-таблице размера 2^size_power ячеек */
#define BUCKET_INDEX(hash, bucket_size) (hash % bucket_size)


/* Следит за ограничениями на число элементов в таблице, если таковые
   установлены, чтобы в таблице всегда было не более (fifo_limit - 1)
   элементов. При необходимости, удаляет старый элемент, освободив место
   для нового. */
static inline void ensure_fifo_limit(HashTable *table) {
  if (table->fifo_limit != NO_FIFO_LIMIT) {
    while (table->total_stored >= table->fifo_limit) {
      HASH_INT_TYPE integer_key;
      struct hash_table_chain_link *deleting_item = table->first_added;
      integer_key = hash_of_key(deleting_item->key, deleting_item->key_size);
      table->on_push_out(deleting_item->key, deleting_item->key_size, deleting_item->value, table->fifo_limit_args);
      drop_table_chain_link(table,
                            deleting_item,
                            table->bucket + BUCKET_INDEX(integer_key, table->size));
    }
  }
}

/* Ищем элемент в хэш таблице (используя метод цепочек), возвращая ссылку на
 * всю структуру элемента. */
static struct hash_table_chain_link *
hash_table_get_link(HashTable *table,
                    const void *key,
                    size_t key_size,
                    HASH_INT_TYPE hash,
                    struct hash_table_chain_link **bucket_item) {
  struct hash_table_chain_link *head, *link;
  head = *bucket_item;
  if (head != NULL) {
    link = head;
    do {
      if (link->hash == hash && link->key_size == key_size && memcmp(link->key, key, key_size) == 0) {
        /* Вынос элемента наверх */
	/*if (link != head) {
	  link->next->prev = link->prev;
	  link->prev->next = link->next;
	  
	  link->prev = head->prev;
	  head->prev->next = link;
	  head->prev = link;
	  link->next = head;
	  *bucket_item = link;
          }*/
	return link;
      }
      link = link->next;
    } while (link != head);
  }
  return NULL;
}

/* Возвращает ссылку на значение, помещённое хэш-таблицу table по ключу key,
   используя для разрешения коллизий метод цепочек. Если значение в таблице
   отсутствует, будет вставлен новый элемент, и возвращена ссылка на его
   значение. Таким образом, функция используется для извлечения или быстрой
   вставки элемента, если его в таблице ещё нет. */
void **hash_table_get_always(HashTable *table, const void *key, size_t key_size) {
  HASH_INT_TYPE hash;
  struct hash_table_chain_link **bucket_item, *link;
  hash = hash_of_key(key, key_size);
  bucket_item = table->bucket + BUCKET_INDEX(hash, table->size);
  link = hash_table_get_link(table, key, key_size, hash, bucket_item);
  if (link != NULL) return &link->value;
  /* Не нашли вхождений, вставляем новый элемент */
  ensure_fifo_limit(table);
  if (*bucket_item == NULL) {
    table->load++;
  }
  link = make_chain_link(bucket_item, key, key_size, hash, NULL);
  if (table->last_added == NULL) {
    table->first_added = link;
  } else {
    table->last_added->later_added = link;
  }
  link->early_added = table->last_added;
  link->later_added = NULL;
  table->last_added = link;
  table->total_stored++;
  return &link->value;
}

/* Помещает значение value в хэш-таблицу table по ключу key,
   используя для разрешения коллизий метод цепочек.
   Проверка на существование значения в таблице ранее - не делается. */
void hash_table_chain_put(HashTable *table, const void *key, size_t key_size, void *value) {
  void **item_value = hash_table_get_always(table, key, key_size);
  *item_value = value;
}

/* Возвращает значение, помещённое хэш-таблицу table по ключу key,
   используя для разрешения коллизий метод цепочек. */
void *hash_table_chain_get(HashTable *table, const void *key, size_t key_size) {
  HASH_INT_TYPE hash = hash_of_key(key, key_size);
  struct hash_table_chain_link **bucket_item = table->bucket + BUCKET_INDEX(hash, table->size);
  struct hash_table_chain_link *link = hash_table_get_link(table, key, key_size, hash, bucket_item);
  return (link == NULL) ? NULL : link->value;
}

/* Удаляет значение, помещённое хэш-таблицу table по ключу key,
   используя для разрешения коллизий метод цепочек. Возвращает значение,
   хранившееся по удалённому ключу, если такое было (либо NULL, в противном случае).*/
void *hash_table_chain_delete(HashTable *table, const void *key, size_t key_size) {
  HASH_INT_TYPE hash = hash_of_key(key, key_size);
  struct hash_table_chain_link **bucket_item = table->bucket + BUCKET_INDEX(hash, table->size);
  struct hash_table_chain_link *link = hash_table_get_link(table, key, key_size, hash, bucket_item);
  void *link_value = NULL;
  if (link != NULL) {
    link_value = drop_table_chain_link(table, link, bucket_item);
  }
  return link_value;
}

/* Оценивает заполнение хэш-таблицы (как много ячеек уже занято) в виде значения от 0 до 1. 
   Отладочная вещь. Используется для сравнения распределённости значений разных хэш-функций. */
double hash_table_fill_rate(HashTable *table) {
  return (double) table->load / (double) table->size;
}

/* Позволяет делать перебор значений таблицы, используя конструкцию вида 
void *state = NULL, *key, *item;
size_t key_size;
while ((item = hash_table_iter_items(table, &key, &key_size, &state)) != NULL) {
   // теперь значение будет в item, а ключ и его размер - в key и key_size
   // переменная state должна быть равна NULL до начала цикла
}
Полагается, что таблица использует метод цепочек.
*/
void *hash_table_chain_iter_items(HashTable *table, void **key, size_t *key_size, void **current_state) {
  struct hash_table_chain_link *cursor;
  void *result;
  if (*current_state == NULL) {
    cursor = *current_state = table->last_added;
    if (cursor != NULL) {
      *key = cursor->key;
      *key_size = cursor->key_size;
      result = cursor->value;
    } else {
      result = NULL;
    }
  } else {
    cursor = (struct hash_table_chain_link *)(*current_state);
    if (cursor->early_added != NULL) {
      cursor = *current_state = cursor->early_added;
      *key = cursor->key;
      *key_size = cursor->key_size;
      result = cursor->value;
    } else {
      result = NULL;
    }
  }
  return result;
}

/* Вызывает функцию processor для каждой пары ключ-значение хэш-таблицы. */
void hash_table_chain_foreach(HashTable *table, ElementProcessor processor, void *params) {
  struct hash_table_chain_link *cursor;
  cursor = table->last_added;
  while (cursor != NULL) {
    processor(cursor->key, cursor->key_size, cursor->value, params);
    cursor = cursor->early_added;
  }
}

inline size_t hash_table_stored(HashTable *table) {
  return table->total_stored;
}
