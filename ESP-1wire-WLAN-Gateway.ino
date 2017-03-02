#include "Arduino.h"

#include "Wire.h"
#include "OneWire.h"

#include "DS2482.h"

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
  
  printHeapFree();

  if (!esp1wire.probeI2C() && !esp1wire.probeGPIO())
    Serial.println("no 1-wire detected!");
  else
    esp1wire.resetSearch();

  Serial.println("list all devices");
  Esp1wire::DeviceFilter deviceFilter = esp1wire.getDeviceFilter();
  while (deviceFilter.hasNext()) {
    Esp1wire::Device *device = deviceFilter.getNextDevice();
    Serial.print("device: " + device->getOneWireDeviceID() + " -> " + device->getName());
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
    lastAlarm = millis();
  }
  
  // read counter
  if ((lastCounter + 10000) < millis()) {
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
    lastCounter = millis();
  }
  
  // read temp
  if ((lastTemp + 20000) < millis()) {
    printHeapFree();

    // calculate temperatures
    esp1wire.requestTemperatures(true); // reset ignoreAlarmFlags for all TemperatureDevices

    // read
    Esp1wire::TemperatureDeviceFilter deviceFilter = esp1wire.getTemperatureDeviceFilter();
    while (deviceFilter.hasNext()) {
      Esp1wire::TemperatureDevice *device = deviceFilter.getNextDevice();

      float tempC;
      Serial.print(device->getOneWireDeviceID());
      unsigned long tempStart;
      tempStart= micros();
      if (((Esp1wire::TemperatureDevice*)device)->readTemperatureC(&tempC))
        Serial.print(" read " + (String)tempC + " " + elapTime(tempStart));
//      tempStart = micros();
//      // reset alarm (set same value for low and high; next calculation updates alarm flag)
//      if (((Esp1wire::TemperatureDevice*)device)->setAlarmTemperatures(20, 25))
//        Serial.print(" set alarm " + elapTime(tempStart));
//      tempStart = micros();
//        if (((Esp1wire::TemperatureDevice*)device)->requestTemperatureC(&tempC))
//          Serial.print(" req " + (String)tempC + " " + elapTime(tempStart));

      Serial.println();
    }
    lastTemp = millis();
  }

  // read batt
  if ((lastBatt + 30000) < millis()) {
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
    lastBatt = millis();
  }
}

void printHeapFree() {
#ifdef _DEBUG_HEAP
  Serial.println((String)F("heap: ") + (String)(ESP.getFreeHeap()));
#endif
}

