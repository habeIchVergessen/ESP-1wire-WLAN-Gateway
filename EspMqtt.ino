#ifdef _MQTT_SUPPORT

#include "EspMqtt.h"

EspMqtt::EspMqtt(String mqttClientName) {
  mMqttClientName = mqttClientName;

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
  if (server == "" || port == "")
    return false;

  if (reconnect && isConnected())
    disconnect();
  else
    return true;

#ifdef _DEBUG_MQTT
  Serial.println("connecting to MQTT-Broker: ");
#endif
  bool conn = false;
  mqttClient.setServer(parseIP(server), atoi(port.c_str()));
  if ((conn = mqttClient.connect(mMqttClientName.c_str()))) {
#ifdef _DEBUG_MQTT
    Serial.println("MQTT connected: " + server + ":" + port);
#endif
  }

  return conn;
}

void EspMqtt::disconnect() {
  mqttClient.disconnect();
}

bool EspMqtt::publish(String topic, String state, bool keepConnection) {
  if (!isConnected() && !connect())
    return false;
    
  mqttClient.publish(topic.c_str(), state.c_str());

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

