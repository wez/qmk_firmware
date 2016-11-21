#include "config.h"
#include "i2cmaster.h"
#include <stdbool.h>

// Controls the SSD1306 128x32 OLED display via i2c

#define i2cAddress 0x3C

#define DisplayHeight 32
#define DisplayWidth 128

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
static inline bool _send_cmd1(unsigned char cmd) {
  if (i2c_write(0x0 /* command byte follows */)) {
    return false;
  }
  return i2c_write(cmd) == 0;
}

// Write 2-byte command sequence.
// Returns true on success
static inline bool _send_cmd2(unsigned char cmd, unsigned char opr) {
  if (!_send_cmd1(cmd)) {
    return false;
  }
  return _send_cmd1(opr);
}

#define send_cmd1(c) if (_send_cmd1(c)) {goto done;}
#define send_cmd2(c,o) if (_send_cmd2(c,o)) {goto done;}

bool iota_gfx_init(void) {
  bool success = false;

  if (i2c_start_write(i2cAddress)) {
    goto done;
  }

  send_cmd1(DisplayOff);
  send_cmd2(SetDisplayClockDiv, 0x80);
  send_cmd2(SetMultiPlex, DisplayHeight - 1);
  send_cmd2(SetDisplayOffset, 0);
  send_cmd1(SetStartLine | 0x0);
  send_cmd2(SetChargePump, 0x14 /* Enable */);
  send_cmd2(SetMemoryMode, 0 /* horizontal addressing */);
  send_cmd1(SegRemap | 0x1);
  send_cmd1(ComScanDec);
  send_cmd2(SetComPins, 0x2);
  send_cmd2(SetContrast, 0x8f);
  send_cmd2(SetPreCharge, 0xf1);
  send_cmd2(SetVComDetect, 0x40);
  send_cmd1(DisplayAllOnResume);
  send_cmd1(NormalDisplay);
  send_cmd1(DeActivateScroll);
  send_cmd1(DisplayOn);

  success = true;
done:
  i2c_stop();
  return success;
}

bool iota_gfx_off(void) {
  bool success = false;

  if (i2c_start_write(i2cAddress)) {
    goto done;
  }

  send_cmd1(DisplayOff);
  success = true;

done:
  i2c_stop();
  return success;
}
