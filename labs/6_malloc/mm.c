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

#define heapcheck(lineno) mm_heapcheck(lineno)
// #define heapcheck(lineno)
#define log_error(M, ...)                                                      \
    fprintf(stdout, "[ERROR] (%s:%d) " M "\n", __FILE__, __LINE__,             \
            ##__VA_ARGS__)

#define assertf(A, M, ...)                                                     \
    if (!(A)) {                                                                \
        log_error(M, ##__VA_ARGS__);                                           \
        assert(A);                                                             \
    }
#define DEBUG false
constexpr unsigned int w_size = 4;
constexpr unsigned int d_size = 8;
constexpr unsigned int chunk_size = 1 << 12;

constexpr unsigned int small_threshold = (1 << 4);
constexpr unsigned int medium_threshold = (1 << 5);
// header, footer, two pointers,
constexpr unsigned int min_block_size = 3 * d_size;

typedef struct free_node_t {
    struct free_node_t *prev;
    struct free_node_t *next;
} free_node_t;

static char *heap_listp;
static free_node_t *small_list;
static free_node_t *medium_list;
static free_node_t *large_list;

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
static inline void insert_into(free_node_t *node, free_node_t **restrict list) {
    node->next = *list;
    node->prev = NULL;
    if (*list) {
        (*list)->prev = node;
    }
    *list = node;
}
static inline void insert_node(free_node_t *node) {
    assert(node);
    if (DEBUG) {
        printf("inserting %p into free-list\n", node);
    }
    unsigned int size = get_size(header((char *)node));
    if (size <= small_threshold) {
        insert_into(node, &small_list);
        assert(small_list);
    } else if (size <= medium_threshold) {
        insert_into(node, &medium_list);
        assert(medium_list);
    } else {
        insert_into(node, &large_list);
        assert(large_list);
    }
}

static inline void remove_node_from(free_node_t *node, free_node_t **list) {
    if (!node->next && !node->prev) { // if there are no next or prev,
        *list = NULL;
    } else if (node->next && !node->prev) { // start of list
        node->next->prev = NULL;
        *list = node->next;
    } else if (!node->next && node->prev) { // end of list
        node->prev->next = NULL;
    } else { // middle of list
        node->next->prev = node->prev;
        node->prev->next = node->next;
    }
}
static inline void remove_node(free_node_t *node) {
    assert(node);
    if (DEBUG) {
        printf("removing %p from free list\n", node);
    }
    unsigned int size = get_size(header((char *)node));
    if (size <= small_threshold) {
        remove_node_from(node, &small_list);
    } else if (size <= medium_threshold) {
        remove_node_from(node, &medium_list);
    } else {
        remove_node_from(node, &large_list);
    }
}

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

    small_list = NULL;
    medium_list = NULL;
    large_list = NULL;

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
    heapcheck(__LINE__);
    if (DEBUG) {
        printf("requested size %zu \n", size);
    }
    if (size == 0) {
        return NULL;
    }

    size_t adj_size;
    if (size <= 2 * d_size) {
        adj_size = min_block_size;
    } else {
        adj_size = ((d_size + size - 1) | 0x7) + 1; // size + header
        assert(!(adj_size % 0x8) && "is aligned");
        assert(adj_size >= size + d_size &&
               "is large enough for headers + size");
        assert(adj_size >= min_block_size && "is at least minimum block size");
    }

    char *bp;
    if ((bp = find_fit(adj_size)) != NULL) {
        place(bp, adj_size);
        return bp;
    }

    size_t ext_size = max(adj_size, chunk_size);
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
    if (DEBUG)
        printf("freeing %p\n", bp);
    heapcheck(__LINE__);
    size_t size = get_size(header(bp));

    put(header(bp), pack(size, 0));
    put(footer(bp), pack(size, 0));
    free_node_t *node = (free_node_t *)bp;
    insert_node(node);
    coalesce(bp);
    heapcheck(__LINE__);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *bp, size_t size) {
    heapcheck(__LINE__);
    void *oldptr = bp;

    void *newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    size_t copy_size = get_size(header(oldptr)) - d_size;
    if (size < copy_size)
        copy_size = size;
    memcpy(newptr, oldptr, copy_size);
    mm_free(oldptr);
    heapcheck(__LINE__);
    return newptr;
}

static void *extend_heap(size_t size) {
    heapcheck(__LINE__);
    char *bp;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    // overwrites the previous epilogue
    put(header(bp), pack(size, 0));
    put(footer(bp), pack(size, 0));
    free_node_t *node = (free_node_t *)bp;
    insert_node(node);
    // new epilogue
    put(header(next_block_pointer(bp)), pack(0, 1));
    heapcheck(__LINE__);
    return coalesce(bp);
}

static void *coalesce(char *bp) {
    if (DEBUG) {
        printf("coalescing %p\n", bp);
    }
    size_t prev_alloc = get_alloc(footer(prev_block_pointer(bp)));
    size_t next_alloc = get_alloc(header(next_block_pointer(bp)));
    size_t size = get_size(header(bp));

    heapcheck(__LINE__);
    if (prev_alloc && next_alloc)
        return bp;
    else if (prev_alloc && !next_alloc) {
        char *next_bp = next_block_pointer(bp);
        size += get_size(header(next_bp));
        free_node_t *current_node = (free_node_t *)bp;
        remove_node(current_node); // refresh size class
        put(header(bp), pack(size, 0));
        put(footer(bp), pack(size, 0));
        free_node_t *neighbour_node = (free_node_t *)next_bp;
        remove_node(neighbour_node);
        insert_node(current_node);
    } else if (!prev_alloc && next_alloc) {
        char *prev_bp = prev_block_pointer(bp);
        size += get_size(footer(prev_bp));
        free_node_t *prev_node = (free_node_t *)prev_bp;
        remove_node(prev_node);
        put(footer(bp), pack(size, 0));
        put(header(prev_block_pointer(bp)), pack(size, 0));
        free_node_t *current_node = (free_node_t *)bp;
        remove_node(current_node);
        insert_node(prev_node);
        bp = prev_block_pointer(bp);
    } else {
        char *next_bp = next_block_pointer(bp);
        char *prev_bp = prev_block_pointer(bp);
        size += get_size(footer(prev_bp)) + get_size(header(next_bp));
        free_node_t *prev_node = (free_node_t *)prev_bp;
        remove_node(prev_node);
        put(header(prev_block_pointer(bp)), pack(size, 0));
        put(footer(next_block_pointer(bp)), pack(size, 0));
        free_node_t *current_node = (free_node_t *)bp;
        free_node_t *next_node = (free_node_t *)next_bp;
        remove_node(current_node);
        remove_node(next_node);
        insert_node(prev_node);
        bp = prev_block_pointer(bp);
    }

    heapcheck(__LINE__);
    return bp;
}

static void *find_fit(size_t size) {
    if (size <= small_threshold) {
        for (free_node_t *node = small_list; node != NULL; node = node->next) {
            if (size <= get_size(header((char *)node))) {
                return node;
            }
        }
    }
    if (size <= medium_threshold) {
        for (free_node_t *node = medium_list; node != NULL; node = node->next) {
            if (size <= get_size(header((char *)node))) {
                return node;
            }
        }
    }

    for (free_node_t *node = large_list; node != NULL; node = node->next) {
        if (size <= get_size(header((char *)node))) {
            return node;
        }
    }
    heapcheck(__LINE__);
    return NULL;
}

static void place(void *bp, size_t size) {
    if (DEBUG)
        printf("allocating %p with size %zu \n", bp, size);
    heapcheck(__LINE__);
    size_t csize = get_size(header(bp));
    if ((csize - size) >= (min_block_size)) {
        free_node_t *node = (free_node_t *)bp;
        remove_node(node);
        put(header(bp), pack(size, 1));
        put(footer(bp), pack(size, 1));
        bp = next_block_pointer(bp);
        put(header(bp), pack(csize - size, 0));
        put(footer(bp), pack(csize - size, 0));
        free_node_t *new_node = (free_node_t *)bp;
        insert_node(new_node);
        heapcheck(__LINE__);
    } else {
        free_node_t *node = (free_node_t *)bp;
        remove_node(node);
        put(header(bp), pack(csize, 1));
        put(footer(bp), pack(csize, 1));
        heapcheck(__LINE__);
    }
}

static void assert_none_allocated_in(free_node_t *free_list, int lineno) {
    for (free_node_t *node = free_list; node != NULL; node = node->next) {
        if (DEBUG) {
            printf("found in large-list %p \n", node);
            fflush(stdout);
        }
        assertf(!get_alloc(header((char *)node)), "lineno: %d", lineno);
    }
}

static void assert_not_found_in(free_node_t *free_list, char *bp) {
    for (free_node_t *node = free_list; node != NULL; node = node->next) {
        assert(!((char *)node == bp));
    }
}

static void mm_heapcheck(int lineno) {
    if (DEBUG) {
        printf("called heapcheck from %d\n", lineno);
        fflush(stdout);
    }
    for (char *bp = heap_listp; get_size(header(bp)) > 0;
         bp = next_block_pointer(bp)) {
        assertf(get_size(header(bp)) == get_size(footer(bp)),
                "lineno: %d, hd: %d, ft: %d", lineno, get_size(header(bp)),
                get_size(footer(bp)));
        assertf(get_alloc(header(bp)) == get_alloc(footer(bp)),
                "lineno: %d, hd: %d, ft: %d", lineno, get_alloc(header(bp)),
                get_alloc(footer(bp)));
        if (get_alloc(header(bp))) {
            assert_not_found_in(large_list, bp);
            assert_not_found_in(medium_list, bp);
            assert_not_found_in(small_list, bp);
        }
    }
    assert_none_allocated_in(small_list, lineno);
    assert_none_allocated_in(medium_list, lineno);
    assert_none_allocated_in(large_list, lineno);
}
