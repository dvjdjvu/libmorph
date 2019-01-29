/* Компактная и урезанная версия автомата (полная - automat.c) для хранения морфологии языка.
   В ней используется минимум памяти, все структуры очень простые. Но этот
   автомат не предназначен для прямого построения или минимизации. Его
   можно только загрузить из готового файла сохранения, созданного полным
   автоматом из automat.c

   Автор: Кирилл Маврешко <kimavr@gmail.com>
*/

#ifndef __MORPHOLOGY_MINIAUTOMAT_H__
#define __MORPHOLOGY_MINIAUTOMAT_H__

#include <stdlib.h>

#include "automat.h"
#include "wordforms.h"

typedef struct {
  int8_t is_final;
  uint8_t transitions_count;
} MiniStateHeader;

typedef struct mini_transition {
  Label label;
  uint32_t target;
} MiniTransition;

typedef struct {
  uint32_t states_count;
  MiniStateHeader **states;
} MiniAutomat;

void *load_mini_automat(char *automat_file_name);
void free_mini_automat(MiniAutomat *automat);
void mini_possible_outputs(void *automat, 
			   Label word[], size_t word_length, 
			   size_t min_prediction_prefix,
			   AutomatOutputProcessor on_complete,
			   void *data);
size_t mini_common_prefix_size(void *automat, Label word[], size_t word_length);

#endif /* __MORPHOLOGY_MINIAUTOMAT_H__ */
