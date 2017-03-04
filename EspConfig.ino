#include "Arduino.h"

#include "EspConfig.h"

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
  return (configFile = configFile = SPIFFS.open(fileName().c_str(), "r"));
}

bool EspConfig::openWrite() {
  return (configFile = configFile = SPIFFS.open(fileName().c_str(), "w"));
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
  ConfigList *curr = first, *last = first;
  while (curr != NULL) {
    if (curr->name == name) {
      if (curr == first)
        first = curr->next;
      else
        last->next = curr->next;
      delete (curr);
      break;
    }
    if (curr != first)
      last = curr;
    curr = curr->next;
  }
}

void EspConfig::unsetAll() {
  ConfigList *curr = first;
  while (curr != NULL) {
    first = curr->next; 
    delete (curr);
    curr = first;
  }
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
  }

  return result;
}

