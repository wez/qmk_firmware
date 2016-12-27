#pragma once
#include <util/atomic.h>
#include "pincontrol.h"

class SPI{
  uint8_t spcr_, spsr_;

public:
  SPI(uint32_t busSpeed) {
    // The bits in div are:
    // 0: ~SPI2x
    // 1: SPR0
    // 2: SPR1
    uint8_t div;
    if (busSpeed >= F_CPU / 2) {
      div = 0;
    } else if (busSpeed >= F_CPU / 4) {
      div = 1;
    } else if (busSpeed >= F_CPU / 8) {
      div = 2;
    } else if (busSpeed >= F_CPU / 16) {
      div = 3;
    } else if (busSpeed >= F_CPU / 32) {
      div = 4;
    } else if (busSpeed >= F_CPU / 64) {
      div = 5;
    } else {
      div = 7; // Not 6!
    }

    // Toggle the SPI2x bit
    div ^= 0x1;

    spcr_ = _BV(SPE) | _BV(MSTR) | ((div >> 1) & 0x3);
    spsr_ = div & 0x1; // We only want the SPI2x bit here

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      // Ensure that SS is OUTPUT High
      digitalWrite(B0, PinLevelHigh);
      pinMode(B0, PinDirectionOutput);

      SPCR |= _BV(MSTR);
      SPCR |= _BV(SPE);
      pinMode(B1 /* SCK */, PinDirectionOutput);
      pinMode(B2 /* MOSI */, PinDirectionOutput);
    }
  }

  inline void begin() {
    SPCR = spcr_;
    SPSR = spsr_;
  }

  inline void end() {
    SPCR = 0;
    SPSR = 0;
  }

  inline uint8_t transferByte(uint8_t data) {
    SPDR = data;
    asm volatile("nop");
    while (!(SPSR & _BV(SPIF))) {
      ; // wait
    }
    return SPDR;
  }

  inline void sendBytes(const uint8_t *buf, uint8_t len) {
    if (len == 0) return;
    const uint8_t *end = buf + len;
    while (buf < end) {
      SPDR = *buf;
      while (!(SPSR & _BV(SPIF))) {
        ; // wait
      }
      ++buf;
    }
  }

  inline uint16_t readByte(void) {
    return transferByte(0x00 /* dummy */);
  }

  inline void recvBytes(uint8_t *buf, uint8_t len) {
    const uint8_t *end = buf + len;
    if (len == 0) return;
    while (buf < end) {
      SPDR = 0; // write a dummy to initiate read
      while (!(SPSR & _BV(SPIF))) {
        ; // wait
      }
      *buf = SPDR;
      ++buf;
    }
  }
};
