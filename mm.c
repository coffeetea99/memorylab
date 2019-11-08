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

/* get the size of allocated memory
 * return the segregated list index of it
 */
static int size_to_index(int size) {
    int block = ALIGN(size) >> 3;
    int index = 0;
    int cur = 1;
    while( cur < block ) {
        cur *= 2;
        ++index;
    }
    return index;
}

/* get start pointer of the block
 * return pointer of its header
 */
static void* header_pointer(void* start) {
    return (char*)start - 4;
}

/* get start pointer of the block
 * return its header entry(4 byte)
 */
static size_t header(void* start) {
    return *((size_t*)header_pointer(start));
}

/* get start pointer of the block
 * return the block's size(4 byte)
 */
static size_t block_size(void* start) {
    return header(start) & ~0x7;
}

/* get start pointer of the block
 * return pointer of its footer
 */
static void* footer_pointer(void* start) {
    return (char*)start + block_size(start) + 8;
}

/* get start pointer of the block
 * return its footer entry(4 byte) 
 */
static size_t footer(void* start) {
    return *((size_t*)footer_pointer(start));
}

static int* front_offset_entry_pointer(void* start) {
    return (int*)((char*)start + 4);
}

static int* back_offset_entry_pointer(void* start) {
    return (int*)start;
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
static void reset_block_size(void* start, size_t size){
    size_t* header_pointer_4bit = header_pointer(start);
    *header_pointer_4bit = size | (*header_pointer_4bit & 0x1);
    size_t* footer_pointer_4bit = footer_pointer(start);
    *footer_pointer_4bit = *header_pointer_4bit;
}

/* get start pointer of the block and block status that will change into
 * rewrite status of the block on header and footer
 */
static void rewrite_block_status(void* start, char status) {
    int* header_pointer_4bit = header_pointer(start);
    int* footer_pointer_4bit = footer_pointer(start);
    *header_pointer_4bit = status | (*header_pointer_4bit & ~0x7);
    *footer_pointer_4bit = status | (*footer_pointer_4bit & ~0x7);
}

/* get start pointer of a free block
 * cut off the block from segregated list
 */
static void cut_off(void* start) {
    printf("cutting %x\n", start);
    int front_offset = *front_offset_entry_pointer(start);
    int back_offset = *back_offset_entry_pointer(start);
    if ( front_offset ) {
        char* front_pointer = (char*)start + front_offset;
        *back_offset_entry_pointer(front_pointer) += back_offset;
        if(back_offset == 0) {
            *back_offset_entry_pointer(front_pointer) = 0;
            printf("back offset of %x is now zero\n", front_pointer);
        }
    }
    if ( back_offset ) {
        char* back_pointer = (char*)start + back_offset;
        *front_offset_entry_pointer(back_pointer) += front_offset;
    }
}

/* get start pointer and size of relocating block
 * put the block in the segregated list
 */
static void set_in(void* new, int size) {
    printf("block start point: %x, size: %x\n", new, size);
    int index = size_to_index(size);
    int* array = mem_heap_lo();
    int offset = array[index];
    if ( offset == 0 ) {        //no one in this list
        array[index] = (char*)new - (char*)&array[index];
        *front_offset_entry_pointer(new) = -array[index];
        *back_offset_entry_pointer(new) = 0;
    } else {
        char* pointer = &array[index];
        while( offset ) {
            printf("pointer %x offset %d\n", pointer, offset);
            pointer += offset;
            offset = *back_offset_entry_pointer(pointer);
        }
        //pointer: last block, new: new block
        int gap = (char*)new - pointer;
        *back_offset_entry_pointer(pointer) = gap;
        *front_offset_entry_pointer(new) = -gap;
        *back_offset_entry_pointer(new) = 0;
        printf("%x %x %x\n", pointer, new, gap);
    }
}

/* get start pointer of a free block
 * if can coalesce with front block, coalesce and return new pointer
 * if not, just return original pointer
 */
static void* coalesce_front(void* start) {
    size_t* front_footer_pointer = (char*)start - 8;
    size_t front_footer = *front_footer_pointer;
    char front_allocated = (char)front_footer & 0x1;
    if ( front_allocated ) {    //can't coaleace
        return start;
    } else {                    //can coaleace
        printf("can coalesce with front\n");
        size_t front_size = front_footer & ~0x7;
        size_t total_size = front_size + block_size(start) + 16;

        void* front_pointer = (char*)start - front_size - 16;

        cut_off(start);
        cut_off(front_pointer);

        int* front_header_pointer_4bit = (char*)front_pointer - 4;
        int* footer_pointer_4bit = footer_pointer(start);
        *front_header_pointer_4bit = total_size;
        *footer_pointer_4bit = total_size;

        set_in(front_pointer, total_size);
        return front_pointer;
    }
}

/* get start pointer of the block
 * if can coalesce with back block, coalesce
 * return nothing
 */
static void coalesce_back(void* start) {
    int* back_header_pointer = (char*)start + block_size(start) + 12;
    int back_header = *back_header_pointer;
    char back_allocated = (char)back_header & 0x1;
    if ( !back_allocated ) {
        printf("can coalesce with back\n");
        int back_size = back_header & ~0x7;
        int total_size = back_size + block_size(start) + 16;

        void* back_pointer = (char*)start + block_size(start) + 16;

        cut_off(start);
        cut_off(back_pointer);

        int* back_footer_pointer_4bit = (char*)start + total_size + 8;
        int* header_pointer_4bit = header_pointer(start);
        *header_pointer_4bit = total_size;
        *back_footer_pointer_4bit = total_size;

        set_in(start, total_size);
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
    mem_sbrk(128);

    void* heap_start = mem_heap_lo();
    *(int*)((char*)heap_start + 116) = 0x1;
    *(int*)((char*)heap_start + 120) = 0x1;
    *(int*)((char*)heap_start + 124) = 0x1;

    /* DON'T MODIFY THIS STAGE AND LEAVE IT AS IT WAS */
    gl_ranges = ranges;

    return 0;
}

/*
 *  mm_malloc - Allocate a block by incrementing the brk pointer (example).
 *  Always allocate a block whose size is a multiple of the alignment.-
 */
void *mm_malloc(size_t size)
{   
    printf("\nmalloc %x\n", size);
    size = ALIGN(size);
    int index = size_to_index(size);

    int* array = mem_heap_lo();

    for ( ; index < 29 ; ++index ) {
        int offset = array[index];

        if ( offset != 0 ){     //뭔가 있다
            char* pointer = &(array[index]);
            while( offset ) {
                int current_size = block_size(pointer);
                char current_allocated = is_allocated(pointer);

                if ( !current_allocated && current_size >= size) {       //we found an empty block!
                    if (current_size == size || current_size == size + 8 ) {      //allocate all
                        rewrite_block_status(pointer, 1);
                    } else {                                                                    //cut the block
                        cut_off(pointer);

                        reset_block_size(pointer, size);
                        rewrite_block_status(pointer, 1);
                        set_in(pointer, size);

                        void* next_pointer = pointer + size + 16;
                        int cut_block_size = current_size - size - 16;

                        reset_block_size(next_pointer, cut_block_size);
                        rewrite_block_status(next_pointer, 0);
                        set_in(next_pointer, cut_block_size);
                    }
                    return pointer;
                }
                //go next
                offset = *back_offset_entry_pointer(pointer);
                pointer += offset;
            }
        }
    }
    //the list for this block is empty, or has no proper size
    
    char* pointer = mem_sbrk(size + 16);
    reset_block_size(pointer, size);
    rewrite_block_status(pointer, 1);
    set_in(pointer, size);

    char* last_pointer = mem_heap_hi();
    last_pointer -= 3;
    *(int*)last_pointer = 0x1;
    return pointer;
}

/*
 *  mm_free - Frees a block. Does nothing (example)
 */
void mm_free(void *ptr)
{
    printf("\nfree %x\n", ptr);
    /* YOUR IMPLEMENTATION */
    rewrite_block_status(ptr, 0);
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
    int* array = mem_heap_lo();
    int index;

    for (index = 0; index < 29; ++index) {
        char* pointer = &(array[index]);
        int offset = array[index];
        while( offset ) {

            if ( is_allocated(pointer) ) {
                mm_free(pointer);
            }
            offset = *back_offset_entry_pointer(pointer);
            pointer += offset;
        }
    }
}
