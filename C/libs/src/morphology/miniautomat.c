/* Компактная и урезанная версия автомата (полная - automat.c) для хранения морфологии языка.
   В ней используется минимум памяти, все структуры очень простые. Но этот
   автомат не предназначен для прямого построения или минимизации. Его
   можно только загрузить из готового файла сохранения, созданного полным
   автоматом из automat.c

   Автор: Кирилл Маврешко <kimavr@gmail.com>
*/

#include "morphology/miniautomat.h"

#include <stdint.h>
#include <string.h>

#include "morphology/automat.h"
#include "morphology/wordforms.h"
#include "common/strict_alloc.h"

#define MAX_AUTOMAT_OUTPUT_SIZE 255

static int transitions_comparer(const void *t1, const void *t2) {
  if (((MiniTransition *)t1)->label == ((MiniTransition *)t2)->label) return 0;
  return ((MiniTransition *)t1)->label < ((MiniTransition *)t2)->label ? -1 : 1;
}

static void *decode_state(void *data, void *prev_state) {
  uint32_t state_id;
  int8_t is_final, *description = (int8_t *)data;
  MiniStateHeader *state;
  int8_t *cursor = description;
  uint32_t i, transitions_count;
  struct class_transition_descriptor transition_descriptor;
  MiniTransition *transition, *transitions_start;
  state_id = *(uint32_t *)cursor; cursor += sizeof(state_id);
  is_final = *(int8_t *)cursor; cursor += sizeof(is_final);
  transitions_count = *(uint32_t *)cursor; cursor += sizeof(transitions_count);
  state = (MiniStateHeader *) malloc(sizeof(MiniStateHeader) + sizeof(MiniTransition)*transitions_count);
  transitions_start = transition = (MiniTransition *)(state + 1);
  state->is_final = is_final;
  state->transitions_count = (uint8_t) transitions_count;
  for (i = 0; i < transitions_count; i++) {
    memcpy(&transition_descriptor, cursor, sizeof(transition_descriptor));
    cursor += sizeof(transition_descriptor);
    transition->target = transition_descriptor.target_id;
    transition->label = transition_descriptor.label;
    transition++;
  }
  qsort(transitions_start, transitions_count, sizeof(MiniTransition), transitions_comparer);
  return state;
}

static void decode_transition(void *state, void *data, void **states_map) {}

static void *prepare_mini_automat(void **states_map, size_t states_count) {
  MiniAutomat *automat = malloc(sizeof(MiniAutomat));
  automat->states = (MiniStateHeader **) malloc(sizeof(MiniStateHeader *)*states_count);
  memcpy(automat->states, states_map, sizeof(MiniStateHeader *)*states_count);
  automat->states_count = (uint32_t) states_count;
  return automat;
}

void *load_mini_automat(char *automat_file_name) {
  return load_automat_process(automat_file_name, 0, decode_state, decode_transition, prepare_mini_automat);
}

void free_mini_automat(MiniAutomat *automat) {
  MiniStateHeader **state;
  uint32_t i;
  for (state = automat->states, i = 0; i < automat->states_count; i++, state++) {
    strict_free(*state);
  }
  strict_free(automat->states);
  strict_free(automat);
}

inline MiniStateHeader *mini_find_transition(MiniAutomat *automat, MiniStateHeader *state, Label label) {
  MiniTransition *transitions = (MiniTransition *)(state + 1);
  MiniTransition *transition, *left, *right;
  /* Двоичный поиск */
  if (state->transitions_count > 0) {
    left = transitions; right = transitions + state->transitions_count - 1;
    do {
      transition = left + ((right - left) >> 1);
      if (transition->label == label) return automat->states[transition->target];
      else {
        if (transition->label < label) {
          left = transition + 1;
        } else {
          right = transition - 1;
        }
      }
    } while (left <= right);
  }
  return NULL;
}

void mini_collect_output(MiniAutomat *automat,
			 MiniStateHeader *state,
			 char is_prediction,
			 size_t prefix_size,
			 int depth,
			 Label buffer[], 
			 int buffer_size, 
			 AutomatOutputProcessor on_complete,
			 void *data) {

  MiniStateHeader *target;
  if (state->is_final) {
    on_complete(is_prediction, prefix_size, buffer, data);
    if (!is_prediction) return;
  }
  if (depth + 1 < buffer_size) {
    if (depth == 0 && !is_prediction) {
      target = mini_find_transition(automat, state, ANNOTATION_DELIMITER);
      buffer[depth] = ANNOTATION_DELIMITER;
      buffer[depth + 1] = L'\0';
      mini_collect_output(automat, target, is_prediction, prefix_size, depth + 1, buffer, buffer_size, on_complete, data);
    } else {
      MiniTransition *transition = (MiniTransition *)(state + 1);
      uint8_t i = 0;
      for (i = 0; i < state->transitions_count; i++, transition++) {
	buffer[depth] = (*transition).label;
	buffer[depth + 1] = L'\0';
	mini_collect_output(automat, automat->states[(*transition).target], is_prediction, prefix_size, depth + 1, buffer, buffer_size, on_complete, data);
      }
    }
  }
}

void mini_common_prefix(MiniAutomat *automat, Label word[], size_t word_length, size_t *prefix_size, MiniStateHeader **last_state) {
  MiniStateHeader *match_transition;
  MiniStateHeader *initial = automat->states[0];
  size_t i;
  *last_state = initial;
  *prefix_size = 0;
  for (i = 0; i < word_length; i++) {
    match_transition = mini_find_transition(automat, *last_state, word[i]);
    if (match_transition != NULL) {
      (*prefix_size)++;
      *last_state = match_transition;
    } else break;
  }
}

/* Определяет максимальную длину куска инвертированного слова word, которую
 * автомат ещё может "узнать". Если точно такое слово уже есть в автомате,
 * возвращаемая длина будет точно совпадать со значением word_length.
 * Используется в определении языка для неизвестных слов: слово предлагается
 * автоматам всех языков, а затем выбирается тот язык, автомат которого "узнал"
 * слово полнее всего. */
size_t mini_common_prefix_size(void *automat, Label word[], size_t word_length) {
    size_t prefix_size;
    MiniStateHeader *last_state;
    mini_common_prefix(automat, word, word_length, &prefix_size, &last_state);
    return prefix_size;
}

/* Генерирует список всех возможных выводов автомата для слова word. 
   Если точного совпадения для слова word не нашлось, и длина уже найденного
   префикса не менее min_prediction_prefix, то будет произведена попытка предсказания
   через вывод всех достижимых далее состояний. Т.о. если min_prediction_prefix >= word_length,
   предсказание производиться не будет.
   В результате работы делаются вызовы функции on_complete, в которую передаётся
   очередной вывод, данные data и флаг, указывающий на то идёт ли сейчас предсказание.
 */
void mini_possible_outputs(void *automat,
			   Label word[], size_t word_length, 
			   size_t min_prediction_prefix,
			   AutomatOutputProcessor on_complete,
			   void *data) {
  size_t prefix_size;
  MiniStateHeader *last_state;
  wchar_t buffer[MAX_AUTOMAT_OUTPUT_SIZE];
  mini_common_prefix(automat, word, word_length, &prefix_size, &last_state);
  if (prefix_size == word_length && mini_find_transition(automat, last_state, ANNOTATION_DELIMITER)) {
    mini_collect_output(automat, last_state, 0, prefix_size, 0, buffer, MAX_AUTOMAT_OUTPUT_SIZE, on_complete, data);
  } else if (prefix_size >= min_prediction_prefix) {
    mini_collect_output(automat, last_state, 1, prefix_size, 0, buffer, MAX_AUTOMAT_OUTPUT_SIZE, on_complete, data);
  }
}
