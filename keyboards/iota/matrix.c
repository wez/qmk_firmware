/*
Copyright 2016 Wez Furlong

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#if defined(__AVR__)
#include <avr/io.h>
#endif
#include <stdbool.h>

#include "debug.h"
#include "iota.h"
#include "config.h"
#include "i2cmaster.h"
#include "matrix.h"
#include "print.h"
#include "timer.h"
#include "util.h"
#include "wait.h"
#include "pincontrol.h"
#include "lufa.h"
#include "suspend.h"

// The keyboard matrix is attached to the following pins:
// row0: A0 - PF7
// row1: A1 - PF6
// row2: A2 - PF5
// row3: A3 - PF4
// row4: A4 - PF3
// col0-7: mcp23107 GPIOA0-7
// col8-14: mcp23107 GPIOB0-6 (note that B7 is unused)
// PD3 (INT3) connect to interrupt pins on mcp23107
static const uint8_t row_pins[MATRIX_ROWS] = {F7, F6, F5, F4, F3};
#if (DEBOUNCING_DELAY > 0)
static bool debouncing;
static matrix_row_t matrix_debouncing[MATRIX_ROWS];
#endif
/* matrix state(1:on, 0:off) */
static matrix_row_t matrix[MATRIX_ROWS];

// matrix power saving
#define MATRIX_POWER_SAVE       10000
static uint32_t matrix_last_modified;
static bool matrix_powered_on;

static inline void select_row(uint8_t row) {
  uint8_t pin = row_pins[row];

  pinMode(pin, PinDirectionOutput);
  digitalWrite(pin, PinLevelLow);
}

static inline void unselect_row(uint8_t row) {
  uint8_t pin = row_pins[row];

  pinMode(pin, PinDirectionInput);
  digitalWrite(pin, PinLevelHigh);
}

static void unselect_rows(void) {
  for (uint8_t x = 0; x < MATRIX_ROWS; x++) {
    unselect_row(x);
  }
}

static void select_rows(void) {
  for (uint8_t x = 0; x < MATRIX_ROWS; x++) {
    select_row(x);
  }
}

// This is just a placeholder so that we can participate in power
// management without faulting the MCU
EMPTY_INTERRUPT(INT3_vect);

void matrix_power_down(void) {
  matrix_powered_on = false;

  iota_gfx_off();

  // if any buttons are pressed, we want to wake up.
  // Set the matrix up for that.
  select_rows();
  if (iota_mcp23017_enable_interrupts()) {
    pinMode(PD3, PinDirectionInput);
    EIMSK |= _BV(INT3);
  }
}

void matrix_power_up(void) {
  matrix_init();
}

void matrix_init(void) {
  // Disable matrix interrupts
  EIMSK &= ~_BV(PCINT6);
  pinMode(PD3, PinDirectionInput);

  i2c_init();
  iota_gfx_init();
  iota_mcp23017_init();
  unselect_rows();

  matrix_powered_on = true;
  matrix_last_modified = timer_read32();
}

bool matrix_is_on(uint8_t row, uint8_t col) {
  return (matrix[row] & ((matrix_row_t)1 << col));
}

matrix_row_t matrix_get_row(uint8_t row) { return matrix[row]; }

extern uint16_t iota_mcp23017_read(void);

static bool read_cols_on_row(matrix_row_t current_matrix[],
                             uint8_t current_row) {
  // Store last value of row prior to reading
  matrix_row_t last_row_value = current_matrix[current_row];

  // Clear data in matrix row
  current_matrix[current_row] = 0;

  // Select row and wait for row selection to stabilize
  select_row(current_row);
  wait_us(30);

  current_matrix[current_row] = iota_mcp23017_read();

  // Unselect row
  unselect_row(current_row);

  return (last_row_value != current_matrix[current_row]);
}

uint8_t matrix_scan(void) {
  for (uint8_t current_row = 0; current_row < MATRIX_ROWS; current_row++) {
#if (DEBOUNCING_DELAY > 0)
    bool matrix_changed = read_cols_on_row(matrix_debouncing, current_row);

    if (matrix_changed) {
      debouncing = true;
      matrix_last_modified = timer_read32();
    }
#else
    if (read_cols_on_row(matrix, current_row)) {
      matrix_last_modified = timer_read32();
    }
#endif
  }
#if (DEBOUNCING_DELAY > 0)
  if (debouncing && (timer_elapsed32(matrix_last_modified) > DEBOUNCING_DELAY)) {
    for (uint8_t i = 0; i < MATRIX_ROWS; i++) {
      matrix[i] = matrix_debouncing[i];
    }
    debouncing = false;
  }
#endif

  // power management
  if (matrix_powered_on && (USB_DeviceState == DEVICE_STATE_Suspended ||
                            USB_DeviceState == DEVICE_STATE_Unattached) &&
      timer_elapsed32(matrix_last_modified) > MATRIX_POWER_SAVE) {
    suspend_power_down();
  }

  matrix_scan_quantum();
  return 1;
}

void matrix_print(void) {
  print("\nr/c 0123456789ABCDEF\n");

  for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
    phex(row);
    print(": ");
    print_bin_reverse16(matrix_get_row(row));
    print("\n");
  }
}
