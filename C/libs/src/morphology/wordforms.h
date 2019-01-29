#ifndef __MORPH_WORDFORMS_H_
#define __MORPH_WORDFORMS_H_

#include "automat.h"
#include "../common/datastruct.h"

#define ANNOTATION_DELIMITER L'|'
#define ANNOTATION_DELIMITER_STRING  L"|"

/* Описание морфологической формы, включающее специальный
   "аношкинский код", часть речи и грамемы (род, число, падеж и пр.) */
typedef struct {
  wchar_t *ancode;
  wchar_t *part_of_speech;
  wchar_t *grammems;
} Grammar;

typedef HashTable GrammarList;

/* Одиночное флективное правило - что надо
   добавить к лемме справа и слева, чтобы получить
   определённую форму */
typedef struct {
  unsigned int form_no;
  wchar_t *flexion;
  wchar_t *ancode;
  Grammar *grammar;
  wchar_t *prefix;
} FlexVariance;

typedef ArrayList FlexModel;

/* Парадигма словообразования - набор флективных
   правил, характерных для какой-то леммы или
   набора лемм */
typedef struct {
  FlexModel **model_list;
  size_t length;
} FlexModelList;

typedef ArrayList PrefixModel;

/* Префиксы ("квази", "мета" и т.п.). */
typedef struct {
  PrefixModel **prefix_list;
  size_t length;
  wchar_t **all_prefixes;
  size_t all_prefixes_count;
} PrefixModelList;

/* Лемма - неизменная основа слова, от которой
   образуются все вариации */
typedef struct {
  wchar_t *base;
  size_t flex_model_no;
  size_t accent_model_no;
  ssize_t prefix_set_no;
  FlexModel *flex_model;
  PrefixModel *prefix_model;
  wchar_t *ancode; /* Аношкинский код, приписываемый всем вариациям от этой леммы */
} Lemma;

typedef struct {
  Lemma **lemmas;
  size_t length;
} LemmaList;

/* Морфологическая база - набор всех правил, лемм и т.п. 
   необходимый для анализа и генерации */
typedef struct {
  Automat *automat;
  FlexModelList *flex_models;
  PrefixModelList *prefix_models;
  LemmaList *lemmas;
  GrammarList *grammars;
} MorphologyBase;

/* Словоформа - слово, образованное по определённому
   флективному правилу определённой парадигмы. Используется
   при генерации всех словоформ, и при распознавании */
typedef struct {
  wchar_t *word;
  size_t word_length;
  uint16_t flex_model_index; /* Флексическая модель */
  uint8_t flexion_size; /* Длина окончания */
  uint8_t base_size; /* Длина основы */
  int frequency; /* Частота встречаемости */
  Grammar *base_grammar;
  Grammar *grammar;
} WordForm;

/* Вывод автомата на основе некоторого слова.
 Используется в морфологическом анализе. */
typedef struct {
  wchar_t *text;
  /* Размер части слова, являющейся приставкой вида "квази", "мульти" и т.п.*/
  uint8_t known_prefix_size;
  /* Морфологическая аннотация, является указателем на часть text */
  wchar_t *annotation;
  int8_t is_prediction;
  uint8_t automat_prefix_size;
} AutomatOutput;

typedef void (*AutomatOutputProcessor) (char is_prediction, size_t prefix_size, Label buffer[], void *data);

wchar_t *variance_flexion(FlexVariance *variance);
wchar_t *variance_ancode(FlexVariance *variance);
wchar_t *variance_prefix(FlexVariance *variance);

FlexModel *make_flex_model(wchar_t *rules_line, GrammarList *grammar);
size_t flex_model_size(FlexModel *model);
FlexVariance *flex_model_variance(FlexModel *model, size_t i);

void free_flex_model(FlexModel *model);
void free_flex_models(FlexModelList *list);

#define NO_PREFIX_SET -1
Lemma *make_lemma(wchar_t *line, FlexModelList *flex_models, PrefixModelList *prefix_models);
void free_lemma(Lemma *lemma);
wchar_t *lemma_base(Lemma *lemma);
size_t lemma_flex_model_no(Lemma *lemma);
ssize_t lemma_prefix_set_no(Lemma *lemma);
FlexModel *lemma_flex_model(Lemma *lemma);
PrefixModel *lemma_prefix_model(Lemma *lemma);
wchar_t *lemma_ancode(Lemma *lemma);

PrefixModel *make_prefix_model(wchar_t *rules_line);
void free_prefix_model(PrefixModel *model);
size_t prefix_model_size(PrefixModel *model);
wchar_t *prefix_model_item(PrefixModel *model, size_t index);

MorphologyBase *init_morphology_base(char *mrd_file_name, const char *grammar_file_name, char no_load_lemmas);
void free_morphology_base(MorphologyBase *base);
ArrayList *generate_all_words(MorphologyBase *base, size_t max_count);
void free_generated_words(ArrayList *words);
void prepare_words_for_automat(ArrayList *all_forms);
Automat *make_morphology_automat(ArrayList *word_forms);
void possible_outputs(void *automat, 
		      Label word[], size_t word_length, 
		      size_t min_prediction_prefix,
		      AutomatOutputProcessor on_complete,
		      void *data);

/* Шаблоны функций, обобщающих работу с разными реализациями автоматов */
/* Генерирует все возможные выводы для слова word */
typedef void (*AutomatOutputsGenerator)(void *automat,
					Label word[], size_t word_length, 
					size_t min_prediction_prefix,
					AutomatOutputProcessor on_complete,
					void *data);
/* Возвращает длину последовательности символов в инвертированном слове word,
 * которую автомат способен узнать  */
typedef size_t (*AutomatCommonPrefixSize)(void *automat,
                                       Label word[], size_t word_length);

ArrayList *analyze_word(const wchar_t *word, size_t word_length, void *automat, AutomatOutputsGenerator outputs_generator, MorphologyBase *morphology, int8_t only_lemmas, int8_t distinct_ancodes);
void free_analyze_word_results(ArrayList *list);
void build_automat(char *mrd_file_name, char *grammar_file_name, char *automat_file_name);
char word_has_known_prefix(const wchar_t *word, size_t prefix_size, 
			   wchar_t **known_prefixes, size_t known_prefixes_count);

#endif /* __MORPH_WORDFORMS_H_ */
