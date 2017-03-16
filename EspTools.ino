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

uint16_t  uptimeDays = 0;
uint8_t   lastDays = 0;

String uptime() {
  String result = "";

  unsigned long uptime = (millis() / 1000);

  uint16_t days = uptime / 86400;
  if (days != lastDays) {
    lastDays = days;
    uptimeDays++;
  }
  if (uptimeDays > 0)
    result += String(uptimeDays) + " day(s) ";
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

