#ifdef ESP8266

#include "Arduino.h"
#include "EspWifi.h"
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

#if defined(_ESP1WIRE_SUPPORT) || defined(_ESPSERIALBRIDGE_SUPPORT)
DeviceConfigCallback deviceConfigCallback = NULL;
void registerDeviceConfigCallback(DeviceConfigCallback callback) {
  deviceConfigCallback = callback;
}
#endif

#ifdef _ESP1WIRE_SUPPORT
DeviceListCallback deviceListCallback = NULL;
void registerDeviceListCallback(DeviceListCallback callback) {
  deviceListCallback = callback;
}

DeviceConfigCallback scheduleConfigCallback = NULL;
void registerScheduleConfigCallback(DeviceConfigCallback callback) {
  scheduleConfigCallback = callback;
}

DeviceListCallback scheduleListCallback = NULL;
void registerScheduleListCallback(DeviceListCallback callback) {
  scheduleListCallback = callback;
}
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

// globals
String otaFileName;
File otaFile;
bool lastWiFiStatus = false;

// Multicast declarations
IPAddress ipMulti(239, 0, 0, 57);
unsigned int portMulti = 12345;      // local port to listen on

// net config changed (loop has to handle it)
bool netConfigChanged = false;

WiFiUDP WiFiUdp;
ESP8266WebServer server(80);

void setupEspWifi() {
  String hostname = espConfig.getValue("hostname");
  if (hostname != "")
    WiFi.hostname(hostname);

  DBG_PRINT("Hostname: "); DBG_PRINTLN(WiFi.hostname());
  DBG_PRINT("MAC:      "); DBG_PRINTLN(WiFi.macAddress());

  setupWifi();

  // init http server
  setupHttp();
}

void loopEspWifi() {
  // apply net config changes
  if (netConfigChanged) {
    // spend some time for http clients
    delay(2000);
    
    String hostname = espConfig.getValue("hostname");
    if (hostname != "")
      WiFi.hostname(hostname);
    setupWifi();
    
    yield();
    netConfigChanged = false;
  }
  
  statusWifi();

  unsigned int start = millis();
  // process http requests
  server.handleClient();
  if (httpRequestProcessed) {
    DBG_PRINTF("%d ms\n", (millis() - start));
    httpRequestProcessed = false;
  }
}

void setupWifi() {
  DBG_PRINT("starting WiFi ");

  if (WiFi.getMode() != WIFI_STA)
    WiFi.mode(WIFI_STA);

  // static ip
  IPAddress ip, mask, gw, dns;
  if (ip.fromString(espConfig.getValue("address")) && ip != INADDR_NONE &&
      mask.fromString(espConfig.getValue("mask")) && mask != INADDR_NONE
  ) {
    if (!gw.fromString(espConfig.getValue("gateway")))
      gw = INADDR_NONE;
    if (!dns.fromString(espConfig.getValue("dns")))
      dns = INADDR_NONE;
    DBG_PRINTF("using static ip %s mask %s %s\n", ip.toString().c_str(), mask.toString().c_str(), String(WiFi.config(ip, gw, mask, dns) ? "ok" : "").c_str());
  } else
    DBG_PRINTF("using dhcp %s\n", String(WiFi.config(0U, 0U, 0U) ? "ok" : "").c_str());
    
  // force wait for connect
  lastWiFiStatus = !(WiFi.status() == WL_CONNECTED);
  statusWifi(true);
}

void statusWifi() {
  statusWifi(false);
}

void statusWifi(bool reconnect) {
  bool connected = (WiFi.status() == WL_CONNECTED);

  if (connected == lastWiFiStatus)
    return;
    
  if (!connected && reconnect) {
    byte retries = 20;
    while (retries > 0 && WiFi.status() != WL_CONNECTED) {
      retries--;
      delay(100);
    }
  }

  lastWiFiStatus = (WiFi.status() == WL_CONNECTED);
  if(lastWiFiStatus) {
    DBG_PRINT("Wifi connected: ");
    DBG_PRINT(WiFi.localIP());
    DBG_PRINT("/");
    DBG_PRINT(WiFi.subnetMask());
    DBG_PRINT(" ");
    DBG_PRINTLN(WiFi.gatewayIP());
    // trigger KVPUDP to reload config
    sendMultiCast("REFRESH CONFIG REQUEST");
  } else {
    DBG_PRINTLN("Wifi not connected");
  }

  setupSoftAP();
}

void setupSoftAP() {
  bool run = (WiFi.status() != WL_CONNECTED);
  bool softAP = (ipString(WiFi.softAPIP()) != "0.0.0.0");
  
  if (run && !softAP) {
    DBG_PRINT("starting SoftAP: ");
  
    String apSsid = PROGNAME;
    apSsid += "@" + getChipID();
    WiFi.softAP(apSsid.c_str(), getChipID().c_str());
    
    DBG_PRINTLN("done (" + ipString(WiFi.softAPIP()) + ")");
  }

  if (!run && softAP) {
    DBG_PRINT("stopping SoftAP: ");
  
    WiFi.softAPdisconnect(true);
    
    DBG_PRINTLN("done");
  }
}

void configWifi() {
  String ssid = server.arg("ssid");
  
  if (WiFi.SSID() != ssid && ssid == "") {
    WiFi.disconnect();  // clear ssid and psk in EEPROM
    delay(1000);
    statusWifi();
  }
  if (WiFi.SSID() != ssid && ssid != "") {
    reconfigWifi(ssid, server.arg("password"));
  }

  httpRequestProcessed = true;
}

void reconfigWifi(String ssid, String password) {
  if (ssid != "" && (WiFi.SSID() != ssid || WiFi.psk() != password)) {
    DBG_PRINT(" apply new config (" + ssid + " & " + password.length() + " bytes psk)");

    if (WiFi.status() == WL_CONNECTED) {
      WiFi.disconnect();
      delay(1000);
    }
    WiFi.begin(ssid.c_str(), password.c_str());
    statusWifi(true);
  }
}

void configNet() {
  String hostname = server.arg("hostname"), defaultHostname = getDefaultHostname();
  if (hostname == "" || hostname == defaultHostname) {
  // reset to default
    espConfig.unsetValue("hostname");
    if (defaultHostname != WiFi.hostname())
      WiFi.hostname(defaultHostname);
  } else
    espConfig.setValue("hostname", hostname.c_str());
  
  IPAddress ip, mask, gw, dns;

  if (!ip.fromString(server.arg("address")) || !mask.fromString(server.arg("mask"))) {
    DBG_PRINT("configNet: clear config ip/mask invalid ");
    ip = mask = INADDR_NONE;
    espConfig.unsetValue("address");
    espConfig.unsetValue("mask");
    espConfig.unsetValue("gateway");
    espConfig.unsetValue("dns");
  } else {
    DBG_PRINT("configNet: apply config ");
    if (ip != INADDR_NONE) 
      espConfig.setValue("address", ip.toString());
    else
      espConfig.unsetValue("address");
    if (mask != INADDR_NONE)
      espConfig.setValue("mask", mask.toString());
    else
      espConfig.unsetValue("mask");
    if (gw.fromString(server.arg("gateway")))
      espConfig.setValue("gateway", gw.toString());
    else
      espConfig.unsetValue("gateway");
    if (dns.fromString(server.arg("dns")))
      espConfig.setValue("dns", dns.toString());
    else
      espConfig.unsetValue("dns");
  }
  
  if (espConfig.hasChanged()) {
    DBG_PRINT("saving ");
    espConfig.saveToFile();

    // apply next loop
    netConfigChanged = true;
  }

  DBG_PRINT("\n");
}

void setupHttp() {
//  if (MDNS.begin(WiFi.hostname().c_str()))
//    DBG_PRINTLN("MDNS responder started");

  DBG_PRINT("starting WebServer");

  server.on("/", HTTP_GET, httpHandleRoot);

  server.on("/config", HTTP_GET, httpHandleConfig);
  server.on("/config", HTTP_POST, httpHandleConfig);
#ifdef _ESP1WIRE_SUPPORT
  server.on("/devices", HTTP_GET, httpHandleDevices);
  server.on("/schedules", HTTP_GET, httpHandleSchedules);
#endif  // _ESP1WIRE_SUPPORT
  server.on("/static/deviceList.css", HTTP_GET, httpHandleDeviceListCss);
  server.on("/static/deviceList.js", HTTP_GET, httpHandleDeviceListJss);
  server.onNotFound(httpHandleNotFound);
  server.addHandler(new FunctionRequestHandler(httpHandleOTA, httpHandleOTAData, ("/ota/" + getChipID() + ".bin").c_str(), HTTP_POST));
#ifdef _OTA_ATMEGA328_SERIAL
  server.addHandler(new FunctionRequestHandler(httpHandleOTAatmega328, httpHandleOTAatmega328Data, String("/ota/atmega328.bin").c_str(), HTTP_POST));
#endif

  server.begin();
  DBG_PRINTLN();
}

void httpHandleRoot() {
  DBG_PRINT("httpHandleRoot: ");
  // menu
  String message = F("<div class=\"menu\">");
#ifdef _ESPSERIALBRIDGE_SUPPORT
  message += F("<a id=\"serial\" class=\"dc\">Serial</a>");
#endif  // _ESPSERIALBRIDGE_SUPPORT
#ifdef _ESP1WIRE_SUPPORT
  message += F("<a href=\"/devices\" class=\"dc\">Devices</a><a href=\"/schedules\" class=\"dc\">Schedules</a>");
#endif  // _ESP1WIRE_SUPPORT
#ifdef _MQTT_SUPPORT
  message += "<a id=\"mqtt\" class=\"dc\">MQTT</a>";
#endif
  message += "<a id=\"ota\" class=\"dc\">OTA</a>";
  message += "<sp>" + uptime() + "</sp></div>";

  // wifi
  String netConfig = "<a id=\"net\" class=\"dc\">...</a>";
  String html = "<table><tr><td>ssid:</td><td>" + WiFi.SSID() + "</td><td><a id=\"wifi\" class=\"dc\">...</a></td></tr>";
  if (WiFi.status() == WL_CONNECTED) {
    html += F("<tr><td>Status:</td><td>connected</td><td></td></tr>");
    html += F("<tr><td>Hostname:</td><td>"); html += WiFi.hostname(); html += F(" (MAC: "); html += WiFi.macAddress();html += F(")</td><td rowspan=\"2\">");
    html += netConfig; html += F("</td></tr>");
    html += F("<tr><td>IP:</td><td>"); html += ipString(WiFi.localIP()); html += F("/"); html +=ipString(WiFi.subnetMask()); html += F(" "); html += ipString(WiFi.gatewayIP()); html += F("</td></tr>");
  } else {
    html += F("<tr><td>Status:</td><td>disconnected (MAC: ");
    html += WiFi.macAddress();
    html += F(")</td><td>"); html +=netConfig; html += F("</td></tr>");
  }
  html += F("</table>");
  message += htmlFieldSet(html, "WiFi");

  server.client().setNoDelay(true);
  server.send(200, "text/html", htmlBody(message));
  httpRequestProcessed = true;
}

void httpHandleConfig() {
  DBG_PRINT("httpHandleConfig: ");
  String message = "", separator = "";

  if (server.method() == HTTP_GET) {
    for (uint8_t i=0; i<server.args(); i++) {
      if (message.length() > 0)
        separator = ",";
        
      if (server.argName(i) == "Version") {
          message += separator + "Version:";
          message += PROGNAME;
          message += ".";
          message += PROGVERS;
      }
      if (server.argName(i) == "ChipID") {
          message += separator + "ChipID:" + getChipID();
      }
      if (server.argName(i) == "Dictionary") {
          message += separator + "Dictionary:" + getDictionary();
      }
    }
  }
  
  if (server.method() == HTTP_POST) {
    // check required parameter ChipID
    if (server.args() == 0 || server.argName(0) != "ChipID" || server.arg(0) != getChipID()) {
      server.client().setNoDelay(true);
      server.send(403, "text/plain", "Forbidden");
      httpRequestProcessed = true;
      return;
    }

#ifdef _DEBUG_HTTP
    for (int i=0; i<server.args();i++) {
      DBG_PRINTLN("#" + String(i) + " '" + server.argName(i) + "' = '" + server.arg(i) + "'");
    }
#endif

#ifdef _ESPSERIALBRIDGE_SUPPORT
    if (server.hasArg("serial") && (server.arg("serial") == "" || server.arg("serial") == "config")) {
      String result = "";
      uint16_t resultCode;
      if (deviceConfigCallback != NULL && (result = deviceConfigCallback(&server, &resultCode))) {
        server.client().setNoDelay(true);
        server.send(resultCode, "text/html", result);
        httpRequestProcessed = true;
        return;
      }

      server.client().setNoDelay(true);
      server.send(403, "text/plain", "Forbidden");
      httpRequestProcessed = true;
      return;
    }
#endif

#ifdef _ESP1WIRE_SUPPORT
    if (server.arg("deviceID") != "") {
      String result = "";
      uint16_t resultCode;
      if (deviceConfigCallback != NULL && (result = deviceConfigCallback(&server, &resultCode))) {
        server.client().setNoDelay(true);
        server.send(resultCode, "text/html", result);
        httpRequestProcessed = true;
        return;
      }

      server.client().setNoDelay(true);
      server.send(403, "text/plain", "Forbidden");
      httpRequestProcessed = true;
      return;
    }
    
    if (server.hasArg("schedule") && server.arg("schedule") != "") {
      uint16_t resultCode = 0;
      String result;
      
      if (scheduleConfigCallback != NULL)
        result = scheduleConfigCallback(&server, &resultCode);

      DBG_PRINT("resultCode: " + String(resultCode) + " ");
      switch (resultCode) {
        case 200:
          server.client().setNoDelay(true);
          server.send(resultCode, "text/html", result);
          break;
        case 303:
          server.client().setNoDelay(true);
          server.sendHeader("Location", "/schedules");
          server.send(303, "text/plain", "See Other");
          break;
        default:
          server.client().setNoDelay(true);
          server.send(403, "text/plain", "Forbidden");
          break;
      }

      httpRequestProcessed = true;
      return;
    }

    if (server.hasArg("resetSearch") && server.arg("resetSearch")== "") {
      esp1wire.resetSearch();
      server.client().setNoDelay(true);
      server.send(200, "text/html", "ok");
      httpRequestProcessed = true;
      return;
    }
#endif
    
    if (server.hasArg("ota") && server.arg("ota")== "") {
      String result = F("<h4>OTA</h4>");
      result += flashForm();
      server.client().setNoDelay(true);
      server.send(200, "text/html", result);
      httpRequestProcessed = true;
      return;
    }

#ifdef _OTA_ATMEGA328_SERIAL
    if (server.hasArg("ota-addon") && server.arg("ota-addon")== "") {
      String result = F("<h4>OTA Addon</h4>");
      result += flashAddonForm();
      server.client().setNoDelay(true);
      server.send(200, "text/html", result);
      httpRequestProcessed = true;
      return;
    }
#endif

    if (server.hasArg("wifi") && server.arg("wifi")== "") {
      String result = F("<h4>WiFi</h4>");
      result += wifiForm();
      server.client().setNoDelay(true);
      server.send(200, "text/html", result);
      httpRequestProcessed = true;
      return;
    }
    
    if (server.arg("wifi") == "submit") {
      configWifi();
      server.client().setNoDelay(true);
      server.sendHeader("Location", "/");
      server.send(303, "text/plain", "See Other");
      return;
    }

    if (server.hasArg("net") && server.arg("net")== "") {
      String result = F("<h4>Network</h4>");
      result += netForm();
      server.client().setNoDelay(true);
      server.send(200, "text/html", result);
      httpRequestProcessed = true;
      return;
    }
    
    if (server.arg("net") == "submit") {
      configNet();
      server.client().setNoDelay(true);
      server.sendHeader("Location", "/");
      server.send(303, "text/plain", "See Other");
      return;
    }

#ifdef _MQTT_SUPPORT
    if (server.hasArg("mqtt") && server.arg("mqtt") == "") {
      String result = F("<h4>MQTT</h4>");
      result += mqttForm();
      server.client().setNoDelay(true);
      server.send(200, "text/html", result);
      httpRequestProcessed = true;
      return;
    }
    
    if (server.arg("mqtt") == "submit") {
      if (espMqtt.testConfig(server.arg("server"), server.arg("port"), server.arg("user"), server.arg("password"))) {
        server.client().setNoDelay(true);
        server.send(200, "text/plain", "ok");
      } else {
        server.client().setNoDelay(true);
        server.send(304, "text/plain", "MQTT connection test failed!");
      }
      httpRequestProcessed = true;
      return;
    }
#endif

    for (uint8_t i=1; i<server.args(); i++) {
      int value, value2;
      bool hasValue = false, hasValue2 = false;

      char r = server.argName(i)[0];
      String values = server.arg(i);

      hasValue=(values.length() > 0);
      if (hasValue) {
        int idx=values.indexOf(',', 0);
        value=atoi(values.substring(0, (idx == -1 ? values.length() : idx)).c_str());

        hasValue2=(idx > 0);
        if (hasValue2)
          value2=atoi(values.substring(idx + 1).c_str());
      }
      handleInput(r, hasValue, value, hasValue2, value2);
    }
  }
  
  server.client().setNoDelay(true);
  server.send(200, "text/plain", message);
  httpRequestProcessed = true;
}

#ifdef _ESP1WIRE_SUPPORT
void httpHandleDevices() {
  DBG_PRINT("httpHandleDevices: ");
  String message = "", devList = "";

  if (server.method() == HTTP_GET) {
    if (deviceListCallback != NULL && (devList = deviceListCallback()) != "") {
#ifdef _DEBUG_TIMING
      unsigned long sendStart = micros();
#endif
      message = F("<div class=\"menu\"><a id=\"resetSearch\" class=\"dc\">resetSearch</a><sp><a id=\"back\" class=\"dc\">Back</a></sp></div>");
      String html = F("<table id=\"devices\"><thead><tr><th>Name</th><th>Type</th></tr></thead>");
      html += devList;
      html += F("</table>");
      String options = htmlOption("255", F("All"), true);
      options += htmlOption("18", F("Battery"));
      options += htmlOption("8", F("Counter"));
      options += htmlOption("4", F("Switch"));
      options += htmlOption("2", F("Temperature"));
      String legend = htmlSelect(F("filter"), options, F("javascript:filter();"));
      legend += F(" Devices");
      message += htmlFieldSet(html, legend);

      server.client().setNoDelay(true);
      server.send(200, "text/html", htmlBody(message));

#ifdef _DEBUG_TIMING
      DBG_PRINT("send " + elapTime(sendStart) + " ");
#endif
      httpRequestProcessed = true;
      return;
    }
  }
  
  server.client().setNoDelay(true);
  server.send(403, "text/plain", "Forbidden");
  httpRequestProcessed = true;
}

void httpHandleSchedules() {
  DBG_PRINT("httpHandleSchedules: ");
  String message = "", schedList = "";

  if (server.method() == HTTP_GET) {
    if (scheduleListCallback != NULL && (schedList = scheduleListCallback()) != "") {
#ifdef _DEBUG_TIMING
      unsigned long sendStart = micros();
#endif
      // menu
      message = F("<div class=\"menu\"><a class=\"dc\" id=\"schedule#add\">Add</a><a id=\"back\" class=\"dc\" style=\"float: right;\">Back</a></div>");
      // schedules
      message += htmlFieldSet(schedList, "Schedules");

      server.client().setNoDelay(true);
      server.send(200, "text/html", htmlBody(message));

#ifdef _DEBUG_TIMING
      DBG_PRINT("send " + elapTime(sendStart) + " ");
#endif
      httpRequestProcessed = true;
      return;
    }
  }
  
  server.client().setNoDelay(true);
  server.send(403, "text/plain", "Forbidden");
  httpRequestProcessed = true;
}
#endif  // _ESP1WIRE_SUPPORT

void httpHandleDeviceListCss() {
  DBG_PRINT("httpHandleDeviceListCss: ");
  server.sendHeader("Cache-Control", "public, max-age=86400");
  server.client().setNoDelay(true);
  String css = F(".menu{height:1.3em;padding:5px 5px 5px 5px;margin-bottom:5px;background-color:#E0E0E0;}.menu .dc{float:left;}.menu sp{float: right;}label{width:4em;text-align:left;display:inline-block;} input[type=text]{margin-bottom:2px;} table td{padding:5px 15px 0px 0px;} table th{text-align:left;} fieldset{margin:0px 10px 10px 10px;max-height:20em;overflow:auto;} legend select{margin-right:5px;}");
  css += F(".dc{border:1px solid #A0A0A0;border-radius:5px;padding:0px 3px 0px 3px;}a.dc{color:black;text-decoration:none;margin-right:3px;outline-style:none;}.dc:hover{border:1px solid #5F5F5F;background-color:#D0D0D0;cursor:pointer;} #mD{background:rgba(0,0,0,0.5);visibility:hidden;position:absolute;top:0;left:0;width:100%;height:100%;z-index:1;padding-top:10%;} #mDC{border:1px solid #A0A0A0;border-radius:5px;background:rgba(255,255,255,1);margin:auto;display:inline-block;} #mDCC label{margin-left:10px;width:8em;text-align:left;display:inline-block;} #mDCC select,input{margin:5px 10px 0px 10px;width:13em;display:inline-block;} #mDCC table td input{margin:0px 0px 0px 0px;width:0.8em;} #mDCC table th {font-size:smaller} #mDCB{float:right;margin:10px 10px 10px 10px;} #mDCB a{margin:0px 2px 0px 2px;}");
  server.send(200, "text/css", css);
  httpRequestProcessed = true;
}

void httpHandleDeviceListJss() {
  DBG_PRINT("httpHandleDeviceListJss: ");
  server.sendHeader("Cache-Control", "public, max-age=86400");
  server.client().setNoDelay(true);
  String script = F("function windowClick(e){if(e.target.className==\"dc\"&&e.target.id){modDlg(true,false,e.target.id);}}function modDlg(open,save,id){if(id=='back'){history.back();return;}document.onkeypress=(open?function(evt){evt=evt||window.event;var charCode=evt.keyCode||evt.which;if(charCode==27)modDlg(false,false);if(charCode==13)modDlg(false,true);}:null);var md=document.getElementById('mD');if(save){var form=document.getElementById('submitForm');if(form){form.submit();return;}form=document.getElementById('configForm');if(form){var aStr=form.action;var idx=aStr.indexOf('?');var url=aStr.substr(0, idx + 1);var params='';var elem;var parse;aStr=aStr.substr(idx + 1);while(1){idx=aStr.indexOf('&');if(idx>0)parse=aStr.substr(0, idx);else parse=aStr;");
  script += F("if(parse.substr(parse.length-1)!='='){params+=parse+'&';}else{elem=document.getElementsByName(parse.substr(0,parse.length-1));if(elem && elem[0])params+=parse+(elem[0].type!=\"checkbox\"?elem[0].value:(elem[0].checked?1:0))+'&';}if(idx>0) aStr=aStr.substr(idx+1); else break;}try{var xmlHttp=new XMLHttpRequest();xmlHttp.open('POST',url+params,false);xmlHttp.send(null);if(xmlHttp.status!=200){alert('Fehler: '+xmlHttp.statusText);return;}}catch(err){alert('Fehler: '+err.message);return;}}}if(open){try{var url='/config?ChipID=");
  script += getChipID();
  script += F("&action=form';if(id.indexOf('schedule#')==0)url+='&schedule='+id.substr(9);else if(id=='mqtt'||id=='wifi'||id.indexOf('ota')==0||id=='resetSearch'||id=='serial'||id=='net')url+='&'+id+'=';else url+='&deviceID='+id;var xmlHttp=new XMLHttpRequest(); xmlHttp.open('POST',url,false);xmlHttp.send(null);if(xmlHttp.status != 200){alert('Fehler: '+xmlHttp.statusText);return;}if(id=='resetSearch'){window.location.reload();return;}document.getElementById('mDCC').innerHTML=xmlHttp.responseText;}catch(err){alert('Fehler: '+err.message);return;}}md.style.visibility=(open?'visible':'hidden');if(!open){document.getElementById('mDCC').innerHTML='';}}");
  script += F("function filter(){var filter=document.getElementsByName('filter')[0];var table=document.getElementById('devices');if(filter&&table){var trs=document.getElementsByTagName('tr');i=1;while(trs[i]){trs[i].style.display=((filter.value&trs[i].firstChild.nodeValue)==filter.value||filter.value==255?'table-row':'none');i++;}}}");
  server.send(200, "text/javascript", script);
  httpRequestProcessed = true;
}

void httpHandleNotFound() {
  DBG_PRINT("httpHandleNotFound: ");
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.client().setNoDelay(true);
  server.send(404, "text/plain", message);
  httpRequestProcessed = true;
}

boolean sendMultiCast(String msg) {
  boolean result = false;

  if (WiFi.status() != WL_CONNECTED)
    return result;

#ifndef _ESP_WIFI_UDP_MULTICAST_DISABLED
  if (WiFiUdp.beginPacketMulticast(ipMulti, portMulti, WiFi.localIP()) == 1) {
    WiFiUdp.write(msg.c_str());
    WiFiUdp.endPacket();
    yield();  // force ESP8266 background tasks (wifi); multicast requires approx. 600 µs vs. delay 1ms
    result = true;
  }
#endif

  return result;
}

String ipString(IPAddress ip) {
  return String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
}

String getChipID() {
  char buf[10];
  sprintf(buf, "%08d", (unsigned int)ESP.getChipId());
  return String(buf);
}

String getDefaultHostname() {
  uint8_t mac[6];
  char buf[11];
  WiFi.macAddress(mac);
  sprintf(buf, "ESP_%02X%02X%02X", mac[3], mac[4], mac[5]);

  return String(buf);
}

void printUpdateError() {
#if DBG_PRINTER != ' '
      Update.printError(DBG_PRINTER);
      DBG_FORCE_OUTPUT();
#endif
}

#ifdef _OTA_NO_SPIFFS

void httpHandleOTA() {
  String message = "\n\nhttpHandleOTA: ";
  bool doReset = false;

  HTTPUpload& upload = server.upload();

  message += upload.totalSize;
  message += " Bytes received, md5: " + Update.md5String();
  if (!Update.hasError()) {
    message += "\nstarting upgrade!";
    doReset = true;
  } else {
    message += "\nupgrade failed!";
    DBG_PRINTLN(message);
    printUpdateError();
    DBG_FORCE_OUTPUT();
    Update.end(true);
  }

  DBG_PRINTLN(message);
  DBG_FORCE_OUTPUT();

  server.client().setNoDelay(true);
  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "See Other");

  if (doReset) {
    delay(1000);
    ESP.reset();
  }
  httpRequestProcessed = true;
}

void httpHandleOTAData() {
  static HeaderBootMode1 otaHeader;
 
  // Upload 
  HTTPUpload& upload = server.upload();
  
  if (upload.status == UPLOAD_FILE_START) {
    DBG_PRINT("httpHandleOTAData: " + upload.filename);
    uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    if (!Update.begin(maxSketchSpace)) {
      printUpdateError();
      DBG_PRINTLN("ERROR: UPLOAD_FILE_START");
      DBG_FORCE_OUTPUT();
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    // first block with data
    if (upload.totalSize == 0) {
      memcpy(&otaHeader, &upload.buf[0], sizeof(HeaderBootMode1));
      DBG_PRINTF(", magic: 0x%0x, size: 0x%0x, speed: 0x%0x\n", otaHeader.magic, ((otaHeader.flash_size_speed & 0xf0) >> 4), (otaHeader.flash_size_speed & 0x0f));
      DBG_FORCE_OUTPUT();

      if (otaHeader.magic != 0xe9)
        ;
    }
    
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      printUpdateError();
      DBG_PRINTLN("ERROR: UPLOAD_FILE_WRITE");
      DBG_FORCE_OUTPUT();
  }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) { 
      DBG_PRINTF("Firmware size: %d", upload.totalSize);
    }
  } else {
    printUpdateError();
    DBG_PRINTLN("ERROR: UPLOAD_FILE_END");
    DBG_FORCE_OUTPUT();
  }
}

#endif  // _OTA_NO_SPIFFS

#if !defined(_OTA_NO_SPIFFS) || defined(_OTA_ATMEGA328_SERIAL)

bool initOtaFile(String filename, String mode) {
  SPIFFS.begin();
  otaFile = SPIFFS.open(filename, mode.c_str());

  if (otaFile)
    otaFileName = filename;

  return otaFile;
}

void clearOtaFile() {
  if (otaFile)
    otaFile.close();
  if (SPIFFS.exists(otaFileName))
    SPIFFS.remove(otaFileName);
  otaFileName = "";
}

#endif  // defined(_OTA_NO_SPIFFS) || defined(_OTA_ATMEGA328_SERIAL)

#ifndef _OTA_NO_SPIFFS

void httpHandleOTA() {
  String message = "\n\nhttpHandleOTA: ";
  bool doUpdate = false;
  
  if (SPIFFS.exists(otaFileName) && initOtaFile(otaFileName, "r")) {
    message += otaFile.name();
    message += + " (";
    message += otaFile.size();
    message += " Bytes) received!";
    if ((doUpdate = Update.begin(otaFile.size()))) {
      message += "\nstarting upgrade!";
    } else
      clearOtaFile();
  }

  Serial.println(message);

  server.client().setNoDelay(true);
  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "See Other");

  if (doUpdate) {
    Serial.print("starting Update: ");
    size_t written = Update.write(otaFile);
    clearOtaFile();

    if (!Update.end() || Update.hasError()) {
      Serial.println("failed!");
      Update.printError(Serial);
      Update.end(true);
    } else {
      Serial.println("ok, md5 is " + Update.md5String());
      Serial.println("restarting");
      delay(1000);
      ESP.reset();
    }
  }
  httpRequestProcessed = true;
}

void httpHandleOTAData() {
  static HeaderBootMode1 otaHeader;
 
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    Serial.print("httpHandleOTAData: " + upload.filename);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    // first block with data
    if (upload.totalSize == 0) {
      memcpy(&otaHeader, &upload.buf[0], sizeof(HeaderBootMode1));
      Serial.printf(", magic: 0x%0x, size: 0x%0x, speed: 0x%0x\n", otaHeader.magic, ((otaHeader.flash_size_speed & 0xf0) >> 4), (otaHeader.flash_size_speed & 0x0f));

      if (otaHeader.magic == 0xe9)
        initOtaFile("/ota/" + getChipID() + ".bin", "w");
    }
    Serial.print(".");
    if ((upload.totalSize % HTTP_UPLOAD_BUFLEN) == 20)
      Serial.println("\n");

    if (otaFile && otaFile.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Serial.println("\nwriting file " + otaFileName + " failed!");
      clearOtaFile();
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (otaFile) {
      bool uploadComplete = (otaFile.size() == upload.totalSize);
      
      Serial.printf("\nend: %s (%d Bytes)\n", otaFile.name(), otaFile.size());
      otaFile.close();
      
      if (!uploadComplete)     
        clearOtaFile();
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    Serial.printf("\naborted\n");
    clearOtaFile();
  }
}

#endif // _OTA_NO_SPIFFS

#ifdef _OTA_ATMEGA328_SERIAL

void clearParser() {
  if (intelHexFormatParser != NULL) {
    free(intelHexFormatParser);
    intelHexFormatParser = NULL;
  }
}

void httpHandleOTAatmega328() {
  String message = "\n\nhttpHandleOTAatmega328: ";
  bool doUpdate = false;
  
  if (SPIFFS.exists(otaFileName) && initOtaFile(otaFileName, "r")) {
    message += otaFile.name();
    message += + " (";
    message += otaFile.size();
    message += " Bytes) received!";
    doUpdate = true;
  } else
    message += "file doesn't exists (maybe wrong IntelHEX format parsed!)";

  DBG_PRINTLN(message);

  if (doUpdate) {
    DBG_PRINT("starting Update: ");
    DBG_FORCE_OUTPUT();

    uint8_t txPin = 1;
#ifdef _ESPSERIALBRIDGE_SUPPORT
    espSerialBridge.enableClientConnect(false);
    txPin = espSerialBridge.getTxPin();
#endif

    FlashATmega328 flashATmega328(2, txPin);

    flashATmega328.flashFile(&otaFile);

#ifdef _ESPSERIALBRIDGE_SUPPORT
    espSerialBridge.enableClientConnect();
#endif

    clearOtaFile();
  }

  server.client().setNoDelay(true);
  server.sendHeader("Location", "/");
  server.send(303, "text/plain", "See Other");
  httpRequestProcessed = true;
}

void httpHandleOTAatmega328Data() {
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    DBG_PRINT("httpHandleOTAatmega328Data: " + upload.filename);
    DBG_FORCE_OUTPUT();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    // first block with data
    if (upload.totalSize == 0) {
        initOtaFile("/ota/atmega328.bin", "w");
        intelHexFormatParser = new IntelHexFormatParser(&otaFile);
    }

    if (intelHexFormatParser == NULL)
      return;

    DBG_PRINT(".");
    if ((upload.totalSize % HTTP_UPLOAD_BUFLEN) == 20)
      DBG_PRINTLN("\n");

    if (!intelHexFormatParser->parse(upload.buf, upload.currentSize)) {
      DBG_PRINTLN("\nwriting file " + otaFileName + " failed!");
      DBG_FORCE_OUTPUT();

      clearParser();
      clearOtaFile();
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (otaFile) {
      bool uploadComplete = (otaFile.size() == intelHexFormatParser->sizeBinaryData() && intelHexFormatParser->isEOF());
      
      DBG_PRINTF("\nend: %s (%d Bytes)\n", otaFile.name(), otaFile.size());
      DBG_FORCE_OUTPUT();
      otaFile.close();
      
      clearParser();
      if (!uploadComplete)     
        clearOtaFile();
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    DBG_PRINTF("\naborted\n");
    DBG_FORCE_OUTPUT();

    clearParser();
    clearOtaFile();
  }
}

#endif  // _OTA_ATMEGA328_SERIAL

#endif  // ESP8266

