#ifdef _ESP_ME_SUPPORT

#include "EspMe.h"

EspMe::EspMe(uint8_t pNpnPin, uint8_t pPnpPin, uint8_t pRecvEnablePin, uint8_t pRecvPin) {
  npnPin = pNpnPin;
  pnpPin = pPnpPin;
  recvEnablePin = pRecvEnablePin;
  recvPin = pRecvPin;

  pinMode(recvPin, INPUT);

  // nicht auf Masse ziehen
  pinMode(npnPin, OUTPUT);
  digitalWrite(npnPin, LOW);

  // 5V aus
  pinMode(pnpPin, OUTPUT);
  digitalWrite(pnpPin, LOW);

  // recv
  pinMode(recvEnablePin, OUTPUT);
  digitalWrite(recvEnablePin, LOW);
  attachInterrupt(digitalPinToInterrupt(recvPin), recvIntHandler, CHANGE);
}

void EspMe::processRecvData() {
  if ((meData = fifo.next()) == NULL)
    return;
    
  // wait for next command after hangup
  if (lastProcessedCommand == cmd_bell && meData->command == cmd_hangup) {
    unsigned long gap = micros();
    // handle overflow
    if (gap < meData->prespanTime)
      gap += (unsigned long)0xffffffff - meData->prespanTime;
    else
      gap -= meData->prespanTime;
      
    if (gap < 600000) {
      if (fifo.queueLength() == 1)
        return;

      // skip hangup
      if (fifo.queueLength() > 1) {
        Serial.print("skip hangup (gap: ");
        Serial.print(gap/1000);
        Serial.println(" ms)");
        fifo.dequeue(meData);
        meData = fifo.next();
      }
    }
  }
  
  // process message
  String msg = SensorDataHeader("ME", "0");

  switch (meData->station) {
    case SW02_11:
      msg += SensorDataValue(Station, "SW02_11");
      break;
    case SW02_10:
      msg += SensorDataValue(Station, "SW02_10");
      break;
    case SW02_01:
      msg += SensorDataValue(Station, "SW02_01");
      break;
    case SW02_INITIAL:
      msg += SensorDataValue(Station, "SW02_INITIAL");
      break;
    case SW02_RESET:
      msg += SensorDataValue(Station, "SW02_RESET");
      break;
    default:
      msg = "";
      break;
  }
  
  if (msg != "")
    switch (meData->command) {
      case   cmd_open:
        msg += SensorDataValue(Command, "open");
        break;
      case cmd_monitor:
        msg += SensorDataValue(Command, "monitor");
        break;
      case cmd_pickup:
        msg += SensorDataValue(Command, "pickup");
        break;
      case cmd_hangup:
        msg += SensorDataValue(Command, "hangup");
        break;
      case cmd_bell:
        msg += SensorDataValue(Command, "bell");
        break;
      default:
        msg = "";
        break;
    }
  
  if (msg != "") {
    sendMessage(msg);
    lastProcessedCommand = meData->command;
  } else {
    Serial.print("unknown station/command! raw: 0x");
    Serial.println(meData->raw, HEX);
  }

  fifo.dequeue(meData);
}

// hardware related functions
void EspMe::send(station_t station, command_t cmd) {
  if (state == state_recv) {
    if (recvInAction()) {
      Serial.print(" recv in action ");
      return;
    }
    receiverEnable(false);
  }

  noInterrupts();
  state = state_send;

  // prespan
  setSendPins(LOW);
  delayCnt(3); 
  
  // send
  sendData(encrypt(station));
  sendData(encrypt(cmd));
  
  // postspan
  setSendPins(HIGH);
  delayCnt(2);
  // 5V aus
  digitalWrite(pnpPin, LOW);

  state = state_idle;
  interrupts();

  receiverEnable(true);
}

void EspMe::sendData(uint16_t data) {
  uint16_t mask = 0x8000;
  
  while (mask > 0) {
    sendBit(((data & mask) ? HIGH : LOW));
    mask >>= 1;
  }
}

void EspMe::sendBit(int data) {
  setSendPins(HIGH);
  delayMicroseconds(clockBase);
  setSendPins(LOW);
  delayMicroseconds(clockBase * (data ? 2 : 1));
  setSendPins(HIGH);
}

void EspMe::setSendPins(byte data) {
  if (data == LOW) {
    digitalWrite(pnpPin, LOW); // 5V aus
    digitalWrite(npnPin, HIGH); // auf Masse ziehen
  } else {
    digitalWrite(npnPin, LOW);  // nicht mehr auf Masse ziehen
    digitalWrite(pnpPin, HIGH);  // 5V an
  }
}

void EspMe::recv() {
  if (state != state_recv)
    return;
    
  if (digitalRead(recvPin) == HIGH) {
    highTime = micros();
  } else {
    // store msg data
    if (bitCnt >= 0 && bitCnt <= 31) {
      data += ((unsigned long)((micros() - highTime) > intervalLong) << (31 - bitCnt));
      bitCnt++;
      if (bitCnt == 32) {
        fifo.queue(prespanTime, (byte)(data >> 24), (byte)((data >> 8) & 0xFF), data);
        bitCnt = -1;
      }
    }
    // reset timed out recv
    recvInAction();
    // prepare msg recv
    if (bitCnt == -1) {
      prespanLength = (unsigned long)(micros() - highTime);
      prespanTime = highTime;
      bitCnt = data = 0;
    }
  }
}

#endif  // _ESP_ME_SUPPORT

