#ifndef MORPH_H
#define MORPH_H

#include <stddef.h>
#include <string.h>
#include <time.h>

#include "morphology/helpers.h"
#include "textprocessor/document.h"
#include "common/timer.h"
/*
#define PATH_BASES               "../dicts/"

#ifndef PATH_DICTS
    #define MORPH_PATH_DICTS     PATH_DICTS ""
#else
    #define MORPH_PATH_DICTS     PATH_BASES ""
#endif
*/
#define MORPH_PATH_DICTS    "/usr/local/morph/dicts"

#define MORPH_OK    0
#define MORPH_FAIL -1

/**
 * @brief Структура морфолгического анализатора.
 */
typedef struct morph_s              morph_t;

/**
 * @brief Структура с нормализованной строкой.
 */
typedef struct morph_doc_s          morph_doc_t;

/**
 * @brief Структура с массивом @ref morph_doc_t строк.
 */
typedef struct morph_doc_array_s    morph_doc_array_t;

/**
 * @brief Загрузка морфолгического анализатора.
 * @param Путь до словарей языков, обычно @ref MORPH_PATH_DICTS.
 * @return Указатель на @ref morph_t.
 */
morph_t *morph_new(const char *dictionary_dir);

/**
 * @brief Удаление морфологического анализатора.
 */
void morph_delete(morph_t *m);

/**
 * @brief Создание структуры описывающей нормализованную строку.
 * @param Указатель на @ref morph_t.
 * @param Строка.
 * @param Размер строки.
 * @param 0 - Не кэшировать строку, 1 - Кэшировать строку, не будут заново создавать суффиксные массивы.
 * @return Указатель на @ref morph_doc_t.
 */
morph_doc_t       *morph_doc_new      (morph_t *morphology, const char *str, size_t len, int cache_on);

/**
 * @brief Создание структуры описывающей ненормализованную строку.
 * @param Указатель на @ref morph_t.
 * @param Строка.
 * @param Размер строки.
 * @param 0 - Не кэшировать строку, 1 - Кэшировать строку, не будут заново создавать суффиксные массивы.
 * @return Указатель на @ref morph_doc_t.
 */
morph_doc_t       *morph_doc_new_dont_normal(morph_t *morphology, const char *str, size_t len, int cache_on);

/**
 * @brief Создание структуры описывающей массив нормализованных строк.
 * @param Указатель на @ref morph_t.
 * @param Строка содержащая подстроки которые будут нормализованны.
 * @param Размер строки.
 * @param Разделитель между подстроками.
 * @return Указатель на @ref morph_doc_array_t.
 */
morph_doc_array_t *morph_doc_array_new(morph_t *morphology, const char *str, size_t len, const char *delim);

/**
 * @brief Очищаем @ref morph_doc_t.
 */
void morph_doc_delete(morph_doc_t *morph_doc);

/**
 * @brief Очищаем @ref morph_doc_array_t.
 */
void morph_doc_array_delete(morph_doc_array_t *morph_doc_array);

/**
 * @brief Процентное вхождение подстроки @ref search_s в строку @ref doc_s.
 * @param Указатель на @ref morph_t.
 * @param Cтрока.
 * @param Подстрока.
 * @return Значение от 0 до 1. 0 - Если strlen(search) > strlen(doc).
 */
double morph_doc_intersect_doc (morph_doc_t *doc, morph_doc_t *search);

/**
 * @brief Процентное вхождение большой строки в меньшую.
 * @param Указатель на @ref morph_t.
 * @param Первая строка.
 * @param Вторая строка.
 * @return Значение от 0 до 1.
 */
double morph_doc_intersect_doc2(morph_doc_t *doc, morph_doc_t *search);

/**
 * @brief Процентное вхождение большой строки в меньшую.
 * @param Указатель на @ref morph_t.
 * @param Первая строка.
 * @param Вторая строка.
 * @return Значение от 0 до 1.
 */
double morph_doc_intersect_str2(morph_doc_t *doc, char *search_s);

/**
 * @brief Процентное вхождение подстроки @ref search_s в строку @ref doc_s.
 * @param Указатель на @ref morph_t.
 * @param Cтрока.
 * @param Подстрока.
 * @return Значение от 0 до 1. 0 - Если strlen(search) > strlen(doc).
 */
double morph_str_intersect_str (morph_t *morph, char *doc_s, char *search_s);

/**
 * @brief Процентное вхождение большой строки в меньшую.
 * @param Указатель на @ref morph_t.
 * @param Первая строка.
 * @param Вторая строка.
 * @return Значение от 0 до 1.
 */
double morph_str_intersect_str2(morph_t *morph, char *doc_s, char *search_s);

/**
 * @brief Ищем вхождение подстроки в строку.
 * @param Указатель на @ref morph_t.
 * @param Строка.
 * @param Подстрока.
 * @return 1 - построка содержится, 0 - подстрока не содержится.
 */
int    morph_doc_case_doc(morph_doc_t *doc, morph_doc_t *search);

/**
 * @brief Ищем вхождение подстроки в строку.
 * @param Указатель на @ref morph_t.
 * @param Строка.
 * @param Подстрока.
 * @return 1 - построка содержится, 0 - подстрока не содержится.
 */
int    morph_str_case_str(morph_t *morph, char *doc_s, char *search_s);

/**
 * @brief Приводит строку к нормализованной форме.
 * @param Строка.
 * @param Указатель на морфологический анализатор.
 * @param Размер строки.
 * @return 1 - построка содержится, 0 - подстрока не содержится.
 */
extern char  *morph_normalize_form(const char *source_text, morph_t* morph, size_t text_size);

/**
 * @brief Структураморфологического анализатора.
 */
struct morph_s {
    Morphology      *morphology;
    MultiMorphology *multi_morphology;
};

/**
 * @brief Структура с нормализованной строкой.
 */
struct morph_doc_s {
	/** Указатель на морфолгический анализатор. */
    morph_t        *morphology;
	/** Строка. Может быть нормализованная или нет. */
    char           *str;
    unsigned int    str_crc32;
	/** Длинна строки. */
    size_t          len;
	/** Время создания структуры. */
    time_t          time_create;
    
    DocumentHeader *doc_header;
};

/**
 * @brief Структура с массивом @ref morph_doc_t строк.
 */
struct morph_doc_array_s {
	/** Указатель на морфолгический анализатор. */
    morph_t        *morphology;
	/** Кол-во нормализованных строк в этой структуре. */
    int             size_array; 
    size_t          len;
    char           *str;
    unsigned int    str_crc32;
	/** Время создания структуры. */
    time_t          time_create;
	/** Массив @ref morph_doc_t. */
    morph_doc_t   **morph_doc;
};

#endif // MORPH_H