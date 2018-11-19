/*
A driver for the Cirque Pinnacle touch controller.

Copyright 2018 Wez Furlong

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
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define PinnacleCSPin D3 // D1
#define PinnacleDRPin F6 // ~INT on A1
// Define this if you didn't connect the DR signal.
// The firmware will use a higher latency poll to
// detect new data.  Not recommended.
//#define PinnacleDRPinNotConnected
#define PinnacleSCKPin B1  // SCK
#define PinnacleMOSIPin B2 // MOSI
#define PinnacleMISOPin B3 // MISO

struct TrackpadData {
  uint8_t buttons;
  int16_t xDelta;
  int16_t yDelta;
  int8_t wheel;
};

enum TrackpadHover {
  OffPad = 0,
  Hovering,
  OnPad,
};

enum TrackpadTap {
  None,
  Tap,
  Drag,
};

struct AbsTrackpadData {
  uint16_t xpos;
  uint16_t ypos;
  enum TrackpadHover hover;
};

#ifdef __cplusplus
extern "C" {
#endif
bool trackpad_init(void);
bool trackpad_get_data(struct TrackpadData *data);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
class Pinnacle {
public:
  bool init();

  bool enableTouchDataFeed(bool enable);
  bool enableTwoFingerScrollGesture();
  bool cyclePower();
  bool testIfPresent();
  bool enableRelativeMode(bool enable);
  bool dataIsReady();

  // Clears Status1 register flags (SW_CC and SW_DR)
  bool clearFlags();

  bool getData(struct TrackpadData *result);

private:
  enum class RegAddr : uint8_t;
  void initSPI();
  void spiBegin();
  bool spiTransferByte(uint8_t data, uint8_t *result);
  bool spiSendBytes(const uint8_t *buf, uint8_t len);

  bool rapWrite(RegAddr regNo, uint8_t value);
  bool rapRead(RegAddr regNo, uint8_t *buf, uint8_t len);

  bool eraWrite(uint16_t reg, uint8_t value);
  bool eraRead(uint16_t regNo, uint8_t *buf, uint8_t len);
  bool waitForEraStatusClear();
  bool setAttenuation(uint8_t level);
  bool tuneSensitivity();
  bool recalibrate();
  bool waitForCommandComplete();
  bool setZScaler(uint8_t value);

  // Sets the number of Z-idle packets to be sent when liftoff is detected
  // NOTE: Z-idle packets contains all zero values and are useful for detecting
  // rapid taps
  bool setZIdleCount(uint8_t count);
  bool getZIdleCount(uint8_t *count);

  bool getRelativeData(struct TrackpadData *result);
  bool getAbsoluteData(struct TrackpadData *result);

  void assertCS();
  void releaseCS();

  uint8_t spcr_;
  uint8_t spsr_;
  bool relative_{false};
  AbsTrackpadData lastData_;
  uint8_t zIdleCount_{0};
  uint8_t activeCount_{0};
  TrackpadTap tap_;
};
#endif
