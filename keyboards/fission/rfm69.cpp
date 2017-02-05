#include "rfm69.hpp"
#include "RFM69registers.h"
#include "timer.h"
#include <util/delay.h>
#include <avr/pgmspace.h>

RFM69::RFM69(uint8_t chipSelectPin, uint8_t resetPin, uint8_t interruptPin)
    : spi_(100000), chipSelectPin_(chipSelectPin), resetPin_(resetPin),
      interruptPin_(interruptPin), mode_(ModeUndefined) {
}

static const struct reg_and_val {
    uint8_t reg;
    uint8_t val;
} initScript[] PROGMEM = {
    {REG_OPMODE,
     RF_OPMODE_SEQUENCER_ON | RF_OPMODE_LISTEN_OFF | RF_OPMODE_STANDBY},
    {REG_DATAMODUL, RF_DATAMODUL_DATAMODE_PACKET |
                        RF_DATAMODUL_MODULATIONTYPE_FSK |
                        RF_DATAMODUL_MODULATIONSHAPING_00},
    {REG_BITRATEMSB, RF_BITRATEMSB_55555},
    {REG_BITRATELSB, RF_BITRATELSB_55555},
    {REG_FDEVMSB, RF_FDEVMSB_50000},
    {REG_FDEVLSB, RF_FDEVLSB_50000},
    {REG_FRFMSB,
#if RFM69_FREQ_BAND == 315
     RF_FRFMSB_315
#elif RFM69_FREQ_BAND == 433
     RF_FRFMSB_433
#elif RFM69_FREQ_BAND == 868
     RF_FRFMSB_868
#else
     RF_FRFMSB_915
#endif
    },
    {REG_FRFLSB,
#if RFM69_FREQ_BAND == 315
     RF_FRFLSB_315
#elif RFM69_FREQ_BAND == 433
     RF_FRFLSB_433
#elif RFM69_FREQ_BAND == 868
     RF_FRFLSB_868
#else
     RF_FRFLSB_915
#endif
    },
    {REG_RXBW, RF_RXBW_DCCFREQ_010 | RF_RXBW_MANT_16 | RF_RXBW_EXP_2},
    {REG_DIOMAPPING1, RF_DIOMAPPING1_DIO0_01},
    {REG_DIOMAPPING2, RF_DIOMAPPING2_CLKOUT_OFF},
    {REG_IRQFLAGS2, RF_IRQFLAGS2_FIFOOVERRUN},
    {REG_RSSITHRESH, 220},
    {REG_SYNCCONFIG,
     RF_SYNC_ON | RF_SYNC_FIFOFILL_AUTO | RF_SYNC_SIZE_2 | RF_SYNC_TOL_0},
    {REG_SYNCVALUE1, 0x2D},
    {REG_SYNCVALUE2, RFM69_NETWORK_ID},
    {REG_PACKETCONFIG1, RF_PACKET1_FORMAT_VARIABLE | RF_PACKET1_DCFREE_OFF |
                            RF_PACKET1_CRC_ON | RF_PACKET1_CRCAUTOCLEAR_ON |
                            RF_PACKET1_ADRSFILTERING_OFF},
    {REG_PAYLOADLENGTH, 66},
    {REG_FIFOTHRESH, RF_FIFOTHRESH_TXSTART_FIFONOTEMPTY | RF_FIFOTHRESH_VALUE},
    {REG_PACKETCONFIG2, RF_PACKET2_RXRESTARTDELAY_2BITS |
                            RF_PACKET2_AUTORXRESTART_ON | RF_PACKET2_AES_OFF},
    {REG_TESTDAGC, RF_DAGC_IMPROVED_LOWBETA0},
};

bool RFM69::initialize(uint8_t nodeId) {
  // Prep chip-select; it is active-low, so set it high to start
  digitalWrite(chipSelectPin_, PinLevelHigh);
  pinMode(chipSelectPin_, PinDirectionOutput);

  auto start = timer_read();
  do {
    writeReg(REG_SYNCVALUE1, 0xaa);
  } while (readReg(REG_SYNCVALUE1) != 0xaa && timer_elapsed(start) < 50);
  start = timer_read();
  do {
    writeReg(REG_SYNCVALUE1, 0x55);
  } while (readReg(REG_SYNCVALUE1) != 0x55 && timer_elapsed(start) < 50);

  for (auto &item_P : initScript) {
    reg_and_val item;
    memcpy_P(&item, &item_P, sizeof(item));
    writeReg(item.reg, item.val);
  }

  disableEncryption();
  setMode(ModeStandBy);
  waitForModeReady(50);

  nodeId_ = nodeId;
}

void RFM69::chipSelect() {
  spi_.begin();
  digitalWrite(chipSelectPin_, PinLevelLow);
}

void RFM69::chipDeSelect() {
  digitalWrite(chipSelectPin_, PinLevelHigh);
  spi_.end();
}

void RFM69::writeReg(uint8_t addr, uint8_t val) {
  chipSelect();
  spi_.transferByte(addr | 0x80);
  spi_.transferByte(val);
  chipDeSelect();
}

uint8_t RFM69::readReg(uint8_t addr) {
  chipSelect();
  spi_.transferByte(addr);
  auto val = spi_.readByte();
  chipDeSelect();
  return val;
}

void RFM69::enableEncryption(const char key[16]) {
  setMode(ModeStandBy);
  chipSelect();
  spi_.transferByte(REG_AESKEY1 | 0x80);
  for (auto i = 0; i < 16; ++i) {
    spi_.transferByte(key[i]);
  }
  chipDeSelect();
  writeReg(REG_PACKETCONFIG2, (readReg(REG_PACKETCONFIG2) & 0xFE) | 0x01);
}

void RFM69::disableEncryption() {
  setMode(ModeStandBy);
  writeReg(REG_PACKETCONFIG2, readReg(REG_PACKETCONFIG2) & 0xFE);
}

void RFM69::setMode(Mode mode) {
  if (mode_ == mode) {
    return;
  }

  switch (mode) {
    case ModeTx:
      writeReg(REG_OPMODE, (readReg(REG_OPMODE) & 0xE3) | RF_OPMODE_TRANSMITTER);
      break;
    case ModeRx:
      writeReg(REG_OPMODE, (readReg(REG_OPMODE) & 0xE3) | RF_OPMODE_RECEIVER);
      break;
    case ModeSynth:
      writeReg(REG_OPMODE, (readReg(REG_OPMODE) & 0xE3) | RF_OPMODE_SYNTHESIZER);
      break;
    case ModeStandBy:
      writeReg(REG_OPMODE, (readReg(REG_OPMODE) & 0xE3) | RF_OPMODE_STANDBY);
      break;
    case ModeSleep:
      writeReg(REG_OPMODE, (readReg(REG_OPMODE) & 0xE3) | RF_OPMODE_SLEEP);
      break;
    default:
      return;
  }

  // When waking up, wait for the mode to be ready
  if (mode_ == ModeSleep) {
    waitForModeReady(0);
  }

  mode_ = mode;
}

void RFM69::waitForModeReady(int timeout) {
  auto start = timer_read();

  while ((readReg(REG_IRQFLAGS1) & RF_IRQFLAGS1_MODEREADY) == 0x00 &&
         (timeout == 0 || timer_elapsed(start) < timeout)) {
    _delay_us(1);
  }
}

uint8_t RFM69::recvPacket(uint8_t *buf, uint8_t buflen) {
  if (!(readReg(REG_IRQFLAGS2) & RF_IRQFLAGS2_PAYLOADREADY)) {
    return 0;
  }

  setMode(ModeStandBy);
  chipSelect();
  spi_.transferByte(REG_FIFO);
  auto len = spi_.readByte();

  if (len > buflen) {
    len = buflen;
  }

  spi_.recvBytes(buf, len);
  chipDeSelect();
  setMode(ModeRx);

  return len;
}

void RFM69::sendPacket(const uint8_t *buf, uint8_t buflen) {
  setMode(ModeStandBy);
  waitForModeReady(0);
  writeReg(REG_DIOMAPPING1, RF_DIOMAPPING1_DIO0_00); // DIO0 is "Packet Sent"
  if (buflen > kMaxData) {
    buflen = kMaxData;
  }

  chipSelect();
  spi_.transferByte(REG_FIFO | 0x80);
  spi_.transferByte(buflen);
  spi_.sendBytes(buf, buflen);
  chipDeSelect();

  setMode(ModeTx);
  auto start = timer_read();
  static const constexpr int kTxTimeoutMs = 1000;
  while (digitalRead(interruptPin_) == 0 &&
         timer_elapsed(start) < kTxTimeoutMs) {
    // wait for DIO0 to turn HIGH signalling transmission finish
  }

  setMode(ModeStandBy);
}
