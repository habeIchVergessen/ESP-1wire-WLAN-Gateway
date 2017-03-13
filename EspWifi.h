#ifndef _ESP_WIFI_H
#define _ESP_WIFI_H

#include "ESP8266WebServer.h"

typedef String (*DeviceListCallback) ();
typedef String (*DeviceConfigCallback) (ESP8266WebServer *server, uint16_t *result);

#endif	// _ESP_WIFI_H
