#ifndef _ESP_WIFI_H
#define _ESP_WIFI_H

#ifdef ESP8266

#define _OTA_NO_SPIFFS  // don't use SPIFFS to store temporary uploaded data

#include "Arduino.h"

#include "ESP8266WebServer.h"
#include "ESP8266WiFi.h"
#include "WiFiClient.h"
#include "ESP8266mDNS.h"
#include "WiFiUDP.h"
#include "FS.h"
#include "detail/RequestHandlersImpl.h"

#include "EspConfig.h"

#ifdef _MQTT_SUPPORT
  #include "EspMqtt.h"
#endif

#ifdef _OTA_ATMEGA328_SERIAL
  #include "IntelHexFormatParser.h"
  #include "FlashATMega328Serial.h"

  IntelHexFormatParser *intelHexFormatParser = NULL;
#endif

extern "C" {
#include "user_interface.h"
}

#include "EspDebug.h"

class EspWiFi {
  public:
    typedef String (*DeviceListCallback) ();
    typedef String (*DeviceConfigCallback) (ESP8266WebServer *server, uint16_t *result);
    EspWiFi();
    static void setup();
    static void loop();
    void setupHttp(bool start=true);
    boolean sendMultiCast(String msg);
    static String getChipID();
    static String getDefaultHostname();

#if defined(_ESP1WIRE_SUPPORT) || defined(_ESPSERIALBRIDGE_SUPPORT) || defined(_ESPIWEAS_SUPPORT)
    void registerDeviceConfigCallback(DeviceConfigCallback callback) { deviceConfigCallback = callback; };
#endif
    
#if defined(_ESP1WIRE_SUPPORT) || defined(_ESPIWEAS_SUPPORT)
    void registerDeviceListCallback(DeviceListCallback callback) { deviceListCallback = callback; };
#endif  // _ESP1WIRE_SUPPORT || _ESPIWEAS_SUPPORT
    
#ifdef _ESP1WIRE_SUPPORT
    void registerScheduleConfigCallback(DeviceConfigCallback callback) { scheduleConfigCallback = callback; };
    void registerScheduleListCallback(DeviceListCallback callback) { scheduleListCallback = callback; };
#endif  // _ESP1WIRE_SUPPORT

  protected:
    String otaFileName;
    File otaFile;
    bool lastWiFiStatus = false;
    
    IPAddress ipMulti = IPAddress(239, 0, 0, 57);
    unsigned int portMulti = 12345;      // local port to listen on
    
    bool netConfigChanged = false;
    
    WiFiUDP WiFiUdp;
    ESP8266WebServer server;
    
    bool httpStarted = false;

#if defined(_ESP1WIRE_SUPPORT) || defined(_ESPSERIALBRIDGE_SUPPORT) || defined(_ESPIWEAS_SUPPORT)
    DeviceConfigCallback deviceConfigCallback = NULL;
#endif
    
#if defined(_ESP1WIRE_SUPPORT) || defined(_ESPIWEAS_SUPPORT)
    DeviceListCallback deviceListCallback = NULL;
#endif  // _ESP1WIRE_SUPPORT || _ESPIWEAS_SUPPORT
    
#ifdef _ESP1WIRE_SUPPORT
    DeviceConfigCallback scheduleConfigCallback = NULL;
    DeviceListCallback scheduleListCallback = NULL;
#endif  // _ESP1WIRE_SUPPORT
    
    // source: http://esp8266-re.foogod.com/wiki/SPI_Flash_Format
    typedef struct __attribute__((packed))
    {
      uint8   magic;
      uint8   unknown;
      uint8   flash_mode;
      uint8   flash_size_speed;
      uint32  entry_addr;
    } HeaderBootMode1;

    void setupInternal();
    void loopInternal();
    void setupWifi();
    void statusWifi(bool reconnect=false);
    void setupSoftAP();
    void configWifi();
    void reconfigWifi(String ssid, String password);
    void configNet();

    String ipString(IPAddress ip);
    void printUpdateError();

    void httpHandleRoot();
    void httpHandleConfig();

    void httpHandleDeviceListCss();
    void httpHandleDeviceListJss();
    void httpHandleNotFound();

#ifdef _ESPIWEAS_SUPPORT
    void httpHandleDevices();
#endif

#ifdef _ESP1WIRE_SUPPORT
    void httpHandleDevices();
    void httpHandleSchedules();
#endif

#if !defined(_OTA_NO_SPIFFS) || defined(_OTA_ATMEGA328_SERIAL)
    bool initOtaFile(String filename, String mode);
    void clearOtaFile();
#endif

    void httpHandleOTA();
    void httpHandleOTAData();

#ifdef _OTA_ATMEGA328_SERIAL
    void clearParser();
    void httpHandleOTAatmega328();
    void httpHandleOTAatmega328Data();
#endif
};

extern EspWiFi espWiFi;
extern bool optionsChanged;
extern bool httpRequestProcessed;

String getChipID() { return EspWiFi::getChipID(); };
String getDefaultHostname() { return EspWiFi::getDefaultHostname(); };

#endif  // ESP8266

#endif	// _ESP_WIFI_H
