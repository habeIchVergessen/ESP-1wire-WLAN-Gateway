#include "Arduino.h"

#ifdef ESP8266

#include "EspConfig.h"

// class EspConfig
EspConfig::EspConfig(String appName) {
  mAppName = appName;
  SPIFFS.begin();
  if (openRead()) {
    while (configFile.available()) {
      String data = configFile.readStringUntil('\n');

      int idx;
      if (data.startsWith("'") && data.endsWith("'") && (idx = data.indexOf("' = '")) > 0) {
        setValue(data.substring(1, idx), data.substring(idx + 5, data.length() - 1));
      }
    }
    configChanged = false;
  }
}

EspConfig::~EspConfig() {
  ConfigList *next = first;

  while (next != NULL) {
    next = first->next;
    delete (first);
    first = next;
  }
}

bool EspConfig::openRead() {
  return (configFile = SPIFFS.open(fileName().c_str(), "r"));
}

bool EspConfig::openWrite() {
  return (configFile = SPIFFS.open(fileName().c_str(), "w"));
}

String EspConfig::getValue(String name) {
  String value;
  ConfigList *curr = first;
  while (curr != NULL) {
    if (curr->name == name) {
      value = curr->value;
      break;
    }
    curr = curr->next;
  }

  return value;
}

void EspConfig::setValue(String name, String value) {
  if (first == NULL) {
    first = new ConfigList();
    first->name = name;
    first->value = value;
    first->next = NULL;
  } else {
    // search all
    ConfigList *curr = first;
    while (curr != NULL) {
      if (curr->name == name) {
        if (curr->value == value)
          return;
        curr->value = value;
        break;
      }

      // not in list yet
      if (curr->next == NULL) {
        curr = new ConfigList();
        curr->name = name;
        curr->value = value;
        curr->next = first;
        first = curr;
        break;
      }
      curr = curr->next;
    }
  }

  configChanged = true;
}

void EspConfig::unsetValue(String name) {
  bool deleted = false;

  ConfigList *curr = first, *last = first;
  while (curr != NULL) {
    if (curr->name == name) {
      if (curr == first)
        first = curr->next;
      else
        last->next = curr->next;
      delete (curr);
      deleted = true;
      break;
    }
    if (curr != first)
      last = curr;
    curr = curr->next;
  }

  if (deleted)
    configChanged = true;
}

void EspConfig::unsetAll() {
  bool deleted = false;

  ConfigList *curr = first;
  while (curr != NULL) {
    first = curr->next; 
    delete (curr);
    curr = first;
    deleted = true;
  }

  if (deleted)
    configChanged = true;
}

bool EspConfig::saveToFile() {
  bool result = false;

  if (!configChanged)
    return true;

  if (openWrite()) {
    ConfigList *curr = first;

    while (curr != NULL) {
      String data = "'" + curr->name + "' = '" + curr->value + "'\n";
      configFile.write((uint8_t*)data.c_str(), data.length());
      curr = curr->next;
    }
    configFile.flush();
    configFile.close();
    configChanged = false;
    result = true;
  } else
    Serial.println("saveToFile: " + fileName() + " failed!");

  return result;
}

EspDeviceConfig EspConfig::getDeviceConfig(String deviceName) {
  return EspDeviceConfig(deviceName);
}

// class EspDeviceConfig
EspDeviceConfig::EspDeviceConfig(String deviceName) : EspConfig(deviceName) {
}

#endif  // ESP8266

