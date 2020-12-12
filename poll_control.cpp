/*
 * Copyright (c) 2020 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "de_web_plugin_private.h"
#include "poll_control.h"

static void handleCheckinCommand(DeRestPluginPrivate *plugin, const deCONZ::ApsDataIndication &ind)
{
    std::vector<Resource*> resources;

    for (auto &s : plugin->sensors)
    {
        if (s.address().ext() == ind.srcAddress().ext() && s.deletedState() == Sensor::StateNormal)
        {
            resources.push_back(&s);
            s.setNeedSaveDatabase(true);
        }
    }

    if (!resources.empty())
    {
        plugin->queSaveDb(DB_SENSORS, DB_LONG_SAVE_DELAY);
    }

    //        stick to sensors for now, perhaps we need to add lights later on
    //        for (auto &n : plugin->nodes)
    //        {
    //            if (n.address().ext() == ind.srcAddress().ext() && n.state() == LightNode::StateNormal)
    //            {
    //                resources.push_back(&n);
    //            }
    //        }

    const auto now = QDateTime::currentDateTimeUtc();

    for (auto *r : resources)
    {
        auto *item = r->item(RStateLastCheckin);
        if (!item)
        {
            item = r->addItem(DataTypeTime, RStateLastCheckin);
        }

        Q_ASSERT(item);
        if (item)
        {
            item->setIsPublic(false);
            item->setValue(now);
            plugin->enqueueEvent(Event(r->prefix(), item->descriptor().suffix, r->toString(RAttrId), item));
        }
    }

    DBG_Printf(DBG_INFO, "Poll control check-in from 0x%016llX\n", ind.srcAddress().ext());
}

void DeRestPluginPrivate::handlePollControlIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    if (zclFrame.isClusterCommand() && (zclFrame.frameControl() & deCONZ::ZclFCDirectionServerToClient) && zclFrame.commandId() == POLL_CONTROL_CMD_CHECKIN)
    {
        handleCheckinCommand(this, ind);
    }
}

/*! Checks open tasks for poll control cluster.
    \return true - when a binding or APS request got queued.
 */
bool DeRestPluginPrivate::checkPollControlClusterTask(Sensor *sensor)
{
    if (!sensor)
    {
        return false;
    }

    if (!sensor->node())
    {
        return false;
    }

    if (searchSensorsState == SearchSensorsActive)
    {
        // defer this until other items have been processed
        return false;
    }

    ResourceItem *item = sensor->item(RConfigPending);
    if (!item)
    {
        return false;
    }

    if ((item->toNumber() & (R_PENDING_WRITE_POLL_CHECKIN_INTERVAL | R_PENDING_SET_LONG_POLL_INTERVAL)) == 0)
    {
        return false; // nothing to do
    }

    if (sensor->node()->simpleDescriptors().empty())
    {
        return false; // only proceed when simple descriptors are queried
    }

    const quint8 pcEndpoint = PC_GetPollControlEndpoint(sensor->node());
    if (pcEndpoint == 0)
    {
        // not supported, remove
        item->setValue(item->toNumber() & ~(R_PENDING_WRITE_POLL_CHECKIN_INTERVAL | R_PENDING_SET_LONG_POLL_INTERVAL));
        return false;
    }

    if (item->toNumber() & R_PENDING_WRITE_POLL_CHECKIN_INTERVAL)
    {
        // write poll control checkin interval
        deCONZ::ZclAttribute attr(0x0000, deCONZ::Zcl32BitUint, QLatin1String("Check-in interval"), deCONZ::ZclReadWrite, false);
        // TODO this needs to be device dependend and configured via a RConfigCheckin
        attr.setValue(static_cast<quint64>(14400)); // 1 hour in 0.25 seconds

        DBG_Printf(DBG_INFO, "Write poll cluster check-in interval for 0x%016llx\n", sensor->address().ext());

        if (writeAttribute(sensor, pcEndpoint, POLL_CONTROL_CLUSTER_ID, attr, 0))
        {
            // mark done
            item->setValue(item->toNumber() & ~R_PENDING_WRITE_POLL_CHECKIN_INTERVAL);
            return true;
        }
    }

    if (item->toNumber() & R_PENDING_SET_LONG_POLL_INTERVAL)
    {
        deCONZ::ApsDataRequest apsReq;
        deCONZ::ZclFrame zclFrame;

         // ZDP Header
         apsReq.dstAddress() = sensor->address();
         apsReq.setDstAddressMode(deCONZ::ApsExtAddress);
         apsReq.setDstEndpoint(pcEndpoint);
         apsReq.setSrcEndpoint(endpoint());
         apsReq.setProfileId(HA_PROFILE_ID);
         apsReq.setRadius(0);
         apsReq.setClusterId(POLL_CONTROL_CLUSTER_ID);
         apsReq.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);

         deCONZ::ZclFrame outZclFrame;
         outZclFrame.setSequenceNumber(zclFrame.sequenceNumber());
         outZclFrame.setCommandId(0x02); // set long poll interval
         outZclFrame.setFrameControl(deCONZ::ZclFCClusterCommand | deCONZ::ZclFCDirectionClientToServer);

         { // ZCL payload
             QDataStream stream(&outZclFrame.payload(), QIODevice::WriteOnly);
             stream.setByteOrder(QDataStream::LittleEndian);
             // TODO this needs to be device dependend and configured via a RConfigLongPoll
             const quint32 longPollInterval = 4 * 60 * 15; // 15 minutes in quarter seconds
             stream << longPollInterval;
         }

         { // ZCL frame
             QDataStream stream(&apsReq.asdu(), QIODevice::WriteOnly);
             stream.setByteOrder(QDataStream::LittleEndian);
             outZclFrame.writeToStream(stream);
         }

         if (apsCtrl && apsCtrl->apsdeDataRequest(apsReq) == deCONZ::Success)
         {
             item->setValue(item->toNumber() & ~R_PENDING_SET_LONG_POLL_INTERVAL);
             return true;
         }
    }

    return false;
}

/*! Returns the endpoint of the Poll Control server cluster.
    \return 0 if not found.
    \return >0 endpoint if found.
 */
quint8 PC_GetPollControlEndpoint(const deCONZ::Node *node)
{
    if (!node)
    {
        return 0;
    }

    for (const auto &sd : node->simpleDescriptors())
    {
        for (const auto &cl : sd.inClusters())
        {
            if (cl.id() == POLL_CONTROL_CLUSTER_ID)
            {
                return sd.endpoint();
            }
        }
    }

    return 0;
}
