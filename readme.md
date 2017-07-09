**OneWire library for ESP8266**

supported 1-wire devices 
- DS1822, DS1825, DS18S20, DS18B20
  - calculate and read temperature
  - set alarm temperature
- DS2406/7
  - configure conditional search options
  - read device state
- DS2423
  - read device state
- DS2438
  - calculate and read temperature
  - calculate and read battery state


missing features
- DS2406/7
  - direct read and write
- DS2438
  - calibration and offset within calculations

[documentation](./doc/Esp1wire.md)

environment
[Arduino IDE v1.6.13](https://www.arduino.cc/download_handler.php?f=/arduino-1.6.13-windows.zip)

required libraries
[ESP8266 v2.3.0](https://github.com/esp8266/Arduino#installing-with-boards-manager) [DS2482 (without DS18B20-sources)](https://github.com/Pfannex/DS18B20_DS2482) [OneWire v2.3.3](https://github.com/PaulStoffregen/OneWire)
