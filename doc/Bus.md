**class [Esp1wire](./Esp1wire.md)::Bus**

***public***

| | method | description |
| --- | --- | --- |
| void | **registerTemperatureDevice(bool parasite, uint8_t resolution)** | |
| bool | **parasite()** | any device in parasite mode present |
| bool | **powerSupply()** | current parasite state |
| uint16_t | **getDeviceCount()** | count of detected devices |
| uint16_t | **getTemperatureDeviceCount()** | count of detected temperature devices |
| void | **resetIgnoreAlarmFlags(DeviceType deviceType)** | clear all ignoreAlarmFlags |
| | deviceType | default DeviceTypeAll |
| uint8_t | **crc8(uint8_t \*address, uint8_t len)** | calculate crc8 |
| | address | data to calculate |
| | len | amount of data to calculate |
| uint16_t | **crc16(uint8_t \*address, uint8_t len, uint16_t crc)** | calculate crc16 |
| | address | data to calculate |
| | len | amount of data to calculate |
| | crc | initial value - default 0 |
| void | **wireWriteBytes(uint8_t \*bytes, uint8_t len)** | write data to 1-wire bus |
        
***virtual public***

| | method | description |
| --- | --- | --- |
| DeviceList\* | **getFirstDevice()** | first element in device list |
| String | **getBusInformation()** | user friendly bus information |
| bool | **reset()** | send reset to 1-wire bus |
| bool | **resetSearch()** | perform a normal search on 1-wire bus |
| bool | **alarmSearch(AlarmFilter \*alarmFilter, DeviceType targetSearch)** | perform a conditional search on 1-wire bus |
| void | **wireResetSearch()** | OneWire\|DS2482 wireResetSearch |
| void | **wireSelect(uint8_t \*address)** | OneWire\|DS2482 wireSelect |
| | address | search result device address |
| void | **wireWriteByte(uint8_t b)** | OneWire\|DS2482 wireWriteByte |
| | b byte to write |
| uint8_t | **wireReadBit()** | OneWire\|DS2482 wireReadBit |
| void | **wireReadBytes(uint8_t \*data, uint16_t len)** | OneWire\|DS2482 wireReadBytes |
| | data | receive buffer |
| | len | amount of bytes to receive |
| void | **setPowerSupply(bool power)** | OneWire\|DS2482 enable/disable parasite mode |
| | power | on or off |

***protected***

| | method | description |
| --- | --- | --- |
| void | **deviceDetected(uint8_t \*address)** | process address found by resetSearch |
| int8_t | **addressCompare(uint8_t \*addr1, uint8_t \*addr2)** | compare device address |
| | addr1\|addr2 | addresses to compare |
| DeviceType | **getDeviceType(uint8_t \*address)** | map family code to DeviceType |


| | member |
| --- | --- |
| DeviceList | \*firstDevice |
| DeviceList | \*lastDevice |
| uint16_t | mDeviceListCount |
| uint16_t | mTemperatureDeviceCount |
| uint8_t | mStatus |

***inheritance***

[class BusIC](./BusIC.md)

[class BusGPIO](./BusGPIO.md)

