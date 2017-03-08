#ifdef _MQTT_SUPPORT

bool espMqttInitDone = false;

void loopEspMqtt() {
  if (!espMqttInitDone && (WiFi.status() == WL_CONNECTED)) {
    espMqttInitDone = true;
    espMqtt.publish(PROGNAME, "Alive");
  }
}

#include "EspMqtt.h"

EspMqtt::EspMqtt(String mqttClientName) {
  mMqttClientName = mqttClientName + "@" + getChipID();

  mqttClient.setClient(wifiClient);
}

bool EspMqtt::testConfig(String server, String port, String user, String password) {
  bool result = false;
  
  if ((result = connect(server, port, user, password, true))) {
    espConfig.setValue("mqttServer", server);
    espConfig.setValue("mqttPort", port);
    espConfig.setValue("mqttUser", user);
    espConfig.setValue("mqttPassword", password);
//    espConfig.saveToFile();
  }

  return result;
}

bool EspMqtt::isConnected() {
  return mqttClient.connected();
}

bool EspMqtt::connect() {
  return connect(espConfig.getValue("mqttServer"), espConfig.getValue("mqttPort"), espConfig.getValue("mqttUser"), espConfig.getValue("mqttPassword"));
}

bool EspMqtt::connect(String server, String port, String user, String password, bool reconnect) {
  if (server == "" || port == "" || mLastConnectAttempFailed)
    return false;

  if (!reconnect && isConnected())
    return true;

  if (reconnect)
    disconnect();

#ifdef _DEBUG_MQTT
  Serial.print("connecting to MQTT-Broker: ");
  unsigned long connStart = micros();
#endif

  bool conn = false;
  mqttClient.setServer(parseIP(server), atoi(port.c_str()));
  if (!(conn = mqttClient.connect(mMqttClientName.c_str()))) {
    mLastConnectAttempFailed = true;
  }

#ifdef _DEBUG_MQTT
  Serial.println(String(conn ? "connected " : "failed") + elapTime(connStart));
#endif

  return conn;
}

void EspMqtt::disconnect() {
  mLastConnectAttempFailed = false;
  mqttClient.disconnect();
}

bool EspMqtt::publish(String deviceName, String attributeName, String attributeValue, bool keepConnection) {
  return publish(deviceName + "/" + attributeName, attributeValue, keepConnection);
}

bool EspMqtt::publish(String topic, String value, bool keepConnection) {
  if (!isConnected() && !connect())
    return false;

#if defined(_DEBUG_MQTT) || defined(_DEBUG_TIMING)
  Serial.print("EspMqtt::publish: topic = '" + topic + "' value '" + value + "'");
  unsigned long pubStart = micros();
#endif

  mqttClient.publish(String("/" + mMqttClientName + "/" + topic).c_str(), value.c_str());

#if defined(_DEBUG_MQTT) || defined(_DEBUG_TIMING)
  Serial.println(" " + elapTime(pubStart));
#endif

  if (!keepConnection)
    disconnect();
}

IPAddress EspMqtt::parseIP(String ip) {
  IPAddress MyIP;
  for (int i = 0; i < 4; i++){
    String x = ip.substring(0, ip.indexOf("."));
    MyIP[i] = x.toInt();
    ip.remove(0, ip.indexOf(".")+1); 
  }
  return MyIP;
}

#endif  // _MQTT_SUPPORT

