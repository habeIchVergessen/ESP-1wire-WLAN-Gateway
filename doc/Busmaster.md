**class [Esp1wire](./Esp1wire.md)::Busmaster**

***public***
| | method | description |
| --- | --- | --- |
| DeviceList* | **getFirstDevice()** | first element in device list |
| String | **getBusInformation()** | user friendly bus information |
| bool | **resetSearch()** | perform a normal search on 1-wire bus |
| bool | **alarmSearch(AlarmFilter *alarmFilter, DeviceType targetSearch)** | perform a conditional search on 1-wire bus |
| bool | **wireReset()** | send reset to 1-wire bus |
| void | **wireResetSearch()** | OneWire\|DS2482 wireResetSearch |
| void | **wireSelect(uint8_t *address)** | OneWire\|DS2482 wireSelect |
| | address | search result device address |
| void | **wireWriteByte(uint8_t b)** | OneWire\|DS2482 wireWriteByte |
| | b byte to write |
| uint8_t | **wireReadBit()** | OneWire\|DS2482 wireReadBit |
| void | **wireReadBytes(uint8_t *data, uint16_t len) | OneWire\|DS2482 wireReadBytes |
| | data | receive buffer |
| | len | amount of bytes to receive |
| bool | **wireSearch(uint8_t *address, bool alarm, DeviceType targetSearch)** | DS2482 wireSearch |
| | address | search result |
| | alarm | normal/conditonal search |
| | targetSearch | filter family code |
| void | wireStrongPullup(bool pullup) | DS2482 enable/disable parasite mode |
| | pullup | on or off |
