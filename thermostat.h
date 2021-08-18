#ifndef THERMOSTAT_H
#define THERMOSTAT_H

#include "utils/utils.h"

//Attribute IDs
#define THERM_ATTRID_LOCAL_TEMPERATURE_CALIBRATION           0x0010
#define THERM_ATTRID_OCCUPIED_COOLING_SETPOINT               0x0011
#define THERM_ATTRID_OCCUPIED_HEATING_SETPOINT               0x0012
#define THERM_ATTRID_CONTROL_SEQUENCE_OF_OPERATION           0x001B
#define THERM_ATTRID_SYSTEM_MODE                             0x001C
#define THERM_ATTRID_TEMPERATURE_SETPOINT_HOLD               0x0023
#define THERM_ATTRID_TEMPERATURE_SETPOINT_HOLD_DURATION      0x0024
#define THERM_ATTRID_THERMOSTAT_PROGRAMMING_OPERATION_MODE   0x0025
#define THERM_ATTRID_AC_LOUVER_POSITION                      0x0045
#define THERM_ATTRID_TEMPERATURE_MEASUREMENT                 0x0403 // ELKO specific
#define THERM_ATTRID_DEVICE_ON                               0x0406 // ELKO specific
#define THERM_ATTRID_CHILD_LOCK                              0x0413 // ELKO specific
#define THERM_ATTRID_CURRENT_TEMPERATURE_SETPOINT            0x4003 // Eurotronic specific
#define THERM_ATTRID_EXTERNAL_OPEN_WINDOW_DETECTED           0x4003 // Danfoss specific
#define THERM_ATTRID_HOST_FLAGS                              0x4008 // Eurotronic specific
#define THERM_ATTRID_MOUNTING_MODE_CONTROL                   0x4013 // Danfoss specific
#define THERM_ATTRID_EXTERNAL_MEASUREMENT                    0x4015 // Danfoss specific
#define THERM_ATTRID_REGULATION_SETPOINT_OFFSET              0x404B // Danfoss specific

// Values for attribute Control Sequence of Operation (0x001B)
#define COOLING_ONLY                            0x00
#define COOLING_WITH_REHEAT                     0x01
#define HEATING_ONLY                            0x02
#define HEATING_WITH_REHEAT                     0x03
#define COOLING_AND_HEATING_4PIPES              0x04
#define COOLING_AND_HEATING_4PIPES_WITH_REHEAT  0x05

extern const std::array<KeyValMap, 6> RConfigModeLegrandValues;
extern const std::array<KeyValMapTuyaSingle, 3> RConfigModeValuesTuya1;
extern const std::array<KeyValMapTuyaSingle, 2> RConfigModeValuesTuya2;
extern const std::array<KeyValMap, 9> RConfigModeValues;
extern const std::array<KeyValMapTuyaSingle, 7> RConfigPresetValuesTuya;
extern const std::array<KeyMap, 2> RConfigPresetValuesTuya2;
extern const std::array<KeyMap, 4> RConfigPresetValuesTuya3;
extern const std::array<KeyValMap, 3> RConfigTemperatureMeasurementValues;
extern const std::array<KeyValMap, 5> RConfigSwingModeValues;
extern const std::array<KeyValMapInt, 6> RConfigControlSequenceValues;
extern const std::array<KeyMap, 3> RConfigModeValuesEurotronic;
extern const std::array<KeyValMap, 5> RStateWindowOpenValuesDanfoss;

#endif // THERMOSTAT_H
