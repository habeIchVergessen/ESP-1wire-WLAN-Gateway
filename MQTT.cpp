#include <Arduino.h>
#include "MQTT.h"


MQTT_Client::MQTT_Client() : mqtt_client(wifi_client){
  //Callback                
  //mqtt_client.setCallback(std::bind(&ESP8266_Basic::mqttBroker_Callback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void MQTT_Client::startMQTT(){
  Serial.println("connecting to MQTT-Broker: ");
  mqtt_client.disconnect();
  mqtt_client.setServer(strToIP(espConfig.getValue("mqttServer")), atoi(espConfig.getValue("mqttPort").c_str()));
  if (mqtt_client.connect(WiFi.hostname().c_str())) {
    Serial.print("MQTT connected: ");
    Serial.print(espConfig.getValue("mqttServer"));
    Serial.print(":");Serial.println(espConfig.getValue("mqttPort"));
  }
}

IPAddress MQTT_Client::strToIP(String IP) {
  IPAddress MyIP;
  for (int i = 0; i < 4; i++){
    String x = IP.substring(0, IP.indexOf("."));
    MyIP[i] = x.toInt();
    IP.remove(0, IP.indexOf(".")+1); 
  }
  return MyIP;
}
