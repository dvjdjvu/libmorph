/* Функции начального преобразования очищенного от HTML'а текста документа. В
 * частности, разбиение текста на слова и приведение к нижнему регистру.
 *
 * Особенность: Код может неправильно работать на платформах, где размер типа
 * wchar_t меньше 32 бит (Windows). В Linux проблем нет (во FreeBSD тоже не
 * должно быть).
 * 
 * Автор: Кирилл Маврешко <kimavr@gmail.com>
 */
#include "textprocessor/tokenizer.h"

#include <wchar.h>
#include <locale.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wctype.h>

#include "common/strict_alloc.h"
#include "common/datastruct.h"
#include "common/strtools.h"

enum state_code {INSIDE_TOKEN, POST_EXTRA_SYMBOL, OUTSIDE_TOKEN, TOKEN_READY, FINAL};

struct tokenizer_state {
    const char *text_position;
    mbstate_t mbstate;
    enum state_code code;
    ArrayList *wide_token;
};

/* Символы, формально считающиеся разделителями, но допустимо входящие в состав слов,
если не находятся в начале или конце слова */
static const wchar_t kExtraAllowedInWord[] = {L'-', L'\'', L'`', L'_', 0L};
static const wchar_t kWideTerminator = L'\0';

static inline int is_terminator(wchar_t c) { return c == kWideTerminator; }

static inline int is_extra_token_char(wchar_t symbol) {
  return (!is_terminator(symbol) && wcsrchr(kExtraAllowedInWord, symbol) != NULL);
}

void final_tokenize(void *memo) {
  if (((struct tokenizer_state *)memo)->wide_token != NULL) 
    free_array_list(((struct tokenizer_state *)memo)->wide_token);
  strict_free(memo);
}

/* Эквивалент функции strtok_r, но с более хитрой логикой.
Работает с UTF-8-строками, считая словами любые алфавитно-цифровые
последовательности (язык не важен), разделённые всеми прочими символами.
Умеет целиком распознавать слова вроде "don't", "как-нибудь" и т.п.
Токен выдаётся в двух форматах - "широкой строкой" (wchar_t) - для библиотеки
морфологии и обычной, многобайтовой - для последующего хранения.
После каждого вызова
  token_start указывает на первую букву (char) токена
  token_end указывает на последнюю букву (char) токена
  wide_token указывает на строку с текстом токена в UTF-32
Возвращает длину токена в char-символах (т.е. в байтах, если sizeof(char) == 1).
*/
ssize_t tokenize(const char *text, const char **token_start, const char **token_end, const wchar_t **wide_token, void **memo) {
  const int error_code = -1;
  char *old_locale;
  struct tokenizer_state *state;
  mbstate_t prev_mbstate;
  wchar_t char_buffer[2], current_symbol;
  size_t char_size = 0, prev_char_size;
  const char *prev_text_position;
  ssize_t retval = 0;
  if (text == NULL) {
    /* Продолжаем обход */
    if (*token_end == NULL) {
      /* Лишний вызов. Обход был завершён на предыдущей итерации */
      return 0;
    } else {
      state = *memo;
      *token_start = *token_end = NULL;
      state->code = OUTSIDE_TOKEN;
      if (wide_token != NULL) {
        *wide_token = NULL;
        array_list_shrink(state->wide_token, 0);
      }
    }
  } else {
    /* Первый вызов */
    *token_start = *token_end = NULL;
    *memo = state = strict_calloc(1, sizeof(*state));
    state->text_position = text;
    if (wide_token != NULL) {
      *wide_token = NULL;
      state->wide_token = make_array_list(sizeof(wchar_t), 20);
    } else {
      state->wide_token = NULL;
    }
    state->code = OUTSIDE_TOKEN;
  }
  
  old_locale = setlocale(LC_CTYPE, MORPHOLOGY_DEFAULT_LOCALE);
  do {  
    prev_text_position = state->text_position;
    prev_mbstate = state->mbstate;
    prev_char_size = char_size;
    if (state->text_position != NULL) {
      char_size = mbsrtowcs(char_buffer, &state->text_position, 1, &state->mbstate);
      current_symbol = *char_buffer;
    }
    
    if (char_size == (size_t)-1) {
      /* Ошибка в ходе разбора строки */
      state->code = FINAL;
      retval = error_code;
    } else if (char_size == 0) {
      if (prev_char_size > 0) {
        current_symbol = kWideTerminator;
      } else {
        state->code = FINAL;
        retval = 0;
      }
    }
    
    switch (state->code) {
      case OUTSIDE_TOKEN:
        if (iswalnum((wint_t)current_symbol)) {
          *token_start = prev_text_position;
          if (wide_token != NULL) array_list_append(state->wide_token, &current_symbol);
          state->code = INSIDE_TOKEN;
        }
        break;
        
      case INSIDE_TOKEN:
        if (iswalnum((wint_t)current_symbol)) {
          if (wide_token != NULL) array_list_append(state->wide_token, &current_symbol);
        } else if (is_extra_token_char(current_symbol)) {
          state->code = POST_EXTRA_SYMBOL;
          if (wide_token != NULL) array_list_append(state->wide_token, &current_symbol);
        } else {
          *token_end = prev_text_position - 1;
          retval = *token_end - *token_start + 1;
          if (wide_token != NULL) {
            array_list_append(state->wide_token, &kWideTerminator);
            *wide_token = array_list_data(state->wide_token);
          }
          state->code = TOKEN_READY;
        }
        break;
        
      case POST_EXTRA_SYMBOL:
        if (iswalnum((wint_t)current_symbol)) {
          if (wide_token != NULL) array_list_append(state->wide_token, &current_symbol);
          state->code = INSIDE_TOKEN;
        } else {
          /* Прокрутим назад */
          state->mbstate = prev_mbstate;
          state->text_position = prev_text_position;
          *token_end = prev_text_position - prev_char_size - 1;
          retval = *token_end - *token_start + 1;
          if (wide_token != NULL) {
            array_list_put(state->wide_token, array_list_size(state->wide_token) - 1, &kWideTerminator);
            *wide_token = array_list_data(state->wide_token);
          }
          state->code = TOKEN_READY;
        }
        break;
        
      case TOKEN_READY:
        break;
        
      case FINAL:
        *token_start = *token_end = NULL;
        if (wide_token != NULL) {
          *wide_token = NULL;
        }
        final_tokenize(state);
        state = *memo = NULL;
        break;
    }
  } while (state != NULL && state->code != FINAL && state->code != TOKEN_READY);
  setlocale(LC_CTYPE, old_locale);
  return retval;
}
