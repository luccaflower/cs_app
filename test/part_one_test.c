#include "unity.h"
#include "unity_internals.h"
#include <stdbool.h>
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

int eq(int x, int y) { return ((x & y) | (~x & ~y)); }

void test_2_15_eq(void) {
  TEST_ASSERT_EQUAL(5 == 15, eq(5, 15));
  TEST_ASSERT_EQUAL(10 == 10, eq(10, 10));
}
