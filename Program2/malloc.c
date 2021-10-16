#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "malloc.h"

#define CHUNK (2 << 16)
#define true 1
#define false 0
#define MINCHUNK sizeof(Header_h) + 16
#define STR_SIZE 128

static Heap *heap_struct = NULL;
static char str_hold[STR_SIZE];

enum err { HEAP_FULL, BRK_ERR, HEAP_START, FREE, STAT };

void my_fprintf(enum err type, FILE *file)
{
    memset(str_hold, 0, STR_SIZE);
    switch(type)
    {
        case HEAP_FULL :
            snprintf(str_hold, 12, "Heap full.\n");
            break;
        case BRK_ERR :
            snprintf(str_hold, 33, "Brk could not reduce heap size.\n");
            break;
        case HEAP_START :
            snprintf(str_hold, 28, "Could not initialize heap.\n");
            break;
        case FREE :
            snprintf(str_hold, 15, "Invalid free.\n");
            break;
        case STAT :
            snprintf(str_hold, 35, "You are using the malloc library!\n");
            break;
        default :
            snprintf(str_hold, 24, "Unknown error occured.\n");
            break;
    }
    fputs(str_hold, file);
}

void init_heap()
{
    if( (heap_struct = sbrk(sizeof(Heap) + CHUNK)) < 0)
    {
        my_fprintf(HEAP_FULL, stderr);
        return;
    }
    heap_struct->end = (Header_h *) sbrk(0);

    heap_struct->heap.next = NULL;
    heap_struct->heap.size = CHUNK;
    heap_struct->heap.used = false;
}

void init_header(Header_h *head, Header_h *next, unsigned char used, unsigned int size)
{
    head->next = next;
    head->used = used;
    head->size = size;
}

void reduce_heap()
{
    Header_h *head, *temp;

    for(head = &heap_struct->heap; head->next; head = head->next);

    if(head->size >= 2 * CHUNK - sizeof(Header_h))
    {
        temp = (Header_h *)(((unsigned long) head) + CHUNK - sizeof(Header_h));
        if(!brk((void *)temp))
        {
            my_fprintf(BRK_ERR, stderr);
            return;
        }
        heap_struct->end = temp;
        head->size = CHUNK + sizeof(Header_h);
    }
}

void *find_open_block(size_t size)
{
    Header_h dummy;
    Header_h *head, *new_chunk;

    if(size % 16)
        size += 16 - (size % 16);
    dummy.next = &heap_struct->heap;
    for(head = &dummy; head->next && (head->next->size <= size || head->next->used); head = head->next);

    if(head->next)
    {
        head = head->next;
        head->used = true;
        if(!(head->size - size < MINCHUNK))
        {
            new_chunk = (Header_h *)(((unsigned long) &head[1]) + size);
            init_header(new_chunk, head->next, false, head->size - size);
            head->size = size;
            head->next = new_chunk;
        }
        return (void *)(&head[1]);
    }
    else
    {
        if(sbrk(CHUNK) < 0)
        {
            my_fprintf(HEAP_FULL, stderr);
            reduce_heap();
            return NULL;
        }

        /* if the last chunk is unused, give it the extra space */
        if(!head->used)
            head->size += CHUNK;
        /* otherwise add a new header */
        else
        {
            head->next = heap_struct->end;
            init_header(heap_struct->end, NULL, false, CHUNK - sizeof(Header_h));
        }
        heap_struct->end = sbrk(0);
        return find_open_block(size);
    }
}

void *my_malloc(size_t size)
{
    if(!heap_struct)
        init_heap();
    if(!size)
        return NULL;
    return find_open_block(size);
}

void *my_calloc(size_t nmemb, size_t size)
{
    if(!heap_struct)
        init_heap();
    void *ptr;
    size_t total;

    total = nmemb * size;

    ptr = find_open_block(total);
    memset(ptr, 0, total);

    return ptr;
}

void *try_expand_chunk(Header_h *head, size_t size)
{
    Header_h *new_chunk;

    if(size % 16)
        size += 16 - (size % 16);
    if(head->next)
    {
        if(!head->next->used && head->size + head->next->size + sizeof(Header_h) >= size)
        {
            head->size = head->size + head->next->size + sizeof(Header_h);

            if(!(head->size - size < MINCHUNK))
            {
                new_chunk = (Header_h *)(((unsigned long) &head[1]) + size);
                init_header(new_chunk, head->next, false, head->size - size);
                head->size = size;
                head->next = new_chunk;
            }
            return &head[1];
        }
        else if(!head->next->used)
        {
            head->size += head->next->size + sizeof(Header_h);
            head->next = head->next->next;
            head->used = false;
        }
    }
    return NULL;
}

void *my_realloc(void *ptr, size_t size)
{
    if(!heap_struct)
        init_heap();
    Header_h *head;
    void *ptr2;

    if(!size)
        my_free(ptr);
    else if(!ptr)
        my_malloc(size);

    head = (Header_h *)(((unsigned long) ptr) - sizeof(Header_h));
    if( !(ptr2 = try_expand_chunk(head, size)) )
        return find_open_block(size);
    return ptr2;
}

void my_free(void *ptr)
{
    if(!heap_struct)
    {
        my_fprintf(FREE, stderr);
        return;
    }
    Header_h *head, *comp;
    unsigned char found;

    if(ptr)
    {
        head = (Header_h *)(((unsigned long) ptr) - sizeof(Header_h));

        found = false;
        for(comp = &heap_struct->heap; comp->next; comp = comp->next)
        {
            if(head == comp && comp->used)
            {
                found = true;
                break;
            }
        }
        if(found)
        {
            head->used = false;
            if(head->next && !head->next->used)
            {
                head->size += head->next->size + sizeof(Header_h);
                head->next = head->next->next;
            }
        }
        else
            my_fprintf(FREE, stderr);
    }
}
