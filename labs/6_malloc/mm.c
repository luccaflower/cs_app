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
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 12)

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
static inline char *header(char *bp) { return bp - WSIZE; }
static inline char *footer(char *bp) {
    return bp + get_size(header(bp)) - DSIZE;
}
static char *next_block_pointer(char *bp) { return bp + get_size(header(bp)); }
static char *prev_block_pointer(char *bp) { return bp - get_size(bp - DSIZE); }

// #define heapcheck(lineno) mm_heapcheck(lineno)
#define heapcheck(lineno)

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
        return -1;
    put(heap_listp, 0);
    put(heap_listp + WSIZE, pack(DSIZE, 1));
    put(heap_listp + (2 * WSIZE), pack(DSIZE, 1));
    put(heap_listp + (3 * WSIZE), pack(0, 1));
    heap_listp += (2 * WSIZE);

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
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

    if (size <= DSIZE) {
        adj_size = 2 * DSIZE;
    } else {
        adj_size = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    }

    if ((bp = find_fit(adj_size)) != NULL) {
        place(bp, adj_size);
        return bp;
    }

    ext_size = max(adj_size, CHUNKSIZE);
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
    void *newptr;
    size_t copy_size;

    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    copy_size = get_size(header(oldptr)) - DSIZE;
    if (size < copy_size)
        copy_size = size;
    memcpy(newptr, oldptr, copy_size);
    mm_free(oldptr);
    return newptr;
}

static void *extend_heap(size_t words) {
    char *bp;
    size_t size;

    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;

    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    put(header(bp), pack(size, 0));
    put(footer(bp), pack(size, 0));
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
    char *bp;
    for (bp = heap_listp; get_size(header(bp)) > 0;
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
    if ((csize - size) >= (2 * DSIZE)) {
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
