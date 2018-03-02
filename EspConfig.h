#ifndef _ESP_CONFIG_H
#define _ESP_CONFIG_H

#include "Arduino.h"

#ifdef ESP8266

#include "FS.h"

class EspDeviceConfig;

class EspConfig {
public:
  EspConfig(String appName);
  ~EspConfig();

  String  getValue(String name);
  void    setValue(String name, String value);
  void    unsetValue(String name);
  void    unsetAll();
  bool    saveToFile();
  bool    hasChanged() { return configChanged; };
  
  EspDeviceConfig   getDeviceConfig(String deviceName);
  
protected:
  typedef struct __attribute__((packed)) ConfigList
  {
    String      name, value;
    ConfigList  *next;
  };

  bool          configChanged = false;
  ConfigList    *first = NULL;
  String        mAppName;

  File configFile;

  String fileName() { return "/config/" + mAppName + ".cfg"; };
  bool openRead();
  bool openWrite();
};

class EspDeviceConfig : public EspConfig {
public:
  EspDeviceConfig(String deviceName);
};

extern EspConfig espConfig;

#endif  // ESP8266

#endif	// _ESP_CONFIG_H
