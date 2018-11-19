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
#include "pinnacle.h"
#include "debug.h"
#include "pincontrol.h"
#include "timer.h"
#include <util/atomic.h>
#include <util/delay.h>

#define SPI_MODE1 0x04
constexpr uint8_t kTapThresh = 30;

enum class Pinnacle::RegAddr : uint8_t {
  FirmwareID = 0x00,
  FirmwareVersion = 0x01,
  Status1 = 0x02,
  SysConfig1 = 0x03,
  FeedConfig1 = 0x04,
  FeedConfig2 = 0x05,
  FeedConfig3 = 0x06,
  CalConfig1 = 0x07,
  ZIdle = 0x0a,
  ZScaler = 0x0b,
  PacketByte0 = 0x12,
  PacketByte1 = 0x13,
  PacketByte2 = 0x14,
  EraValue = 0x1b,
  EraHighByte = 0x1c,
  EraLowByte = 0x1d,
  EraControl = 0x1e,
};

namespace {
#ifndef __AVR__
#error these structs assume a little endian host
#endif
struct Status1Reg {
  unsigned bit0 : 1;
  unsigned bit1 : 1;
  // Software Data Ready
  unsigned sw_dr : 1;
  // Command Complete
  unsigned sw_cc : 1;
  unsigned bit4 : 1;
  unsigned bit5 : 1;
  unsigned bit6 : 1;
  unsigned bit7 : 1;
};
static_assert(sizeof(Status1Reg) == 1, "Status1Reg is 1 byte in size");

struct SysConfig1Reg {
  unsigned reset : 1;
  unsigned standby : 1;
  unsigned auto_sleep : 1;
  unsigned track_disable : 1;
  unsigned anymeas_enable : 1;
  unsigned gpio_ctrl_enable : 1;
  unsigned wakeup_toggle : 1;
  unsigned force_wakeup : 1;
};

struct FeedConfig1Reg {
  unsigned feed_enable : 1;
  unsigned data_mode_absolute : 1;
  unsigned filter_disable : 1;
  unsigned x_disable : 1;
  unsigned y_disable : 1;
  // 1 = x axis, 0 = y-axis
  unsigned axis_for_z : 1;
  unsigned x_data_invert : 1;
  unsigned y_data_invert : 1;
};
static_assert(sizeof(FeedConfig1Reg) == 1, "FeedConfig1Reg is 1 byte in size");

struct FeedConfig2Reg {
  unsigned intellimouse_enable : 1;
  unsigned all_taps_disable : 1;
  unsigned secondary_tap_disable : 1;
  unsigned scroll_disable : 1;
  unsigned glide_extend_disable : 1;
  unsigned palm_before_z_enable : 1;
  unsigned buttons_scroll_enable : 1;
  // 1 = 90 degree rotation, 0 = no rotation
  unsigned swap_x_and_y : 1;
};
static_assert(sizeof(FeedConfig2Reg) == 1, "FeedConfig2Reg is 1 byte in size");

struct FeedConfig3Reg {
  unsigned buttons_456_to_123_in_rel : 1;
  unsigned disable_cross_rate_smoothing : 1;
  unsigned disable_palm_nerd_meas : 1;
  unsigned disable_noise_avoidance : 1;
  unsigned disable_wrap_lockout : 1;
  unsigned disable_dynamic_emi_adjust : 1;
  unsigned disable_hw_emi_detect : 1;
  unsigned disable_sw_emi_detect : 1;
};
static_assert(sizeof(FeedConfig3Reg) == 1, "FeedConfig3Reg is 1 byte in size");

struct CalConfig1Reg {
  unsigned calibrate : 1;
  unsigned background_comp_enable : 1;
  unsigned nerd_comp_enable : 1;
  unsigned track_error_comp_enable : 1;
  unsigned tap_comp_enable : 1;
  unsigned palm_error_comp_enable : 1;
  unsigned calibration_matrix_disable : 1;
  unsigned force_precalibration_noise_check : 1;
};
static_assert(sizeof(CalConfig1Reg) == 1, "CalConfig1Reg is 1 byte in size");

}; // namespace

// TM0xx0xx Mapping and Dimensions
// max value Pinnacle can report for X (0 to (8 * 256) - 1)
#define PINNACLE_XMAX 2047
// max value Pinnacle can report for Y (0 to (6 * 256) - 1)
#define PINNACLE_YMAX 1535

#define PINNACLE_X_LOWER 127  // min "reachable" X value
#define PINNACLE_X_UPPER 1919 // max "reachable" X value
#define PINNACLE_Y_LOWER 63   // min "reachable" Y value
#define PINNACLE_Y_UPPER 1471 // max "reachable" Y value
#define PINNACLE_X_RANGE (PINNACLE_X_UPPER - PINNACLE_X_LOWER)
#define PINNACLE_Y_RANGE (PINNACLE_Y_UPPER - PINNACLE_Y_LOWER)
// divisor for reducing x,y values to an array index for the LUT
#define ZONESCALE 256
#define ROWS_Y ((PINNACLE_YMAX + 1) / ZONESCALE)
#define COLS_X ((PINNACLE_XMAX + 1) / ZONESCALE)

// ADC-attenuation settings (held in BIT_7 and BIT_6)
// 1X = most sensitive, 4X = least sensitive
#define ADC_ATTENUATE_1X 0x00
#define ADC_ATTENUATE_2X 0x40
#define ADC_ATTENUATE_3X 0x80
#define ADC_ATTENUATE_4X 0xC0

bool Pinnacle::init() {
#ifndef PinnacleDRPinNotConnected
  pinMode(PinnacleDRPin, PinDirectionInput);
  digitalWrite(PinnacleDRPin, PinLevelLow);
#endif

  lastData_.hover = TrackpadHover::OffPad;
  tap_ = TrackpadTap::None;
  zIdleCount_ = 0;
  activeCount_ = 0;

  initSPI();
  if (!cyclePower()) {
    print("pinnacle: failed to cyclePower\n");
    return false;
  }

  if (!setAttenuation(ADC_ATTENUATE_1X)) {
    print("pinnacle: failed to set attenuation\n");
    return false;
  }

  // Increase the scaler, otherwise the touchpad is not sensitive enough
  if (!setZScaler(20)) {
    print("pinnacle: failed to set Z scaler\n");
    return false;
  }

  if (!setZIdleCount(kTapThresh + 1)) {
    print("pinnacle: failed to set zidle count\n");
    return false;
  }

  if (!enableRelativeMode(false)) {
    print("pinnacle: failed call enableRelativeMode\n");
    return false;
  }

  if (!tuneSensitivity()) {
    print("pinnacle: failed to tune sensitivity\n");
    return false;
  }

  if (!enableTwoFingerScrollGesture()) {
    print("pinnacle: failed to enableTwoFingerScrollGesture\n");
    return false;
  }

  if (!recalibrate()) {
    print("pinnacle: recalibrate failed\n");
    return false;
  }

  if (!enableTouchDataFeed(true)) {
    print("pinnacle: failed to enableTouchDataFeed\n");
    return false;
  }

  if (!clearFlags()) {
    print("failed to clear flags as last step of init\n");
    return false;
  }

  print("trackpad is online\n");

  return true;
}

bool Pinnacle::getData(struct TrackpadData *result) {
  auto success = relative_ ? getRelativeData(result) : getAbsoluteData(result);
  if (!clearFlags()) {
    print("pinnacle: SPI fail while clearFlags\n");
    return false;
  }
  return success;
}

bool Pinnacle::setZScaler(uint8_t value) {
  return rapWrite(RegAddr::ZScaler, value);
}

bool Pinnacle::getAbsoluteData(struct TrackpadData *result) {
  uint8_t rawdata[5];
  if (!rapRead(RegAddr::PacketByte1, rawdata, sizeof(rawdata))) {
    return false;
  }

  AbsTrackpadData data;
  data.hover = TrackpadHover::OffPad;
  data.xpos = uint16_t(rawdata[1]) | ((uint16_t(rawdata[3]) & 0xf) << 8);
  data.ypos = uint16_t(rawdata[2]) | ((uint16_t(rawdata[3]) & 0xf0) << 4);
  uint8_t z = rawdata[4] & 0x3f;
  uint8_t palm = rawdata[0] & 0b00111111;

  TrackpadTap tap = TrackpadTap::None;

  auto is_z_idle = data.xpos == 0 && data.ypos == 0 && z == 0;
  if (!is_z_idle) {
    // Constrain to valid ranges; it is possible to receive values outside
    // this range due to electrical noise.
    if (data.xpos < PINNACLE_X_LOWER) {
      data.xpos = PINNACLE_X_LOWER;
    } else if (data.xpos > PINNACLE_X_UPPER) {
      data.xpos = PINNACLE_X_UPPER;
    }

    if (data.ypos < PINNACLE_Y_LOWER) {
      data.ypos = PINNACLE_Y_LOWER;
    } else if (data.ypos > PINNACLE_Y_UPPER) {
      data.ypos = PINNACLE_Y_UPPER;
    }

    // Apply some processing to detect whether a finger is "hovering"
    // over the pad.  Curved pads cause the finger to be closer to the
    // sensors in the center of the pad compared to the edges of the pad,
    // so this buckets the finger position and applies a transformation.

    auto zone_x = data.xpos / ZONESCALE;
    auto zone_y = data.ypos / ZONESCALE;

    static const uint8_t mapping[ROWS_Y][COLS_X] = {
        {0, 0, 0, 0, 0, 0, 0, 0},   {0, 2, 3, 5, 5, 3, 2, 0},
        {0, 3, 5, 15, 15, 5, 2, 0}, {0, 3, 5, 15, 15, 5, 3, 0},
        {0, 2, 3, 5, 5, 3, 2, 0},   {0, 0, 0, 0, 0, 0, 0, 0},
    };

    auto hovering = !(z > mapping[zone_x][zone_y]);
    data.hover = hovering ? TrackpadHover::Hovering : TrackpadHover::OnPad;

    // Does this look like a drag?  We get some number of zIdle packets
    // after the finger has left the pad and those come at the rate of
    // one every 10ms.  We can use that count to determine how long it
    // has been since the finger left the pad.
    if (zIdleCount_ && zIdleCount_ < kTapThresh) {
      // We transitioned "on->off->on" and the off portion was within
      // our threshold for deciding that the primary button is down
      tap = TrackpadTap::Drag;
    } else if (tap_ == TrackpadTap::Drag) {
      tap = TrackpadTap::Drag;
    } else {
      tap = TrackpadTap::None;
    }

    zIdleCount_ = 0;
    activeCount_++;
  } else {
    if (zIdleCount_ == 0 && activeCount_ <= kTapThresh) {
      // Transitioned "off->on->off" and the on portion was
      // within the threshold, so this is a  tap
      tap = TrackpadTap::Tap;
    } else {
      tap = TrackpadTap::None;
    }
    zIdleCount_++;
    activeCount_ = 0;
  }

  result->buttons = (tap == TrackpadTap::None) ? 0 : 1;
  tap_ = tap;

#if 0
  print("abs x=");
  pdec(data.xpos);
  print(" y=");
  pdec(data.ypos);
  print(" z=");
  pdec(z);
  print(" hover=");
  if(data.hover == TrackpadHover::OffPad) {
    print("OffPad");
  } else if (data.hover == TrackpadHover::Hovering) {
    print("Hovering");
  } else {
    print("OnPad");
  }
  print(" zidle=");
  pdec(zIdleCount_);
  print(" palm=");
  pdec(palm);
  print(" button=");
  if (tap == TrackpadTap::None) {
    print("None");
  } else if (tap == TrackpadTap::Tap) {
    print("Tap");
  } else {
    print("Drag");
  }
  print("\n");
#endif

  // Convert this to some kind of relative mode.
  if (tap != TrackpadTap::Tap && data.hover == TrackpadHover::OnPad &&
      lastData_.hover == TrackpadHover::OnPad) {
    result->xDelta = int16_t(data.xpos) - int16_t(lastData_.xpos);
    // Note: abs data has inverted y coord vs. relative data
    result->yDelta = int16_t(lastData_.ypos) - int16_t(data.ypos);
  } else {
    result->xDelta = 0;
    result->yDelta = 0;
  }
  result->wheel = 0;

  lastData_ = data;

  return true;
}

bool Pinnacle::getRelativeData(struct TrackpadData *result) {
  uint8_t data[4];
  if (!rapRead(RegAddr::PacketByte0, data, sizeof(data))) {
    return false;
  }

#if 0
  print("raw ");
  pbin(data[0]);
  print(" ");
  pbin(data[1]);
  print(" ");
  pbin(data[2]);
  print(" ");
  pbin(data[3]);
  print("\n");
#endif

  static float Accel = 1.2;

  result->buttons = data[0] & 0b111;
  result->xDelta = int8_t(Accel * float(int8_t(data[1])));
  result->yDelta = int8_t(Accel * float(int8_t(data[2])));
  result->wheel = int8_t(data[3]);

  return true;
}

bool Pinnacle::setZIdleCount(uint8_t count) {
  return rapWrite(RegAddr::ZIdle, count);
}

bool Pinnacle::getZIdleCount(uint8_t *count) {
  return rapRead(RegAddr::ZIdle, count, sizeof(*count));
}

bool Pinnacle::testIfPresent() {
  if (!rapWrite(RegAddr::ZIdle, 0)) {
    return false;
  }

  _delay_us(500);

  uint8_t current = 0;
  if (!getZIdleCount(&current)) {
    return false;
  }
  return current == 0;
}

bool Pinnacle::waitForCommandComplete() {
  Status1Reg status;
  do {
    if (!rapRead(RegAddr::Status1, (uint8_t *)&status, sizeof(status))) {
      print("failed to read status1 in waitForCommandComplete\n");
      return false;
    }
  } while (!status.sw_cc);

  return clearFlags();
}

bool Pinnacle::cyclePower() {
  SysConfig1Reg config;
  if (!rapRead(RegAddr::SysConfig1, (uint8_t *)&config, sizeof(config))) {
    return false;
  }

  config.reset = true;

  if (!rapWrite(RegAddr::SysConfig1, *(uint8_t *)&config)) {
    return false;
  }

  _delay_ms(100);

  config.reset = false;
  config.force_wakeup = true;
  config.auto_sleep = false;

  if (!rapWrite(RegAddr::SysConfig1, *(uint8_t *)&config)) {
    return false;
  }

  _delay_us(500);

  return waitForCommandComplete();
}

bool Pinnacle::enableRelativeMode(bool enable) {
  FeedConfig1Reg current;
  if (!rapRead(RegAddr::FeedConfig1, (uint8_t *)&current, sizeof(current))) {
    return false;
  }

  current.data_mode_absolute = !enable;

  if (!rapWrite(RegAddr::FeedConfig1, *(uint8_t *)&current)) {
    return false;
  }
  relative_ = enable;
  return true;
}

bool Pinnacle::enableTouchDataFeed(bool enable) {
  FeedConfig1Reg current;
  if (!rapRead(RegAddr::FeedConfig1, (uint8_t *)&current, sizeof(current))) {
    return false;
  }
  current.feed_enable = enable;

  if (!rapWrite(RegAddr::FeedConfig1, *(uint8_t *)&current)) {
    return false;
  }
  return true;
}

bool Pinnacle::enableTwoFingerScrollGesture() {
  FeedConfig2Reg current;
  if (!rapRead(RegAddr::FeedConfig2, (uint8_t *)&current, sizeof(current))) {
    return false;
  }

  current.palm_before_z_enable = false;
  current.glide_extend_disable = false;
  current.scroll_disable = false;
  current.secondary_tap_disable = false;
  current.all_taps_disable = false;
  current.intellimouse_enable = true;
  return rapWrite(RegAddr::FeedConfig2, *(uint8_t *)&current);
}

bool Pinnacle::recalibrate() {
  CalConfig1Reg current;
  if (!rapRead(RegAddr::CalConfig1, (uint8_t *)&current, sizeof(current))) {
    return false;
  }
  current.calibrate = true;
  current.background_comp_enable = true;
  current.tap_comp_enable = true;
  current.track_error_comp_enable = true;
  current.nerd_comp_enable = true;

  if (!rapWrite(RegAddr::CalConfig1, *(uint8_t *)&current)) {
    return false;
  }

  return waitForCommandComplete();
}

void Pinnacle::initSPI() {
  spcr_ = _BV(SPE) | _BV(MSTR) | SPI_MODE1;
  spsr_ = _BV(SPI2X);

  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    // Ensure that SS is OUTPUT High.
    // We do this both for the attached pin and the internal
    // SS signal.
    digitalWrite(PinnacleCSPin, PinLevelHigh);
    pinMode(PinnacleCSPin, PinDirectionOutput);

    // Internal SS pin
    // Even though we don't directly use this signal, we must
    // set it to output mode otherwise the hardware can revert
    // to slave mode.
    digitalWrite(B0, PinLevelHigh);
    pinMode(B0, PinDirectionOutput);

    SPCR = spcr_;
    SPSR = spsr_;
    pinMode(PinnacleSCKPin, PinDirectionOutput);
    pinMode(PinnacleMOSIPin, PinDirectionOutput);
  }
}

bool Pinnacle::dataIsReady() {
#ifdef PinnacleDRPinNotConnected
  Status1Reg status;
  if (!rapRead(RegAddr::Status1, (uint8_t *)&status, sizeof(status))) {
    print("failed to read status1 in dataIsReady\n");
    return false;
  }

  if (status.sw_dr) {
    return true;
  }
#else
  if (digitalRead(PinnacleDRPin)) {
    return true;
  }
#endif

  return false;
}

bool Pinnacle::clearFlags() {
  if (!rapWrite(RegAddr::Status1, 0)) {
    return false;
  }
  return true;
}

void Pinnacle::spiBegin() {
  SPCR = spcr_;
  SPSR = spsr_;
}

bool Pinnacle::spiTransferByte(uint8_t data, uint8_t *result) {
  uint16_t timerStart = timer_read();

  SPDR = data;
  asm volatile("nop");
  while (!(SPSR & _BV(SPIF))) {
    if (timer_elapsed(timerStart) >= 50) {
      print("SPI timeout\n");
      SPCR &= ~_BV(SPE);
      return false;
    }
  }
  *result = SPDR;
  return true;
}

void Pinnacle::assertCS() { digitalWrite(PinnacleCSPin, PinLevelLow); }

void Pinnacle::releaseCS() { digitalWrite(PinnacleCSPin, PinLevelHigh); }

#if 0
#define checkFiller(x)                                                         \
  do {                                                                         \
    if (x != 0xfb) {                                                           \
      print("expected filled byte 0xfb but got ");                             \
      phex(x);                                                                 \
      print("\n");                                                             \
      goto done;                                                               \
    }                                                                          \
  } while (0)
#else
#define checkFiller(x) /* nothing */
#endif

bool Pinnacle::rapWrite(Pinnacle::RegAddr regNo, uint8_t value) {
  bool success = false;
  spiBegin();
  assertCS();
  uint8_t ignored;
  static constexpr uint8_t kWriteMask = 0x80;
  if (!spiTransferByte(kWriteMask | uint8_t(regNo), &ignored)) {
    goto done;
  }
  if (!spiTransferByte(value, &ignored)) {
    goto done;
  }
  checkFiller(ignored);
  success = true;

done:
  releaseCS();
  return success;
}

bool Pinnacle::rapRead(Pinnacle::RegAddr regNo, uint8_t *buf, uint8_t len) {
  bool success = false;
  const uint8_t *end = buf + len;

  spiBegin();
  assertCS();
  // Signal a read from address regNo
  uint8_t ignored;
  static constexpr uint8_t kReadMask = 0xa0;
  if (!spiTransferByte(kReadMask | uint8_t(regNo), &ignored)) {
    goto done;
  }
  // Send two filler bytes; 0xFC is auto-increment mode and tells
  // the device to return data from regNo..=regNo+len
  if (!spiTransferByte(0xfc, &ignored)) {
    goto done;
  }
  checkFiller(ignored);

  if (!spiTransferByte(0xfc, &ignored)) {
    goto done;
  }
  checkFiller(ignored);

  while (buf < end) {
    if (!spiTransferByte(buf == end - 1 ? 0xfb : 0xfc, buf)) {
      goto done;
    }
    ++buf;
  }

  success = true;

done:
  releaseCS();
  return success;
}

bool Pinnacle::waitForEraStatusClear() {
  uint8_t control;
  do {
    if (!rapRead(RegAddr::EraControl, &control, sizeof(control))) {
      return false;
    }
  } while (control != 0);
  return true;
}

bool Pinnacle::eraRead(uint16_t reg, uint8_t *buf, uint8_t len) {
  bool success = false;
  enableTouchDataFeed(false);

  const uint8_t *end = buf + len;

  if (!rapWrite(RegAddr::EraHighByte, uint8_t(reg >> 8))) {
    goto done;
  }
  if (!rapWrite(RegAddr::EraLowByte, uint8_t(reg & 0xff))) {
    goto done;
  }

  while (buf < end) {
    // Signal auto-increment read
    if (!rapWrite(RegAddr::EraControl, 5)) {
      goto done;
    }
    if (!waitForEraStatusClear()) {
      goto done;
    }

    if (!rapRead(RegAddr::EraValue, buf, 1)) {
      goto done;
    }

    ++buf;
  }

  success = true;
done:
  if (!clearFlags()) {
    return false;
  }
  return success;
}

bool Pinnacle::eraWrite(uint16_t reg, uint8_t value) {
  bool success = false;
  enableTouchDataFeed(false);

  if (!rapWrite(RegAddr::EraValue, value)) {
    goto done;
  }
  if (!rapWrite(RegAddr::EraHighByte, uint8_t(reg >> 8))) {
    goto done;
  }
  if (!rapWrite(RegAddr::EraLowByte, uint8_t(reg & 0xff))) {
    goto done;
  }

  if (!waitForEraStatusClear()) {
    goto done;
  }

  success = true;

done:
  if (!clearFlags()) {
    return false;
  }
  return success;
}

bool Pinnacle::setAttenuation(uint8_t level) {
  uint8_t current;
  static constexpr uint16_t kAttentuationReg = 0x0187;
  if (!eraRead(kAttentuationReg, &current, sizeof(current))) {
    return false;
  }
  current &= 0x3f;
  current |= level;
  return eraWrite(kAttentuationReg, current);
}

bool Pinnacle::tuneSensitivity() {
  // xaxis wide z min (4)
  if (!eraWrite(0x0149, 0)) {
    return false;
  }
  // yaxis wide z min (3)
  if (!eraWrite(0x0168, 0)) {
    return false;
  }

  FeedConfig3Reg config3;
  if (!rapRead(RegAddr::FeedConfig3, (uint8_t *)&config3, sizeof(config3))) {
    return false;
  }
  config3.disable_noise_avoidance = false;
  config3.disable_palm_nerd_meas = false;
  config3.disable_cross_rate_smoothing = false;
  if (!rapWrite(RegAddr::FeedConfig3, *(uint8_t *)&config3)) {
    return false;
  }

  return true;
}

static Pinnacle trackpad;

bool trackpad_init() { return trackpad.init(); }

bool trackpad_get_data(struct TrackpadData *data) {
  if (!trackpad.dataIsReady()) {
    return false;
  }
  if (!trackpad.getData(data)) {
    print("Error reading trackpad data\n");
    return false;
  }
  return true;
}
