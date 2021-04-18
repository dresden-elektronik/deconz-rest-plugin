/*
 * Copyright (c)2018-2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef TUYA_H
#define TUYA_H

// Value for dp_type
// ------------------    
// 0x00 	DP_TYPE_RAW 	?
// 0x01 	DP_TYPE_BOOL 	?
// 0x02 	DP_TYPE_VALUE 	4 byte unsigned integer
// 0x03 	DP_TYPE_STRING 	variable length string
// 0x04 	DP_TYPE_ENUM 	1 byte enum
// 0x05 	DP_TYPE_FAULT 	1 byte bitmap (didn't test yet)

#define DP_TYPE_RAW 0x00
#define DP_TYPE_BOOL 0x01
#define DP_TYPE_VALUE 0x02
#define DP_TYPE_STRING 0x03
#define DP_TYPE_ENUM 0x04
#define DP_TYPE_FAULT 0x05

// Value for dp_identifier for sensor
//-----------------------------------
#define DP_IDENTIFIER_WINDOW_OPEN 0x12
#define DP_IDENTIFIER_WINDOW_OPEN2 0x08
#define DP_IDENTIFIER_THERMOSTAT_SCHEDULE_1 0x65 // Moe thermostat W124 (4) + W002 (4) + W001 (4)
#define DP_IDENTIFIER_THERMOSTAT_SCHEDULE_2 0x70 // work days (6)
#define DP_IDENTIFIER_THERMOSTAT_SCHEDULE_3 0x71 // holiday = Not working day (6)
#define DP_IDENTIFIER_THERMOSTAT_SCHEDULE_4 0x6D // Not finished

#define DP_IDENTIFIER_THERMOSTAT_HEATSETPOINT 0x02 // Heatsetpoint
#define DP_IDENTIFIER_THERMOSTAT_HEATSETPOINT_2 0x67 // Heatsetpoint
#define DP_IDENTIFIER_THERMOSTAT_VALVE 0x14 // Heatsetpoint


// Value for dp_identifier for covering
//-----------------------------------
#define DP_IDENTIFIER_CONTROL 0x01
#define DP_IDENTIFIER_PERCENT_CONTROL 0x02
#define DP_IDENTIFIER_POSITION_REACHED 0x03
#define DP_IDENTIFIER_WORK_STATE 0x05
#define DP_IDENTIFIER_POSITION_MOVING 0x07

// Value for dp_identifier for switches
//-----------------------------------
#define DP_IDENTIFIER_BUTTON_1 0x01
#define DP_IDENTIFIER_BUTTON_2 0x02
#define DP_IDENTIFIER_BUTTON_3 0x03

// Value for dp_identifier for siren
//-----------------------------------
#define DP_IDENTIFIER_MELODY 0x66
#define DP_IDENTIFIER_ALARM 0x68
#define DP_IDENTIFIER_TRESHOLDTEMPMINI 0x6B
#define DP_IDENTIFIER_TRESHOLDTEMPMAXI 0x6C
#define DP_IDENTIFIER_TRESHOLDTHUMIMINI 0x6D
#define DP_IDENTIFIER_TRESHOLDHUMIMAXI 0x6E
#define DP_IDENTIFIER_TEMPERATURE_ALARM 0x71
#define DP_IDENTIFIER_HUMIDITY_ALARM 0x72
#define DP_IDENTIFIER_VOLUME 0x74


// Value for tuya command
//-----------------------
#define TUYA_REQUEST 0x00
#define TUYA_REPORTING 0x01
#define TUYA_QUERY 0x02
#define TUYA_TIME_SYNCHRONISATION 0x24

bool UseTuyaCluster(const QString &manufacturer);

#endif // TUYA_H
