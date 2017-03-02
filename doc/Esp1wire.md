# class Esp1wire
** public **

| method | description |
| --- | --- |
| bool probeI2C(uint8_t sda, uint8_t scl) | scan i2c bus for DS2482-100/800 bus master chips |
| | parameter sda - gpio pin for SDA (default SDA) |
| | parameter scl - gpio pin for SCL (default SCL) |
| bool probeGPIO(uint8_t gpio) | probe gpio for 1-wire devices |
| | parameter gpio - gpio pin (default 0) |
| bool resetSearch() | start scanning each detected 1-wire bus |
| AlarmFilter alarmSearch(DeviceType targetSearch) | perform a conditional search on each detected 1-wire bus |
| | parameter targetSearch - enable FAMILY CODE filter (only implemented for DeviceTypeSwitch yet) |