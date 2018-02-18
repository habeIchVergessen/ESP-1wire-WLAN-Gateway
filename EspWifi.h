#ifndef _ESP_WIFI_H
#define _ESP_WIFI_H

#define _OTA_NO_SPIFFS  // don't use SPIFFS to store temporary uploaded data

#include "ESP8266WebServer.h"

#include "EspDebug.h"

typedef String (*DeviceListCallback) ();
typedef String (*DeviceConfigCallback) (ESP8266WebServer *server, uint16_t *result);

#endif	// _ESP_WIFI_H
