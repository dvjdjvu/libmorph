/* 
Хэш-таблица, сделанная по классической схеме, с разрешением коллизий методом цепочек.
Из особенностей - поддержка быстрого перечисления всех элементов таблицы, 
перемещение запрошенных элементов в начало списка и возможность
сделать FIFO-внесение данных, при котором можно установить предельное число хранимых
элементов, так чтобы вновь добавляемые вытесняли старые.

Автор: Кирилл Маврешко <kimavr@gmail.com>
*/

#ifndef __HASH_TABLE_H_
#define __HASH_TABLE_H_

#include <stdlib.h>
#include <stdint.h>
#include <math.h>

/* Целый тип, используемый для представления результата хэш-функции */
#define HASH_INT_TYPE uint32_t

/* Элемент списка, используемого для разрешения коллизий.
Элементы связаны между собой в кольцевой двунаправленный список.
Также они связаны по порядку добавления - это позволяет
быстро перебирать их и делать вытесняющее добавление.
*/
struct hash_table_chain_link {
  void *value;
  void *key;
  size_t key_size;
  HASH_INT_TYPE hash;
  struct hash_table_chain_link *next;
  struct hash_table_chain_link *prev;
  struct hash_table_chain_link *early_added;
  struct hash_table_chain_link *later_added;
};

typedef void (*ElementProcessor)(const void *key, size_t key_size, void *value, void *params);

/* Сама хэш-таблица и дополнительные параметры. */
typedef struct hash_table {
  struct hash_table_chain_link **bucket;
  struct hash_table_chain_link *last_added;
  struct hash_table_chain_link *first_added;
  uint8_t size_power;
  size_t size;
  size_t total_stored;
  size_t load;
  size_t fifo_limit;
  void *fifo_limit_args;
  ElementProcessor on_push_out;
} HashTable;

HASH_INT_TYPE hash_of_key(const void *key, size_t len);
uint8_t near_int_log2(size_t n);
size_t bucket_index(HASH_INT_TYPE key, uint8_t size_power);
HashTable *make_hash_table(uint8_t size_power);
void free_hash_table(HashTable *table);
void hash_table_fifo_limit(HashTable *table, size_t limit, ElementProcessor on_push_out, void *args);
void **hash_table_get_always(HashTable *table, const void *key, size_t key_size);
void hash_table_chain_put(HashTable *table, const void *key, size_t key_size, void *value);
void *hash_table_chain_get(HashTable *table, const void *key, size_t key_size);
void *hash_table_chain_delete(HashTable *table, const void *key, size_t key_size);
double hash_table_fill_rate(HashTable *table);
void *hash_table_chain_iter_items(HashTable *table, void **key, size_t *key_size, void **current_state);
/* Вызывает функцию processor для каждой пары ключ-значение хэш-таблицы. */
void hash_table_chain_foreach(HashTable *table, ElementProcessor processor, void *params);
size_t hash_table_stored(HashTable *table);

#endif /* __HASH_TABLE_H_ */
