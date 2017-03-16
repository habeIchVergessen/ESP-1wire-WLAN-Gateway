#include "Arduino.h"

#include "SPI.h"      // suppress compile errors from other libraries
#include "Ticker.h"   // suppress compile errors from other libraries

#include "Wire.h"
#include "OneWire.h"
#include "DS2482.h"

#include "ESP8266WiFi.h"
#include "ESP8266WebServer.h"
#include "EspWifi.h"

// KeyValueProtocol with full format keys
//#define KVP_LONG_KEY_FORMAT 1
#include "KVPSensors.h"

byte BMP_ID                   = 0;
bool httpRequestProcessed     = false;

//#define _DEBUG
#define _DEBUG_SETUP
#define _DEBUG_TIMING
//#define _DEBUG_TIMING_UDP
//#define _DEBUG_HEAP
//#define _DEBUG_TEST_DATA
//#define _DEBUG_DUMMY_DEVICES
//#define _DEBUG_DEVICE_DS2438

// EspWifi
//#define _ESP_WIFI_UDP_MULTICAST_DISABLED

#define PROGNAME "Esp1wire"
#define PROGVERS "0.1"

//#define _MQTT_SUPPORT

#include "EspConfig.h"
#include "Esp1wire.h"

Esp1wire esp1wire;
Esp1wire::Scheduler scheduler;

// global config object
EspConfig espConfig(PROGNAME);

// prototypes
void alarmSearch(Esp1wire::DeviceType filter=Esp1wire::DeviceTypeAll);
void readTemperatures(Esp1wire::DeviceType filter=Esp1wire::DeviceTypeAll);
void readBatteries(Esp1wire::DeviceType filter=Esp1wire::DeviceTypeAll);
void readCounter(Esp1wire::DeviceType filter=Esp1wire::DeviceTypeAll);
void resetSearch(Esp1wire::DeviceType filter=Esp1wire::DeviceTypeAll);

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
    Serial.println("device: " + device->getOneWireDeviceID() + " -> " + device->getName());
  }

  // deviceConfig handler
  registerDeviceConfigCallback(handleDeviceConfig);
  registerDeviceListCallback(handleDeviceList);
  
  // scheduleConfig handler
  registerScheduleConfigCallback(handleScheduleConfig);
  registerScheduleListCallback(handleScheduleList);

  // scheduler
  scheduler.registerCallback(Esp1wire::Scheduler::scheduleAlarmSearch, alarmSearch);
  scheduler.registerCallback(Esp1wire::Scheduler::scheduleRequestTemperatues, readTemperatures);
  scheduler.registerCallback(Esp1wire::Scheduler::scheduleRequestBatteries, readBatteries);
  scheduler.registerCallback(Esp1wire::Scheduler::scheduleReadCounter, readCounter);
  scheduler.registerCallback(Esp1wire::Scheduler::scheduleResetSearch, resetSearch);
  scheduler.loadSchedules();

  printHeapFree();

#ifdef _DEBUG_TEST_DATA
  esp1wire.testData();
#endif
}

void loop() {
  // scheduler
  scheduler.runSchedules();
  
  // handle wifi
  loopEspWifi();

  // handle input
  if (Serial.available()) {
    handleSerialPort(Serial.read());
  }
}

void alarmSearch(Esp1wire::DeviceType filter) {
  // add parameter Esp1wire::DeviceTypeSwitch
  Esp1wire::AlarmFilter alarmFilter = esp1wire.alarmSearch(filter);
  while (alarmFilter.hasNext()) {
    Esp1wire::Device *device = alarmFilter.getNextDevice();

    // temperature
    unsigned long tempStart = micros();
    float tempC;
    if (device->getDeviceType() == Esp1wire::DeviceTypeTemperature && ((Esp1wire::TemperatureDevice*)device)->readTemperatureC(&tempC)) {
      String message = SensorDataHeader(PROGNAME, device->getOneWireDeviceID());

      message += SensorDataValue(Temperature, tempC);
      message += SensorDataValue(Device, device->getName());

      sendMessage(message, tempStart);
    }

    // switch
    tempStart = micros();
    Esp1wire::SwitchDevice::SwitchChannelStatus channelStatus;
    if (device->getDeviceType() == Esp1wire::DeviceTypeSwitch && ((Esp1wire::SwitchDevice*)device)->resetAlarm(&channelStatus)) {
      String message = SensorDataHeader(PROGNAME, device->getOneWireDeviceID());

      message += SensorDataValuePort(Latch, 'A', channelStatus.latchA);
      message += SensorDataValuePort(Sense, 'A', channelStatus.senseA);
      message += SensorDataValuePort(FlipFlopQ, 'A', channelStatus.flipFlopQA);
      if (channelStatus.noChannels == 2) {
        message += SensorDataValuePort(Latch, 'B', channelStatus.latchB);
        message += SensorDataValuePort(Sense, 'B', channelStatus.senseB);
        message += SensorDataValuePort(FlipFlopQ, 'B', channelStatus.flipFlopQB);
      }
      message += SensorDataValue(Device, device->getName());

      sendMessage(message, tempStart);
    }
  }
}

void readCounter(Esp1wire::DeviceType filter) {
  printHeapFree();
  Esp1wire::DeviceFilter deviceFilter = esp1wire.getDeviceFilter(Esp1wire::DeviceTypeCounter);
  while (deviceFilter.hasNext()) {
    Esp1wire::Device *device = deviceFilter.getNextDevice();

    uint32_t c1, c2;
    unsigned long tempStart = micros();
    if (((Esp1wire::CounterDevice*)device)->getCounter(&c1, &c2)) {
      String message = SensorDataHeader(PROGNAME, device->getOneWireDeviceID());
      message += SensorDataValuePort(Counter, '1', c1);
      message += SensorDataValuePort(Counter, '2', c2);
      message += SensorDataValue(Device, device->getName());

      sendMessage(message, tempStart);
    }
  }
}

void readTemperatures(Esp1wire::DeviceType filter) {
  printHeapFree();

  // calculate temperatures
  esp1wire.requestTemperatures(true); // reset ignoreAlarmFlags for all TemperatureDevices

  // read
  Esp1wire::TemperatureDeviceFilter deviceFilter = esp1wire.getTemperatureDeviceFilter();
  while (deviceFilter.hasNext()) {
    Esp1wire::TemperatureDevice *device = deviceFilter.getNextDevice();

    float tempC;
    unsigned long tempStart = micros();
    if (((Esp1wire::TemperatureDevice*)device)->readTemperatureC(&tempC)) {
      String message = SensorDataHeader(PROGNAME, device->getOneWireDeviceID());
      message += SensorDataValue(Temperature, tempC);
      message += SensorDataValue(Device, device->getName());

      sendMessage(message, tempStart);
    }
  }
}

void readBatteries(Esp1wire::DeviceType filter) {
  printHeapFree();

  // read
  Esp1wire::DeviceFilter deviceFilter = esp1wire.getDeviceFilter(Esp1wire::DeviceTypeBattery);
  while (deviceFilter.hasNext()) {
    Esp1wire::BatteryDevice *device = (Esp1wire::BatteryDevice*)deviceFilter.getNextDevice();

    String message = SensorDataHeader(PROGNAME, device->getOneWireDeviceID());
    float voltage, current, capacity, temperature;
    unsigned long battStart = micros();
    if (device->requestVAD(&voltage, &current, &capacity)) {
      message += SensorDataValuePort(Voltage, "VAD", String(voltage, 3));
      message += SensorDataValuePort(Current, "VAD", String(current, 3));
      message += SensorDataValuePort(Capacity, "VAD", String(capacity, 3));
    }
    // requestVdd depends on device config (default vad only)
//    if (device->getRequestVdd() && device->requestVDD(&voltage, &current, &capacity)) {
    if (device->requestVDD(&voltage, &current, &capacity)) {
      message += SensorDataValuePort(Voltage, "VDD", String(voltage, 3));
      message += SensorDataValuePort(Current, "VDD", String(current, 3));
      message += SensorDataValuePort(Capacity, "VDD", String(capacity, 3));
    }
    message += SensorDataValue(Device, device->getName());

    sendMessage(message, battStart);
  }
}

void resetSearch(Esp1wire::DeviceType filter) {
  esp1wire.resetSearch();
}

void listDevices() {
  Esp1wire::DeviceFilter deviceFilter = esp1wire.getDeviceFilter();
  while (deviceFilter.hasNext()) {
    Esp1wire::Device *device = deviceFilter.getNextDevice();
    Serial.println("device: " + device->getOneWireDeviceID() + " -> " + device->getName());
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
  DictionaryValue(Device) +
  DictionaryValue(Temperature) +
  DictionaryValuePort(Counter, 1) +
  DictionaryValuePort(Counter, 2) +
  DictionaryValuePort(Latch, 'A') +
  DictionaryValuePort(Latch, 'B') +
  DictionaryValuePort(Sense, 'A') +
  DictionaryValuePort(Sense, 'B') +
  DictionaryValuePort(FlipFlopQ, 'A') +
  DictionaryValuePort(FlipFlopQ, 'B') +
  DictionaryValuePort(Voltage, "VDD") +
  DictionaryValuePort(Current, "VDD") +
  DictionaryValuePort(Capacity, "VDD") +
  DictionaryValuePort(Voltage, "VAD") +
  DictionaryValuePort(Current, "VAD") +
  DictionaryValuePort(Capacity, "VAD");
#endif

  return result;
}

String handleDeviceConfig(ESP8266WebServer *server, uint16_t *resultCode) {
  String result = "";
  String reqAction = server->arg("action"), deviceID = server->arg("deviceID");
  
  if (reqAction != "form" && reqAction != "submit")
    return result;

  // search device
  Esp1wire::DeviceFilter deviceFilter = esp1wire.getDeviceFilter();
  Esp1wire::Device *device;

  while (deviceFilter.hasNext() && (device = deviceFilter.getNextDevice())->getOneWireDeviceID() != deviceID)
    ;

  // no match
  if (device == NULL || device->getOneWireDeviceID() != deviceID) {
    *resultCode = 404;
    return "Not Found";
  }

//#ifdef _DEBUG_TIMING
//    unsigned long formStart = micros();
//#endif
  if (reqAction == "form") {
    String action = F("/config?ChipID=");
    action += getChipID();
    action += F("&deviceID=");
    action += deviceID;
    action += F("&action=submit");

    String html = "";
    // temperature
    if (device->getDeviceType() == Esp1wire::DeviceTypeTemperature) {
      EspDeviceConfig devConf = espConfig.getDeviceConfig(deviceID);
      String value = devConf.getValue(F("conditionalSearch")), minStr, maxStr;
      int8_t idx, min, max;
      if ((idx = value.indexOf(",")) > 0) {  // min & max
        min = value.substring(0, idx).toInt();
        max = value.substring(idx + 1, value.length()).toInt();
        minStr = (min != 0 || max != 0 ? String(min) : "");
        maxStr = (min != 0 || max != 0 ? String(max) : "");
      } else {
        ((Esp1wire::TemperatureDevice*)device)->getAlarmTemperatures(&min, &max);
        minStr = (min != 0 || max != 0 ? String(min) : "");
        maxStr = (min != 0 || max != 0 ? String(max) : "");
      }
    
      html += htmlLabel(F("minTemp"), F("min. temperature: "));
      html += htmlInput(F("minTemp"), F("number"), minStr, 0, "-55", "125") + htmlNewLine();
      action += F("&minTemp=");
      html += htmlLabel(F("maxTemp"), F("max. temperature: "));
      html += htmlInput(F("maxTemp"), F("number"), maxStr, 0, "-55", "125");
      action += F("&maxTemp=");
      html = htmlFieldSet(html, F("ConditionalSearch"));
      html += htmlLabel(F("customName"), F("custom name: "));
      html += htmlInput(F("customName"), "", devConf.getValue(F("customName")), 20, "", "");
      action += F("&customName=");
    }
    // switch
    if (device->getDeviceType() == Esp1wire::DeviceTypeSwitch) {
      EspDeviceConfig devConf = espConfig.getDeviceConfig(deviceID);
      uint8_t curr = devConf.getValue(F("conditionalSearch")).toInt();
      html += htmlLabel(F("polarity"), F("Polarity: "));
      String options = htmlOption(String(Esp1wire::SwitchDevice::ConditionalSearchPolarityLow), F("LOW"), (curr & 0x01) == Esp1wire::SwitchDevice::ConditionalSearchPolarityLow);
      options += htmlOption(String(Esp1wire::SwitchDevice::ConditionalSearchPolarityHigh), F("HIGH"), (curr & 0x01) == Esp1wire::SwitchDevice::ConditionalSearchPolarityHigh);
      html += htmlSelect(F("polarity"), options) + htmlNewLine();
      action += F("&polarity=");
      html += htmlLabel(F("sourceselect"), F("SourceSelect: "));
      options = htmlOption(String(Esp1wire::SwitchDevice::SourceSelectActivityLatch), F("Activity Latch"), (curr & 0x06) == Esp1wire::SwitchDevice::SourceSelectActivityLatch);
      options += htmlOption(String(Esp1wire::SwitchDevice::SourceSelectChannelFlipFlop), F("Channel FlipFlop"), (curr & 0x06) == Esp1wire::SwitchDevice::SourceSelectChannelFlipFlop);
      options += htmlOption(String(Esp1wire::SwitchDevice::SourceSelectPIOStatus), F("PIO Status"), (curr & 0x06) == Esp1wire::SwitchDevice::SourceSelectPIOStatus);
      html += htmlSelect(F("sourceselect"), options) + htmlNewLine();
      action += F("&sourceselect=");
      html += htmlLabel(F("channelselect"), F("ChannelSelect: "));
      options = htmlOption(String(Esp1wire::SwitchDevice::ChannelSelectDisabled), F("Disabled"), (curr & 0x18) == Esp1wire::SwitchDevice::ChannelSelectDisabled);
      options += htmlOption(String(Esp1wire::SwitchDevice::ChannelSelectA), F("A"), (curr & 0x18) == Esp1wire::SwitchDevice::ChannelSelectA);
      options += htmlOption(String(Esp1wire::SwitchDevice::ChannelSelectB), F("B"), (curr & 0x18) == Esp1wire::SwitchDevice::ChannelSelectB);
      options += htmlOption(String(Esp1wire::SwitchDevice::ChannelSelectBoth), F("Both"), (curr & 0x18) == Esp1wire::SwitchDevice::ChannelSelectBoth);
      html += htmlSelect(F("channelselect"), options) + htmlNewLine();
      action += F("&channelselect=");
      html += htmlLabel(F("channelflipflop"), F("ChannelFlipFlop: "));
      options = htmlOption(String(Esp1wire::SwitchDevice::ChannelFlipFlopA), F("A"), (curr & 0x60) == Esp1wire::SwitchDevice::ChannelFlipFlopA);
      options += htmlOption(String(Esp1wire::SwitchDevice::ChannelFlipFlopB), F("B"), (curr & 0x60) == Esp1wire::SwitchDevice::ChannelFlipFlopB);
      options += htmlOption(String(Esp1wire::SwitchDevice::ChannelFlipFlopBoth), F("Both"), (curr & 0x60) == Esp1wire::SwitchDevice::ChannelFlipFlopBoth);
      html += htmlSelect(F("channelflipflop"), options);
      action += F("&channelflipflop=");
      html = htmlFieldSet(html, F("ConditionalSearch"));
      html += htmlLabel(F("customName"), F("custom name: "));
      html += htmlInput(F("customName"), "", devConf.getValue(F("customName")), 20, "", "");
      action += F("&customName=");
    }
    // battery
    if (device->getDeviceType() == Esp1wire::DeviceTypeBattery) {
      EspDeviceConfig devConf = espConfig.getDeviceConfig(deviceID);
      uint8_t curr = devConf.getValue(F("requestVdd")).toInt();
      html += htmlLabel(F("request"), F("request: "));
      String options = htmlOption("0", F("VAD"), (curr & 0x01) == 0);
      options += htmlOption("1", F("VAD+VDD"), (curr & 0x01) == 1);
      html += htmlSelect(F("request"), options) + htmlNewLine();
      action += F("&request=");
      html += htmlLabel(F("customName"), F("custom name: "));
      html += htmlInput(F("customName"), "", devConf.getValue(F("customName")), 20, "", "");
      action += F("&customName=");
    }
    
    if (html != "") {
      *resultCode = 200;
      html = "<h4>" + device->getName() + " (" + deviceID + ")" + "</h4>" + html;
      result = htmlForm(html, action, "post", "configForm", "", "");
    }
  }

  if (reqAction == "submit") {
    // TemperatureDevice
    if (device->getDeviceType() == Esp1wire::DeviceTypeTemperature) {
      int8_t minTemp = server->arg("minTemp").toInt(), maxTemp = server->arg("maxTemp").toInt();

      EspDeviceConfig devConf = espConfig.getDeviceConfig(deviceID);
      if (minTemp != 0 && maxTemp != 0 && minTemp < maxTemp)
        devConf.setValue("conditionalSearch", String(minTemp) + "," + String(maxTemp));
      if (server->arg("customName") != "")
        devConf.setValue("customName", server->arg("customName"));
      else
        devConf.unsetValue("customName");

      if (devConf.hasChanged()) {
        devConf.saveToFile();
        ((Esp1wire::TemperatureDevice*)device)->readConfig();
      }

      *resultCode = 200;
      result = "ok";
    }

    // SwitchDevice
    if (device->getDeviceType() == Esp1wire::DeviceTypeSwitch) {
      int8_t polarity = server->arg("polarity").toInt() & 0x01;
      int8_t sourceselect = server->arg("sourceselect").toInt() & 0x06;
      int8_t channelselect = server->arg("channelselect").toInt() & 0x18;
      int8_t channelflipflop = server->arg("channelflipflop").toInt() & 0x60;

      switch(sourceselect) {
        case Esp1wire::SwitchDevice::SourceSelectActivityLatch:
        case Esp1wire::SwitchDevice::SourceSelectChannelFlipFlop:
        case Esp1wire::SwitchDevice::SourceSelectPIOStatus:
          break;
        default:
          *resultCode = 403;
          return "wrong value for source select"; // SourceSelect: invalid value report Error
          break;
      }
      
      EspDeviceConfig devConf = espConfig.getDeviceConfig(deviceID);
      devConf.setValue("conditionalSearch", String(channelflipflop | channelselect | sourceselect | polarity));

      if (server->arg("customName") != "")
        devConf.setValue("customName", server->arg("customName"));
      else
        devConf.unsetValue("customName");

      if (devConf.hasChanged()) {
        devConf.saveToFile();
        ((Esp1wire::SwitchDevice*)device)->readConfig();
      }
    }

    // BatteryDevice
    if (device->getDeviceType() == Esp1wire::DeviceTypeBattery) {
      int8_t request = server->arg("request").toInt() & 0x01;

      EspDeviceConfig devConf = espConfig.getDeviceConfig(deviceID);
      devConf.setValue("requestVdd", String(request));

      if (server->arg("customName") != "")
        devConf.setValue("customName", server->arg("customName"));
      else
        devConf.unsetValue("customName");

      if (devConf.hasChanged()) {
        devConf.saveToFile();
        ((Esp1wire::BatteryDevice*)device)->readConfig();
      }

      *resultCode = 200;
      result = "ok";
    }
  }
//#ifdef _DEBUG_TIMING
//    Serial.println("handleDeviceConfig: result = " + String(result.length()) + " " + elapTime(formStart));
//#endif
  
  return result;
}

String handleDeviceList() {
  String result = "";

  // search device
  Esp1wire::DeviceFilter deviceFilter = esp1wire.getDeviceFilter();
  Esp1wire::Device *device;
  while (deviceFilter.hasNext()) {
    device = deviceFilter.getNextDevice();

    result += F("<tr><td>");
    result += device->getOneWireDeviceID();
    result += F("</td><td>");
    result += device->getName();
    result += F("</td><td>");
    switch (device->getDeviceType()) {
      case Esp1wire::DeviceTypeSwitch:
      case Esp1wire::DeviceTypeTemperature:
      case Esp1wire::DeviceTypeBattery:
        result += htmlAnker(device->getOneWireDeviceID(), F("dc"), F("..."));
        break;
    }
    result += F("</td></tr>");
  }

  return result;
}

String handleScheduleConfig(ESP8266WebServer *server, uint16_t *resultCode) {
  uint16_t interval;
  Esp1wire::Scheduler::ScheduleAction action;
  Esp1wire::DeviceType filter;

  String schedule = server->arg("schedule"), result = "";

  if (server->arg("action") == "form") {
    Serial.println("handleScheduleConfig: form");
    String actStr = F("/config?ChipID=");
    actStr += getChipID();
    actStr += F("&schedule=");
    actStr += schedule;
    actStr += F("&action=submit");

    bool isNumber = (schedule.length() > 0 && schedule.substring(0, 1) >= "0" && schedule.substring(0, 1) <= "9");
    bool useValues = (isNumber && scheduler.getSchedule(schedule.toInt(), &interval, &action, &filter));
  
    result = htmlLabel(F("interval"), F("Interval"));
    result += htmlInput(F("interval"), F("number"), String(useValues ? String(interval) : "60"), 0, F("1"), F("3600")) + htmlNewLine();
    result +=  htmlLabel(F("schedAction"), F("Action"));
    String options = htmlOption(String(Esp1wire::Scheduler::scheduleAlarmSearch), F("AlarmSearch"), (useValues && action == Esp1wire::Scheduler::scheduleAlarmSearch));
    options += htmlOption(String(Esp1wire::Scheduler::scheduleRequestTemperatues), F("RequestTemperatues"), (useValues && action == Esp1wire::Scheduler::scheduleRequestTemperatues));
    options += htmlOption(String(Esp1wire::Scheduler::scheduleRequestBatteries), F("RequestBatteries"), (useValues && action == Esp1wire::Scheduler::scheduleRequestBatteries));
    options += htmlOption(String(Esp1wire::Scheduler::scheduleReadCounter), F("ReadCounter"), (useValues && action == Esp1wire::Scheduler::scheduleReadCounter));
    options += htmlOption(String(Esp1wire::Scheduler::scheduleResetSearch), F("ResetSearch"), (useValues && action == Esp1wire::Scheduler::scheduleResetSearch));
    if (useValues) // aka change existing
      options += htmlOption(String(0xFF), F("Disable"), false);
    result += htmlSelect(F("schedAction"), options) + htmlNewLine();
    result += htmlLabel(F("filter"), F("Filter"));
    options = htmlOption(String(Esp1wire::DeviceTypeAll), F("None"), (useValues && filter == Esp1wire::DeviceTypeAll));
    options += htmlOption(String(Esp1wire::DeviceTypeSwitch), F("DeviceTypeSwitch"), (useValues && filter == Esp1wire::DeviceTypeSwitch));
    result += htmlSelect(F("filter"), options) + htmlNewLine();
    result = "<h4>Schedule</h4>" + htmlForm(result, actStr, F("post"), F("submitForm"), F(""), F("")); 
      
    *resultCode = 200;
  }

  if (server->arg("action") == "submit") {
    String intStr = server->arg("interval");
    String actStr = server->arg("schedAction");
    String filtStr = server->arg("filter");

    bool isNumber = (schedule.length() > 0 && schedule.substring(0, 1) >= "0" && schedule.substring(0, 1) <= "9");
    uint16_t interval = 0;
    
    if ((intStr.length() > 0 && intStr.substring(0, 1) >= "0" && intStr.substring(0, 1) <= "9"))
      interval = intStr.toInt();
// TODO: probe actStr & filtStr
    Esp1wire::Scheduler::ScheduleAction action;
    Esp1wire::DeviceType filter = Esp1wire::DeviceTypeAll;
    
//    Serial.print("handleScheduleConfig: submit idx " + schedule + ", intStr " + intStr + ", actStr " + actStr + ", filtStr " + filtStr + ", interval " + String(interval));
    
    if (interval == 0) {
      *resultCode = 403;
      return "wrong interval";
    }
    
    if (isNumber) {
      if (actStr.toInt() == 0xFF)
        scheduler.removeSchedule(schedule.toInt());
      else {
        scheduler.updateSchedule(schedule.toInt(), interval, (Esp1wire::Scheduler::ScheduleAction)actStr.toInt(), (Esp1wire::DeviceType)filtStr.toInt());
      }
    } else {
      scheduler.addSchedule(interval, (Esp1wire::Scheduler::ScheduleAction)actStr.toInt(), (Esp1wire::DeviceType)filtStr.toInt());
    }
    
    *resultCode = 303;
  }
  
  return result;  
}

String handleScheduleList() {
  uint16_t interval;
  Esp1wire::Scheduler::ScheduleAction action;
  Esp1wire::DeviceType filter;
  
  String result = F("<table><tr><th>Gap</th><th>Action</th><th>Filter</th><th></th><th></th></tr>");
  for (uint8_t idx=0; idx<scheduler.getSchedulesCount(); idx++) {
    if (scheduler.getSchedule(idx, &interval, &action, &filter)) {
      result += F("<tr><td>");
      result += String(interval);
      result += F("</td><td>");
      switch (action) {
        case Esp1wire::Scheduler::scheduleRequestTemperatues:
          result += F("RequestTemperatues</td><td>");
          break;
        case Esp1wire::Scheduler::scheduleRequestBatteries:
          result += F("RequestBatteries</td><td>");
          break;
        case Esp1wire::Scheduler::scheduleReadCounter:
          result += F("ReadCounter</td><td>");
          break;
        case Esp1wire::Scheduler::scheduleAlarmSearch:
          result += F("AlarmSearch</td><td>");
          switch (filter) {
            case Esp1wire::DeviceTypeSwitch:
              result += F("DeviceTypeSwitch");
              break;
          }
          break;
        case Esp1wire::Scheduler::scheduleResetSearch:
          result += F("ResetSearch</td><td>");
          break;
      }
      result += F("</td><td><a id=\"schedule#");
      result += String(idx);
      result += F("\" class=\"dc\">...</a>");
      result += F("</td></tr>");
    }
  }

  result += F("</table>");

  return result;
}

void handleInput(char r, bool hasValue, unsigned long value, bool hasValue2, unsigned long value2) {
  switch (r) {
//    case 'p':
//      esp1wire.probeI2C();
//      esp1wire.probeGPIO();
//      listDevices();
//      break;
    case 'r':
      esp1wire.resetSearch();
      listDevices();
      Serial.println("uptime: " + uptime());
      break;
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
#ifdef _DEBUG_TIMING_UDP
  unsigned long multiTime = micros();
#endif
  sendMultiCast(message);
#ifdef _DEBUG_TIMING_UDP
  Serial.println("udp-multicast: " + elapTime(multiTime));
#endif
}

// helper
void print_config() {
  String blank = F(" ");
  
  Serial.print(F("config:"));
  Serial.println();
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

