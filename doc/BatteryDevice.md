**class [Esp1wire](./Esp1wire.md)::BatteryDevice : [Device](./Device.md)**

***public***

| | method | description |
| --- | --- | --- |
| bool | **readTemperatureC(float \*temperature)** | read temperature |
| bool | **requestTemperatureC(float \*temperature)** | calculate and read temperature |
| bool | **readBattery(float \*voltage, float \*current, float \*capacity, float resistorSens)** | |
| | voltage/current/capacity | current values |
| | resistorSens | used for calculations of current and capacity (default 0.025 in Ohm) |
| bool | **requestBattery(float \*voltage, float \*current, float \*capacity, float resistorSens)** | calculate and read values |
