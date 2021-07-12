/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "utils/utils.h"
#include "zdp_handlers.h"

/*! Handle the case that a node (re)joins the network.
    \param ind a ZDP DeviceAnnce_req
 */
void DeRestPluginPrivate::handleDeviceAnnceIndication(const deCONZ::ApsDataIndication &ind)
{
    std::vector<LightNode>::iterator i = nodes.begin();
    std::vector<LightNode>::iterator end = nodes.end();

    quint16 nwk;
    quint64 ext;
    quint8 macCapabilities;

    {
        QDataStream stream(ind.asdu());
        stream.setByteOrder(QDataStream::LittleEndian);

        quint8 seq;

        stream >> seq;
        stream >> nwk;
        stream >> ext;
        stream >> macCapabilities;
    }

    for (; i != end; ++i)
    {
        if (i->state() != LightNode::StateNormal)
        {
            continue;
        }

        if (i->address().ext() == ext)
        {
            i->rx();
            i->setValue(RAttrLastAnnounced, i->lastRx().toUTC());

            // clear to speedup polling
            for (NodeValue &val : i->zclValues())
            {
                val.timestamp = QDateTime();
                val.timestampLastReport = QDateTime();
                val.timestampLastConfigured = QDateTime();
            }

            i->setLastAttributeReportBind(0);

            std::vector<RecoverOnOff>::iterator rc = recoverOnOff.begin();
            std::vector<RecoverOnOff>::iterator rcend = recoverOnOff.end();
            for (; rc != rcend; ++rc)
            {
                if (rc->address.ext() == ext || rc->address.nwk() == nwk)
                {
                    rc->idleTotalCounterCopy -= 60; // speedup release
                    // light was off before, turn off again
                    if (!rc->onOff)
                    {
                        DBG_Printf(DBG_INFO, "Turn off light 0x%016llX again after powercycle\n", rc->address.ext());
                        TaskItem task;
                        task.lightNode = &*i;
                        task.req.dstAddress().setNwk(nwk);
                        task.req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
                        task.req.setDstEndpoint(task.lightNode->haEndpoint().endpoint());
                        task.req.setSrcEndpoint(getSrcEndpoint(task.lightNode, task.req));
                        task.req.setDstAddressMode(deCONZ::ApsNwkAddress);
                        task.req.setSendDelay(1000);
                        queryTime = queryTime.addSecs(5);
                        addTaskSetOnOff(task, ONOFF_COMMAND_OFF, 0);
                    }
                    else if (rc->bri > 0 && rc->bri < 256)
                    {
                        DBG_Printf(DBG_INFO, "Turn on light 0x%016llX on again with former brightness after powercycle\n", rc->address.ext());
                        TaskItem task;
                        task.lightNode = &*i;
                        task.req.dstAddress().setNwk(nwk);
                        task.req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
                        task.req.setDstEndpoint(task.lightNode->haEndpoint().endpoint());
                        task.req.setSrcEndpoint(getSrcEndpoint(task.lightNode, task.req));
                        task.req.setDstAddressMode(deCONZ::ApsNwkAddress);
                        task.req.setSendDelay(1000);
                        queryTime = queryTime.addSecs(5);
                        addTaskSetBrightness(task, rc->bri, true);
                    }
                    break;
                }
            }

            deCONZ::Node *node = i->node();
            if (node && node->endpoints().end() == std::find(node->endpoints().begin(),
                                                     node->endpoints().end(),
                                                     i->haEndpoint().endpoint()))
            {
                continue; // not a active endpoint
            }

            ResourceItem *item = i->item(RStateReachable);

            if (item)
            {
                item->setValue(true); // refresh timestamp after device announce
                if (i->state() == LightNode::StateNormal)
                {
                    Event e(i->prefix(), RStateReachable, i->id(), item);
                    enqueueEvent(e);
                }

                updateEtag(gwConfigEtag);
            }

            DBG_Printf(DBG_INFO, "DeviceAnnce of LightNode: %s Permit Join: %i\n", qPrintable(i->address().toStringExt()), gwPermitJoinDuration);

            // force reading attributes
            i->enableRead(READ_GROUPS | READ_SCENES);

            // bring to front to force next polling
            const PollNodeItem pollItem(i->uniqueId(), i->prefix());
            pollNodes.push_front(pollItem);

            for (uint32_t ii = 0; ii < 32; ii++)
            {
                uint32_t item = 1 << ii;
                if (i->mustRead(item))
                {
                    i->setNextReadTime(item, queryTime);
                    i->setLastRead(item, idleTotalCounter);
                }
            }

            queryTime = queryTime.addSecs(1);
            updateEtag(i->etag);

        }
    }

    int found = 0;
    std::vector<Sensor>::iterator si = sensors.begin();
    std::vector<Sensor>::iterator send = sensors.end();

    for (; si != send; ++si)
    {
        if (si->deletedState() != Sensor::StateNormal)
        {
            continue;
        }

        if (si->address().ext() == ext)
        {
            si->rx();
            found++;
            DBG_Printf(DBG_INFO, "DeviceAnnce of SensorNode: 0x%016llX [1]\n", si->address().ext());

            ResourceItem *item = si->item(RConfigReachable);
            if (item)
            {
                item->setValue(true); // refresh timestamp after device announce
                Event e(si->prefix(), RConfigReachable, si->id(), item);
                enqueueEvent(e);
            }
            
            item = si->item(RConfigEnrolled); // holds per device IAS state variable

            if (item)
            {
                item->setValue(IAS_STATE_INIT);
            }
            
            checkSensorGroup(&*si);
            checkSensorBindingsForAttributeReporting(&*si);
            checkSensorBindingsForClientClusters(&*si);
            updateSensorEtag(&*si);

            if (searchSensorsState == SearchSensorsActive && si->node())
            {
                // address changed?
                if (si->address().nwk() != nwk)
                {
                    DBG_Printf(DBG_INFO, "\tnwk address changed 0x%04X -> 0x%04X [2]\n", si->address().nwk(), nwk);
                    // indicator that the device was resettet
                    si->address().setNwk(nwk);

                    if (searchSensorsState == SearchSensorsActive &&
                        si->deletedState() == Sensor::StateNormal)
                    {
                        updateSensorEtag(&*si);
                        Event e(RSensors, REventAdded, si->id());
                        enqueueEvent(e);
                    }
                }

                // clear to speedup polling
                for (NodeValue &val : si->zclValues())
                {
                    val.timestamp = QDateTime();
                    val.timestampLastReport = QDateTime();
                    val.timestampLastConfigured = QDateTime();
                }

                addSensorNode(si->node()); // check if somethings needs to be updated
            }

            if (si->type() == QLatin1String("ZHATime"))
            {
                if (!si->mustRead(READ_TIME))
                {
                    DBG_Printf(DBG_INFO, "  >>> %s sensor %s: set READ_TIME from handleDeviceAnnceIndication()\n", qPrintable(si->type()), qPrintable(si->name()));
                    si->enableRead(READ_TIME);
                    si->setLastRead(READ_TIME, idleTotalCounter);
                    si->setNextReadTime(READ_TIME, queryTime);
                    queryTime = queryTime.addSecs(1);
                }
            }
        }
    }

    if (searchSensorsState == SearchSensorsActive)
    {
        if (!found && apsCtrl)
        {
            int i = 0;
            const deCONZ::Node *node;

            // try to add sensor nodes even if they existed in deCONZ bevor and therefore
            // no node added event will be triggert in this phase
            while (apsCtrl->getNode(i, &node) == 0)
            {
                if (ext == node->address().ext())
                {
                    addSensorNode(node);
                    break;
                }
                i++;
            }
        }

        deCONZ::ZclFrame zclFrame; // dummy
        handleIndicationSearchSensors(ind, zclFrame);
    }
}

struct MapMfCode
{
    quint64 macPrefix;
    quint16 mfcode;
    /* Bits 9-15. These bits indicate the revision of the ZigBee Pro Core specification that the running stack is implemented to. Prior
       to revision 21 of the specification these bits were reserved and thus set to 0. A stack that is compliant to revision 22
       would set these bits to 22 (0010110b). A stack shall indicate the revision of the specification it is compliant to by
       setting these bits.

       0x0000  Reserved  prior Rev. 21
       0x2A00  (21 << 9) Rev. 21
       0x2C00  (22 << 9) Rev. 22
    */
    quint16 serverMask;
};

static const std::array<MapMfCode, 2> mapMfCode = {
    {
        { 0x04cf8c0000000000ULL, 0x115F, 0x0040}, // Xiaomi
        { 0x54ef440000000000ULL, 0x115F, 0x0040}  // Xiaomi
    }
};

/*! Sends a Node Descriptor response.

    Sends modified Manufacturer Code and Server Mask for some devices.

    \param ind - a ZDP NodeDescriptor_req
    \param apsCtrl - APS controller instance
 */
void ZDP_HandleNodeDescriptorRequest(const deCONZ::ApsDataIndication &ind, deCONZ::ApsController *apsCtrl)
{
    if (!apsCtrl)
    {
        return;
    }

    const deCONZ::Node *self = getCoreNode(apsCtrl->getParameter(deCONZ::ParamMacAddress), apsCtrl);

    if (!self)
    {
        return;
    }

    quint8 seq;
    quint16 nwkAddr;

    {
        QDataStream stream(ind.asdu());
        stream.setByteOrder(QDataStream::LittleEndian);

        stream >> seq;
        stream >> nwkAddr;

        if (stream.status() != QDataStream::Ok)
        {
            return;
        }
    }

    if (nwkAddr != self->address().nwk())
    {
        return;
    }

    quint16 mfCode = VENDOR_DDEL;
    quint16 serverMask = 0x0040; // compatible with stack revisions below version 21
    QByteArray ndRaw;

    if (!self->nodeDescriptor().isNull())
    {
        ndRaw = self->nodeDescriptor().toByteArray();
        serverMask = static_cast<quint16>(self->nodeDescriptor().serverMask()) & 0xFFFF;
    }
    else // fallback if not known
    {
        ndRaw = QByteArray("\x10\x40\x0f\x35\x11\x47\x2b\x00\x40\x00\x2b\x00\x00", 13);
    }

    auto i = std::find_if(mapMfCode.cbegin(), mapMfCode.cend(), [&ind](const auto &entry) {
        Q_ASSERT(entry.macPrefix != 0); // array size larger than given entries
        return (ind.srcAddress().ext() & entry.macPrefix) == entry.macPrefix;
    });

    if (i != mapMfCode.cend())
    {
        mfCode = i->mfcode;
        serverMask = i->serverMask;
    }

    {   // change manufacturer code and server mask if needed
        QDataStream stream(&ndRaw, QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        stream.device()->seek(3);
        stream << mfCode;
        stream.device()->seek(8);
        stream << serverMask;
    }

    deCONZ::ApsDataRequest req;

    req.setProfileId(ZDP_PROFILE_ID);
    req.setSrcEndpoint(ZDO_ENDPOINT);
    req.setDstEndpoint(ZDO_ENDPOINT);
    req.setClusterId(ZDP_NODE_DESCRIPTOR_RSP_CLID);
    req.setDstAddressMode(deCONZ::ApsNwkAddress);
    req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
    req.dstAddress() = ind.srcAddress();

    QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream << seq;
    stream << quint8(ZDP_SUCCESS);
    stream << nwkAddr;
    stream.writeRawData(ndRaw.constData(), ndRaw.size());

    if (apsCtrl->apsdeDataRequest(req) == deCONZ::Success) { }
}

/*! Handle node descriptor response.
    \param ind a ZDP NodeDescriptor_rsp
 */
void DeRestPluginPrivate::handleNodeDescriptorResponseIndication(const deCONZ::ApsDataIndication &ind)
{
    patchNodeDescriptor(ind);
}

/*! Handle mgmt lqi response.
    \param ind a ZDP MgmtLqi_rsp
 */
void DeRestPluginPrivate::handleMgmtLqiRspIndication(const deCONZ::ApsDataIndication &ind)
{
    quint8 zdpSeq;
    quint8 zdpStatus;
    quint8 neighEntries;
    quint8 startIndex;
    quint8 listCount;

    QDataStream stream(ind.asdu());
    stream.setByteOrder(QDataStream::LittleEndian);

    stream >> zdpSeq;
    stream >> zdpStatus;
    stream >> neighEntries;
    stream >> startIndex;
    stream >> listCount;

    if (stream.status() == QDataStream::ReadPastEnd)
    {
        return;
    }

    if ((startIndex + listCount) >= neighEntries || listCount == 0)
    {
        // finish
        for (LightNode &l : nodes)
        {
            if (l.address().ext() == ind.srcAddress().ext())
            {
                l.rx();
            }
        }
    }
}

/*! Handle IEEE address request indication.
    \param ind a ZDP IeeeAddress_req
 */
void DeRestPluginPrivate::handleIeeeAddressReqIndication(const deCONZ::ApsDataIndication &ind)
{
    if (!apsCtrl)
    {
        return;
    }

    quint8 seq;
    quint64 extAddr;
    quint16 nwkAddr;
    quint8 reqType;
    quint8 startIndex;

    {
        QDataStream stream(ind.asdu());
        stream.setByteOrder(QDataStream::LittleEndian);

        stream >> seq;
        stream >> nwkAddr;
        stream >> reqType;
        stream >> startIndex;
    }

    if (nwkAddr != apsCtrl->getParameter(deCONZ::ParamNwkAddress))
    {
        return;
    }

    deCONZ::ApsDataRequest req;

    req.setProfileId(ZDP_PROFILE_ID);
    req.setSrcEndpoint(ZDO_ENDPOINT);
    req.setDstEndpoint(ZDO_ENDPOINT);
    req.setClusterId(ZDP_IEEE_ADDR_RSP_CLID);
    req.setDstAddressMode(deCONZ::ApsNwkAddress);
    req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
    req.dstAddress() = ind.srcAddress();

    QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    extAddr = apsCtrl->getParameter(deCONZ::ParamMacAddress);

    quint8 status = ZDP_SUCCESS;
    stream << seq;
    stream << status;
    stream << extAddr;
    stream << nwkAddr;

    if (reqType == 0x01) // extended request type
    {
        stream << (quint8)0; // num of assoc devices
        stream << (quint8)0; // start index
    }

    if (apsCtrlWrapper.apsdeDataRequest(req) == deCONZ::Success)
    {

    }
}

/*! Handle NWK address request indication.
    \param ind a ZDP NwkAddress_req
 */
void DeRestPluginPrivate::handleNwkAddressReqIndication(const deCONZ::ApsDataIndication &ind)
{
    if (!apsCtrl)
    {
        return;
    }

    quint8 seq;
    quint16 nwkAddr;
    quint64 extAddr;
    quint8 reqType;
    quint8 startIndex;

    {
        QDataStream stream(ind.asdu());
        stream.setByteOrder(QDataStream::LittleEndian);

        stream >> seq;
        stream >> extAddr;
        stream >> reqType;
        stream >> startIndex;
    }

    if (extAddr != apsCtrl->getParameter(deCONZ::ParamMacAddress))
    {
        return;
    }

    deCONZ::ApsDataRequest req;

    req.setProfileId(ZDP_PROFILE_ID);
    req.setSrcEndpoint(ZDO_ENDPOINT);
    req.setDstEndpoint(ZDO_ENDPOINT);
    req.setClusterId(ZDP_NWK_ADDR_RSP_CLID);
    req.setDstAddressMode(deCONZ::ApsNwkAddress);
    req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
    req.dstAddress() = ind.srcAddress();

    QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    nwkAddr = apsCtrl->getParameter(deCONZ::ParamNwkAddress);
    quint8 status = ZDP_SUCCESS;
    stream << seq;
    stream << status;
    stream << extAddr;
    stream << nwkAddr;

    if (reqType == 0x01) // extended request type
    {
        stream << (quint8)0; // num of assoc devices
        stream << (quint8)0; // start index
    }

    if (apsCtrlWrapper.apsdeDataRequest(req) == deCONZ::Success)
    {

    }
}

/*! Patch Node Descriptor if fields are invalid.
    \param ind a ZDP NodeDescriptor_rsp
 */
void DeRestPluginPrivate::patchNodeDescriptor(const deCONZ::ApsDataIndication &ind)
{
    quint16 nwk = 0xffff;
    deCONZ::NodeDescriptor nd;

    {
        quint8 seq;
        quint8 status = ZDP_NO_DESCRIPTOR;
        QDataStream stream(ind.asdu());
        stream.setByteOrder(QDataStream::LittleEndian);

        stream >> seq;
        stream >> status;
        stream >> nwk;

        nd.readFromStream(stream);

        if (stream.status() != QDataStream::Ok)
        {
            return;
        }

        if (nwk == 0x0000)
        {
            return; // skip the coordinator
        }

        if (status != ZDP_SUCCESS || nd.isNull())
        {
            return;
        }
    }

    enum { UpdatedMacCapabilities = 0x01, UpdatedManufacturerCode = 0x02 };

    int i = 0;
    const deCONZ::Node *node;
    while (apsCtrl->getNode(i, &node) == 0)
    {
        i++;
        if (nwk != node->address().nwk() || !node->address().hasExt())
        {
            continue;
        }

        int updated = 0;

        // Not having 'allocate address' 0x80 is valid but currently expected for all devices
        if (!nd.macCapabilities().testFlag(deCONZ::MacAllocateAddress))
        {
            nd.setMacCapabilities(nd.macCapabilities() | deCONZ::MacAllocateAddress);
            updated |= UpdatedMacCapabilities;
        }

        // Fix incorrect manufacturer code for older Develco devices
        if ((node->address().ext() & develcoMacPrefix) == develcoMacPrefix && nd.manufacturerCode() == 0x0000)
        {
            nd.setManufacturerCode(VENDOR_DEVELCO);
            updated |= UpdatedManufacturerCode;
        }

        if (updated && (node->nodeDescriptor().macCapabilities() != nd.macCapabilities() ||
                        node->nodeDescriptor().manufacturerCode() != nd.manufacturerCode()))
        {
            if (updated & UpdatedMacCapabilities)
            {
                DBG_Printf(DBG_INFO, "[ND] 0x%016llX add 'allocate address' flag (0x80) to MAC capabilities\n", node->address().ext());
            }

            if (updated & UpdatedManufacturerCode)
            {
                DBG_Printf(DBG_INFO, "[ND] 0x%016llX update manufacturer code: 0x%04X\n", node->address().ext(), nd.manufacturerCode());
            }

            const_cast<deCONZ::Node*>(node)->setNodeDescriptor(nd);
            pushZdpDescriptorDb(node->address().ext(), ZDO_ENDPOINT, ZDP_NODE_DESCRIPTOR_CLID, node->nodeDescriptor().toByteArray());
        }

        break;
    }
}
