#include "config.h"
#include "i2cmaster.h"
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "print.h"
#include "iota.h"
#include "../common/glcdfont.c"
#ifdef BLE_ENABLE
#include "ble.h"
#endif
#ifdef PROTOCOL_LUFA
#include "lufa.h"
#endif

// Controls the SSD1306 128x32 OLED display via i2c

#define i2cAddress 0x3C

#define DisplayHeight 32
#define DisplayWidth 128

#define FontHeight 8
#define FontWidth 6

#define MatrixRows (DisplayHeight / FontHeight)
#define MatrixCols (DisplayWidth / FontWidth)

static uint8_t display[MatrixRows][MatrixCols];
static uint8_t *cursor;
static bool dirty;

enum ssd1306_cmds {
  DisplayOff = 0xae,
  DisplayOn = 0xaf,

  SetContrast = 0x81,
  DisplayAllOnResume = 0xA4,

  DisplayAllOn = 0xA5,
  NormalDisplay = 0xA6,
  InvertDisplay = 0xA7,
  SetDisplayOffset = 0xd3,
  SetComPins = 0xda,
  SetVComDetect = 0xdb,
  SetDisplayClockDiv = 0xd5,
  SetPreCharge = 0xd9,
  SetMultiPlex = 0xa8,
  SetLowColumn = 0x00,
  SetHighColumn = 0x10,
  SetStartLine = 0x40,

  SetMemoryMode = 0x20,
  ColumnAddr = 0x21,
  PageAddr = 0x22,

  ComScanInc = 0xc0,
  ComScanDec = 0xc8,
  SegRemap = 0xa0,
  SetChargePump = 0x8d,
  ExternalVcc = 0x01,
  SwitchCapVcc = 0x02,

  ActivateScroll = 0x2f,
  DeActivateScroll = 0x2e,
  SetVerticalScrollArea = 0xa3,
  RightHorizontalScroll = 0x26,
  LeftHorizontalScroll = 0x27,
  VerticalAndRightHorizontalScroll = 0x29,
  VerticalAndLeftHorizontalScroll = 0x2a,
};

// Write command sequence.
// Returns true on success.
static inline bool _send_cmd1(uint8_t cmd) {
  bool res = false;

  if (i2c_start_write(i2cAddress)) {
    xprintf("failed to start write to %d\n", i2cAddress);
    goto done;
  }

  if (i2c_write(0x0 /* command byte follows */)) {
    print("failed to write control byte\n");

    goto done;
  }

  if (i2c_write(cmd)) {
    xprintf("failed to write command %d\n", cmd);
    goto done;
  }
  res = true;
done:
  i2c_stop();
  return res;
}

// Write 2-byte command sequence.
// Returns true on success
static inline bool _send_cmd2(uint8_t cmd, uint8_t opr) {
  if (!_send_cmd1(cmd)) {
    return false;
  }
  return _send_cmd1(opr);
}

// Write 3-byte command sequence.
// Returns true on success
static inline bool _send_cmd3(uint8_t cmd, uint8_t opr1, uint8_t opr2) {
  if (!_send_cmd1(cmd)) {
    return false;
  }
  if (!_send_cmd1(opr1)) {
    return false;
  }
  return _send_cmd1(opr2);
}

#define send_cmd1(c) if (!_send_cmd1(c)) {goto done;}
#define send_cmd2(c,o) if (!_send_cmd2(c,o)) {goto done;}
#define send_cmd3(c,o1,o2) if (!_send_cmd3(c,o1,o2)) {goto done;}

static void clear_display(void) {
  memset(display, ' ', sizeof(display));
  cursor = &display[0][0];

  // Clear all of the display bits (there can be random noise
  // in the RAM on startup)
  send_cmd3(PageAddr, 0, (DisplayHeight / 8) - 1);
  send_cmd3(ColumnAddr, 0, DisplayWidth - 1);

  if (i2c_start_write(i2cAddress)) {
    goto done;
  }
  if (i2c_write(0x40)) {
    // Data mode
    goto done;
  }
  for (uint8_t row = 0; row < MatrixRows; ++row) {
    for (uint8_t col = 0; col < DisplayWidth; ++col) {
      i2c_write(0);
    }
  }

  dirty = false;

done:
  i2c_stop();
}

bool iota_gfx_init(void) {
  bool success = false;

  send_cmd1(DisplayOff);
  send_cmd2(SetDisplayClockDiv, 0x80);
  send_cmd2(SetMultiPlex, DisplayHeight - 1);

  send_cmd2(SetDisplayOffset, 0);

  send_cmd1(SetStartLine | 0x0);
  send_cmd2(SetChargePump, 0x14 /* Enable */);
  send_cmd2(SetMemoryMode, 0 /* horizontal addressing */);
  send_cmd1(SegRemap | 0x1); // Flip the display orientation
  send_cmd1(ComScanDec);
  send_cmd2(SetComPins, 0x2);
  send_cmd2(SetContrast, 0x8f);
  send_cmd2(SetPreCharge, 0xf1);
  send_cmd2(SetVComDetect, 0x40);
  send_cmd1(DisplayAllOnResume);
  send_cmd1(NormalDisplay);
  send_cmd1(DeActivateScroll);
  send_cmd1(DisplayOn);

  send_cmd2(SetContrast, 0); // Dim

  clear_display();

  success = true;

  iota_gfx_write_P(PSTR(STR(PRODUCT) " " STR(DESCRIPTION)));
  iota_gfx_flush();

done:
  return success;
}

bool iota_gfx_off(void) {
  bool success = false;

  send_cmd1(DisplayOff);
  success = true;

done:
  return success;
}

bool iota_gfx_on(void) {
  bool success = false;

  send_cmd1(DisplayOn);
  success = true;

done:
  return success;
}


static inline void write_char(uint8_t c) {
  *cursor = c;
  ++cursor;

  if (cursor - &display[0][0] == sizeof(display)) {
    // We went off the end; scroll the display upwards by one line
    memmove(&display[0], &display[1], MatrixCols * (MatrixRows - 1));
    cursor = &display[MatrixRows - 1][0];
    memset(cursor, ' ', MatrixCols);
  }
}

void iota_gfx_write_char(uint8_t c) {
  dirty = true;

  if (c == '\n') {
    // Clear to end of line from the cursor and then move to the
    // start of the next line
    uint8_t cursor_col = (cursor - &display[0][0]) % MatrixCols;

    while (cursor_col++ < MatrixCols) {
      write_char(' ');
    }
    return;
  }

  write_char(c);
}

void iota_gfx_write(const char *data) {
  const char *end = data + strlen(data);
  while (data < end) {
    iota_gfx_write_char(*data);
    ++data;
  }
}

void iota_gfx_write_P(const char *data) {
  while (true) {
    uint8_t c = pgm_read_byte(data);
    if (c == 0) {
      return;
    }
    iota_gfx_write_char(c);
    ++data;
  }
}

void iota_gfx_clear_screen(void) {
  memset(&display[0][0], ' ', sizeof(display));
  cursor = &display[0][0];
  dirty = true;
}

void iota_gfx_flush(void) {
  // Move to the home position
  send_cmd3(PageAddr, 0, MatrixRows - 1);
  send_cmd3(ColumnAddr, 0, (MatrixCols * FontWidth) - 1);

  if (i2c_start_write(i2cAddress)) {
    goto done;
  }
  if (i2c_write(0x40)) {
    // Data mode
    goto done;
  }

  for (uint8_t row = 0; row < MatrixRows; ++row) {
    for (uint8_t col = 0; col < MatrixCols; ++col) {
      const uint8_t *glyph = font + (display[row][col] * (FontWidth - 1));

      for (uint8_t glyphCol = 0; glyphCol < FontWidth - 1; ++glyphCol) {
        uint8_t colBits = pgm_read_byte(glyph + glyphCol);
        i2c_write(colBits);
      }

      // 1 column of space between chars (it's not included in the glyph)
      i2c_write(0);
    }
  }

  dirty = false;

done:
  i2c_stop();
}

void iota_gfx_task(void) {
  iota_gfx_clear_screen();
  iota_gfx_write_P(PSTR("USB: "));
#ifdef PROTOCOL_LUFA
  switch (USB_DeviceState) {
    case DEVICE_STATE_Unattached:
      iota_gfx_write_P(PSTR("Unattached"));
      break;
    case DEVICE_STATE_Suspended:
      iota_gfx_write_P(PSTR("Suspended"));
      break;
    case DEVICE_STATE_Configured:
      iota_gfx_write_P(PSTR("Configured"));
      break;
    case DEVICE_STATE_Powered:
      iota_gfx_write_P(PSTR("Powered"));
      break;
    case DEVICE_STATE_Default:
      iota_gfx_write_P(PSTR("Default"));
      break;
    case DEVICE_STATE_Addressed:
      iota_gfx_write_P(PSTR("Addressed"));
      break;
    default:
      iota_gfx_write_P(PSTR("Invalid"));
  }
#endif
  iota_gfx_write_P(PSTR("\nBLE: "));
#ifdef BLE_ENABLE
  iota_gfx_write_P(ble_is_connected() ? PSTR("Connected")
                                      : PSTR("Not Connected"));
#endif
  iota_gfx_write_P(PSTR("\n"));

  char buf[40];
  snprintf(buf, sizeof(buf), "Mod 0x%02x VBat: %4lumVLayer: 0x%04lx", get_mods(),
#ifdef BLE_ENABLE
           ble_read_battery_voltage(),
#else
           0LU,
#endif
           layer_state);
  iota_gfx_write(buf);


  if (dirty) {
    iota_gfx_flush();
  }
}
