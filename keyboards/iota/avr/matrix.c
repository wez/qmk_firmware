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
#ifdef PROTOCOL_LUFA
#include "lufa.h"
#endif
#include "suspend.h"
#ifdef BLE_ENABLE
#include "ble.h"
#endif
#include <util/atomic.h>
#include <string.h>

// The keyboard matrix is attached to the following pins:
// row0: A0 - PF7
// row1: A1 - PF6
// row2: A2 - PF5
// row3: A3 - PF4
// row4: A4 - PF1
// col0-7: mcp23107 GPIOA0-7
// col8-14: mcp23107 GPIOB1-7 (note that B0 is unused)
// PD3 (INT3) connect to interrupt pins on mcp23107
static const uint8_t row_pins[MATRIX_ROWS] = {F7, F6, F5, F4, F1};
#if DEBOUNCING_DELAY > 0
static bool debouncing;
static matrix_row_t matrix_debouncing[MATRIX_ROWS];
#endif
/* matrix state(1:on, 0:off) */
static matrix_row_t matrix[MATRIX_ROWS];

// matrix power saving
#define MATRIX_POWER_SAVE       10000
static uint32_t matrix_last_modified;
static bool matrix_powered_on;

#ifdef DEBUG_MATRIX_SCAN_RATE
static uint32_t scan_timer;
static uint32_t scan_count;
#endif

static inline void select_row(uint8_t row) {
  uint8_t pin = row_pins[row];

  pinMode(pin, PinDirectionOutput);
  digitalWrite(pin, PinLevelLow);
}

static inline void unselect_row(uint8_t row) {
  uint8_t pin = row_pins[row];

  digitalWrite(pin, PinLevelHigh);
  pinMode(pin, PinDirectionInput);
}

static void unselect_rows(void) {
  for (uint8_t x = 0; x < MATRIX_ROWS; x++) {
    unselect_row(x);
  }
}

#if FANCY_POWER_MGR
static void select_rows(void) {
  for (uint8_t x = 0; x < MATRIX_ROWS; x++) {
    select_row(x);
  }
}

// This is just a placeholder so that we can participate in power
// management without faulting the MCU
EMPTY_INTERRUPT(INT3_vect);
#endif

void matrix_power_down(void) {
  matrix_powered_on = false;

#if FANCY_POWER_MGR
  iota_gfx_off();

  // if any buttons are pressed, we want to wake up.
  // Set the matrix up for that.
  select_rows();

  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    if (iota_mcp23017_enable_interrupts()) {
      pinMode(PD3, PinDirectionInput);
      EIMSK |= _BV(INT3);
    }
  }
#endif
}

void matrix_power_up(void) {
  unselect_rows();

  memset(matrix, 0, sizeof(matrix));
#if DEBOUNCING_DELAY > 0
  memset(matrix_debouncing, 0, sizeof(matrix_debouncing));
#endif

  matrix_powered_on = true;
  matrix_last_modified = timer_read32();
#ifdef DEBUG_MATRIX_SCAN_RATE
  scan_timer = timer_read32();
  scan_count = 0;
#endif
}

void matrix_init(void) {
#if FANCY_POWER_MGR
  // Disable matrix interrupts
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    EIMSK &= ~_BV(PCINT6); // this seems to break timers FIXME!
    pinMode(PD3, PinDirectionInput);
  }
#endif

  i2c_init();
  iota_mcp23017_init();
  iota_gfx_init();

  matrix_power_up();
}

bool matrix_is_on(uint8_t row, uint8_t col) {
  return (matrix[row] & ((matrix_row_t)1 << col));
}

matrix_row_t matrix_get_row(uint8_t row) { return matrix[row]; }

static bool read_cols_on_row(matrix_row_t current_matrix[],
                             uint8_t current_row) {
  // Store last value of row prior to reading
  matrix_row_t last_row_value = current_matrix[current_row];

  // Clear data in matrix row
  current_matrix[current_row] = 0;

  // Select row and wait for row selection to stabilize
  select_row(current_row);
  _delay_us(30);

  current_matrix[current_row] = iota_mcp23017_read();

  unselect_row(current_row);

  return last_row_value != current_matrix[current_row];
}

uint8_t matrix_scan(void) {
  iota_gfx_task();

  if (!iota_mcp23017_make_ready()) {
    return 0;
  }

  for (uint8_t current_row = 0; current_row < MATRIX_ROWS; current_row++) {
    bool matrix_changed = read_cols_on_row(
#if DEBOUNCING_DELAY > 0
        matrix_debouncing,
#else
        matrix,
#endif
        current_row);

    if (matrix_changed) {
#if DEBOUNCING_DELAY > 0
      debouncing = true;
#endif
      matrix_last_modified = timer_read32();
    }
  }

#ifdef DEBUG_MATRIX_SCAN_RATE
  scan_count++;

  uint32_t timer_now = timer_read32();
  if (TIMER_DIFF_32(timer_now, scan_timer)>1000) {
    print("matrix scan frequency: ");
    pdec(scan_count);
    print("\n");

    scan_timer = timer_now;
    scan_count = 0;
  }
#endif

#if DEBOUNCING_DELAY > 0
  if (debouncing &&
      (timer_elapsed32(matrix_last_modified) > DEBOUNCING_DELAY)) {
    memcpy(matrix, matrix_debouncing, sizeof(matrix));
    debouncing = false;
  }
#endif

#if FANCY_POWER_MGR
  // power management
  if (matrix_powered_on && (USB_DeviceState == DEVICE_STATE_Suspended ||
        USB_DeviceState == DEVICE_STATE_Unattached) &&
#if BLE_ENABLE
      !ble_is_connected() &&
#endif
      timer_elapsed32(matrix_last_modified) > MATRIX_POWER_SAVE) {
    suspend_power_down();
  }
#endif

#if 0
  debug_enable = true;
  debug_keyboard = false;
#endif

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
