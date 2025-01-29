/*
 * My implementation of Malloc
 * Segregated free-list implementation with free-lists divided
 * by size-classes of powers of two
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

#define DEBUG false
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
constexpr size_t w_size = sizeof(size_t);
constexpr size_t w_bits = w_size * w_size;
constexpr size_t chunk_size = 1 << 12;

// header, footer, two pointers,
constexpr size_t min_block_size = 4 * w_size;

typedef struct free_node_t {
    struct free_node_t *prev;
    struct free_node_t *next;
} free_node_t;

static char *heap_listp;
static free_node_t **free_lists;

static void *extend_heap(size_t);
static void *coalesce(char *);
static void *find_fit(size_t size);
static void place(void *bp, size_t size);
static void mm_heapcheck(int lineno);

static inline size_t max(size_t x, size_t y) { return x > y ? x : y; }
static inline size_t get(void *p) { return *(size_t *)p; }
static inline size_t get_size(char *p) { return get(p) & ~0x7; }
static inline void set_prev_alloc(char *p, size_t alloc) {
    *(size_t *)p = (alloc << 1) | (*(size_t *)p & ~(0x2 | 0x4));
}
static inline size_t get_prev_alloc(char *p) { return get(p) & 0x2; }
static inline size_t get_alloc(char *p) { return get(p) & 0x1; }
static inline void put(void *p, size_t val) {
    *(size_t *)p = (val | (*(size_t *)p & 0x2));
}
static inline size_t pack(size_t size, size_t alloc) { return size | alloc; }
static inline char *header(char *bp) { return bp - w_size; }
static inline char *footer(char *bp) {
    return bp + get_size(header(bp)) - (2 * w_size);
}
static char *next_block_pointer(char *bp) { return bp + get_size(header(bp)); }
static char *prev_block_pointer(char *bp) {
    return bp - get_size(bp - (2 * w_size));
}

// we use log base2 to index into the array of free-lists
static inline size_t log2_of(size_t n) {
    size_t shift = w_bits >> 1;
    size_t step = w_bits >> 2;
    size_t shifted_n;
    while ((shifted_n = (n >> shift)) != 1) {
        shift = shifted_n ? shift + step : shift - step;
        step = max(step >> 1, 1);
    }

    return shift;
}

static inline void insert_into(free_node_t *node, free_node_t **list) {
    node->next = *list;
    node->prev = NULL;
    if (*list) {
        (*list)->prev = node;
    }
    *list = node;
}
static inline void insert_node(free_node_t *node) {
    assert(node);
    size_t size = get_size(header((char *)node));
    size_t i = log2_of(size) - 1;
    assert(i < w_bits);
    if (DEBUG) {
        printf("inserting %p into free-list\n", node);
        printf("size: %zu, class-size: %zu\n", size, i);
    }
    insert_into(node, &free_lists[i]);
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
    size_t size = get_size(header((char *)node));
    size_t i = log2_of(size) - 1;
    assert(i < w_bits);
    if (DEBUG) {
        printf("removing %p from free list\n", node);
        printf("size: %zu, class-size: %zu\n", size, i);
    }
    remove_node_from(node, &free_lists[i]);
}

static inline void split(void *bp, size_t size, size_t csize) {
    if ((csize - size) >= min_block_size) {
        put(header(bp), pack(size, 1));
        bp = next_block_pointer(bp);
        put(header(bp), pack(csize - size, 0));
        put(footer(bp), pack(csize - size, 0));
        set_prev_alloc(header(next_block_pointer(bp)), 0);
        free_node_t *new_node = (free_node_t *)bp;
        insert_node(new_node);
    } else {
        put(header(bp), pack(csize, 1));
    }
}
/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {
    size_t free_list_size = sizeof(free_node_t *) * w_bits;
    if ((heap_listp = mem_sbrk(free_list_size + (3 * w_size))) == (void *)-1)
        return -1;
    free_lists = (free_node_t **)heap_listp;
    for (size_t i = 0; i < w_bits; i++) {
        free_lists[i] = NULL;
    }
    heap_listp += free_list_size;
    put(heap_listp, pack(2 * w_size, 1)); // prologue headers
    set_prev_alloc(heap_listp, 1);
    put(heap_listp + w_size, pack(2 * w_size, 1));
    put(heap_listp + (2 * w_size), pack(0, 1)); // epilogue marking the end of
                                                // the currently available heap
    set_prev_alloc(heap_listp + 2 * w_size, 1);
    heap_listp += w_size; // position heap base at the prologue

    heapcheck(__LINE__);
    if (extend_heap(chunk_size) == NULL)
        return -1;

    heapcheck(__LINE__);
    return 0;
}

/*
 * mm_malloc - allocate a memory block using a first-fit policy.
 * Extend the heap as needed.
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
    if (size <= 3 * w_size) {
        adj_size = min_block_size;
    } else {
        adj_size = ((w_size + size - 1) | 0x7) + 1; // size + header
        assert(!(adj_size % 0x8) && "is aligned");
        assert(adj_size >= min_block_size && "is at least minimum block size");
        if (DEBUG) {
            printf("adjusted size: %zu\n", adj_size);
        }
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
 * mm_free - Freeing a block of memory coalesces it with neighbouring free
 * block and inserts it into its appropriate free-list.
 */
void mm_free(void *bp) {
    if (DEBUG)
        printf("freeing %p\n", bp);
    heapcheck(__LINE__);
    size_t size = get_size(header(bp));
    set_prev_alloc(header(next_block_pointer(bp)), 0);

    put(header(bp), pack(size, 0));
    put(footer(bp), pack(size, 0));
    bp = coalesce(bp);
    insert_node(bp);
    heapcheck(__LINE__);
}

/*
 * mm_realloc - memcpy is expensive so we want to avoid that. If we realloc for
 * a smaller size, we can simply return the same pointer. If we can coalesce
 * with the next block to fit the requested size, then that too will avoid the
 * memcpy. Finally as a last resort, we malloc a new block to fit the requested
 * size, memcpy from the old pointer, and free it. We could improve the
 * utilization of memory here by doing some splitting.
 */
void *mm_realloc(void *bp, size_t size) {
    if (DEBUG) {
        printf("reallocating %p\n", bp);
    }
    heapcheck(__LINE__);
    void *oldptr = bp;

    size_t block_size = get_size(header(oldptr));
    if (size + w_size < block_size) {
        return oldptr;
    } else if (size < block_size) {
        size = block_size;
    }

    char *next_bp = next_block_pointer(bp);
    size_t next_alloc = get_alloc(header(next_bp));
    size_t next_size = get_size(header(next_bp));
    if (!next_alloc && size + w_size <= block_size + next_size) {
        block_size += next_size;
        put(header(bp), pack(block_size, 1));
        remove_node((free_node_t *)next_bp);
        set_prev_alloc(header(next_block_pointer(bp)), 1);
        heapcheck(__LINE__);
        return bp;
    }

    void *newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    memcpy(newptr, oldptr, block_size);
    mm_free(oldptr);
    if (DEBUG) {
        printf("reallocated %p to %p", oldptr, newptr);
    }
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
    // new epilogue
    put(header(next_block_pointer(bp)), pack(0, 1));
    set_prev_alloc(next_block_pointer(bp), 0);
    bp = coalesce(bp);
    insert_node((free_node_t *)bp);
    heapcheck(__LINE__);
    return bp;
}

static void *coalesce(char *bp) {
    if (DEBUG) {
        printf("coalescing %p\n", bp);
    }

    size_t prev_alloc = get_prev_alloc(header(bp));
    size_t next_alloc = get_alloc(header(next_block_pointer(bp)));
    size_t size = get_size(header(bp));

    if (prev_alloc && next_alloc)
        return bp;
    else if (prev_alloc && !next_alloc) {
        char *next_bp = next_block_pointer(bp);
        size += get_size(header(next_bp));
        put(header(bp), pack(size, 0));
        put(footer(bp), pack(size, 0));
        free_node_t *neighbour_node = (free_node_t *)next_bp;
        remove_node(neighbour_node);
    } else if (!prev_alloc && next_alloc) {
        char *prev_bp = prev_block_pointer(bp);
        size += get_size(footer(prev_bp));
        free_node_t *prev_node = (free_node_t *)prev_bp;
        remove_node(prev_node);
        put(footer(bp), pack(size, 0));
        put(header(prev_block_pointer(bp)), pack(size, 0));
        bp = prev_block_pointer(bp);
    } else {
        char *next_bp = next_block_pointer(bp);
        char *prev_bp = prev_block_pointer(bp);
        size += get_size(footer(prev_bp)) + get_size(header(next_bp));
        free_node_t *prev_node = (free_node_t *)prev_bp;
        remove_node(prev_node);
        put(header(prev_block_pointer(bp)), pack(size, 0));
        put(footer(next_block_pointer(bp)), pack(size, 0));
        free_node_t *next_node = (free_node_t *)next_bp;
        remove_node(next_node);
        bp = prev_block_pointer(bp);
    }

    return bp;
}

static void *find_fit(size_t size) {
    size_t i = log2_of(size) - 1;
    while (i < w_bits) {
        for (free_node_t *node = free_lists[i]; node; node = node->next) {
            if (size <= get_size(header((char *)node))) {
                return node;
            }
        }
        i++;
    }
    return NULL;
}

static void place(void *bp, size_t size) {
    if (DEBUG)
        printf("allocating %p with size %zu \n", bp, size);
    heapcheck(__LINE__);
    size_t block_size = get_size(header(bp));

    free_node_t *node = (free_node_t *)bp;
    remove_node(node);
    split(bp, size, block_size);
    set_prev_alloc(header(next_block_pointer(bp)), 1);
}

static void assert_none_allocated_in(free_node_t *free_list, int lineno) {
    for (free_node_t *node = free_list; node != NULL; node = node->next) {
        if (DEBUG) {
            printf("found in free-list %p \n", node);
            fflush(stdout);
        }
        assertf(!get_alloc(header((char *)node)), "lineno: %d, addr: %p",
                lineno, node);
    }
}

static void assert_not_found_in(free_node_t *free_list, char *bp) {
    for (free_node_t *node = free_list; node != NULL; node = node->next) {
        assert(!((char *)node == bp));
    }
}
static void assert_found_in(free_node_t *free_list, char *bp) {
    size_t size = get_size(header(bp));
    size_t i = log2_of(size);
    for (free_node_t *node = free_list; node != NULL; node = node->next) {
        if ((char *)node == bp)
            return;
    }
    assertf(0, "p: %p size: %zu size-class: %zu", bp, size, i);
}

static void mm_heapcheck(int lineno) {
    if (DEBUG) {
        printf("called heapcheck from %d\n", lineno);
        fflush(stdout);
    }
    for (char *bp = heap_listp; get_size(header(bp)) > 0;
         bp = next_block_pointer(bp)) {
        size_t i = log2_of(get_size(header(bp))) - 1;
        if (!get_alloc(header(bp))) {
            assertf(get_size(header(bp)) == get_size(footer(bp)),
                    "lineno: %d, hd: %zu, ft: %zu, addr: %p", lineno,
                    get_size(header(bp)), get_size(footer(bp)), bp);
            assertf(get_alloc(header(bp)) == get_alloc(footer(bp)),
                    "lineno: %d, hd: %zu, ft: %zu, addr: %p", lineno,
                    get_alloc(header(bp)), get_alloc(footer(bp)), bp);
            assert_found_in(free_lists[i], bp);
        }
        if (get_alloc(header(bp))) {
            assert_not_found_in(free_lists[i], bp);
        }
    }
    for (size_t i = 0; i < w_bits; i++) {
        assert_none_allocated_in(free_lists[i], lineno);
    }
}
