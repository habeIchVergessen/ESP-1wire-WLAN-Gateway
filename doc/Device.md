**class [Esp1wire](./Esp1wire.md)::Device**

###public###
| | method | description |
| --- | --- | --- |
| String | **getName()** | user friendly device type |
| DeviceType | **getDeviceType()** | device type |
| String | **getOneWireDeviceID()** | user friendly device id | 
| uint8_t | **getOneWireDeviceType()** | device id |
| bool | **getIgnoreAlarmSearch()** | device setting for alarm search excluded |
| void | **setIgnoreAlarmSearch(bool ignore)** | modify device setting for alarm search excluded |

###protected###
| | member |
| --- | --- |
|uint8_t | mAddress[8] |
| uint8_t | mStatus = 0 |
| Bus | *mBus |
| DeviceType | mDeviceType |
