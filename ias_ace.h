#ifndef IAS_ACE_H
#define IAS_ACE_H

#include <QString>

// server send
#define IAS_ACE_CMD_ARM_RESPONSE 0x00
#define IAS_ACE_CMD_GET_ZONE_ID_MAP_RESPONSE 0x01
#define IAS_ACE_CMD_GET_ZONE_INFORMATION_RESPONSE 0x02
#define IAS_ACE_CMD_ZONE_STATUS_CHANGED 0x03
#define IAS_ACE_CMD_PANEL_STATUS_CHANGED 0x04
#define IAS_ACE_CMD_GET_PANEL_STATUS_RESPONSE 0x05
#define IAS_ACE_CMD_SET_BYPASSED_ZONE_LIST 0x06
#define IAS_ACE_CMD_BYPASS_RESPONSE 0x07
#define IAS_ACE_CMD_GET_ZONE_STATUS_RESPONSE 0x08
// server receive
#define IAS_ACE_CMD_ARM 0x00
#define IAS_ACE_CMD_BYPASS 0x01
#define IAS_ACE_CMD_EMERGENCY 0x02
#define IAS_ACE_CMD_FIRE 0x03
#define IAS_ACE_CMD_PANIC 0x04
#define IAS_ACE_CMD_GET_ZONE_ID_MAP 0x05
#define IAS_ACE_CMD_GET_ZONE_INFORMATION 0x06
#define IAS_ACE_CMD_GET_PANEL_STATUS 0x07
#define IAS_ACE_CMD_GET_BYPASSED_ZONE_LIST 0x08
#define IAS_ACE_CMD_GET_ZONE_STATUS 0x09

#define IAS_ACE_PANEL_STATUS_PANEL_DISARMED           0x00
#define IAS_ACE_PANEL_STATUS_ARMED_STAY               0x01
#define IAS_ACE_PANEL_STATUS_ARMED_NIGHT              0x02
#define IAS_ACE_PANEL_STATUS_ARMED_AWAY               0x03
#define IAS_ACE_PANEL_STATUS_EXIT_DELAY               0x04
#define IAS_ACE_PANEL_STATUS_ENTRY_DELAY              0x05
#define IAS_ACE_PANEL_STATUS_NOT_READY_TO_ARM         0x06
#define IAS_ACE_PANEL_STATUS_IN_ALARM                 0x07
#define IAS_ACE_PANEL_STATUS_ARMING_STAY              0x08
#define IAS_ACE_PANEL_STATUS_ARMING_NIGHT             0x09
#define IAS_ACE_PANEL_STATUS_ARMING_AWAY              0x0a

namespace deCONZ {
    class ApsDataIndication;
    class ZclFrame;
}

class AlarmSystems;
class ApsControllerWrapper;

QLatin1String IAS_PanelStatusToString(quint8 panelStatus);
int IAS_PanelStatusFromString(const QString &panelStatus);
void IAS_IasAceClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame, AlarmSystems *alarmSystems, ApsControllerWrapper &apsCtrlWrapper);

#endif // IAS_ACE_H
