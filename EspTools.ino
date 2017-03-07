void setupEspTools() {
  pinMode(BUILTIN_LED, OUTPUT);
  setLed(false);
}

void setLed(bool on) {
  digitalWrite(BUILTIN_LED, !on);
}

void blinkLed() {
  blinkLed(1);
}

void blinkLed(byte cnt) {
  blinkLed(cnt, 100);
}

void blinkLed(byte cnt, byte microseconds) {
  for (int i=cnt * 2 - 1; i>=0; i--) {
    setLed((i % 2));
    delayMicroseconds(microseconds);
  }
}

uint8_t EspToolsUptimeWeeks = 0;
uint8_t EspToolsUptimeDays = 0;

String uptime() {
  String result = "";

  unsigned long uptime = (millis() / 1000);

  unsigned long days = (unsigned long)(uptime / 86400);
  if (EspToolsUptimeDays != days) {
    EspToolsUptimeDays = days;
    if ((days % 7) == 0 && days > 0)
      EspToolsUptimeWeeks++;
  }
  if (EspToolsUptimeWeeks > 0)
    result += String(EspToolsUptimeWeeks) + " week(s) ";

  unsigned long dayOfWeek = (days % 7);
  if (dayOfWeek > 0)
    result += String(dayOfWeek) + " day(s) ";
    
  uptime %= 86400;
  uint8_t hours = uptime / 3600;
  result += String(hours < 10 ? String("0") + hours : hours) + ":";
  uptime %= 3600;
  uint8_t minutes = uptime / 60;
  result += String(minutes < 10 ? String("0") + minutes : minutes) + ".";
  uptime %= 60;
  result += String(uptime < 10 ? String("0") + uptime : uptime);

  return result;
}

