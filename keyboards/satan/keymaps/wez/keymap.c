#include "satan.h"
#include "action_layer.h"
#include "mousekey.h"

/* GH60 LEDs
 *   B2 Capslock LED
 * Unlike the GH60 keyboard, this satan model doesn't have
 * WASD, FN and arrow key LEDs, just the backlight.
 */
inline void gh60_caps_led_on(void)      { DDRB |=  (1<<2); PORTB &= ~(1<<2); }
inline void gh60_caps_led_off(void)     { DDRB &= ~(1<<2); PORTB &= ~(1<<2); }

/* Satan GH60 matrix layout with both shift keys and backspace split
   * ,-----------------------------------------------------------.
   * | 00 |01| 02| 03| 04| 05| 06| 07| 08| 09| 0a| 0b| 0c| 0d| 49|
   * |-----------------------------------------------------------|
   * | 10  | 11| 12| 13| 14| 15| 16| 17| 18| 19| 1a| 1b| 1c|  1d |
   * |-----------------------------------------------------------|
   * | 20    | 21| 22| 23| 24| 25| 26| 27| 28| 29| 2a| 2b| 2c|2d |
   * |-----------------------------------------------------------|
   * | 30 | 31| 32| 33| 34| 35| 36| 37| 38| 39| 3a| 3b|  3d | 3c |
   * |-----------------------------------------------------------|
   * | 40 | 41 | 42 |        45             | 4a | 4b | 4c | 4d  |
   * `-----------------------------------------------------------'
 */
// The first section contains all of the arguments
// The second converts the arguments into a two-dimensional array
#define MY_KEYMAP( \
  k00, k01, k02, k03, k04, k05, k06, k07, k08, k09, k0a, k0b, k0c, k0d, k49,\
	k10, k11, k12, k13, k14, k15, k16, k17, k18, k19, k1a, k1b, k1c, k1d, \
	k20, k21, k22, k23, k24, k25, k26, k27, k28, k29, k2a, k2b, k2c, k2d, \
	k30, k31, k32, k33, k34, k35, k36, k37, k38, k39, k3a, k3b, k3d, k3c, \
	k40, k41, k42,           k45,                     k4a, k4b, k4c, k4d  \
) \
{ \
	{k00, k01, k02, k03, k04, k05, k06, k07, k08, k09, k0a, k0b, k0c, k0d}, \
	{k10, k11, k12, k13, k14, k15, k16, k17, k18, k19, k1a, k1b, k1c, k1d}, \
	{k20, k21, k22, k23, k24, k25, k26, k27, k28, k29, k2a, k2b, XXX, k2d}, \
	{k30, k31, k32, k33, k34, k35, k36, k37, k38, k39, k3a, k3b, k3c, k3d}, \
	{k40, k41, k42, XXX, XXX, k45, XXX, XXX, XXX, k49, k4a, k4b, k4c, k4d}  \
}

// Each layer gets a name for readability, which is then used in the keymap matrix below.
enum layer_id {
  BASE = 0,
  MDIA,
  FUNC
};

// Tap dance function ids
enum tap_action_id {
  TD_SFT_CAPS,
};

enum function_id {
  FN_MDIA_TOG,
};

#define ____ KC_TRNS

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
  /* Keymap BASE: (Base Layer) Default Layer
   * ,--------------------------------------------------------------------------.
   * | `  | 1  | 2  | 3  | 4  | 5  | 6  | 7  | 8  | 9  | 0  | -  | =  |Bksp|Alt |
   * |--------------------------------------------------------------------------|
   * |Tab   | Q  | W  | E  | R  | T  | Y  | U  | I  | O  | P  | [  | ]  | \     |
   * |--------------------------------------------------------------------------|
   * |Esc/Ctrl| A  | S  | D  | F  | G  | H  | J  | K  | L  |;FN | '   | Return  |
   * |--------------------------------------------------------------------------|
   * |SCaps|    | Z  | X  | C  | V  | B  | N  | M  | ,  | .  | /  | Up | Shift  |
   * |--------------------------------------------------------------------------|
   * | FN  | Alt | Gui |        Space                   | Gui | Left| Down|Right|
   * `--------------------------------------------------------------------------'
   */
[BASE] = MY_KEYMAP(
  KC_GRV,         KC_1,   KC_2,   KC_3,   KC_4,   KC_5,   KC_6,   KC_7,   KC_8,   KC_9,   KC_0,             KC_MINS, KC_EQL, KC_BSPC, KC_RALT,
  KC_TAB,         KC_Q,   KC_W,   KC_E,   KC_R,   KC_T,   KC_Y,   KC_U,   KC_I,   KC_O,   KC_P,             KC_LBRC, KC_RBRC,KC_BSLS,
  CTL_T(KC_ESC),  KC_A,   KC_S,   KC_D,   KC_F,   KC_G,   KC_H,   KC_J,   KC_K,   KC_L,   LT(MDIA, KC_SCLN),KC_QUOT, XXX,    KC_ENT,
  TD(TD_SFT_CAPS),XXX,    KC_Z,   KC_X,   KC_C,   KC_V,   KC_B,   KC_N,   KC_M,   KC_COMM,KC_DOT,           KC_SLSH, KC_UP,  KC_RSFT,
  F(FN_MDIA_TOG), KC_LALT,KC_LGUI,                KC_SPC,                                 KC_RGUI,          KC_LEFT, KC_DOWN,KC_RIGHT),

  /* Keymap MDIA: Media and mouse keys
   * ,--------------------------------------------------------------------------.
   * |    | F1 | F2 | F3 | F4 | F5 | F6 | F7 | F8 | F9 | F10| VDn| VUp| Br-|Br+ |
   * |--------------------------------------------------------------------------|
   * |      |    |MsUp|    |ScDn|    |    |    |    |    |    | BL-| BL+| BL    |
   * |--------------------------------------------------------------------------|
   * |        |MsL |MsDn|MsRt|ScUp|    |     |    |Lclk|Rclk|    |Play|         |
   * |--------------------------------------------------------------------------|
   * |     |RST |    |    |    |    |    |    |    |Prev|Next|    |PgUp|        |
   * |--------------------------------------------------------------------------|
   * |     |     |     |                                |     | Home| PgDn| End |
   * `--------------------------------------------------------------------------'
   */
[MDIA] = MY_KEYMAP(
  ____, KC_F1,  KC_F2,   KC_F3,   KC_F4,         KC_F5, KC_F6, KC_F7, KC_F8,   KC_F9,   KC_F10,  KC_VOLD, KC_VOLU, KC_F14,  KC_F15,
  ____, ____,   KC_MS_U, ____,    KC_MS_WH_DOWN, ____,  ____,  ____,  ____,    ____,    ____,    BL_DEC,  BL_INC,  BL_TOGG,
  ____, KC_MS_L,KC_MS_D, KC_MS_R, KC_MS_WH_UP,   ____,  ____,  ____,  KC_BTN1, KC_BTN2, ____,    KC_MPLY, ____,    ____,
  ____, RESET,  ____,    ____,    ____,          ____,  ____,  ____,  ____,    KC_MPLY, KC_MNXT, ____,    KC_PGUP, ____,
  ____, ____,   ____,                 ____,                                             ____,    KC_HOME, KC_PGDN, KC_END),
};

// Using TD(n) causes the firmware to lookup the tapping action here
qk_tap_dance_action_t tap_dance_actions[] = {
  // Double tap shift to turn on caps lock
  [TD_SFT_CAPS] = ACTION_TAP_DANCE_DOUBLE(KC_LSFT, KC_CAPS),
};

// Runs just one time when the keyboard initializes.
void matrix_init_user(void) {
  // mousekey: A bit faster by default, use accel keys for fine control
  mk_max_speed = 10;
  // accelerate a bit faster than usual
  mk_time_to_max = 15;
  // Slightly slower mouse wheel speed than the default
  mk_wheel_max_speed = 4;
}

static void blink_led(void) {
  gh60_caps_led_off();

  gh60_caps_led_on();
  _delay_ms(150);
  gh60_caps_led_off();

  _delay_ms(50);

  gh60_caps_led_on();
  _delay_ms(150);
  gh60_caps_led_off();

  _delay_ms(50);

  gh60_caps_led_on();
  _delay_ms(150);
  gh60_caps_led_off();
}

// Runs constantly in the background, in a loop.
void matrix_scan_user(void) {
#if 0
  if (layer && !in_layer) {
    blink_led();
  } else if (!layer && in_layer) {
    blink_led();
  }

  // (Double-tap left shift to toggle caps lock)
  if (host_keyboard_leds() & (1 << USB_LED_CAPS_LOCK)) {
    gh60_caps_led_on();
  } else {
    gh60_caps_led_off();
  }
#endif

#if 0
  // Activate the backlight if any layer is active
  uint8_t layer = biton32(layer_state);
  static bool in_layer = false;
  if (layer && !in_layer) {
    backlight_set(BACKLIGHT_LEVELS);
#ifdef BACKLIGHT_BREATHING
    breathing_enable();
#endif
    in_layer = true;
  } else if (!layer && in_layer) {
#ifdef BACKLIGHT_BREATHING
    breathing_disable();
#endif
    backlight_set(0);
    in_layer = false;
  }
#endif
}

// Using F(n) causes the firmware to lookup what to do from this table
const uint16_t PROGMEM fn_actions[] = {
  // See also TAPPING_TOGGLE in our config.h
  [FN_MDIA_TOG] = ACTION_LAYER_TAP_TOGGLE(MDIA),
};
