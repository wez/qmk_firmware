/* Bluetooth Low Energy Protocol for QMK.
 * Author: Wez Furlong, 2016
 * Supports the Adafruit BLE board built around the nRF51822 chip.
 */
#pragma once
#ifdef ADAFRUIT_BLE_ENABLE
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef AdafruitBlePowerPin
/* Power on or off the BLE device */
extern bool adafruit_ble_power_enable(bool enable);
extern bool adafruit_ble_power_is_enabled(void);
#endif

/* Instruct the module to enable HID keyboard support and reset */
extern bool adafruit_ble_enable_keyboard(void);

/* Query to see if the BLE module is connected */
extern bool adafruit_ble_query_is_connected(void);

/* Returns true if we believe that the BLE module is connected.
 * This uses our cached understanding that is maintained by
 * calling ble_task() periodically. */
extern bool adafruit_ble_is_connected(void);

/* Call this periodically to process BLE-originated things */
extern void adafruit_ble_task(void);

/* Generates keypress events for a set of keys.
 * The hid modifier mask specifies the state of the modifier keys for
 * this set of keys.
 * Also sends a key release indicator, so that the keys do not remain
 * held down. */
extern bool adafruit_ble_send_keys(uint8_t hid_modifier_mask, uint8_t *keys,
                                   uint8_t nkeys);

/* Send a consumer keycode, holding it down for the specified duration
 * (milliseconds) */
extern bool adafruit_ble_send_consumer_key(uint16_t keycode, int hold_duration);

#ifdef MOUSE_ENABLE
/* Send a mouse/wheel movement report.
 * The parameters are signed and indicate positive of negative direction
 * change. */
extern bool adafruit_ble_send_mouse_move(int8_t x, int8_t y, int8_t scroll,
                                         int8_t pan, uint8_t buttons);
#endif

extern bool adafruit_ble_set_mode_leds(bool on);
extern bool adafruit_ble_set_power_level(int8_t level);

extern bool adafruit_ble_set_battery_pct(uint8_t pct);

#ifdef __cplusplus
}
#endif

#endif // ADAFRUIT_BLE_ENABLE
