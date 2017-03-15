#include <Arduino.h>

#ifndef _KVP_Sensors_h

#define _KVP_Sensors_h

#define KEY_DELIMITER 			" "
#define KEY_VALUE_DELIMITER 	"="
#define VALUE_DELIMITER 		","
#define PORT_DELIMITER 			"."

#define DiagName				"DIAGNOSTIC"
#define DiscName				"DISCOVERY"
#define ProtName				"VALUES"

#define Header(type)							              "OK" KEY_DELIMITER type KEY_DELIMITER
#define DiagnosticHeader(type)					        (String)Header(DiagName) + type + KEY_DELIMITER
#define DiscoveryHeader(type, id) 				      (String)Header(DiscName) + type + KEY_DELIMITER + id + KEY_DELIMITER
#define SensorName(id)							            (String)#id
#define SensorNamePort(id, port)                (String)#id + PORT_DELIMITER + port
#define SensorDataHeader(type, id) 				      (String)Header(ProtName) + type + KEY_DELIMITER + id + KEY_DELIMITER

// enable short key names
#ifndef KVP_LONG_KEY_FORMAT

#define DictionaryHeader 						            "INIT DICTIONARY" KEY_DELIMITER
#define DictionaryValue(id)						          (String)id + KEY_VALUE_DELIMITER + #id + VALUE_DELIMITER
#define DictionaryValuePort(id, port)			      (String)id + PORT_DELIMITER + port + KEY_VALUE_DELIMITER + #id + PORT_DELIMITER + port + VALUE_DELIMITER
#define SensorDataValue(key, value) 			      (String)key + KEY_VALUE_DELIMITER + value + VALUE_DELIMITER
#define SensorDataValuePort(key, port, value) 	(String)key + PORT_DELIMITER + port + KEY_VALUE_DELIMITER + value + VALUE_DELIMITER

#else

#define SensorDataValue(key, value) 			      (String)(#key) + KEY_VALUE_DELIMITER + value + VALUE_DELIMITER
#define SensorDataValuePort(key, port, value) 	(String)(#key) + PORT_DELIMITER + port + KEY_VALUE_DELIMITER + value + VALUE_DELIMITER

#endif

enum Sensors {
// weather Readings
	Temperature 		  = (byte)  1
,	Pressure			    = (byte)  2
,	Humidity			    = (byte)  3
,	WindSpeed			    = (byte)  4
,	WindDirection		  = (byte)  5
,	WindGust			    = (byte)  6
,	WindGustRef			  = (byte)  7
,	RainTipCount		  = (byte)  8
,	RainSecs			    = (byte)  9
,	Solar				      = (byte) 10
,	VoltageSolar		  = (byte) 11
,	VoltageCapacitor	= (byte) 12
,	SoilLeaf			    = (byte) 13
,	UV					      = (byte) 14
,	SoilTemperature	  = (byte) 15
,	SoilMoisture		  = (byte) 16
,	LeafWetness			  = (byte) 17
// techn. Readings
,	Channel				    = (byte) 20
,	Battery				    = (byte) 21
,	RSSI				      = (byte) 22
// Esp1wire Readings
, Device            = (byte) 30
, Counter           = (byte) 31
, Latch             = (byte) 32
, Sense             = (byte) 33
, FlipFlopQ         = (byte) 34
, Voltage           = (byte) 35
, Current           = (byte) 36
, Capacity          = (byte) 37
// diagnostic Readings
,	PacketDump			= (byte)255
} ;


#endif  // _KVP_Sensors_h
