#ifndef IAS_ACE_H
#define IAS_ACE_H

#include "utils/utils.h"

// server send
#define CMD_ARM_RESPONSE 0x00
#define CMD_GET_ZONE_ID_MAP_RESPONSE 0x01
#define CMD_GET_ZONE_INFORMATION_RESPONSE 0x02
#define CMD_ZONE_STATUS_CHANGED 0x03
#define CMD_PANEL_STATUS_CHANGED 0x04
#define CMD_GET_PANEL_STATUS_RESPONSE 0x05
#define CMD_SET_BYPASSED_ZONE_LIST 0x06
#define CMD_BYPASS_RESPONSE 0x07
#define CMD_GET_ZONE_STATUS_RESPONSE 0x08
// server receive
#define CMD_ARM 0x00
#define CMD_BYPASS 0x01
#define CMD_EMERGENCY 0x02
#define CMD_FIRE 0x03
#define CMD_PANIC 0x04
#define CMD_GET_ZONE_ID_MAP 0x05
#define CMD_GET_ZONE_INFORMATION 0x06
#define CMD_GET_PANEL_STATUS 0x07
#define CMD_GET_BYPASSED_ZONE_LIST 0x08
#define CMD_GET_ZONE_STATUS 0x09

extern const std::array<KeyMap, 7> RConfigArmModeValues;
extern const std::array<KeyMap, 11> RConfigPanelValues;

#endif // IAS_ACE_H