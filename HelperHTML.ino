#ifdef _MQTT_SUPPORT
  #include "EspConfig.h"
#endif

#define textMark         F("\"")
#define actionField      F(" action=")
#define enctypeField     F(" enctype=")
#define maxLengthField   F(" maxlength=")
#define methodField      F(" method=")
#define nameField        F(" name=")
#define typeField        F(" type=")
#define valueField       F(" value=")

String htmlBody(String html) {
  String doc = F("<html><body><style>label {width: 4em; text-align:left; display: inline-block;} input[type=text] { width: 20em; margin-bottom: 2px;} </style><center><div style=\"width: 40em;\">");
  doc += "<h1>"; doc += PROGNAME; doc += " v"; doc += PROGVERS; doc += "@" + getChipID() + "</h1>";
  doc += wifiForm();
#ifdef _MQTT_SUPPORT
  doc += mqttForm();
#endif
  doc += flashForm();
  html.replace("\n", "<br>");
  doc += html;
  doc += F("</div></center></body></html>");

  return doc;
}

String wifiForm() {
  struct station_config current_conf;
  
  String action = F("/config?ChipID=");
  action += getChipID();
  action += F("&wifi=submit");

  String html = htmlLabel("ssid", "ssid: ");
  html += htmlInput("ssid", "",  WiFi.SSID(), sizeof(current_conf.ssid)) + htmlNewLine();
  html += htmlLabel("password", "psk: ");
  html += htmlInput("password", "",  "", sizeof(current_conf.password)) + htmlNewLine();
  html += htmlButton("submit", "action", "setup", "Setup");
  html += htmlButton("submit", "action", "reset", "Reset");

  return htmlForm(html, action, "post", "", "Wifi");
}

#ifdef _MQTT_SUPPORT
String mqttForm() {
  String action = F("/config?ChipID=");
  action += getChipID();
  action += F("&mqtt=submit");

  String html = htmlLabel("server", "server: ");
  html += htmlInput("server", "", espConfig.getValue("mqttServer"), 40) + htmlNewLine();
  html += htmlLabel("port", "port: ");
  html += htmlInput("port", "", espConfig.getValue("mqttPort"), 5) + htmlNewLine();
  html += htmlLabel("user", "user: ");
  html += htmlInput("user", "", espConfig.getValue("mqttUser"), 40) + htmlNewLine();
  html += htmlLabel("password", "password: ");
  html += htmlInput("password", "", "", 40) + htmlNewLine();
  html += htmlButton("submit", "action", "setup", "Setup");
  html += htmlButton("submit", "action", "test", "Test");

  return htmlForm(html, action, "post", "", "MQTT");
}
#endif  // _MQTT_SUPPORT

String flashForm() {
  String action = F("/ota/");
  action += getChipID();
  action += F(".bin");

  String html = htmlLabel("file", "file: ");
  html += htmlInput("file", "file", "", 0) + htmlNewLine();
  html += htmlButton("submit", "action", "flash", "Flash");

  return htmlForm(html, action, "post", "multipart/form-data", "OTA");
}

String htmlForm(String html, String pAction, String pMethod, String pEnctype, String pLegend) {
  String result = F("<form");
  result += actionField;
  result += textMark;
  result += pAction;
  result += textMark;
  result += methodField;
  result += textMark;
  result += pMethod;
  result += textMark;
  if (pEnctype != "") {
    result += enctypeField;
    result += textMark;
    result += pEnctype;
    result += textMark;
  }
  result += F(">");
  result += F("<fieldset>");
  if (pLegend != "") {
    result += F("<legend>");
    result += pLegend;
    result += F("</legend>");
  }
  result += html;
  result += F("</fieldset>");
  result += F("</form>");
  
  return result;
}

String htmlNewLine() {
  return F("<br>");
}

String htmlLabel(String pFor, String pText) {
  String result = F("<label for=");
  result += textMark;
  result += pFor;
  result += textMark;
  result += ">" + pText;
  result += F("</label>");

  return result;
}

String htmlInput(String pName, String pType, String pValue, int pMaxLength) {
  String result = F("<input ");
  result += nameField;
  result += textMark;
  result += pName;
  result += textMark;
  result += typeField;
  result += textMark;
  result += (pType != "" ? pType : "text");
  result += textMark;
  
  if (pValue != "") {
    result += valueField;
    result += textMark;
    result += pValue;
    result += textMark;
  }
  if (pMaxLength > 0) {
    result += maxLengthField;
    result += textMark;
    result += String(pMaxLength);
    result += textMark;
  }
  result += F(">");
  
  return result;
}

String htmlButton(String pType, String pName, String pValue, String pText) {
  String result = F("<button type=");
  result += textMark;
  result += pType;
  result += textMark;
  result += nameField;
  result += textMark;
  result += pName;
  result += textMark;
  result += valueField;
  result += textMark;
  result += pValue;
  result += textMark;
  result += F(">");
  result += pText;
  result += F("</button>");

  return result;
}

