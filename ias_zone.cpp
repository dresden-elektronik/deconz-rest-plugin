/*
 * Copyright (c) 2017-2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "device_descriptions.h"
#include "ias_zone.h"

/*
    IAS Zone Enrollment is handled in a per device state machine.
    The actual state is managed via RConfigEnrolled as state variable.
    State timeouts are based on the ResourceItem::lastSet() timestamp.

    A IAS device is enrolled if:

      1. CIE address written
      2. Zone state = 1
      3. Both values are verified by read

    The state machine ensures all tasks are done and recovers automatically
    from any errors which might happen.

    The state machine is described in following PlantUML diagram,
    and can be displayed with online PlantUML viewer: http://www.plantuml.com/plantuml/uml
*/

/*

@startuml
hide empty description
state Init
state "Read Attributes" as Read
state "Wait Read Response" as WaitRead
state "Write CIE Address" as WriteCieAddr
state "Wait Write Response" as WaitWriteCieAddr
state ReadRsp <<choice>>
state "Delay Enroll" as DelayEnroll
state "Enroll" as Enroll
state "Wait Enroll" as WaitEnroll



[*] --> Init
Init : Mark CIE Address and
Init : Zone State unknown.

Init --> Read
Read : CIE Address
Read : Zone State
Read --> WaitRead :  Command Send
Read --> Read

WaitRead --> Init: 8 sec. Timeout\nError
WaitRead --> ReadRsp : Read Attributes\nResponse

ReadRsp --> WriteCieAddr : Invalid CIE Address
ReadRsp --> DelayEnroll : Valid CIE Address\nZone State = 0
ReadRsp --> Enrolled : Valid CIE Address\nZone State = 1

DelayEnroll --> Enroll : After 5 sec. or\nReceiving\nEnroll Request
Enroll --> WaitEnroll : Command Send
Enroll --> Enroll
WaitEnroll --> Read : After 2 sec.

WriteCieAddr --> WaitWriteCieAddr : Command Send
WriteCieAddr --> WriteCieAddr

WaitWriteCieAddr --> Read : Write Attribute\nResponse
WaitWriteCieAddr --> Init: 8 sec. Timeout\nError

Enrolled --> [*]
@enduml

*/

/*! Helper to set IAS device state and print debug information on state changes.
 */
#define IAS_SetState(sensor, item, state) IAS_SetState1(sensor, item, state, #state)
static quint32 IAS_SetState1(const Sensor *sensor, ResourceItem *item, quint32 state, const char *strState)
{
    DBG_Assert(item);

    if (item->toNumber() != state)
    {
        DBG_Printf(DBG_IAS, "[IAS ZONE] - 0x%016llX set state: %s (%u)\n", sensor->address().ext(), strState, state);
        item->setValue(state);
    }

    return state;
}

/*! Sanity function to ensure IAS state variable has a valid value.
    A invalid value will be set to IAS_STATE_INIT.
 */
static void IAS_EnsureValidState(ResourceItem *itemIasState)
{
    DBG_Assert(itemIasState);
    if (itemIasState && itemIasState->toNumber() >= IAS_STATE_MAX)
    {
        DBG_Printf(DBG_IAS, "[IAS ZONE] - invalid state: %u, set to IAS_STATE_INIT\n", itemIasState->toNumber());
        itemIasState->setValue(IAS_STATE_INIT);
    }
}

/*! Configure presence restoration timer */
static void IAS_QueueRestorePresence(Sensor *const sensor, const ResourceItem &presence)
{
    const NodeValue &val = sensor->getZclValue(IAS_ZONE_CLUSTER_ID, IAS_ZONE_CLUSTER_ATTR_ZONE_STATUS_ID);
    const ResourceItem *const duration = sensor->item(RConfigDuration);
    if (val.maxInterval > 0)
    {
        sensor->durationDue = presence.lastSet().addSecs(val.maxInterval);
    }
    else if (duration && duration->toNumber() > 0)
    {
        sensor->durationDue = presence.lastSet().addSecs(duration->toNumber());
    }
}

/*! Check whether a sensor sends Zone Status Change when an alarm is reset */
static bool IAS_SensorSendsRestoreReports(const Sensor &sensor, const quint16 zoneStatus)
{
    if (zoneStatus & STATUS_RESTORE_REP)
    {
        return true;
    }
    const std::array<const QLatin1String, 5> supportedSensors = {
        QLatin1String("TY0202"),
        QLatin1String("MS01"),
        QLatin1String("MSO1"),
        QLatin1String("ms01"),
        QLatin1String("66666")
    };
    return std::find(supportedSensors.cbegin(), supportedSensors.cend(), sensor.modelId()) != supportedSensors.cend();
}

/*! Handle packets related to the ZCL IAS Zone cluster.
    \param ind - The APS level data indication containing the ZCL packet
    \param zclFrame - The actual ZCL frame which holds the IAS zone server command
 */
void DeRestPluginPrivate::handleIasZoneClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    QDataStream stream(zclFrame.payload());
    stream.setByteOrder(QDataStream::LittleEndian);

    if (!(zclFrame.frameControl() & deCONZ::ZclFCDirectionServerToClient))
    {
        return;
    }
    
    DBG_Printf(DBG_IAS, "[IAS ZONE] - Address 0x%016llX, Payload %s, Command 0x%02X\n", ind.srcAddress().ext(), qPrintable(zclFrame.payload().toHex()), zclFrame.commandId());

    // during setup the IAS Zone type will be read
    // start to proceed discovery here
    if (searchSensorsState == SearchSensorsActive)
    {
        if (!fastProbeTimer->isActive())
        {
            fastProbeTimer->start(5);
        }
    }

    Sensor *sensor = nullptr;
    ResourceItem *itemIasState = nullptr;
    ResourceItem *itemPending = nullptr;

    for (auto &s : sensors)
    {
        if (!(s.address().ext() == ind.srcAddress().ext() &&
              s.fingerPrint().endpoint == ind.srcEndpoint() &&
              s.fingerPrint().hasInCluster(IAS_ZONE_CLUSTER_ID) &&
              s.deletedState() == Sensor::StateNormal))
        {
            continue;
        }
        
        //check if the device have itemIasState and itemPending, because a device can have many sensor and not this field on all
        itemIasState = s.item(RConfigEnrolled);
        itemPending = s.item(RConfigPending);

        if (!itemIasState || !itemPending)
        {
            continue;
        }

        sensor = &s;
    }

    DBG_Assert(itemIasState);
    DBG_Assert(itemPending);

    if (!sensor)
    {
        DBG_Printf(DBG_IAS, "[IAS ZONE] - 0x%016llX No IAS sensor found for endpoint: 0x%02X\n", ind.srcAddress().ext(), ind.srcEndpoint());
        return;
    }

    IAS_EnsureValidState(itemIasState);

    bool isReadAttr = false;
    bool isReporting = false;
    bool isWriteResponse = false;
    bool isClusterCmd = false;
    if (zclFrame.isProfileWideCommand() && zclFrame.commandId() == deCONZ::ZclReadAttributesResponseId)
    {
        isReadAttr = true;
    }
    if (zclFrame.isProfileWideCommand() && zclFrame.commandId() == deCONZ::ZclReportAttributesId)
    {
        isReporting = true;
    }
    if (zclFrame.isProfileWideCommand() && zclFrame.commandId() == deCONZ::ZclWriteAttributesResponseId)
    {
        isWriteResponse = true;
    }
    if ((zclFrame.frameControl() & 0x09) == (deCONZ::ZclFCDirectionServerToClient | deCONZ::ZclFCClusterCommand))
    {
        isClusterCmd = true;
    }

    // Read ZCL Reporting and ZCL Read Attributes Response
    if (isReadAttr || isReporting)
    {
        const NodeValue::UpdateType updateType = isReadAttr ? NodeValue::UpdateByZclRead : NodeValue::UpdateByZclReport;

        if (isReadAttr)
        {
            DBG_Printf(DBG_IAS, "[IAS ZONE] - 0x%016llX Read attributes response:\n", sensor->address().ext());
        }

        while (!stream.atEnd())
        {
            quint16 attrId;
            quint8 attrTypeId;

            stream >> attrId;
            if (isReadAttr)
            {
                quint8 status;
                stream >> status;  // Read Attribute Response status
                if (status != deCONZ::ZclSuccessStatus)
                {
                    DBG_Printf(DBG_IAS, "[IAS ZONE] - 0x%016llX Read attribute 0x%04X status: 0x%02X\n", sensor->address().ext(), attrId, status);
                    continue;
                }
            }
            stream >> attrTypeId;

            deCONZ::ZclAttribute attr(attrId, attrTypeId, QLatin1String(""), deCONZ::ZclRead, false);

            if (!attr.readFromStream(stream))
            {
                continue;
            }

            DBG_Assert(stream.status() == QDataStream::Ok);

            switch (attrId)
            {
                case IAS_ZONE_STATE:
                {
                    const quint8 iasZoneState = attr.numericValue().u8;

                    if (iasZoneState == 1)
                    {
                        DBG_Printf(DBG_IAS, "[IAS ZONE] - 0x%016llX   -> IAS Zone State: enrolled.\n", sensor->address().ext());
                        R_ClearFlags(itemPending, R_PENDING_ENROLL_RESPONSE);
                    }
                    else if (iasZoneState == 0)
                    {
                        DBG_Printf(DBG_IAS, "[IAS ZONE] - 0x%016llX   -> IAS Zone State: NOT enrolled.\n", sensor->address().ext());
                        R_SetFlags(itemPending, R_PENDING_ENROLL_RESPONSE);
                    }

                    sensor->setZclValue(updateType, ind.srcEndpoint(), IAS_ZONE_CLUSTER_ID, attrId, attr.numericValue());
                }
                    break;

                case IAS_ZONE_TYPE:
                {
                    sensor->setZclValue(updateType, ind.srcEndpoint(), IAS_ZONE_CLUSTER_ID, attrId, attr.numericValue());
                }
                    break;

                case IAS_ZONE_STATUS:
                {
                    if (!DEV_TestStrict())
                    {
                        const quint16 zoneStatus = attr.numericValue().u16;   // might be reported or received via CMD_STATUS_CHANGE_NOTIFICATION

                        processIasZoneStatus(sensor, zoneStatus, updateType);
                    }

                }
                    break;

                case IAS_CIE_ADDRESS:
                {
                    const quint64 iasCieAddress = attr.numericValue().u64;

                    if (iasCieAddress != 0 && iasCieAddress != 0xFFFFFFFFFFFFFFFF)
                    {
                        DBG_Assert(iasCieAddress == apsCtrl->getParameter(deCONZ::ParamMacAddress));
                        DBG_Printf(DBG_IAS, "[IAS ZONE] - 0x%016llX   -> IAS CIE address = 0x%016llX: already written.\n", sensor->address().ext(), iasCieAddress);
                        R_ClearFlags(itemPending, R_PENDING_WRITE_CIE_ADDRESS);
                    }
                    else
                    {
                        DBG_Printf(DBG_IAS, "[IAS ZONE] - 0x%016llX   -> IAS CIE address = 0x%016llX: NOT written.\n", sensor->address().ext());
                        R_SetFlags(itemPending, R_PENDING_WRITE_CIE_ADDRESS);
                    }

                    sensor->setZclValue(updateType, ind.srcEndpoint(), IAS_ZONE_CLUSTER_ID, attrId, attr.numericValue());
                }
                    break;

                default:
                    break;

            }
        }

        if (itemIasState->toNumber() == IAS_STATE_WAIT_READ)
        {
            // Read attributes response, decide next state.
            if (R_HasFlags(itemPending, R_PENDING_WRITE_CIE_ADDRESS)) // 1. task to be setup
            {
                IAS_SetState(sensor, itemIasState, IAS_STATE_WRITE_CIE_ADDR);
            }
            else if (R_HasFlags(itemPending, R_PENDING_ENROLL_RESPONSE)) // 2. task to be setup
            {
                IAS_SetState(sensor, itemIasState, IAS_STATE_DELAY_ENROLL);
            }
            else
            {
                // Valid CIE Address and Zone State = 1 --> finished
                IAS_SetState(sensor, itemIasState, IAS_STATE_ENROLLED);
                sensor->setNeedSaveDatabase(true);
            }
        }

        checkIasEnrollmentStatus(sensor);
    }

    // Read ZCL Cluster Command Response
    if (isClusterCmd && zclFrame.commandId() == CMD_STATUS_CHANGE_NOTIFICATION)
    {
        if (!DEV_TestStrict())
        {
            quint16 zoneStatus;
            quint8 extendedStatus;
            quint8 zoneId;
            quint16 delay;
            stream >> zoneStatus;
            stream >> extendedStatus; // reserved, set to 0
            stream >> zoneId;
            stream >> delay;
            DBG_Assert(stream.status() == QDataStream::Ok);
            DBG_Printf(DBG_IAS, "[IAS ZONE] - 0x%016llX Status Change, status: 0x%04X, zoneId: %u, delay: %u\n", sensor->address().ext(), zoneStatus, zoneId, delay);

            processIasZoneStatus(sensor, zoneStatus, NodeValue::UpdateByZclReport);
        }
        
        checkIasEnrollmentStatus(sensor);
    }
    else if (isClusterCmd && zclFrame.commandId() == CMD_ZONE_ENROLL_REQUEST)
    {
        quint16 zoneType;
        quint16 manufacturer;

        stream >> zoneType;
        stream >> manufacturer;
        DBG_Assert(stream.status() == QDataStream::Ok);

        DBG_Printf(DBG_IAS, "[IAS ZONE] - 0x%016llX Zone Enroll Request, zone type: 0x%04X, manufacturer: 0x%04X\n", sensor->address().ext(), zoneType, manufacturer);

        if (itemIasState->toNumber() == IAS_STATE_DELAY_ENROLL ||
            itemIasState->toNumber() == IAS_STATE_ENROLL) // This state might still be active if previous send didn't work
        {
            // End waiting and send Enroll Response within state machine.
            IAS_SetState(sensor, itemIasState, IAS_STATE_ENROLL);
            checkIasEnrollmentStatus(sensor);
        }
        else
        {
            // Send independend of state to don't interfere with state machine.
            sendIasZoneEnrollResponse(ind, zclFrame);
        }
        return; // don't trigger ZCL Default Response
    }

    // ZCL Write Attributes Response
    if (isWriteResponse)
    {
        DBG_Printf(DBG_IAS, "[IAS ZONE] - 0x%016llX Write of IAS CIE address done.\n", sensor->address().ext());

        if (itemIasState->toNumber() == IAS_STATE_WAIT_WRITE_CIE_ADDR)
        {
            // read attributes again to see if it worked
            IAS_SetState(sensor, itemIasState, IAS_STATE_READ);
        }

        checkIasEnrollmentStatus(sensor);
    }
}

/*! Processes the received IAS zone status value.
    \param sensor - Sensor containing the IAS zone cluster
    \param zoneStatus - IAS zone status value
    \param updateType - Update type
 */
void DeRestPluginPrivate::processIasZoneStatus(Sensor *sensor, quint16 zoneStatus, NodeValue::UpdateType updateType)
{
    ResourceItem *item2;

    // Valid for all devices type
    item2 = sensor->item(RStateLowBattery);
    if (item2)
    {
        bool battery = (zoneStatus & STATUS_BATTERY) ? true : false;
        item2->setValue(battery);
        enqueueEvent(Event(RSensors, RStateLowBattery, sensor->id(), item2));
    }

    item2 = sensor->item(RStateTampered);
    if (item2)
    {
        bool tamper = (zoneStatus & STATUS_TAMPER) ? true : false;
        item2->setValue(tamper);
        enqueueEvent(Event(RSensors, RStateTampered, sensor->id(), item2));
    }
    
    item2 = sensor->item(RConfigReachable);
    if (item2 && !item2->toBool())
    {
        item2->setValue(true);
        enqueueEvent(Event(RSensors, RConfigReachable, sensor->id(), item2));
    }
    
    const char *attr = nullptr;
    if (sensor->type() == QLatin1String("ZHAAlarm"))
    {
        attr = RStateAlarm;
    }
    else if (sensor->type() == QLatin1String("ZHACarbonMonoxide"))
    {
        attr = RStateCarbonMonoxide;
    }
    else if (sensor->type() == QLatin1String("ZHAFire"))
    {
        attr = RStateFire;
    }
    else if (sensor->type() == QLatin1String("ZHAOpenClose"))
    {
        attr = RStateOpen;
    }
    else if (sensor->type() == QLatin1String("ZHAPresence"))
    {
        attr = RStatePresence;
    }
    else if (sensor->type() == QLatin1String("ZHAVibration"))
    {
        attr = RStateVibration;
    }
    else if (sensor->type() == QLatin1String("ZHAWater"))
    {
        attr = RStateWater;
    }

    ResourceItem *item = nullptr;
    if (attr)
    {
        item = sensor->item(attr);
    }

    if (item)
    {
        bool alarm = (zoneStatus & (STATUS_ALARM1 | STATUS_ALARM2)) ? true : false;
        item->setValue(alarm);
        enqueueEvent(Event(RSensors, item->descriptor().suffix, sensor->id(), item));

        // TODO DDF DDF_AnnoteZclParseCommand()
        DDF_AnnoteZclParse(sensor, item, 0, IAS_ZONE_CLUSTER_ID, IAS_ZONE_STATUS, "Item.val = (Attr.val & 0x3) != 0");

        item2 = sensor->item(RStateTest);
        if (item2)
        {
            bool test = (zoneStatus & STATUS_TEST) ? true : false;
            item2->setValue(test);
            enqueueEvent(Event(RSensors, RStateTest, sensor->id(), item2));
        }

        deCONZ::NumericUnion num = {0};
        num.u16 = zoneStatus;
        sensor->setZclValue(updateType, sensor->fingerPrint().endpoint, IAS_ZONE_CLUSTER_ID, IAS_ZONE_CLUSTER_ATTR_ZONE_STATUS_ID, num);

        if (alarm && item->descriptor().suffix == RStatePresence)
        {
            if (!IAS_SensorSendsRestoreReports(*sensor, zoneStatus)) {
                IAS_QueueRestorePresence(sensor, *item);
            }
        }
    }
    
    sensor->updateStateTimestamp();
    enqueueEvent(Event(RSensors, RStateLastUpdated, sensor->id()));

    updateEtag(sensor->etag);
    updateEtag(gwConfigEtag);
    sensor->setNeedSaveDatabase(true);
    queSaveDb(DB_SENSORS, DB_LONG_SAVE_DELAY);
    
}

/*! Sends IAS Zone enroll response to IAS Zone server.
    \param ind - The APS level data indication containing the ZCL packet
    \param zclFrame - The actual ZCL frame which holds the IAS Zone enroll request
 */
bool DeRestPluginPrivate::sendIasZoneEnrollResponse(Sensor *sensor)
{
    deCONZ::ApsDataRequest req;
    deCONZ::ZclFrame outZclFrame;

    req.setProfileId(sensor->fingerPrint().profileId);
    req.setClusterId(IAS_ZONE_CLUSTER_ID); // todo check for other ias clusters
    req.setDstAddressMode(deCONZ::ApsNwkAddress);
    req.dstAddress() = sensor->address();
    req.setDstEndpoint(sensor->fingerPrint().endpoint);
    req.setSrcEndpoint(endpoint());

    outZclFrame.setSequenceNumber(zclSeq++);
    outZclFrame.setCommandId(CMD_ZONE_ENROLL_RESPONSE);

    outZclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCDisableDefaultResponse);

    { // payload
        QDataStream stream(&outZclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        quint8 code = 0x00; // success
        quint8 zoneId = IAS_DEFAULT_ZONE;

        stream << code;
        stream << zoneId;
    }

    { // ZCL frame
        QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        outZclFrame.writeToStream(stream);
    }

    DBG_Printf(DBG_IAS, "[IAS ZONE] - 0x%016llX Send Zone Enroll Response, zcl.seq: %u\n", sensor->address().ext(), outZclFrame.sequenceNumber());

    if (apsCtrlWrapper.apsdeDataRequest(req) != deCONZ::Success)
    {
        DBG_Printf(DBG_IAS, "[IAS ZONE] - 0x%016llX Failed sending Zone Enroll Response\n", sensor->address().ext());
        return false;
    }

    return true;
}

/*! Sends IAS Zone enroll response to IAS Zone server.
    \param ind - The APS level data indication containing the ZCL packet
    \param zclFrame - The actual ZCL frame which holds the IAS Zone enroll request
 */
bool DeRestPluginPrivate::sendIasZoneEnrollResponse(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
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
    outZclFrame.setCommandId(CMD_ZONE_ENROLL_RESPONSE);

    outZclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCDisableDefaultResponse);

    { // payload
        QDataStream stream(&outZclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        quint8 code = 0x00; // success
        quint8 zoneId = IAS_DEFAULT_ZONE;

        stream << code;
        stream << zoneId;
    }

    { // ZCL frame
        QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        outZclFrame.writeToStream(stream);
    }

    DBG_Printf(DBG_IAS, "[IAS ZONE] - 0x%016llX Send Zone Enroll Response, zcl.seq: %u\n", ind.srcAddress().ext(), zclFrame.sequenceNumber());

    if (apsCtrlWrapper.apsdeDataRequest(req) != deCONZ::Success)
    {
        DBG_Printf(DBG_IAS, "[IAS ZONE] - 0x%016llX Failed sending Zone Enroll Response\n", ind.srcAddress().ext());
        return false;
    }

    return true;
}

/*! Drives the IAS Zone Enrollment state machine.

    This handler can be called at any time, e.g. after receiving a command or from a timer.
    \param sensor - Sensor containing the IAS zone cluster
 */
void DeRestPluginPrivate::checkIasEnrollmentStatus(Sensor *sensor)
{
    ResourceItem *itemIasState = sensor->item(RConfigEnrolled); // holds per device IAS state variable

    if (!itemIasState)
    {
        return;
    }

    ResourceItem *itemPending = sensor->item(RConfigPending);
    if (!itemPending)
    {
        // All IAS devices should have config.enrolled and config.pending items.
        // Bail out early for non IAS devices.
        return;
    }

    IAS_EnsureValidState(itemIasState);
    quint32 iasState = itemIasState->toNumber();

    if (iasState == IAS_STATE_ENROLLED)
    {
        DBG_Printf(DBG_IAS, "[IAS ZONE] - 0x%016llX Sensor (%s) is enrolled.\n", sensor->address().ext(), qPrintable(sensor->type()));
        return; // already enrolled nothing todo
    }

    if (sensor->fingerPrint().hasInCluster(IAS_ZONE_CLUSTER_ID))
    {
        const auto now = QDateTime::currentDateTime();

        if (iasState != IAS_STATE_WAIT_READ) // don't print in WAIT_READ since it's too noisy
        {
            DBG_Printf(DBG_IAS, "[IAS ZONE] - 0x%016llX Sensor ID: %s\n", sensor->address().ext(), qPrintable(sensor->uniqueId()));
            DBG_Printf(DBG_IAS, "[IAS ZONE] - 0x%016llX Sensor type: %s\n", sensor->address().ext(), qPrintable(sensor->type()));

            const NodeValue val = sensor->getZclValue(IAS_ZONE_CLUSTER_ID, IAS_ZONE_STATE);
            DBG_Printf(DBG_IAS, "[IAS ZONE] - 0x%016llX Sensor zone state value: %d\n", sensor->address().ext(), val.value.u8);

            const NodeValue val1 = sensor->getZclValue(IAS_ZONE_CLUSTER_ID, IAS_CIE_ADDRESS);
            DBG_Printf(DBG_IAS, "[IAS ZONE] - 0x%016llX Sensor IAS CIE address: 0x%016llX\n", sensor->address().ext(), val1.value.u64);
            DBG_Printf(DBG_IAS, "[IAS ZONE] - 0x%016llX Sensor config pending value: %d\n", sensor->address().ext(), itemPending->toNumber());
        }

        if (iasState == IAS_STATE_INIT)
        {
            // At the beginning we don't know device values of CIE address and Zone state.
            // The device might already be enrolled, which will be verified by IAS_STATE_READ.
            DBG_Printf(DBG_IAS, "[IAS ZONE] - 0x%016llX Sensor init enrollment.\n", sensor->address().ext());
            R_SetFlags(itemPending, R_PENDING_ENROLL_RESPONSE | R_PENDING_WRITE_CIE_ADDRESS);
            iasState = IAS_SetState(sensor, itemIasState, IAS_STATE_READ);
        }
        else if (iasState == IAS_STATE_DELAY_ENROLL)
        {
            // Some devices don't send an Enroll Request.
            // Wait a few seconds, and if no Enroll Request is received move on to IAS_STATE_ENROLL
            // to send an unsoliticed Enroll Response.
            const auto dt = itemIasState->lastSet().secsTo(now);

            if (dt > 5)
            {
                DBG_Printf(DBG_IAS, "[IAS ZONE] - 0x%016llX initiate unsoliticed enroll response after %d seconds delay.\n", sensor->address().ext(), static_cast<int>(dt));
                iasState = IAS_SetState(sensor, itemIasState, IAS_STATE_ENROLL);
            }
        }
        else if (iasState == IAS_STATE_WAIT_ENROLL)
        {
            // After sending a Enroll Response, wait a few seconds and read the attributes again to verify.
            const auto dt = itemIasState->lastSet().secsTo(now);

            if (dt > 2)
            {
                // Read attributes again to verify if it worked.
                iasState = IAS_SetState(sensor, itemIasState, IAS_STATE_READ);
            }
        }

        if (!R_HasFlags(itemPending, R_PENDING_ENROLL_RESPONSE) && !R_HasFlags(itemPending, R_PENDING_WRITE_CIE_ADDRESS))
        {
            if (iasState != IAS_STATE_ENROLLED) // everything seems to be done, finish here
            {
                IAS_SetState(sensor, itemIasState, IAS_STATE_ENROLLED);
                sensor->setNeedSaveDatabase(true);
            }
            return;
        }

        if (iasState == IAS_STATE_READ)
        {
            DBG_Printf(DBG_IAS, "[IAS ZONE] - 0x%016llX Read IAS zone state, type and CIE address...\n", sensor->address().ext());

            if (readAttributes(sensor, sensor->fingerPrint().endpoint, IAS_ZONE_CLUSTER_ID, {IAS_ZONE_STATE, IAS_ZONE_TYPE, IAS_CIE_ADDRESS}))
            {
                queryTime = queryTime.addSecs(1);
                IAS_SetState(sensor, itemIasState, IAS_STATE_WAIT_READ);
            }
            else
            {
                // Remain in IAS_STATE_READ and try again in next invocation.
                DBG_Printf(DBG_IAS, "[IAS ZONE] - 0x%016llX Failed to send read attributes.\n", sensor->address().ext());
            }
        }
        else if (iasState == IAS_STATE_WRITE_CIE_ADDR)
        {
            if (writeIasCieAddress(sensor))
            {
                IAS_SetState(sensor, itemIasState, IAS_STATE_WAIT_WRITE_CIE_ADDR);
            }
            // On error remain in IAS_STATE_WRITE_CIE_ADDR and try again in next invocation.
        }
        else if (iasState == IAS_STATE_ENROLL)
        {
            if (sendIasZoneEnrollResponse(sensor))
            {
                IAS_SetState(sensor, itemIasState, IAS_STATE_WAIT_ENROLL);
            }
            // On error remain in IAS_STATE_ENROLL and try again in next invocation.
        }
        else if (iasState == IAS_STATE_WAIT_READ ||
                 iasState == IAS_STATE_WAIT_WRITE_CIE_ADDR)
        {
            const auto dt = itemIasState->lastSet().secsTo(now);

            if (dt > 8) // Wait up to 8 seconds, because next mac poll might take 7.x seconds until max transactions expires.
            {
                DBG_Printf(DBG_IAS, "[IAS ZONE] - 0x%016llX timeout after %d seconds, state: %d, retry...\n", sensor->address().ext(), static_cast<int>(dt), iasState);
                IAS_SetState(sensor, itemIasState, IAS_STATE_INIT);
            }
            else
            {
                DBG_Printf(DBG_IAS, "[IAS ZONE] - 0x%016llX Sensor (%s) enrollment pending... since %d seconds.\n", sensor->address().ext(), qPrintable(sensor->type()), static_cast<int>(dt));
            }
        }
    }
}

/*! Write IAS CIE address attribute for a node.
    \param sensor - Sensor containing the IAS zone cluster
 */
bool DeRestPluginPrivate::writeIasCieAddress(Sensor *sensor)
{
    ResourceItem *item = nullptr;
    item = sensor->item(RConfigPending);

    DBG_Printf(DBG_IAS, "[IAS ZONE] - 0x%016llX Send write IAS CIE address.\n", sensor->address().ext());

    if (sensor->fingerPrint().hasInCluster(IAS_ZONE_CLUSTER_ID) && item && R_HasFlags(item, R_PENDING_WRITE_CIE_ADDRESS))
    {
        // write CIE address needed for some IAS Zone devices
        const quint64 iasCieAddress = apsCtrl->getParameter(deCONZ::ParamMacAddress);
        deCONZ::ZclAttribute attribute(IAS_CIE_ADDRESS, deCONZ::ZclIeeeAddress, QLatin1String("CIE address"), deCONZ::ZclReadWrite, false);
        attribute.setValue(iasCieAddress);

        if (writeAttribute(sensor, sensor->fingerPrint().endpoint, IAS_ZONE_CLUSTER_ID, attribute, 0))
        {
            return true;
        }
    }

    DBG_Printf(DBG_IAS, "[IAS ZONE] - 0x%016llX Failed sending write IAS CIE address.\n", sensor->address().ext());

    return false;
}
