**class [Esp1wire](./Esp1wire.md)::TemperatureDevice : [Device](./Device.md)**

***public***

| | method | description |
| --- | --- | --- |
| bool | **getAlarmTemperatures(int8_t \*lowTemperature, int8_t \*highTemperature)** | read alarm temperature settings |
| bool | **setAlarmTemperatures(int8_t lowTemperature, int8_t highTemperature)** | set alarm temperature settings |
| bool | **readTemperatureC(float \*temperature)** | read temperature |
| bool | **requestTemperatureC(float \*temperature)** | calculate and read temperature |
| bool | **powerSupply()** | device use parasite mode |
| TemperatureResolution | **readResolution()** | read current temperature resolution |

***TemperatureResolution***

| name | description |
| --- | --- |
| resolutionUnknown | detection of resolution failed | 
| resolution9bit | |
| resolution10bit | |
| resolution11bit | |
| resolution12bit | |

***protected***

| | method | description |
| --- | --- | --- |
| bool | **readScratch(uint8_t data[9])** | read device configuration |
| bool | **writeScratch(uint8_t data[9])** | write device configuration | 
| bool | **readPowerSupply()** | read power supply from device (implicit called at device detection) |
