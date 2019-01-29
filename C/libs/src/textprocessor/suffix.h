#ifndef _TEXTPROCESSOR_SUFFIX_H_
#define _TEXTPROCESSOR_SUFFIX_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Для указанного текста text размером text_size возвращает суффиксный массив
 * длины text_size */
int32_t *text_to_suffix_array(const char *text, size_t text_size);

/* Ищет все вхождения строки sample в тексте text, используя суффиксный массив
 * suffix_array. Результатом являются указатели start_suffix и end_suffix,
 * указывающие на первый и последний элементы суффиксного массива, хранящие
 * позиции вхождения sample в текст text. */
void find_with_suffix_array(const char *sample, size_t sample_length,
                            const char *text, size_t text_length,
                            const int32_t *suffix_array,
                            const int32_t **start_suffix, const int32_t **end_suffix);
#ifdef __cplusplus
}
#endif
  
#endif /* _TEXTPROCESSOR_SUFFIX_H_ */
