/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc isn't implemented.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/**********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please  *
 * provide  your  information  in the  following  struct. *
 **********************************************************/
team_t team = {
    /* Your full name */
    "Jinje Han",
    /* Your student ID */
    "2018-18786"
};

/* DON'T MODIFY THIS VALUE AND LEAVE IT AS IT WAS */
static range_t **gl_ranges;

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* user-defined functions */

/* get start pointer of the block
 * return pointer of its header
 */
static void* header_pointer(void* start) {
    return (char*)start - 4;
}

/* get start pointer of the block
 * return its header entry(4 byte)
 */
static int header(void* start) {
    return *((int*)header_pointer(start));
}

/* get start pointer of the block
 * return the block's size(4 byte)
 */
static int block_size(void* start) {
    return header(start) & ~0x7;
}

/* get start pointer of the block
 * return pointer of its footer
 */
static void* footer_pointer(void* start) {
    return (char*)start + block_size(start);
}

/* get start pointer of the block
 * return its footer entry(4 byte) 
 */
static int footer(void* start) {
    return *((int*)footer_pointer(start));
}

/* get start pointer of the block
 * return if the block is allocated(1 byte)
 */
static char is_allocated(void* start) {
    return (char)header(start) & 0x1;
}

/* get start pointer of the block and block size that will change into
 * rewrite size of the block on header and footer
 */
static void reset_block_size(void* start, int size){
    int* header_pointer_4bit = header_pointer(start);
    *header_pointer_4bit = size | (*header_pointer_4bit & 0x1);
    int* footer_pointer_4bit = footer_pointer(start);
    *footer_pointer_4bit = *header_pointer_4bit;
}

/* get start pointer of the block and block status that will change into
 * rewrite status of the block on header and footer
 */
static void rewrite_block_status(void* start, char status) {
    char* header_pointer_1bit = header_pointer(start);
    char* footer_pointer_1bit = footer_pointer(start);
    *header_pointer_1bit = *footer_pointer_1bit = status | (*footer_pointer_1bit & ~0x7);
}

/* get start pointer of the block
 * if can coalesce with front block, coalesce and return new pointer
 * if not, just return original pointer
 */
static void* coalesce_front(void* start) {
    int* front_footer_pointer = (char*)start - 8;
    int front_footer = *front_footer_pointer;
    char front_allocated = (char)front_footer & 0x1;
    if ( front_allocated != is_allocated(start) ) {
        return start;
    } else {
        int front_size = front_footer & ~0x7;
        int total_size = front_size + block_size(start) + 8;

        void* front_pointer = (char*)start - front_size - 8;
        int* front_header_pointer_4bit = (char*)front_pointer - 4;
        int* footer_pointer_4bit = footer_pointer(start);
        *front_header_pointer_4bit = *footer_pointer_4bit = total_size | front_allocated;
        return front_pointer;
    }
}

/* get start pointer of the block
 * if can coalesce with back block, coalesce
 * return nothing
 */
static void coalesce_back(void* start) {
    int* back_header_pointer = (char*)start + block_size(start) + 4;
    int back_header = *back_header_pointer;
    char back_allocated = (char)back_header & 0x1;
    if ( back_allocated == is_allocated(start)) {
        int back_size = back_header & ~0x7;
        int total_size = back_size + block_size(start) + 8;

        int* back_footer_pointer_4bit = (char*)start + total_size + 8;
        int* header_pointer_4bit = header_pointer(start);
        *header_pointer_4bit = *back_footer_pointer_4bit = total_size | back_allocated;
    }
}

/*
 * remove_range - manipulate range lists
 * DON'T MODIFY THIS FUNCTION AND LEAVE IT AS IT WAS
 */
static void remove_range(range_t **ranges, char *lo)
{
    range_t *p;
    range_t **prevpp = ranges;

    if (!ranges)
      return;

    for (p = *ranges;  p != NULL; p = p->next) {
      if (p->lo == lo) {
        *prevpp = p->next;
        free(p);
        break;
      }
      prevpp = &(p->next);
    }
}

/*
 *  mm_init - initialize the malloc package.
 */
int mm_init(range_t **ranges)
{
    /* YOUR IMPLEMENTATION */
    mem_sbrk(16);

    void* heap_start = mem_heap_lo();
    //printf("start: %x\n", heap_start);
    *((char*)heap_start + 4) = 0x1;
    *((char*)heap_start + 8) = 0x1;
    *((char*)heap_start + 12) = 0x1;

    /* DON'T MODIFY THIS STAGE AND LEAVE IT AS IT WAS */
    gl_ranges = ranges;

    return 0;
}

/*
 *  mm_malloc - Allocate a block by incrementing the brk pointer (example).
 *  Always allocate a block whose size is a multiple of the alignment.-
 */
int i;
void *mm_malloc(size_t size)
{
    ++i;
    printf("%d\n", i);
    printf("size: %d\n", mem_heap_hi() - mem_heap_lo());
    //printf("size: %x  ", size);
    int newsize = ALIGN(size + SIZE_T_SIZE);

    char* pointer = (char*)mem_heap_lo() + 16;
    char* heap_end = mem_heap_hi();

    while(pointer < heap_end) {
        int current_block_size = block_size(pointer);

        if ( !is_allocated(pointer) && ALIGN(size) <= current_block_size ) {      //can allocate
            if ( current_block_size <= ALIGN(size) + 8) {   //just allocate whole
                rewrite_block_status(pointer, 1);
            } else {
                reset_block_size(pointer, ALIGN(size));
                rewrite_block_status(pointer, 1);

                char* next_pointer = pointer + newsize;
                reset_block_size(next_pointer, current_block_size - newsize);
                rewrite_block_status(next_pointer, 0);
            }
            //printf("allocate %x\n", pointer);
            return pointer;
        } else {                                        //can't allocate
            pointer += current_block_size + 8;
        }
    }

    //must increase heap
    int last_block_footer = *(int*)(pointer-8);
    if ( last_block_footer & 0x1 == 1 ) {       //last block is allocated
        void *start_pointer = mem_sbrk(newsize);
        if (start_pointer == (void *)-1){
            return NULL;
        }
        else {
            reset_block_size(start_pointer, ALIGN(size));
            rewrite_block_status(start_pointer, 1);
            int* back_header_pointer_4bit = (char*)footer_pointer(start_pointer) + 4;
            *back_header_pointer_4bit = 0x1;

            //printf("allocate %x\n", start_pointer);
            return start_pointer;
        }
    } else {                                    //last block is not allocated
        int last_block_size = last_block_footer & ~0x7;
        char* last_block_pointer = pointer - last_block_size - 8;

        int increase_heap_size = ALIGN(size) - last_block_size;
        mem_sbrk(increase_heap_size);
        reset_block_size(last_block_pointer, ALIGN(size));
        rewrite_block_status(last_block_pointer, 1);

        int* last_4byte_pointer = last_block_pointer + ALIGN(size) + 4;
        *last_4byte_pointer = 0x1;

        //printf("(increased)allocate %x\n", last_block_pointer);
        return last_block_pointer;
    }
    
    
    
    /*
    int newsize = ALIGN(size + SIZE_T_SIZE);
    void *p = mem_sbrk(newsize);
    if (p == (void *)-1)
        return NULL;
    else {
        *(size_t *)p = size;
        return (void *)((char *)p + SIZE_T_SIZE);
    }
    */
}

/*
 *  mm_free - Frees a block. Does nothing (example)
 */
void mm_free(void *ptr)
{
    ++i;
    printf("%d\n", i);
    //printf("free %x\n", ptr);
    /* YOUR IMPLEMENTATION */
    rewrite_block_status(ptr, 0);
    //coalesce_back(coalesce_front(ptr));
    void* middle = coalesce_front(ptr);
    coalesce_back(middle);

    /* DON'T MODIFY THIS STAGE AND LEAVE IT AS IT WAS */
    if (gl_ranges)
      remove_range(gl_ranges, ptr);
}

/*
 *  mm_realloc - empty implementation; YOU DO NOT NEED TO IMPLEMENT THIS
 */
void *mm_realloc(void *ptr, size_t t)
{
    return NULL;
}

/*
 *  mm_exit - finalize the malloc package.
 */
void mm_exit(void)
{
    /* YOUR IMPLEMENTATION */
    //printf("start clean\n");
    char* pointer = (char*)mem_heap_lo() + 16;
    char* heap_end = mem_heap_hi();

    while(pointer < heap_end) {
        char* next_pointer = pointer + block_size(pointer) + 8;
        if(is_allocated(pointer)) {
            mm_free(pointer);
        }
        pointer = next_pointer;
    }

}
