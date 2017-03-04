#include "Arduino.h"

#include "Wire.h"
#include "OneWire.h"
#include "DS2482.h"

#include "ESP8266WiFi.h"

#define PROGNAME "Esp1wire"
#define PROGVERS "0.1"

// KeyValueProtocol with full format keys
//#define KVP_LONG_KEY_FORMAT 1
#include "KVPSensors.h"


// The following settings can also be set from FHEM
#define DEBUG_DISABLED        0x0
#define DEBUG_MODE            0x1
#define DEBUG_PACKET_RAW      0x2
#define DEBUG_UNDECODED_RAW   0x4
byte DEBUG                    = DEBUG_DISABLED;        // bit mask 1=debug, 2=packet raw data, 3=undecoded packet raw data

byte BMP_ID                   = 0;
bool sendUdpPackets           = true;
bool httpRequestProcessed     = false;

//#define _DEBUG
//#define _DEBUG_SETUP
#define _DEBUG_TIMING
//#define _DEBUG_HEAP
//#define _DEBUG_TEST_DATA

#include "Esp1wire.h"

unsigned long lastTemp = 0, lastAlarm = 0, lastCounter = 0, lastBatt = 0;

void setup() {
  Serial.begin(115200);
  yield();

  Serial.println("\n\n");
  
  setupEspTools();
  setupEspWifi();

  printHeapFree();

  if (!esp1wire.probeI2C() && !esp1wire.probeGPIO())
    Serial.println("no 1-wire detected!");
  else
    esp1wire.resetSearch();

  // for all detected devices
  Serial.println("list all devices");
  Esp1wire::DeviceFilter deviceFilter = esp1wire.getDeviceFilter();
  while (deviceFilter.hasNext()) {
    Esp1wire::Device *device = deviceFilter.getNextDevice();
    Serial.print("device: " + device->getOneWireDeviceID() + " -> " + device->getName());
    // temperature devices
    if (device->getDeviceType() == Esp1wire::DeviceTypeTemperature) {
      switch (((Esp1wire::TemperatureDevice*)device)->readResolution()) {
        case Esp1wire::TemperatureDevice::resolution12bit:
          Serial.print(" res: 12 bit");
          break;
        case Esp1wire::TemperatureDevice::resolution11bit:
          Serial.print(" res: 11 bit");
          break;
        case Esp1wire::TemperatureDevice::resolution10bit:
          Serial.print(" res: 10 bit");
          break;
        case Esp1wire::TemperatureDevice::resolution9bit:
          Serial.print(" res: 9 bit");
          break;
      }
      if (((Esp1wire::TemperatureDevice*)device)->powerSupply())
        Serial.print(" parasite");

//      if (((Esp1wire::TemperatureDevice*)device)->setAlarmTemperatures(20, 25))
//        Serial.print(" set");
      int8_t alarmLow, alarmHigh;
      if (((Esp1wire::TemperatureDevice*)device)->getAlarmTemperatures(&alarmLow, &alarmHigh)) {
        Serial.print(" alarm low: " + String(alarmLow) + " high: " + String(alarmHigh));
      }
    }
    // switch devices
    if (device->getDeviceType() == Esp1wire::DeviceTypeSwitch) {
      ((Esp1wire::SwitchDevice*)device)->setConditionalSearch(
          Esp1wire::SwitchDevice::ConditionalSearchPolarityHigh
        , Esp1wire::SwitchDevice::SourceSelectActivityLatch
        , Esp1wire::SwitchDevice::ChannelSelectBoth
        , Esp1wire::SwitchDevice::ChannelFlipFlopBoth
      );
    }
    Serial.println();
  }

  printHeapFree();

#ifdef _DEBUG_TEST_DATA
    esp1wire.testData();
#endif
}

void loop() {
  // read alarm
  if ((lastAlarm + 5000) < millis()) {
    alarmSearch();
    lastAlarm = millis();
  }
  
  // read counter
  if ((lastCounter + 10000) < millis()) {
    readCounter();
    lastCounter = millis();
  }
  
  // read temp
  if ((lastTemp + 20000) < millis()) {
    readTemperatures();
    lastTemp = millis();
  }

  // read batt
  if ((lastBatt + 30000) < millis()) {
    readBatteries();
    lastBatt = millis();
  }

  // handle wifi
  loopEspWifi();
}

void alarmSearch() {
  // add parameter Esp1wire::DeviceTypeSwitch
  Esp1wire::AlarmFilter alarmFilter = esp1wire.alarmSearch(Esp1wire::DeviceTypeSwitch);
  while (alarmFilter.hasNext()) {
    Esp1wire::Device *device = alarmFilter.getNextDevice();
    Serial.println("alarm: " + device->getOneWireDeviceID());

    // suppress alarm
//      if (device->getDeviceType() == Esp1wire::DeviceTypeTemperature)
//        device->setIgnoreAlarmSearch(true);

    // switch
    Esp1wire::SwitchDevice::SwitchChannelStatus channelStatus;
    if (device->getDeviceType() == Esp1wire::DeviceTypeSwitch && ((Esp1wire::SwitchDevice*)device)->resetAlarm(&channelStatus)) {
      Serial.print("latch,sense A: " + String(channelStatus.latchA) + "," + String(channelStatus.senseA));
      if (channelStatus.noChannels == 2)
        Serial.print(" B: " + String(channelStatus.latchB) + "," + String(channelStatus.senseB));
      Serial.println();
    }
  }
}

void readCounter() {
  printHeapFree();
  Esp1wire::DeviceFilter deviceFilter = esp1wire.getDeviceFilter(Esp1wire::DeviceTypeCounter);
  while (deviceFilter.hasNext()) {
    Esp1wire::Device *device = deviceFilter.getNextDevice();

    Serial.print(device->getOneWireDeviceID() + " ");
//      if (device->getDeviceType() == Esp1wire::DeviceTypeCounter) {
      uint32_t c1, c2;
      unsigned long tempStart;
      tempStart= micros();
      if (((Esp1wire::CounterDevice*)device)->getCounter(&c1, &c2))
        Serial.print("counter " + String(c1) + " " + String(c2) + " " + elapTime(tempStart));
//      }
    Serial.println();
  }
}

void readTemperatures() {
  printHeapFree();

  // calculate temperatures
  esp1wire.requestTemperatures(true); // reset ignoreAlarmFlags for all TemperatureDevices

  // read
  Esp1wire::TemperatureDeviceFilter deviceFilter = esp1wire.getTemperatureDeviceFilter();
  while (deviceFilter.hasNext()) {
    Esp1wire::TemperatureDevice *device = deviceFilter.getNextDevice();

    float tempC;
    String message = SensorDataHeader(PROGNAME, device->getOneWireDeviceID());
    unsigned long tempStart;
    tempStart = micros();
    if (((Esp1wire::TemperatureDevice*)device)->readTemperatureC(&tempC))
      message += SensorDataValue(Temperature, tempC); // + " " + elapTime(tempStart));
//      // reset alarm (set same value for low and high; next calculation updates alarm flag)
//      if (((Esp1wire::TemperatureDevice*)device)->setAlarmTemperatures(20, 25))
//        Serial.print(" set alarm " + elapTime(tempStart));
//      tempStart = micros();
//        if (((Esp1wire::TemperatureDevice*)device)->requestTemperatureC(&tempC))
//          Serial.print(" req " + (String)tempC + " " + elapTime(tempStart));

    sendMessage(message, tempStart);
  }
}

void readBatteries() {
  printHeapFree();

  // calculate temperatures
  esp1wire.requestBatteries();

  // read
  Esp1wire::DeviceFilter deviceFilter = esp1wire.getDeviceFilter(Esp1wire::DeviceTypeBattery);
  while (deviceFilter.hasNext()) {
    Esp1wire::BatteryDevice *device = (Esp1wire::BatteryDevice*)deviceFilter.getNextDevice();

    float voltage, current, capacity;
    Serial.print(device->getOneWireDeviceID());
    unsigned long tempStart;
    tempStart= micros();
    if (((Esp1wire::BatteryDevice*)device)->readBattery(&voltage, &current, &capacity))
      Serial.print(" voltage " + (String)voltage + " current " + (String)current + " capacity " + (String)capacity + " " + elapTime(tempStart));
    Serial.println();
  }
}

void printHeapFree() {
#ifdef _DEBUG_HEAP
  Serial.println((String)F("heap: ") + (String)(ESP.getFreeHeap()));
#endif
}

String getDictionary() {
  String result = "";

#ifndef KVP_LONG_KEY_FORMAT
  result +=
  DictionaryValue(Temperature);// +
//  DictionaryValue(Pressure) +
//  DictionaryValue(Humidity) +
//  DictionaryValue(WindSpeed) +
//  DictionaryValue(WindDirection) +
//  DictionaryValue(WindGust) +
//  DictionaryValue(WindGustRef) +
//  DictionaryValue(RainTipCount) +
//  DictionaryValue(RainSecs) +
//  DictionaryValue(Solar) +
//  DictionaryValue(VoltageSolar) +
//  DictionaryValue(VoltageCapacitor) +
//  DictionaryValue(SoilLeaf) +
//  DictionaryValue(UV) +
//  DictionaryValue(Channel) +
//  DictionaryValue(Battery) +
//  DictionaryValue(RSSI) +
//  DictionaryValuePort(SoilTemperature, 1) + 
//  DictionaryValuePort(SoilMoisture, 1) + 
//  DictionaryValuePort(SoilTemperature, 2) + 
//  DictionaryValuePort(SoilMoisture, 2) + 
//  DictionaryValuePort(SoilTemperature, 3) + 
//  DictionaryValuePort(SoilMoisture, 3) + 
//  DictionaryValuePort(SoilTemperature, 4) + 
//  DictionaryValuePort(SoilMoisture, 4) + 
//  DictionaryValuePort(LeafWetness, 1) +
//  DictionaryValuePort(LeafWetness, 2) +
//  DictionaryValue(PacketDump);
#endif

  return result;
}

void handleInput(char r, bool hasValue, unsigned long value, bool hasValue2, unsigned long value2) {
  switch (r) {
    case 'v':
      // Version info
      handleCommandV();
      print_config();
      break;
    case ' ':
    case '\n':
    case '\r':
      break;
    default:
      handleCommandV();
/*
#ifndef NOHELP
      Help::Show();
#endif
*/      break;
    }
}

void handleSerialPort(char c) {
  static long value, value2;
  bool hasValue, hasValue2;
  char r = c;

  // reset variables
  value = 0; hasValue = false;
  value2 = 0; hasValue2 = false;
  
  byte sign = 0;
  // char is a number
  if ((r >= '0' && r <= '9') || r == '-'){
    byte delays = 2;
    while ((r >= '0' && r <= '9') || r == ',' || r == '-') {
      if (r == '-') {
        sign = 1;
      } else {
        // check value separator
        if (r == ',') {
          if (!hasValue || hasValue2) {
            print_warning(2, "format");
            return;
          }
          
          hasValue2 = true;
          if (sign == 0) {
            value = value * -1;
            sign = 0;
          }
        } else {
          if (!hasValue || !hasValue2) {
            value = value * 10 + (r - '0');
            hasValue = true;
          } else {
            value2 = value2 * 10 + (r - '0');
            hasValue2 = true;
          }
        }
      }
            
      // wait a little bit for more input
      while (Serial.available() == 0 && delays > 0) {
        delay(20);
        delays--;
      }

      // more input available
      if (delays == 0 && Serial.available() == 0) {
        return;
      }

      r = Serial.read();
    }
  }

  // Vorzeichen
  if (sign == 1) {
    if (hasValue && !hasValue2)
      value = value * -1;
    if (hasValue && hasValue2)
      value2 = value2 * -1;
  }

  handleInput(r, hasValue, value, hasValue2, value2);
}

void handleCommandV() {
  Serial.print("[");
  Serial.print(PROGNAME);
  Serial.print('.');
  Serial.print(PROGVERS);

  Serial.print("] ");
}

void bmpDataCallback(float temperature, int pressure) {
  sendMessage((String)SensorDataHeader("BMP180", BMP_ID) + SensorDataValue(Temperature, temperature) + SensorDataValue(Pressure, pressure));
}

void sendMessage(String message) {
  Serial.println(message);  
  sendMultiCast(message);
}

void sendMessage(String message, unsigned long startTime) {
  Serial.println(message + " " + elapTime(startTime));  
  sendMultiCast(message);
}

// helper
void print_config() {
  String blank = F(" ");
  
  Serial.print(F("config:"));
}

void print_warning(byte type, String msg) {
  return;
  Serial.print(F("\nwarning: "));
  if (type == 1)
    Serial.print(F("skipped incomplete command "));
  if (type == 2)
    Serial.print(F("wrong parameter "));
  if (type == 3)
    Serial.print(F("failed: "));
  Serial.println(msg);
}

