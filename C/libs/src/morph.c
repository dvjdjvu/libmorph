/* 
 * Оболочка для работы с библиотекой admorph.
 * Реализуется в виде библиотеки, ключ компиляции -lmorph.
 *
 * Автор: Сергеев Дмитрий <djvu@inbox.ru>
 */

#include "morph.h"

#define CACHE_SIZE 150

morph_t *
morph_new(const char *dictionary_dir)
{   
    morph_t *morph;
    morph = (morph_t *) calloc(1, sizeof(morph_t));
    if (morph == NULL) {
        return NULL;
    }
    morph->multi_morphology = NULL;
    morph->morphology       = NULL;
 
    if (dictionary_dir == NULL) {
        
        //if ((morph->morphology = init_morphology_bases(dictionary_dir, CACHE_SIZE)) == NULL) {
        //    fprintf(stderr, "Morphology base loading failed1.\n");
        //    return NULL;
        //}
        char *path = (char *) malloc(strlen(MORPH_PATH_DICTS));
        if (path == NULL) {
            fprintf(stderr, "Allocated failed.\n");
            return NULL;
        }
        memcpy(path, MORPH_PATH_DICTS, strlen(MORPH_PATH_DICTS));
        //puts(PATH_DICTS);
        
        if ((morph->multi_morphology = init_multi_morphology(path, CACHE_SIZE)) == NULL) {
            fprintf(stderr, "Morphology base loading failed.\n");
            free(path);
            return NULL;
        }
        
        free(path);
    } else {
        //if ((morph->morphology = init_morphology_bases(dictionary_dir, CACHE_SIZE)) == NULL) {
        //    fprintf(stderr, "Morphology base loading failed1.\n");
        //    return NULL;
        //}
    
        if ((morph->multi_morphology = init_multi_morphology(dictionary_dir, CACHE_SIZE)) == NULL) {
            fprintf(stderr, "Morphology base loading failed.\n");
            return NULL;
        }        
    }
            
    return morph;
}

void 
morph_delete(morph_t *morphology)
{
    if (morphology->morphology) {
        unload_morphology_bases(morphology->morphology);
    }

    if (morphology->multi_morphology) {
        free_multi_morphology(morphology->multi_morphology);
    }
}

morph_doc_t *
morph_doc_new(morph_t *morphology, const char *str, size_t len, int cache_on)
{
    size_t normal_len = len;
    char *normal_str  = normalize_text(str, &normal_len);
    
    morph_doc_t *morph_doc;
    morph_doc = (morph_doc_t *) calloc(1, sizeof(morph_doc_t));
    if (morph_doc == NULL) {
        fprintf(stderr, "morph_doc_new () Create doc morphology failed.\n");
        return NULL;
    }
    
    morph_doc->len        = len;
    morph_doc->morphology = morphology;
    
    morph_doc->str = (char *) calloc(normal_len+1, sizeof(char));
    if (morph_doc->str == NULL) {
        fprintf(stderr, "morph_doc_new () Create doc morphology failed.\n");
        return NULL;
    }
    memcpy((void *) (morph_doc->str), (void *) normal_str, normal_len);
    
    if (cache_on) {
        morph_doc->doc_header = (DocumentHeader *) make_document(normal_str, 0, morph_doc->morphology->multi_morphology, &normal_len);
    } else {
        morph_doc->doc_header = NULL;
    }
    
    free(normal_str);
    
    morph_doc->time_create = time(NULL);
    
    return morph_doc;
}

morph_doc_t *
morph_doc_new_dont_normal(morph_t *morphology, const char *str, size_t len, int cache_on)
{
    size_t _len = len;
    
    morph_doc_t *morph_doc;
    morph_doc = (morph_doc_t *) calloc(1, sizeof(morph_doc_t));
    if (morph_doc == NULL) {
        fprintf(stderr, "morph_doc_new_dont_normal() Create doc morphology failed.\n");
        return NULL;
    }
    
    morph_doc->len        = len;
    morph_doc->morphology = morphology;
    
    morph_doc->str = (char *) calloc(len+1, sizeof(char));
    if (morph_doc->str == NULL) {
        fprintf(stderr, "morph_doc_new_dont_normal() Create doc morphology failed.\n");
        return NULL;
    }
    memcpy((void *) (morph_doc->str), (void *) str, len);
    
    if (cache_on) {
        morph_doc->doc_header = (DocumentHeader *) make_document(str, 0, morph_doc->morphology->multi_morphology, &_len);
    } else {
        morph_doc->doc_header = NULL;
    }
    
    morph_doc->time_create = time(NULL);
    
    return morph_doc;
}

morph_doc_array_t *
morph_doc_array_new(morph_t *morphology, const char *str, size_t len, const char *delim)
{
    int count_phrase  = 1;
    
    size_t normal_len = 0;
    char  *normal_str = NULL;
    
    int i, j;
    char *str1 = NULL, *str2 = NULL, *token = NULL, *subtoken = NULL;
    char *saveptr1 = NULL, *saveptr2 = NULL;
    
    morph_doc_array_t *morph_doc_array = (morph_doc_array_t *) calloc(1, sizeof(morph_doc_array_t));
    if (morph_doc_array == NULL) {
        fprintf(stderr, "Create doc morphology failed.\n");
        return NULL;
    }
    
    morph_doc_array->str = (char *) calloc(len+1, sizeof(char));
    if (morph_doc_array->str == NULL) {
        return NULL;
    }
    memcpy((void *) (morph_doc_array->str), (void *) str, len);
    
    morph_doc_array->len        = len;
    
    for (i = 0; i < len; i++) {
        for (j = 0; j < strlen(delim); j++) {
            if (!strncmp(&str[i], &delim[j], 1)) {
                if (i == len - 1) {
                    break;
                }
                count_phrase++;
                break;
            }
        }
    }

    morph_doc_array->size_array = count_phrase;
    morph_doc_array->morphology = morphology;

    morph_doc_array->morph_doc = (morph_doc_t **) calloc(count_phrase, sizeof(morph_doc_t *));
    if (morph_doc_array->morph_doc == NULL) {
        fprintf(stderr, "Create doc morphology failed.\n");
        return NULL;
    }

    for (i = 0, str2 = (char *) str; i < count_phrase; i++, str2 = NULL) {
        token = strtok_r(str2, delim, &saveptr2);
        if (token == NULL) {
            break;
        }
        
        morph_doc_array->morph_doc[i] = (morph_doc_t *) calloc(1, sizeof(morph_doc_t));
        if (morph_doc_array->morph_doc[i] == NULL) {
            fprintf(stderr, "Create doc morphology failed.\n");
            return NULL;
        }
        
        normal_len = strlen(token);
        normal_str = normalize_text(token, &normal_len);

        morph_doc_array->morph_doc[i]->len        = strlen(token);
        morph_doc_array->morph_doc[i]->morphology = morphology;
    
        morph_doc_array->morph_doc[i]->str = (char *) calloc(normal_len+1, sizeof(char));
        if (morph_doc_array->morph_doc[i]->str == NULL) {
            fprintf(stderr, "Create doc morphology failed.\n");
            return NULL;
        }

        memcpy((void *) (morph_doc_array->morph_doc[i]->str), (void *) normal_str, normal_len);
        
        morph_doc_array->morph_doc[i]->doc_header = (DocumentHeader *) make_document(normal_str, 0, morph_doc_array->morph_doc[i]->morphology->multi_morphology, &normal_len);
        
        free(normal_str);
    }
    
    morph_doc_array->time_create = time(NULL);
    
    return morph_doc_array;
}

void 
morph_doc_delete(morph_doc_t *morph_doc)
{
    if (morph_doc != NULL) {
        free(morph_doc->str);
        free_document(morph_doc->doc_header);
    
        free(morph_doc);
    }
}

void 
morph_doc_array_delete(morph_doc_array_t *morph_doc_array)
{
    int i;
    
    if (morph_doc_array == NULL) {
        return;
    }
    
    free(morph_doc_array->str);
    
    for (i = 0; i < morph_doc_array->size_array; i++) {
        //free(morph_doc_array->morph_doc[i]->str);
        //free_document(morph_doc_array->morph_doc[i]->doc_header);
        //free(morph_doc_array->morph_doc[i]);
        
        morph_doc_delete(morph_doc_array->morph_doc[i]);
    }
    
    free(morph_doc_array);
}

// ищем вхождение подстроки search в строку doc и возвращаем процентное вхождение одной строки в другую
// если длинна search больше doc, то результатом вхождения будет 0.
// порядок сравнения важен.
double
morph_doc_intersect_doc(morph_doc_t *doc, morph_doc_t *search)
{
    char *result = NULL;
    size_t search_result_length;
    int i = 0;
    char *str1 = NULL, *token = NULL;
    char *saveptr1 = NULL;
    
    //printf("%s %s\n", doc->str, search->str);
    
    for (str1 = search->str; ; str1 = NULL) {
        token = strtok_r(str1, " ", &saveptr1);
        if (token == NULL) {
            break;
        }
        
        result = document_find_multi_intersection(doc->doc_header, doc->morphology->multi_morphology, token, &search_result_length);
        if (result != NULL) {
            i += (strlen(result));
        }
        free(result);
    }

    //printf("%d %d %d\n", i, doc->len, strlen(doc->str));
    
    if (search->len > doc->len) {
        return 0.0;    
    }
    
    if ((double)i >= (double)(doc->len)) {
        return 1.0;
    } else {
        return (double) ((double)i/(double)doc->len);
    }
}

// возвращаем процентное вхождение одной строки в другую(большой в меньшую)
double
morph_doc_intersect_doc2(morph_doc_t *doc, morph_doc_t *search)
{
    char *result = NULL;
    size_t search_result_length;
    int i = 0;
    char *str1 = NULL, *token = NULL;
    char *saveptr1 = NULL;
    
    //printf("%s %s\n", doc->str, search->str);
    
    for (str1 = search->str; ; str1 = NULL) {
        token = strtok_r(str1, " ", &saveptr1);
        if (token == NULL) {
            break;
        }
        
        result = document_find_multi_intersection(doc->doc_header, doc->morphology->multi_morphology, token, &search_result_length);
        if (result != NULL) {
            i += (strlen(result));
        }
        free(result);
    }
    
    if ((double)i >= (double)(doc->len)) {
        return 1.0;
    } else {
        return (double) ((double)i/(double)doc->len);
    }
}

double 
morph_str_intersect_str(morph_t *morph, char *doc_s, char *search_s)
{
    double pr = 0.0;
    morph_doc_t *doc    = NULL;
    morph_doc_t *search = NULL;
    do {
        doc    = morph_doc_new(morph, doc_s,    strlen(doc_s),    1);
        search = morph_doc_new(morph, search_s, strlen(search_s), 0);
        if (search == NULL || doc == NULL) {
            break;
        }
        pr = morph_doc_intersect_doc(doc, search);
    } while(0);
    
    morph_doc_delete(doc);
    morph_doc_delete(search);
    
    return pr;
}

double 
morph_str_intersect_str2(morph_t *morph, char *doc_s, char *search_s)
{
    double pr = 0.0;
    morph_doc_t *doc    = NULL;
    morph_doc_t *search = NULL;
    do {
        doc    = morph_doc_new(morph, doc_s,    strlen(doc_s),    1);
        search = morph_doc_new(morph, search_s, strlen(search_s), 0);
        if (search == NULL || doc == NULL) {
            break;
        }
        pr = morph_doc_intersect_doc2(doc, search);
    } while(0);
    
    
    morph_doc_delete(doc);
    morph_doc_delete(search);
    
    return pr;
}

double 
morph_doc_intersect_str2(morph_doc_t *doc, char *search_s)
{
    double pr = 0.0;
    
    morph_doc_t *search = morph_doc_new_dont_normal(doc->morphology, search_s, strlen(search_s), 0);
    if (search == NULL) {
        return 0;
    }
    pr = morph_doc_intersect_doc2(doc, search);
    
    morph_doc_delete(search);
    
    return pr;
}

// Проверяет входит ли подстрока search в строку doc.
int
morph_doc_case_doc(morph_doc_t *doc, morph_doc_t *search)
{
    char *result = NULL;
    size_t search_result_length;
    int i;
       
    result = document_find_multi_intersection(doc->doc_header, doc->morphology->multi_morphology, search->str, &search_result_length);
    if (result != NULL) {
        free(result);
        return 1;
    }
    
    return 0;
}

int
morph_str_case_str(morph_t *morph, char *doc_s, char *search_s)
{
    char *result = NULL;
    size_t search_result_length;
    int pr;
    
    morph_doc_t *doc    = NULL;
    morph_doc_t *search = NULL;
    do {
        
        doc    = morph_doc_new(morph, doc_s,    strlen(doc_s),    1);
        search = morph_doc_new(morph, search_s, strlen(search_s), 0);
        if (search == NULL || doc == NULL) {
            break;
        }
        
        result = document_find_multi_intersection(doc->doc_header, doc->morphology->multi_morphology, search->str, &search_result_length);
        
    } while(0);
    
    morph_doc_delete(doc);
    morph_doc_delete(search);
    
    if (result != NULL) {
        free(result);
        return 1;
    }
    
    return 0;
}

inline char *
morph_normalize_form(const char *source_text, morph_t* morph, size_t text_size)
{
    return normalize_morph_form(source_text, morph->multi_morphology, text_size);
}

