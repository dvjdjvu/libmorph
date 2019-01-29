/* Набор функций для
   - загрузки базы морфологии
   - определения начальной формы слова русского языка (лемматизация),
   - определение всех возможных словоформ указанного слова,
   - морфологический анализ слова: часть речи, число, род, падеж и т.д. 

   Автор: Кирилл Маврешко <kimavr@gmail.com>
*/

#include "morphology/wordforms.h"

#include <wchar.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "common/strict_alloc.h"
#include "common/datastruct.h"
#include "common/strtools.h"
#include "common/hashtable.h"

#define MRD_LINE_BUFFER_SIZE 10240
/* Максимальная длина одного вывода автомата */
#define MAX_AUTOMAT_OUTPUT_SIZE 255
/* Максимальная длина одной словоформы, которая может быть составлена по
 * морфологическому словарю для обучения автомата */
#define MAX_WORD_FORM_SIZE 240
/* Максимальная длина приставки во флективном правиле FlexVariance */
#define MAX_FLEX_PREFIX_SIZE 15
/* Максимальная длина окончания во флективном правиле FlexVariance */
#define MAX_FLEX_FLEXION_SIZE 30
/* Минимальная длина части всего слова, которая должна совпасть с автоматом,
   чтобы можно было делать предсказание словоформ */
#define MIN_MATCH_FOR_PREDICTION 4
/* Минимальная длина части леммы слова, которая должна совпасть с автоматом,
   чтобы можно было делать предсказание словоформ */
#define MIN_BASE_LENGTH 3


size_t read_mrd_file_section_size(FILE *mrd_file) {
  char line_buffer[MRD_LINE_BUFFER_SIZE];
  char *line, *end_of_number;
  line = fgets(line_buffer, MRD_LINE_BUFFER_SIZE, mrd_file);
  if (line) {
    return (size_t) strtoul(line, &end_of_number, 10);
  }
  return 0;
}

wchar_t *mrd_file_next_line(FILE *mrd_file, size_t section_size, char *line_buffer, int line_buffer_size, void **memo) {
  struct iter_state {size_t readed;} *state;
  char *line;
  wchar_t *result = NULL;
  size_t line_length;
  if (*memo == NULL) {
    *memo = state = strict_malloc(sizeof(struct iter_state));
    state->readed = 0;
  } else {
    state = (struct iter_state *) *memo;
  }
  if (state->readed < section_size) {
    line = fgets(line_buffer, line_buffer_size, mrd_file);
    if (line) {
      strip_line(line);
      result = to_wide_string(line, NULL, &line_length);
      state->readed++;
    }
  }
  if (result == NULL) {
    strict_free(state);
    *memo = NULL;
  }
  return result;
}

static void mrd_file_skip_section(FILE *mrd_file, char *line_buffer, int line_buffer_size) {
  char *line;
  size_t section_size = read_mrd_file_section_size(mrd_file);
  while (section_size > 0) {
    line = fgets(line_buffer, line_buffer_size, mrd_file);
    if (line == NULL) {
      section_size = 0;
    }
    section_size--;
  }
}

void init_flex_variance(FlexVariance *variance, unsigned int form_no, wchar_t *flexion, wchar_t *ancode, Grammar *grammar, wchar_t *prefix) {
  variance->form_no = form_no;
  variance->flexion = flexion;
  variance->ancode = ancode;
  variance->prefix = prefix;
  variance->grammar = grammar;
}

void free_flex_variance(void *item) {
  FlexVariance *variance = (FlexVariance *) item;
  strict_free(variance->flexion);
  strict_free(variance->ancode);
  strict_free(variance->prefix);
}

inline wchar_t *variance_flexion(FlexVariance *variance) {
  return variance->flexion;
}

inline wchar_t *variance_ancode(FlexVariance *variance) {
  return variance->ancode;
}

inline wchar_t *variance_prefix(FlexVariance *variance) {
  return variance->prefix;
}

static Grammar *make_grammar(wchar_t *line) {
  Grammar *grammar = strict_malloc(sizeof(Grammar));
  wchar_t *xcode, *memo = NULL;
  grammar->ancode = wcsdup(wcstok(line, L" ", &memo));
  xcode = wcstok(NULL, L" ", &memo);
  grammar->part_of_speech = wcsdup(wcstok(NULL, L" ", &memo));
  grammar->grammems = wcstok(NULL, L" ", &memo);
  if (grammar->grammems != NULL) 
    grammar->grammems = wcsdup(grammar->grammems);
  return grammar;
}

static void free_grammar(Grammar *grammar) {
  strict_free(grammar->ancode);
  strict_free(grammar->part_of_speech);
  strict_free(grammar->grammems);
  strict_free(grammar);
}

static GrammarList *load_grammars(const char *grammar_file_name, char *line_buffer, int line_buffer_size) {
  HashTable *grammars;
  FILE *file = fopen(grammar_file_name, "rt");
  char *line;
  wchar_t *result = NULL;
  Grammar *grammar;
  size_t line_length;
  void **stored_grammar;
  if (file == NULL) return NULL;
  grammars = make_hash_table(10);
  while ((line = fgets(line_buffer, line_buffer_size, file)) != NULL) {
    strip_line(line);
    result = to_wide_string(line, result, &line_length);
    if (*result != L'\0' && result[0] != L'/' && result[1] != L'/') {
      grammar = make_grammar(result);
      stored_grammar = hash_table_get_always(grammars, grammar->ancode, wcslen(grammar->ancode)*sizeof(wchar_t));
      if (*stored_grammar == NULL) {
        *stored_grammar = grammar;
      } else {
        free_grammar(grammar);
      }
    }
  }
  strict_free(result);
  fclose(file);
  return grammars;
}

static Grammar *find_ancode_grammar(GrammarList *grammars, wchar_t *ancode) {
  return hash_table_chain_get(grammars, ancode, sizeof(wchar_t)*wcslen(ancode));
}

static void free_grammar_item(const void *key, size_t key_size, void *value, void *params) {
  free_grammar(value);
}

static void free_grammars(GrammarList *grammars) {
  hash_table_chain_foreach(grammars, free_grammar_item, NULL);
  free_hash_table(grammars);
}

/* Парсит строку rules_line .mrd-файла, и генерирует описание
   одной парадигмы (модели) словообразования, которая состоит их разных сочетаний
   приставок, окончаний и частей речи, которые можно применить к какой-то
   базовой основе. */
FlexModel *make_flex_model(wchar_t *rules_line, GrammarList *grammars) {
  const wchar_t *comment = L"q//q";
  FlexModel *model = make_array_list(sizeof(FlexVariance), 20);
  FlexVariance *variance;
  wchar_t *variance_token, *model_memo = NULL, *variance_memo = NULL, *line, *comment_start;
  unsigned int form_no = 0;
  wchar_t *flexion, *ancode, *prefix;
  wchar_t *variance_splitter;
  Grammar *grammar;
  line = rules_line;
  while ((variance_token = wcstok(line, L"%", &model_memo)) != NULL) {
    line = NULL;
    comment_start = wcsstr(variance_token, comment);
    if (comment_start != NULL) {
      *comment_start = L'\0';
    }
    variance = array_list_append(model, NULL);
    variance_memo = NULL;
   
    variance_splitter = wcschr(variance_token, L'*');
    if (variance_splitter != variance_token) {
      *variance_splitter = L'\0';
      flexion = wcslower(wcsdup(variance_token));
    } else {
      flexion = NULL;
    }
    ancode = variance_splitter + 1;
    variance_splitter = wcschr(ancode, L'*');
    if (variance_splitter != NULL) {
      *variance_splitter = L'\0';
      prefix = wcslower(wcsdup(variance_splitter + 1));
    } else {
      prefix = NULL;
    }
    
    grammar = (grammars != NULL) ? find_ancode_grammar(grammars, ancode) : NULL;
    ancode = wcsdup(ancode);

    init_flex_variance(variance,
		       form_no++,
		       flexion,
		       ancode,
		       grammar,
		       prefix);
  }
  array_list_minimize(model);
  return model;
}

void free_array_listed_ptr(void *pointer) {
  strict_free(*((void **)pointer));
}

void free_flex_model(FlexModel *model) {
  array_list_foreach(model, free_flex_variance);
  free_array_list(model);
}

inline size_t flex_model_size(FlexModel *model) {
  return array_list_size(model);
}

inline FlexVariance *flex_model_variance(FlexModel *model, size_t i) {
  return array_list_get(model, i);
}

/* Считывает из морфологической базы правила словообразования */
FlexModelList *load_flex_models(FILE *mrd_file, char *line_buffer, int line_buffer_size, GrammarList *grammars) {
  wchar_t *line;
  void *memo = NULL;
  size_t section_size = read_mrd_file_section_size(mrd_file);
  FlexModelList *list = strict_malloc(sizeof(FlexModelList));
  FlexModel **cursor = list->model_list = strict_malloc(section_size * sizeof(FlexModel *));
  list->length = section_size;
  while ((line = mrd_file_next_line(mrd_file, section_size, line_buffer, line_buffer_size, &memo)) != NULL) {
    *cursor = make_flex_model(line, grammars);
    strict_free(line);
    cursor++;
  }
  return list;
}

void free_flex_models(FlexModelList *list) {
  size_t i;
  FlexModel **cursor;
  for(i = 0, cursor=list->model_list; i < list->length; i++, cursor++) {
    free_flex_model(*cursor);
  }
  strict_free(list->model_list);
  strict_free(list);
}

PrefixModel *make_prefix_model(wchar_t *rules_line) {
  wchar_t *memo = NULL, *prefix, *line = rules_line;
  PrefixModel *model = make_array_list(sizeof(wchar_t *), 5);
  while ((prefix = wcstok(line, L", ", &memo)) != NULL) {
    line = NULL;
    prefix = wcslower(wcsdup(prefix));
    array_list_append(model, &prefix);
  }
  array_list_minimize(model);
  return model;
}

inline size_t prefix_model_size(PrefixModel *model) {
  return array_list_size(model);
}

inline wchar_t *prefix_model_item(PrefixModel *model, size_t index) {
  return *((wchar_t **) array_list_get(model, index));
}

void free_prefix_model(PrefixModel *model) {
  array_list_foreach(model, free_array_listed_ptr);
  free_array_list(model);
}

/* Считывает из морфологической базы правила присоединения префиксов ("квази","мега" и т.п.) */
static PrefixModelList *load_prefix_models(FILE *mrd_file, char *line_buffer, int line_buffer_size) {
  wchar_t *line, **current_prefix;
  void *memo = NULL;
  size_t section_size = read_mrd_file_section_size(mrd_file), i,k;
  PrefixModelList *list = strict_malloc(sizeof(PrefixModelList));
  PrefixModel **cursor = list->prefix_list = strict_malloc(section_size * sizeof(PrefixModel *));
  list->length = section_size;
  list->all_prefixes_count = 0;
  while ((line = mrd_file_next_line(mrd_file, section_size, line_buffer, line_buffer_size, &memo)) != NULL) {
    *cursor = make_prefix_model(line);
    list->all_prefixes_count += array_list_size(*cursor);
    strict_free(line);
    cursor++;
  }
  current_prefix = list->all_prefixes = strict_malloc(list->all_prefixes_count * sizeof(wchar_t *));
  for(i = 0, cursor=list->prefix_list; i < list->length; i++, cursor++) {
    for(k = 0; k < array_list_size(*cursor); k++) {
      *current_prefix = *(wchar_t **)array_list_get(*cursor, k);
      current_prefix++;
    }
  }
  qsort(list->all_prefixes, list->all_prefixes_count, sizeof(wchar_t *), wcs_simple_comparer);
  return list;
}

void free_prefix_models(PrefixModelList *list) {
  size_t i;
  PrefixModel **cursor;
  for(i = 0, cursor=list->prefix_list; i < list->length; i++, cursor++) {
    free_prefix_model(*cursor);
  }
  strict_free(list->prefix_list);
  strict_free(list->all_prefixes);
  strict_free(list);
}

Lemma *make_lemma(wchar_t *line, FlexModelList *flex_models, PrefixModelList *prefix_models) {
  wchar_t *memo = NULL, *end_of_number = NULL, *token;
  Lemma *lemma = strict_malloc(sizeof(Lemma));
  /* Основа слова */
  token = wcstok(line, L" ", &memo);
  lemma->base = (wcscmp(token, L"#") == 0) ? NULL : wcslower(wcsdup(token));
  /* Модели словообразования и ударения (всегда указаны) */
  lemma->flex_model_no = wcstoul(wcstok(NULL, L" ", &memo), &end_of_number, 10);
  lemma->accent_model_no = wcstoul(wcstok(NULL, L" ", &memo), &end_of_number, 10);
  wcstok(NULL, L" ", &memo); /* Пропускаем описание сессии пользователя */
  /* Код Аношкина */
  token = wcstok(NULL, L" ", &memo);
  lemma->ancode = (wcscmp(token, L"-") == 0) ? NULL : wcsdup(token);
  /* Набор префиксов */
  token = wcstok(NULL, L" ", &memo);
  lemma->prefix_set_no = (wcscmp(token, L"-") == 0) ? NO_PREFIX_SET : wcstol(token, &end_of_number, 10);
  /* Прямые указатели на нужные модели (для упрощения и ускорения) */
  lemma->flex_model = (flex_models != NULL) ? flex_models->model_list[lemma->flex_model_no] : NULL;
  lemma->prefix_model = (prefix_models != NULL && lemma->prefix_set_no > NO_PREFIX_SET) ? prefix_models->prefix_list[lemma->prefix_set_no] : NULL;
  return lemma;
}

void free_lemma(Lemma *lemma) {
  strict_free(lemma->base);
  strict_free(lemma->ancode);
  strict_free(lemma);
}

inline wchar_t *lemma_base(Lemma *lemma) { return lemma->base; }
inline size_t lemma_flex_model_no(Lemma *lemma) { return lemma->flex_model_no; }
inline ssize_t lemma_prefix_set_no(Lemma *lemma) { return lemma->prefix_set_no; }
inline FlexModel *lemma_flex_model(Lemma *lemma) { return lemma->flex_model; }
inline PrefixModel *lemma_prefix_model(Lemma *lemma) { return lemma->prefix_model; }
inline wchar_t *lemma_ancode(Lemma *lemma) { return lemma->ancode; }

static LemmaList *load_lemmas(FILE *mrd_file, char *line_buffer, int line_buffer_size, 
					 FlexModelList *flex_models, PrefixModelList *prefix_models) {
  wchar_t *line;
  void *memo = NULL;
  size_t section_size = read_mrd_file_section_size(mrd_file);
  LemmaList *list = strict_malloc(sizeof(LemmaList));
  Lemma **cursor = list->lemmas = strict_malloc(section_size * sizeof(Lemma *));
  list->length = section_size;
  while ((line = mrd_file_next_line(mrd_file, section_size, line_buffer, line_buffer_size, &memo)) != NULL) {
    *cursor = make_lemma(line, flex_models, prefix_models);
    strict_free(line);
    cursor++;
  }
  return list;
}

void free_lemmas(LemmaList *list) {
  size_t i;
  Lemma **cursor;
  for (i = 0, cursor=list->lemmas; i < list->length; i++, cursor++) {
    free_lemma(*cursor);
  }
  strict_free(list->lemmas);
  strict_free(list);
}

MorphologyBase *init_morphology_base(char *mrd_file_name, const char *grammar_file_name, char no_load_lemmas) {
  MorphologyBase *base = strict_malloc(sizeof(MorphologyBase));
  FILE *mrd_file;
  char line_buffer[MRD_LINE_BUFFER_SIZE];
  base->automat = NULL;
  base->grammars = load_grammars(grammar_file_name, line_buffer, MRD_LINE_BUFFER_SIZE);
  if (base->grammars == NULL) return NULL;
  mrd_file = fopen(mrd_file_name, "rt");
  if (mrd_file == NULL) return NULL;
  base->flex_models = load_flex_models(mrd_file, line_buffer, MRD_LINE_BUFFER_SIZE, base->grammars); /* Модели словообразования */
  mrd_file_skip_section(mrd_file, line_buffer, MRD_LINE_BUFFER_SIZE); /* Пропускаем ударения */
  mrd_file_skip_section(mrd_file, line_buffer, MRD_LINE_BUFFER_SIZE); /* Пропускаем пользовательские сессии */
  base->prefix_models = load_prefix_models(mrd_file, line_buffer, MRD_LINE_BUFFER_SIZE); /* Модели префиксов */
  if (no_load_lemmas) {
    mrd_file_skip_section(mrd_file, line_buffer, MRD_LINE_BUFFER_SIZE); /* Пропускаем леммы слов */
    base->lemmas = NULL;
  } else {
    base->lemmas = load_lemmas(mrd_file, line_buffer, MRD_LINE_BUFFER_SIZE, base->flex_models, base->prefix_models); /* Основы слов */
  }
  fclose(mrd_file);
  return base;
}

void free_morphology_base(MorphologyBase *base) {
  free_grammars(base->grammars);
  free_flex_models(base->flex_models);
  free_prefix_models(base->prefix_models);
  if (base->lemmas != NULL) 
    free_lemmas(base->lemmas);
  strict_free(base);
}

inline LemmaList *morphology_lemmas(MorphologyBase *base) {
  return base->lemmas;
}

/* На основе леммы lemma и флективного правила variance создаёт новую словоформу,
   записывая результат в буфер result. */
static wchar_t *build_word(Lemma *lemma, FlexVariance *variance, wchar_t *result) {
  wchar_t *word_tail = result, *word_base, *flex_prefix, *flex_ancode, *flex_flexion;
  word_base = lemma_base(lemma);
  flex_prefix = variance_prefix(variance);
  flex_ancode = variance_ancode(variance);
  flex_flexion = variance_flexion(variance);
  if (flex_prefix != NULL) word_tail = wcpcpy(word_tail, flex_prefix);
  if (word_base != NULL) word_tail = wcpcpy(word_tail, word_base);
  if (flex_flexion != NULL) word_tail = wcpcpy(word_tail, flex_flexion);
  return word_tail;
}

/* Превращает морфологическую аннотацию в обычную строку, кодируя её в 36-ичной
 системе счисления, для укорачивания. Например число №1234567 будет 
 преобразовано в строку "QJLJ" */
static wchar_t *build_morphology_annotation(uint16_t flex_model_index, uint8_t flexion_size, uint8_t base_size, wchar_t *result) {
  wchar_t number_code[128];
  uint32_t code = 0;
  /* Используя допущения, что число флективных моделей 
     не больше 65536 (в русском словаре их всего ~2600),
     а окончания и приставки не могут быть длиннее 255 символов,
     можно упаковать эту информацию в 4-байтовое целое. Это упрощает
     и ускоряет последующий разбор*/
  code = (uint32_t)flex_model_index << 16;
  code |= (uint32_t)flexion_size << 8;
  code |= (uint32_t)base_size;
  return wcpcpy(result, ultowcs(code, 36, number_code));
}

void parse_morphology_annotation(wchar_t *annotation, WordForm *form) {
  wchar_t *end_of_number = NULL;
  uint32_t flex_code_num;
  flex_code_num = (uint32_t)wcstoul(annotation, &end_of_number, 36);
  form->base_size = flex_code_num & 255;
  form->flexion_size = (flex_code_num >> 8) & 255;
  form->flex_model_index = (uint16_t)(flex_code_num >> 16);
  /*fprintf(stderr, "%ls -> %d %d %d %d\n", annotation, flex_code_num, form->base_size, form->flexion_size, form->flex_model_index);*/
}

/* Генерирует все возможные словоформы из по базе base.
 Если указать max_count > 0, число словоформ будет ограничено 
 указанным количеством (для нужд тестирования).
 Возвращается массив словоформ, содержащий записи WordForm.
*/
ArrayList *generate_all_words(MorphologyBase *base, size_t max_count) {
  ArrayList *result = make_array_list(sizeof(wchar_t *), 1000000);
  LemmaList *lemmas = morphology_lemmas(base);
  unsigned int lemma_index, variances_count, variance_index, counter = 0;
  Lemma **lemma;
  FlexVariance *variance;
  FlexModel *flex_model;
  wchar_t word[MAX_WORD_FORM_SIZE], *word_base, *word_tail, *result_word;
  for (lemma=lemmas->lemmas, lemma_index = 0; lemma_index < lemmas->length; lemma_index++, lemma++) {
    flex_model = lemma_flex_model(*lemma);
    word_base = lemma_base(*lemma);
    variances_count = (unsigned int) flex_model_size(flex_model);
    for (variance_index = 0; variance_index < variances_count; variance_index++) {
      variance = flex_model_variance(flex_model, variance_index);
      word_tail = build_word(*lemma, variance, word);
      wcssubreverse(word, word_tail);
      word_tail = wcpcpy(word_tail, ANNOTATION_DELIMITER_STRING);
      word_tail = build_morphology_annotation((uint16_t) lemma_flex_model_no(*lemma),
					      (uint8_t)(variance->flexion == NULL ? 0 : wcslen(variance->flexion)),
					      (uint8_t)(word_base == NULL ? 0 : wcslen(word_base)), word_tail);
      *word_tail = L'\0';
      result_word = wcsdup(word);
      array_list_append(result, &result_word);
      counter++;
      if (max_count > 0 && counter >= max_count) return result;
    }
  }
  array_list_minimize(result);
  return result;
}

void free_word_form(void *data) {
  free(*(wchar_t **)data);
}

void free_generated_words(ArrayList *words) {
  array_list_foreach(words, free_word_form);
  free_array_list(words);
}

/* Используется для сортировки словоформ по тексту слова */
static int word_form_comparer(const void *form1, const void *form2) {
  return wcscmp(*(wchar_t **)form1, *(wchar_t **)form2);
}

/* Обращает все словоформы (но не морфологическую аннотацию) и сортирует их.
   В таком виде их можно использовать для построения автомата разбора.
 */
void prepare_words_for_automat(ArrayList *all_forms) {
  qsort(array_list_data(all_forms), array_list_size(all_forms), sizeof(wchar_t *), word_form_comparer);
}

/* Строит автомат морфологического разбора на основе словоформ word_forms */
Automat *make_morphology_automat(ArrayList *word_forms) {
  Automat *automat = (Automat *) make_automat();
  wchar_t *sample;
  size_t i;
  size_t percent, last_percent = 0;
  for (i = 0; i < array_list_size(word_forms); i++) {
    sample = *(wchar_t **) array_list_get(word_forms, i);
    percent = 100 * i / array_list_size(word_forms);
    if (percent != last_percent) {
      last_percent = percent;
      fprintf(stderr, "%ld%% words processed. ", percent);
      fprintf(stderr, "Equivalence classes stored %ld. Fill rate:%f\n", 
	      automat->class_list->total_stored, hash_table_fill_rate(automat->class_list));
    }
    automat_add_word(automat, sample, wcslen(sample));
  }
  complete_automat(automat);
  return automat;
}


/* Функция, собирающая все выводы автомата, начиная с указанного состояния
   state, используя при этом временный буфер buffer длиной buffer_size.
   Как только функция достигает состояния автомата с меткой "финальное",
   она вызывает функцию on_complete, передавая ей буфер с выводом до этого
   состояния и некие дополнительные данные data. 
   Используется для морфологического анализа слов.
*/
static void collect_output(State *state,
			   char is_prediction,
			   size_t prefix_size,
			   int depth,
			   Label buffer[], 
			   int buffer_size, 
			   AutomatOutputProcessor on_complete,
			   void *data) {
  TransitionList *transition_list;
  Transition *transition;
  void *loop_memo = NULL;
  if (is_final_state(state)) {
    on_complete(is_prediction, prefix_size, buffer, data);
    if (!is_prediction) return;
  }
  if (depth + 1 < buffer_size) {
    transition_list = state_transitions(state);
    if (depth == 0 && !is_prediction) {
      transition = find_transition(transition_list, ANNOTATION_DELIMITER);
      buffer[depth + 1] = L'\0';
      buffer[depth] = transition_label(transition);
      collect_output(transition_target(transition), is_prediction, prefix_size, depth + 1, buffer, buffer_size, on_complete, data);
    } else {
      while ((transition = next_transition(transition_list, &loop_memo))!= NULL) {
	buffer[depth + 1] = L'\0';
	buffer[depth] = transition_label(transition);
	collect_output(transition_target(transition), is_prediction, prefix_size, depth + 1, buffer, buffer_size, on_complete, data);
      }
    }
  }
}

/* Генерирует список всех возможных выводов автомата (Automat из automat.c) для слова word. 
   Если точного совпадения для слова word не нашлось, и длина уже найденного
   префикса не менее min_prediction_prefix, то будет произведена попытка предсказания
   через вывод всех достижимых далее состояний. Т.о. если min_prediction_prefix >= word_length,
   предсказание производиться не будет.
   В результате работы делаются вызовы функции on_complete, в которую передаётся
   очередной вывод, данные data и флаг, указывающий на то идёт ли сейчас предсказание.
 */
void possible_outputs(void *automat, 
		      Label word[], size_t word_length, 
		      size_t min_prediction_prefix,
		      AutomatOutputProcessor on_complete,
		      void *data) {
  size_t prefix_size;
  State *first_state, *last_state;
  wchar_t buffer[MAX_AUTOMAT_OUTPUT_SIZE];
  common_prefix(automat, word, word_length, &prefix_size, &first_state, &last_state);
  if (prefix_size == word_length && find_transition(state_transitions(last_state), '|')) {
    collect_output(last_state, 0, prefix_size, 0, buffer, MAX_AUTOMAT_OUTPUT_SIZE, on_complete, data);
  } else if (prefix_size >= min_prediction_prefix) {
    collect_output(last_state, 1, prefix_size, 0, buffer, MAX_AUTOMAT_OUTPUT_SIZE, on_complete, data);
  }
}

/* Возвращает массив всех возможных словоформ слова word, допустимых
   для флективной модели flex_model_no. Параметры flexion_size и base_size
   определяют ту часть слова word, которую надо использовать как основу при
   добавлении приставок и окончаний флективной модели. */
WordForm *all_word_variations(const wchar_t *word,
                              size_t word_length,
			      int8_t only_lemma,
			      uint8_t flexion_size, 
			      uint8_t base_size, 
			      uint16_t flex_model_no, 
			      MorphologyBase *base, 
			      size_t *variances_count) {
  FlexModel *model = base->flex_models->model_list[flex_model_no];
  size_t i, part_length;
  WordForm *result, *result_cursor;
  FlexVariance *variance;
  wchar_t *buffer = strict_malloc(sizeof(*buffer) *
                                  (word_length +
                                   MAX_FLEX_PREFIX_SIZE +
                                   MAX_FLEX_FLEXION_SIZE + 1));
  wchar_t *word_base = buffer + MAX_FLEX_PREFIX_SIZE, *word_start;
  size_t result_length = 0;
  *variances_count = only_lemma ? 1 : flex_model_size(model);
  result_cursor = result = strict_malloc(*variances_count * sizeof(WordForm));
  wcsncpy(word_base, word + (word_length - flexion_size - base_size), base_size);
  word_base[base_size] = L'\0';
  for (i = 0; i < *variances_count; i++, result_cursor++) {
    result_length = base_size;
    variance = flex_model_variance(model, i);
    if (variance->prefix != NULL) {
      part_length = wcslen(variance->prefix);
      word_start = word_base - part_length;
      wmemcpy(word_start, variance->prefix, part_length);
      result_length += part_length;
    } else {
      word_start = word_base;
    }
    if (variance->flexion != NULL) {
      wcpcpy(word_base + base_size, variance->flexion);
      (*result_cursor).flexion_size = (uint8_t) wcslen(variance->flexion);
      result_length += (*result_cursor).flexion_size;
    } else {
      *(word_base + base_size) = L'\0';
      (*result_cursor).flexion_size = 0;
    }
    (*result_cursor).word = wcsdup(word_start);
    (*result_cursor).flex_model_index = flex_model_no;
    (*result_cursor).base_size = base_size;
    (*result_cursor).grammar = variance->grammar;
    (*result_cursor).frequency = 0;
    (*result_cursor).word_length = result_length;
  }
  strict_free(buffer);
  return result;
}

/* Проверяет, что часть слова word, размером prefix_size
 состоит из одной или нескольких известных приставок known_prefixes. Так,
даже если автомат не распознал слово полностью, мы можем
узнать, что совпадение было полным, и повысить точность вывода словоформ.
Функция работает рекурсивно, и возвращает истину даже для составных приставок
вида "СУПЕРУЛЬТРАМЕГА".
*/
char word_has_known_prefix(const wchar_t *word, size_t prefix_size, 
			   wchar_t **known_prefixes, size_t known_prefixes_count) {
  wchar_t **test_prefix;
  size_t i, test_prefix_length;
  for (i = 0, test_prefix = known_prefixes; 
       i < known_prefixes_count; 
       i++, test_prefix++) {
    test_prefix_length = wcslen(*test_prefix);
    if (test_prefix_length == prefix_size) {
      if (wcsncmp(*test_prefix, word, test_prefix_length) == 0) {
	return 1;
      }
    } else if (test_prefix_length < prefix_size) {
      if (wcsncmp(*test_prefix, word, test_prefix_length) == 0) {
	return word_has_known_prefix(word + test_prefix_length, 
				     prefix_size - test_prefix_length, 
				     known_prefixes, 
				     known_prefixes_count);
      }
    }
  }
  return 0;
}

void init_automat_output(AutomatOutput *output, wchar_t *text, uint8_t automat_prefix_size, int8_t is_prediction) {
  output->text = wcsdup(text);
  output->annotation = wcschr(output->text, ANNOTATION_DELIMITER);
  *(output->annotation) = L'\0';
  wcssubreverse(output->text, output->annotation);
  output->annotation++;
  output->known_prefix_size = 0;
  output->is_prediction = is_prediction;
  output->automat_prefix_size = automat_prefix_size;
}

void free_automat_output(void *output) {
  strict_free(((AutomatOutput *)output)->text);
}

/* Удаляет выводы автомата, могущие давать неправильные предсказания.
   Иногда слово распозналость полностью, а помешала только какая-то
   приставка. Тогда вывод можно считать не предсказательным, а законченным,
   и предсказательные выводы совсем отбросить. */
void filter_productive_output(ArrayList *outputs, const wchar_t *word, size_t word_length, MorphologyBase *morphology) {
  size_t outputs_count = array_list_size(outputs), output_text_length;
  ssize_t i;
  char drop_prediction_outputs = 0;
  for (i = (ssize_t)outputs_count - 1; i >= 0; i--) {
    AutomatOutput *output = array_list_get(outputs, (size_t)i);
    if (output->is_prediction) {
      output_text_length = wcslen(output->text);
      /* Если автомат упёрся в аннотацию, а нераспознанная часть - приставка, 
       такой вывод можно считать точным, а не предсказательным */
      if (output_text_length == 0 &&
	  word_has_known_prefix(word, 
				word_length - output->automat_prefix_size,
				morphology->prefix_models->all_prefixes,
				morphology->prefix_models->all_prefixes_count)) {
	output->is_prediction = 0;
	output->known_prefix_size = (uint8_t)(word_length - output->automat_prefix_size);
	drop_prediction_outputs = 1;
      } else if (drop_prediction_outputs) {
	free_automat_output(output);
	array_list_delete(outputs, (size_t)i);
      }
    }
  }
  if (drop_prediction_outputs) {
    outputs_count = array_list_size(outputs);
    for (i = (ssize_t)outputs_count - 1; i >= 0; i--) {
      AutomatOutput *output = array_list_get(outputs, (size_t)i);
      if (output->is_prediction) {
	free_automat_output(output);
	array_list_delete(outputs, (size_t)i);
      } else {
	break;
      }
    }
  }
}

static void collect_automat_output(char is_prediction, size_t prefix_size, Label buffer[], void *data) {
  AutomatOutput *output = array_list_append((ArrayList *)data, NULL);
  init_automat_output(output, buffer, (uint8_t) prefix_size, is_prediction);
}

/* Мелкая вспомогательная функция для analyze_word */
static char is_analyzed_model(uint16_t model, int *models, int count) {
  int i;
  for (i = 0; i < count; i++, models++) {
    if (*models == model) return 1;
  }
  return 0;
}

static int is_same_word_form(const void *form1, const void *form2) {
  return wcscmp(((WordForm *)form1)->word, ((WordForm *)form2)->word) == 0;
}

static int is_same_word_form_with_ancode(const void *form1, const void *form2) {
  return wcscmp(((WordForm *)form1)->word, ((WordForm *)form2)->word) == 0 
    && ((WordForm *)form1)->grammar == ((WordForm *)form2)->grammar;
}

static int word_form_frequency_comparer(const void *form1, const void *form2) {
  if (((WordForm *)form1)->frequency == ((WordForm *)form2)->frequency) return 0;
  return (((WordForm *)form1)->frequency > ((WordForm *)form2)->frequency) ? -1 : 1; 
}

static void unique_variations_to_result(WordForm *variations, size_t variations_count, ArrayList *result, EqFunction eq_function) {
  size_t j;
  ssize_t variation_index;
  WordForm *variation;
  for (variation = variations, j = 0; j < variations_count; j++, variation++) {
    variation_index = array_list_index(result, variation, eq_function);
    if (variation_index == -1) {
      array_list_append(result, variation);
    } else {
      ((WordForm *)array_list_get(result, (size_t)variation_index))->frequency++;
      strict_free(variation->word);
    }
  }
  free(variations);
}

/* Анализирует слово, используя морфологическую базу, и 
   выдаёт словоформы этого слова - все, или только начальные 
   флаг only_lemmas указывает, выдавать ли только начальные формы слов или все возможные
   флаг distinct_ancodes указывает, учитывать ли различия в аношкинском коде 
      (фактически - часть речи, падеж и т.п.) при удалении дубликатов.
*/

ArrayList *analyze_word(const wchar_t *word, size_t word_length,
                        void *automat, AutomatOutputsGenerator outputs_generator,
                        MorphologyBase *morphology,
                        int8_t only_lemmas, int8_t distinct_ancodes) {
  ArrayList *outputs = make_array_list(sizeof(AutomatOutput), 10);
  ArrayList *result = make_array_list(sizeof(WordForm), 15);
  size_t i, variations_count, outputs_count, result_size;
  WordForm form;
  int *checked_models, checked_models_count;
  EqFunction eq_func = distinct_ancodes ? is_same_word_form_with_ancode : is_same_word_form;
  wchar_t *reversed_word = strict_malloc(sizeof(wchar_t)*(word_length + 1));
  wmemcpy(reversed_word, word, word_length);
  wcssubreverse(reversed_word, reversed_word + word_length);
  outputs_generator(automat, reversed_word, word_length, MIN_MATCH_FOR_PREDICTION, collect_automat_output, outputs);
  strict_free(reversed_word);
  filter_productive_output(outputs, word, word_length, morphology);
  outputs_count = array_list_size(outputs);
  checked_models = strict_malloc(sizeof(int)*outputs_count);
  checked_models_count = 0;
  for (i=0; i < outputs_count; i++) {
    WordForm *variations;
    AutomatOutput *output = array_list_get(outputs, i);
    ssize_t base_part_size;
    parse_morphology_annotation(output->annotation, &form);
    if (!is_analyzed_model(form.flex_model_index, checked_models, checked_models_count)) {
      if (output->is_prediction) { /* Нужно предсказание */
	base_part_size = (ssize_t) word_length - form.flexion_size;
	if (base_part_size >= MIN_BASE_LENGTH) {
	  variations = all_word_variations(word, word_length, only_lemmas, form.flexion_size, (uint8_t) base_part_size, form.flex_model_index, morphology, &variations_count);
	  unique_variations_to_result(variations, variations_count, result, eq_func);
	}
      } else {
	variations = all_word_variations(word, word_length, only_lemmas, form.flexion_size, (uint8_t)(output->known_prefix_size + form.base_size), form.flex_model_index, morphology, &variations_count);
	unique_variations_to_result(variations, variations_count, result, eq_func);
      }
      checked_models[checked_models_count++] = form.flex_model_index;
    }
  }
  free(checked_models);
  array_list_foreach(outputs, free_automat_output);
  free_array_list(outputs);
  result_size = array_list_size(result);
  if (result_size > 1) {
    qsort(array_list_data(result), result_size, sizeof(WordForm), word_form_frequency_comparer);
  }
  return result;
}

static void free_analyze_variation(void *form) {
  strict_free(((WordForm *)form)->word);
}

void free_analyze_word_results(ArrayList *list) {
  array_list_foreach(list, free_analyze_variation);
  free_array_list(list);
}

/* Генерирует новый автомат морфологического разбора, на основе файлов словаря
 * mrd_file_name и grammar_file_name. Результат работы сохраняется в файл automat_file_name. */
void build_automat(char *mrd_file_name, char *grammar_file_name, char *automat_file_name) {
  MorphologyBase *base;
  ArrayList *words;
  Automat *automat;
  base = init_morphology_base(mrd_file_name, grammar_file_name, 0);
  fprintf(stderr, "Generating word forms...");
  words = generate_all_words(base, 0);
  fprintf(stderr, "OK\nSorting word forms...");
  prepare_words_for_automat(words);
  fprintf(stderr, "OK\nBuilding automat...");
  automat = make_morphology_automat(words);
  fprintf(stderr, "OK\nSaving automat...");
  save_automat(automat, automat_file_name);
  fprintf(stderr, "OK\n");
  free_generated_words(words);
  free_morphology_base(base);
  free_automat(automat);
}
