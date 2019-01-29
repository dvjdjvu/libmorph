#include "common/strict_alloc.h"

#include <stdio.h>
#include <assert.h>

/* Обёртка над стандартным менеджером памяти, для упрощения отладки и 
   предотвращения неправильной работы программы из-за ошибок выделения памяти. */

void *strict_calloc(size_t nmemb, size_t size) {
  void *result = calloc(nmemb, size);
  /*
  if (result == NULL && nmemb && size) {
    perror("Cannot allocate memory with calloc");
    puts("strict_calloc Cannot allocate memory with calloc");
    exit(EXIT_FAILURE);
  }
  */
  return result;
}

void *strict_malloc(size_t size) {
  void *result = malloc(size);
  /*
  if (result == NULL && size) {
    perror("Cannot allocate memory with malloc");
    puts("strict_malloc Cannot allocate memory with calloc");
    exit(EXIT_FAILURE);
  }
  */
  return result;
}

void *strict_realloc(void *ptr, size_t size) {
    void *result = realloc(ptr, size);
    /*
    if (result == NULL && size) {
        perror("Cannot allocate memory with realloc");
        puts("strict_realloc Cannot allocate memory with calloc");
        exit(EXIT_FAILURE);
    }
    */
    return result;
}

void strict_free(void *ptr) {
    free(ptr);
}
