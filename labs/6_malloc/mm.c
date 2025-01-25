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

// #define heapcheck(lineno) mm_heapcheck(lineno)
#define heapcheck(lineno)
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
constexpr unsigned int min_block_size =
    3 * d_size; // header, footer, two pointers,

typedef struct free_node_t {
    struct free_node_t *prev;
    struct free_node_t *next;
} free_node_t;

static char *heap_listp;
static free_node_t *free_list;

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
static inline void insert_node(free_node_t *node) {
    assert(node);
    node->next = free_list;
    node->prev = NULL;
    if (free_list) {
        free_list->prev = node;
    }
    free_list = node;
    assert(free_list);
}

static inline void remove_node(free_node_t *node) {
    assert(node);
    if (DEBUG) {
        printf("removing %p from free list\n", node);
    }
    if (!node->next && !node->prev) { // if there are no next or prev,
        free_list = NULL;
    } else if (node->next && !node->prev) { // start of list
        node->next->prev = NULL;
        free_list = node->next;
    } else if (!node->next && node->prev) { // end of list
        node->prev->next = NULL;
    } else { // middle of list
        node->next->prev = node->prev;
        node->prev->next = node->next;
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
    free_list = NULL;

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
        put(header(bp), pack(size, 0));
        put(footer(bp), pack(size, 0));
        free_node_t *neighbour_node = (free_node_t *)next_bp;
        assert(neighbour_node->next || neighbour_node->prev);
        remove_node(neighbour_node);
    } else if (!prev_alloc && next_alloc) {
        size += get_size(footer(prev_block_pointer(bp)));
        put(footer(bp), pack(size, 0));
        put(header(prev_block_pointer(bp)), pack(size, 0));
        free_node_t *current_node = (free_node_t *)bp;
        assert(current_node->next || current_node->prev);
        remove_node(current_node);
        bp = prev_block_pointer(bp);
    } else {
        char *next_bp = next_block_pointer(bp);
        size += get_size(footer(prev_block_pointer(bp))) +
                get_size(header(next_bp));
        put(header(prev_block_pointer(bp)), pack(size, 0));
        put(footer(next_block_pointer(bp)), pack(size, 0));
        free_node_t *current_node = (free_node_t *)bp;
        free_node_t *next_node = (free_node_t *)next_bp;
        assert(current_node->next || current_node->prev);
        remove_node(current_node);
        assert(next_node->next || next_node->prev);
        remove_node(next_node);
        bp = prev_block_pointer(bp);
    }

    heapcheck(__LINE__);
    return bp;
}

static void *find_fit(size_t size) {
    for (free_node_t *node = free_list; node != NULL; node = node->next) {
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
        put(header(bp), pack(size, 1));
        put(footer(bp), pack(size, 1));
        free_node_t *node = (free_node_t *)bp;
        remove_node(node);
        bp = next_block_pointer(bp);
        put(header(bp), pack(csize - size, 0));
        put(footer(bp), pack(csize - size, 0));
        free_node_t *new_node = (free_node_t *)bp;
        insert_node(new_node);
        heapcheck(__LINE__);
    } else {
        put(header(bp), pack(csize, 1));
        put(footer(bp), pack(csize, 1));
        free_node_t *node = (free_node_t *)bp;
        remove_node(node);
        heapcheck(__LINE__);
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
            for (free_node_t *node = free_list; node != NULL;
                 node = node->next) {
                assert(!((char *)node == bp));
            }
        }
    }
    for (free_node_t *node = free_list; node != NULL; node = node->next) {
        if (DEBUG) {
            printf("found in free-list %p \n", node);
            fflush(stdout);
        }
        assertf(!get_alloc(header((char *)node)), "lineno: %d", lineno);
    }
}
