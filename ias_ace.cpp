
// TODO remove dependency on plugin

#include "de_web_plugin.h"
#include "de_web_plugin_private.h"

#include "ias_ace.h"
#include "ias_zone.h"

//  Arm mode command
//-------------------
// 0x00 Disarm    
// 0x01 Arm Day/Home Zones Only
// 0x02 Arm Night/Sleep Zones Only
// 0x03 Arm All Zones

//  Arm mode response
//-------------------
// 0x00 All Zones Disarmed
// 0x01 Only Day/Home Zones Armed
// 0x02 Only Night/Sleep Zones Armed
// 0x03 All Zones Armed
// 0x04 Invalid Arm/Disarm Code
// 0x05 Not ready to arm
// 0x06 Already disarmed

//   Panel status
// --------------        
// 0x00 Panel disarmed (all zones disarmed) and ready to arm
// 0x01 Armed stay
// 0x02 Armed night
// 0x03 Armed away
// 0x04 Exit delay
// 0x05 Entry delay
// 0x06 Not ready to arm
// 0x07 In alarm
// 0x08 Arming Stay
// 0x09 Arming Night
// 0x0a Arming Away

// Alarm Status
// ------------
// 0x00 No alarm
// 0x01 Burglar
// 0x02 Fire
// 0x03 Emergency
// 0x04 Police Panic
// 0x05 Fire Panic
// 0x06 Emergency Panic (i.e., medical issue)

// Audible Notification
// ----------------------   
// 0x00 Mute (i.e., no audible notification)
// 0x01 Default sound
// 0x80-0xff Manufacturer specific

#define IAS_ACE_ARM_MODE_DISARM                       0x00
#define IAS_ACE_ARM_MODE_ARM_DAY_HOME_ZONES_ONLY      0x01
#define IAS_ACE_ARM_MODE_ARM_NIGHT_SLEEP_ZONES_ONLY   0x02
#define IAS_ACE_ARM_MODE_ARM_ALL_ZONES                0x03

#define IAS_ACE_ARM_NOTF_ALL_ZONES_DISARMED           0x00
#define IAS_ACE_ARM_NOTF_ONLY_DAY_HOME_ZONES_ARMED    0x01
#define IAS_ACE_ARM_NOTF_ONLY_NIGHT_SLEEP_ZONES_ARMED 0x02
#define IAS_ACE_ARM_NOTF_ALL_ZONES_ARMED              0x03
#define IAS_ACE_ARM_NOTF_INVALID_ARM_DISARM_CODE      0x04
#define IAS_ACE_ARM_NOTF_NOT_READY_TO_ARM             0x05
#define IAS_ACE_ARM_NOTF_ALREADY_DISARMED             0x06

// strings mapping directly to IAS_ACE_ARM_MODE_* and IAS_ACE_ARM_NOTF_*
static const std::array<QLatin1String, 7> IAS_ArmResponse = {
    QLatin1String("disarmed"),
    QLatin1String("armed_stay"),
    QLatin1String("armed_night"),
    QLatin1String("armed_away"),
    QLatin1String("invalid_code"),
    QLatin1String("not_ready"),
    QLatin1String("already_disarmed")
};

static const std::array<QLatin1String, 11> IAS_PanelStates = {
    QLatin1String("disarmed"),
    QLatin1String("armed_stay"),
    QLatin1String("armed_night"),
    QLatin1String("armed_away"),
    QLatin1String("exit_delay"),
    QLatin1String("entry_delay"),
    QLatin1String("not_ready"),
    QLatin1String("in_alarm"),
    QLatin1String("arming_stay"),
    QLatin1String("arming_night"),
    QLatin1String("arming_away")
};

static void sendArmResponse(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame, quint8 armMode, ApsControllerWrapper &apsCtrlWrapper);
static void sendGetPanelStatusResponse(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame, quint8 panelStatus, quint8 secs, ApsControllerWrapper &apsCtrlWrapper);

QLatin1String IAS_PanelStatusToString(quint8 panelStatus)
{
    if (panelStatus < IAS_PanelStates.size())
    {
        return IAS_PanelStates[panelStatus];
    }

    return QLatin1String("");
}

int IAS_PanelStatusFromString(const QString &panelStatus)
{
    const auto i = std::find(IAS_PanelStates.cbegin(), IAS_PanelStates.cend(), panelStatus);

    if (i != IAS_PanelStates.cend())
    {
        return std::distance(IAS_PanelStates.cbegin(), i);
    }

    return -1;
}

static quint8 handleArmCommand(AlarmSystem *alarmSys, quint8 armMode, const QString &pinCode, quint64 srcAddress)
{
    if (!alarmSys || armMode > IAS_ACE_ARM_MODE_ARM_ALL_ZONES)
    {
        return IAS_ACE_ARM_NOTF_NOT_READY_TO_ARM;
    }

    if (!alarmSys->isValidCode(pinCode, srcAddress))
    {
        return IAS_ACE_ARM_NOTF_INVALID_ARM_DISARM_CODE;
    }

    const quint8 armMode0 = alarmSys->targetArmMode();

    if (armMode0 == IAS_ACE_ARM_MODE_DISARM && armMode == armMode0)
    {
        return IAS_ACE_ARM_NOTF_ALREADY_DISARMED;
    }

    static_assert (IAS_ACE_ARM_MODE_DISARM == AS_ArmModeDisarmed, "");
    static_assert (IAS_ACE_ARM_MODE_ARM_DAY_HOME_ZONES_ONLY == AS_ArmModeArmedStay, "");
    static_assert (IAS_ACE_ARM_MODE_ARM_NIGHT_SLEEP_ZONES_ONLY == AS_ArmModeArmedNight, "");
    static_assert (IAS_ACE_ARM_MODE_ARM_ALL_ZONES == AS_ArmModeArmedAway, "");

    if (armMode0 != armMode)
    {
        alarmSys->setTargetArmMode(AS_ArmMode(armMode));
    }

    return armMode;
}

void IAS_IasAceClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame, AlarmSystems *alarmSystems, ApsControllerWrapper &apsCtrlWrapper)
{
    if (zclFrame.isDefaultResponse())
    {
        return;
    }

    if (zclFrame.frameControl() & deCONZ::ZclFCDirectionServerToClient)
    {
        return;
    }

    Sensor *sensor = plugin->getSensorNodeForAddressAndEndpoint(ind.srcAddress(), ind.srcEndpoint(), QLatin1String("ZHAAncillaryControl"));
    if (!sensor)
    {
        return;
    }

    bool stateUpdated = false;

    if (zclFrame.commandId() == IAS_ACE_CMD_ARM && zclFrame.payload().size() >= 2)
    {
        // [0] arm mode (enum8)
        const quint8 armMode = quint8(zclFrame.payload().at(0));

        if (armMode > IAS_ACE_ARM_MODE_ARM_ALL_ZONES)
        {
            DBG_Printf(DBG_IAS, "[IAS ACE] 0x%016llX invalid arm mode: %d, skip\n", ind.srcAddress().ext(), armMode);
            return;
        }
        
        quint8 armRsp = IAS_ACE_ARM_NOTF_NOT_READY_TO_ARM;

        // [1] arm/disarm code in payload (pascal string, allowed to be empty, e.g. for keyfobs)
        QString armCode;
        if (zclFrame.payload().size() > 2)
        {
            int length = zclFrame.payload().at(1);
            if (length <= zclFrame.payload().size() - 2)
            {
                armCode = QString::fromUtf8(zclFrame.payload().constData() + 2, length);
            }
            else
            {
                armRsp = IAS_ACE_ARM_NOTF_INVALID_ARM_DISARM_CODE;
                armCode = QLatin1String("invalid_code");
            }
        }

        // [2] zone id (uint8, ignore, we don't do anything with it)
        // const quint8 zoneId = static_cast<quint8>(zclFrame.payload().at(zclFrame.payload().size() - 1));
        
        DBG_Printf(DBG_IAS, "[IAS ACE] 0x%016llX arm command received, arm mode: 0x%02X, code length: %d\n", ind.srcAddress().ext(), armMode, armCode.size());

        AlarmSystem *alarmSys = AS_GetAlarmSystemForDevice(ind.srcAddress().ext(), *alarmSystems);

        if (alarmSys)
        {
            armRsp = handleArmCommand(alarmSys, armMode, armCode, ind.srcAddress().ext());
        }

        {
            ResourceItem *actionItem = sensor->item(RStateAction);

            if (actionItem && armRsp < IAS_ArmResponse.size())
            {
                actionItem->setValue(QString(IAS_ArmResponse[armRsp]));
                enqueueEvent(Event(sensor->prefix(), actionItem->descriptor().suffix, sensor->id(), armMode));
                stateUpdated = true;
            }
        }

        sendArmResponse(ind, zclFrame, armRsp, apsCtrlWrapper);
    }
    else if (zclFrame.commandId() == IAS_ACE_CMD_GET_PANEL_STATUS)
    {
        quint8 panelStatus = IAS_ACE_PANEL_STATUS_NOT_READY_TO_ARM;
        quint8 secondsRemaining = 0;

        AlarmSystem *alarmSys = AS_GetAlarmSystemForDevice(ind.srcAddress().ext(), *alarmSystems);

        if (alarmSys)
        {
            panelStatus = alarmSys->iasAcePanelStatus();

            if (panelStatus == IAS_ACE_PANEL_STATUS_ENTRY_DELAY || panelStatus == IAS_ACE_PANEL_STATUS_EXIT_DELAY)
            {
                secondsRemaining = quint8(alarmSys->secondsRemaining());
            }
        }

        sendGetPanelStatusResponse(ind, zclFrame, panelStatus, secondsRemaining, apsCtrlWrapper);
    }
    else if (zclFrame.commandId() >= IAS_ACE_CMD_EMERGENCY && zclFrame.commandId() <= IAS_ACE_CMD_PANIC)
    {
        ResourceItem *actionItem = sensor->item(RStateAction);

        const std::array<QLatin1String, 3> cmds = {
            QLatin1String("emergency"),
            QLatin1String("fire"),
            QLatin1String("panic")
        };

        const quint8 index = zclFrame.commandId() - IAS_ACE_CMD_EMERGENCY;

        if (actionItem && index < cmds.size())
        {
            actionItem->setValue(QString(cmds[index]));
            enqueueEvent(Event(sensor->prefix(), actionItem->descriptor().suffix, sensor->id(), zclFrame.commandId()));
            stateUpdated = true;
        }
    }
    else
    {
        DBG_Printf(DBG_IAS, "[IAS ACE] 0x%016llX unhandled command: 0x%02X\n", ind.srcAddress().ext(), zclFrame.commandId());
    }
    
    if (stateUpdated)
    {
        sensor->updateStateTimestamp();
        enqueueEvent(Event(RSensors, RStateLastUpdated, sensor->id()));
        plugin->updateSensorEtag(sensor);
        sensor->setNeedSaveDatabase(true);
        plugin->queSaveDb(DB_SENSORS, DB_SHORT_SAVE_DELAY);
    }
}

static void sendArmResponse(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame, quint8 armMode, ApsControllerWrapper &apsCtrlWrapper)
{
    DBG_Assert(armMode <= IAS_ACE_ARM_NOTF_ALREADY_DISARMED);

    if (armMode > IAS_ACE_ARM_NOTF_ALREADY_DISARMED)
    {
        return;
    }

    deCONZ::ApsDataRequest req;
    deCONZ::ZclFrame outZclFrame;

    req.setProfileId(ind.profileId());
    req.setClusterId(ind.clusterId());
    req.setDstAddressMode(ind.srcAddressMode());
    req.dstAddress() = ind.srcAddress();
    req.setDstEndpoint(ind.srcEndpoint());
    req.setSrcEndpoint(plugin->endpoint());

    outZclFrame.setSequenceNumber(zclFrame.sequenceNumber());
    outZclFrame.setCommandId(IAS_ACE_CMD_ARM_RESPONSE);

    outZclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionServerToClient |
                             deCONZ::ZclFCDisableDefaultResponse);

    { // payload
        QDataStream stream(&outZclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        stream << armMode;
    }

    { // ZCL frame
        QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        outZclFrame.writeToStream(stream);
    }

    if (apsCtrlWrapper.apsdeDataRequest(req) != deCONZ::Success)
    {
        DBG_Printf(DBG_IAS, "[IAS ACE] 0x%016llX failed to send IAS ACE arm reponse.\n", ind.srcAddress().ext());
    }
}

static void sendGetPanelStatusResponse(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame, quint8 panelStatus, quint8 secs, ApsControllerWrapper &apsCtrlWrapper)
{
    deCONZ::ApsDataRequest req;
    deCONZ::ZclFrame outZclFrame;

    req.setProfileId(ind.profileId());
    req.setClusterId(ind.clusterId());
    req.setDstAddressMode(ind.srcAddressMode());
    req.dstAddress() = ind.srcAddress();
    req.setDstEndpoint(ind.srcEndpoint());
    req.setSrcEndpoint(plugin->endpoint());

    DBG_Printf(DBG_IAS, "[IAS ACE] 0x%016llX panel status response: 0x%02X\n", ind.srcAddress().ext(), panelStatus);

    outZclFrame.setSequenceNumber(zclFrame.sequenceNumber());
    outZclFrame.setCommandId(IAS_ACE_CMD_GET_PANEL_STATUS_RESPONSE);

    outZclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                                deCONZ::ZclFCDirectionServerToClient); // deCONZ::ZclFCDisableDefaultResponse

    { // payload
        QDataStream stream(&outZclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream << (quint8) panelStatus; // Panel status
        stream << (quint8) secs; // Seconds Remaining 
        stream << (quint8) 0x01; // Audible Notification

        if (panelStatus == IAS_ACE_PANEL_STATUS_IN_ALARM)
        {
            // TODO make this dynamic and managed by alarm system
            stream << (quint8) 0x03; // Alarm status, emergency
        }
        else
        {
            stream << (quint8) 0x00; // Alarm status
        }
    }

    { // ZCL frame
        QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        outZclFrame.writeToStream(stream);
    }

    if (apsCtrlWrapper.apsdeDataRequest(req) != deCONZ::Success)
    {
        DBG_Printf(DBG_IAS, "[IAS ACE] 0x%016llX failed to send IAS ACE get panel reponse.\n", ind.srcAddress().ext());
    }
}
