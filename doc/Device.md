**class [Esp1wire](./Esp1wire.md)::Device**

###public###
| | method | description |
| --- | --- | --- |
| String | **getName()** | user friendly device type |
| DeviceType | **getDeviceType()** | device type |
| String | **getOneWireDeviceID()** | user friendly device id | 
| uint8_t | **getOneWireDeviceType()** | device id |
| bool | **getIgnoreAlarmSearch()** | alarm search suppress setting |
| void | **setIgnoreAlarmSearch(bool ignore)** | modify alarm search suppress setting |

###protected###
| | member |
| --- | --- |
|uint8_t | mAddress[8] |
| uint8_t | mStatus = 0 |
| Bus | *mBus |
| DeviceType | mDeviceType |
