#ifndef _MY_MALLOC_H_
#define _MY_MALLOC_H_
#include <stddef.h>

#define HEAP_CHUNK (2 << 16)

// #define malloc(x)       my_malloc((x))
// #define calloc(x, y)    my_calloc((x), (y))
// #define realloc(x, y)   my_realloc((x), (y))
// #define free(x)         my_free((x))

#define ALIGNMENT 16
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))

typedef struct __attribute__((packed)) header
{
    size_t size : 8*sizeof(size_t) - 1;
    unsigned char used : 1;
    struct header *free_next;
} Header;

void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void free(void *ptr);

#endif