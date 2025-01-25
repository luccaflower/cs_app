/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "memlib.h"
#include "mm.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "0x1eaf",
    /* First member's full name */
    "0x1eaf",
    /* First member's email address */
    "mail@0x1eaf.dev",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

constexpr unsigned int w_size = 4;
constexpr unsigned int d_size = 8;
constexpr unsigned int chunk_size = 1 << 12;

static char *heap_listp;

static void *extend_heap(size_t);
static void *coalesce(char *);
static void *find_fit(size_t size);
static void place(void *bp, size_t size);
static void mm_heapcheck(int lineno);

static inline size_t max(size_t x, size_t y) { return x > y ? x : y; }
static inline unsigned int get(void *p) { return *(unsigned int *)p; }
static inline unsigned int get_size(char *p) { return get(p) & ~0x7; }
static inline unsigned int get_alloc(char *p) { return get(p) & 0x1; }
static inline void put(void *p, unsigned int val) { *(unsigned int *)p = val; }
static inline unsigned int pack(unsigned int size, unsigned int alloc) {
    return size | alloc;
}
static inline char *header(char *bp) { return bp - w_size; }
static inline char *footer(char *bp) {
    return bp + get_size(header(bp)) - d_size;
}
static char *next_block_pointer(char *bp) { return bp + get_size(header(bp)); }
static char *prev_block_pointer(char *bp) { return bp - get_size(bp - d_size); }

// #define heapcheck(lineno) mm_heapcheck(lineno)
#define heapcheck(lineno)

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {
    if ((heap_listp = mem_sbrk(4 * w_size)) == (void *)-1)
        return -1;
    put(heap_listp, 0);                        // padding
    put(heap_listp + w_size, pack(d_size, 1)); // prologue headers
    put(heap_listp + (2 * w_size), pack(d_size, 1));
    put(heap_listp + (3 * w_size), pack(0, 1)); // epilogue marking the end of
                                                // the currently available heap
    heap_listp += (2 * w_size); // position heap base at the prologue

    if (extend_heap(chunk_size) == NULL)
        return -1;

    heapcheck(__LINE__);
    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) {
    size_t adj_size;
    size_t ext_size;
    char *bp;

    heapcheck(__LINE__);
    if (size == 0) {
        return NULL;
    }

    if (size <= d_size) {
        adj_size = 2 * d_size;
    } else {
        adj_size = d_size * ((size + (d_size) + (d_size - 1)) / d_size);
    }

    if ((bp = find_fit(adj_size)) != NULL) {
        place(bp, adj_size);
        return bp;
    }

    ext_size = max(adj_size, chunk_size);
    if ((bp = extend_heap(ext_size)) == NULL) {
        return NULL;
    }
    place(bp, adj_size);
    heapcheck(__LINE__);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp) {
    size_t size = get_size(header(bp));

    put(header(bp), pack(size, 0));
    put(footer(bp), pack(size, 0));
    coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *bp, size_t size) {
    void *oldptr = bp;

    void *newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    size_t copy_size = get_size(header(oldptr)) - d_size;
    if (size < copy_size)
        copy_size = size;
    memcpy(newptr, oldptr, copy_size);
    mm_free(oldptr);
    return newptr;
}

static void *extend_heap(size_t size) {

    char *bp;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    // overwrites the previous epilogue
    put(header(bp), pack(size, 0));
    put(footer(bp), pack(size, 0));
    // new epilogue
    put(header(next_block_pointer(bp)), pack(0, 1));
    heapcheck(__LINE__);
    return coalesce(bp);
}

static void *coalesce(char *bp) {
    size_t prev_alloc = get_alloc(footer(prev_block_pointer(bp)));
    size_t next_alloc = get_alloc(header(next_block_pointer(bp)));
    size_t size = get_size(header(bp));

    if (prev_alloc && next_alloc)
        return bp;
    else if (prev_alloc && !next_alloc) {
        size += get_size(header(next_block_pointer(bp)));
        put(header(bp), pack(size, 0));
        put(footer(bp), pack(size, 0));
    } else if (!prev_alloc && next_alloc) {
        size += get_size(footer(prev_block_pointer(bp)));
        put(footer(bp), pack(size, 0));
        put(header(prev_block_pointer(bp)), pack(size, 0));
        bp = prev_block_pointer(bp);
    } else {
        size += get_size(footer(prev_block_pointer(bp))) +
                get_size(header(next_block_pointer(bp)));
        put(header(prev_block_pointer(bp)), pack(size, 0));
        put(footer(next_block_pointer(bp)), pack(size, 0));
        bp = prev_block_pointer(bp);
    }

    heapcheck(__LINE__);
    return bp;
}

static void *find_fit(size_t size) {
    for (char *bp = heap_listp; get_size(header(bp)) > 0;
         bp = next_block_pointer(bp)) {
        if (!get_alloc(header(bp)) && (size <= get_size(header(bp)))) {
            heapcheck(__LINE__);
            return bp;
        }
    }
    heapcheck(__LINE__);
    return NULL;
}

static void place(void *bp, size_t size) {
    size_t csize = get_size(header(bp));
    if ((csize - size) >= (2 * d_size)) {
        put(header(bp), pack(size, 1));
        put(footer(bp), pack(size, 1));
        bp = next_block_pointer(bp);
        put(header(bp), pack(csize - size, 0));
        put(footer(bp), pack(csize - size, 0));
    } else {
        put(header(bp), pack(csize, 1));
        put(footer(bp), pack(csize, 1));
    }
}

static void mm_heapcheck(int lineno) {
    printf("heapcheck called from line %d \n", lineno);
    for (char *bp = heap_listp; get_size(header(bp)) > 0;
         bp = next_block_pointer(bp)) {
        assert(get_size(header(bp)) == get_size(footer(bp)));
        assert(get_alloc(header(bp)) == get_alloc(footer(bp)));
    }
}
