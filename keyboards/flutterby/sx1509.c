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
#include "config.h"
#include <stdbool.h>
#include "pincontrol.h"
#include "debug.h"
#define USE_LUFA_TWI

#ifdef USE_LUFA_TWI
#include "tmk_core/protocol/lufa/LUFA-git/LUFA/Drivers/Peripheral/TWI.h"
#include "tmk_core/protocol/lufa/LUFA-git/LUFA/Drivers/Peripheral/AVR8/TWI_AVR8.c"
#else
#include "i2cmaster.h"
#endif


// Controls the SX1509 16 pin I/O expander
static bool initialized;
static uint8_t reinit_counter;

#define i2cAddress 0x3e // Configurable with jumpers
#define i2cTimeout 200 // milliseconds
enum sx1509_registers {
  RegReset = 0x7d,
	RegDirA = 0x0f,
  RegDirB = 0x0e,
  RegPullUpA = 0x07,
  RegPullUpB = 0x06,
  DataA = 0x11,
  DataB = 0x10,
};
#define MCP23017_INT_ERR 255

#ifdef USE_LUFA_TWI
static const char *twi_err_str(uint8_t res) {
  switch (res) {
    case TWI_ERROR_NoError: return "OK";
    case TWI_ERROR_BusFault: return "BUSFAULT";
    case TWI_ERROR_BusCaptureTimeout: return "BUSTIMEOUT";
    case TWI_ERROR_SlaveResponseTimeout: return "SLAVETIMEOUT";
    case TWI_ERROR_SlaveNotReady: return "SLAVENOTREADY";
    case TWI_ERROR_SlaveNAK: return "SLAVENAK";
    default: return "UNKNOWN";
  }
}
#endif

static inline bool _set_register(enum sx1509_registers reg, unsigned char val) {
#ifdef USE_LUFA_TWI
  uint8_t addr = reg;
  uint8_t result = TWI_WritePacket(i2cAddress << 1, i2cTimeout, &addr, sizeof(addr),
                                   &val, sizeof(val));
  if (result) {
    xprintf("mcp: set_register %d = %d failed: %s\n", reg, val, twi_err_str(result));
  }
  return result == 0;
#else
  bool success = false;
  if (i2c_start_write(i2cAddress)) {
    xprintf("mcp: start_write failed\n");
    goto done;
  }

  if (i2c_write((unsigned char)reg)) {
    xprintf("mcp: write reg addr %d failed\n", reg);
    goto done;
  }

  success = i2c_write(val) == 0;
  if (!success) {
    xprintf("mcp: write reg addr %d val = %d failed\n", reg, val);
  }
done:
  i2c_stop();
  return success;
#endif
}
#define set_reg(reg, val) if (!_set_register(reg, val)) { goto done; }

bool sx1509_init(void) {
  initialized = false;

  // Software reset
  set_reg(RegReset, 0x12);
  set_reg(RegReset, 0x34);

  // Set all the pins as inputs
  set_reg(RegDirA, 0xff);
  set_reg(RegDirB, 0xff);

  // Turn on internal pull-ups
  set_reg(RegPullUpA, 0xff);
  set_reg(RegPullUpB, 0xff);

  initialized = true;
done:
  if (!initialized) {
    dprint("failed to init mcp\n");
  } else {
    dprint("mcp initialized!\n");
  }
  return initialized;
}

bool sx1509_make_ready(void) {
  if (initialized) {
    return true;
  }
  // This will roll over 1 every 255 matrix scans
  if (reinit_counter++ != 0) {
    return false;
  }
  return sx1509_init();
}

// Read all 16 inputs and return them
uint16_t sx1509_read(void) {
  uint16_t pins = 0;

  if (!initialized) {
    return 0;
  }

#ifdef USE_LUFA_TWI
  uint8_t addr = DataB;
  uint8_t buf[2];
  uint8_t result = TWI_ReadPacket(i2cAddress << 1, i2cTimeout, &addr,
                                  sizeof(addr), buf, sizeof(buf));
  if (result) {
    xprintf("mcp: read pins failed: %s\n", twi_err_str(result));
    initialized = false;
    return 0;
  }
  pins = (buf[0] << 8) | buf[1];
  return ~pins;
#else

  if (i2c_start_write(i2cAddress)) {
    initialized = false;
    goto done;
  }

  if (i2c_write(DataB)) {
    initialized = false;
    goto done;
  }

  if (i2c_start_read(i2cAddress)) {
    initialized = false;
    goto done;
  }

  // Read PortB
  pins = i2c_readAck() << 8;
  // Read PortA
  pins |= i2c_readNak();

done:
  i2c_stop();

  if (!initialized) {
    dprint("failed to read mcp, will re-init\n");
    return 0;
  }
#endif

  return ~pins;
}
