
#ifndef _ESP_MQTT_H
#define _ESP_MQTT_H

#include "ESP8266WiFi.h"
#include "WiFiClient.h"

//#define MQTT_KEEPALIVE 120
#define MQTT_SOCKET_TIMEOUT 3

#include "PubSubClient.h"

#include "EspConfig.h"

class EspMqtt {
public:
  EspMqtt(String mqttClientName);

  bool          testConfig(String server, String port, String user, String password);
  bool          isConnected();
  bool          connect();
  void          disconnect();
  bool          publish(String topic, String value, bool keepConnection=true);
  bool          publish(String deviceName, String attributeName, String attributeValue, bool keepConnection=true);
  bool          sendAlive();

protected:
  bool          mLastConnectAttempFailed = false;
  String        mMqttClientName;
  String        mMqttTopicPrefix;
  WiFiClient    wifiClient;
  PubSubClient  mqttClient;

  bool          connect(String server, String port, String user, String password, bool reconnect=false);
  IPAddress     parseIP(String ip);
};

extern EspMqtt espMqtt;

#endif  // _ESP_MQTT_H
