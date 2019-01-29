#ifndef __STRICT_ALLOC_H
#define __STRICT_ALLOC_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

void *strict_calloc(size_t nmemb, size_t size);
void *strict_malloc(size_t size);
void *strict_realloc(void *ptr, size_t size);
void strict_free(void *ptr);

#ifdef __cplusplus
}
#endif
  
#endif /* __STRICT_ALLOC_H */
