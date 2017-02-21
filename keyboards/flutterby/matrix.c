/*
Copyright 2016-2017 Wez Furlong

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
#include "flutterby.h"
#include "config.h"
#include "tmk_core/protocol/lufa/LUFA-git/LUFA/Drivers/Peripheral/TWI.h"
#include "matrix.h"
#include "print.h"
#include "timer.h"
#include "util.h"
#include "wait.h"
#include "pincontrol.h"
#include "mousekey.h"
#ifdef PROTOCOL_LUFA
#include "lufa.h"
#endif
#include "suspend.h"
#ifdef ADAFRUIT_BLE_ENABLE
#include "adafruit_ble.h"
#endif
#include <util/atomic.h>
#include <string.h>

// The keyboard matrix is attached to the following pins:
// thumbstick X: A0 - PF7
// thumbstick Y: A1 - PF6
// row0: A2 - PF5
// row1: A3 - PF4
// row2: A4 - PF1
// row3: A5 - PF0
// col0-15:   sx1509
static const uint8_t row_pins[MATRIX_ROWS] = {F5, F4, F1, F0};
#if DEBOUNCING_DELAY > 0
static bool debouncing;
static matrix_row_t matrix_debouncing[MATRIX_ROWS];
#endif
/* matrix state(1:on, 0:off) */
static matrix_row_t matrix[MATRIX_ROWS];

// matrix power saving
#define MATRIX_POWER_SAVE       600000 // 10 minutes
static uint32_t matrix_last_modified;

#define ENABLE_BLE_MODE_LEDS 0

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

static void select_rows(void) {
  for (uint8_t x = 0; x < MATRIX_ROWS; x++) {
    select_row(x);
  }
}

void matrix_power_down(void) {
#if defined(ADAFRUIT_BLE_ENABLE) && defined(AdafruitBlePowerPin)
  adafruit_ble_power_enable(false);
#elif defined(ADAFRUIT_BLE_ENABLE) && ADAFRUIT_BLE_ENABLE_MODE_LEDS
  adafruit_ble_set_mode_leds(false);
#endif
}

#include "LUFA/Drivers/Peripheral/ADC.h"

void matrix_power_up(void) {
  unselect_rows();

  memset(matrix, 0, sizeof(matrix));
#if DEBOUNCING_DELAY > 0
  memset(matrix_debouncing, 0, sizeof(matrix_debouncing));
#endif

  matrix_last_modified = timer_read32();
#ifdef DEBUG_MATRIX_SCAN_RATE
  scan_timer = timer_read32();
  scan_count = 0;
#endif

#if defined(ADAFRUIT_BLE_ENABLE) && defined(AdafruitBlePowerPin)
  adafruit_ble_power_enable(true);
#elif defined(ADAFRUIT_BLE_ENABLE) && ADAFRUIT_BLE_ENABLE_MODE_LEDS
  adafruit_ble_set_mode_leds(true);
#endif

#ifdef MOUSEKEY_ENABLE
  ADC_Init(ADC_SINGLE_CONVERSION | ADC_PRESCALE_32);
  ADC_SetupChannel(6); // Y
  ADC_SetupChannel(7); // X
#endif
}

void matrix_init(void) {
  TWI_Init(TWI_BIT_PRESCALE_1, TWI_BITLENGTH_FROM_FREQ(1, 400000));
  sx1509_init();

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

  current_matrix[current_row] = sx1509_read();

  unselect_row(current_row);

  return last_row_value != current_matrix[current_row];
}

static uint8_t matrix_scan_raw(void) {
  if (!sx1509_make_ready()) {
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

  return 1;
}

#ifdef MOUSEKEY_ENABLE

#define StickMax 832
#define StickMin 160
#define StickCenter 512
#define StickSlop 64  // Dead band around the middle

// Take an input in the range 0-1024 and return a value in the
// range -127 to 127.
// [0   160    512     832    1024]
//      [-127    0     127]
static int8_t map_value(int32_t v) {
  v -= StickCenter;

  int scale = 1;
  if (v < 0) {
    scale = -1;
    v = -v;
  }

  if (v < StickSlop) {
    // It's within the dead band, so treat it as zero
    return 0;
  }

  int band = v / 64;

  return band * mk_max_speed * scale / 3;
}

static void thumbstick_read(uint8_t chanmask, int8_t *value, int8_t *last_value,
                            bool *dirty) {
  *value = map_value(ADC_GetChannelReading(ADC_REFERENCE_AVCC | chanmask));

  if (*value != *last_value) {
    *last_value = *value;
    *dirty = true;
  }
}

void process_thumbstick(void) {
  // Cache the prior read to avoid over-reporting mouse movement
  static int8_t last_x = 0;
  static int8_t last_y = 0;
  int8_t x;
  int8_t y;

  bool dirty = false;
  thumbstick_read(ADC_CHANNEL7, &x, &last_x, &dirty);
  thumbstick_read(ADC_CHANNEL6, &y, &last_y, &dirty);

  if (dirty) {
    mousekey_set_x(x);
    mousekey_set_y(-y);
    mousekey_send();
  }
}
#endif

uint8_t matrix_scan(void) {
  if (!matrix_scan_raw()) {
    return 0;
  }

  // Try to manage battery power a little better than the default scan.
  // If the user is idle for a while, turn off some things that draw
  // power.

  if (timer_elapsed32(matrix_last_modified) > MATRIX_POWER_SAVE) {
    matrix_power_down();

    // Turn on all the rows; we're going to read the columns in
    // the loop below to see if we got woken up.
    select_rows();

    while (true) {
      suspend_power_down();

      // See if any keys have been pressed.
      if (!sx1509_read()) {
        continue;
      }

      // Wake us up
      matrix_last_modified = timer_read32();
      suspend_wakeup_init();
      matrix_power_up();

      // Wake the host up, if appropriate.
      if (USB_DeviceState == DEVICE_STATE_Suspended &&
          USB_Device_RemoteWakeupEnabled) {
        USB_Device_SendRemoteWakeup();
      }
      break;
    }
  }

#ifdef MOUSEKEY_ENABLE
  process_thumbstick();
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
