/*
 * Copyright (c) 2017 dresden elektronik ingenieurtechnik gmbh.
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
        if (findSensorsState == FindSensorsActive)
        {
            if (!fastProbeTimer->isActive())
            {
                fastProbeTimer->start(5);
            }
        }
    }

    if (!zclFrame.isClusterCommand())
    {
        return;
    }

    if (zclFrame.commandId() == CMD_STATUS_CHANGE_NOTIFICATION)
    {
        quint16 zoneStatus;
        quint8 extendedStatus;
        quint8 zoneId;
        quint16 delay;

        stream >> zoneStatus;
        stream >> extendedStatus; // reserved, set to 0
        stream >> zoneId;
        stream >> delay;

        DBG_Printf(DBG_ZCL, "IAS Zone Status Change, status: 0x%04X, zoneId: %u, delay: %u\n", zoneStatus, zoneId, delay);

        Sensor *sensor = 0;

        for (Sensor &s : sensors)
        {
            if (s.deletedState() != Sensor::StateNormal)
            {
                continue;
            }

            if (s.type() != QLatin1String("ZHAAlarm") &&
                s.type() != QLatin1String("ZHACarbonMonoxide") &&
                s.type() != QLatin1String("ZHAFire") &&
                s.type() != QLatin1String("ZHAOpenClose") &&
                s.type() != QLatin1String("ZHAPresence") &&
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

        const char *attr = 0;
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
        else if (sensor->type() == QLatin1String("ZHAWater"))
        {
            attr = RStateWater;
        }

        ResourceItem *item = 0;
        if (attr)
        {
            item = sensor->item(attr);
        }

        if (item)
        {
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

            deCONZ::NumericUnion num = {0};
            num.u16 = zoneStatus;
            sensor->setZclValue(NodeValue::UpdateByZclReport, IAS_ZONE_CLUSTER_ID, 0x0000, num);

            item2 = sensor->item(RConfigReachable);
            if (item2 && !item2->toBool())
            {
                item2->setValue(true);
                enqueueEvent(Event(RSensors, RConfigReachable, sensor->id(), item2));
            }

            if (alarm && item->descriptor().suffix == RStatePresence)
            {   // prepare to automatically set presence to false
                item2 = sensor->item(RConfigDuration);
                if (item2 && item2->toNumber() > 0)
                {
                    sensor->durationDue = item->lastSet().addSecs(item2->toNumber());
                }
            }
        }

    }
    else if (zclFrame.commandId() == CMD_ZONE_ENROLL_REQUEST)
    {
        quint16 zoneType;
        quint16 manufacturer;

        stream >> zoneType;
        stream >> manufacturer;

        DBG_Printf(DBG_ZCL, "IAS Zone Enroll Request, zone type: 0x%04X, manufacturer: 0x%04X\n", zoneType, manufacturer);

        sendIasZoneEnrollResponse(ind, zclFrame);
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
        DBG_Printf(DBG_INFO, "IAS Zone failed to send enroll reponse\n");
    }
}
