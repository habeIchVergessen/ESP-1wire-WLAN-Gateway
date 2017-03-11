#ifndef _ESP_WIFI_H
#define _ESP_WIFI_H

#include "ESP8266WebServer.h"

typedef String (*DeviceListCallback) ();
typedef String (*DeviceConfigCallback) (ESP8266WebServer *server);

#endif	// _ESP_WIFI_H
