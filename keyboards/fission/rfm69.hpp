#pragma once
#include "spi.hpp"
#include <avr/io.h>

// Some defaults for the adafruit feather with embedded
// RFM69HCW.  I built this using the 433MHz model.  The
// pinouts should be the same for the other radio bands.
// Pay attention; the adafruit schematics list arduino
// pin numbers and you have to translate those to AVR
// pins to use here.
#ifndef RFM69_RESET_PIN
# define RFM69_RESET_PIN D4
#endif
#ifndef RFM69_CS_PIN
# define RFM69_CS_PIN B4
#endif
#ifndef RFM69_INT_PIN
# define RFM69_INT_PIN E6
#endif
#ifndef RFM69_FREQ_BAND
# define RFM69_FREQ_BAND 433
#endif
#ifndef RFM69_NETWORK_ID
# define RFM69_NETWORK_ID 1337
#endif

class RFM69 {
public:
  RFM69(uint8_t chipSelectPin = RFM69_CS_PIN,
        uint8_t resetPin = RFM69_RESET_PIN,
        uint8_t interruptPin = RFM69_INT_PIN);

  enum Mode {
    ModeUndefined,
    ModeSleep,
    ModeStandBy,
    ModeSynth,
    ModeRx,
    ModeTx,
  };

  bool initialize(uint8_t nodeId);

  void enableEncryption(const char key[16]);
  void disableEncryption();
  void setMode(Mode mode);

  static const constexpr uint8_t kMaxData = 64;
  uint8_t recvPacket(uint8_t *buf, uint8_t buflen);
  void sendPacket(const uint8_t *buf, uint8_t buflen);

private:
  SPI spi_;
  uint8_t chipSelectPin_;
  uint8_t resetPin_;
  uint8_t interruptPin_;
  Mode mode_;
  uint8_t nodeId_;

  void writeReg(uint8_t addr, uint8_t val);
  uint8_t readReg(uint8_t addr);
  void chipSelect();
  void chipDeSelect();
  void waitForModeReady(int timeout);
};
