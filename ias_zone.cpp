/*
 * Copyright (c) 2017-2019 dresden elektronik ingenieurtechnik gmbh.
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

/*! Handle packets related to the ZCL IAS Zone cluster.
    \param ind the APS level data indication containing the ZCL packet
    \param zclFrame the actual ZCL frame which holds the IAS zone server command
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

    if (zclFrame.isProfileWideCommand() && zclFrame.commandId() == deCONZ::ZclReadAttributesResponseId)
    {
        // during setup the IAS Zone type will be read
        // start to proceed discovery here
        if (searchSensorsState == SearchSensorsActive)
        {
            if (!fastProbeTimer->isActive())
            {
                fastProbeTimer->start(5);
            }
        }
    }

    quint16 attrId = 0;
    quint16 zoneStatus = 0; // might be reported or received via CMD_STATUS_CHANGE_NOTIFICATION

    if (zclFrame.isProfileWideCommand() && zclFrame.commandId() == deCONZ::ZclReportAttributesId)
    {
        quint16 a;
        quint8 dataType;

        stream >> a;
        stream >> dataType;

        if (a == IAS_ZONE_CLUSTER_ATTR_ZONE_STATUS_ID && dataType == deCONZ::Zcl16BitBitMap)
        {
            attrId = a; // mark as reported
            stream >> zoneStatus;
        }

        if (stream.status() == QDataStream::ReadPastEnd)
        {
            return; // sanity
        }
    }

    if ((zclFrame.commandId() == CMD_STATUS_CHANGE_NOTIFICATION && zclFrame.isClusterCommand()) || attrId == IAS_ZONE_CLUSTER_ATTR_ZONE_STATUS_ID)
    {

        if (zclFrame.commandId() == CMD_STATUS_CHANGE_NOTIFICATION)
        {
            quint8 extendedStatus;
            quint8 zoneId;
            quint16 delay;
            stream >> zoneStatus;
            stream >> extendedStatus; // reserved, set to 0
            stream >> zoneId;
            stream >> delay;
            DBG_Printf(DBG_ZCL, "IAS Zone Status Change, status: 0x%04X, zoneId: %u, delay: %u\n", zoneStatus, zoneId, delay);
        }

        Sensor *sensor = nullptr;

        for (Sensor &s : sensors)
        {
            if (s.deletedState() != Sensor::StateNormal)
            {
                continue;
            }

            if (!s.fingerPrint().hasInCluster(IAS_ZONE_CLUSTER_ID) && !s.fingerPrint().hasInCluster(IAS_WD_CLUSTER_ID))
            {
                continue;
            }

            if (s.type() != QLatin1String("ZHAAlarm") &&
                s.type() != QLatin1String("ZHACarbonMonoxide") &&
                s.type() != QLatin1String("ZHAFire") &&
                s.type() != QLatin1String("ZHAOpenClose") &&
                s.type() != QLatin1String("ZHAPresence") &&
                s.type() != QLatin1String("ZHAVibration") &&
                s.type() != QLatin1String("ZHAWater"))
            {
                continue;
            }

            if ((ind.srcAddress().hasExt() && s.address().ext() == ind.srcAddress().ext()) ||
                (ind.srcAddress().hasNwk() && s.address().nwk() == ind.srcAddress().nwk()))
            {
                sensor = &s;
                break;
            }
        }

        if (!sensor)
        {
            return;
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
            sensor->rx();
            sensor->incrementRxCounter();
            bool alarm = (zoneStatus & (STATUS_ALARM1 | STATUS_ALARM2)) ? true : false;
            item->setValue(alarm);
            sensor->updateStateTimestamp();
            sensor->setNeedSaveDatabase(true);
            updateSensorEtag(sensor);
            enqueueEvent(Event(RSensors, item->descriptor().suffix, sensor->id(), item));
            enqueueEvent(Event(RSensors, RStateLastUpdated, sensor->id()));

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
            sensor->setZclValue(NodeValue::UpdateByZclReport, ind.srcEndpoint(), IAS_ZONE_CLUSTER_ID, IAS_ZONE_CLUSTER_ATTR_ZONE_STATUS_ID, num);

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
    else if (zclFrame.commandId() == CMD_ZONE_ENROLL_REQUEST && zclFrame.isClusterCommand())
    {
        quint16 zoneType;
        quint16 manufacturer;

        stream >> zoneType;
        stream >> manufacturer;

        DBG_Printf(DBG_INFO_L2, "[IAS] Zone Enroll Request, zone type: 0x%04X, manufacturer: 0x%04X\n", zoneType, manufacturer);

        sendIasZoneEnrollResponse(ind, zclFrame);

        Sensor *sensor = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), ind.srcEndpoint());
        ResourceItem *item = sensor ? sensor->item(RConfigPending) : nullptr;

        if (sensor && item)
        {
            item->setValue(item->toNumber() & ~R_PENDING_ENROLL_RESPONSE);
        }
        return;
    }

    // Allow clearing the alarm bit for Develco devices
    if (!(zclFrame.frameControl() & deCONZ::ZclFCDisableDefaultResponse))
    {
        sendZclDefaultResponse(ind, zclFrame, deCONZ::ZclSuccessStatus);
    }
}

/*! Sends IAS Zone enroll response to IAS Zone server.
    \param ind the APS level data indication containing the ZCL packet
    \param zclFrame the actual ZCL frame which holds the IAS Zone enroll request
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
        DBG_Printf(DBG_INFO_L2, "[IAS] Zone failed to send enroll reponse\n");
    }
}

/*! Check if a sensor is already enrolled
    \param Sensor - sensor containing the IAS zone cluster
 */
void DeRestPluginPrivate::checkIasEnrollmentStatus(Sensor *sensor)
{
    if (sensor->fingerPrint().hasInCluster(IAS_ZONE_CLUSTER_ID))
    {
        NodeValue val = sensor->getZclValue(IAS_ZONE_CLUSTER_ID, 0x0000);
        deCONZ::NumericUnion iasZoneStatus = val.value;

        ResourceItem *item = nullptr;
        item = sensor->item(RConfigPending);

        if (item && item->toNumber() == 0 && iasZoneStatus.u8 == 0)
        {
            DBG_Printf(DBG_INFO_L2, "[IAS] Sensor NOT enrolled\n");
            item->setValue(item->toNumber() | R_PENDING_WRITE_CIE_ADDRESS | R_PENDING_ENROLL_RESPONSE);
            std::vector<uint16_t> attributes;
            attributes.push_back(0x0000); // IAS zone status
            attributes.push_back(0x0010); // IAS CIE address
            if (readAttributes(sensor, sensor->fingerPrint().endpoint, IAS_ZONE_CLUSTER_ID, attributes))
            {
                queryTime = queryTime.addSecs(1);
            }
        }
        else if (item &&
                (item->toNumber() & R_PENDING_WRITE_CIE_ADDRESS) &&
                (item->toNumber() & R_PENDING_ENROLL_RESPONSE) &&
                iasZoneStatus.u8 == 0)
        {
            DBG_Printf(DBG_INFO_L2, "[IAS] Sensor enrollment pending\n");
        }
        else if (iasZoneStatus.u8 == 1)
        {
            DBG_Printf(DBG_INFO_L2, "[IAS] Sensor enrolled\n");
        }
        else if (item && item->toNumber() == (R_PENDING_WRITE_CIE_ADDRESS | R_PENDING_ENROLL_RESPONSE) && iasZoneStatus.u8 == 1)        // Sanity check
        {
            DBG_Printf(DBG_INFO_L2, "[IAS] Sensor reporting enrolled. Clearing config pending...\n");
            item->setValue(item->toNumber() & ~R_PENDING_WRITE_CIE_ADDRESS);
            item->setValue(item->toNumber() & ~R_PENDING_ENROLL_RESPONSE);
        }
        else
        {
            DBG_Printf(DBG_INFO_L2, "[IAS] Enrolling...\n");
        }
    }
}

/*! Write IAS CIE address attribute for a node.
    \param Sensor - sensor containing the IAS zone cluster
 */
void DeRestPluginPrivate::writeIasCieAddress(Sensor *sensor)
{
    ResourceItem *item = nullptr;
    item = sensor->item(RConfigPending);

    if (sensor->fingerPrint().hasInCluster(IAS_ZONE_CLUSTER_ID) && item && (item->toNumber() & R_PENDING_WRITE_CIE_ADDRESS))
    {
        // write CIE address needed for some IAS Zone devices
        const quint64 iasCieAddress = apsCtrl->getParameter(deCONZ::ParamMacAddress);
        deCONZ::ZclAttribute attr(0x0010, deCONZ::ZclIeeeAddress, QLatin1String("CIE address"), deCONZ::ZclReadWrite, false);
        attr.setValue(iasCieAddress);

        DBG_Printf(DBG_INFO_L2, "[IAS] Write IAS CIE address for 0x%016llx\n", sensor->address().ext());

        if (writeAttribute(sensor, sensor->fingerPrint().endpoint, IAS_ZONE_CLUSTER_ID, attr, 0))
        {
            // mark done
            item->setValue(item->toNumber() & ~R_PENDING_WRITE_CIE_ADDRESS);
        }
    }
}
