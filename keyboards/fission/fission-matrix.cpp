#include "matrix.h"
#include "print.h"
#include "spi.hpp"

#if DEBOUNCING_DELAY > 0
static bool debouncing;
static matrix_row_t matrix_debouncing[MATRIX_ROWS];
#endif
/* matrix state(1:on, 0:off) */
static matrix_row_t matrix[MATRIX_ROWS];

matrix_row_t matrix_get_row(uint8_t row) { return matrix[row]; }

void matrix_init(void) {
}

uint8_t matrix_scan(void) {}

void matrix_print(void) {
  print("\nr/c 0123456789ABCDEF\n");

  for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
    phex(row);
    print(": ");
    print_bin_reverse16(matrix_get_row(row));
    print("\n");
  }
}
