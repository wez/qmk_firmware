/*
Copyright 2017 Wez Furlong

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

#ifndef CONFIG_H
#define CONFIG_H

#include "config_common.h"

/* USB Device descriptor parameter */
#define VENDOR_ID       0xFEED
#define PRODUCT_ID      0x1337
#define DEVICE_VER      0x0001
#define MANUFACTURER    Wez Furlong
#define PRODUCT         flutterby
#define DESCRIPTION     Wez Mk2

/* key matrix size */
#define MATRIX_ROWS 4
#define MATRIX_COLS 16
#define DIODE_DIRECTION CUSTOM_MATRIX

#define MOUSEKEY_ANALOG_XY 1

// We have an Adafruit BLE SPI Friend board attached
#define AdafruitBleResetPin -1
#define AdafruitBleCSPin D3
#define AdafruitBleIRQPin C6
#define AdafruitBlePowerPin D2

// Turn off the mode leds on the BLE module
#define ADAFRUIT_BLE_ENABLE_MODE_LEDS 0

/* Debounce reduces chatter (unintended double-presses) - set 0 if debouncing is not needed */
#define DEBOUNCING_DELAY 5

/* key combination for magic key command */
#define IS_COMMAND() ( \
    keyboard_report->mods == (MOD_BIT(KC_LSHIFT) | MOD_BIT(KC_RSHIFT)) \
)

#endif
