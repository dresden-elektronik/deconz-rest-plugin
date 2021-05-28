#ifndef THERMOSTAT_H
#define THERMOSTAT_H

#include <QString>
#include <array>
#include "utils/utils.h"

#define LOCAL_TEMPERATURE_CALIBRATION           0x0010
#define OCCUPIED_COOLING_SETPOINT               0x0011
#define OCCUPIED_HEATING_SETPOINT               0x0012
#define CONTROL_SEQUENCE_OF_OPERATION           0x001B
#define SYSTEM_MODE                             0x001C
#define THERMOSTAT_PROGRAMMING_OPERATION_MODE   0x0025
#define AC_LOUVER_POSITION                      0x0045
#define TEMPERATURE_MEASUREMENT                 0x0403 // ELKO specific
#define DEVICE_ON                               0x0406 // ELKO specific
#define CHILD_LOCK                              0x0413 // ELKO specific
#define CURRENT_TEMPERATURE_SETPOINT            0x4003 // Eurotronic specific
#define EXTERNAL_OPEN_WINDOW_DETECTED           0x4003 // Danfoss specific
#define HOST_FLAGS                              0x4008 // Eurotronic specific
#define EXTERNAL_MEASUREMENT                    0x4015 // Danfoss specific
#define REGULATION_SETPOINT_OFFSET              0x404B // Danfoss specific

// Thermostat cluster, Control Sequence of Operation (0x001B)
#define COOLING_ONLY                            0x00
#define COOLING_WITH_REHEAT                     0x01
#define HEATING_ONLY                            0x02
#define HEATING_WITH_REHEAT                     0x03
#define COOLING_AND_HEATING_4PIPES              0x04
#define COOLING_AND_HEATING_4PIPES_WITH_REHEAT  0x05

static const std::array<KeyValMap, 6> RConfigModeLegrandValues = { { {QLatin1String("confort"), 0}, {QLatin1String("confort-1"), 1}, {QLatin1String("confort-2"), 2},
                                                              {QLatin1String("eco"), 3}, {QLatin1String("hors gel"), 4}, {QLatin1String("off"), 5} } };

static const std::array<KeyValMapTuyaSingle, 3> RConfigModeValuesTuya1 = { { {QLatin1String("auto"), {0x00}}, {QLatin1String("heat"), {0x01}}, {QLatin1String("off"), {0x02}} } };

static const std::array<KeyValMapTuyaSingle, 2> RConfigModeValuesTuya2 = { { {QLatin1String("off"), {0x00}}, {QLatin1String("heat"), {0x01}} } };

static const std::array<KeyValMap, 9> RConfigModeValues = { { {QLatin1String("off"), 0}, {QLatin1String("auto"), 1}, {QLatin1String("cool"), 3}, {QLatin1String("heat"), 4},
                                                              {QLatin1String("emergency heating"), 5}, {QLatin1String("precooling"), 6}, {QLatin1String("fan only"), 7},
                                                              {QLatin1String("dry"), 8}, {QLatin1String("sleep"), 9} } };

static const std::array<KeyValMapTuyaSingle, 7> RConfigPresetValuesTuya = { { {QLatin1String("holiday"), {0x00}}, {QLatin1String("auto"), {0x01}}, {QLatin1String("manual"), {0x02}},
                                                                              {QLatin1String("comfort"), {0x04}}, {QLatin1String("eco"), {0x05}}, {QLatin1String("boost"), {0x06}},
                                                                              {QLatin1String("complex"), {0x07}} } };

static const std::array<KeyMap, 2> RConfigPresetValuesTuya2 = { { {QLatin1String("auto")}, {QLatin1String("program")} } };

static const std::array<KeyMap, 4> RConfigPresetValuesTuya3 = { { {QLatin1String("both")}, {QLatin1String("humidity")}, {QLatin1String("temperature")}, {QLatin1String("off")} } };

static const std::array<KeyValMap, 5> RConfigTemperatureMeasurementValues = { { {QLatin1String("air sensor"), 0}, {QLatin1String("floor sensor"), 1},
                                                                                {QLatin1String("floor protection"), 3} } };

static const std::array<KeyValMap, 5> RConfigSwingModeValues = { { {QLatin1String("fully closed"), 1}, {QLatin1String("fully open"), 2}, {QLatin1String("quarter open"), 3},
                                                                   {QLatin1String("half open"), 4}, {QLatin1String("three quarters open"), 5} } };

static const std::array<KeyValMapInt, 6> RConfigControlSequenceValues = { { {1, COOLING_ONLY}, {2, COOLING_WITH_REHEAT}, {3, HEATING_ONLY}, {4, HEATING_WITH_REHEAT},
                                                                            {5, COOLING_AND_HEATING_4PIPES}, {6, COOLING_AND_HEATING_4PIPES_WITH_REHEAT} } };

static const std::array<KeyMap, 3> RConfigModeValuesEurotronic = { { {QLatin1String("off")}, {QLatin1String("heat")}, {QLatin1String("auto")} } };

#endif // THERMOSTAT_H
