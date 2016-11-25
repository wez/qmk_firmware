/* Bluetooth Low Energy Protocol for QMK.
 * Author: Wez Furlong, 2016
 * Supports the Adafruit BLE board built around the nRF51822 chip.
 */
#pragma once
#ifdef BLE_ENABLE
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
/* Send the full AT command line held in cmd.
 * Will automatically add the newline to the command; don't include it
 * in the string you send in.
 * The response from the command will be stored into the resp buffer.
 * Returns true for an OK response, false for an ERROR response. */
extern bool ble_at_command(const char *cmd, char *resp, uint16_t resplen, bool verbose);
extern bool ble_at_command_P(const char *cmd, char *resp, uint16_t resplen);

/* Instruct the module to enable HID keyboard support and reset */
extern bool ble_enable_keyboard(void);

/* Query the RSSI for the bluetooth connection.
 * Returns the value in dBm if connected, else 0.
 */
extern int ble_get_rssi(void);

/* Query to see if the BLE module is connected */
extern bool ble_query_is_connected(void);

/* Returns true if we believe that the BLE module is connected.
 * This uses our cached understanding that is maintained by
 * calling ble_task() periodically. */
extern bool ble_is_connected(void);

/* Call this periodically to process BLE-originated things */
extern void ble_task(void);

/* Generates keypress events for a set of keys.
 * The hid modifier mask specifies the state of the modifier keys for
 * this set of keys.
 * Also sends a key release indicator, so that the keys do not remain
 * held down. */
extern bool ble_send_keys(uint8_t hid_modifier_mask, uint8_t *keys, uint8_t nkeys);

/* Send a consumer keycode, holding it down for the specified duration
 * (milliseconds) */
extern bool ble_send_consumer_key(uint16_t keycode, int hold_duration);

#ifdef MOUSE_ENABLE
/* Send a mouse/wheel movement report.
 * The parameters are signed and indicate positive of negative direction
 * change. */
extern bool ble_send_mouse_move(int8_t x, int8_t y, int8_t scroll, int8_t pan);
#endif

/* Compute battery voltage by reading an analog pin.
 * Returns the integer number of millivolts */
extern uint16_t ble_read_battery_voltage(void);
#endif
