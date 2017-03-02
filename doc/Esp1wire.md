**class Esp1wire**

|| method | description |
| --- | --- | --- |
| bool | **probeI2C(uint8_t sda, uint8_t scl)** | scan i2c bus for DS2482-100/800 bus master chips |
| | sda | gpio pin for SDA (default SDA) |
| | scl | gpio pin for SCL (default SCL) |
| bool | **probeGPIO(uint8_t gpio)** | probe gpio for 1-wire devices |
| | gpio | gpio pin (default 0) |
| bool | **resetSearch()** | start scanning each detected 1-wire bus |
| [AlarmFilter](./AlarmFilter.md) | **alarmSearch(DeviceType targetSearch)** | perform a conditional search on each detected 1-wire bus |
| | targetSearch | enable FAMILY CODE filter (just implemented for DeviceTypeSwitch yet) |

| **DeviceType** | |
| --- | --- |
| DeviceTypeUnsupported | all not implemeted devices |
| DeviceTypeTemperature | several temperature devices (DS1822, DS1825, DS18S20, DS18B20) |
| DeviceTypeSwitch | DS2406/7 (family code 0x12) |
| DeviceTypeCounter | DS2423 (family code 0x08) |
| DeviceTypeBattery | DS2438 (family code 0x26) |
| DeviceTypeAll | all supported devices |