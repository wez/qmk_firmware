#pragma once
// Some helpers for controlling gpio pins
#include <avr/io.h>

enum {
  PinDirectionInput = 0,
  PinDirectionOutput = 1,
  PinLevelHigh = 1,
  PinLevelLow = 0,
};

static inline void pinMode(uint8_t pin, int mode) {
  uint8_t bv = _BV(pin & 0xf);
  if (mode == PinDirectionOutput) {
    _SFR_IO8((pin >> 4) + 1) |= bv;
  } else {
    _SFR_IO8((pin >> 4) + 1) &= ~bv;
  }
}

static inline void digitalWrite(uint8_t pin, int mode) {
  uint8_t bv = _BV(pin & 0xf);
  if (mode == PinLevelHigh) {
    _SFR_IO8((pin >> 4) + 2) |= bv;
  } else {
    _SFR_IO8((pin >> 4) + 2) &= ~bv;
  }
}

static inline bool digitalRead(uint8_t pin) {
  return _SFR_IO8(pin >> 4) & _BV(pin & 0xf);
}
