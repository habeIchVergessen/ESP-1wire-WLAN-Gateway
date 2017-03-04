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

