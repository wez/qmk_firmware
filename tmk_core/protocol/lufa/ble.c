#include "ble.h"
#include <stdio.h>
#include <stdlib.h>
#include <alloca.h>
#include <util/delay.h>
#include "debug.h"
#include "pincontrol.h"
#include "analog.h"
#include "analog.c" // oof, but quantum/analog.c isn't in any makefile

static volatile bool is_connected;

// Commands are encoded using SDEP and sent via SPI
// https://github.com/adafruit/Adafruit_BluefruitLE_nRF51/blob/master/SDEP.md

#define SDEP_MAX_PAYLOAD 16
struct sdep_msg {
  uint8_t type;
  uint8_t cmd_low;
  uint8_t cmd_high;
  uint8_t len; // MSB: more msgs follow.
  uint8_t payload[SDEP_MAX_PAYLOAD];
} __attribute__((packed));

enum sdep_type {
  SdepCommand = 0x10,
  SdepResponse = 0x20,
  SdepAlert = 0x40,
  SdepError = 0x80,
  SdepSlaveNotReady = 0xfe, // Try again later
  SdepSlaveOverflow = 0xff, // You read more data than is available
};

enum ble_cmd {
  BleInitialize = 0xbeef,
  BleAtWrapper = 0x0a00,
  BleUartTx = 0x0a01,
  BleUartRx = 0x0a02,
};

enum ble_system_event_bits {
  BleSystemConnected = 0,
  BleSystemDisconnected = 1,
  BleSystemUartRx = 8,
  BleSystemMidiRx = 10,
};

// The SDEP.md file says 2MHz but the web page and the sample driver
// both use 4MHz
#define SpiBusSpeed 4000000

#define SdepTimeoutLoops 5000 /* based on 250ms with a 50us delay */

#define BleResetPin D4
#define BleCSPin    B4
#define BleIRQPin   E6

#include <LUFA/Drivers/Peripheral/SPI.h>

static inline void spi_send_bytes(const uint8_t *buf, uint8_t len) {
  const uint8_t *end = buf + len;
  while (buf < end) {
    SPI_SendByte(*buf);
    ++buf;
  }
}

// Read a byte; we use 0xff as the dummy value to initiate the SPI read.
static inline uint16_t spi_read_byte(void) {
  return SPI_TransferByte(0xff);
}

static inline void spi_recv_bytes(uint8_t *buf, uint8_t len) {
  const uint8_t *end = buf + len;
  while (buf < end) {
    *buf = spi_read_byte();
    ++buf;
  }
}

// Send a single SDEP packet
static bool sdep_send_pkt(uint16_t command, const uint8_t *payload, uint8_t len,
                          bool moredata) {
  struct sdep_msg msg = {
    SdepCommand,
    command & 0xff,
    command >> 8,
    len | ((moredata && len == SDEP_MAX_PAYLOAD) ? 0x80 : 0)
  };

  memcpy(msg.payload, payload, len);

  digitalWrite(BleCSPin, PinLevelLow);
  uint16_t retries = SdepTimeoutLoops;

  while (SPI_TransferByte(msg.type) == SdepSlaveNotReady && retries > 0) {
    // Release it and let it initialize
    digitalWrite(BleCSPin, PinLevelHigh);
    _delay_us(50);
    digitalWrite(BleCSPin, PinLevelLow);
    --retries;
  }

  if (retries > 0) {
    // Slave is ready; send the rest of the packet
    spi_send_bytes(&msg.cmd_low, sizeof(msg) - sizeof(msg.payload) + len);
  }

  digitalWrite(BleCSPin, PinLevelHigh);

  return retries > 0;
}

// Read a single SDEP packet
static bool sdep_recv_pkt(struct sdep_msg *msg) {
  // Use a longer timeout for reading results
  uint16_t retries = 2 * SdepTimeoutLoops;
  bool success = false;

  digitalWrite(BleCSPin, PinLevelLow);

  while (retries > 0) {
    // Read the command type, waiting for the data to be ready
    msg->type = spi_read_byte();
    if (msg->type == SdepSlaveNotReady || msg->type == SdepSlaveOverflow) {
      // Release it and let it initialize
      digitalWrite(BleCSPin, PinLevelHigh);
      _delay_us(50);
      digitalWrite(BleCSPin, PinLevelLow);
      --retries;
      continue;
    }

    // Read the rest of the header
    spi_recv_bytes(&msg->cmd_low, sizeof(*msg) - sizeof(msg->payload));

    // and get the payload if there is any
    if ((msg->len & 0x7f) <= SDEP_MAX_PAYLOAD) {
      spi_recv_bytes(msg->payload, msg->len & 0x7f);
    }
    success = true;
    break;
  }

  digitalWrite(BleCSPin, PinLevelLow);
  return success;
}

static bool ble_init(void) {
  uint8_t speed = 0;
  bool res;

  pinMode(BleIRQPin, PinDirectionInput);
  pinMode(BleCSPin, PinDirectionOutput);
  digitalWrite(BleCSPin, PinLevelHigh);

#if SpiBusSpeed == F_CPU / 2
  speed = SPI_SPEED_FCPU_DIV_2;
#elif SpiBusSpeed == F_CPU / 4
  speed = SPI_SPEED_FCPU_DIV_4;
#elif SpiBusSpeed == F_CPU / 8
  speed = SPI_SPEED_FCPU_DIV_8;
#elif SpiBusSpeed == F_CPU / 16
  speed = SPI_SPEED_FCPU_DIV_16;
#elif SpiBusSpeed == F_CPU / 32
  speed = SPI_SPEED_FCPU_DIV_32;
#elif SpiBusSpeed == F_CPU / 64
  speed = SPI_SPEED_FCPU_DIV_64;
#elif SpiBusSpeed == F_CPU / 128
  speed = SPI_SPEED_FCPU_DIV_128;
#else
#error cannot figure out how to set up SPI speed
#endif

  // SPI Mode 0 with MSB first
  SPI_Init(SPI_ORDER_MSB_FIRST | SPI_SCK_LEAD_FALLING | SPI_SAMPLE_LEADING |
           SPI_MODE_MASTER | speed);

  res = sdep_send_pkt(BleInitialize, 0, 0, 0);

  if (res) {
    // Give it a second to reset
    _delay_ms(1000);
  }

  return true;
}

static inline uint8_t min(uint8_t a, uint8_t b) {
  return a < b ? a : b;
}

static bool read_response(char *resp, uint16_t resplen) {
  char *dest = resp;
  char *end = dest + resplen;

  while (true) {
    struct sdep_msg msg;

    if (!sdep_recv_pkt(&msg)) {
      return false;
    }

    if (msg.type != SdepResponse) {
      return false;
    }

    uint8_t len = min(msg.len & 0x7f, end - dest);
    if (len > 0) {
      memcpy(dest, msg.payload, len);
      dest += len;
    }

    if ((msg.len & 0x80) == 0) {
      // No more data is expected!
      break;
    }
  }

  // Ensure the response is NUL terminated
  *dest = 0;

  // "Parse" the result text; we want to snip off the trailing OK or ERROR line

  // Rewind past the possible trailing CRLF so that we can strip it
  --dest;
  while (dest >= resp && (dest[0] == '\n' || dest[0] == '\r')) {
    *dest = 0;
    --dest;
  }

  // Skip over the last line until we find the preceeding CR (or start of
  // string)
  while (dest >= resp && dest[0] != '\r') {
    --dest;
  }

  bool success = false;
  static const char kOK[] PROGMEM = "OK";
  static const char kNlOK[] PROGMEM = "\r\nOK";

  success = !strcmp_P(dest, dest == resp ? kOK : kNlOK);

  if (success) {
    // Trim off the success trailer
    *dest = 0;
  }

  dprintf("result: %s\n", resp);
  return success;
}

bool ble_at_command(const char *cmd, char *resp, uint16_t resplen) {
  char *end = resp + resplen;

  dprintf("ble send: %s\n", cmd);

  // Fragment the command into a series of SDEP packets
  while (end - cmd > SDEP_MAX_PAYLOAD) {
    if (!sdep_send_pkt(BleAtWrapper, (uint8_t*)cmd, SDEP_MAX_PAYLOAD, true)) {
      return false;
    }
    cmd += SDEP_MAX_PAYLOAD;
  }
  if (!sdep_send_pkt(BleAtWrapper, (uint8_t*)cmd, end - cmd, false)) {
    return false;
  }

  return read_response(resp, resplen);
}

bool ble_at_command_P(const char *cmd, char *resp, uint16_t resplen) {
  char *cmdbuf  = alloca(strlen(cmd) + 1);
  strcpy_P(cmdbuf, cmd);
  return ble_at_command(cmd, resp, resplen);
}

bool ble_query_is_connected(void) {
  char resbuf[32];

  static const char kGetConn[] PROGMEM = "AT+GAPGETCONN";
  ble_at_command_P(kGetConn, resbuf, sizeof(resbuf));
  return atoi(resbuf);
}

#define GAPNAME(a, b) #a " " #b

bool ble_enable_keyboard(void) {
  char resbuf[128];

  if (!ble_init()) {
    return false;
  }

  // Make the advertised name match the keyboard
  static const char kGapDevName[] PROGMEM =
      "AT+GAPDEVNAME=" GAPNAME(PRODUCT, DESCRIPTION);
  ble_at_command_P(kGapDevName, resbuf, sizeof(resbuf));

  // Turn on keyboard support
  static const char kHidEnOn[] PROGMEM = "AT+BleHIDEn=On";
  ble_at_command_P(kHidEnOn, resbuf, sizeof(resbuf));

  // Enable battery level reporting
  static const char kBleBatEn[] PROGMEM = "AT+BLEBATTEN";
  ble_at_command_P(kBleBatEn, resbuf, sizeof(resbuf));

  // Reset the device so that it picks up the above changes
  static const char kATZ[] PROGMEM = "ATZ";
  ble_at_command_P(kATZ, resbuf, sizeof(resbuf));

  // Turn down the power level a bit
  static const char kPower[] PROGMEM = "AT+BLEPOWERLEVEL=-12";
  ble_at_command_P(kPower, resbuf, sizeof(resbuf));

  is_connected = ble_query_is_connected();

  return true;
}

int ble_get_rssi(void) {
  char resbuf[32];

  static const char kGetRSSI[] PROGMEM = "AT+BLEGETRSSI";
  ble_at_command_P(kGetRSSI, resbuf, sizeof(resbuf));
  return atoi(resbuf);
}

void ble_task(void) {
  // TODO: use timer_read32() here to periodically do this and
  // update the battery percentage service.
  ble_read_battery_voltage();

  if (!digitalRead(BleIRQPin)) {
    return;
  }
  char resbuf[48];
  static const char kEventStatus[] PROGMEM = "AT+EVENTSTATUS";
  if (!ble_at_command_P(kEventStatus, resbuf, sizeof(resbuf))) {
    return;
  }

  uint32_t system_event = 0;
  char *end;
  system_event = strtoul(resbuf, &end, 10);

#if 0
  uint32_t gatts_event=0;
  if (end && *end == ',') {
    gatts_event = strtoul(end + 1, &end, 10);
  }
#endif

  if (system_event & (1 << BleSystemConnected)) {
    is_connected = true;
  }
  if (system_event & (1 << BleSystemDisconnected)) {
    is_connected = false;
  }
}

bool ble_send_keys(uint8_t hid_modifier_mask, uint8_t *keys, uint8_t nkeys) {
  char cmdbuf[48];
  char fmtbuf[48];

  // Pre-process to avoid sending lots of zero'ed out keys
  while (nkeys > 0) {
    if (keys[nkeys - 1] == 0) {
      --nkeys;
    } else {
      break;
    }
  }

  static const char kCmd[] PROGMEM = "AT+BLEKEYBOARDCODE=%02x-%02x";
  static const char kHex[] PROGMEM = "-%02x";
  while (nkeys > 0) {
    // We can send up to 6 keys at once
#define safe_key(n) nkeys > (n-1) ? keys[n] : 0
    fmtbuf[0] = 0;
    strcat_P(fmtbuf, kCmd);

    for (uint8_t n = 1; n < min(nkeys, 6); ++n) {
      strcat_P(fmtbuf, kHex);
    }
    snprintf(cmdbuf, sizeof(cmdbuf), fmtbuf, hid_modifier_mask, keys[0],
             safe_key(1), safe_key(2), safe_key(3), safe_key(4), safe_key(5));

    ble_at_command(cmdbuf, cmdbuf, sizeof(cmdbuf));
    if (nkeys <= 6) {
      break;
    }
    nkeys -= 6;
    keys += 6;
  }

  // Release the keys
  static const char kUp[] PROGMEM = "AT+BLEKEYBOARDCODE=00-00";
  ble_at_command_P(kUp, cmdbuf, sizeof(cmdbuf));

  return true;
}

bool ble_send_consumer_key(uint16_t keycode, int hold_duration) {
  char cmdbuf[48];

  snprintf(cmdbuf, sizeof(cmdbuf), "AT+BLEHIDMOUSEMOVE=0x%04x,%d", keycode,
           hold_duration);
  ble_at_command(cmdbuf, cmdbuf, sizeof(cmdbuf));
  return true;
}

#ifdef MOUSE_ENABLE
bool ble_send_mouse_move(int8_t x, int8_t y, int8_t scroll, int8_t pan) {
  char cmdbuf[32];
  snprintf(cmdbuf, sizeof(cmdbuf), "AT+BLEHIDMOUSEMOVE=%d,%d,%d,%d", x, y,
           scroll, pan);
  ble_at_command(cmdbuf, cmdbuf, sizeof(cmdbuf));
  return true;
}
#endif

bool ble_send_battery_percentage(uint8_t percent) {
  char cmdbuf[32];
  snprintf(cmdbuf, sizeof(cmdbuf), "AT+BLEBATTVAL=%d", percent);
  ble_at_command(cmdbuf, cmdbuf, sizeof(cmdbuf));
  return true;
}

// Lipoly batteries are 'maxed out' at 4.2V and stick around 3.7V for much of
// the battery life, then slowly sink down to 3.2V or so before the protection
// circuitry cuts it off. By measuring the voltage you can quickly tell when
// you're heading below 3.7V
float ble_read_battery_voltage(void) {
  float vbat = analogRead(9);
  // The level of A9 is divided by 2 by a resistor attached to this pin,
  // so we need to double it here.
  vbat *= 2 /* resistor */ * 3.3 /* reference voltage */;
  vbat /= 1024; /* from scaled digital value to float */
  dprintf("VBat: %f\n", vbat);
  return vbat;
}
