// This is my layout, influenced by the Kinesis Advantage layout
// See ../../README.md for info about the various modifier macros that are available
#include "ergodox.h"
#include "debug.h"
#include "action_layer.h"
#include "mousekey.h"

enum layer_id {
  BASE = 0, // default layer
  MDIA,     // media keys
};

#define _______ KC_TRNS
#define XXX     KC_NO

// Macro ids for use with M(n)
enum macro_id {
  MCOPY = 1,
  MCUT,
  MPASTE,
};

// Function ids for use with F(n)
enum function_id {
  FNCOPYCUT,
  FNOSTOGGLE,
};

// Tap dance function ids
enum tap_action_id {
  TD_SFT_CAPS,
};

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
/* Basic layer
 *
 * ,--------------------------------------------------.           ,--------------------------------------------------.
 * |   =    |   1  |   2  |   3  |   4  |   5  | Copy |           | Paste|   6  |   7  |   8  |   9  |   0  |   -    |
 * |--------+------+------+------+------+-------------|           |------+------+------+------+------+------+--------|
 * | Tab    |   Q  |   W  |   E  |   R  |   T  |      |           |      |   Y  |   U  |   I  |   O  |   P  |   \    |
 * |--------+------+------+------+------+------|      |           |      |------+------+------+------+------+--------|
 * |Esc/Ctrl|   A  |   S  |   D  |   F  |   G  |------|           |------|   H  |   J  |   K  |   L  |;/MDIA|  '"    |
 * |--------+------+------+------+------+------| MDIA |           | Ctrl |------+------+------+------+------+--------|
 * |Shft/Caps|  Z  |   X  |   C  |   V  |   B  |      |           |      |   N  |   M  |   ,  |   .  |  Up  | RShift |
 * `--------+------+------+------+------+-------------'           `-------------+------+------+------+------+--------'
 *   | Grv  | MDIA | LGui |      |   /  |                                       |   [  |   ]  | Left | Down | Right|
 *   `----------------------------------'                                       `----------------------------------'
 *                                        ,-------------.       ,-------------.
 *                                        | Alt  | LGui |       | LGui | Alt  |
 *                                 ,------|------|------|       |------+--------+------.
 *                                 |      |      | Home |       | PgUp |        |      |
 *                                 |Backsp| Del  |------|       |------|  Enter |Space |
 *                                 |      |      | End  |       | PgDn |        |      |
 *                                 `--------------------'       `----------------------'
 */
[BASE] = KEYMAP(
        // left hand
        KC_EQL,         KC_1,         KC_2,   KC_3,   KC_4,   KC_5,   F(FNCOPYCUT),
        KC_TAB,         KC_Q,         KC_W,   KC_E,   KC_R,   KC_T,   XXX,
        CTL_T(KC_ESC),  KC_A,         KC_S,   KC_D,   KC_F,   KC_G,
        TD(TD_SFT_CAPS),KC_Z,         KC_X,   KC_C,   KC_V,   KC_B,   MO(MDIA),
        KC_GRV,         MO(MDIA),     KC_LGUI,XXX,    KC_SLSH,
                                                      KC_LALT,KC_LGUI,
                                                              KC_HOME,
                                               KC_BSPC,KC_DEL,KC_END,
        // right hand
             M(MPASTE),   KC_6,   KC_7,   KC_8,   KC_9,   KC_0,             KC_MINS,
             XXX,         KC_Y,   KC_U,   KC_I,   KC_O,   KC_P,             KC_BSLS,
                          KC_H,   KC_J,   KC_K,   KC_L,   LT(MDIA, KC_SCLN),KC_QUOT,
             KC_RCTL,     KC_N,   KC_M,   KC_COMM,KC_DOT, KC_UP,            KC_RSFT,
                                  KC_LBRC,KC_RBRC,KC_LEFT,KC_DOWN,          KC_RIGHT,
             KC_LGUI, KC_RALT,
             KC_PGUP,
             KC_PGDN, KC_ENT, KC_SPC
    ),
/* Media and mouse keys
 *
 * ,--------------------------------------------------.           ,--------------------------------------------------.
 * |        |  F1  |  F2  |  F3  |  F4  |  F5  |      |           |      |  F6  |  F7  |  F8  |  F9  |  F10 | Bright+|
 * |--------+------+------+------+------+-------------|           |------+------+------+------+------+------+--------|
 * |        |      |      | MsUp |      |ScrlDn|      |           |      |      |      | Acc0 | Acc1 | Acc2 | Bright-|
 * |--------+------+------+------+------+------|      |           |      |------+------+------+------+------+--------|
 * |        |      |MsLeft|MsDown|MsRght|ScrlUp|------|           |------|      |      | Lclk | Rclk |      |  Play  |
 * |--------+------+------+------+------+------|      |           |      |------+------+------+------+------+--------|
 * |        |      |      |      |      |      |      |           |      |      |      | Prev | Next | VolUp|        |
 * `--------+------+------+------+------+-------------'           `-------------+------+------+------+------+--------'
 *   |OSTOGL|      |      |      |      |                                       |      |      | Mute | VolDn|      |
 *   `----------------------------------'                                       `----------------------------------'
 *                                        ,-------------.       ,-------------.
 *                                        |      |      |       |      |      |
 *                                 ,------|------|------|       |------+------+------.
 *                                 |      |      |      |       |      |      |      |
 *                                 |      |      |------|       |------|      |      |
 *                                 |      |      |      |       |      |      |      |
 *                                 `--------------------'       `--------------------'
 */
// MEDIA AND MOUSE
[MDIA] = KEYMAP(
       // left hand
       _______,      KC_F1,   KC_F2,   KC_F3,   KC_F4,   KC_F5,         _______,
       _______,      _______, _______, KC_MS_U, _______, KC_MS_WH_DOWN, _______,
       _______,      _______, KC_MS_L, KC_MS_D, KC_MS_R, KC_MS_WH_UP,
       _______,      _______, _______, _______, _______, _______,       _______,
       F(FNOSTOGGLE),_______, _______, _______, _______,
                                           _______, _______,
                                                    _______,
                                  _______, _______, _______,
    // right hand
       _______, KC_F6,    KC_F7,   KC_F8,        KC_F9,        KC_F10,       KC_F15,
       _______,  _______, _______, KC_MS_ACCEL0, KC_MS_ACCEL1, KC_MS_ACCEL2, KC_F14,
                 _______, _______, KC_BTN1,      KC_BTN2,      _______,      KC_MPLY,
       _______,  _______, _______, KC_MPRV,      KC_MNXT,      KC_VOLU,      _______,
                          _______, _______,      KC_MUTE,      KC_VOLD,      _______,
       _______, _______,
       _______,
       _______, _______, _______
),
};

// I mostly use macs, so default to mac mode
static bool is_mac = true;

static void blink_led(uint8_t led) {
  ergodox_led_all_off();
  ergodox_right_led_set(led, LED_BRIGHTNESS_HI);

  ergodox_right_led_on(led);
  _delay_ms(150);
  ergodox_right_led_off(led);

  _delay_ms(50);

  ergodox_right_led_on(led);
  _delay_ms(150);
  ergodox_right_led_off(led);

  _delay_ms(50);

  ergodox_right_led_on(led);
  _delay_ms(150);
  ergodox_right_led_off(led);
}

static const macro_t mac_cut[] PROGMEM = { D(LGUI), T(X), U(LGUI), END };
static const macro_t win_cut[] PROGMEM = { D(LSFT), T(DELT), U(LSFT), END };

static const macro_t mac_copy[] PROGMEM = { D(LGUI), T(C), U(LGUI), END };
static const macro_t win_copy[] PROGMEM = { D(LCTL), T(INS), U(LCTL), END };

static const macro_t mac_paste[] PROGMEM = { D(LGUI), T(V), U(LGUI), END };
static const macro_t win_paste[] PROGMEM = { D(LSFT), T(INS), U(LSFT), END };

const macro_t *action_get_macro(keyrecord_t *record, uint8_t id, uint8_t opt) {
  if (!record->event.pressed) {
    return MACRO_NONE;
  }

  switch (id) {
  case MCUT:
    return is_mac ? mac_cut : win_cut;
  case MCOPY:
    return is_mac ? mac_copy : win_copy;
  case MPASTE:
    return is_mac ? mac_paste : win_paste;
  default:
    return MACRO_NONE;
  }
}

// Using F(n) causes the firmware to lookup what to do from this table
const uint16_t PROGMEM fn_actions[] = {
  [FNCOPYCUT] = ACTION_FUNCTION(FNCOPYCUT),
  [FNOSTOGGLE] = ACTION_FUNCTION(FNOSTOGGLE),
};

void action_function(keyrecord_t *record, uint8_t id, uint8_t opt) {
  switch (id) {
    // The OS-Toggle function toggles our concept of mac or windows
    case FNOSTOGGLE:
      if (IS_RELEASED(record->event)) {
        is_mac = !is_mac;
        // Blink blue for mac, red otherwise
        blink_led(is_mac ? 3 : 1);
      }
      return;

    // The copy-cut function sends the copy key sequence for mac or windows
    // when it is pressed.  If shift is held down, it will send the cut key
    // sequence instead, and cancels the shift modifier.
    case FNCOPYCUT:
      if (IS_RELEASED(record->event)) {
        int8_t shifted = get_mods() & (MOD_BIT(KC_LSHIFT) | MOD_BIT(KC_RSHIFT));

        // Implicitly release the shift key so that it doesn't
        // mess with the macro that we play back
        unregister_mods(shifted);

        if (shifted) {
          action_macro_play(is_mac ? mac_cut : win_cut);
        } else {
          action_macro_play(is_mac ? mac_copy : win_copy);
        }
      }
      return;
  }
}

// Using TD(n) causes the firmware to lookup the tapping action here
qk_tap_dance_action_t tap_dance_actions[] = {
  // Double tap shift to turn on caps lock
  [TD_SFT_CAPS] = ACTION_TAP_DANCE_DOUBLE(KC_LSFT, KC_CAPS),
};

// Runs just one time when the keyboard initializes.
void matrix_init_user(void) {
  // AFAICT, we don't have one of these
  ergodox_board_led_off();
  // mousekey: A bit faster by default, use accel keys for fine control
  mk_max_speed = 6;
  // Slightly slower mouse wheel speed than the default
  mk_wheel_max_speed = 4;
}

// Runs constantly in the background, in a loop.
void matrix_scan_user(void) {
  uint8_t layer = biton32(layer_state);

  // Dim the LEDs as much as possible
  ergodox_led_all_set(0);

  // Show the active layer number as binary bits in the LEDs.
  // Note that LED1 is left-most, so bit1 -> LED3 and bit3 -> LED1
  if (layer & 0b001) {
    ergodox_right_led_3_on();
  } else {
    ergodox_right_led_3_off();
  }
  if (layer & 0b010) {
    ergodox_right_led_2_on();
  } else {
    ergodox_right_led_2_off();
  }

  // Show caps lock on the left most LED.
  // (Double-tap left shift to toggle caps lock)
  if (host_keyboard_leds() & (1 << USB_LED_CAPS_LOCK)) {
    ergodox_right_led_1_on();
  } else {
    ergodox_right_led_1_off();
  }
}
