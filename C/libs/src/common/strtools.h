/* Функции общего назначения, используемые в разных частях морфологической библиотеки 

   Автор: Кирилл Маврешко <kimavr@gmail.com>
*/

#ifndef __MORPHOLOGY_UTILS_H__
#define __MORPHOLOGY_UTILS_H__

#include <wchar.h>

#define MORPHOLOGY_DEFAULT_LOCALE "ru_RU.UTF-8"

wchar_t *to_wide_string(const char *text, wchar_t *result, size_t *result_length);
char *to_multibyte_string(const wchar_t *text, size_t *result_length);
void wcssubreverse(wchar_t *start, wchar_t *end);
wchar_t *wcsreverse(wchar_t *word);
wchar_t *ultowcs(unsigned long num, unsigned char base, wchar_t *result);
wchar_t *wcslower(wchar_t *string);
int wcs_simple_comparer(const void *data1, const void *data2);
void strip_text(char *text, const char *strip_chars);
void strip_line(char *line);
wchar_t *to_wide_string_exact(const char *text, size_t length, size_t *result_length);
int is_garbage_word(const wchar_t *word, size_t length);
char *strict_strndup(const char *text, size_t length);
char *join_path(unsigned int chunks_count, ...);

#endif /* __MORPHOLOGY_UTILS_H__ */

