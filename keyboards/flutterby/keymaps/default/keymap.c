#include "flutterby.h"
#include "action_layer.h"
#include "pincontrol.h"
#ifdef MOUSEKEY_ENABLE
#include "mousekey.h"
#endif

// Each layer gets a name for readability, which is then used in the keymap matrix below.
enum layer_id {
  BASE = 0,
  RAISE,
  LOWER,
};

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
  FNTHUMBSTICKMODE,
};

static bool is_mac = false;

#define ____ KC_TRNS
#define CTL(k)  ACTION_MODS_KEY(MOD_LCTL, k)

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
[BASE] = KEYMAP(
           KC_1, KC_2, KC_3, KC_4, KC_5, F(FNCOPYCUT),
  KC_TAB,   KC_Q, KC_W, KC_E, KC_R, KC_T, KC_LBRC,
  CTL_T(KC_ESC),  KC_A, KC_S, KC_D, KC_F, KC_G, KC_MINS,
  KC_LSFT, KC_Z, KC_X, KC_C, KC_V, KC_B,
                                                    KC_LGUI,
                                         KC_LCTRL,  MO(LOWER),
                                         KC_LALT,   KC_BSPC,

  M(MPASTE), KC_6, KC_7, KC_8,    KC_9,   KC_0,
  KC_RBRC,   KC_Y, KC_U, KC_I,    KC_O,   KC_P,    KC_BSLS,
  KC_EQL,    KC_H, KC_J, KC_K,    KC_L,   KC_SCLN, KC_QUOT,
             KC_N, KC_M, KC_COMM, KC_DOT, KC_SLSH, KC_RSFT,
  KC_RGUI,
  KC_RALT,   KC_RCTRL,
  KC_ENT,    KC_SPC
),

[RAISE] = KEYMAP(
          KC_F1,     KC_F2,     KC_F3,     KC_F4,     KC_F5,     ____,
  ____,   CTL(KC_Q), CTL(KC_W), CTL(KC_E), CTL(KC_R), CTL(KC_T), ____,
  ____,   CTL(KC_A), CTL(KC_S), CTL(KC_D), CTL(KC_F), CTL(KC_G), KC_VOLD,
  ____,   CTL(KC_Z), CTL(KC_X), CTL(KC_C), CTL(KC_V), CTL(KC_B),
                                                 ____,
                                          ____,  ____,
                                          ____,  ____,

  ____,    KC_F6,   KC_F7,   KC_F8,   KC_F9,    KC_F10,
  ____,    ____,    KC_PGUP, ____,    ____,     ____,  F(FNOSTOGGLE),
  KC_VOLU, KC_LEFT, KC_DOWN, KC_UP,   KC_RIGHT, ____,  KC_GRV,
           ____,    ____,    KC_MPRV, KC_MNXT,  KC_MPLY,  ____,
  ____,
  ____,    ____,
  ____,    ____
  ),

[LOWER] = KEYMAP(
          KC_F1,   KC_F2, KC_F3,   KC_F4,   KC_F5,   ____,
  ____,   F(FNTHUMBSTICKMODE),    ____,  KC_END,  ____,    ____,    ____,
  ____,   KC_HOME, ____,  KC_PGDN, KC_BTN1, KC_BTN2, KC_F14,
  KC_GRV, RESET,   ____,  ____,    ____,    ____,
                                                     ____,
                                            ____,    ____,
                                            ____,    ____,

  ____,    KC_F6,   KC_F7,   KC_F8,   KC_F9,    KC_F10,
  ____,    ____,    KC_PGUP, ____,    ____,     ____,   ____,
  KC_F15,  KC_LEFT, KC_DOWN, KC_UP,   KC_RIGHT, ____,   KC_GRV,
           ____,    ____,    KC_MPRV, KC_MNXT,  KC_MPLY,   ____,
  ____,
  KC_PGUP, ____,
  KC_PGDN, ____
),

};

void matrix_init_user(void) {
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
  [FNTHUMBSTICKMODE] = ACTION_FUNCTION(FNTHUMBSTICKMODE),
};

void action_function(keyrecord_t *record, uint8_t id, uint8_t opt) {
  switch (id) {
    // The OS-Toggle function toggles our concept of mac or windows
    case FNOSTOGGLE:
      if (IS_RELEASED(record->event)) {
        is_mac = !is_mac;
        flutterby_blink_led(is_mac ? 3 : 1);
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

    case FNTHUMBSTICKMODE:
      if (IS_RELEASED(record->event)) {
        switch (flutterby_thumbstick_get_mode()){
          case ThumbStickMovesPointer:
            flutterby_thumbstick_set_mode(ThumbStickPanning);
            flutterby_blink_led(2);
            break;
          case ThumbStickPanning:
            flutterby_thumbstick_set_mode(ThumbStickMovesPointer);
            flutterby_blink_led(4);
            break;
          default:
            flutterby_blink_led(7);
            break;
        }
      }
      return;
  }
}

void matrix_scan_user(void) {
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
  return true;
}

void led_set_user(uint8_t usb_led) {
}
