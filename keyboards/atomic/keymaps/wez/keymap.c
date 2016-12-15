#include "atomic.h"
#include "action_layer.h"
#ifdef MOUSEKEY_ENABLE
#include "mousekey.h"
#endif

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
  FN_MDIA_TOG,
};

#define ____ KC_TRNS

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
/* Keymap BASE: (Base Layer) Default Layer
 * .--------------------------------------------------------------------------------------------------------------------------------------.
 * | `      | 1      | 2      | 3      | 4      | 5      | COPY   |        | PASTE  | 6      | 7      | 8      | 9      | 0      | -      |
 * |--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+-----------------|
 * | TAB    | Q      | W      | E      | R      | T      | [      |        | ]      | Y      | U      | I      | O      | P      | \      |
 * |--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+-----------------+--------|
 * | CTL/ESC| A      | S      | D      | F      | G      | VOLUP  |        | PGUP   | H      | J      | K      | L      |  ;     | '      |
 * |--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------------------------+--------|
 * | SFT/CAP| Z      | X      | C      | V      | B      | VOLDN  | ALT    | PGDN   | N      | M      | ,      | .      | /      | RSHIFT |
 * |--------+--------+--------+--------+--------+-----------------+--------+--------+--------+--------+-----------------+--------+--------|
 * | `      | MDIA   | LALT   | LGUI   | BKSPC  | DEL    | CTRL   | LGUI   | ENTER  | SPACE  | LEFT   | DOWN   | UP     | RIGHT  | MDIA   |
 * '--------------------------------------------------------------------------------------------------------------------------------------'
 */
[BASE] = {
  {KC_EQL,        KC_1,    KC_2,   KC_3,   KC_4,   KC_5,   F(FNCOPYCUT), KC_NO,    M(MPASTE),KC_6,   KC_7,   KC_8,   KC_9,    KC_0,    KC_MINS},
  {KC_TAB,        KC_Q,    KC_W,   KC_E,   KC_R,   KC_T,   KC_LBRC,      KC_NO,    KC_RBRC,  KC_Y,   KC_U,   KC_I,   KC_O,    KC_P,    KC_BSLS},
  {CTL_T(KC_ESC), KC_A,    KC_S,   KC_D,   KC_F,   KC_G,   KC_VOLU,      KC_NO,    KC_PGUP,  KC_H,   KC_J,   KC_K,   KC_L,    KC_SCLN, KC_QUOT},
  {KC_LSFT,       KC_Z,    KC_X,   KC_C,   KC_V,   KC_B,   KC_VOLD,      KC_RALT,  KC_PGDN,  KC_N,   KC_M,   KC_COMM,KC_DOT,  KC_SLSH, KC_RSFT},
  {KC_GRV,        MO(MDIA),KC_LALT,KC_LGUI,KC_BSPC,KC_DEL, KC_LCTL,      KC_LGUI,  KC_ENT,   KC_SPC, KC_LEFT,KC_DOWN,KC_UP,   KC_RIGHT,MO(MDIA)},
},
/* Keymap MDIA: Media and mouse keys
 * .--------------------------------------------------------------------------------------------------------------------------------------.
 * |        | F1     | F2     | F3     | F4     | F5     |        |        |        | F6     | F7     | F8     | F9     | F10    |        |
 * |--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+-----------------|
 * |        |        |        |        |        |        |        |        |        |        |        |        |        |        |        |
 * |--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+-----------------+--------|
 * |        |        |        |        |        |        | BrghtUp|        | HOME   |        |        |        |        |        | Play   |
 * |--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------+--------------------------+--------|
 * |        | Reset  |        |        |        |        | BrghtDn|        | END    |        |        | Prev   | Next   |        |        |
 * |--------+--------+--------+--------+--------+-----------------+--------+--------+--------+--------+-----------------+--------+--------|
 * |        |        |        |        |        |        |        |        |        |        |        |        |        |        |        |
 * '--------------------------------------------------------------------------------------------------------------------------------------'
 */
[MDIA] = {
  {____, KC_F1,  KC_F2,   KC_F3,   KC_F4, KC_F5, ____,   ____,  ____,    KC_F6,   KC_F7,   KC_F8,   KC_F9,   KC_F10, ____},
  {____, ____,   ____,    ____,    ____,  ____,  ____,   ____,  ____,    ____,    ____,    ____,    ____,    ____,   ____},
  {____, ____,   ____,    ____,    ____,  ____,  KC_F15, ____,  KC_HOME, ____,    ____,    ____,    ____,    ____,   KC_MPLY},
  {____, RESET,  ____,    ____,    ____,  ____,  KC_F14, ____,  KC_END,  ____,    ____,    KC_MPRV, KC_MNXT, ____,   ____},
  {____, ____,   ____,    ____,    ____,  ____,  ____,   ____,  ____,    ____,    ____,    ____,    ____,    ____,   ____},
},
};

// Using TD(n) causes the firmware to lookup the tapping action here
qk_tap_dance_action_t tap_dance_actions[] = {
  // Double tap shift to turn on caps lock
  [TD_SFT_CAPS] = ACTION_TAP_DANCE_DOUBLE(KC_LSFT, KC_CAPS),
};


// Runs just one time when the keyboard initializes.
void matrix_init_user(void) {
#ifdef MOUSEKEY_ENABLE
  // mousekey: A bit faster by default, use accel keys for fine control
  mk_max_speed = 10;
  // accelerate a bit faster than usual
  mk_time_to_max = 15;
  // Slightly slower mouse wheel speed than the default
  mk_wheel_max_speed = 4;
#endif
}

// Runs constantly in the background, in a loop.
void matrix_scan_user(void) {
}

static const macro_t mac_cut[] PROGMEM = { D(LGUI), T(X), U(LGUI), END };
static const macro_t win_cut[] PROGMEM = { D(LSFT), T(DELT), U(LSFT), END };

static const macro_t mac_copy[] PROGMEM = { D(LGUI), T(C), U(LGUI), END };
static const macro_t win_copy[] PROGMEM = { D(LCTL), T(INS), U(LCTL), END };

static const macro_t mac_paste[] PROGMEM = { D(LGUI), T(V), U(LGUI), END };
static const macro_t win_paste[] PROGMEM = { D(LSFT), T(INS), U(LSFT), END };

// I mostly use macs, so default to mac mode
static bool is_mac = true;

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

void action_function(keyrecord_t *record, uint8_t id, uint8_t opt) {
  switch (id) {
    // The OS-Toggle function toggles our concept of mac or windows
    case FNOSTOGGLE:
      if (IS_RELEASED(record->event)) {
        is_mac = !is_mac;
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



// Using F(n) causes the firmware to lookup what to do from this table
const uint16_t PROGMEM fn_actions[] = {
  // See also TAPPING_TOGGLE in our config.h
  [FN_MDIA_TOG] = ACTION_LAYER_TAP_TOGGLE(MDIA),
  [FNCOPYCUT] = ACTION_FUNCTION(FNCOPYCUT),
  [FNOSTOGGLE] = ACTION_FUNCTION(FNOSTOGGLE),
};
