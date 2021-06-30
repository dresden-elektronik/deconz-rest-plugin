#include <QString>
#include <QVariantMap>
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "json.h"

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


void DeRestPluginPrivate::handleIasAceClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    if (zclFrame.isDefaultResponse())
    {
        return;
    }

    QDataStream stream(zclFrame.payload());
    stream.setByteOrder(QDataStream::LittleEndian);

    if ((zclFrame.frameControl() & deCONZ::ZclFCDirectionServerToClient))
    {
        return;
    }

    if (zclFrame.commandId() == CMD_ARM)
    {
        quint8 armMode;
        QString code;
        quint8 zoneId;

        stream >> armMode;

        if (zclFrame.payload().length() == 3)
        {
            quint8 codeTemp;
            stream >> codeTemp;     // 0 for keyfobs or other devices not supporting any codes
            code = codeTemp;
        }
        else
        {
            // Not yet supported
            return;
        }

        stream >> zoneId;

        if (armMode <= 3)
        {
            sendArmResponse(ind, zclFrame, armMode);
        }

        return;
    }
    else if (zclFrame.commandId() == CMD_EMERGENCY)
    {
    }
    else if (zclFrame.commandId() == CMD_FIRE)
    {
    }
    else if (zclFrame.commandId() == CMD_PANIC)
    {
    }
    else if (zclFrame.commandId() == CMD_GET_ZONE_ID_MAP)
    {
    }
    else if (zclFrame.commandId() == CMD_GET_ZONE_INFORMATION)
    {
    }
    else if (zclFrame.commandId() == CMD_GET_PANEL_STATUS)
    {
    }
    else if (zclFrame.commandId() == CMD_GET_BYPASSED_ZONE_LIST)
    {
    }
    else if (zclFrame.commandId() == CMD_GET_ZONE_STATUS)
    {
    }
}

void DeRestPluginPrivate::sendArmResponse(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame, quint8 armMode)
{
    quint8 armNotification = 0xFF;

    if (armMode <= 3)
    {
        // 0: All zones disarmed, 1: Only day/home zones armed, 2: Only night/sleep zones armed, 3: All zones armed
        armNotification = armMode;
    }
    else if (armMode > 3 && armMode <= 6)
    {
        // 4 - 6 not supported
        return;
    }
    else
    {
        // invalid
        return;
    }

    deCONZ::ApsDataRequest req;
    deCONZ::ZclFrame outZclFrame;

    req.setProfileId(ind.profileId());
    req.setClusterId(ind.clusterId());
    req.setDstAddressMode(ind.srcAddressMode());
    req.dstAddress() = ind.srcAddress();
    req.setDstEndpoint(ind.srcEndpoint());
    req.setSrcEndpoint(endpoint());

    outZclFrame.setSequenceNumber(zclFrame.sequenceNumber());
    outZclFrame.setCommandId(CMD_ARM_RESPONSE);

    outZclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionServerToClient |
                             deCONZ::ZclFCDisableDefaultResponse);

    { // payload
        QDataStream stream(&outZclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        stream << armNotification;
    }

    { // ZCL frame
        QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        outZclFrame.writeToStream(stream);
    }

    if (apsCtrlWrapper.apsdeDataRequest(req) != deCONZ::Success)
    {
        DBG_Printf(DBG_INFO_L2, "[IAS ACE] - Failed to send IAS ACE arm reponse.\n");
    }
}
