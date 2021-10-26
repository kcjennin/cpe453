#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "malloc.h"

#define STR_SIZE 128

void *my_malloc(size_t size);
void *my_calloc(size_t nmemb, size_t size);
void *my_realloc(void *ptr, size_t size);
void my_free(void *ptr);

Header *get_heap_start();
Header *next_header(Header *prev);
Header *get_heap_end();

static Header *free_blocks = NULL;
static Header *heap = NULL;

enum print_flags { MALLOC, CALLOC, REALLOC, FREE } flags;

void print_stat(int s, void *ptr, size_t size1, size_t size2)
{
    char str[STR_SIZE];
    memset(str, 0, STR_SIZE);

    switch(s)
    {
        case MALLOC :
            snprintf(str, STR_SIZE, "MALLOC: malloc(%ld)\t=>\t(ptr=%p, size=%ld)\n",
                size1, ptr, size1);
            break;
        case CALLOC :
            snprintf(str, STR_SIZE, "MALLOC: calloc(%ld,%ld)\t=>\t(ptr=%p, size=%ld)\n",
                size1, size2, ptr, size1*size2);
            break;
        case REALLOC :
            snprintf(str, STR_SIZE, "MALLOC: realloc(%p,%ld)\t=>\t(ptr=%p, size=%ld)\n",
                ptr, size1, ptr, size1);
            break;
        case FREE :
            snprintf(str, STR_SIZE, "MALLOC: free(%p)\n", ptr);
            break;
        default :
            break;
    }
    fputs(str, stderr);
}

Header *get_heap_start()
{
    if(!heap)
    {
        if( !(heap = sbrk(2*sizeof(Header) + HEAP_CHUNK)))
            return NULL;
        heap->size = 0;
        heap->used = 1;
        heap->free_next = &heap[1];

        free_blocks = heap->free_next;
        free_blocks->size = HEAP_CHUNK;
        free_blocks->used = 0;
        free_blocks->free_next = next_header(free_blocks);
    }
    return heap;
}

Header *get_heap_end()
{
    return sbrk(0);
}

Header *next_header(Header *prev)
{
    return (Header *) (((char *)(void *)prev) + prev->size + sizeof(Header));
}

Header *find_open(size_t size)
{
    Header *next, *head = free_blocks;

    if(!free_blocks)
    {
        if(!get_heap_start())
        {
            errno = ENOMEM;
            return NULL;
        }
    }

    while(head < get_heap_end())
    {
        /* if the block is unused */
        if(!head->used)
        {
            /* if it fits, return it */
            if(head->size >= size)
                return head;

            /* check to see if we can combine this block with the next */
            next = next_header(head);
            if(next < get_heap_end() && !next->used)
            {
                head->size += next->size + sizeof(Header);
                head->free_next = head->free_next->free_next;
                continue;
            }
        }
        /* move to the next block */
        head = next_header(head);
    }
    /* we didn't find a spot */
    return NULL;
}

void *my_malloc(size_t size)
{
    size_t block_size, new_size;
    Header *head, *new_head;

    block_size = ALIGN(size);
    head = find_open(block_size);

    while(1)
    {
        new_size = head->size - block_size - sizeof(Header);

        /* if we found a spot and we need to split */
        if(head && new_size > 0)
        {
            new_head = next_header(head);
            new_head->used = 0;
            new_head->size = new_size;
            break;
        }
        else if (!head)
        {
            if(errno == ENOMEM)
                return NULL;
            /* didn't find a spot */
            if( (new_head = sbrk(HEAP_CHUNK + sizeof(Header))) < 0 )
            {
                errno = ENOMEM;
                return NULL;
            }
            new_head->used = 0;
            new_head->size = HEAP_CHUNK;
            new_head->free_next = next_header(new_head);
            
            head = find_open(block_size);
        }
    }
    head->used = 1;
    head->size = block_size;

    print_stat(MALLOC, &head[1], size, 0);
    return &head[1];
}

void *my_calloc(size_t nmemb, size_t size)
{
    size_t total;
    void *ptr;

    total = nmemb * size;
    ptr = my_malloc(total);
    memset(ptr, 0, total);

    print_stat(CALLOC, ptr, nmemb, size);
    return ptr;
}

void *my_realloc(void *ptr, size_t size)
{
    Header *head, *next, *prev_free, dummy, *new_block;
    void *ret_ptr;
    size_t block_size, new_size;
    int size_diff;

    if(!ptr && !size)
        return NULL;
    else if(!ptr && size)
        return my_malloc(size);
    else if(ptr && !size)
    {
        my_free(ptr);
        return NULL;
    }

    block_size = ALIGN(size);

    /* check if there is space to expand */
    head = (Header *)ptr - 1;
    next = next_header(head);

    /* find the free that is closest to head */
    dummy.free_next = free_blocks;
    for(prev_free = &dummy; prev_free->free_next < head; prev_free = prev_free->free_next);

    size_diff = head->size - block_size - sizeof(Header);

    /* we need to split the block */
    if(size_diff > 0)
    {
        head->size = block_size;
        new_block = next_header(head);
        new_block->free_next = prev_free->free_next;
        new_block->size = size_diff;
        new_block->used = 0;
        prev_free->free_next = new_block;

        print_stat(REALLOC, ptr, size, 0);
        return ptr;
    }
    /* same size, dont do anything */
    else if(size_diff == 0)
    {
        print_stat(REALLOC, ptr, size, 0);
        return ptr;
    }
    /* we need a bigger block */
    else
    {
        /* we might be able to use the next block */
        if(!next->used)
        {
            new_size = head->size + next->size + sizeof(Header);

            /* if there is enough size */
            if(block_size <= new_size)
            {
                /* skip the next block in the free list and combine with head */
                prev_free->free_next = next->free_next;

                head->size += next->size + sizeof(Header);
            }
        }
        else
        {
            ret_ptr = my_malloc(block_size);
            memcpy(ret_ptr, &head[1], head->size);
            head->used = 0;
            head->free_next = prev_free->free_next;
            prev_free->free_next = head;
        }
        print_stat(REALLOC, ret_ptr, size, 0);
        return ret_ptr;
    }
}

void my_free(void *ptr)
{
    Header *next, *head, *last_free = NULL;

    if(!ptr)
        return;

    print_stat(FREE, ptr, 0, 0);
    if( !(head = get_heap_start()) )
    {
        errno = ENOMEM;
        return;
    }

    /* find the block */
    while(head < get_heap_end())
    {
        next = next_header(head);

        if(!head->used)
            last_free = head;
        else
        {
            /* found it */
            if(ptr >= (void *)head && ptr < (void *)next)
            {
                head->used = 0;

                /* insert into free list */
                /* if this is the first occuring free */
                if(!last_free)
                {
                    head->free_next = free_blocks;
                    free_blocks = head;
                }
                /* not the first */
                else
                {
                    head->free_next = last_free->free_next;
                    last_free->free_next = head;
                }
            }
        }
        head = next;
    }

    /* combine with next free block if possible */
    if(head != get_heap_end() && !next->used)
    {
        head->free_next = next->free_next;
        head->size += next->size + sizeof(Header);
    }
}
