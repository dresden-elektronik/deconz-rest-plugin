#include <QString>
#include <QVariantMap>
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
//#include "json.h"
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

const std::array<KeyMap, 7> RConfigArmModeValues = { { {QLatin1String("disarmed")}, {QLatin1String("armed_stay")}, {QLatin1String("armed_night")}, {QLatin1String("armed_away")},
                                                       {QLatin1String("invalid_code")}, {QLatin1String("not_ready")}, {QLatin1String("already_disarmed")} } };


const std::array<KeyMap, 11> RConfigPanelValues = { {
    {QLatin1String("disarmed")},
    {QLatin1String("armed_stay")},
    {QLatin1String("armed_night")},
    {QLatin1String("armed_away")},
    {QLatin1String("exit_delay")},
    {QLatin1String("entry_delay")},
    {QLatin1String("not_ready_to_arm")},
    {QLatin1String("in_alarm")},
    {QLatin1String("arming_stay")},
    {QLatin1String("arming_night")},
    {QLatin1String("arming_away")}
} };

const std::array<QLatin1String, 11> PanelStatusList = {
    QLatin1String("disarmed") ,
    QLatin1String("armed_stay"),
    QLatin1String("armed_night") ,
    QLatin1String("armed_away"),
    QLatin1String("exit_delay"),
    QLatin1String("entry_delay"),
    QLatin1String("not_ready_to_arm"),
    QLatin1String("in_alarm"),
    QLatin1String("arming_stay"),
    QLatin1String("arming_night"),
    QLatin1String("arming_away")
};

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

const std::array<QLatin1String, 4> ArmModeValues = {
    QLatin1String("disarm"),
    QLatin1String("arm_day_home_zones_only"),
    QLatin1String("arm_night_sleep_zones_only"),
    QLatin1String("arm_all_zones")
};

// following strings map directly to IAS_ACE_ARM_MODE_* and IAS_ACE_ARM_NOTF_*
const std::array<QLatin1String, 7> ArmModeListReturn ={
    QLatin1String("disarmed"),
    QLatin1String("armed_stay"),
    QLatin1String("armed_night"),
    QLatin1String("armed_away"),
    QLatin1String("invalid_code"),
    QLatin1String("not_ready"),
    QLatin1String("already_disarmed")
};

IASZone *IAS_GetZone(quint8 zoneId)
{
    static IASZone zone;

    return &zone;
}

bool IAS_IsValidPinCode(quint8 zoneId, const QString &pinCode)
{
    if (zoneId == 100 && pinCode == QLatin1String("1234"))
    {
        return true;
    }

    return false;
}


quint8 IAS_ArmModeFromString(const QString &armMode)
{
    quint8 result = 0;
    const auto i = std::find(ArmModeListReturn.cbegin(), ArmModeListReturn.cend(), armMode);

    if (i != ArmModeListReturn.cend())
    {
        result = static_cast<quint8>(std::distance(ArmModeListReturn.cbegin(), i));
    }

    return result;
}

bool IAS_DeviceAllowedToArmZone(IASZone *zone, const deCONZ::Address &srcAddress)
{
    Q_UNUSED(zone);
    Q_UNUSED(srcAddress);
    return true;
}

quint8 IAS_HandleArmCommand(IASZone *zone, quint8 armMode, const QString &pinCode, const deCONZ::Address &srcAddress)
{
    if (!zone || armMode > IAS_ACE_ARM_MODE_ARM_ALL_ZONES)
    {
        return IAS_ACE_ARM_NOTF_NOT_READY_TO_ARM;
    }

    const quint8 zoneId = zone->item(RConfigIasZoneId)->toNumber();
    ResourceItem *armModeItem = zone->item(RConfigArmMode);
    const quint8 armMode0 = IAS_ArmModeFromString(armModeItem->toString());

    if (!pinCode.isEmpty() && !IAS_IsValidPinCode(zoneId, pinCode))
    {
        return IAS_ACE_ARM_NOTF_INVALID_ARM_DISARM_CODE;
    }

    if (pinCode.isEmpty() && !IAS_DeviceAllowedToArmZone(zone, srcAddress))
    {
        return IAS_ACE_ARM_NOTF_INVALID_ARM_DISARM_CODE; // correct status?
    }

    if (armMode0 == IAS_ACE_ARM_MODE_DISARM && armMode == armMode0)
    {
        return IAS_ACE_ARM_NOTF_ALREADY_DISARMED;
    }

    quint8 result = IAS_ACE_ARM_NOTF_NOT_READY_TO_ARM;

    if (armMode == IAS_ACE_ARM_MODE_ARM_ALL_ZONES)
    {
        result = IAS_ACE_ARM_NOTF_ALL_ZONES_ARMED;
    }
    else if (armMode == IAS_ACE_ARM_MODE_DISARM)
    {
        result = IAS_ACE_ARM_NOTF_ALL_ZONES_DISARMED;
    }
    else if (armMode == IAS_ACE_ARM_MODE_ARM_DAY_HOME_ZONES_ONLY)
    {
        result = IAS_ACE_ARM_NOTF_ONLY_DAY_HOME_ZONES_ARMED;
    }
    else if (armMode == IAS_ACE_ARM_MODE_ARM_NIGHT_SLEEP_ZONES_ONLY)
    {
        result = IAS_ACE_ARM_NOTF_ONLY_NIGHT_SLEEP_ZONES_ARMED;
    }
    else
    {
        return result;
    }

    if (armMode0 != armMode)
    {
        armModeItem->setValue(QString(ArmModeListReturn[result]));
        plugin->enqueueEvent(Event(RIASZones, RConfigArmMode, zone->item(RAttrId)->toString(), armModeItem));
    }

    return result;
}

void DeRestPluginPrivate::handleIasAceClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    if (zclFrame.isDefaultResponse())
    {
        return;
    }
    
    DBG_Printf(DBG_IAS, "[IAS ACE] - Address 0x%016llX, Payload %s, Command 0x%02X\n", ind.srcAddress().ext(), qPrintable(zclFrame.payload().toHex()), zclFrame.commandId());

    if (zclFrame.frameControl() & deCONZ::ZclFCDirectionServerToClient)
    {
        return;
    }

    Sensor *sensor = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), ind.srcEndpoint(), QLatin1String("ZHAAncillaryControl"));
    if (!sensor || !sensor->item(RConfigIasZoneId))
    {
        return;
    }

    QDataStream stream(zclFrame.payload());
    stream.setByteOrder(QDataStream::LittleEndian);   
    
    bool stateUpdated = false;

    if (zclFrame.commandId() == IAS_ACE_CMD_ARM && zclFrame.payload().size() >= 2)
    {
        // payload: [0] enum8 arm mode | [1] string arm/disarm code | [2] u8 zone id

        // [0] arm mode
        const quint8 armMode = static_cast<quint8>(zclFrame.payload().at(0));

        if (armMode > IAS_ACE_ARM_MODE_ARM_ALL_ZONES)
        {
            DBG_Printf(DBG_IAS, "[IAS ACE] 0x%016llX invalid arm mode: %d, skip\n", ind.srcAddress().ext(), armMode);
            return;
        }
        
        // [2] zone id
        const quint8 zoneId = static_cast<quint8>(zclFrame.payload().at(zclFrame.payload().size() - 1));
        
        DBG_Printf(DBG_IAS, "[IAS ACE] 0x%016llX arm command received, arm mode: 0x%02X, Zone id: %u\n", ind.srcAddress().ext(), armMode, zoneId);

        QString armCode;

        // [1] arm/disarm code in payload (allowed to be empty, e.g. for keyfobs)
        if (zclFrame.payload().size() > 2)
        {
            armCode = QString::fromUtf8(zclFrame.payload().constData() + 1, zclFrame.payload().size() - 2);
        }

        IASZone *zone = IAS_GetZone(zoneId);

        if (!zone)
        {
            return;
        }

        quint8 armRsp = IAS_HandleArmCommand(zone, armMode, armCode, ind.srcAddress());

        sendArmResponse(ind, zclFrame, armRsp);
    }
    else if (zclFrame.commandId() == IAS_ACE_CMD_GET_PANEL_STATUS)
    {
        IASZone *zone = IAS_GetZone(sensor->item(RConfigIasZoneId)->toNumber());

        if (!zone)
        {
            return;
        }


        quint8 panelStatus = 0; // disarmed
        
        ResourceItem *item = sensor->item(RConfigPanel);
        if (item)
        {
            const auto i = std::find(PanelStatusList.cbegin(), PanelStatusList.cend(), item->toString());

            if (i != PanelStatusList.cend())
            {
                panelStatus = static_cast<quint8>(std::distance(PanelStatusList.cbegin(), i));
            }
            else
            {
                DBG_Printf(DBG_IAS, "[IAS ACE] : Unknow PanelStatus, default to 'disarmed'\n");
            }
        }

        sendGetPanelStatusResponse(ind, zclFrame, panelStatus, 0x00);
        
        // Update too the presence detection, this device have one, triger when you move front of it
        if (sensor->modelId() == QLatin1String("URC4450BC0-X-R") ||
            sensor->modelId() == QLatin1String("3405-L"))
        {
            Sensor *sensor2 = getSensorNodeForAddressAndEndpoint(sensor->address(), sensor->fingerPrint().endpoint, QLatin1String("ZHAPresence"));
            if (sensor2)
            {
                ResourceItem *item2 = sensor2->item(RStatePresence);
                if (item2)
                {
                    item2->setValue(true);
                }
                
                item2 = sensor2->item(RConfigDuration);
                if (item2 && item2->toNumber() > 0)
                {
                    sensor2->durationDue = item2->lastSet().addSecs(item2->toNumber());
                }
                
                sensor2->updateStateTimestamp();
                enqueueEvent(Event(RSensors, RStatePresence, sensor2->id()));
                updateSensorEtag(&*sensor2);
            }
        }        
    }
    
    if (stateUpdated)
    {
        sensor->updateStateTimestamp();
        enqueueEvent(Event(RSensors, RStateLastUpdated, sensor->id()));
        updateSensorEtag(sensor);
        sensor->setNeedSaveDatabase(true);
        queSaveDb(DB_SENSORS, DB_SHORT_SAVE_DELAY);
    }
}

void DeRestPluginPrivate::sendArmResponse(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame, quint8 armMode)
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
    req.setSrcEndpoint(endpoint());

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

    if (apsCtrl && apsCtrl->apsdeDataRequest(req) != deCONZ::Success)
    {
        DBG_Printf(DBG_IAS, "[IAS ACE] - Failed to send IAS ACE arm reponse.\n");
    }
}

void DeRestPluginPrivate::sendGetPanelStatusResponse(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame , quint8 PanelStatus, quint8 secs)
{

    deCONZ::ApsDataRequest req;
    deCONZ::ZclFrame outZclFrame;

    req.setProfileId(ind.profileId());
    req.setClusterId(ind.clusterId());
    req.setDstAddressMode(ind.srcAddressMode());
    req.dstAddress() = ind.srcAddress();
    req.setDstEndpoint(ind.srcEndpoint());
    req.setSrcEndpoint(endpoint());

    outZclFrame.setSequenceNumber(zclFrame.sequenceNumber());
    outZclFrame.setCommandId(IAS_ACE_CMD_GET_PANEL_STATUS_RESPONSE);

    outZclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                                deCONZ::ZclFCDirectionServerToClient); // deCONZ::ZclFCDisableDefaultResponse

    // The Seconds Remaining parameter SHALL be provided if the Panel Status parameter has a value of 0x04
    // (Exit delay) or 0x05 (Entry delay).

    { // payload
        QDataStream stream(&outZclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream << (quint8) PanelStatus; // Panel status
        stream << (quint8) secs; // Seconds Remaining 
        stream << (quint8) 0x01; // Audible Notification
        stream << (quint8) 0x00; // Alarm status
    }

    { // ZCL frame
        QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        outZclFrame.writeToStream(stream);
    }

    if (apsCtrlWrapper.apsdeDataRequest(req) != deCONZ::Success)
    {
        DBG_Printf(DBG_IAS, "[IAS ACE] - Failed to send IAS ACE get panel reponse.\n");
    }
}

bool DeRestPluginPrivate::addTaskPanelStatusChanged(TaskItem &task, const QString &mode, bool sound)
{
    task.taskType = TaskIASACE;

    task.req.setClusterId(IAS_ACE_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(IAS_ACE_CMD_PANEL_STATUS_CHANGED);
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                                  deCONZ::ZclFCDirectionServerToClient); //| deCONZ::ZclFCDisableDefaultResponse);
     // payload
    QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    //data
    int panelstatus = 0; //PanelStatusList.indexOf(mode);
    
    //Unknow mode ?
    if (panelstatus < 0)
    {
        return false;
    }
    
    stream << static_cast<quint8>(panelstatus);
    
    // The Seconds Remaining parameter SHALL be provided if the Panel Status parameter has a value of 0x04
    // (Exit delay) or 0x05 (Entry delay).
    if (panelstatus == 0x04 || panelstatus == 0x05)
    {
        stream << (quint8) 0x05; // Seconds Remaining
    }
    else
    {
       stream << (quint8) 0x00; // Seconds Remaining 
    }
    
    if (sound)
    {
        stream << (quint8) 0x01; // Audible Notification
    }
    else
    {
        stream << (quint8) 0x00; // Audible Notification
    }
    stream << (quint8) 0x00; // Alarm status

    // ZCL frame
    {
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    return addTask(task);
}

bool DeRestPluginPrivate::addTaskSendArmResponse(TaskItem &task, const QString &mode, quint8 sn)
{
    task.taskType = TaskIASACE;

    task.req.setClusterId(IAS_ACE_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(sn);
    task.zclFrame.setCommandId(IAS_ACE_CMD_ARM_RESPONSE);

    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionServerToClient |
                             deCONZ::ZclFCDisableDefaultResponse);
     // payload
    QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    
    quint8 armmode = 0;
    
    armmode = 0; //ArmModeListReturn.indexOf(mode);
    if (armmode > ArmModeListReturn.size())
    {
        return false;
    }

    //data
    stream << (quint8) armmode; // Alarm status

    // ZCL frame
    {
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    return addTask(task);
}
