/* Конечный автомат с инкрементальным построением и минимизацией. 
   Используется алгоритм Jan Daciuk и Bruce Watson ("Incremental 
   Construction of Minimal Acyclic Finite-State Automata and Transducers,
   and Use in the Natural Language Processing").

   Автор: Кирилл Маврешко <kimavr@gmail.com>
*/
#include "morphology/automat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/strict_alloc.h"
#include "common/hashtable.h"
#include "common/datastruct.h"

#define StatesCount uint32_t
#define StateDescriptionSize uint64_t

/* Классы эквивалентности */
static void add_to_equivalence_class(EquivalenceClass *_class, State *state) {
  StateListItem *item = strict_malloc(sizeof(StateListItem));
  item->next = _class->state_list;
  item->state = state;
  _class->state_list = item;
  _class->count++;
}

static void remove_from_equivalence_class(EquivalenceClass *_class, State *state) {
  StateListItem *item = _class->state_list, *prev_item = NULL;
  while (item != NULL) {
    if (item->state == state) {
      if (prev_item != NULL) {
	prev_item->next = item->next;
      } else {
	_class->state_list = item->next;
      }
      strict_free(item);
      break;
    }
    prev_item = item;
    item = item->next;
  }
}

static inline int8_t is_empty_equivalence_class(EquivalenceClass *_class) {
  return _class->state_list == NULL;
}

static EquivalenceClass *add_equivalence_class(EquivalenceClassList *list, void *key, size_t key_size) {
  EquivalenceClass *_class = strict_malloc(sizeof(EquivalenceClass));
  _class->count = 0;
  _class->state_list = NULL;
  hash_table_chain_put(*list, key, key_size, _class);
  return _class;
}

static void remove_equivalence_class(EquivalenceClassList list, void *key, size_t key_size) {
  hash_table_chain_delete(list, key, key_size);
}

static State *any_state_from_class(EquivalenceClass *_class) {
  return (_class->state_list == NULL) ? NULL : _class->state_list->state;
}

static void free_state_list(StateListItem *item) {
  StateListItem *next;
  while (item != NULL) {
    next = item->next;
    strict_free(item);
    item = next;
  }
}

static void free_class_list(EquivalenceClassList list) {
  void *state = NULL, *key;
  size_t key_size;
  EquivalenceClass *_class;
  while ((_class = hash_table_chain_iter_items(list, &key, &key_size, &state)) != NULL) {
    free_state_list(_class->state_list);
    strict_free(_class);
  }
  free_hash_table(list);
}

/* Переходы */

static Transition *make_transition(Label label, State *target, Transition *transition) {
  if (transition == NULL) {
    transition = strict_malloc(sizeof(Transition));
  }
  transition->label = label;
  transition->target = target;
  return transition;
}

inline State *transition_target(Transition *transition) {
  return transition->target;
}

static void set_transition_target(Transition *transition, State *state) {
  transition->target = state;
}

inline Label transition_label(Transition *transition) {
  return transition->label;
}

/* Списки переходов на другие состояния */

static TransitionList *make_transition_list(void) {
  TransitionList *result = make_array_list(sizeof(Transition), 5);
  return result;
}

static void free_transition_list(TransitionList *list) {
  free_array_list(list);
}

Transition *next_transition(TransitionList *list, void **memo) {
  return array_list_iter(list, memo);
}

static TransitionList *extend_transition_list(TransitionList *list, Label label, State *to_state) {
  Transition *new_transition = array_list_append(list, NULL);
  make_transition(label, to_state, new_transition);
  return list;
}

static Transition *last_transition(TransitionList *list) {
  size_t list_size = array_list_size(list);
  return list_size > 0 ? array_list_get(list, list_size - 1) : NULL;
}

static size_t transition_list_size(TransitionList *list) {
  return list->size;
}

Transition *find_transition(TransitionList *list, Label label) {
  size_t i, list_size = array_list_size(list);
  Transition *transition;
  for (i = 0; i < list_size; i++) {
    transition = array_list_get(list, i);
    if (transition->label == label) return transition;
  }
  return NULL;
}

/* Состояния */

State *make_state(uint32_t id, State *prev_state) {
  State *result = strict_malloc(sizeof(State));
  result->transition_list = make_transition_list();
  result->flags = UNMARKED_STATE;
  result->_class = NULL;
  result->id = id;
  if (prev_state == NULL) {
    result->next = result;
    result->prev = result;
  } else {
    result->next = prev_state->next;
    prev_state->next->prev = result;
    result->prev = prev_state;
    prev_state->next = result;
  }
  return result;
}

void free_state(State *state) {
  free_transition_list(state->transition_list);
  strict_free(state);
}

inline static void set_state_id(State *state, uint32_t id) {
  state->id = id;
}

inline TransitionList *state_transitions(State *state) {
  return state->transition_list;
}

void update_state_transitions(State *state, TransitionList *list) {
  state->transition_list = list;
}

size_t transitions_count(State *state) {
  return transition_list_size(state_transitions(state));
}

int8_t has_children(State *state) {
  return transitions_count(state) ? 1 : 0;
}

State *last_child(State *state) {
  return transition_target(last_transition(state->transition_list));
}

void set_last_child(State *state, State *child) {
  set_transition_target(last_transition(state->transition_list), child);
}

void mark_as_final(State *state) {
  state->flags |= FINAL_STATE;
}

int8_t is_final_state(State *state) {
  return (state->flags & FINAL_STATE) ? 1 : 0;
}

void mark_as_registered(State *state) {
  state->flags |= REGISTERED_STATE;
}

int8_t is_marked_as_registered(State *state) {
  return (state->flags & REGISTERED_STATE) ? 1 : 0;
}

State *append_new_state(State *to_state, Label label, uint32_t id, State *prev_state) {
  State *new_state = make_state(id, prev_state);
  TransitionList *transitions = state_transitions(to_state);
  update_state_transitions(to_state, extend_transition_list(transitions, label, new_state));
  return new_state;
}

void append_state(State *from_state, State *to_state, Label label) {
  TransitionList *transitions = state_transitions(from_state);
  update_state_transitions(from_state, extend_transition_list(transitions, label, to_state));
}


/* Используется для сортировки описаний переходов. См. state_description_key */
static int transition_descriptor_comparer(const void *obj1, const void *obj2) {
  struct class_transition_descriptor 
    *td1 = (struct class_transition_descriptor *) obj1, 
    *td2 = (struct class_transition_descriptor *) obj2;
  if (td1->label == td2->label) return 0;
    return td1->label < td2->label ? -1 : 1;
  return 0;
}

/* Генерирует байтовый массив, который можно трактовать как универсальный идентификатор состояния - 
   - он описывает его переходы и метки, куда идут переходы, является ли состояние конечным.
  Используется для построения реестра состояний. Позволяет быстро сравнивать состояния по 
  принадлежности к классу эквивалентности.
  Указатель на массив записывается в result, а в result_size помещается размер буфера.
  После использования, буфер надо освободить через free.
*/
void state_description_key(State *state, int8_t **result, size_t *result_size) {
  size_t links_count = transitions_count(state);
  struct class_transition_descriptor transition_descriptor;
  int8_t *result_cursor, *transitions_block_start, is_final = is_final_state(state);
  Transition *transition;
  TransitionList *transition_list;
  void *loop_memo = NULL;
  *result_size = sizeof(is_final) + links_count * sizeof(struct class_transition_descriptor);
  result_cursor = *result = strict_malloc(*result_size);
  memset(&transition_descriptor, 0, sizeof(transition_descriptor));
  memcpy(result_cursor, &is_final, sizeof(is_final)); result_cursor += sizeof(is_final);
  transitions_block_start = result_cursor;
  transition_list = state_transitions(state);
  while ((transition = next_transition(transition_list, &loop_memo))!= NULL) {
    transition_descriptor.label = transition_label(transition);
    transition_descriptor.target_id = transition_target(transition)->id;
    memcpy(result_cursor, &transition_descriptor, sizeof(struct class_transition_descriptor)); result_cursor += sizeof(transition_descriptor);
  }
  qsort(transitions_block_start, links_count, sizeof(struct class_transition_descriptor), transition_descriptor_comparer);
}

void print_state_sign(int8_t *key, size_t key_size) {
  int8_t *cursor;
  size_t i;
  for (i = 0, cursor = key; i < key_size; i++, cursor++) {
    fprintf(stderr, "%d,", *cursor);
  }
  fprintf(stderr, "\n");
}

void print_state_sign2(State *state) {
  int8_t *key;
  size_t key_size;
  state_description_key(state, &key, &key_size);
  print_state_sign(key, key_size);
  strict_free(key);
}

#ifndef _GNU_SOURCE
void *mempcpy(void *dest, const void *src, size_t n) {
  memcpy(dest, src, n);
  return (int8_t *)dest + n;
}
#endif

/* Функция, аналогичная state_description_key, но генерирующая
   полное описание состояния, по которому его можно будет
   целиком восстановить. */
int serialize_state(State *state, MemBuffer *buffer) {
  size_t links_count = transitions_count(state);
  struct class_transition_descriptor transition_descriptor;
  int8_t is_final = is_final_state(state);
  uint32_t transitions_count;
  Transition *transition;
  TransitionList *transition_list;
  void *loop_memo = NULL;
  StateDescriptionSize result_size;
  result_size = sizeof(state->id) + sizeof(is_final) + sizeof(transitions_count) + links_count * sizeof(transition_descriptor);
  append_to_mem_buffer(buffer, &result_size, sizeof(result_size));
  append_to_mem_buffer(buffer, &state->id, sizeof(state->id));
  append_to_mem_buffer(buffer, &is_final, sizeof(is_final));
  transition_list = state_transitions(state);
  transitions_count = (uint32_t)transition_list_size(transition_list);
  append_to_mem_buffer(buffer, &transitions_count, sizeof(transitions_count));
  while ((transition = next_transition(transition_list, &loop_memo))!= NULL) {
    transition_descriptor.label = transition_label(transition);
    transition_descriptor.target_id = transition_target(transition)->id;
    append_to_mem_buffer(buffer, &transition_descriptor, sizeof(transition_descriptor));
  }
  return 1;
}

/* Воссоздаёт объект состояния из сериализованного представления, созданного
 serialize_state. Переходы не восстанавливаются - это делает отдельная функция
 deserialize_state_transitions. */
static void *deserialize_state(void *data, void *prev_state) {
  uint32_t state_id;
  int8_t is_final, *description = (int8_t *)data;
  State *state;
  int8_t *cursor = description;
  memcpy(&state_id, cursor, sizeof(state_id)); cursor += sizeof(state_id);
  state = make_state(state_id, (State *)prev_state);
  memcpy(&is_final, cursor, sizeof(is_final));
  if (is_final) mark_as_final(state);
  return state;
}

/* Воссоздаёт переходы состояния из сериализованного представления, созданного
 serialize_state. Для этого используется массив состояний states_map,
 в котором id состояний являются индексами массива. */
static void deserialize_state_transitions(void *_state, void *data, void **states_map) {
  State *state = (State *)_state;
  int8_t *description = (int8_t *)data;
  int8_t *cursor = description + sizeof(state->flags) + sizeof(state->id);
  uint32_t transitions_count, i;
  struct class_transition_descriptor transition_descriptor;
  memcpy(&transitions_count, cursor, sizeof(transitions_count));
  cursor += sizeof(transitions_count);
  for (i = 0; i < transitions_count; i++) {
    mempcpy(&transition_descriptor, cursor, sizeof(transition_descriptor));
    cursor += sizeof(transition_descriptor);
    append_state(state, states_map[transition_descriptor.target_id], transition_descriptor.label);
  }
}

/* Отладочная функция, выводящая состояние автомата */
void print_state(State *state) {
  Transition *transition;
  TransitionList *transition_list;
  void *loop_memo = NULL;
  printf("State %d: %s %s\n", state->id, is_final_state(state) ? "final" : "", is_marked_as_registered(state) ? "registered" : "");
  transition_list = state_transitions(state);
  while ((transition = next_transition(transition_list, &loop_memo)) != NULL) {
    printf("  %c -> %d\n", transition_label(transition), transition_target(transition)->id);
  }
}

/* Отладочная функция, выводящая список состояний автомата */
void print_tree(State *root) {
  Transition *transition;
  TransitionList *transition_list;
  void *loop_memo = NULL;
  transition_list = state_transitions(root);
  print_state(root);
  while ((transition = next_transition(transition_list, &loop_memo))!= NULL) {
    print_tree(transition_target(transition));
  }
}

/* Автомат в целом */

Automat *make_automat(void) {
  Automat *automat = strict_malloc(sizeof(Automat));
  automat->last_state_id = 0;
  automat->initial_state = make_state(++automat->last_state_id, NULL);
  automat->class_list = make_hash_table(19);
  return automat;
}

State *initial_state(Automat *automat) {
  return automat->initial_state;
}

void free_automat(Automat *automat) {
  State *next, *state = automat->initial_state;
  do {
    next = state->next;
    free_state(state);
    state = next;
  } while (state != automat->initial_state);
  if (automat->class_list != NULL) {
    free_class_list(automat->class_list);
  }
  strict_free(automat);
}

/* Для указанной последовательности символов word длиной word_length,
   находит самый длинный префикс, существующий в данном автомате и 
   возвращает первое и последнее состояние first_state и last_state,
   соответствующее переходам по символам этого префикса.
   в prefix_size возвращается длина этого максимального префикса.

   Используется и в построении автомата, и в поиске по нему (если
   word_length == prefix_size, то слово было найдено полностью).
*/
void common_prefix(Automat *automat, Label word[], size_t word_length, size_t *prefix_size, State **first_state, State **last_state) {
  Transition *match_transition;
  State *initial = initial_state(automat);
  size_t i;
  *last_state = initial;
  *first_state = initial;
  *prefix_size = 0;
  for (i = 0; i < word_length; i++) {
    match_transition = find_transition(state_transitions(*last_state), word[i]);
    if (match_transition != NULL) {
      (*prefix_size)++;
      *last_state = transition_target(match_transition);
    } else break;
  }
}


/* Регистрирует новый класс эквивалентности состояний, в который
 сразу включается состояние state. */
void add_to_register(Automat *automat, State *state) {
  EquivalenceClass *_class;
  int8_t *class_key;
  size_t class_key_size;
  state_description_key(state, &class_key, &class_key_size);
  _class = add_equivalence_class(&automat->class_list, class_key, class_key_size);
  add_to_equivalence_class(_class, state);
  state->_class = _class;
  strict_free(class_key);
}

/* Ищет состояние, эквивалентное состоянию state. Т.е. одинакового вида (оба - финальные или нет),
   имеющее то же количество переходов, которые ведут на те же состояния, которые уникальны
   в своём классе эквивалентности. */
State *find_equivalent(Automat *automat, State *state) {
  EquivalenceClass *_class = NULL;
  int8_t *class_key;
  size_t class_key_size;
  state_description_key(state, &class_key, &class_key_size);
  _class = hash_table_chain_get(automat->class_list, class_key, class_key_size);
  strict_free(class_key);
  if (_class != NULL) {
    return any_state_from_class(_class);
  }
  return NULL;
}

static void re_register_state(Automat *automat, State *state, int8_t *old_class_key, size_t old_class_key_size) {
  int8_t *class_key;
  size_t class_key_size;
  EquivalenceClass *_class;
  _class = hash_table_chain_get(automat->class_list, old_class_key, old_class_key_size);
  remove_from_equivalence_class(_class, state);
  if (is_empty_equivalence_class(_class)) {
    remove_equivalence_class(automat->class_list, old_class_key, old_class_key_size);
  }
  state_description_key(state, &class_key, &class_key_size);
  _class = hash_table_chain_get(automat->class_list, class_key, class_key_size);
  if (_class != NULL) {
    add_to_equivalence_class(_class, state);
  } else {
    add_to_register(automat, state);
  }
  strict_free(class_key);
}

/* Добавляет ветвь автомата, соответствующую последовательности current_suffix с финальным
   состоянием на конце. Используется в процессе построения автомата. Добавляемая
   ветвь затем минимизируется. */
static void add_suffix(Automat *automat, State *last_state, Label current_suffix[], size_t suffix_size) {
  size_t label_index;
  State *forked_state = last_state;
  int8_t *class_key;
  size_t class_key_size;
  if (is_marked_as_registered(forked_state)) {
    state_description_key(forked_state, &class_key, &class_key_size);
  }
  for (label_index = 0; label_index < suffix_size; label_index++) {
    last_state = append_new_state(last_state, 
				  current_suffix[label_index], 
				  ++automat->last_state_id, 
				  automat->initial_state);
  }
  mark_as_final(last_state);
  if (is_marked_as_registered(forked_state)) {
    re_register_state(automat, forked_state, class_key, class_key_size);
    strict_free(class_key);
  }
}

/* Удаляет ветвь автомата до первого состояния, зарегистрированного
   в классах эквивалентности. Используется в минимизации автомата. */
void delete_branch(State *state) {
  Transition *transition;
  TransitionList *transition_list = state_transitions(state);
  void *loop_memo = NULL;
  if (!is_marked_as_registered(state)) {
    while ((transition = next_transition(transition_list, &loop_memo))!= NULL) {
      delete_branch(transition_target(transition));
    }
    state->prev->next = state->next;
    state->next->prev = state->prev;
    free_state(state);
  }
}

/* Ядро минимизации автомата. Ищет состояния, которые можно 
   исключить, не меняя выводимый автоматом язык, и удаляет их.
   Новые и уникальные состояния регистрируются в классах эквивалентности,
   чтобы опираться на них при удалении будущих дублей. */
void replace_or_register(Automat *automat, State *state) {
  State *child = last_child(state);
  State *equivalent;
  int8_t *class_key;
  size_t class_key_size;
  if (!is_marked_as_registered(child)) {
    if (has_children(child)) {
      replace_or_register(automat, child);
    }
    equivalent = find_equivalent(automat, child);
    if (equivalent != NULL) {
      if (is_marked_as_registered(state)) {
	state_description_key(state, &class_key, &class_key_size);
      }
      delete_branch(child);
      set_last_child(state, equivalent);
      if (is_marked_as_registered(state)) {
	re_register_state(automat, state, class_key, class_key_size);
	strict_free(class_key);
      }
    } else {
      add_to_register(automat, child);
      mark_as_registered(child);
    }
  }
}

/* Добавляет слово в автомат и проводит минимизацию.
   Это единичный шаг построения автомата. После передачи последнего слова,
   необходимо вызвать функцию complete_automat.
   Важно! После вызова complete_automat, добавлять слова в автомат больше нельзя!
   Для корректной работы, слова должны передаваться только в лексикографическом порядке! */
void automat_add_word(Automat *automat, Label word[], size_t size) {
  size_t prefix_size;
  State *first_state, *last_state;
  Label *current_suffix;
  common_prefix(automat, word, size, &prefix_size, &first_state, &last_state);
  /*printf("Last state:%p\n", (void *) last_state);
    printf("Prefix_size:%ld\n", prefix_size);*/
  current_suffix = &word[prefix_size];
  if (has_children(last_state)) {
    replace_or_register(automat, last_state);
  }
  add_suffix(automat, last_state, current_suffix, size - prefix_size);
}

/* Завершает построение автомата. Функция должна быть вызвана после
   добавления последнего слова через automat_add_word. */
void complete_automat(Automat *automat) {
  replace_or_register(automat, initial_state(automat));
  free_class_list(automat->class_list);
  automat->class_list = NULL;
}

/* Перенумеровывает идентификаторы состояний так чтобы они начинались 
   с нулевого и далее увеличивались на единицу, без пробелов.
   Это свойство используется в сохранении и загрузке автомата.
   Возвращает полное число состояний в автомате. */
static uint32_t renumerate_states(Automat *automat) {
  State *state = automat->initial_state;
  automat->last_state_id = 0;
  do {
    set_state_id(state, automat->last_state_id++);
    state = state->next;
  } while (state != automat->initial_state);
  return automat->last_state_id;
}

long int save_automat(Automat *automat, char *file_name) {
  const size_t write_buffer_size = 8192;
  StatesCount states_count = renumerate_states(automat);
  State *state = automat->initial_state;
  int error = 0;
  MemBuffer *write_buffer;
  FILE *file = fopen(file_name, "w+b");
  if (file == NULL) return -1;
  write_buffer = make_mem_buffer(write_buffer_size, file);
  if (append_to_mem_buffer(write_buffer, &states_count, sizeof(states_count)) == 0) {
    do {
      serialize_state(state, write_buffer);
      state = state->next;
    } while (state != automat->initial_state);
  } else {
    error = 1;
  }
  flush_mem_buffer(write_buffer);
  free_mem_buffer(write_buffer);
  fclose(file);
  if (error) return -1;
  return states_count;
}

static void *deserialize_automat(void **states_map, size_t states_count) {
  Automat *automat = make_automat();
  free_state(automat->initial_state);
  automat->initial_state = states_map[0];
  return automat;
}

/* "Ядро" загрузки автомата, используемое разными интерпретациями. 
   Обеспечивает считывание основного каркаса, а дальше просто вызывает
   нужные функции, которые и выполняют остальную инициализацию.
   use_full_preload - (1) стоит ли предварительно загрузить все бинарные представления,
       а лишь затем начать восстанавливать состояния и переходы,
       или же (0) сразу восстанавливать состояние, едва прочитав его из файла.

   callback-функции:
   load_state - создаёт объект состояния из бинарного представления.
   load_state_transitions - вызывается после создания всех состояний, и позволяет восстановить переходы.
   prepare_loaded_automat - создаёт сам автомат, из кучи состояний и переходов.
 */
Automat *load_automat_process(char *file_name,
			      char use_full_preload,
			      DeserializeStateFunction load_state,
			      DeserializeTransitionsFunction load_state_transitions,
			      DeserializeAutomatFunction prepare_loaded_automat) {
  void *automat;
  void **states_map = NULL, *state;
  StatesCount states_count, i;
  int8_t *state_description = NULL, **description_map = NULL;
  StateDescriptionSize state_description_size, description_buffer_size = 0;
  int error = 0;
  /* Загрузим сериализованные состояния */
  FILE *file = fopen(file_name, "rb");
  if (file == NULL) return NULL;
  if (fread(&states_count, 1, sizeof(StatesCount), file) != sizeof(StatesCount)) return NULL;
  if (use_full_preload) {
    description_map = (int8_t **) strict_calloc(states_count, sizeof(int8_t *));
  } else {
    state = NULL;
    states_map = (void **) strict_calloc(states_count, sizeof(void *));
  }
  for (i = 0; i < states_count; i++) {
    if (fread(&state_description_size, 1, sizeof(StateDescriptionSize), file) != sizeof(StateDescriptionSize)) {
      error = 1;
      break;
    }
    if (use_full_preload) {
      state_description = strict_malloc(state_description_size);
    } else {
      if (description_buffer_size < state_description_size) {
        state_description = strict_realloc(state_description, state_description_size);
        description_buffer_size = state_description_size;
      }
    }
    if (fread(state_description, 1, state_description_size, file) != state_description_size) {
      error = 1;
      break;
    }
    if (use_full_preload) {
      description_map[i] = state_description;
    } else {
      state = load_state(state_description, state);
      load_state_transitions(state, state_description, states_map);
      states_map[i] = state;
    }
  }
  if (!use_full_preload) {
    strict_free(state_description);
  }
  fclose(file);
  if (error) {
    if (use_full_preload) {
      for (i = 0; i < states_count; i++) strict_free(description_map[i]);
      strict_free(description_map);
    } else {
      strict_free(states_map);
    }
    return NULL;
  }
  /* Десериализация */
  if (use_full_preload) {
    states_map = (void **) strict_calloc(states_count, sizeof(void *));
    state = NULL;
    for (i = 0; i < states_count; i++) {
      state = load_state(description_map[i], state);
      states_map[i] = state;
    }

    for (i = 0; i < states_count; i++) {
      state_description = description_map[i];
      load_state_transitions(states_map[i], state_description, states_map);
      strict_free(state_description);
    }
  }
  automat = prepare_loaded_automat(states_map, states_count);
  if (use_full_preload) {
    strict_free(description_map);
  }
  strict_free(states_map);
  return automat;
}

Automat *load_automat(char *file_name) {
  return load_automat_process(file_name, 1, deserialize_state, deserialize_state_transitions, deserialize_automat);
}
