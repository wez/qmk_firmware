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


// Controls the MCP23017 16 pin I/O expander
static bool initialized;
static uint8_t reinit_counter;

#define i2cAddress 0x27 // Configurable with jumpers
#define i2cTimeout 200 // milliseconds
enum mcp23017_registers {
	IODirectionA = 0x00,
  IODirectionB = 0x01,
  InputPolarityA = 0x02,
  InputPolarityB = 0x03,
  InterruptOnChangeA = 0x04,
  InterruptOnChangeB = 0x05,
  DefaultValueA = 0x06,
  DefaultValueB = 0x07,
  InterruptControlA = 0x08,
  InterruptControlB = 0x09,
  IOConfigurationA = 0x0a,
  IOConfigurationB = 0x0b,
  PullUpA = 0x0c,
  PullUpB = 0x0d,
  InterruptFlagA = 0x0e,
  InterruptFlagB = 0x0f,
  InterruptCaptureA = 0x10,
  InterruptCaptureB = 0x11,
  IOPortA = 0x12,
  IOPortB = 0x13,
  OutputLatchA = 0x14,
  OutputLatchB = 0x15,
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

static inline bool _set_register(enum mcp23017_registers reg, unsigned char val) {
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

bool iota_mcp23017_enable_interrupts(void) {
  bool success = false;
  // Configure interrupt pins to mirror each other and OR
  // the interrupts from both ports.
  set_reg(IOConfigurationA, 0b01000000);
  set_reg(IOConfigurationB, 0b01000000);

  // We want interrupts to fire when the buttons toggle
  set_reg(InterruptControlA, 0xff);
  set_reg(InterruptControlB, 0xff);

  // And enable interrupts
  set_reg(InterruptOnChangeA, 0xfe); // Note: A0 is floating
  set_reg(InterruptOnChangeB, 0xff);

  success = true;
done:
  return success;
}

bool iota_mcp23017_init(void) {
  initialized = false;

  // Set all the pins as inputs
  set_reg(IODirectionA, 0xff);
  set_reg(IODirectionB, 0xff);

  // Read key presses (logic low) as 0s
  set_reg(InputPolarityB, 0x00);
  set_reg(InputPolarityA, 0x00);

  // Turn on internal pull-ups; we're adding our own
  set_reg(PullUpA, 0xff);
  set_reg(PullUpB, 0xff);

  // Disable interrupts
  set_reg(InterruptOnChangeA, 0x0);
  set_reg(InterruptOnChangeB, 0x0);

  initialized = true;
done:
  if (!initialized) {
    dprint("failed to init mcp\n");
  } else {
    dprint("mcp initialized!\n");
  }
  return initialized;
}

bool iota_mcp23017_make_ready(void) {
  if (initialized) {
    return true;
  }
  // This will roll over 1 every 255 matrix scans
  if (reinit_counter++ != 0) {
    return false;
  }
  return iota_mcp23017_init();
}

// Read all 16 inputs and return them
uint16_t iota_mcp23017_read(void) {
  uint16_t pins = 0;

  if (!initialized) {
    return 0;
  }

#ifdef USE_LUFA_TWI
  uint8_t addr = IOPortA;
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

  if (i2c_write(IOPortA)) {
    initialized = false;
    goto done;
  }

  if (i2c_start_read(i2cAddress)) {
    initialized = false;
    goto done;
  }

  // Read PortA
  pins = i2c_readAck() << 8;
  // Read PortB
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
