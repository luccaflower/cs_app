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

result_t process_trace(FILE *trace, int set_bits, int block_bits);
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
  result_t result = process_trace(trace, set_bits, block_bits);
  printSummary(result.hits, result.misses, result.evictions);
  return EXIT_SUCCESS;
}

typedef struct {
  _Bool valid;
  unsigned int tag;
} entry_t;
void zero_out(entry_t *array, unsigned int len);
void hit_miss_evict(entry_t *sets, unsigned int set, unsigned int tag,
                    result_t *result);
result_t process_trace(FILE *trace, int set_bits, int block_bits) {
  result_t result = {.hits = 0, .evictions = 0, .misses = 0};

  unsigned int set_len = 1 << set_bits;
  unsigned int set_mask = set_len - 1;
  entry_t sets[set_len];
  zero_out(sets, set_len);

  char instruction;
  unsigned int addr;
  unsigned int size;
  while (fscanf(trace, " %c %x,%u\n", &instruction, &addr, &size) != -1) {
    unsigned int set = (addr >> block_bits) & set_mask;
    unsigned int tag = addr >> (set_bits + block_bits);
    switch (instruction) {
    case 'M':
      hit_miss_evict(sets, set, tag, &result);
      hit_miss_evict(sets, set, tag, &result);
      break;
    case 'L':
    case 'S':
      hit_miss_evict(sets, set, tag, &result);
    }
  }
  return result;
}
void hit_miss_evict(entry_t *sets, unsigned int set, unsigned int tag,
                    result_t *result) {
  entry_t entry = sets[set];
  if (!entry.valid) {
    result->misses++;
    entry_t new_entry = {.valid = true, .tag = tag};
    sets[set] = new_entry;
  } else if (entry.tag != tag) {
    result->misses++;
    result->evictions++;
    entry_t new_entry = {.valid = true, .tag = tag};
    sets[set] = new_entry;
  } else {
    result->hits++;
  }
}

void zero_out(entry_t *array, unsigned int len) {
  for (unsigned int i = 0; i < len; i++) {
    entry_t initial_entry = {.valid = false, .tag = 0};
    array[i] = initial_entry;
  }
}
