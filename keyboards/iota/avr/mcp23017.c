#include "config.h"
#include "i2cmaster.h"
#include <stdbool.h>
#include "pincontrol.h"

// Controls the MCP23017 16 pin I/O expander
static bool initialized;

#define i2cAddress 0x27 // Configurable with jumpers
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

static inline bool _set_register(enum mcp23017_registers reg, unsigned char val) {
  bool success = false;

  if (i2c_start_write(i2cAddress)) {
    goto done;
  }

  if (i2c_write((unsigned char)reg)) {
    goto done;
  }

  success = i2c_write(val) == 0;
done:
  i2c_stop();
  return success;
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
  set_reg(InterruptOnChangeA, 0xff);
  set_reg(InterruptOnChangeB, 0xfe); // Note: B0 is floating

  success = true;
done:
  return success;
}

bool iota_mcp23017_init(void) {
  initialized = false;

  // Set all the pins as inputs
  set_reg(IODirectionA, 0xff);
  set_reg(IODirectionB, 0xff);

  // Read key presses (logic low) as 1s
  set_reg(InputPolarityB, 0xff);
  set_reg(InputPolarityA, 0xff);

  // Turn off internal pull-ups; we're adding our own
  set_reg(PullUpA, 0xff);
  set_reg(PullUpB, 0xff);

  // Disable interrupts
  set_reg(InterruptOnChangeA, 0x0);
  set_reg(InterruptOnChangeB, 0x0);

  initialized = true;
done:
  return initialized;
}

bool iota_mcp23017_make_ready(void) {
  if (initialized) {
    return true;
  }
  return iota_mcp23017_init();
}

// Read all 16 inputs and return them
uint16_t iota_mcp23017_read(void) {
  uint16_t pins = 0;

  if (!initialized) {
    return false;
  }

  if (i2c_start_write(i2cAddress)) {
    goto done;
  }

  if (i2c_write(IOPortA)) {
    goto done;
  }

  if (i2c_start_read(i2cAddress)) {
    goto done;
  }

  // Read PortA
  pins = i2c_readAck() << 8;
  // Read PortB
  pins |= i2c_readNak();

done:
  i2c_stop();

  return pins;
}
