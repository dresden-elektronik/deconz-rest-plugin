#ifndef XIAOMI_H
#define XIAOMI_H

#define XIAOMI_CLUSTER_ID                 0xFCC0

#define XIAOMI_ATTRID_DEVICE_MODE         0x0009
#define XIAOMI_ATTRID_SPECIAL_REPORT      0x00F7
#define XIAOMI_ATTRID_MOTION_SENSITIVITY  0x010C
#define XIAOMI_ATTRID_MULTICLICK_MODE     0x0125
#define XIAOMI_ATTRID_HONEYWELL_CONFIG    0xFFF0
#define XIAOMI_ATTRID_SMOKE_SENSITIVITY   0xFFF1

extern const std::array<KeyValMapUint8Uint32, 3> RConfigXiaomiHoneywellSensitivityValues;

#endif // XIAOMI_H
