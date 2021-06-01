#ifndef SIMPLE_METERING_H
#define SIMPLE_METERING_H

#include "utils/utils.h"

//Attribute IDs
#define METERING_ATTRID_CURRENT_SUMMATION_DELIVERED             0x0000
#define METERING_ATTRID_PULSE_CONFIGURATION                     0x0300  // Develco specific
#define METERING_ATTRID_INTERFACE_MODE                          0x0302  // Develco specific
#define METERING_ATTRID_INSTANTANEOUS_DEMAND                    0x0400

// Values for attribute Interface Mode (0x0302), Develco specific
#define PULSE_COUNTING_ELECTRICITY      0x0000
#define PULSE_COUNTING_GAS              0x0001
#define PULSE_COUNTING_WATER            0x0002
#define KAMSTRUP_KMP                    0x0100
#define LINKY                           0x0101
#define DLMS_COSEM                      0x0102
#define DSMR_23                         0x0103
#define DSMR_40                         0x0104
#define NORWEGIAN_HAN                   0x0200
#define NORWEGIAN_HAN_EXTRA_LOAD        0x0201
#define AIDON_METER                     0x0202
#define KAIFA_KAMSTRUP_METERS           0x0203
#define AUTO_DETECT                     0x0204

extern const std::array<KeyValMapInt, 8> RConfigInterfaceModeValuesZHEMI;
extern const std::array<KeyValMapInt, 5> RConfigInterfaceModeValuesEMIZB;

#endif // SIMPLE_METERING_H
