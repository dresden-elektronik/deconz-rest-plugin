/*
 * Copyright (c) 2017-2020 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QString>
#include <QVariantMap>
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "json.h"

// server send
#define CMD_STATUS_CHANGE_NOTIFICATION 0x00
#define CMD_ZONE_ENROLL_REQUEST 0x01
// server receive
#define CMD_ZONE_ENROLL_RESPONSE 0x00

// Zone status flags
#define STATUS_ALARM1         0x0001
#define STATUS_ALARM2         0x0002
#define STATUS_TAMPER         0x0004
#define STATUS_BATTERY        0x0008
#define STATUS_SUPERVISION    0x0010
#define STATUS_RESTORE_REP    0x0020
#define STATUS_TROUBLE        0x0040
#define STATUS_AC_MAINS       0x0080
#define STATUS_TEST           0x0100
#define STATUS_BATTERY_DEFECT 0x0200

// Attributes
#define IAS_ZONE_STATE        0x0000
#define IAS_ZONE_TYPE         0x0001
#define IAS_ZONE_STATUS       0x0002
#define IAS_CIE_ADDRESS       0x0010
#define IAS_ZONE_ID           0x0011


/*! Handle packets related to the ZCL IAS Zone cluster.
    \param ind - The APS level data indication containing the ZCL packet
    \param zclFrame - The actual ZCL frame which holds the IAS zone server command
 */
void DeRestPluginPrivate::handleIasZoneClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    Q_UNUSED(ind);

    QDataStream stream(zclFrame.payload());
    stream.setByteOrder(QDataStream::LittleEndian);

    if (!(zclFrame.frameControl() & deCONZ::ZclFCDirectionServerToClient))
    {
        return;
    }

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

    for (auto &s : sensors)
    {
        if (!(s.address().ext() == ind.srcAddress().ext() && s.fingerPrint().endpoint == ind.srcEndpoint() &&
             s.fingerPrint().hasInCluster(IAS_ZONE_CLUSTER_ID) && s.deletedState() == Sensor::StateNormal))
        {
            continue;
        }

        sensor = &s;
    }

    if (!sensor)
    {
        DBG_Printf(DBG_INFO, "[IAS ZONE] - 0x%016llX No IAS sensor found for endpoint: 0x%02X\n", ind.srcAddress().ext(), ind.srcEndpoint());
        return;
    }

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

    // Read ZCL reporting and ZCL Read Attributes Response
    if (isReadAttr || isReporting)
    {
        const NodeValue::UpdateType updateType = isReadAttr ? NodeValue::UpdateByZclRead : NodeValue::UpdateByZclReport;

        bool configUpdated = false;
        bool stateUpdated = false;

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
                    continue;
                }
            }
            stream >> attrTypeId;

            deCONZ::ZclAttribute attr(attrId, attrTypeId, QLatin1String(""), deCONZ::ZclRead, false);

            if (!attr.readFromStream(stream))
            {
                continue;
            }

            ResourceItem *item = nullptr;
            item = sensor->item(RConfigPending);

            switch (attrId)
            {
                case IAS_ZONE_STATE: // IAS zone state
                {
                    quint8 iasZoneState = attr.numericValue().u8;

                    if (item && iasZoneState == 1)
                    {
                        DBG_Printf(DBG_INFO_L2, "[IAS ZONE] - 0x%016llX Sensor already enrolled (read).\n", sensor->address().ext());
                    }
                    else if (item && iasZoneState == 0)
                    {
                        DBG_Printf(DBG_INFO_L2, "[IAS ZONE] - 0x%016llX Sensor NOT enrolled (read)...\n", sensor->address().ext());
                    }

                    sensor->setZclValue(updateType, ind.srcEndpoint(), IAS_ZONE_CLUSTER_ID, attrId, attr.numericValue());
                    //sensor->setNeedSaveDatabase(true);
                }
                    break;

                case IAS_ZONE_TYPE: // IAS zone type
                {
                    sensor->setZclValue(updateType, ind.srcEndpoint(), IAS_ZONE_CLUSTER_ID, attrId, attr.numericValue());
                }
                    break;

                case IAS_ZONE_STATUS: // IAS zone status
                {
                    quint16 zoneStatus = attr.numericValue().u16;   // might be reported or received via CMD_STATUS_CHANGE_NOTIFICATION

                    processIasZoneStatus(sensor, zoneStatus, updateType);
                    stateUpdated = true;
                }
                    break;

                case IAS_CIE_ADDRESS: // IAS CIE address
                {
                    quint64 iasCieAddress = attr.numericValue().u64;

                    if (item && iasCieAddress != 0 && iasCieAddress != 0xFFFFFFFFFFFFFFFF)
                    {
                        DBG_Printf(DBG_INFO_L2, "[IAS ZONE] - 0x%016llX IAS CIE address already written (read).\n", sensor->address().ext());

                        if ((item->toNumber() & R_PENDING_WRITE_CIE_ADDRESS))
                        {
                            DBG_Printf(DBG_INFO_L2, "[IAS ZONE] - 0x%016llX Removing 'pending IAS CIE write' flag.\n", sensor->address().ext());
                            item->setValue(item->toNumber() & ~R_PENDING_WRITE_CIE_ADDRESS);
                        }
                    }
                    else if (item && iasCieAddress == 0)
                    {
                        DBG_Printf(DBG_INFO_L2, "[IAS ZONE] - 0x%016llX IAS CIE address NOT written (read).\n", sensor->address().ext());

                        if (!(item->toNumber() & R_PENDING_WRITE_CIE_ADDRESS))
                        {
                            DBG_Printf(DBG_INFO_L2, "[IAS ZONE] - 0x%016llX Adding 'pending IAS CIE write' flag since missing.\n", sensor->address().ext());
                            item->setValue(item->toNumber() | R_PENDING_WRITE_CIE_ADDRESS);
                        }
                    }

                    sensor->setZclValue(updateType, ind.srcEndpoint(), IAS_ZONE_CLUSTER_ID, attrId, attr.numericValue());
                    //sensor->setNeedSaveDatabase(true);
                }
                    break;

                default:
                    break;

            }
        }

        if (stateUpdated)
        {
            sensor->updateStateTimestamp();
            enqueueEvent(Event(RSensors, RStateLastUpdated, sensor->id()));
        }

        if (configUpdated || stateUpdated)
        {
            updateEtag(sensor->etag);
            updateEtag(gwConfigEtag);
            sensor->setNeedSaveDatabase(true);
            queSaveDb(DB_SENSORS, DB_SHORT_SAVE_DELAY);
        }

        // Sensor might be uninitialized ???
        NodeValue val = sensor->getZclValue(IAS_ZONE_CLUSTER_ID, IAS_CIE_ADDRESS);
        deCONZ::NumericUnion iasCieAddress = val.value;

        if (iasCieAddress.u64 == 0 || iasCieAddress.u64 == 0xFFFFFFFFFFFFFFFF)
        {
            writeIasCieAddress(sensor);
        }

        checkIasEnrollmentStatus(sensor);
    }

    // Read ZCL Cluster Command Response
    if (isClusterCmd && zclFrame.commandId() == CMD_STATUS_CHANGE_NOTIFICATION)
    {
        quint16 zoneStatus;
        quint8 extendedStatus;
        quint8 zoneId;
        quint16 delay;
        stream >> zoneStatus;
        stream >> extendedStatus; // reserved, set to 0
        stream >> zoneId;
        stream >> delay;
        DBG_Printf(DBG_INFO, "[IAS Zone] - 0x%016llX Status Change, status: 0x%04X, zoneId: %u, delay: %u\n", sensor->address().ext(), zoneStatus, zoneId, delay);

        const NodeValue::UpdateType updateType = NodeValue::UpdateByZclReport;
        processIasZoneStatus(sensor, zoneStatus, updateType);

        sensor->updateStateTimestamp();
        enqueueEvent(Event(RSensors, RStateLastUpdated, sensor->id()));
        updateEtag(sensor->etag);
        updateEtag(gwConfigEtag);
        sensor->setNeedSaveDatabase(true);
        queSaveDb(DB_SENSORS, DB_SHORT_SAVE_DELAY);
        
        checkIasEnrollmentStatus(sensor);
    }
    else if (isClusterCmd && zclFrame.commandId() == CMD_ZONE_ENROLL_REQUEST)
    {
        quint16 zoneType;
        quint16 manufacturer;

        stream >> zoneType;
        stream >> manufacturer;

        sendIasZoneEnrollResponse(ind, zclFrame);

        ResourceItem *item = nullptr;
        item = sensor->item(RConfigPending);

        if (item)
        {
            DBG_Printf(DBG_INFO, "[IAS ZONE] - 0x%016llX Zone Enroll Request, zone type: 0x%04X, manufacturer: 0x%04X\n", sensor->address().ext(), zoneType, manufacturer);
            DBG_Printf(DBG_INFO_L2, "[IAS ZONE] - 0x%016llX Removing 'pending enroll response' flag.\n", sensor->address().ext());
            item->setValue(item->toNumber() & ~R_PENDING_ENROLL_RESPONSE);
        }
        //sensor->setNeedSaveDatabase(true);
        checkIasEnrollmentStatus(sensor);
        return;
    }

    // ZCL Write Attributes Response
    if (isWriteResponse)
    {
        ResourceItem *item = nullptr;
        item = sensor->item(RConfigPending);

        if (item && (item->toNumber() & R_PENDING_WRITE_CIE_ADDRESS))
        {
            item->setValue(item->toNumber() & ~R_PENDING_WRITE_CIE_ADDRESS);
            DBG_Printf(DBG_INFO, "[IAS ZONE] - 0x%016llX Write of IAS CIE address successful.\n", sensor->address().ext());
            DBG_Printf(DBG_INFO_L2, "[IAS ZONE] - 0x%016llX Removing 'pending IAS CIE write' flag.\n", sensor->address().ext());
        }

        checkIasEnrollmentStatus(sensor);
    }

    // Allow clearing the alarm bit for Develco devices
    if (!(zclFrame.frameControl() & deCONZ::ZclFCDisableDefaultResponse))
    {
        sendZclDefaultResponse(ind, zclFrame, deCONZ::ZclSuccessStatus);
    }
}

/*! Processes the received IAS zone status value.
    \param sensor - Sensor containing the IAS zone cluster
    \param zoneStatus - IAS zone status value
    \param updateType - Update type
 */
void DeRestPluginPrivate::processIasZoneStatus(Sensor *sensor, quint16 zoneStatus, NodeValue::UpdateType updateType)
{
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
        sensor->rx();
        sensor->incrementRxCounter();
        bool alarm = (zoneStatus & (STATUS_ALARM1 | STATUS_ALARM2)) ? true : false;
        item->setValue(alarm);
        enqueueEvent(Event(RSensors, item->descriptor().suffix, sensor->id(), item));

        ResourceItem *item2 = sensor->item(RStateLowBattery);
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

        item2 = sensor->item(RConfigReachable);
        if (item2 && !item2->toBool())
        {
            item2->setValue(true);
            enqueueEvent(Event(RSensors, RConfigReachable, sensor->id(), item2));
        }

        if (alarm && item->descriptor().suffix == RStatePresence)
        {   // prepare to automatically set presence to false
            NodeValue &val = sensor->getZclValue(IAS_ZONE_CLUSTER_ID, IAS_ZONE_CLUSTER_ATTR_ZONE_STATUS_ID);

            item2 = sensor->item(RConfigDuration);
            if (val.maxInterval > 0)
            {
                sensor->durationDue = item->lastSet().addSecs(val.maxInterval);
            }
            else if (item2 && item2->toNumber() > 0)
            {
                sensor->durationDue = item->lastSet().addSecs(item2->toNumber());
            }
        }
    }
}

/*! Sends IAS Zone enroll response to IAS Zone server.
    \param ind - The APS level data indication containing the ZCL packet
    \param zclFrame - The actual ZCL frame which holds the IAS Zone enroll request
 */
void DeRestPluginPrivate::sendIasZoneEnrollResponse(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
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
        quint8 zoneId = 100;

        stream << code;
        stream << zoneId;
    }

    { // ZCL frame
        QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        outZclFrame.writeToStream(stream);
    }

    if (apsCtrl && apsCtrl->apsdeDataRequest(req) != deCONZ::Success)
    {
        DBG_Printf(DBG_INFO, "[IAS ZONE] - 0x%016llX Zone failed to send enroll reponse\n", ind.srcAddress().ext());
    }
}

/*! Check if a sensor is already enrolled
    \param sensor - Sensor containing the IAS zone cluster
 */
void DeRestPluginPrivate::checkIasEnrollmentStatus(Sensor *sensor)
{
    if (sensor->fingerPrint().hasInCluster(IAS_ZONE_CLUSTER_ID))
    {

        DBG_Printf(DBG_INFO_L2, "[IAS ZONE] - 0x%016llX Sensor ID: %s\n", sensor->address().ext(), qPrintable(sensor->uniqueId()));
        DBG_Printf(DBG_INFO_L2, "[IAS ZONE] - 0x%016llX Sensor ID: %s\n", sensor->address().ext(), qPrintable(sensor->type()));

        NodeValue val = sensor->getZclValue(IAS_ZONE_CLUSTER_ID, IAS_ZONE_STATE);
        deCONZ::NumericUnion iasZoneStatus = val.value;
        DBG_Printf(DBG_INFO_L2, "[IAS ZONE] - 0x%016llX Sensor zone state timestamp: %s\n", sensor->address().ext(), qPrintable(val.timestamp.toString()));
        DBG_Printf(DBG_INFO_L2, "[IAS ZONE] - 0x%016llX Sensor zone state value: %d\n", sensor->address().ext(), iasZoneStatus.u8);

        NodeValue val1 = sensor->getZclValue(IAS_ZONE_CLUSTER_ID, IAS_CIE_ADDRESS);
        deCONZ::NumericUnion iasCieAddress = val1.value;

        DBG_Printf(DBG_INFO_L2, "[IAS ZONE] - 0x%016llX Sensor IAS CIE address timestamp: %s\n", sensor->address().ext(), qPrintable(val1.timestamp.toString()));
        DBG_Printf(DBG_INFO_L2, "[IAS ZONE] - 0x%016llX Sensor IAS CIE address: 0x%016llX\n", sensor->address().ext(), iasCieAddress.u64);

        ResourceItem *item = nullptr;
        item = sensor->item(RConfigPending);

        if (item)
        {
            DBG_Printf(DBG_INFO_L2, "[IAS ZONE] - 0x%016llX Sensor config pending value: %d\n", sensor->address().ext(), item->toNumber());
        }

        if (iasZoneStatus.u8 == 1 && iasCieAddress.u64 != 0 && iasCieAddress.u64 != 0xFFFFFFFFFFFFFFFF)
        {
            DBG_Printf(DBG_INFO, "[IAS ZONE] - 0x%016llX Sensor enrolled. Removing all pending flags.\n", sensor->address().ext());
            item->setValue(item->toNumber() & ~R_PENDING_WRITE_CIE_ADDRESS);
            item->setValue(item->toNumber() & ~R_PENDING_ENROLL_RESPONSE);
            sensor->setNeedSaveDatabase(true);
            return;
        }

        if (item && item->toNumber() == 0 && iasZoneStatus.u8 == 0)
        {
            DBG_Printf(DBG_INFO, "[IAS ZONE] - 0x%016llX Sensor NOT enrolled (check).\n", sensor->address().ext());

            if ((iasCieAddress.u64 == 0 || iasCieAddress.u64 == 0xFFFFFFFFFFFFFFFF) && !(item->toNumber() & R_PENDING_WRITE_CIE_ADDRESS))
            {
                DBG_Printf(DBG_INFO_L2, "[IAS ZONE] - 0x%016llX Adding 'pending IAS CIE write' flag since missing.\n", sensor->address().ext());
                item->setValue(item->toNumber() | R_PENDING_WRITE_CIE_ADDRESS);
            }
            if (!(item->toNumber() & R_PENDING_ENROLL_RESPONSE))
            {
                DBG_Printf(DBG_INFO_L2, "[IAS ZONE] - 0x%016llX Adding 'pending enroll response' flag since missing.\n", sensor->address().ext());
                item->setValue(item->toNumber() | R_PENDING_ENROLL_RESPONSE);
            }

            DBG_Printf(DBG_INFO_L2, "[IAS ZONE] - 0x%016llX Querying IAS zone state and CIE address (EP %d)...\n", sensor->address().ext(), sensor->fingerPrint().endpoint);
            std::vector<uint16_t> attributes;
            attributes.push_back(IAS_ZONE_STATE); // IAS zone state
            attributes.push_back(IAS_CIE_ADDRESS); // IAS CIE address
            if (readAttributes(sensor, sensor->fingerPrint().endpoint, IAS_ZONE_CLUSTER_ID, attributes))
            {
                DBG_Printf(DBG_INFO, "[IAS ZONE] - 0x%016llX Attributes querried.\n", sensor->address().ext());
                queryTime = queryTime.addSecs(1);
            }
            else
            {
                // Ensure failed attrubute reads are caught and tried again
                DBG_Printf(DBG_INFO, "[IAS ZONE] - 0x%016llX Attributes could NOT be querried.\n", sensor->address().ext());
                item->setValue(item->toNumber() & ~R_PENDING_WRITE_CIE_ADDRESS);
                item->setValue(item->toNumber() & ~R_PENDING_ENROLL_RESPONSE);
            }
            sensor->setNeedSaveDatabase(true);
        }
        else if (item && iasZoneStatus.u8 == 0 && ((item->toNumber() & R_PENDING_ENROLL_RESPONSE) || (item->toNumber() & R_PENDING_WRITE_CIE_ADDRESS)))
        {
            DBG_Printf(DBG_INFO, "[IAS ZONE] - 0x%016llX Sensor enrollment pending...\n", sensor->address().ext());
        }
    }
}

/*! Write IAS CIE address attribute for a node.
    \param sensor - Sensor containing the IAS zone cluster
 */
void DeRestPluginPrivate::writeIasCieAddress(Sensor *sensor)
{
    ResourceItem *item = nullptr;
    item = sensor->item(RConfigPending);

    if (sensor->fingerPrint().hasInCluster(IAS_ZONE_CLUSTER_ID) && item && (item->toNumber() & R_PENDING_WRITE_CIE_ADDRESS))
    {
        // write CIE address needed for some IAS Zone devices
        const quint64 iasCieAddress = apsCtrl->getParameter(deCONZ::ParamMacAddress);
        deCONZ::ZclAttribute attribute(IAS_CIE_ADDRESS, deCONZ::ZclIeeeAddress, QLatin1String("CIE address"), deCONZ::ZclReadWrite, false);
        attribute.setValue(iasCieAddress);

        DBG_Printf(DBG_INFO_L2, "[IAS ZONE] - 0x%016llX Write IAS CIE address.\n", sensor->address().ext());

        if (!writeAttribute(sensor, sensor->fingerPrint().endpoint, IAS_ZONE_CLUSTER_ID, attribute, 0))
        {
            // By removing all pending flags, a read of relevant attributes is triggered again, also resulting in a new write attempt
            DBG_Printf(DBG_INFO, "[IAS ZONE] - 0x%016llX Writing IAS CIE address failed.\n", sensor->address().ext());
            item->setValue(item->toNumber() & ~R_PENDING_WRITE_CIE_ADDRESS);
            item->setValue(item->toNumber() & ~R_PENDING_ENROLL_RESPONSE);
        }
    }
}
