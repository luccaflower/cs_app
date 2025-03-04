#include "unity.h"
#include "unity_internals.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
void test_pp_2_12_bitwise_expressions(void) {
  size_t x = 0x87654321;
  TEST_ASSERT_EQUAL(0x21, 0xFF & x);
  TEST_ASSERT_EQUAL(0x789ABC21, (~x & 0xFFFFFF00) + (0xFF & x));
  TEST_ASSERT_EQUAL(0x876543FF, (0xFF | x));
}

int bis(int x, int m) { return x | m; }

int bic(int x, int m) { return x & ~m; }

int bool_or(int x, int y) { return bis(x, y); }
int bool_xor(int x, int y) { return bis(bic(x, y), bic(y, x)); }
void test_2_13_bis_and_bic(void) {
  int x = 0x87654321;
  int y = 0x543210FE;
  TEST_ASSERT_EQUAL(x | y, bool_or(x, y));
  TEST_ASSERT_EQUAL(x ^ y, bool_xor(x, y));
}

int shift1(unsigned i) { return (int)((i << 24) >> 24); }
int shift2(unsigned i) { return ((int)i << 24) >> 24; }

void test_shift_operations(void) {
  TEST_ASSERT_EQUAL(shift1(0x00000076u), (int)0x76);
  TEST_ASSERT_EQUAL(shift2(0x00000076u), (int)0x76);
  TEST_ASSERT_EQUAL(shift1(0x87654321u), (int)0x21);
  TEST_ASSERT_EQUAL(shift2(0x87654321u), (int)0x21);
  TEST_ASSERT_EQUAL(shift1(0x000000C9u), (int)0xC9);
  TEST_ASSERT_EQUAL(shift2(0x000000C9u), (int)0xFFFFFFC9);
  TEST_ASSERT_EQUAL(shift1(0xedcba987u), (int)0x00000087);
  TEST_ASSERT_EQUAL(shift2(0xedcba987u), (int)0xFFFFFF87);
}

int uadd_ok(unsigned x, unsigned y) { return x + y >= x; }

void test_uadd_ok(void) {
  TEST_ASSERT_EQUAL(1, uadd_ok(1, 1));
  TEST_ASSERT_EQUAL(0, uadd_ok(0x80000000, 0x80000000));
  TEST_ASSERT_EQUAL(0, uadd_ok(0xFFFFFFFF, 0xFFFFFFFF));
  TEST_ASSERT_EQUAL(1, uadd_ok(0xF0000000, 0x0FFFFFFF));
  TEST_ASSERT_EQUAL(0, uadd_ok(0x7FFFFFFF, 0x80000001));
}

int div16(int x) {
  int x_neg = ((x + (1 << 4) - 1) & (!(x & 0x80000000) - 1));
  int x_pos = (!!x_neg - 1) & x;
  return (x_neg | x_pos) >> 4;
}
void test_div16(void) {
  TEST_ASSERT_EQUAL(16 / 16, div16(16));
  TEST_ASSERT_EQUAL(32 / 16, div16(32));
  TEST_ASSERT_EQUAL(35 / 16, div16(35));
  TEST_ASSERT_EQUAL(-35 / 16, div16(-35));
}

int count_bits(int x) {
  int count = 0;
  for (int i = 4; i >= 0; i--) {
    int present = (x & (1 << i));
    count = ((!!count - 1) & (!present - 1) & (1 + i)) | count;
  }
  return count;
}
void test_count_bits(void) {
  TEST_ASSERT_EQUAL(1, count_bits(1));
  TEST_ASSERT_EQUAL(2, count_bits(2));
  TEST_ASSERT_EQUAL(2, count_bits(3));
  TEST_ASSERT_EQUAL(3, count_bits(4));
}
