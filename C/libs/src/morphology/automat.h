/* Конечный автомат с инкрементальным построением и минимизацией. 
   Используется алгоритм Jan Daciuk и Bruce Watson ("Incremental 
   Construction of Minimal Acyclic Finite-State Automata and Transducers,
   and Use in the Natural Language Processing").

   Автор: Кирилл Маврешко <kimavr@gmail.com>
*/

#ifndef __AUTOMAT_H_
#define __AUTOMAT_H_

#include <wchar.h>
#include <stdlib.h>
#include <stdint.h>

#include "../common/hashtable.h"
#include "../common/datastruct.h"

enum automate_state_markers {UNMARKED_STATE = 00, FINAL_STATE = 01, REGISTERED_STATE = 02};

typedef wchar_t Label;

typedef struct transition {
  Label label;
  struct state *target;
} Transition;

typedef ArrayList TransitionList;


typedef struct state {
  TransitionList *transition_list;
  uint32_t id;
  int8_t flags;
  struct equivalence_class *_class;
  /* Используются для обхода состояний при сохранении 
     или освобождении памяти автомата */
  struct state *next;
  struct state *prev;
} State;

typedef struct state_list_item {
  State *state;
  struct state_list_item *next;
} StateListItem;

typedef struct equivalence_class {
  size_t count;
  struct equivalence_class *next;
  StateListItem *state_list;
} EquivalenceClass;

/* Описатель перехода, используемый в state_description_key для
   генерации ключа-описателя класса эквивалентности */
struct class_transition_descriptor {
  Label label; 
  uint32_t target_id;
};

typedef HashTable *EquivalenceClassList;

typedef struct {
  State *initial_state;
  uint32_t last_state_id;
  EquivalenceClassList class_list;
} Automat;

typedef void *(*DeserializeStateFunction)(void *data, void *prev_state);
typedef void (*DeserializeTransitionsFunction)(void *state, void *data, void **states_map);
typedef void *(*DeserializeAutomatFunction)(void **states_map, size_t states_count);

Label transition_label(Transition *transition);
State *transition_target(Transition *transition);

void print_tree(State *root);
void print_state(State *state);
void print_state_sign(int8_t *key, size_t key_size);
void print_state_sign2(State *state);
State *initial_state(Automat *automat);
void state_description_key(State *state, int8_t **result, size_t *result_size);
int8_t is_final_state(State *state);
TransitionList *state_transitions(State *state);
Transition *next_transition(TransitionList *list, void **memo);
Transition *find_transition(TransitionList *list, Label label);

Automat *make_automat(void);
void free_automat(Automat *automat);
void automat_add_word(Automat *automat, Label word[], size_t size);
void complete_automat(Automat *automat);
void common_prefix(Automat *automat, Label word[], size_t word_length, size_t *prefix_size, State **first_state, State **last_state);
long int save_automat(Automat *automat, char *file_name);
Automat *load_automat_process(char *file_name, 
			      char use_full_preload,
			      DeserializeStateFunction load_state,
			      DeserializeTransitionsFunction load_state_transitions,
			      DeserializeAutomatFunction prepare_loaded_automat);
Automat *load_automat(char *file_name);

#endif /* __AUTOMAT_H_ */
