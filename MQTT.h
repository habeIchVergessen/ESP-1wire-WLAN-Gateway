#pragma once
//#ifdef _MQTT_SUPPORT
  #include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino  
  #include <WiFiClient.h> 
  #include <PubSubClient.h>
  #include "EspConfig.h"
//#endif

//#ifdef _MQTT_SUPPORT
//#endif


class MQTT_Client{

public:
  MQTT_Client();
  WiFiClient wifi_client;
  PubSubClient mqtt_client;
  
  void startMQTT();
  
private:

  //helpers----------------------------
  IPAddress strToIP(String IP);
};
