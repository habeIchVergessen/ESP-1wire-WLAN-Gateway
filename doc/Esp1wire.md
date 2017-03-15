**class Esp1wire**

***public***

| | method | description |
| --- | --- | --- |
| bool | **probeI2C(uint8_t sda, uint8_t scl)** | scan i2c bus for DS2482-100/800 bus master chips |
| | sda, scl | gpio pin for SDA, SCL (default SDA, SCL) |
| bool | **probeGPIO(uint8_t gpio)** | probe gpio for 1-wire devices |
| | gpio | gpio pin (default 0) |
| bool | **resetSearch()** | start scanning each detected 1-wire bus |
| [AlarmFilter](./AlarmFilter.md) | **alarmSearch(DeviceType targetSearch)** | perform a conditional search on each detected 1-wire bus |
| | targetSearch | enable FAMILY CODE filter (just implemented for DeviceTypeSwitch yet) |
| [DeviceFilter](./DeviceFilter.md) | **getDeviceFilter(DeviceType filter)** | device list iterator |
| | filter | default DeviceTypeAll |
| [TemperatureDeviceFilter](./TemperatureDeviceFilter.md) | **getTemperatureDeviceFilter()** | temperature device list iterator |
| uint8_t | **getBusCount()** | amount of detected 1-wire busses |

##### DeviceType

| name | description |
| --- | --- |
| DeviceTypeUnsupported | all not implemeted devices |
| DeviceTypeTemperature | several temperature devices (DS1822, DS1825, DS18S20, DS18B20) |
| DeviceTypeSwitch | DS2406/7 (family code 0x12) |
| DeviceTypeCounter | DS2423 (family code 0x08) |
| DeviceTypeBattery | DS2438 (family code 0x26) |
| DeviceTypeAll | all devices |

##### BusmasterType

| name | description |
| --- | --- |
| DS2482_100 | DS2482-100 |
| DS2482_800 | DS2482-800 |

[class Device](./Device.md)

[class BatteryDevice](./BatteryDevice.md) : Device

[class CounterDevice](./CounterDevice.md) : Device

[class SwitchDevice](./SwitchDevice.md) : Device

[class TemperatureDevice](./TemperatureDevice.md) : Device

***protected***

[class Busmaster](./Busmaster.md)

[class Bus](./Bus.md)

[class BusIC](./BusIC.md) : Bus

[class BusGPIO](./BusGPIO.md) : Bus

[class HelperDevice](./HelperDevice.md) : [Device](./Device.md)

[class HelperBatteryDevice](./HelperBatteryDevice.md) : [BatteryDevice](./BatteryDevice.md)

[class HelperCounterDevice](./HelperCounterDevice.md) : [CounterDevice](./CounterDevice.md)

[class HelperSwitchDevice](./HelperSwitchDevice.md) : [SwitchDevice](./SwitchDevice.md)

[class HelperTemperatureDevice](./HelperTemperatureDevice.md) : [TemperatureDevice](./TemperatureDevice.md)
