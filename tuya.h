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

// Value for dp_identifier for different sensor
//--------------------------------------------


// Value for thermostat
//---------------------

// Value for thermostat
//---------------------
// 0x04     Preset
// 0x6C     Auto / Manu
// 0x65     Manu / Off
// 0x6E     Low battery
// 0x02     Actual temperature
// 0x03     Thermostat temperature
// 0x14     Valve
// 0x15     Battery level
// 0x6A     Mode

#define DP_IDENTIFIER_WINDOW_OPEN 0x12
#define DP_IDENTIFIER_WINDOW_OPEN2 0x08
#define DP_IDENTIFIER_THERMOSTAT_SCHEDULE_1 0x65 // Moe thermostat W124 (4) + W002 (4) + W001 (4)
#define DP_IDENTIFIER_THERMOSTAT_SCHEDULE_2 0x70 // work days (6)
#define DP_IDENTIFIER_THERMOSTAT_SCHEDULE_3 0x71 // holiday = Not working day (6)
#define DP_IDENTIFIER_THERMOSTAT_SCHEDULE_4 0x6D // Not finished

#define DP_IDENTIFIER_THERMOSTAT_HEATSETPOINT 0x02 // Heatsetpoint
#define DP_IDENTIFIER_THERMOSTAT_HEATSETPOINT_2 0x67 // Heatsetpoint for Moe
#define DP_IDENTIFIER_THERMOSTAT_HEATSETPOINT_3 0x10 // Heatsetpoint for TRV_MOE mode heat
#define DP_IDENTIFIER_THERMOSTAT_HEATSETPOINT_4 0x69 // Heatsetpoint for TRV_MOE mode auto
#define DP_IDENTIFIER_THERMOSTAT_VALVE 0x14 // Valve
#define DP_IDENTIFIER_THERMOSTAT_CHILDLOCK_1 0x07
#define DP_IDENTIFIER_THERMOSTAT_CHILDLOCK_2 0x28
#define DP_IDENTIFIER_THERMOSTAT_CHILDLOCK_3 0x1E // For Moe device

#define DP_IDENTIFIER_THERMOSTAT_MODE_1 0x6A // mode used with DP_TYPE_ENUM
#define DP_IDENTIFIER_THERMOSTAT_MODE_2 0x02 // mode for Moe device used with DP_TYPE_ENUM
#define DP_IDENTIFIER_THERMOSTAT_MODE_3 0x65 // mode for Saswell device used with DP_TYPE_BOOL


// Value for dp_identifier for covering
//-----------------------------------

// Value for windows covering
//-----------------------------------------------------
// 0x01 	control         	enum 	open, stop, close, continue
// 0x02 	percent_control 	value 	0-100% control
// 0x03 	percent_state 	    value 	Report from motor about current percentage
// 0x04 	control_back     	enum 	Configures motor direction (untested)
// 0x05 	work_state       	enum 	Supposedly shows if motor is opening or closing, always 0 for me though
// 0x06 	situation_set 	    enum 	Configures if 100% equals to fully closed or fully open (untested)
// 0x07 	fault           	bitmap 	Anything but 0 means something went wrong (untested)

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
#define DP_IDENTIFIER_BUTTON_ALL 0x0D
#define DP_IDENTIFIER_DIMMER_LEVEL_MODE1 0x03
#define DP_IDENTIFIER_DIMMER_LEVEL_MODE2 0x02

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

// Value for dp_identifier for sensor
//-----------------------------------
#define DP_IDENTIFIER_REPORTING_TIME 0x62
#define DP_IDENTIFIER_TEMPERATURE 0x6B
#define DP_IDENTIFIER_HUMIDITY 0x6C
#define DP_IDENTIFIER_BATTERY 0x6E
#define DP_IDENTIFIER_REPORTING 0x70

// Value for tuya command
//-----------------------
#define TUYA_REQUEST 0x00
#define TUYA_REPORTING 0x01
#define TUYA_QUERY 0x02
#define TUYA_STATUS_SEARCH 0x06
#define TUYA_TIME_SYNCHRONISATION 0x24

bool UseTuyaCluster(const QString &manufacturer);

#endif // TUYA_H
