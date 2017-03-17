#ifdef _MQTT_SUPPORT

#include "EspMqtt.h"

bool espMqttInitDone = false;

void loopEspMqtt() {
  if (!espMqttInitDone && (WiFi.status() == WL_CONNECTED)) {
    espMqttInitDone = true;
    espMqtt.sendAlive();
  }
}

EspMqtt::EspMqtt(String mqttClientName) {
  mMqttClientName   = mqttClientName + "@" + getChipID();
  mMqttTopicPrefix  = mqttClientName + "/" + getChipID() + "/";

  mqttClient.setClient(wifiClient);
}

bool EspMqtt::testConfig(String server, String port, String user, String password) {
  bool result = (server == "" && port == "");
  
  if ((server == "" && port == "") || ((result = connect(server, port, user, password, true)) && sendAlive())) {
    espConfig.setValue(F("mqttServer"), server);
    espConfig.setValue(F("mqttPort"), port);
    espConfig.setValue(F("mqttUser"), user);
    espConfig.setValue(F("mqttPassword"), password);
    espConfig.saveToFile();
  }

  return result;
}

bool EspMqtt::isConnected() {
  return mqttClient.connected();
}

bool EspMqtt::connect() {
  return connect(espConfig.getValue(F("mqttServer")), espConfig.getValue(F("mqttPort")), espConfig.getValue(F("mqttUser")), espConfig.getValue(F("mqttPassword")));
}

bool EspMqtt::connect(String server, String port, String user, String password, bool reconnect) {
  if (server == "" || port == "" || (mLastConnectAttempFailed && !reconnect))
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
  Serial.print("EspMqtt::publish: topic = '" + mMqttTopicPrefix + topic + "' value '" + value + "'");
  unsigned long pubStart = micros();
#endif

  mqttClient.publish(String(mMqttTopicPrefix + topic).c_str(), value.c_str());

#if defined(_DEBUG_MQTT) || defined(_DEBUG_TIMING)
  Serial.println(" " + elapTime(pubStart));
#endif

  if (!keepConnection)
    disconnect();
}

bool EspMqtt::sendAlive() {
  return publish(F("Status"), F("Alive"));
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

