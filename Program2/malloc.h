#ifndef _MY_MALLOC_H
#define _MY_MALLOC_H
#include <stddef.h>

#define malloc(x)       my_malloc((x))
#define calloc(x, y)    my_calloc((x), (y))
#define realloc(x, y)   my_realloc((x), (y))
#define free(x)         my_free((x))

typedef struct __attribute__ ((aligned(16))) __attribute__ ((__packed__)) header_h
{
    struct header_h *next;
    unsigned char used;
    unsigned int size;
    unsigned char padding[7];
} Header_h; /* size = 16 bytes */

typedef struct __attribute__ ((aligned(16))) __attribute__ ((__packed__)) heap
{
    Header_h *end;
    unsigned char padding[8];
    Header_h heap;
} Heap; /* size = 32 bytes */

void *my_malloc(size_t size);
void *my_calloc(size_t nmemb, size_t size);
void *my_realloc(void *ptr, size_t size);
void my_free(void *ptr);

#endif