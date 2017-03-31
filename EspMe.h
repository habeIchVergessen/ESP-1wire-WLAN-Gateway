#ifdef _ESP_ME_SUPPORT

#ifndef _ESP_ME_H
#define _ESP_ME_H

#include "PacketFifo.h"

class EspMe {
public:
  enum station_t : byte {
    SW02_01       = 0x01
  , SW02_10       = 0x02
  , SW02_11       = 0x80
  , SW02_INITIAL  = 0x83
  , SW02_RESET    = 0xFF
  };
  
  enum command_t : byte {
    cmd_open      = 0x0B
  , cmd_monitor   = 0x0D
  , cmd_pickup    = 0x0E
  , cmd_hangup    = 0x9D
  , cmd_bell      = 0x9E
  };
  
  enum state_t : byte {
    state_idle = 0
  , state_send = 1
  , state_recv = 2
  };
  
  EspMe(uint8_t pNpnPin=MOSI, uint8_t pPnpPin=MISO, uint8_t pRecvEnablePin=SCK, uint8_t pRecvPin=SS);
  void receiverEnable(bool enable) { if (enable) { state = state_recv; bitCnt == -1; digitalWrite(recvEnablePin, HIGH); } else { digitalWrite(recvEnablePin, LOW); state = state_idle; } };
  void processRecvData();
  void recv();
  void send(station_t station, command_t cmd);
  void delayCnt(uint16_t cnt) { for (int idx=0; idx<cnt; idx++) delayMicroseconds(clockBase); };

protected:
  uint8_t npnPin, pnpPin, recvEnablePin, recvPin;
  byte state = state_idle;
  byte station = SW02_11, lastProcessedCommand = 0;

  const uint16_t clockBase = 1000;

  unsigned long highTime = 0, prespanTime = 0, prespanLength = 0, data;
  unsigned long intervalLong = (unsigned long)clockBase * 1.5f, maxRecvTime = (unsigned long)clockBase * (5 + 3 * 32);
  int bitCnt = -1;

  PacketFifo fifo;
  MeData *meData;

  void sendData(uint16_t data);
  void sendBit(int data);
  void setSendPins(byte data);

  // helper
  uint16_t encrypt(uint8_t data) { return (((uint16_t)data << 8) + (data ^ 0xFF)); };
  bool recvInAction() { if (bitCnt == -1 || (unsigned long)(micros() - prespanTime) > maxRecvTime) { bitCnt = -1; return false; } return true; };
};

extern EspMe espMe;

void recvIntHandler() {
  espMe.recv();
};

#endif  // _ESP_ME_H

#endif  // _ESP_ME_SUPPORT
