#include "ble.h"
#include <stdio.h>
#include <stdlib.h>
#include <alloca.h>
#include <util/delay.h>
#include <util/atomic.h>
#include "debug.h"
#include "pincontrol.h"
#include "timer.h"
#include "action_util.h"

static volatile bool is_connected;
static bool initialized;
static bool configured;
#ifdef SAMPLE_BATTERY
static uint32_t last_battery_update;
#endif
#define ConnectionUpdateMinInterval 1000 /* milliseconds */
#define ConnectionUpdateMaxInterval 10000 /* milliseconds */
static uint32_t last_connection_update;
static uint16_t connection_interval = ConnectionUpdateMinInterval;

// Commands are encoded using SDEP and sent via SPI
// https://github.com/adafruit/Adafruit_BluefruitLE_nRF51/blob/master/SDEP.md

#define SdepMaxPayload 16
struct sdep_msg {
  uint8_t type;
  uint8_t cmd_low;
  uint8_t cmd_high;
  struct __attribute__((packed)) {
    uint8_t len:7;
    uint8_t more:1;
  };
  uint8_t payload[SdepMaxPayload];
} __attribute__((packed));

// The recv latency is relatively high, so when we're hammering keys quickly,
// we want to avoid waiting for the responses in the matrix loop.  We maintain
// a short queue for that.  Since there is quite a lot of space overhead for
// the AT command representation wrapped up in SDEP, we queue the minimal
// information here.

enum queue_type {
  QTKeyReport, // 1-byte modifier + 6-byte key report
  QTConsumer,  // 16-bit key code
#ifdef MOUSE_ENABLE
  QTMouseMove, // 4-byte mouse report
#endif
};

struct queue_item {
  enum queue_type queue_type;
  union __attribute__((packed)) {
    struct __attribute__((packed)) {
      uint8_t modifier;
      uint8_t keys[6];
    } key;
    uint16_t consumer;
    struct __attribute__((packed)) {
      uint8_t x, y, scroll, pan;
    } mousemove;
  };
};

struct send_queue {
#define SdepRingBufSize 160
  uint8_t buf[SdepRingBufSize];
  uint8_t head, tail;
  // There's a packet on the wire that we should read back before
  // we send any others
  bool waiting_for_result;
  uint32_t last_send;
};
static struct send_queue send_buf;
static bool send_buf_dequeue(struct queue_item *item);
static void process_queue_item(struct queue_item *item);

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

#define SdepTimeout 150 /* milliseconds */
#define SdepBackOff 25 /* microseconds */
#define BatteryUpdateInterval 10000 /* milliseconds */

#define BleResetPin D4
#define BleCSPin    B4
#define BleIRQPin   E6

struct SPI_Settings {
  uint8_t spcr, spsr;
};

static struct SPI_Settings spi;

// Initialize 4Mhz MSBFIRST MODE0
void SPI_init(struct SPI_Settings *spi) {
  spi->spcr = _BV(SPE) | _BV(MSTR);
  spi->spsr = _BV(SPI2X);

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

static inline void SPI_begin(struct SPI_Settings*spi) {
  SPCR = spi->spcr;
  SPSR = spi->spsr;
}

static inline uint8_t SPI_TransferByte(uint8_t data) {
  SPDR = data;
  asm volatile("nop");
  while (!(SPSR & _BV(SPIF))) {
    ; // wait
  }
  return SPDR;
}

static inline void spi_send_bytes(const uint8_t *buf, uint8_t len) {
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

// Read a byte; we use 0xff as the dummy value to initiate the SPI read.
static inline uint16_t spi_read_byte(void) {
  return SPI_TransferByte(0x00);
}

static inline void spi_recv_bytes(uint8_t *buf, uint8_t len) {
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

static void dump_pkt(const struct sdep_msg *msg) {
  print("pkt: type=");
  print_hex8(msg->type);
  print(" cmd=");
  print_hex8(msg->cmd_high);
  print_hex8(msg->cmd_low);
  print(" len=");
  print_hex8(msg->len);
  print(" more=");
  print_hex8(msg->more);
  print("\n");
}

// Send a single SDEP packet
static bool sdep_send_pkt(const struct sdep_msg *msg) {
  SPI_begin(&spi);

  digitalWrite(BleCSPin, PinLevelLow);
  uint32_t timerStart = timer_read32();
  bool success = false;

  while (SPI_TransferByte(msg->type) == SdepSlaveNotReady &&
         timer_elapsed32(timerStart) < SdepTimeout) {
    // Release it and let it initialize
    digitalWrite(BleCSPin, PinLevelHigh);
    _delay_us(SdepBackOff);
    digitalWrite(BleCSPin, PinLevelLow);
  }

  if (timer_elapsed32(timerStart) < SdepTimeout) {
    // Slave is ready; send the rest of the packet
    spi_send_bytes(&msg->cmd_low,
                   sizeof(*msg) - (1 + sizeof(msg->payload)) + msg->len);
    success = true;
  }

  digitalWrite(BleCSPin, PinLevelHigh);
  if (!success) {
    xprintf("send_pkt success=%d\n", success);
    dump_pkt(msg);
  }

  return success;
}

static inline void sdep_build_pkt(struct sdep_msg *msg, uint16_t command,
                                  const uint8_t *payload, uint8_t len,
                                  bool moredata) {
  msg->type = SdepCommand;
  msg->cmd_low = command & 0xff;
  msg->cmd_high = command >> 8;
  msg->len = len;
  msg->more = (moredata && len == SdepMaxPayload) ? 1 : 0;

  _Static_assert(sizeof(*msg) == 20, "msg is correctly packed");

  memcpy(msg->payload, payload, len);
}

// Read a single SDEP packet
static bool sdep_recv_pkt(struct sdep_msg *msg) {
  bool success = false;
  uint32_t timerStart = timer_read32();
  uint32_t timeout = SdepTimeout * 2;

  while (!digitalRead(BleIRQPin) && timer_elapsed32(timerStart) < timeout) {
    _delay_us(1);
  }

  if (timer_elapsed32(timerStart) < timeout) {
    SPI_begin(&spi);

    digitalWrite(BleCSPin, PinLevelLow);

    while (timer_elapsed32(timerStart) < timeout) {
      // Read the command type, waiting for the data to be ready
      msg->type = spi_read_byte();
      if (msg->type == SdepSlaveNotReady || msg->type == SdepSlaveOverflow) {
        // Release it and let it initialize
        digitalWrite(BleCSPin, PinLevelHigh);
        _delay_us(SdepBackOff);
        digitalWrite(BleCSPin, PinLevelLow);
        continue;
      }

      // Read the rest of the header
      spi_recv_bytes(&msg->cmd_low, sizeof(*msg) - (1 + sizeof(msg->payload)));

      // and get the payload if there is any
      if (msg->len <= SdepMaxPayload) {
        spi_recv_bytes(msg->payload, msg->len);
      }
      success = true;
      break;
    }

    digitalWrite(BleCSPin, PinLevelHigh);
  } else {
    xprintf("note: IRQ was never asserted\n");
  }
  if (!success) xprintf("success = %d, elapsed %d\n", success, timer_elapsed32(timerStart));
  return success;
}

static bool send_buf_read_one(void) {

  if (send_buf.waiting_for_result) {
    if (digitalRead(BleIRQPin)) {
      struct sdep_msg msg;

      while (sdep_recv_pkt(&msg)) {
        if (!msg.more) {
          break;
        }
      }
      // It takes about 5-6ms to get a response
//      xprintf("async: now=%lu sent %lu\n", timer_read32(), send_buf.last_send);
      send_buf.waiting_for_result = false;
#if 0
      print("async ");
      dump_pkt(&msg);
#endif

      return true;

    } else if (timer_elapsed32(send_buf.last_send) > SdepTimeout * 2) {
      print("waiting_for_result: IRQ was never ready\n");
      send_buf.waiting_for_result = false;

      return true;
    } else {
      // It's ok, we can wait
      return false;
    }
  }

  struct queue_item item;
  if (send_buf_dequeue(&item)) {
    process_queue_item(&item);
    return true;
  }

  return false;
}

static void send_buf_wait(const char *cmd) {
  bool didPrint = false;
  while (send_buf.waiting_for_result || send_buf.head != send_buf.tail) {
    if (!didPrint) {
      xprintf("wait on buf for %s\n", cmd);
      didPrint = true;
    }
    send_buf_read_one();
  }
}

static bool ble_init(void) {
  initialized = false;
  configured = false;
  is_connected = false;

  pinMode(BleIRQPin, PinDirectionInput);
  pinMode(BleCSPin, PinDirectionOutput);
  digitalWrite(BleCSPin, PinLevelHigh);

  SPI_init(&spi);

  // Perform a hardware reset
  pinMode(BleResetPin, PinDirectionOutput);
  digitalWrite(BleResetPin, PinLevelHigh);
  digitalWrite(BleResetPin, PinLevelLow);
  _delay_ms(10);
  digitalWrite(BleResetPin, PinLevelHigh);

  _delay_ms(1000); // Give it a second to initialize

  initialized = true;
  return initialized;
}

static inline uint8_t min(uint8_t a, uint8_t b) {
  return a < b ? a : b;
}

static bool read_response(char *resp, uint16_t resplen, bool verbose) {
  char *dest = resp;
  char *end = dest + resplen;

  while (true) {
    struct sdep_msg msg;

    if (!sdep_recv_pkt(&msg)) {
      print("sdep_recv_pkt failed\n");
      return false;
    }

#if 0
    print("recv ");
    dump_pkt(&msg);
#endif

    if (msg.type != SdepResponse) {
      *resp = 0;
      return false;
    }

    uint8_t len = min(msg.len, end - dest);
    if (len > 0) {
      memcpy(dest, msg.payload, len);
      dest += len;
    }

    if (!msg.more) {
      // No more data is expected!
      break;
    }
  }

  // Ensure the response is NUL terminated
  *dest = 0;

  // "Parse" the result text; we want to snip off the trailing OK or ERROR line
  // Rewind past the possible trailing CRLF so that we can strip it
  --dest;
  while (dest > resp && (dest[0] == '\n' || dest[0] == '\r')) {
    *dest = 0;
    --dest;
  }

  // Look back for start of preceeding line
  char *last_line = strrchr(resp, '\n');
  if (last_line) {
    ++last_line;
  } else {
    last_line = resp;
  }

  bool success = false;
  static const char kOK[] PROGMEM = "OK";

  success = !strcmp_P(last_line, kOK );

  if (verbose || !success) {
    xprintf("result: %s\n", resp);
  }
  return success;
}

bool ble_at_command(const char *cmd, char *resp, uint16_t resplen, bool verbose) {
  const char *end = cmd + strlen(cmd);
  struct sdep_msg msg;

  if (verbose) {
    xprintf("ble send: %s\n", cmd);
  }

  if (resp) {
    // They want to decode the response, so we need to flush and wait
    // for all pending I/O to finish before we start this one, so
    // that we don't confuse the results
    send_buf_wait(cmd);
  }

  // Fragment the command into a series of SDEP packets
  while (end - cmd > SdepMaxPayload) {
    sdep_build_pkt(&msg, BleAtWrapper, (uint8_t *)cmd, SdepMaxPayload, true);
    if (!sdep_send_pkt(&msg)) {
      return false;
    }
    cmd += SdepMaxPayload;
  }

  sdep_build_pkt(&msg, BleAtWrapper, (uint8_t *)cmd, end - cmd, false);
  if (!sdep_send_pkt(&msg)) {
    return false;
  }

  if (resp == NULL) {
    send_buf.waiting_for_result = true;
    send_buf.last_send = timer_read32();
    return true;
  }

  return read_response(resp, resplen, verbose);
}

bool ble_at_command_P(const char *cmd, char *resp, uint16_t resplen) {
  char *cmdbuf  = alloca(strlen_P(cmd) + 1);
  strcpy_P(cmdbuf, cmd);
  return ble_at_command(cmdbuf, resp, resplen, false);
}

bool ble_is_connected(void) {
  return is_connected;
}


bool ble_enable_keyboard(void) {
  char resbuf[128];

  if (!initialized && !ble_init()) {
    return false;
  }

  configured = false;

  // Disable command echo
  static const char kEcho[] PROGMEM = "ATE=0";
  // Make the advertised name match the keyboard
  static const char kGapDevName[] PROGMEM =
      "AT+GAPDEVNAME=" STR(PRODUCT) " " STR(DESCRIPTION);
  // Turn on keyboard support
  static const char kHidEnOn[] PROGMEM = "AT+BLEHIDEN=1";
  // Enable battery level reporting
//  static const char kBleBatEn[] PROGMEM = "AT+BLEBATTEN";
  // Reset the device so that it picks up the above changes
  static const char kATZ[] PROGMEM = "ATZ";
  // Turn down the power level a bit
//  static const char kPower[] PROGMEM = "AT+BLEPOWERLEVEL=-12";
  static const PGM_P const configure_commands[] PROGMEM = {
    kEcho,
    kGapDevName,
    kHidEnOn,
//    kBleBatEn,
    // kPower,
    kATZ,
  };

  uint8_t i;
  for (i = 0; i < sizeof(configure_commands) / sizeof(configure_commands[0]);
       ++i) {
    PGM_P cmd;
    memcpy_P(&cmd, configure_commands + i, sizeof(cmd));

    if (!ble_at_command_P(cmd, resbuf, sizeof(resbuf))) {
      goto fail;
    }
  }

  configured = true;
fail:
  return configured;
}

int ble_get_rssi(void) {
  char resbuf[32];

  static const char kGetRSSI[] PROGMEM = "AT+BLEGETRSSI";
  ble_at_command_P(kGetRSSI, resbuf, sizeof(resbuf));
  return atoi(resbuf);
}

void ble_task(void) {
  char resbuf[48];

  if (!configured && !ble_enable_keyboard()) {
    return;
  }

  if (send_buf_read_one()) {
    // Arrange to re-check connection after keys have settled
    connection_interval = ConnectionUpdateMinInterval;
    last_connection_update = timer_read32();
  }

  if (timer_elapsed32(last_connection_update) > connection_interval) {
    static const char kGetConn[] PROGMEM = "AT+GAPGETCONN";
    last_connection_update = timer_read32();

    if (ble_at_command_P(kGetConn, resbuf, sizeof(resbuf))) {
      int state = atoi(resbuf);
      if (state != is_connected) {

        if (state) {
          print("****** BLE CONNECT!!!!\n");
        } else {
          print("****** BLE DISCONNECT!!!!\n");
        }
      }
      is_connected = state;
    }

    // Exponential back off
    if (connection_interval < ConnectionUpdateMaxInterval) {
      connection_interval *= 2;
      if (connection_interval > ConnectionUpdateMaxInterval) {
        connection_interval = ConnectionUpdateMaxInterval;
      }
    }
  }

#ifdef SAMPLE_BATTERY
  if (timer_elapsed32(last_battery_update) > BatteryUpdateInterval) {
    last_battery_update = timer_read32();

    uint16_t vbat = ble_read_battery_voltage();
    // It's impossible to really tell the battery percentage just
    // from the voltage, but we can give a rough estimate.
    // The battery shuts itself off at 3v so we treat that as 0%.
#define kVmax 4200
#define kVmin 3000
    int pct = ((vbat - kVmin) * 100.0) / (kVmax - kVmin);
    xprintf("vbat %d mV -> %d pct\n", vbat, pct);
  }
#endif
}

// Writes a sequence of bytes to the send queue.
// Returns false if there is not enough room.
static bool send_buf_write_bytes(const uint8_t *bytes, uint8_t len) {
  uint8_t head = send_buf.head;
  const uint8_t *end = bytes + len;

  while (bytes < end) {
    uint8_t next = (head + 1) % SdepRingBufSize;
    if (next == send_buf.tail) {
      return false;
    }
    send_buf.buf[head] = *bytes;
    head = next;
    ++bytes;
  }
  // only move the head if everything fits
  send_buf.head = head;
  return true;
}

static bool send_buf_dequeue(struct queue_item *item) {
  uint8_t tail = send_buf.tail;

  if (send_buf.head == tail) {
    // Empty
    return false;
  }

  item->queue_type = send_buf.buf[tail];
  tail = (tail + 1) % SdepRingBufSize;
  uint8_t len;

  switch (item->queue_type) {
    case QTKeyReport:
      len = sizeof(item->key);
      break;
    case QTConsumer:
      len = sizeof(item->consumer);
      break;
#ifdef MOUSE_ENABLE
    case QTMouseMove:
      len = sizeof(item->mousemove);
      break;
#endif
    broken:
    default:
      print("argh, send buffer contents are corrupt\n");
      send_buf.head = send_buf.tail = 0;
      return false;
  }

  // Write to the first byte following the type
  uint8_t *dest = &item->key.modifier;
  uint8_t *end = dest + len;

  while (dest < end) {
    if (send_buf.head == tail) {
      goto broken;
    }
    *dest = send_buf.buf[tail];
    tail = (tail + 1) % SdepRingBufSize;
    ++dest;
  }

  // Commit the new tail position
  send_buf.tail = tail;
  return true;
}

static void process_queue_item(struct queue_item *item) {
  char cmdbuf[48];
  char fmtbuf[64];

  // Arrange to re-check connection after keys have settled
  connection_interval = ConnectionUpdateMinInterval;
  last_connection_update = timer_read32();

  switch (item->queue_type) {
    case QTKeyReport:
      strcpy_P(fmtbuf,
          PSTR("AT+BLEKEYBOARDCODE=%02x-00-%02x-%02x-%02x-%02x-%02x-%02x"));
      snprintf(cmdbuf, sizeof(cmdbuf), fmtbuf, item->key.modifier,
               item->key.keys[0], item->key.keys[1], item->key.keys[2],
               item->key.keys[3], item->key.keys[4], item->key.keys[5]);
      ble_at_command(cmdbuf, NULL, 0, false);
      return;

    case QTConsumer:
      strcpy_P(fmtbuf, PSTR("AT+BLEHIDCONTROLKEY=0x%04x"));
      snprintf(cmdbuf, sizeof(cmdbuf), fmtbuf, item->consumer);
      ble_at_command(cmdbuf, NULL, 0, true);
      return;

#ifdef MOUSE_ENABLE
    case QTMouseMove:
      strcpy_P(fmtbuf, PSTR("AT+BLEHIDMOUSEMOVE=%d,%d,%d,%d"));
      snprintf(cmdbuf, sizeof(cmdbuf), fmtbuf, item->mousemove.x,
          item->mousemove.y, item->mousemove.scroll, item->mousemove.pan);
      ble_at_command(cmdbuf, NULL, 0, true);
      return;
#endif
  }
}

bool ble_send_keys(uint8_t hid_modifier_mask, uint8_t *keys, uint8_t nkeys) {
  struct queue_item item;
  bool didWait = false;

  item.queue_type = QTKeyReport;
  item.key.modifier = hid_modifier_mask;

  while (nkeys >= 0) {
    item.key.keys[0] = keys[0];
    item.key.keys[1] = nkeys >= 1 ? keys[1] : 0;
    item.key.keys[2] = nkeys >= 2 ? keys[2] : 0;
    item.key.keys[3] = nkeys >= 3 ? keys[3] : 0;
    item.key.keys[4] = nkeys >= 4 ? keys[4] : 0;
    item.key.keys[5] = nkeys >= 5 ? keys[5] : 0;

    if (!send_buf_write_bytes((uint8_t*)&item, 1 + sizeof(item.key))) {
      if (!didWait) {
        print("wait for buf space\n");
        didWait = true;
      }
      send_buf_read_one();
      continue;
    }

    if (nkeys <= 6) {
      return true;
    }

    nkeys -= 6;
    keys += 6;
  }

  return true;
}

bool ble_send_consumer_key(uint16_t keycode, int hold_duration) {
  struct queue_item item;

  item.queue_type = QTConsumer;
  item.consumer = keycode;

  return send_buf_write_bytes((uint8_t*)&item, 1 + sizeof(item.consumer));
}

#ifdef MOUSE_ENABLE
bool ble_send_mouse_move(int8_t x, int8_t y, int8_t scroll, int8_t pan) {
  struct queue_item item;

  item.queue_type = QTMouseMove;
  item.mousemove.x = x;
  item.mousemove.y = y;
  item.mousemove.scroll = scroll;
  item.mousemove.pan = pan;

  return send_buf_write_bytes((uint8_t*)&item, 1 + sizeof(item.mousemove));
}
#endif

// Lipoly batteries are 'maxed out' at 4.2V and stick around 3.7V for much of
// the battery life, then slowly sink down to 3.2V or so before the protection
// circuitry cuts it off. By measuring the voltage you can quickly tell when
// you're heading below 3.7V
float ble_read_battery_voltage(void) {
  int low, high;

#define BatteryChannel 12 // Pin A9
  ADCSRB = (ADCSRB & ~(1 << MUX5)) | (((BatteryChannel >> 3) & 0x01) << MUX5);
  ADMUX = (1 << 6) | (BatteryChannel & 0x07);
  ADCSRA |= 1<<ADSC;
	while (ADCSRA & (1<<ADSC)) ;			// wait for result
  low = ADCL;
  high = ADCH;
  float vbat = (high << 8) | low;

  // The level of A9 is divided by 2 by a resistor attached to this pin,
  // so we need to double it here.
  vbat *= 2 /* resistor */ * 3.3 /* reference voltage */;
  vbat *= 1024.0; /* from scaled digital value */
  return vbat;
}
