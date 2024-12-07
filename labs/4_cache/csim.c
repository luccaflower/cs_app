#include "cachelab.h"
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  int hits;
  int misses;
  int evictions;
} result_t;

typedef struct {
  _Bool valid;
  unsigned int tag;
} block_t;

typedef struct {
  unsigned int set_count;
  unsigned int set_bits;
  unsigned int associativity;
  unsigned int block_bits;
  block_t **sets;
} cache_t;

result_t process_trace(FILE *trace, cache_t *cache);
cache_t *new_cache(unsigned int set_bits, unsigned int associativity,
                   unsigned int block_bits);
int main(int argc, char **argv) {
  char opt;
  unsigned int set_bits, associativity, block_bits;
  FILE *trace;
  while ((opt = getopt(argc, argv, "s:E:b:t:")) != -1) {
    switch (opt) {
    case 's':
      set_bits = atoi(optarg);
      break;
    case 'E':
      associativity = atoi(optarg);
      break;
    case 'b':
      block_bits = atoi(optarg);
      break;
    case 't':
      trace = fopen(optarg, "r");
      if (!trace) {
        fputs("Invalid trace file:", stderr);
        fputs(optarg, stderr);
        return EXIT_FAILURE;
      }
    }
  }
  if (false)
    printf("%u", associativity);
  cache_t *cache = new_cache(set_bits, associativity, block_bits);
  result_t result = process_trace(trace, cache);
  printSummary(result.hits, result.misses, result.evictions);
  return EXIT_SUCCESS;
}

void hit_miss_evict(block_t **sets, unsigned int set, unsigned int tag,
                    unsigned int associativity, result_t *result);
cache_t *new_cache(unsigned int set_bits, unsigned int associativity,
                   unsigned int block_bits) {
  unsigned int set_count = 1 << set_bits;
  cache_t *cache = calloc(1, sizeof(*cache));
  cache->set_count = set_count;
  cache->set_bits = set_bits;
  cache->associativity = associativity;
  cache->block_bits = block_bits;
  block_t **sets = calloc(set_count, sizeof(block_t *));
  cache->sets = sets;
  for (size_t i = 0; i < set_count; i++) {
    sets[i] = calloc(associativity, sizeof(block_t));
  }
  return cache;
}

result_t process_trace(FILE *trace, cache_t *cache) {
  result_t result = {.hits = 0, .evictions = 0, .misses = 0};

  unsigned int set_mask = cache->set_count - 1;
  char instruction;
  unsigned int addr;
  unsigned int size;
  while (fscanf(trace, " %c %x,%u\n", &instruction, &addr, &size) != -1) {
    unsigned int set = (addr >> cache->block_bits) & set_mask;
    unsigned int tag = addr >> (cache->set_bits + cache->block_bits);
    switch (instruction) {
    case 'M':
      hit_miss_evict(cache->sets, set, tag, cache->associativity, &result);
      hit_miss_evict(cache->sets, set, tag, cache->associativity, &result);
      break;
    case 'L':
    case 'S':
      hit_miss_evict(cache->sets, set, tag, cache->associativity, &result);
    }
  }
  return result;
}

int index_of(unsigned int tag, block_t *set, unsigned int associativity);
void move_up(unsigned int index, block_t *set);
typedef enum { NO_EVICT, EVICT } eviction_t;
eviction_t insert(unsigned int tag, block_t *set, unsigned int associativity);
void hit_miss_evict(block_t **sets, unsigned int set_index, unsigned int tag,
                    unsigned int associativity, result_t *result) {
  block_t *set = sets[set_index];
  int index = index_of(tag, set, associativity);
  if (index == -1) {
    result->misses++;
    eviction_t eviction = insert(tag, set, associativity);
    if (eviction == EVICT)
      result->evictions++;
  } else {
    result->hits++;
    move_up(index, set);
  }
}
int index_of(unsigned int tag, block_t *set, unsigned int associativity) {
  for (size_t i = 0; i < associativity; i++) {
    if (set[i].valid && set[i].tag == tag) {
      return i;
    }
  }
  return -1;
}

void move_up(unsigned int index, block_t *set) {
  block_t to_move = set[index];
  for (size_t i = index; i > 0; i--) {
    set[i] = set[i - 1];
  }
  set[0] = to_move;
}

eviction_t insert(unsigned int tag, block_t *set, unsigned int associativity) {
  block_t block = {.valid = true, .tag = tag};
  for (size_t i = 0; i < associativity; i++) {
    if (!set[i].valid) {
      set[i] = block;
      move_up(i, set);
      return NO_EVICT;
    }
  }
  set[associativity - 1] = block;
  move_up(associativity - 1, set);
  return EVICT;
}
