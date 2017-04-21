#include "Arduino.h"
#include "EspWifi.h"
#include "ESP8266WiFi.h"
#include "WiFiClient.h"
#include "ESP8266mDNS.h"
#include "WiFiUDP.h"
#include "FS.h"
#include "detail/RequestHandlersImpl.h"

#ifdef _MQTT_SUPPORT
  #include "EspConfig.h"
  #include "EspMqtt.h"
#endif

extern "C" {
#include "user_interface.h"
}

DeviceConfigCallback deviceConfigCallback = NULL;
void registerDeviceConfigCallback(DeviceConfigCallback callback) {
  deviceConfigCallback = callback;
}

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

WiFiUDP WiFiUdp;
ESP8266WebServer server(80);

void setupEspWifi() {
//  WiFi.hostname("bla");

  Serial.print("Hostname: "); Serial.println(WiFi.hostname());
  Serial.print("MAC:      "); Serial.println(WiFi.macAddress());

  setupWifi();

  // init http server
  setupHttp();
}

void loopEspWifi() {
  statusWifi();

  unsigned int start = millis();
  // process http requests
  server.handleClient();
  if (httpRequestProcessed) {
    Serial.printf("%d ms\n", (millis() - start));
    httpRequestProcessed = false;
  }
}

void setupWifi() {
  Serial.println("starting WiFi");

  if (WiFi.getMode() != WIFI_STA)
    WiFi.mode(WIFI_STA);

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
    Serial.print("Wifi connected: ");
    Serial.print(WiFi.localIP());
    Serial.print("/");
    Serial.print(WiFi.subnetMask());
    Serial.print(" ");
    Serial.println(WiFi.gatewayIP());
    // trigger KVPUDP to reload config
    sendMultiCast("REFRESH CONFIG REQUEST");
  } else {
    Serial.println("Wifi not connected");
  }

  setupSoftAP();
}

void setupSoftAP() {
  bool run = (WiFi.status() != WL_CONNECTED);
  bool softAP = (ipString(WiFi.softAPIP()) != "0.0.0.0");
  
  if (run && !softAP) {
    Serial.print("starting SoftAP: ");
  
    String apSsid = PROGNAME;
    apSsid += "@" + getChipID();
    WiFi.softAP(apSsid.c_str(), getChipID().c_str());
    
    Serial.println("done (" + ipString(WiFi.softAPIP()) + ")");
  }

  if (!run && softAP) {
    Serial.print("stopping SoftAP: ");
  
    WiFi.softAPdisconnect(true);
    
    Serial.println("done");
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
    Serial.print(" apply new config (" + ssid + ")");

    if (WiFi.status() == WL_CONNECTED) {
      WiFi.disconnect();
      delay(1000);
    }
    WiFi.begin(ssid.c_str(), password.c_str());
    statusWifi(true);
  }
}

void setupHttp() {
//  if (MDNS.begin(WiFi.hostname().c_str()))
//    Serial.println("MDNS responder started");

  Serial.print("starting WebServer");

  server.on("/", HTTP_GET, httpHandleRoot);

  server.on("/config", HTTP_GET, httpHandleConfig);
  server.on("/config", HTTP_POST, httpHandleConfig);
  server.on("/devices", HTTP_GET, httpHandleDevices);
  server.on("/schedules", HTTP_GET, httpHandleSchedules);
  server.on("/static/deviceList.css", HTTP_GET, httpHandleDeviceListCss);
  server.on("/static/deviceList.js", HTTP_GET, httpHandleDeviceListJss);
  server.onNotFound(httpHandleNotFound);
  server.addHandler(new FunctionRequestHandler(httpHandleOTA, httpHandleOTAData, ("/ota/" + getChipID() + ".bin").c_str(), HTTP_POST));

  server.begin();
  Serial.println();
}

void httpHandleRoot() {
  Serial.print("httpHandleRoot: ");
  // menu
  String message = F("<div class=\"menu\"><a href=\"/devices\" class=\"dc\">Devices</a><a href=\"/schedules\" class=\"dc\">Schedules</a>");
#ifdef _MQTT_SUPPORT
  message += "<a id=\"mqtt\" class=\"dc\">MQTT</a>";
#endif
  message += "<a id=\"ota\" class=\"dc\">OTA</a>";
  message += "<sp>" + uptime() + "</sp></div>";

  // wifi
  String html = "<table><tr><td>ssid:</td><td>" + WiFi.SSID() + "</td><td><a id=\"wifi\" class=\"dc\">...</a></td></tr>";
  if (WiFi.status() == WL_CONNECTED) {
    html += F("<tr><td>Status:</td><td>connected</td><td></td></tr>");
    html += F("<tr><td>Hostname:</td><td>"); html += WiFi.hostname(); html += F(" (MAC: "); html += WiFi.macAddress();html += F(")</td><td></td></tr>");
    html += F("<tr><td>IP:</td><td>"); html += ipString(WiFi.localIP()); html += F("/"); html +=ipString(WiFi.subnetMask()); html += F(" "); html += ipString(WiFi.gatewayIP()); html += F("</td><td></td></tr>");
  } else {
    html += F("<tr><td>Status:</td><td>disconnected</td><td></td></tr>");
  }
  html += F("</table>");
  message += htmlFieldSet(html, "WiFi");

  server.client().setNoDelay(true);
  server.send(200, "text/html", htmlBody(message));
  httpRequestProcessed = true;
}

void httpHandleConfig() {
  Serial.print("httpHandleConfig: ");
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
      Serial.println("#" + String(i) + " '" + server.argName(i) + "' = '" + server.arg(i) + "'");
    }
#endif

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

      Serial.print("resultCode: " + String(resultCode) + " ");
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
    
    if (server.hasArg("ota") && server.arg("ota")== "") {
      String result = F("<h4>OTA</h4>");
      result += flashForm();
      server.client().setNoDelay(true);
      server.send(200, "text/html", result);
      httpRequestProcessed = true;
      return;
    }
    
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

void httpHandleDevices() {
  Serial.print("httpHandleDevices: ");
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
      Serial.print("send " + elapTime(sendStart) + " ");
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
  Serial.print("httpHandleSchedules: ");
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
      Serial.print("send " + elapTime(sendStart) + " ");
#endif
      httpRequestProcessed = true;
      return;
    }
  }
  
  server.client().setNoDelay(true);
  server.send(403, "text/plain", "Forbidden");
  httpRequestProcessed = true;
}

void httpHandleDeviceListCss() {
  Serial.print("httpHandleDeviceListCss: ");
  server.sendHeader("Cache-Control", "public, max-age=86400");
  server.client().setNoDelay(true);
  String css = F(".menu{height:1.3em;padding:5px 5px 5px 5px;margin-bottom:5px;background-color:#E0E0E0;}.menu .dc{float:left;}.menu sp{float: right;}label{width:4em;text-align:left;display:inline-block;} input[type=text]{margin-bottom:2px;} table td{padding:5px 15px 0px 0px;} table th{text-align:left;} fieldset{margin:0px 10px 10px 10px;max-height:20em;overflow:auto;} legend select{margin-right:5px;}");
  css += F(".dc{border:1px solid #A0A0A0;border-radius:5px;padding:0px 3px 0px 3px;}a.dc{color:black;text-decoration:none;margin-right:3px;outline-style:none;}.dc:hover{border:1px solid #5F5F5F;background-color:#D0D0D0;cursor:pointer;} #mD{background:rgba(0,0,0,0.5);visibility:hidden;position:absolute;top:0;left:0;width:100%;height:100%;z-index:1;padding-top:10%;} #mDC{border:1px solid #A0A0A0;border-radius:5px;background:rgba(255,255,255,1);margin:auto;display:inline-block;} #mDCC label{margin-left:10px;width:8em;text-align:left;display:inline-block;} #mDCC select,input{margin:5px 10px 0px 10px;width:13em;display:inline-block;} #mDCB{float:right;margin:10px 10px 10px 10px;} #mDCB a{margin:0px 2px 0px 2px;}");
  server.send(200, "text/css", css);
  httpRequestProcessed = true;
}

void httpHandleDeviceListJss() {
  Serial.print("httpHandleDeviceListJss: ");
  server.sendHeader("Cache-Control", "public, max-age=86400");
  server.client().setNoDelay(true);
  String script = F("function windowClick(e){if(e.target.className==\"dc\"&&e.target.id){modDlg(true,false,e.target.id);}}function modDlg(open,save,id){if(id=='back'){history.back();return;}document.onkeypress=(open?function(evt){evt=evt||window.event;var charCode=evt.keyCode||evt.which;if(charCode==27)modDlg(false,false);if(charCode==13)modDlg(false,true);}:null);var md=document.getElementById('mD');if(save){var form=document.getElementById('submitForm');if(form){form.submit();return;}form=document.getElementById('configForm');if(form){var aStr=form.action;var idx=aStr.indexOf('?');var url=aStr.substr(0, idx + 1);var params='';var elem;var parse;aStr=aStr.substr(idx + 1);while(1){idx=aStr.indexOf('&');if(idx>0)parse=aStr.substr(0, idx);else parse=aStr;");
  script += F("if(parse.substr(parse.length-1)!='='){params+=parse+'&';}else{elem=document.getElementsByName(parse.substr(0,parse.length-1));if(elem && elem[0])params+=parse+elem[0].value+'&';}if(idx>0) aStr=aStr.substr(idx+1); else break;}try{var xmlHttp=new XMLHttpRequest();xmlHttp.open('POST',url+params,false);xmlHttp.send(null);if(xmlHttp.status!=200){alert('Fehler: '+xmlHttp.statusText);return;}}catch(err){alert('Fehler: '+err.message);return;}}}if(open){try{var url='/config?ChipID=");
  script += getChipID();
  script += F("&action=form';if(id.indexOf('schedule#')==0)url+='&schedule='+id.substr(9);else if(id=='mqtt'||id=='wifi'||id=='ota'||id=='resetSearch')url+='&'+id+'=';else url+='&deviceID='+id;var xmlHttp=new XMLHttpRequest(); xmlHttp.open('POST',url,false);xmlHttp.send(null);if(xmlHttp.status != 200){alert('Fehler: '+xmlHttp.statusText);return;}if(id=='resetSearch'){window.location.reload();return;}document.getElementById('mDCC').innerHTML=xmlHttp.responseText;}catch(err){alert('Fehler: '+err.message);return;}}md.style.visibility=(open?'visible':'hidden');if(!open){document.getElementById('mDCC').innerHTML='';}}");
  script += F("function filter(){var filter=document.getElementsByName('filter')[0];var table=document.getElementById('devices');if(filter&&table){var trs=document.getElementsByTagName('tr');i=1;while(trs[i]){trs[i].style.display=((filter.value&trs[i].firstChild.nodeValue)==filter.value||filter.value==255?'table-row':'none');i++;}}}");
  server.send(200, "text/javascript", script);
  httpRequestProcessed = true;
}

void httpHandleNotFound() {
  Serial.print("httpHandleNotFound: ");
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

