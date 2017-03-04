#ifndef _ESP_CONFIG_H
#define _ESP_CONFIG_H

#include "Arduino.h"

#include "FS.h"

class EspConfig {
public:
  EspConfig(String appName);
  ~EspConfig();

  String  getValue(String name);
  void    setValue(String name, String value);
  void    unsetValue(String name);
  void    unsetAll();
  bool    saveToFile();

protected:
  typedef struct __attribute__((packed)) ConfigList
  {
    String      name, value;
    ConfigList  *next;
  };

  bool        configChanged = false;
  String      mAppName;
  ConfigList  *first = NULL;

  File configFile;

  String fileName() { return "/config/" + mAppName + ".cfg"; };
  bool openRead();
  bool openWrite();
};

#endif	// _ESP_CONFIG_H
