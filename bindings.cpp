/*
 * Copyright (c) 2017-2023 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "device.h"
#include "device_descriptions.h"
#include "utils/utils.h"
#include "zdp/zdp.h"

#define MAX_ACTIVE_BINDING_TASKS 3

/*! Constructor. */
Binding::Binding() :
    srcAddress(0),
    srcEndpoint(0),
    clusterId(0),
    dstAddrMode(0),
    dstEndpoint(0)
{
    dstAddress.ext = 0;
}

/*! Returns true if bindings are equal. */
bool Binding::operator==(const Binding &rhs) const
{
    if (rhs.dstAddrMode == dstAddrMode &&
        rhs.srcAddress == srcAddress &&
        rhs.dstAddress.ext == dstAddress.ext &&
        rhs.clusterId == clusterId &&
        rhs.dstEndpoint == dstEndpoint &&
        rhs.srcEndpoint == srcEndpoint)
    {
        return true;
    }
    return false;
}

/*! Returns false if bindings are not equal. */
bool Binding::operator!=(const Binding &rhs) const
{
    return !(*this == rhs);
}

/*! Reads a binding entry from stream. */
bool Binding::readFromStream(QDataStream &stream)
{
    if (stream.atEnd()) return false;
    stream >> srcAddress;
    if (stream.atEnd()) return false;
    stream >> srcEndpoint;
    if (stream.atEnd()) return false;
    stream >> clusterId;
    if (stream.atEnd()) return false;
    stream >> dstAddrMode;

    if (dstAddrMode == GroupAddressMode)
    {
        if (stream.atEnd()) return false;
        stream >> dstAddress.group;
        dstEndpoint = 0; // not present
        return true;
    }
    else if (dstAddrMode == ExtendedAddressMode)
    {
        if (stream.atEnd()) return false;
        stream >> dstAddress.ext;
        if (stream.atEnd()) return false;
        stream >> dstEndpoint;
        return true;
    }

    return false;
}

/*! Writes a binding to stream.
    \param stream the data stream
 */
bool Binding::writeToStream(QDataStream &stream) const
{
    if (!srcAddress || !srcEndpoint)
    {
        return false;
    }

    stream << srcAddress;
    stream << srcEndpoint;
    stream << clusterId;
    stream << dstAddrMode;

    if (dstAddrMode == GroupAddressMode)
    {
        stream << dstAddress.group;
        return true;
    }
    else if ((dstAddrMode == ExtendedAddressMode) && (dstAddress.ext != 0) && (dstEndpoint != 0))
    {
        stream << dstAddress.ext;
        stream << dstEndpoint;
        return true;
    }

    return false;
}

/*! Converts a plugin Binding object to core deCONZ::Binding. */
deCONZ::Binding convertToCoreBinding(const Binding &bnd)
{
    if (bnd.dstAddrMode == deCONZ::ApsExtAddress)
    {
        return deCONZ::Binding(bnd.srcAddress, bnd.dstAddress.ext, bnd.clusterId, bnd.srcEndpoint, bnd.dstEndpoint);
    }
    else if (bnd.dstAddrMode == deCONZ::ApsGroupAddress)
    {
        return deCONZ::Binding(bnd.srcAddress, bnd.dstAddress.group, bnd.clusterId, bnd.srcEndpoint);
    }

    return { };
}

/*! Queue reading ZDP binding table.
    \param node the node from which the binding table shall be read
    \param startIndex the index to start the reading
    \return true if the request is queued
 */
bool DeRestPluginPrivate::readBindingTable(RestNodeBase *node, quint8 startIndex)
{
    DBG_Assert(node != 0);

    if (!node || !node->node())
    {
        return false;
    }

    Device *device = DEV_GetDevice(m_devices, node->address().ext());
    if (device && device->managed())
    {
        return false;
    }

    Resource *r = dynamic_cast<Resource*>(node);

    // whitelist
    if (node->mgmtBindSupported())
    {
    }
    else if (!node->mgmtBindSupported())
    {
        node->clearRead(READ_BINDING_TABLE);
        return false;
    }
    else if (existDevicesWithVendorCodeForMacPrefix(node->address(), VENDOR_DDEL))
    {
    }
    else if (existDevicesWithVendorCodeForMacPrefix(node->address(), VENDOR_UBISYS))
    {
    }
    else if (existDevicesWithVendorCodeForMacPrefix(node->address(), VENDOR_DEVELCO))
    {
    }
    else
    {
        node->clearRead(READ_BINDING_TABLE);
        return false;
    }

    std::vector<BindingTableReader>::iterator i = bindingTableReaders.begin();
    std::vector<BindingTableReader>::iterator end = bindingTableReaders.end();

    for (; i != end; ++i)
    {
        if (i->apsReq.dstAddress().ext() == node->address().ext())
        {
            // already running
            if (i->state == BindingTableReader::StateIdle)
            {
                i->index = startIndex;
                DBG_Assert(bindingTableReaderTimer->isActive());
            }
            return true;
        }
    }

    BindingTableReader btReader;
    btReader.state = BindingTableReader::StateIdle;
    btReader.index = startIndex;
    btReader.isEndDevice = !node->node()->nodeDescriptor().receiverOnWhenIdle();
    btReader.apsReq.dstAddress() = node->address();

    bindingTableReaders.push_back(btReader);

    if (!bindingTableReaderTimer->isActive())
    {
        bindingTableReaderTimer->start();
    }

    return true;
}

/*! Handle bind table confirm.
    \param conf a APSDE-DATA.confirm
    \return true if confirm was processed
 */
bool DeRestPluginPrivate::handleMgmtBindRspConfirm(const deCONZ::ApsDataConfirm &conf)
{
    if (conf.srcEndpoint() != ZDO_ENDPOINT || conf.dstEndpoint() != ZDO_ENDPOINT)
    {
        return false;
    }

    std::vector<BindingTableReader>::iterator i = bindingTableReaders.begin();
    std::vector<BindingTableReader>::iterator end = bindingTableReaders.end();

    for (; i != end; ++i)
    {
        if (i->apsReq.id() == conf.id())
        {
            if (i->state == BindingTableReader::StateWaitConfirm)
            {
                i->time.start();
                i->state = BindingTableReader::StateWaitResponse;
            }
            return true;
        }
    }
    return false;
}

/*! Handle bind table response.
    \param ind a ZDP MgmtBind_rsp
 */
void DeRestPluginPrivate::handleMgmtBindRspIndication(const deCONZ::ApsDataIndication &ind)
{
    if (ind.asdu().size() < 2)
    {
        // at least seq number and status
        return;
    }

    BindingTableReader *btReader = 0;

    {
        std::vector<BindingTableReader>::iterator i = bindingTableReaders.begin();
        std::vector<BindingTableReader>::iterator end = bindingTableReaders.end();

        for (; i != end; ++i)
        {
            if (isSameAddress(ind.srcAddress(), i->apsReq.dstAddress()))
            {
                btReader = &(*i);
                break;
            }
        }
    }

    RestNodeBase *node = getSensorNodeForAddress(ind.srcAddress());

    if (!node)
    {
        node = getLightNodeForAddress(ind.srcAddress());
    }

    if (!node)
    {
        if (btReader)
        {
            // no more needed
            btReader->state = BindingTableReader::StateFinished;
        }
        return;
    }

    QDataStream stream(ind.asdu());
    stream.setByteOrder(QDataStream::LittleEndian);

    quint8 seqNo;
    quint8 status;

    stream >> seqNo;
    stream >> status;

    if (status != deCONZ::ZdpSuccess)
    {
        if (status == deCONZ::ZdpNotPermitted ||
            status == deCONZ::ZdpNotSupported)
        {
            if (node->mgmtBindSupported())
            {
                node->setMgmtBindSupported(false);
            }
        }

        if (btReader)
        {
            // no more needed
            btReader->state = BindingTableReader::StateFinished;
        }
        return;
    }

    quint8 entries;
    quint8 startIndex;
    quint8 listCount;
    bool bend = false;

    stream >> entries;
    stream >> startIndex;
    stream >> listCount;

    if (entries > (startIndex + listCount))
    {
        if (btReader)
        {
            if (btReader->state == BindingTableReader::StateWaitResponse || btReader->state == BindingTableReader::StateWaitConfirm)
            {
                // read more
                btReader->state = BindingTableReader::StateIdle;
                btReader->index = startIndex + listCount;
            }
            else
            {
                DBG_Printf(DBG_ZDP, "unexpected BTR state %d\n", (int)btReader->state);
            }
        }
    }
    else
    {
        bend = true;
        if (btReader)
        {
            btReader->state = BindingTableReader::StateFinished;
        }

        enqueueEvent({RDevices, REventBindingTable, status, ind.srcAddress().ext()}); // TODO(mpi): I think this event is obsolete and should be removed
    }

    while (listCount && !stream.atEnd())
    {
        Binding bnd;

        if (bnd.readFromStream(stream))
        {
            if (bnd.dstAddrMode == deCONZ::ApsExtAddress)
            {
                DBG_Printf(DBG_ZDP, "found binding 0x%04X, 0x%02X -> 0x%016llX : 0x%02X\n", bnd.clusterId, bnd.srcEndpoint, bnd.dstAddress.ext, bnd.dstEndpoint);
            }
            else if (bnd.dstAddrMode == deCONZ::ApsGroupAddress)
            {
                DBG_Printf(DBG_ZDP, "found binding 0x%04X, 0x%02X -> 0x%04X\n", bnd.clusterId, bnd.srcEndpoint, bnd.dstAddress.group);
            }
            else
            {
                continue;
            }

            auto i = bindingQueue.begin();
            auto end = bindingQueue.end();

            for (;i != end; ++i)
            {
                if (i->binding == bnd)
                {
                    if (i->action == BindingTask::ActionBind && i->state != BindingTask::StateFinished)
                    {
                        DBG_Printf(DBG_ZDP, "binding 0x%04X, 0x%02X already exists, drop task\n", bnd.clusterId, bnd.dstEndpoint);
                        i->state = BindingTask::StateFinished; // already existing
                        sendConfigureReportingRequest(*i); // (re?)configure
                    }
                    else if (i->action == BindingTask::ActionUnbind && i->state == BindingTask::StateCheck)
                    {
                        DBG_Printf(DBG_ZDP, "binding 0x%04X, 0x%02X exists, start unbind task\n", bnd.clusterId, bnd.dstEndpoint);
                        i->state = BindingTask::StateIdle; // exists -> unbind
                    }
                    break;
                }
            }
        }
        else // invalid
        {
            DBG_Printf(DBG_ZDP, "invalid binding entry");
            break;
        }

        listCount--;
    }

    // end, check remaining tasks
    if (bend)
    {
        std::list<BindingTask>::iterator i = bindingQueue.begin();
        std::list<BindingTask>::iterator end = bindingQueue.end();

        for (;i != end; ++i)
        {
            if (i->state == BindingTask::StateCheck &&
                i->binding.srcAddress == ind.srcAddress().ext())
            {
                // if binding was not found, activate binding task
                if (i->action == BindingTask::ActionBind)
                {
                    DBG_Printf(DBG_ZDP, "binding 0x%04X, 0x%02X not found, start bind task\n", i->binding.clusterId, i->binding.dstEndpoint);
                    i->state = BindingTask::StateIdle;
                }
                else if (i->action == BindingTask::ActionUnbind)
                {
                    // nothing to unbind
                    DBG_Printf(DBG_ZDP, "binding 0x%04X, 0x%02X not found, remove unbind task\n", i->binding.clusterId, i->binding.dstEndpoint);
                    i->state = BindingTask::StateFinished; // already existing
                }
            }
        }
    }
}

/*! Handle incoming ZCL configure reporting response.
 */
void DeRestPluginPrivate::handleZclConfigureReportingResponseIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    Device *device = DEV_GetDevice(m_devices, ind.srcAddress().ext());
    if (device && device->managed())
    {
        return;
    }

    QDateTime now = QDateTime::currentDateTime();
    std::vector<RestNodeBase*> allNodes;
    for (Sensor &s : sensors)
    {
        allNodes.push_back(&s);
    }

    for (LightNode &l : nodes)
    {
        allNodes.push_back(&l);
    }

    for (RestNodeBase * restNode : allNodes)
    {
        if (restNode->address().ext() != ind.srcAddress().ext())
        {
            continue;
        }

        DBG_Assert(zclFrame.sequenceNumber() != 0);

        QDataStream stream(zclFrame.payload());
        stream.setByteOrder(QDataStream::LittleEndian);

        if (zclFrame.payload().size() == 1)
        {
            // Response contains a single status for all attributes
            quint8 status;
            stream >> status;

            for (NodeValue &val : restNode->zclValues())
            {
                if (val.zclSeqNum != zclFrame.sequenceNumber())
                {
                    continue;
                }

                if (val.clusterId != ind.clusterId())
                {
                    continue;
                }

                DBG_Printf(DBG_INFO, "ZCL configure reporting rsp seq: %u 0x%016llX for ep: 0x%02X cluster: 0x%04X attr: 0x%04X status: 0x%02X\n", zclFrame.sequenceNumber(), ind.srcAddress().ext(), ind.srcEndpoint(), ind.clusterId(), val.attributeId, status);

                // mark as succefully configured
                if (status == deCONZ::ZclSuccessStatus)
                {
                    val.timestampLastConfigured = now;
                    val.zclSeqNum = 0; // clear
                }
            }
            break;
        }

        while (!stream.atEnd())
        {
            // Response contains status per attribute
            quint8 status;
            quint8 direction;
            quint16 attrId;

            stream >> status;
            stream >> direction;
            stream >> attrId;

            NodeValue &val = restNode->getZclValue(ind.clusterId(), attrId, ind.srcEndpoint());
            if (val.zclSeqNum == zclFrame.sequenceNumber() && val.clusterId == ind.clusterId())
            {
                DBG_Printf(DBG_INFO, "ZCL configure reporting rsp seq: %u 0x%016llX for ep: 0x%02X cluster: 0x%04X attr: 0x%04X status: 0x%02X\n", zclFrame.sequenceNumber(), ind.srcAddress().ext(), ind.srcEndpoint(), ind.clusterId(), val.attributeId, status);

                if (status == deCONZ::ZclSuccessStatus)
                {
                    // mark as succefully configured
                    val.timestampLastConfigured = now;
                    val.zclSeqNum = 0; // clear
                }
            }
        }
    }

    if (searchSensorsState == SearchSensorsActive && fastProbeAddr.hasExt() && bindingQueue.empty())
    {
        for (auto &s : sensors)
        {
            if (s.address().ext() == fastProbeAddr.ext())
            {
                checkSensorBindingsForAttributeReporting(&s);
            }
        }
    }

    bindingTimer->start(0); // fast process of next request
}

/*! Handle bind/unbind response.
    \param ind a ZDP Bind/Unbind_rsp
 */
void DeRestPluginPrivate::handleBindAndUnbindRspIndication(const deCONZ::ApsDataIndication &ind)
{
    Device *device = DEV_GetDevice(m_devices, ind.srcAddress().ext());
    if (device && device->managed())
    {
        return;
    }

    QDataStream stream(ind.asdu());
    stream.setByteOrder(QDataStream::LittleEndian);

    quint8 zdpSeqNum;
    quint8 status;

    stream >> zdpSeqNum;
    stream >> status;

    std::list<BindingTask>::iterator i = bindingQueue.begin();
    std::list<BindingTask>::iterator end = bindingQueue.end();

    for (; i != end; ++i)
    {
        if (i->zdpSeqNum == zdpSeqNum)
        {
            const char *what = (ind.clusterId() == ZDP_BIND_RSP_CLID) ? "Bind" : "Unbind";

            if (status == deCONZ::ZdpSuccess)
            {
                DBG_Printf(DBG_INFO, "%s response success for 0x%016llx ep: 0x%02X cluster: 0x%04X\n", what, i->binding.srcAddress, i->binding.srcEndpoint, i->binding.clusterId);
                if (ind.clusterId() == ZDP_BIND_RSP_CLID)
                {
                    if (sendConfigureReportingRequest(*i))
                    {
                        return;
                    }
                }
            }
            else
            {
                DBG_Printf(DBG_INFO, "%s response failed with status 0x%02X for 0x%016llx ep: 0x%02X cluster: 0x%04X\n", what, status, i->binding.srcAddress, i->binding.srcEndpoint, i->binding.clusterId);
            }

            i->state = BindingTask::StateFinished;
            break;
        }
    }

    bindingTimer->start(0); // fast process of next binding requests
}

/*! Sends a ZDP bind request.
    \param bt a binding task
 */
bool DeRestPluginPrivate::sendBindRequest(BindingTask &bt)
{
    DBG_Assert(apsCtrl != nullptr);

    if (!apsCtrl)
    {
        return false;
    }

    for (auto &s : sensors)
    {
        if (s.address().ext() != bt.binding.srcAddress)
        {
            continue;
        }

        if (!s.node() || s.node()->nodeDescriptor().isNull())
        {
            // Whitelist sensors which don't seem to have a valid node descriptor.
            // This is a workaround currently only required for Develco smoke sensor
            // and potentially Bosch motion sensor
            if (s.modelId().startsWith(QLatin1String("EMIZB-1")) ||      // Develco EMI Norwegian HAN
                s.modelId().startsWith(QLatin1String("ISW-ZPR1-WP13")))  // Bosch motion sensor
            {
            }
            else
            {
                return false; // needs to be known
            }
        }

        if (s.node()->nodeDescriptor().receiverOnWhenIdle())
        {
            break; // ok
        }

        if (permitJoinFlag || searchSensorsState == SearchSensorsActive)
        {
            break; // ok
        }

        const QDateTime now = QDateTime::currentDateTime();
        if (s.lastRx().secsTo(now) > 7)
        {
            return false;
        }

        break; // ok
    }

    Binding &bnd = bt.binding;
    deCONZ::ApsDataRequest apsReq;

    // set destination addressing
    apsReq.setDstAddressMode(deCONZ::ApsExtAddress);
    apsReq.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
    apsReq.dstAddress().setExt(bnd.srcAddress);
    apsReq.setDstEndpoint(ZDO_ENDPOINT);
    apsReq.setSrcEndpoint(ZDO_ENDPOINT);
    apsReq.setProfileId(ZDP_PROFILE_ID);

    if (bt.action == BindingTask::ActionBind)
    {
        apsReq.setClusterId(ZDP_BIND_REQ_CLID);
    }
    else
    {
        apsReq.setClusterId(ZDP_UNBIND_REQ_CLID);
    }

    // prepare payload
    QDataStream stream(&apsReq.asdu(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    // generate and remember a new ZDP transaction sequence number
    bt.zdpSeqNum = ZDP_NextSequenceNumber();

    stream << bt.zdpSeqNum; // ZDP transaction sequence number

    if (!bnd.writeToStream(stream))
    {
        return false;
    }

    if (apsCtrlWrapper.apsdeDataRequest(apsReq) == deCONZ::Success)
    {
        return true;
    }

    return false;
}

/*! Sends a ZCL configure attribute reporting request.
    \param bt a former binding task
    \param requests list of configure reporting requests which will be combined in a message
 */
bool DeRestPluginPrivate::sendConfigureReportingRequest(BindingTask &bt, const std::vector<ConfigureReportingRequest> &requests)
{
    DBG_Assert(!requests.empty());
    if (requests.empty())
    {
        return false;
    }

    // clue code to get classic hard coded C++ bindings into DDF
    Device *device = DEV_GetDevice(m_devices, bt.binding.srcAddress);
    if (!device)
    {  }
    else if (!device->managed())
    {
        DDF_Binding ddfBinding;

        ddfBinding.isUnicastBinding = bt.binding.dstAddrMode == deCONZ::ApsExtAddress;
        ddfBinding.isGroupBinding = bt.binding.dstAddrMode == deCONZ::ApsGroupAddress;
        if (ddfBinding.isUnicastBinding)
        {
            ddfBinding.dstExtAddress = bt.binding.dstAddress.ext;
        }
        else if (ddfBinding.isGroupBinding)
        {
            ddfBinding.dstGroup = bt.binding.dstAddress.group;
        }
        ddfBinding.clusterId = bt.binding.clusterId;
        ddfBinding.dstEndpoint =  bt.binding.dstEndpoint;
        ddfBinding.srcEndpoint = bt.binding.srcEndpoint;

        for (const ConfigureReportingRequest &rep : requests)
        {
            DDF_ZclReport ddfRep;

            ddfRep.attributeId = rep.attributeId;
            ddfRep.dataType = rep.dataType;
            ddfRep.direction = rep.direction;
            ddfRep.manufacturerCode = rep.manufacturerCode;
            ddfRep.minInterval = rep.minInterval;
            ddfRep.maxInterval = rep.maxInterval;
            ddfRep.valid = true;

            if      (rep.reportableChange16bit != 0xFFFF)     { ddfRep.reportableChange = rep.reportableChange16bit; }
            else if (rep.reportableChange8bit != 0xFF)        { ddfRep.reportableChange = rep.reportableChange8bit; }
            else if (rep.reportableChange24bit != 0xFFFFFF)   { ddfRep.reportableChange = rep.reportableChange24bit; }
            else if (rep.reportableChange48bit != 0xFFFFFFFF) { ddfRep.reportableChange = rep.reportableChange48bit; }

            ddfBinding.reporting.push_back(ddfRep);
        }

        device->addBinding(ddfBinding);

        auto ddf = deviceDescriptions->get(device);
        if (ddf.status == QLatin1String("Draft"))
        {
            if (ddf.bindings != device->bindings())
            {
                ddf.bindings = device->bindings();
                deviceDescriptions->put(ddf);
            }
        }
    }
    else if (device->managed())
    {
        return false;
    }

    if (zclSeq == 0) // don't use zero, simplify matching
    {
        zclSeq = 1;
    }
    const quint8 zclSeqNum = zclSeq++; // to match in configure reporting response handler
    LightNode *lightNode = dynamic_cast<LightNode*>(bt.restNode);
    QDateTime now = QDateTime::currentDateTime();
    std::vector<ConfigureReportingRequest> out;

    for (const ConfigureReportingRequest &rq : requests)
    {
        NodeValue &val = bt.restNode->getZclValue(bt.binding.clusterId, rq.attributeId, bt.binding.srcEndpoint);
        if (val.clusterId == bt.binding.clusterId)
        {
            // value exists
            if (rq.maxInterval != 0xffff && // disable reporting
                val.timestampLastReport.isValid() &&
                val.timestampLastReport.secsTo(now) < qMin((rq.maxInterval * 3), 1800))
            {
                DBG_Printf(DBG_INFO, "skip configure report for cluster: 0x%04X attr: 0x%04X of node 0x%016llX (seems to be active)\n",
                           bt.binding.clusterId, rq.attributeId, bt.restNode->address().ext());
            }
            else
            {
                if (!val.timestampLastReport.isValid())
                {
                    // fake first report timestamp to mark succesful binding
                    // and prevent further bind requests before reports arrive
                    val.timestampLastReport = QDateTime::currentDateTime();
                }
                val.zclSeqNum = zclSeqNum;
                val.minInterval = rq.minInterval;
                val.maxInterval = rq.maxInterval;
                out.push_back(rq);
            }
        }
        else if (lightNode && rq.maxInterval != 0xffff /* disable reporting */)
        {
            // wait for value is created via polling
            DBG_Printf(DBG_INFO, "skip configure report for cluster: 0x%04X attr: 0x%04X of node 0x%016llX (wait reading or unsupported)\n",
                       bt.binding.clusterId, rq.attributeId, bt.restNode->address().ext());
        }
        else // sensors and disabled reporting
        {
            // values doesn't exist, create
            deCONZ::NumericUnion dummy;
            dummy.u64 = 0;
            bt.restNode->setZclValue(NodeValue::UpdateByZclReport, bt.binding.srcEndpoint, bt.binding.clusterId, rq.attributeId, dummy);
            val.zclSeqNum = zclSeqNum;
            val.minInterval = rq.minInterval;
            val.maxInterval = rq.maxInterval;
            out.push_back(rq);
        }
    }

    if (out.empty())
    {
        return false;
    }

    deCONZ::ApsDataRequest apsReq;

    // ZDP Header
    apsReq.dstAddress() = bt.restNode->address();
    apsReq.setDstAddressMode(deCONZ::ApsExtAddress);
    apsReq.setDstEndpoint(bt.binding.srcEndpoint);
    apsReq.setSrcEndpoint(endpoint());
    apsReq.setProfileId(HA_PROFILE_ID);
    apsReq.setRadius(0);
    apsReq.setClusterId(bt.binding.clusterId);
    apsReq.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);

    deCONZ::ZclFrame zclFrame;
    zclFrame.setSequenceNumber(zclSeqNum);
    zclFrame.setCommandId(deCONZ::ZclConfigureReportingId);

    if (requests.front().manufacturerCode)
    {
        zclFrame.setFrameControl(deCONZ::ZclFCProfileCommand |
                                 deCONZ::ZclFCManufacturerSpecific |
                                 deCONZ::ZclFCDirectionClientToServer |
                                 deCONZ::ZclFCDisableDefaultResponse);
        zclFrame.setManufacturerCode(requests.front().manufacturerCode);
    }
    else
    {
        zclFrame.setFrameControl(deCONZ::ZclFCProfileCommand |
                                 deCONZ::ZclFCDirectionClientToServer |
                                 deCONZ::ZclFCDisableDefaultResponse);
    }

    { // payload
        QDataStream stream(&zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        for (const ConfigureReportingRequest &rq : out)
        {
            stream << rq.direction;
            stream << rq.attributeId;
            stream << rq.dataType;
            stream << rq.minInterval;
            stream << rq.maxInterval;

            if (rq.reportableChange16bit != 0xFFFF)
            {
                stream << rq.reportableChange16bit;
            }
            else if (rq.reportableChange8bit != 0xFF)
            {
                stream << rq.reportableChange8bit;
            }
            else if (rq.reportableChange24bit != 0xFFFFFF)
            {
                stream << (qint8) (rq.reportableChange24bit & 0xFF);
                stream << (qint8) ((rq.reportableChange24bit >> 8) & 0xFF);
                stream << (qint8) ((rq.reportableChange24bit >> 16) & 0xFF);
            }
            else if (rq.reportableChange48bit != 0xFFFFFFFF)
            {
                stream << (qint8) (rq.reportableChange48bit & 0xFF);
                stream << (qint8) ((rq.reportableChange48bit >> 8) & 0xFF);
                stream << (qint8) ((rq.reportableChange48bit >> 16) & 0xFF);
                stream << (qint8) ((rq.reportableChange48bit >> 24) & 0xFF);
                stream << (qint8) 0x00;
                stream << (qint8) 0x00;
            }
            DBG_Printf(DBG_INFO_L2, "configure reporting rq seq %u for 0x%016llX, attribute 0x%04X/0x%04X\n", zclSeqNum, bt.restNode->address().ext(), bt.binding.clusterId, rq.attributeId);
        }
    }

    { // ZCL frame
        QDataStream stream(&apsReq.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        zclFrame.writeToStream(stream);
    }


    if (apsCtrlWrapper.apsdeDataRequest(apsReq) == deCONZ::Success)
    {
        queryTime = queryTime.addSecs(1);
        return true;
    }

    return false;
}

/*! Sends a ZCL configure attribute reporting request.
    \param bt a former binding task
 */
bool DeRestPluginPrivate::sendConfigureReportingRequest(BindingTask &bt)
{
    if (!bt.restNode || !bt.restNode->node())
    {
        return false;
    }

    deCONZ::SimpleDescriptor *sd = bt.restNode->node()->getSimpleDescriptor(bt.binding.srcEndpoint);
    if (!sd)
    {
        return false;
    }

    // check if bound cluster is server cluster
    deCONZ:: ZclCluster *cl = sd->cluster(bt.binding.clusterId, deCONZ::ServerCluster);
    if (!cl)
    {
        return false;
    }

    const QDateTime now = QDateTime::currentDateTime();
    ConfigureReportingRequest rq;

    const LightNode *lightNode = dynamic_cast<LightNode *>(bt.restNode);
    Sensor *sensor = dynamic_cast<Sensor *>(bt.restNode);

    const Resource *r = [lightNode, sensor]() -> const Resource* {
        if (lightNode) return lightNode;
        else           return sensor;
    }();

    if (!r || !r->item(RAttrModelId))
    {
        return false;
    }

    const QString modelId = r->item(RAttrModelId)->toString();
    const quint16 manufacturerCode = bt.restNode->node()->nodeDescriptor().manufacturerCode();

    if (bt.binding.clusterId == BOSCH_AIR_QUALITY_CLUSTER_ID && manufacturerCode == VENDOR_BOSCH2)
    {
        return false; // nothing todo
    }

    if (bt.binding.clusterId == OCCUPANCY_SENSING_CLUSTER_ID)
    {
        // add values if not already present
        deCONZ::NumericUnion dummy;
        dummy.u64 = 0;
        if (bt.restNode->getZclValue(bt.binding.clusterId, 0x0000, bt.binding.srcEndpoint).clusterId != bt.binding.clusterId)
        {
            bt.restNode->setZclValue(NodeValue::UpdateInvalid, bt.binding.srcEndpoint, bt.binding.clusterId, 0x0000, dummy);
        }

        rq.dataType = deCONZ::Zcl8BitBitMap;
        rq.attributeId = 0x0000; // occupancy
        rq.minInterval = 1;     // value used by Hue bridge
        rq.maxInterval = 300;   // value used by Hue bridge

        int processed = 0;
        if (sendConfigureReportingRequest(bt, {rq}))
        {
            processed++;
        }

        return processed > 0;
    }
    else if (bt.binding.clusterId == IAS_ZONE_CLUSTER_ID)
    {
        // zone status reporting only supported by some devices
        if (manufacturerCode != VENDOR_CENTRALITE &&
            manufacturerCode != VENDOR_C2DF &&
            manufacturerCode != VENDOR_SAMJIN)
        {
            return false;
        }

        // add values if not already present
        deCONZ::NumericUnion dummy;
        dummy.u64 = 0;
        if (bt.restNode->getZclValue(bt.binding.clusterId, IAS_ZONE_CLUSTER_ATTR_ZONE_STATUS_ID, bt.binding.srcEndpoint).clusterId != bt.binding.clusterId)
        {
            bt.restNode->setZclValue(NodeValue::UpdateInvalid, bt.binding.srcEndpoint, bt.binding.clusterId, IAS_ZONE_CLUSTER_ATTR_ZONE_STATUS_ID, dummy);
        }

        if (sensor && sensor->type() == QLatin1String("ZHAOpenClose") && modelId.startsWith(QLatin1String("multi")))
        {
            // Only configure periodic reports, as events are already sent though zone status change notification commands
            rq.minInterval = 300;
            rq.maxInterval = 3600;
        }
        else if (sensor && sensor->type() == QLatin1String("ZHASwitch") && modelId == QLatin1String("button"))
        {
            rq.minInterval = 65535; // Disable reporting so devices must not be reset to not have it
            rq.maxInterval = 65535; // configured at all. Should be changed in future to explicitly exclude device from reporting.
        }
        else
        {
            rq.minInterval = 300;
            rq.maxInterval = 3600;

            const ResourceItem *item = sensor ? sensor->item(RConfigDuration) : nullptr;

            if (item && item->toNumber() > 15 && item->toNumber() <= UINT16_MAX)
            {
                rq.maxInterval = static_cast<quint16>(item->toNumber());
                rq.maxInterval -= 5; // report before going presence: false
            }
        }

        rq.dataType = deCONZ::Zcl16BitBitMap;
        rq.attributeId = IAS_ZONE_CLUSTER_ATTR_ZONE_STATUS_ID;
        rq.reportableChange16bit = 0xffff;
        return sendConfigureReportingRequest(bt, {rq});
    }
    else if (bt.binding.clusterId == ILLUMINANCE_MEASUREMENT_CLUSTER_ID)
    {
        rq.dataType = deCONZ::Zcl16BitUint;
        rq.attributeId = 0x0000;         // measured value

        if (modelId.startsWith(QLatin1String("MOSZB-1")) ||           // Develco motion sensor
            modelId.startsWith(QLatin1String("MotionSensor51AU")))    // Aurora (Develco) motion sensor
        {
            rq.minInterval = 0;
            rq.maxInterval = 600;
            rq.reportableChange16bit = 0xFFFF;
        }
        else
        {
            rq.minInterval = 5;              // value used by Hue bridge
            rq.maxInterval = 300;            // value used by Hue bridge
            rq.reportableChange16bit = 2000; // value used by Hue bridge
        }
        return sendConfigureReportingRequest(bt, {rq});
    }
    else if (bt.binding.clusterId == TEMPERATURE_MEASUREMENT_CLUSTER_ID)
    {
        rq.dataType = deCONZ::Zcl16BitInt;
        rq.attributeId = 0x0000;       // measured value

        if (modelId.startsWith(QLatin1String("MOSZB-1")) ||         // Develco motion sensor
            modelId.startsWith(QLatin1String("FLSZB-1")) ||         // Develco water leak sensor
            modelId.startsWith(QLatin1String("ZHMS101")) ||         // Wattle (Develco) magnetic sensor
            modelId.startsWith(QLatin1String("MotionSensor51AU")))  // Aurora (Develco) motion sensor
        {
            rq.minInterval = 60;           // according to technical manual
            rq.maxInterval = 600;          // according to technical manual
            rq.reportableChange16bit = 10; // according to technical manual
        }
        else
        {
            rq.minInterval = 10;           // value used by Hue bridge
            rq.maxInterval = 300;          // value used by Hue bridge
            rq.reportableChange16bit = 20; // value used by Hue bridge
        }

        return sendConfigureReportingRequest(bt, {rq});
    }
    else if (bt.binding.clusterId == THERMOSTAT_CLUSTER_ID)
    {
        if (modelId.startsWith(QLatin1String("SPZB"))) // Eurotronic Spirit
        {
            rq.dataType = deCONZ::Zcl16BitInt;
            rq.attributeId = 0x0000;        // Local Temperature
            rq.minInterval = 1;             // report changes every second
            rq.maxInterval = 600;           // recommended value
            rq.reportableChange16bit = 20;  // value from TEMPERATURE_MEASUREMENT_CLUSTER_ID

            ConfigureReportingRequest rq2;
            rq2.dataType = deCONZ::Zcl8BitUint;
            rq2.attributeId = 0x0008;        // Pi Heating Demand (valve position %)
            rq2.minInterval = 1;             // report changes every second
            rq2.maxInterval = 600;           // recommended value
            rq2.reportableChange8bit = 1;    // recommended value

            ConfigureReportingRequest rq3;
            rq3.dataType = deCONZ::Zcl16BitInt;
            rq3.attributeId = 0x0012;        // Occupied Heating Setpoint - unused
            rq3.minInterval = 65535;         // disable
            rq3.maxInterval = 65535;         // disable
            rq3.reportableChange16bit = 0;   // disable

            ConfigureReportingRequest rq4;
            rq4.dataType = deCONZ::Zcl16BitInt;
            rq4.attributeId = 0x0014;        // Unoccupied Heating Setpoint - unused
            rq4.minInterval = 65535;         // disable
            rq4.maxInterval = 65535;         // disable
            rq4.reportableChange16bit = 0;   // disable

            ConfigureReportingRequest rq5;
            rq5.dataType = deCONZ::Zcl16BitInt;
            rq5.attributeId = 0x4003;        // Current Temperature Set point
            rq5.minInterval = 1;             // report changes every second
            rq5.maxInterval = 600;           // recommended value
            rq5.reportableChange16bit = 50;  // recommended value
            rq5.manufacturerCode = VENDOR_JENNIC;

            ConfigureReportingRequest rq6;
            rq6.dataType = deCONZ::Zcl24BitUint;
            rq6.attributeId = 0x4008;        // Host Flags
            rq6.minInterval = 1;             // report changes every second
            rq6.maxInterval = 600;           // recommended value
            rq6.reportableChange24bit = 1;   // recommended value
            rq6.manufacturerCode = VENDOR_JENNIC;

            return sendConfigureReportingRequest(bt, {rq, rq2, rq3, rq4}) || // Use OR because of manuf. specific attributes
                   sendConfigureReportingRequest(bt, {rq5, rq6});
        }
        else if (modelId == QLatin1String("Thermostat")) // eCozy
        {
            rq.dataType = deCONZ::Zcl16BitInt;
            rq.attributeId = 0x0000;        // Local Temperature
            rq.minInterval = 1;             // report changes every second
            rq.maxInterval = 600;           // recommended value
            rq.reportableChange16bit = 20;  // value from TEMPERATURE_MEASUREMENT_CLUSTER_ID

            ConfigureReportingRequest rq2;
            rq2.dataType = deCONZ::Zcl8BitUint;
            rq2.attributeId = 0x0008;        // Pi Heating Demand (valve position %)
            rq2.minInterval = 1;             // report changes every second
            rq2.maxInterval = 600;           // recommended value
            rq2.reportableChange8bit = 1;    // recommended value

            return sendConfigureReportingRequest(bt, {rq, rq2});
        }
        else if (modelId == QLatin1String("Super TR")) // Elko Super TR
        {
            rq.dataType = deCONZ::Zcl16BitInt;
            rq.attributeId = 0x0000;        // Local temperature
            rq.minInterval = 1;
            rq.maxInterval = 600;
            rq.reportableChange16bit = 20;

            ConfigureReportingRequest rq2;
            rq2.dataType = deCONZ::Zcl16BitInt;
            rq2.attributeId = 0x0012;        // Occupied heating setpoint
            rq2.minInterval = 1;
            rq2.maxInterval = 600;
            rq2.reportableChange16bit = 50;

            ConfigureReportingRequest rq4;
            rq4.dataType = deCONZ::ZclBoolean;
            rq4.attributeId = 0x0406;        // Device on
            rq4.minInterval = 1;
            rq4.maxInterval = 600;

            ConfigureReportingRequest rq5;
            rq5.dataType = deCONZ::Zcl16BitInt;
            rq5.attributeId = 0x0409;        // Floor temperature
            rq5.minInterval = 1;
            rq5.maxInterval = 600;
            rq5.reportableChange16bit = 20;

            ConfigureReportingRequest rq6;
            rq6.dataType = deCONZ::ZclBoolean;
            rq6.attributeId = 0x0413;        // Child lock
            rq6.minInterval = 1;
            rq6.maxInterval = 600;

            ConfigureReportingRequest rq7;
            rq7.dataType = deCONZ::ZclBoolean;
            rq7.attributeId = 0x0415;        // Heating active/inactive
            rq7.minInterval = 1;
            rq7.maxInterval = 600;

            return sendConfigureReportingRequest(bt, {rq, rq2, rq4, rq5, rq6, rq7});
        }
        else if (modelId == QLatin1String("SORB")) // Stelpro Orleans Fan
        {
            rq.dataType = deCONZ::Zcl16BitInt;
            rq.attributeId = 0x0000;         // Local Temperature
            rq.minInterval = 1;
            rq.maxInterval = 600;
            rq.reportableChange16bit = 20;

            ConfigureReportingRequest rq2;
            rq2.dataType = deCONZ::Zcl16BitInt;
            rq2.attributeId = 0x0011;        // Occupied cooling setpoint
            rq2.minInterval = 1;
            rq2.maxInterval = 600;
            rq2.reportableChange16bit = 50;

            ConfigureReportingRequest rq3;
            rq3.dataType = deCONZ::Zcl16BitInt;
            rq3.attributeId = 0x0012;        // Occupied heating setpoint
            rq3.minInterval = 1;
            rq3.maxInterval = 600;
            rq3.reportableChange16bit = 50;

            ConfigureReportingRequest rq4;
            rq4.dataType = deCONZ::Zcl8BitEnum;
            rq4.attributeId = 0x001C;        // Thermostat mode
            rq4.minInterval = 1;
            rq4.maxInterval = 600;
            rq4.reportableChange8bit = 0xff;

            return sendConfigureReportingRequest(bt, {rq, rq2, rq3, rq4});
        }
        else if (modelId.startsWith(QLatin1String("STZB402"))) // Stelpro baseboard thermostat
        {
            rq.dataType = deCONZ::Zcl16BitInt;
            rq.attributeId = 0x0000;         // Local Temperature
            rq.minInterval = 1;
            rq.maxInterval = 600;
            rq.reportableChange16bit = 20;

            ConfigureReportingRequest rq2;
            rq2.dataType = deCONZ::Zcl16BitInt;
            rq2.attributeId = 0x0012;        // Occupied heating setpoint
            rq2.minInterval = 1;
            rq2.maxInterval = 600;
            rq2.reportableChange16bit = 50;

            ConfigureReportingRequest rq3;
            rq3.dataType = deCONZ::Zcl8BitEnum;
            rq3.attributeId = 0x001C;        // Thermostat mode
            rq3.minInterval = 1;
            rq3.maxInterval = 600;
            rq3.reportableChange8bit = 0xff;

            return sendConfigureReportingRequest(bt, {rq, rq2, rq3});
        }
        else if (modelId == QLatin1String("Zen-01")) // Zen
        {
            rq.dataType = deCONZ::Zcl16BitInt;
            rq.attributeId = 0x0000;        // Local Temperature
            rq.minInterval = 1;             // report changes every second
            rq.maxInterval = 600;           // recommended value
            rq.reportableChange16bit = 20;  // value from TEMPERATURE_MEASUREMENT_CLUSTER_ID

            ConfigureReportingRequest rq2;
            rq2.dataType = deCONZ::Zcl16BitInt;
            rq2.attributeId = 0x0011;        // Occupied cooling setpoint
            rq2.minInterval = 1;             // report changes every second
            rq2.maxInterval = 600;
            rq2.reportableChange16bit = 50;

            ConfigureReportingRequest rq3;
            rq3.dataType = deCONZ::Zcl16BitInt;
            rq3.attributeId = 0x0012;        // Occupied heating setpoint
            rq3.minInterval = 1;
            rq3.maxInterval = 600;
            rq3.reportableChange16bit = 50;

            ConfigureReportingRequest rq4;
            rq4.dataType = deCONZ::Zcl16BitBitMap;
            rq4.attributeId = 0x0029;        // Thermostat running state
            rq4.minInterval = 1;
            rq4.maxInterval = 600;
            rq4.reportableChange16bit = 0xffff;

            ConfigureReportingRequest rq5;
            rq5.dataType = deCONZ::Zcl8BitEnum;
            rq5.attributeId = 0x001C;        // Thermostat mode
            rq5.minInterval = 1;
            rq5.maxInterval = 600;
            rq5.reportableChange8bit = 0xff;

            return sendConfigureReportingRequest(bt, {rq, rq2, rq3, rq4, rq5});
        }
        else if (modelId.startsWith(QLatin1String("SLR2")) || // Hive
                 modelId == QLatin1String("SLR1b"))           // Hive
        {
            rq.dataType = deCONZ::Zcl16BitInt;
            rq.attributeId = 0x0000;       // local temperature
            rq.minInterval = 0;
            rq.maxInterval = 300;
            rq.reportableChange16bit = 10;

            ConfigureReportingRequest rq3;
            rq3.dataType = deCONZ::Zcl16BitInt;
            rq3.attributeId = 0x0012;        // Occupied heating setpoint
            rq3.minInterval = 1;
            rq3.maxInterval = 600;
            rq3.reportableChange16bit = 50;

            ConfigureReportingRequest rq4;
            rq4.dataType = deCONZ::Zcl8BitEnum;
            rq4.attributeId = 0x001C;        // Thermostat mode
            rq4.minInterval = 1;
            rq4.maxInterval = 600;
            rq4.reportableChange8bit = 0xff;

            ConfigureReportingRequest rq2;
            rq2.dataType = deCONZ::Zcl16BitBitMap;
            rq2.attributeId = 0x0029;        // Thermostat running state
            rq2.minInterval = 1;
            rq2.maxInterval = 600;
            rq2.reportableChange16bit = 0xffff;

            return sendConfigureReportingRequest(bt, {rq, rq2, rq3, rq4});
        }
        else if (modelId.startsWith(QLatin1String("3157100"))) // Centralite Pearl
        {
            rq.dataType = deCONZ::Zcl16BitInt;
            rq.attributeId = 0x0000;        // Local Temperature
            rq.minInterval = 1;
            rq.maxInterval = 600;
            rq.reportableChange16bit = 20;

            ConfigureReportingRequest rq2;
            rq2.dataType = deCONZ::Zcl16BitInt;
            rq2.attributeId = 0x0011;        // Occupied cooling setpoint
            rq2.minInterval = 1;
            rq2.maxInterval = 600;
            rq2.reportableChange16bit = 50;

            ConfigureReportingRequest rq3;
            rq3.dataType = deCONZ::Zcl16BitInt;
            rq3.attributeId = 0x0012;        // Occupied heating setpoint
            rq3.minInterval = 1;
            rq3.maxInterval = 600;
            rq3.reportableChange16bit = 50;

            ConfigureReportingRequest rq4;
            rq4.dataType = deCONZ::Zcl16BitBitMap;
            rq4.attributeId = 0x0029;        // Thermostat running state
            rq4.minInterval = 1;
            rq4.maxInterval = 600;
            rq4.reportableChange16bit = 0xffff;

            ConfigureReportingRequest rq5;
            rq5.dataType = deCONZ::Zcl8BitEnum;
            rq5.attributeId = 0x001C;        // Thermostat mode
            rq5.minInterval = 1;
            rq5.maxInterval = 600;
            rq5.reportableChange8bit = 0xff;

            return sendConfigureReportingRequest(bt, {rq, rq2, rq3, rq4, rq5});
        }
        else if (modelId == QLatin1String("PR412C")) // OWON PCT502 Thermostat
        {
            rq.dataType = deCONZ::Zcl16BitInt;
            rq.attributeId = 0x0000;         // Local Temperature
            rq.minInterval = 1;
            rq.maxInterval = 600;
            rq.reportableChange16bit = 50;

            return sendConfigureReportingRequest(bt, {rq});
        }
        else if (sensor && (modelId == QLatin1String("0x8020") || // Danfoss RT24V Display thermostat
                            modelId == QLatin1String("0x8021") || // Danfoss RT24V Display thermostat with floor sensor
                            modelId == QLatin1String("0x8030") || // Danfoss RTbattery Display thermostat
                            modelId == QLatin1String("0x8031") || // Danfoss RTbattery Display thermostat with infrared
                            modelId == QLatin1String("0x8034") || // Danfoss RTbattery Dial thermostat
                            modelId == QLatin1String("0x8035")))  // Danfoss RTbattery Dial thermostat with infrared
        {
            rq.dataType = deCONZ::Zcl16BitInt;
            rq.attributeId = 0x0000;         // Local temperature
            rq.minInterval = 60;
            rq.maxInterval = 3600;
            rq.reportableChange16bit = 50;

            ConfigureReportingRequest rq2;
            rq2.dataType = deCONZ::Zcl8BitBitMap;
            rq2.attributeId = 0x0002;        // Occupancy
            rq2.minInterval = 60;
            rq2.maxInterval = 43200;
            rq2.reportableChange8bit = 1;

            ConfigureReportingRequest rq3;
            rq3.dataType = deCONZ::Zcl16BitInt;
            rq3.attributeId = 0x0012;        // Occupied heating setpoint
            rq3.minInterval = 1;
            rq3.maxInterval = 43200;
            rq3.reportableChange16bit = 1;

            ConfigureReportingRequest rq4;
            rq4.dataType = deCONZ::Zcl16BitInt;
            rq4.attributeId = 0x0014;        // Unoccupied heating setpoint
            rq4.minInterval = 1;
            rq4.maxInterval = 43200;
            rq4.reportableChange16bit = 1;

            ConfigureReportingRequest rq5;
            rq5.dataType = deCONZ::Zcl8BitBitMap;
            rq5.attributeId = 0x4110;        // Danfoss Output Status
            rq5.minInterval = 60;
            rq5.maxInterval = 3600;
            rq5.reportableChange8bit = 1;
            rq5.manufacturerCode = VENDOR_DANFOSS;

            return sendConfigureReportingRequest(bt, {rq, rq2, rq3, rq4}) || // Use OR because of manuf. specific attributes
                   sendConfigureReportingRequest(bt, {rq5});
        }
        else if (modelId == QLatin1String("902010/32")) // Bitron thermostat
        {
            rq.dataType = deCONZ::Zcl16BitInt;
            rq.attributeId = 0x0000;         // local temperature
            rq.minInterval = 0;
            rq.maxInterval = 300;
            rq.reportableChange16bit = 10;

            ConfigureReportingRequest rq2;
            rq2.dataType = deCONZ::Zcl16BitInt;
            rq2.attributeId = 0x0011;        // Occupied cooling setpoint
            rq2.minInterval = 1;
            rq2.maxInterval = 600;
            rq2.reportableChange16bit = 50;

            ConfigureReportingRequest rq3;
            rq3.dataType = deCONZ::Zcl16BitInt;
            rq3.attributeId = 0x0012;        // Occupied heating setpoint
            rq3.minInterval = 1;
            rq3.maxInterval = 600;
            rq3.reportableChange16bit = 50;

            ConfigureReportingRequest rq4;
            rq4.dataType = deCONZ::Zcl8BitEnum;
            rq4.attributeId = 0x001B;        // Control Sequence of operation
            rq4.minInterval = 1;
            rq4.maxInterval = 600;
            rq4.reportableChange8bit = 0xff;

            ConfigureReportingRequest rq5;
            rq5.dataType = deCONZ::Zcl8BitEnum;
            rq5.attributeId = 0x001C;        // Thermostat mode
            rq5.minInterval = 1;
            rq5.maxInterval = 600;
            rq5.reportableChange8bit = 0xff;

            return sendConfigureReportingRequest(bt, {rq, rq2, rq3, rq4, rq5});
        }
        else if (modelId == QLatin1String("TH1300ZB")) // Sinope thermostat
        {
            rq.dataType = deCONZ::Zcl16BitInt;
            rq.attributeId = 0x0000;       // local temperature
            rq.minInterval = 60;
            rq.maxInterval = 3600;
            rq.reportableChange16bit = 50;

            ConfigureReportingRequest rq2;
            rq2.dataType = deCONZ::Zcl8BitUint;
            rq2.attributeId = 0x0008;        // Pi heating demand
            rq2.minInterval = 60;
            rq2.maxInterval = 43200;
            rq2.reportableChange8bit = 1;

            ConfigureReportingRequest rq3;
            rq3.dataType = deCONZ::Zcl16BitInt;
            rq3.attributeId = 0x0012;        // Occupied heating setpoint
            rq3.minInterval = 1;
            rq3.maxInterval = 43200;
            rq3.reportableChange16bit = 1;

            ConfigureReportingRequest rq4;
            rq4.dataType = deCONZ::Zcl8BitEnum;
            rq4.attributeId = 0x001C;        // Thermostat mode
            rq4.minInterval = 1;
            rq4.maxInterval = 600;
            rq4.reportableChange8bit = 0xff;

            return sendConfigureReportingRequest(bt, {rq, rq2, rq3, rq4});
        }
        else if (modelId == QLatin1String("ALCANTARA2 D1.00P1.01Z1.00")) // Alcantara 2 acova
        {
            rq.dataType = deCONZ::Zcl16BitInt;
            rq.attributeId = 0x0000;         // Local Temperature
            rq.minInterval = 1;
            rq.maxInterval = 600;
            rq.reportableChange16bit = 50;

            ConfigureReportingRequest rq2;
            rq2.dataType = deCONZ::Zcl16BitInt;
            rq2.attributeId = 0x0011;        // Occupied cooling setpoint
            rq2.minInterval = 1;
            rq2.maxInterval = 600;
            rq2.reportableChange16bit = 50;

            ConfigureReportingRequest rq3;
            rq3.dataType = deCONZ::Zcl16BitInt;
            rq3.attributeId = 0x0012;        // Occupied heating setpoint
            rq3.minInterval = 1;
            rq3.maxInterval = 600;
            rq3.reportableChange16bit = 50;

            ConfigureReportingRequest rq4;
            rq4.dataType = deCONZ::Zcl8BitEnum;
            rq4.attributeId = 0x001C;        // Thermostat mode
            rq4.minInterval = 1;
            rq4.maxInterval = 600;
            rq4.reportableChange8bit = 0xff;

            return sendConfigureReportingRequest(bt, {rq, rq2, rq3, rq4});
        }
        else
        {
            rq.dataType = deCONZ::Zcl16BitInt;
            rq.attributeId = 0x0000;       // local temperature
            rq.minInterval = 0;
            rq.maxInterval = 300;
            rq.reportableChange16bit = 10;
            return sendConfigureReportingRequest(bt, {rq});
        }
    }
    else if (bt.binding.clusterId == THERMOSTAT_UI_CONFIGURATION_CLUSTER_ID)
    {
        if (modelId == QLatin1String("SORB") ||               // Stelpro Orleans Fan
                 modelId == QLatin1String("TH1300ZB") ||           // Sinope thermostat
                 modelId == QLatin1String("PR412C") ||             // Owon thermostat
                 modelId.startsWith(QLatin1String("3157100")) ||   // Centralite pearl
                 modelId.startsWith(QLatin1String("STZB402")))     // Stelpro baseboard thermostat
        {
            rq.dataType = deCONZ::Zcl8BitEnum;
            rq.attributeId = 0x0001;       // Keypad Lockout
            rq.minInterval = 1;
            rq.maxInterval = 43200;
            rq.reportableChange8bit = 0xff;

            return sendConfigureReportingRequest(bt, {rq});
        }
    }
    else if (bt.binding.clusterId == FAN_CONTROL_CLUSTER_ID)
    {
        if (modelId.startsWith(QLatin1String("3157100")))   // Centralite pearl
        {
            rq.dataType = deCONZ::Zcl8BitEnum;
            rq.attributeId = 0x0000;        // Fan mode
            rq.minInterval = 1;
            rq.maxInterval = 600;
            rq.reportableChange8bit = 0xff;
            return sendConfigureReportingRequest(bt, {rq});
        }
    }
    else if (bt.binding.clusterId == RELATIVE_HUMIDITY_CLUSTER_ID)
    {
        rq.dataType = deCONZ::Zcl16BitUint;
        rq.attributeId = 0x0000;       // measured value
        rq.minInterval = 10;
        rq.maxInterval = 300;
        rq.reportableChange16bit = 100; // resolution: 1%

        return sendConfigureReportingRequest(bt, {rq});
    }
    else if (bt.binding.clusterId == PRESSURE_MEASUREMENT_CLUSTER_ID)
    {
        rq.dataType = deCONZ::Zcl16BitUint;
        rq.attributeId = 0x0000; // measured value
        rq.minInterval = 10;
        rq.maxInterval = 300;
        rq.reportableChange16bit = 20;
        return sendConfigureReportingRequest(bt, {rq});
    }
    else if (bt.binding.clusterId == SOIL_MOISTURE_CLUSTER_ID)
    {
        rq.dataType = deCONZ::Zcl16BitUint;
        rq.attributeId = 0x0000; // measured value
        rq.minInterval = 10;
        rq.maxInterval = 300;
        rq.reportableChange16bit = 20;
        return sendConfigureReportingRequest(bt, {rq});
    }
    else if (bt.binding.clusterId == BINARY_INPUT_CLUSTER_ID)
    {
        rq.dataType = deCONZ::ZclBoolean;
        rq.attributeId = 0x0055; // present value
        rq.minInterval = 10;
        rq.maxInterval = 300;
        return sendConfigureReportingRequest(bt, {rq});
    }
    else if (bt.binding.clusterId == POWER_CONFIGURATION_CLUSTER_ID)
    {
        // Thoses device use only Attribute 0x0000 for tension and 0x001 for frequency
        if (modelId == QLatin1String("SLP2") ||
            modelId == QLatin1String("SLP2b"))
        {
            return false;
        }

        rq.dataType = deCONZ::Zcl8BitUint;
        rq.attributeId = 0x0021;   // battery percentage remaining
        if (modelId.startsWith(QLatin1String("SPZB")))   // Eurotronic Spirit
        {
            rq.minInterval = 7200;       // value used by Hue bridge
            rq.maxInterval = 7200;       // value used by Hue bridge
            rq.reportableChange8bit = 0; // value used by Hue bridge
        }
        else if (modelId == QLatin1String("0x8020") ||   // Danfoss RT24V Display thermostat
                 modelId == QLatin1String("0x8021") ||   // Danfoss RT24V Display thermostat with floor sensor
                 modelId == QLatin1String("0x8030") ||   // Danfoss RTbattery Display thermostat
                 modelId == QLatin1String("0x8031") ||   // Danfoss RTbattery Display thermostat with infrared
                 modelId == QLatin1String("0x8034") ||   // Danfoss RTbattery Dial thermostat
                 modelId == QLatin1String("0x8035"))     // Danfoss RTbattery Dial thermostat with infrared
        {
            rq.minInterval = 3600;         // Vendor defaults
            rq.maxInterval = 43200;        // Vendor defaults
            rq.reportableChange8bit = 2;   // Vendor defaults
        }
        else if (modelId == QLatin1String("4512705") ||          // Namron remote control
                 modelId == QLatin1String("4512726") ||          // Namron rotary switch
                 modelId == QLatin1String("CCT591011_AS") ||     // LK Wiser Door / Window Sensor
                 modelId == QLatin1String("CCT592011_AS") ||     // LK Wiser Water Leak Sensor
                 modelId.startsWith(QLatin1String("S57003")) ||  // SLC switches
                 modelId == QLatin1String("CCT593011_AS") ||     // LK Wiser Temperature and Humidity Sensor
                 modelId == QLatin1String("CCT595011_AS") ||     // LK Wiser Motion Sensor
                 modelId == QLatin1String("ZB-DoorSensor-D0003") || // Linkind Door/Window Sensor
                 modelId.startsWith(QLatin1String("FNB56-")) ||  // Feibit devices
                 modelId.startsWith(QLatin1String("FB56-")))     // Feibit devices
        {
            rq.minInterval = 3600;
            rq.maxInterval = 43200;
            rq.reportableChange8bit = 1;
        }
        else if (modelId == QLatin1String("HG06323") || // LIDL
                 modelId == QLatin1String("lumi.flood.agl02"))           // Xiaomi Aqara T1 water leak sensor SJCGQ12LM
        {
            rq.minInterval = 7200;
            rq.maxInterval = 7200;
            rq.reportableChange8bit = 1;
        }
        else if (sensor && (sensor->manufacturer().startsWith(QLatin1String("Climax")) ||
                            modelId.startsWith(QLatin1String("902010/23"))))
        {
            rq.attributeId = 0x0035; // battery alarm mask
            rq.dataType = deCONZ::Zcl8BitBitMap;
            rq.minInterval = 300;
            rq.maxInterval = 1800;
            rq.reportableChange8bit = 0xFF;
        }
        else if (modelId == QLatin1String("tagv4") ||
                 modelId == QLatin1String("motionv4") ||
                 modelId == QLatin1String("moisturev4") ||
                 modelId == QLatin1String("multiv4") ||
                 modelId == QLatin1String("RFDL-ZB-MS") ||
                 modelId == QLatin1String("SZ-DWS04") ||
                 modelId == QLatin1String("Zen-01") ||
                 modelId == QLatin1String("Bell") ||
                 modelId == QLatin1String("ISW-ZPR1-WP13") ||
                 modelId == QLatin1String("SLT2") ||
                 modelId == QLatin1String("SLT3") ||
                 modelId == QLatin1String("TS0202") || // Tuya sensor
                 modelId == QLatin1String("3AFE14010402000D") || // Konke presence sensor
                 modelId == QLatin1String("3AFE28010402000D") || // Konke presence sensor
                 modelId.startsWith(QLatin1String("GZ-PIR02")) ||          // Sercomm motion sensor
                 modelId.startsWith(QLatin1String("SZ-WTD02N_CAR")) ||     // Sercomm water sensor
                 modelId.startsWith(QLatin1String("3300")) ||          // Centralite contatc sensor
                 modelId.startsWith(QLatin1String("3315")) ||
                 modelId.startsWith(QLatin1String("3157100")) ||
                 modelId == QLatin1String("URC4450BC0-X-R") || // Xfinity Keypad XHK1-UE / URC4450BC0-X-R
                 modelId == QLatin1String("3405-L") || // IRIS 3405-L Keypad
                 modelId.startsWith(QLatin1String("4655BC0")))
        {
            rq.attributeId = 0x0020;   // battery voltage
            rq.minInterval = 3600;
            rq.maxInterval = 3600;
            rq.reportableChange8bit = 0;
        }
        else if (modelId.startsWith(QLatin1String("Lightify Switch Mini")) ||  // Osram 3 button remote
                 modelId.startsWith(QLatin1String("Switch 4x EU-LIGHTIFY")) || // Osram 4 button remote
                 modelId.startsWith(QLatin1String("Switch 4x-LIGHTIFY")) ||    // Osram 4 button remote
                 modelId.startsWith(QLatin1String("Switch-LIGHTIFY")))         // Osram 4 button remote
        {
            rq.attributeId = 0x0020;
            rq.minInterval = 21600;
            rq.maxInterval = 21600;
            rq.reportableChange8bit = 0;
        }
        else if (modelId.startsWith(QLatin1String("MOSZB-1")) ||         // Develco motion sensor
                 modelId.startsWith(QLatin1String("FLSZB-1")) ||         // Develco water leak sensor
                 modelId.startsWith(QLatin1String("SIRZB-1")) ||         // Develco siren
                 modelId.startsWith(QLatin1String("ZHMS101")) ||         // Wattle (Develco) magnetic sensor
                 modelId.startsWith(QLatin1String("MotionSensor51AU")))  // Aurora (Develco) motion sensor
        {
            rq.attributeId = 0x0020;   // battery voltage
            rq.minInterval = 300;      // according to technical manual
            rq.maxInterval = 43200;    // according to technical manual
            rq.reportableChange8bit = 1;
        }
        else if (sensor && sensor->manufacturer() == QLatin1String("Samjin"))
        {
            // https://github.com/SmartThingsCommunity/SmartThingsPublic/blob/master/devicetypes/smartthings/smartsense-multi-sensor.src/smartsense-multi-sensor.groovy
            rq.minInterval = 30;
            rq.maxInterval = 21600;
            rq.reportableChange8bit = 10;
        }
        else if (modelId == QLatin1String("lumi.motion.agl04"))                 // Xiaomi Aqara RTCGQ13LM high precision motion sensor
        {
            rq.attributeId = 0x0020;   // battery voltage
            rq.minInterval = 3;
            rq.maxInterval = 3600;
            rq.reportableChange8bit = 1;
        }
        else
        {
            rq.minInterval = 300;
            rq.maxInterval = 60 * 45;
            rq.reportableChange8bit = 1;
        }

        // add values if not already present
        deCONZ::NumericUnion dummy;
        dummy.u64 = 0;
        if (bt.restNode->getZclValue(POWER_CONFIGURATION_CLUSTER_ID, rq.attributeId, bt.binding.srcEndpoint).attributeId != rq.attributeId)
        {
            bt.restNode->setZclValue(NodeValue::UpdateInvalid, bt.binding.srcEndpoint, POWER_CONFIGURATION_CLUSTER_ID, rq.attributeId, dummy);
        }

        NodeValue &val = bt.restNode->getZclValue(POWER_CONFIGURATION_CLUSTER_ID, rq.attributeId, bt.binding.srcEndpoint);

        if (val.timestampLastReport.isValid() && (val.timestampLastReport.secsTo(now) < val.maxInterval * 1.5))
        {
            return false;
        }

        return sendConfigureReportingRequest(bt, {rq});
    }
    else if (bt.binding.clusterId == ONOFF_CLUSTER_ID)
    {
        rq.dataType = deCONZ::ZclBoolean;
        rq.attributeId = 0x0000; // on/off

        if (existDevicesWithVendorCodeForMacPrefix(bt.restNode->address(), VENDOR_DDEL))
        {
            rq.minInterval = 5;
            rq.maxInterval = 180;
        }
        else if (existDevicesWithVendorCodeForMacPrefix(bt.restNode->address(), VENDOR_XAL) ||
                 manufacturerCode == VENDOR_XAL)
        {
            rq.minInterval = 5;
            rq.maxInterval = 1200;
        }
        else if (manufacturerCode == VENDOR_IKEA)
        {
            // IKEA gateway uses min = 0, max = 0
            // Instead here we use relaxed settings to not stress the network and device.
            rq.minInterval = 1;
            rq.maxInterval = 1800;
        }
        else // default configuration
        {
            rq.minInterval = 1;
            rq.maxInterval = 300;
        }
        return sendConfigureReportingRequest(bt, {rq});
    }
    else if (bt.binding.clusterId == METERING_CLUSTER_ID)
    {
        rq.dataType = deCONZ::Zcl48BitUint;
        rq.attributeId = 0x0000; // Curent Summation Delivered
        rq.minInterval = 1;
        rq.maxInterval = 300;
        if (modelId == QLatin1String("SmartPlug") ||               // Heiman
            modelId == QLatin1String("SKHMP30-I1") ||              // GS smart plug
            modelId.startsWith(QLatin1String("E13-")) ||           // Sengled PAR38 Bulbs
            modelId.startsWith(QLatin1String("Z01-A19")))          // Sengled smart led
        {
            rq.reportableChange48bit = 10; // 0.001 kWh (1 Wh)
        }
        else if (modelId == QLatin1String("SZ-ESW01-AU")) // Sercomm / Telstra smart plug
        {
            rq.reportableChange48bit = 1000; // 0.001 kWh (1 Wh)
        }
        else if (modelId.startsWith(QLatin1String("ROB_200")) ||            // ROBB Smarrt micro dimmer
                 modelId.startsWith(QLatin1String("Micro Smart Dimmer")) || // Sunricher Micro Smart Dimmer
                 modelId.startsWith(QLatin1String("SPW35Z")))               // RT-RK OBLO SPW35ZD0 smart plug
        {
            rq.reportableChange48bit = 3600; // 0.001 kWh (1 Wh)
        }
        else
        {
            rq.reportableChange48bit = 1; // 0.001 kWh (1 Wh)
        }

        ConfigureReportingRequest rq2;
        rq2.dataType = deCONZ::Zcl24BitInt;
        rq2.attributeId = 0x0400; // Instantaneous Demand
        rq2.minInterval = 1;
        rq2.maxInterval = 300;
        if (modelId == QLatin1String("SmartPlug") ||        // Heiman
            modelId == QLatin1String("902010/25") ||        // Bitron
            modelId == QLatin1String("SKHMP30-I1") ||       // GS smart plug
            modelId.startsWith(QLatin1String("Z01-A19")) || // Sengled smart led
            modelId == QLatin1String("160-01"))             // Plugwise smart plug
        {
            rq2.reportableChange24bit = 10; // 1 W
        }
        else if (modelId == QLatin1String("SZ-ESW01-AU")) // Sercomm / Telstra smart plug
        {
            rq2.reportableChange24bit = 1000; // 1 W
        }
        else
        {
            rq2.reportableChange24bit = 1; // 1 W
        }

        return sendConfigureReportingRequest(bt, {rq, rq2});
    }
    else if (bt.binding.clusterId == ELECTRICAL_MEASUREMENT_CLUSTER_ID)
    {
        rq.dataType = deCONZ::Zcl16BitInt;
        rq.attributeId = 0x050B; // Active power
        rq.minInterval = 1;
        rq.maxInterval = 300;
        if (modelId == QLatin1String("SmartPlug") ||                   // Heiman
            modelId == QLatin1String("SKHMP30-I1") ||                  // GS smart plug
            modelId == QLatin1String("SZ-ESW01-AU") ||                 // Sercomm / Telstra smart plug
            modelId.startsWith(QLatin1String("ROB_200")) ||            // ROBB Smarrt micro dimmer
            modelId.startsWith(QLatin1String("Micro Smart Dimmer")) || // Sunricher Micro Smart Dimmer
            modelId.startsWith(QLatin1String("lumi.switch.n0agl1")) || // Xiaomi Aqara Single Switch Module T1 (With Neutral)
            modelId.startsWith(QLatin1String("lumi.switch.b1naus01"))) // Xiaomi ZB3.0 Smart Wall Switch
        {
            rq.reportableChange16bit = 10; // 1 W
        }
        else
        {
            rq.reportableChange16bit = 1; // 1 W
        }

        ConfigureReportingRequest rq2;
        rq2.dataType = deCONZ::Zcl16BitUint;
        rq2.attributeId = 0x0505; // RMS Voltage
        rq2.minInterval = 1;
        rq2.maxInterval = 300;
        if (modelId == QLatin1String("SmartPlug") ||           // Heiman
            modelId == QLatin1String("PoP") ||                 // Apex Smart Plug
            modelId == QLatin1String("SKHMP30-I1") ||          // GS smart plug
            modelId == QLatin1String("SMRZB-1") ||             // Develco smart cable
            modelId == QLatin1String("Smart16ARelay51AU"))     // Aurora (Develco) smart plug
        {
            rq2.reportableChange16bit = 100; // 1 V
        }
        else if (modelId == QLatin1String("SZ-ESW01-AU")) // Sercomm / Telstra smart plug
        {
            rq2.reportableChange16bit = 125; // 1 V
        }
        else if (modelId.startsWith(QLatin1String("ROB_200")) ||            // ROBB Smarrt micro dimmer
                 modelId.startsWith(QLatin1String("Micro Smart Dimmer")))   // Sunricher Micro Smart Dimmer
        {
            rq2.reportableChange16bit = 10; // 1 V
        }
        else
        {
            rq2.reportableChange16bit = 1; // 1 V
        }

        ConfigureReportingRequest rq3;
        rq3.dataType = deCONZ::Zcl16BitUint;
        rq3.attributeId = 0x0508; // RMS Current
        rq3.minInterval = 1;
        rq3.maxInterval = 300;
        if (modelId == QLatin1String("PoP") ||                     // Apex Smart Plug
            modelId == QLatin1String("DoubleSocket50AU") ||        // Aurora
            modelId == QLatin1String("Smart16ARelay51AU") ||       // Aurora (Develco) smart plug
            modelId == QLatin1String("SZ-ESW01-AU") ||             // Sercomm / Telstra smart plug
            modelId == QLatin1String("SMRZB-1") ||                 // Develco smart cable
            modelId == QLatin1String("TS0121"))                    // Tuya / Blitzwolf
        {
            rq3.reportableChange16bit = 100; // 0.1 A
        }
        else if (modelId == QLatin1String("SmartPlug") ||        // Heiman
                 modelId.startsWith(QLatin1String("EMIZB-1")) || // Develco EMI
                 modelId == QLatin1String("SKHMP30-I1") ||       // GS smart plug
                 modelId.startsWith(QLatin1String("SPW35Z")) ||  // RT-RK OBLO SPW35ZD0 smart plug
                 modelId == QLatin1String("TH1300ZB"))           // Sinope thermostat
        {
            rq3.reportableChange16bit = 10; // 0.1 A
        }
        else
        {
            rq3.reportableChange16bit = 1; // 0.1 A
        }

        if (modelId == QLatin1String("TH1300ZB"))
        {
            ConfigureReportingRequest rq4;
            rq4.dataType = deCONZ::Zcl16BitUint;
            rq4.attributeId = 0x050f; // Apparent power
            rq4.minInterval = 1;
            rq4.maxInterval = 300;
            rq4.reportableChange16bit = 100; // 0.1 W

            return sendConfigureReportingRequest(bt, {rq2, rq3, rq4});
        }

        return sendConfigureReportingRequest(bt, {rq, rq2, rq3});
    }
    else if (bt.binding.clusterId == LEVEL_CLUSTER_ID)
    {
        rq.dataType = deCONZ::Zcl8BitUint;
        rq.attributeId = 0x0000; // current level

        if (existDevicesWithVendorCodeForMacPrefix(bt.restNode->address(), VENDOR_DDEL))
        {
            rq.minInterval = 5;
            rq.maxInterval = 180;
            rq.reportableChange8bit = 5;
        }
        else if (manufacturerCode ==  VENDOR_IKEA)
        {
            // IKEA gateway uses min = 1, max = 0, change = 0
            // Instead here we use relaxed settings to not stress the network and device.
            rq.minInterval = 5;
            rq.maxInterval = 1800;
            rq.reportableChange8bit = 1;
        }
        else // default configuration
        {
            rq.minInterval = 1;
            rq.maxInterval = 300;
            rq.reportableChange8bit = 1;
        }
        return sendConfigureReportingRequest(bt, {rq});
    }
    else if (bt.binding.clusterId == WINDOW_COVERING_CLUSTER_ID)
    {
        rq.dataType = deCONZ::Zcl8BitUint;
        rq.attributeId = 0x0008; // Current Position Lift Percentage
        rq.minInterval = 1;
        rq.maxInterval = 300;
        rq.reportableChange8bit = 1;

        return sendConfigureReportingRequest(bt, {rq});
    }
    else if (bt.binding.clusterId == DOOR_LOCK_CLUSTER_ID)
    {
        rq.dataType = deCONZ::Zcl8BitEnum;;
        rq.attributeId = 0x0000; // Current Lock Position
        rq.minInterval = 1;
        rq.maxInterval = 300;
        //rq.reportableChange8bit = 1;

        return sendConfigureReportingRequest(bt, {rq});
    }
    else if (bt.binding.clusterId == FAN_CONTROL_CLUSTER_ID)
    {
        rq.dataType = deCONZ::Zcl8BitEnum;
        rq.attributeId = 0x0000; // fan speed
        rq.minInterval = 1;
        rq.maxInterval = 300;

        return sendConfigureReportingRequest(bt, {rq});
    }
    else if (bt.binding.clusterId == COLOR_CLUSTER_ID)
    {
        rq.dataType = deCONZ::Zcl16BitUint;
        rq.attributeId = 0x0007; // color temperature
        rq.minInterval = 1;
        rq.maxInterval = 300;
        rq.reportableChange16bit = 1;

        ConfigureReportingRequest rq2;
        rq2.dataType = deCONZ::Zcl16BitUint;
        rq2.attributeId = 0x0003; // colorX
        rq2.minInterval = 1;
        rq2.maxInterval = 300;
        rq2.reportableChange16bit = 10;

        ConfigureReportingRequest rq3;
        rq3.dataType = deCONZ::Zcl16BitUint;
        rq3.attributeId = 0x0004; // colorY
        rq3.minInterval = 1;
        rq3.maxInterval = 300;
        rq3.reportableChange16bit = 10;

        ConfigureReportingRequest rq4;
        rq4.dataType = deCONZ::Zcl8BitEnum;
        rq4.attributeId = 0x0008; // color mode
        rq4.minInterval = 1;
        rq4.maxInterval = 300;

        if (manufacturerCode == VENDOR_IKEA)
        {
            // IKEA gateway uses all zero values for min, max and change, which results in very rapid reports.
            // Instead here we use relaxed settings to not stress the network and device.
            rq.minInterval = 5;
            rq.maxInterval = 1800;
            rq.reportableChange16bit = 1;
            rq2.minInterval = 5;
            rq2.maxInterval = 1795;
            rq2.reportableChange16bit = 10;
            rq3.minInterval = 5;
            rq3.maxInterval = 1795;
            rq3.reportableChange16bit = 10;
            rq4.minInterval = 1;
            rq4.maxInterval = 1800;

//          TODO re activate. Don't disable for now until more testing is done.
//            const ResourceItem *cap = lightNode ? lightNode->item(RCapColorCapabilities) : nullptr;

//            if (cap && (cap->toNumber() & 0x0008) == 0) // doesn't support xy --> color temperature light
//            {
//                rq2.minInterval = 0;
//                rq2.maxInterval = 0xffff; // disable reporting
//                rq3.minInterval = 0;
//                rq3.maxInterval = 0xffff; // disable reporting
//            }
        }

        return sendConfigureReportingRequest(bt, {rq, rq2, rq3, rq4});
    }
    else if (bt.binding.clusterId == SAMJIN_CLUSTER_ID)
    {
        // based on https://github.com/SmartThingsCommunity/SmartThingsPublic/blob/master/devicetypes/smartthings/smartsense-multi-sensor.src/smartsense-multi-sensor.groovy
        if (sensor && sensor->type() == QLatin1String("ZHAVibration"))
        {
            const quint16 manufacturerCode2 = sensor->manufacturer() == QLatin1String("Samjin") ? VENDOR_SAMJIN
                : sensor->manufacturer() == QLatin1String("SmartThings") ? VENDOR_PHYSICAL : VENDOR_CENTRALITE;
            const quint16 minInterval = manufacturerCode2 == VENDOR_SAMJIN ? 0 : 1;

            rq.dataType = deCONZ::Zcl8BitBitMap;
            rq.attributeId = 0x0010; // active
            rq.minInterval = manufacturerCode2 == VENDOR_SAMJIN ? 0 : 10;
            rq.maxInterval = 3600;
            rq.reportableChange8bit = 0xFF;
            rq.manufacturerCode = manufacturerCode2;

            ConfigureReportingRequest rq1;
            rq1.dataType = deCONZ::Zcl16BitInt;
            rq1.attributeId = 0x0012; // acceleration x
            rq1.minInterval = minInterval;
            rq1.maxInterval = 3600;
            rq1.reportableChange16bit = 1;
            rq1.manufacturerCode = manufacturerCode2;

            ConfigureReportingRequest rq2;
            rq2.dataType = deCONZ::Zcl16BitInt;
            rq2.attributeId = 0x0013; // acceleration y
            rq2.minInterval = minInterval;
            rq2.maxInterval = 3600;
            rq2.reportableChange16bit = 1;
            rq2.manufacturerCode = manufacturerCode2;

            ConfigureReportingRequest rq3;
            rq3.dataType = deCONZ::Zcl16BitInt;
            rq3.attributeId = 0x0014; // acceleration z
            rq3.minInterval = minInterval;
            rq3.maxInterval = 3600;
            rq3.reportableChange16bit = 1;
            rq3.manufacturerCode = manufacturerCode2;

            return sendConfigureReportingRequest(bt, {rq, rq1, rq2, rq3});
        }
    }
    else if (bt.binding.clusterId == BASIC_CLUSTER_ID && manufacturerCode == VENDOR_IKEA && lightNode)
    {
        deCONZ::NumericUnion dummy;
        dummy.u64 = 0;
        // 'sw build id' value if not already present
        if (bt.restNode->getZclValue(BASIC_CLUSTER_ID, 0x4000, bt.binding.srcEndpoint).attributeId != 0x4000)
        {
            bt.restNode->setZclValue(NodeValue::UpdateInvalid, bt.binding.srcEndpoint, BASIC_CLUSTER_ID, 0x4000, dummy);
        }

        NodeValue &val = bt.restNode->getZclValue(BASIC_CLUSTER_ID, 0x4000, bt.binding.srcEndpoint);

        if (val.timestampLastReport.isValid() && (val.timestampLastReport.secsTo(now) > val.maxInterval * 1.5))
        {
            return false; // reporting this attribute might be already disabled
        }

        // already configured? wait for report ...
        if (val.timestampLastConfigured.isValid() && (val.timestampLastConfigured.secsTo(now) < val.maxInterval * 1.5))
        {
            return false;
        }

        rq.dataType = deCONZ::ZclCharacterString;
        rq.attributeId = 0x4000; // sw build id
        rq.minInterval = 0;   // value used by IKEA gateway
        rq.maxInterval = 0xffff; // disable reporting to prevent group casts

        return sendConfigureReportingRequest(bt, {rq});
    }
    else if (bt.binding.clusterId == BASIC_CLUSTER_ID && manufacturerCode == VENDOR_MUELLER && lightNode)
    {
        rq.dataType = deCONZ::Zcl8BitUint;
        rq.attributeId = 0x4005; // Mueller special scene
        rq.minInterval = 1;
        rq.maxInterval = 300;
        rq.reportableChange8bit = 1;
        rq.manufacturerCode = VENDOR_MUELLER;

        return sendConfigureReportingRequest(bt, {rq});
    }

    return false;
}

/*! Creates binding for attribute reporting to gateway node. */
void DeRestPluginPrivate::checkLightBindingsForAttributeReporting(LightNode *lightNode)
{
    if (!apsCtrl || !lightNode || !lightNode->address().hasExt())
    {
        return;
    }

    Device *device = DEV_GetDevice(m_devices, lightNode->address().ext());
    if (device && device->managed())
    {
        return;
    }

    BindingTask::Action action = BindingTask::ActionUnbind;

    // whitelist
    if (gwReportingEnabled)
    {
        action = BindingTask::ActionBind;
        if (lightNode->manufacturer() == QLatin1String("OSRAM"))
        {
        }
        else if (lightNode->manufacturer() == QLatin1String("LEDVANCE"))
        {
        }
        else if (lightNode->manufacturerCode() == VENDOR_JASCO)
        {
        }
        else if (lightNode->manufacturerCode() == VENDOR_UBISYS)
        {
        }
        // Danalock support
        else if (lightNode->manufacturerCode() == VENDOR_DANALOCK)
        {
            DBG_Printf(DBG_INFO, "Binding DanaLock\n");
        }
        // Schlage support
        else if (lightNode->manufacturerCode() == VENDOR_SCHLAGE)
        {
            DBG_Printf(DBG_INFO, "Binding Schlage\n");
        }
        else if (lightNode->manufacturerCode() == VENDOR_IKEA)
        {
        }
        else if (lightNode->manufacturerCode() == VENDOR_EMBER)
        {
        }
        else if (lightNode->manufacturerCode() == VENDOR_LGE)
        {
        }
        else if (lightNode->manufacturerCode() == VENDOR_MUELLER)
        {
        }
        else if (lightNode->manufacturerCode() == VENDOR_KEEN_HOME)
        {
        }
        else if (lightNode->manufacturerCode() == VENDOR_SUNRICHER)
        {
        }
        else if (lightNode->manufacturerCode() == VENDOR_XAL)
        {
        }
        else if (lightNode->manufacturerCode() == VENDOR_SINOPE)
        {
        }
        else if (lightNode->manufacturerCode() == VENDOR_OWON)
        {
        }
        else if (lightNode->manufacturerCode() == VENDOR_XIAOMI)
        {
        }
        else if (lightNode->manufacturerCode() == VENDOR_STELPRO)
        {
        }
        else if (lightNode->manufacturerCode() == VENDOR_DATEK)
        {
        }
        else if (lightNode->modelId().startsWith(QLatin1String("SP ")))
        {
        }
        else if (lightNode->manufacturer().startsWith(QLatin1String("Climax")))
        {
        }
        else if (lightNode->manufacturer().startsWith(QLatin1String("Bitron")))
        {
        }
        else if (lightNode->modelId() == QLatin1String("NL08-0800")) // Nanoleaf Ivy
        {
        }
        else if (lightNode->modelId().startsWith(QLatin1String("ICZB-"))) // iCasa Dimmer and Switch
        {
        }
        else if (lightNode->manufacturer().startsWith(QLatin1String("Develco"))) // Develco devices
        {
        }
        else if (lightNode->manufacturer() == QLatin1String("Aurora"))
        {
        }
        else if (lightNode->modelId().startsWith(QLatin1String("RICI01"))) // LifeControl smart plug
        {
        }
        else if (lightNode->modelId() == QLatin1String("SPLZB-131"))
        {
        }
        else if (lightNode->manufacturer() == QLatin1String("Computime")) //Hive
        {
        }
        else if (lightNode->manufacturer() == QString("欧瑞博") || lightNode->manufacturer() == QLatin1String("ORVIBO"))
        {
        }
        else if (lightNode->manufacturerCode() == VENDOR_LEGRAND) // Legrand switch and plug
        {
        }
        else if (lightNode->manufacturerCode() == VENDOR_NETVOX) // Netvox smart plug
        {
        }
        else if (lightNode->manufacturer() == QLatin1String("ptvo.info"))
        {
        }
        else if (lightNode->manufacturer() == QLatin1String("DIYRUZ"))
        {
        }
        else if (lightNode->manufacturer() == QLatin1String("Immax"))
        {
        }
        else if (lightNode->manufacturer().startsWith(QLatin1String("EcoDim")))
        {
        }
        else if (lightNode->manufacturer().startsWith(QLatin1String("ROBB smarrt")))
        {
        }
        else if (lightNode->manufacturer().startsWith(QLatin1String("Feibit")))
        {
        }
        else if (lightNode->manufacturer() == QLatin1String("sengled"))
        {
        }
        else if (lightNode->manufacturer() == QLatin1String("LDS"))
        {
        }
        else if (lightNode->manufacturer() == QLatin1String("Vimar"))
        {
        }
        else if (lightNode->manufacturer() == QLatin1String("Sercomm Corp."))
        {
        }
        else if (lightNode->manufacturer() == QLatin1String("Kwikset"))
        {
        }
        else if (lightNode->manufacturerCode() == VENDOR_YALE)
        {
        }
        else if (lightNode->manufacturer() == QLatin1String("NIKO NV"))
        {
        }
        else if (lightNode->manufacturerCode() == VENDOR_AXIS || lightNode->manufacturerCode() == VENDOR_MMB) // Axis shade
        {
        }
        else if (lightNode->manufacturer() == QLatin1String("yookee") || // IDK if it s better use VENDOR_SI_LABS
                 lightNode->manufacturer() == QLatin1String("yooksmart"))
        {
        }
        else if (lightNode->manufacturer() == QLatin1String("Sunricher"))
        {
        }
        else
        {
            return;
        }
    }
    else
    {
        return;
    }

    auto i = lightNode->haEndpoint().inClusters().begin();
    const auto end = lightNode->haEndpoint().inClusters().end();

    int tasksAdded = 0;
    QDateTime now = QDateTime::currentDateTime();

    for (; i != end; ++i)
    {
        switch (i->id())
        {
        case BASIC_CLUSTER_ID:
        case ONOFF_CLUSTER_ID:
        case LEVEL_CLUSTER_ID:
        case COLOR_CLUSTER_ID:
        case WINDOW_COVERING_CLUSTER_ID:
        // Danalock support
        case DOOR_LOCK_CLUSTER_ID:
        case IAS_ZONE_CLUSTER_ID:
        case FAN_CONTROL_CLUSTER_ID:
        {
            bool bindingExists = false;
            for (const NodeValue &val : lightNode->zclValues())
            {
                if (val.clusterId != i->id())
                {
                    continue;
                }

                quint16 maxInterval = val.maxInterval > 0 && val.maxInterval < 65535 ? (val.maxInterval * 3 / 2) : (60 * 6);

                if (val.timestampLastReport.isValid() && val.timestampLastReport.secsTo(now) < maxInterval)
                {
                    bindingExists = true;
                    break;
                }

                if (val.timestampLastConfigured.isValid())
                {
                    bindingExists = true;
                    break;
                }
            }

            // only IKEA lights should report basic cluster attributes
            if (lightNode->manufacturerCode() != VENDOR_IKEA && i->id() == BASIC_CLUSTER_ID)
            {
                continue;
            }

            BindingTask bt;
            if (existDevicesWithVendorCodeForMacPrefix(lightNode->address(), VENDOR_DDEL))
            {
                bt.state = BindingTask::StateCheck;
            }
            else
            {
                bt.state = BindingTask::StateIdle;
            }
            bt.action = action;
            bt.restNode = lightNode;
            Binding &bnd = bt.binding;
            bnd.srcAddress = lightNode->address().ext();
            bnd.dstAddrMode = deCONZ::ApsExtAddress;
            bnd.srcEndpoint = lightNode->haEndpoint().endpoint();
            bnd.clusterId = i->id();
            bnd.dstAddress.ext = apsCtrl->getParameter(deCONZ::ParamMacAddress);
            bnd.dstEndpoint = endpoint();

            if (bnd.dstEndpoint > 0) // valid gateway endpoint?
            {
                if (bindingExists)
                {
                    DBG_Printf(DBG_INFO, "binding for cluster 0x%04X of 0x%016llX exists (verified by reporting)\n", i->id(), lightNode->address().ext());
                    sendConfigureReportingRequest(bt);
                }
                else
                {
                    DBG_Printf(DBG_INFO_L2, "create binding for attribute reporting of cluster 0x%04X\n", i->id());
                    queueBindingTask(bt);
                    tasksAdded++;
                }
            }
        }
            break;

        default:
            break;
        }
    }

    if (tasksAdded == 0)
    {
        return;
    }

    if (existDevicesWithVendorCodeForMacPrefix(lightNode->address(), VENDOR_DDEL) || lightNode->manufacturerCode() == VENDOR_XAL)
    {
        lightNode->enableRead(READ_BINDING_TABLE);
        lightNode->setNextReadTime(READ_BINDING_TABLE, queryTime);
        queryTime = queryTime.addSecs(5);
        Q_Q(DeRestPlugin);
        q->startZclAttributeTimer(1000);
    }

    if (!bindingTimer->isActive())
    {
        bindingTimer->start();
    }
}


/*! Creates binding for attribute reporting to gateway node.
    \return true - when a binding request got queued.
 */
bool DeRestPluginPrivate::checkSensorBindingsForAttributeReporting(Sensor *sensor)
{
    if (!apsCtrl || !sensor || !sensor->address().hasExt() || !sensor->node() || !sensor->toBool(RConfigReachable))
    {
        return false;
    }

    if (searchSensorsState != SearchSensorsActive &&
        idleTotalCounter < (IDLE_READ_LIMIT + (60 * 15))) // wait for some input before fire bindings
    {
        return false;
    }

    if (sensor->node()->nodeDescriptor().isNull())
    {
        // Whitelist sensors which don't seem to have a valid node descriptor.
        // This is a workaround currently only required for Develco smoke sensor
        // and potentially Bosch motion sensor
        if (sensor->modelId().startsWith(QLatin1String("EMIZB-1")) ||     // Develco EMI Norwegian HAN
            sensor->modelId().startsWith(QLatin1String("ISW-ZPR1-WP13"))) // Bosch motion sensor
        {
        }
        else
        {
            return false;
        }
    }

    if (sensor->deletedState() != Sensor::StateNormal)
    {
        return false;
    }

    Device *device = DEV_GetDevice(m_devices, sensor->address().ext());
    if (device && device->managed())
    {
        return false;
    }

    bool deviceSupported = false;
    // whitelist
        // Climax
    if (sensor->modelId().startsWith(QLatin1String("LM_")) ||
        sensor->modelId().startsWith(QLatin1String("LMHT_")) ||
        sensor->modelId().startsWith(QLatin1String("IR_")) ||
        sensor->modelId().startsWith(QLatin1String("DC_")) ||
        sensor->modelId().startsWith(QLatin1String("PSMD_")) ||
        sensor->modelId().startsWith(QLatin1String("PSMP5_")) ||
        sensor->modelId().startsWith(QLatin1String("PCM_")) ||
        // CentraLite
        sensor->modelId().startsWith(QLatin1String("3300")) ||
        sensor->modelId().startsWith(QLatin1String("332")) ||
        sensor->modelId().startsWith(QLatin1String("3200-Sgb")) ||
        sensor->modelId() == QLatin1String("3200-de") ||
        sensor->modelId().startsWith(QLatin1String("3305-S")) ||
        sensor->modelId().startsWith(QLatin1String("3315")) ||
        sensor->modelId().startsWith(QLatin1String("3320-L")) ||
        sensor->modelId().startsWith(QLatin1String("3323")) ||
        sensor->modelId().startsWith(QLatin1String("3326-L")) ||
        sensor->modelId().startsWith(QLatin1String("3157100")) ||
        // GE
        (sensor->manufacturer() == QLatin1String("Jasco Products") && sensor->modelId() == QLatin1String("45856")) ||
        // NYCE
        sensor->modelId() == QLatin1String("3011") ||
        sensor->modelId() == QLatin1String("3014") ||
        sensor->modelId() == QLatin1String("3041") ||
        sensor->modelId() == QLatin1String("3043") ||
        //Datek
        sensor->modelId().startsWith(QLatin1String("ID Lock 150")) ||
        // Yale
        sensor->modelId() == QLatin1String("YRD256 TSDB") ||
        sensor->modelId() == QLatin1String("YRD226 TSDB") ||
        sensor->modelId() == QLatin1String("YRD226/246 TSDB") ||
        sensor->modelId() == QLatin1String("YRD220/240 TSDB") ||
        sensor->modelId() == QLatin1String("easyCodeTouch_v1") ||
        // IKEA
        sensor->modelId().startsWith(QLatin1String("TRADFRI")) ||
        sensor->modelId().startsWith(QLatin1String("SYMFONISK")) ||
        // OSRAM
        sensor->modelId().startsWith(QLatin1String("Lightify Switch Mini")) ||  // Osram 3 button remote
        sensor->modelId().startsWith(QLatin1String("Switch 4x EU-LIGHTIFY")) || // Osram 4 button remote
        sensor->modelId().startsWith(QLatin1String("Switch 4x-LIGHTIFY")) || // Osram 4 button remote
        sensor->modelId().startsWith(QLatin1String("Switch-LIGHTIFY")) || // Osram 4 button remote
        // Keen Home
        sensor->modelId().startsWith(QLatin1String("SV01-")) ||
        sensor->modelId().startsWith(QLatin1String("SV02-")) ||
        // Trust ZPIR-8000
        sensor->modelId().startsWith(QLatin1String("VMS_ADUROLIGHT")) ||
        // Trust ZMST-808
        sensor->modelId().startsWith(QLatin1String("CSW_ADUROLIGHT")) ||
        // iCasa
        sensor->modelId().startsWith(QLatin1String("ICZB-RM")) ||
        // Envilar
        sensor->modelId() == QLatin1String("ZGR904-S") ||
        // innr
        sensor->modelId().startsWith(QLatin1String("SP ")) ||
        sensor->modelId().startsWith(QLatin1String("RC 110")) ||
        // Eurotronic
        sensor->modelId() == QLatin1String("SPZB0001") ||
        // Heiman
        // I don't think the IAS Zone sensor need to be listed here?
        sensor->modelId().startsWith(QLatin1String("SmartPlug")) ||
        sensor->modelId().startsWith(QLatin1String("CO_")) ||
        sensor->modelId().startsWith(QLatin1String("DOOR_")) ||
        sensor->modelId().startsWith(QLatin1String("PIR_")) ||
        sensor->modelId().startsWith(QLatin1String("GAS")) ||
        sensor->modelId().startsWith(QLatin1String("TH-")) ||
        sensor->modelId().startsWith(QLatin1String("HT-")) ||
        sensor->modelId().startsWith(QLatin1String("SMOK_")) ||
        sensor->modelId().startsWith(QLatin1String("WATER_")) ||
        sensor->modelId().startsWith(QLatin1String("Smoke")) ||
        sensor->modelId().startsWith(QLatin1String("COSensor")) ||
        sensor->modelId().startsWith(QLatin1String("Water")) ||
        sensor->modelId().startsWith(QLatin1String("Door")) ||
        sensor->modelId().startsWith(QLatin1String("WarningDevice")) ||
        sensor->modelId().startsWith(QLatin1String("PIRS")) ||
        sensor->modelId().startsWith(QLatin1String("SKHMP30")) || // GS smart plug
        sensor->modelId().startsWith(QLatin1String("RC_V14")) ||
        sensor->modelId().startsWith(QLatin1String("RC-EM")) ||
        sensor->modelId().startsWith(QLatin1String("RC-EF-3.0")) ||
        // lidl / SilverCrest
        sensor->modelId() == QLatin1String("TY0203") ||  // Door sensor
        sensor->modelId() == QLatin1String("TY0202") || // Motion Sensor
        sensor->modelId() == QLatin1String("TS0211") || // Door bell
        // DIYRuZ
        sensor->modelId() == QLatin1String("DIYRuZ_Flower") || // DIYRuZ_Flower
        // Konke
        sensor->modelId() == QLatin1String("3AFE140103020000") ||
        sensor->modelId() == QLatin1String("3AFE130104020015") ||
        sensor->modelId() == QLatin1String("3AFE14010402000D") ||
        sensor->modelId() == QLatin1String("3AFE220103020000") ||
        sensor->modelId() == QLatin1String("3AFE28010402000D") ||
        // Danalock support
        sensor->modelId().startsWith(QLatin1String("V3")) ||
        // Schlage support
        sensor->modelId().startsWith(QLatin1String("BE468")) ||
        // SmartThings
        sensor->modelId().startsWith(QLatin1String("tagv4")) ||
        sensor->modelId().startsWith(QLatin1String("motionv4")) ||
        sensor->modelId().startsWith(QLatin1String("moisturev4")) ||
        sensor->modelId() == QLatin1String("button") ||
        (sensor->manufacturer() == QLatin1String("Samjin") && sensor->modelId() == QLatin1String("motion")) ||
        sensor->modelId().startsWith(QLatin1String("multi")) ||
        sensor->modelId() == QLatin1String("water") ||
        (sensor->manufacturer() == QLatin1String("Samjin") && sensor->modelId() == QLatin1String("outlet")) ||
        // Axis
        sensor->modelId() == QLatin1String("Gear") ||
        // Yookee
        sensor->modelId() == QLatin1String("D10110") ||
        // Datek
        sensor->modelId() == QLatin1String("PoP") ||
        // Bitron
        sensor->modelId().startsWith(QLatin1String("902010")) ||
        // Develco
        sensor->modelId().startsWith(QLatin1String("FLSZB-1")) ||   // water leak sensor
        sensor->modelId().startsWith(QLatin1String("MOSZB-1")) ||   // motion sensor
        sensor->modelId().startsWith(QLatin1String("ZHMS101")) ||   // Wattle (Develco) magnetic sensor
        sensor->modelId().startsWith(QLatin1String("EMIZB-1")) ||   // EMI Norwegian HAN
        sensor->modelId().startsWith(QLatin1String("SMRZB-3")) ||   // Smart Relay DIN
        sensor->modelId().startsWith(QLatin1String("SMRZB-1")) ||   // Smart Cable
        sensor->modelId().startsWith(QLatin1String("SIRZB-1")) ||   // siren
        sensor->modelId() == QLatin1String("MotionSensor51AU") ||   // Aurora (Develco) motion sensor
        sensor->modelId() == QLatin1String("Smart16ARelay51AU") ||  // Aurora (Develco) smart plug
        // LG
        sensor->modelId() == QLatin1String("LG IP65 HMS") ||
        // Sinope
        sensor->modelId().startsWith(QLatin1String("WL4200")) || // water leak sensor
        sensor->modelId().startsWith(QLatin1String("TH1300ZB")) || // thermostat
        //LifeControl smart plug
        sensor->modelId() == QLatin1String("RICI01") ||
        //LifeControl enviroment sensor
        sensor->modelId() == QLatin1String("VOC_Sensor") ||
        // EDP-WITHUS
        sensor->modelId() == QLatin1String("ZB-SmartPlug-1.0.0") ||
        sensor->modelId().startsWith(QLatin1String("ZBT-DIMController-D0800")) || // Mueller-Licht tint dimmer
        //Legrand
        sensor->modelId() == QLatin1String("Connected outlet") || //Legrand Plug
        sensor->modelId() == QLatin1String("Shutter switch with neutral") || //Legrand shutter switch
        sensor->modelId() == QLatin1String("Shutter SW with level control") || //Bticino shutter small size
        sensor->modelId() == QLatin1String("Dimmer switch w/o neutral") || //Legrand dimmer wired
        sensor->modelId() == QLatin1String("Cable outlet") || //Legrand Cable outlet
        sensor->modelId() == QLatin1String("Remote switch") || //Legrand wireless switch
        sensor->modelId() == QLatin1String("Double gangs remote switch") || //Legrand wireless double switch
        sensor->modelId() == QLatin1String("Shutters central remote switch") || //Legrand wireless shutter switch
        sensor->modelId() == QLatin1String("DIN power consumption module") || //Legrand DIN power consumption module
        sensor->modelId() == QLatin1String("Remote motion sensor") || //Legrand Motion detector
        sensor->modelId() == QLatin1String("Remote toggle switch") || //Legrand switch module
        sensor->modelId() == QLatin1String("Teleruptor") || //Legrand teleruptor
        sensor->modelId() == QLatin1String("Contactor") || //Legrand Contactor
        sensor->modelId() == QLatin1String("Pocket remote") || //Legrand wireless remote 4 scene
        // Adeo
        sensor->modelId() == QLatin1String("LDSENK10") || // ADEO Animal compatible motion sensor (Leroy Merlin)
        // Philio
        sensor->modelId() == QLatin1String("PST03A-v2.2.5") || //Philio pst03-a
        // ORVIBO
        sensor->modelId() == QLatin1String("c3442b4ac59b4ba1a83119d938f283ab") ||
        sensor->modelId().startsWith(QLatin1String("SN10ZW")) ||
        sensor->modelId().startsWith(QLatin1String("SF2")) ||
        sensor->modelId() == QLatin1String("e70f96b3773a4c9283c6862dbafb6a99") ||
        // Netvox
        sensor->modelId().startsWith(QLatin1String("Z809A")) ||
        // Samsung SmartPlug 2019
        sensor->modelId().startsWith(QLatin1String("ZB-ONOFFPlug-D0005")) ||
        // Aurora
        sensor->modelId().startsWith(QLatin1String("DoubleSocket50AU")) ||
        // Ecolink
        sensor->modelId().startsWith(QLatin1String("4655BC0")) ||
        // Bosch
        sensor->modelId().startsWith(QLatin1String("ISW-ZDL1-WP11G")) ||
        sensor->modelId().startsWith(QLatin1String("ISW-ZPR1-WP13")) ||
        sensor->modelId().startsWith(QLatin1String("RFDL-ZB-MS")) ||
        (sensor->node()->nodeDescriptor().manufacturerCode() == VENDOR_BOSCH2 && sensor->modelId() == QLatin1String("AIR")) ||
        // Salus
        sensor->modelId().contains(QLatin1String("SP600")) ||
        sensor->modelId().contains(QLatin1String("SPE600")) ||
        // Zen
        sensor->modelId().contains(QLatin1String("Zen-01")) ||
        // eCozy
        sensor->modelId() == QLatin1String("Thermostat") ||
        // Alcantara
        sensor->modelId() == QLatin1String("ALCANTARA2 D1.00P1.01Z1.00") ||
        // Stelpro
        sensor->modelId().contains(QLatin1String("ST218")) ||
        sensor->modelId().contains(QLatin1String("STZB402")) ||
        sensor->modelId() == QLatin1String("SORB") ||
        // Tuya
        sensor->modelId().startsWith(QLatin1String("TS01")) ||
        sensor->modelId().startsWith(QLatin1String("TS02")) ||
        sensor->modelId().startsWith(QLatin1String("TS03")) ||
        sensor->modelId().startsWith(QLatin1String("TS0202")) || // motion sensor, manu = _TYZB01_zwvaj5wy
        sensor->modelId().startsWith(QLatin1String("TS0043")) || // to test
        sensor->modelId().startsWith(QLatin1String("TS0041")) ||
        sensor->modelId().startsWith(QLatin1String("TS0044")) ||
        sensor->modelId().startsWith(QLatin1String("TS0203")) ||
        sensor->modelId().startsWith(QLatin1String("TS0222")) || // TYZB01 light sensor
        sensor->modelId().startsWith(QLatin1String("TS004F")) || // 4 Gang Tuya ZigBee Wireless 12 Scene Switch
        sensor->modelId().startsWith(QLatin1String("TS011F")) || // Plugs
        // Tuyatec
        sensor->modelId().startsWith(QLatin1String("RH3040")) ||
        sensor->modelId().startsWith(QLatin1String("RH3001")) ||
        sensor->modelId().startsWith(QLatin1String("RH3052")) ||
        // Xiaomi
        sensor->modelId().startsWith(QLatin1String("lumi.switch.b1naus01")) ||
        sensor->modelId() == QLatin1String("lumi.motion.agl04") ||
        sensor->modelId() == QLatin1String("lumi.flood.agl02") ||
        sensor->modelId() == QLatin1String("lumi.switch.n0agl1") ||
        // iris
        sensor->modelId().startsWith(QLatin1String("1116-S")) ||
        sensor->modelId().startsWith(QLatin1String("1117-S")) ||
        // ELKO
        sensor->modelId().startsWith(QLatin1String("Super TR")) ||
        sensor->modelId().startsWith(QLatin1String("ElkoDimmer")) ||
        // Hive
        sensor->modelId() == QLatin1String("MOT003") ||
        sensor->modelId() == QLatin1String("DWS003") ||
        //Computime
        sensor->modelId() == QLatin1String("SLP2") ||
        sensor->modelId() == QLatin1String("SLP2b") ||
        sensor->modelId() == QLatin1String("SLR2") ||
        sensor->modelId() == QLatin1String("SLR2b") ||
        sensor->modelId() == QLatin1String("SLR1b") ||
        sensor->modelId() == QLatin1String("SLT2") ||
        sensor->modelId() == QLatin1String("SLT3") ||
        // Sengled
        sensor->modelId().startsWith(QLatin1String("E13-")) ||
        sensor->modelId().startsWith(QLatin1String("E1D-")) ||
        sensor->modelId().startsWith(QLatin1String("E1E-")) ||
        sensor->modelId().startsWith(QLatin1String("Z01-A19")) ||
        // Linkind
        sensor->modelId() == QLatin1String("ZB-MotionSensor-D0003") ||
        sensor->modelId() == QLatin1String("ZB-DoorSensor-D0003") ||
        // LK Wiser
        sensor->modelId() == QLatin1String("CCT591011_AS") ||
        sensor->modelId() == QLatin1String("CCT592011_AS") ||
        sensor->modelId() == QLatin1String("CCT593011_AS") ||
        sensor->modelId() == QLatin1String("CCT595011_AS") ||
        // Immax
        sensor->modelId() == QLatin1String("4in1-Sensor-ZB3.0") ||
        sensor->modelId() == QLatin1String("DoorWindow-Sensor-ZB3.0") ||
        // Casa.IA
        sensor->modelId().startsWith(QLatin1String("CTHS317ET")) ||
        // Sercomm
        sensor->modelId().startsWith(QLatin1String("SZ-")) ||
        sensor->modelId().startsWith(QLatin1String("GZ-")) ||
        sensor->modelId() == QLatin1String("Tripper") ||
        // WAXMAN
        sensor->modelId() == QLatin1String("leakSMART Water Sensor V2") ||
        // GamaBit
        sensor->modelId() == QLatin1String("GMB-HAS-WL-B01") ||
        sensor->modelId() == QLatin1String("GMB-HAS-DW-B01") ||
        sensor->modelId() == QLatin1String("GMB-HAS-VB-B01") ||
        // RGBgenie
        sensor->modelId().startsWith(QLatin1String("RGBgenie ZB-5")) ||
        sensor->modelId().startsWith(QLatin1String("ZGRC-KEY")) ||
        sensor->modelId().startsWith(QLatin1String("ZG2833PAC")) || // Sunricher C4
        // Embertec
        sensor->modelId().startsWith(QLatin1String("BQZ10-AU")) ||
        // ROBB Smarrt
        sensor->modelId().startsWith(QLatin1String("ROB_200")) ||
        // Sunricher
        sensor->modelId().startsWith(QLatin1String("Micro Smart Dimmer")) ||
        sensor->modelId() == QLatin1String("4512705") || // Namron remote control
        sensor->modelId() == QLatin1String("4512726") || // Namron rotary switch
        sensor->modelId().startsWith(QLatin1String("ZG2835")) ||
        // RT-RK
        sensor->modelId().startsWith(QLatin1String("SPW35Z")) ||
        // SLC
        sensor->modelId().startsWith(QLatin1String("S57003")) ||
        // Plugwise
        sensor->modelId().startsWith(QLatin1String("160-01")) ||
        // Feibit
        sensor->modelId().startsWith(QLatin1String("FNB56-")) ||
        sensor->modelId().startsWith(QLatin1String("FB56-")) ||
        // Niko
        sensor->modelId() == QLatin1String("Smart plug Zigbee PE") ||
        // Sage
        sensor->modelId() == QLatin1String("Bell") ||
        // Owon
        sensor->modelId() == QLatin1String("PR412C") ||
        // D-Link
        sensor->modelId() == QLatin1String("DCH-B112") ||
        sensor->modelId() == QLatin1String("DCH-B122") ||
        // Sonoff
        sensor->modelId() == QLatin1String("WB01") ||
        sensor->modelId() == QLatin1String("WB-01") ||
        sensor->modelId() == QLatin1String("MS01") ||
        sensor->modelId() == QLatin1String("MSO1") ||
        sensor->modelId() == QLatin1String("ms01") ||
        sensor->modelId() == QLatin1String("66666") ||
        sensor->modelId() == QLatin1String("TH01") ||
        sensor->modelId() == QLatin1String("DS01") ||
        // Danfoss
        sensor->modelId() == QLatin1String("0x8020") ||
        sensor->modelId() == QLatin1String("0x8021") ||
        sensor->modelId() == QLatin1String("0x8030") ||
        sensor->modelId() == QLatin1String("0x8031") ||
        sensor->modelId() == QLatin1String("0x8034") ||
        sensor->modelId() == QLatin1String("0x8035") ||
        // Swann
        sensor->modelId() == QLatin1String("SWO-MOS1PA") ||
        // LIDL
        sensor->modelId() == QLatin1String("HG06323") ||
        // Xfinity
        sensor->modelId() == QLatin1String("URC4450BC0-X-R") ||
        // Iris
        sensor->modelId() == QLatin1String("3405-L") ||
        // Eria
        sensor->modelId() == QLatin1String("Adurolight_NCC")
        )
    {
        deviceSupported = true;
        if (!sensor->node()->nodeDescriptor().receiverOnWhenIdle() ||
            (sensor->node()->nodeDescriptor().manufacturerCode() != VENDOR_DDEL))
        {
            sensor->setMgmtBindSupported(false);
        }
    }

    if (!deviceSupported)
    {
        DBG_Printf(DBG_INFO_L2, "don't create binding for attribute reporting of sensor %s\n", qPrintable(sensor->name()));
        return false;
    }

    // prevent binding action if otau was busy recently
    if (otauLastBusyTimeDelta() < OTA_LOW_PRIORITY_TIME)
    {
        if (sensor->modelId().startsWith(QLatin1String("FLS-")))
        {
            DBG_Printf(DBG_INFO_L2, "don't check binding for attribute reporting of %s (otau busy)\n", qPrintable(sensor->name()));
            return false;
        }
    }

    BindingTask::Action action = BindingTask::ActionUnbind;

    // whitelist by Model ID
    if (gwReportingEnabled)
    {
        if (deviceSupported)
        {
            action = BindingTask::ActionBind;
        }
    }

    if (sensor->modelId().startsWith(QLatin1String("Lightify Switch Mini")) ||  // Osram 3 button remote
        sensor->modelId().startsWith(QLatin1String("Switch 4x EU-LIGHTIFY")) || // Osram 4 button remote
        sensor->modelId().startsWith(QLatin1String("Switch 4x-LIGHTIFY")) || // Osram 4 button remote
        sensor->modelId().startsWith(QLatin1String("Switch-LIGHTIFY")) ) // Osram 4 button remote
    {
        // Make bind only for endpoint 01
        if (sensor->fingerPrint().endpoint != 0x01)
        {
            return false;
        }
    }

    bool ret = false;
    bool checkBindingTable = false;
    QDateTime now = QDateTime::currentDateTime();

    // sort server clusters so that 'more important' clusters will be bound as soon as possible
    // 0xfc00, 0x0500, 0x0406, 0x0402, 0x0400, 0x0001

    // for example for Philips motion sensor after joining the occupancy cluster 0x0406 is more
    // important than power configuration cluster 0x0001 and should be bound first

    // for the Philips dimmer switch the 0xfc00 for button events is also the most important
    std::vector<quint16> inClusters = sensor->fingerPrint().inClusters;
    std::sort(sensor->fingerPrint().inClusters.begin(), sensor->fingerPrint().inClusters.end(),
              [](quint16 a, quint16 b) { return a < b; });

    std::vector<quint16>::const_iterator i = inClusters.begin();
    std::vector<quint16>::const_iterator end = inClusters.end();

    for (; i != end; ++i)
    {
        NodeValue val;

        if (*i == ILLUMINANCE_MEASUREMENT_CLUSTER_ID)
        {
            if (sensor->node()->nodeDescriptor().manufacturerCode() == VENDOR_BOSCH2 && sensor->modelId() == QLatin1String("AIR"))
            {
                continue; // use BOSCH_AIR_QUALITY_CLUSTER_ID instead
            }
            val = sensor->getZclValue(*i, 0x0000); // measured value
        }
        else if (*i == TEMPERATURE_MEASUREMENT_CLUSTER_ID)
        {
            val = sensor->getZclValue(*i, 0x0000); // measured value
        }
        else if (*i == RELATIVE_HUMIDITY_CLUSTER_ID)
        {
            if (sensor->node()->nodeDescriptor().manufacturerCode() == VENDOR_BOSCH2 && sensor->modelId() == QLatin1String("AIR"))
            {
                continue; // use BOSCH_AIR_QUALITY_CLUSTER_ID instead
            }
            val = sensor->getZclValue(*i, 0x0000); // measured value
        }
        else if (*i == PRESSURE_MEASUREMENT_CLUSTER_ID)
        {
            if (sensor->node()->nodeDescriptor().manufacturerCode() == VENDOR_BOSCH2 && sensor->modelId() == QLatin1String("AIR"))
            {
                continue; // use BOSCH_AIR_QUALITY_CLUSTER_ID instead
            }
            val = sensor->getZclValue(*i, 0x0000); // measured value
        }
        else if (*i == SOIL_MOISTURE_CLUSTER_ID)
        {
            val = sensor->getZclValue(*i, 0x0000); // measured value
        }
        else if (*i == OCCUPANCY_SENSING_CLUSTER_ID)
        {
            val = sensor->getZclValue(*i, 0x0000); // occupied state
        }
        else if (*i == POWER_CONFIGURATION_CLUSTER_ID)
        {
            if (sensor->manufacturer() == QLatin1String("Samjin") && sensor->modelId() == QLatin1String("multi") && sensor->type() != QLatin1String("ZHAOpenClose"))
            {
                continue; // process only once
            }

            if (sensor->modelId() == QLatin1String("Remote switch") ||
                sensor->modelId() == QLatin1String("Shutters central remote switch") ||
                sensor->modelId() == QLatin1String("Double gangs remote switch") ||
                sensor->modelId() == QLatin1String("Pocket remote") ||
                sensor->modelId() == QLatin1String("Remote toggle switch") )
            {
                //Those device don't support report attribute
                continue;
            }
            if (sensor->manufacturer().startsWith(QLatin1String("Climax")) ||
                sensor->modelId().startsWith(QLatin1String("902010/23")))
            {
                val = sensor->getZclValue(*i, 0x0035); // battery alarm mask
            }
            else if (sensor->modelId().startsWith(QLatin1String("MOSZB-1")) ||
                     sensor->modelId().startsWith(QLatin1String("FLSZB-1")) ||
                     sensor->modelId() == QLatin1String("MotionSensor51AU") ||
                     sensor->modelId() == QLatin1String("Zen-01") ||
                     sensor->modelId() == QLatin1String("ISW-ZPR1-WP13") ||
                     sensor->modelId().startsWith(QLatin1String("Lightify Switch Mini")) ||  // Osram 3 button remote
                     sensor->modelId().startsWith(QLatin1String("Switch 4x EU-LIGHTIFY")) || // Osram 4 button remote
                     sensor->modelId().startsWith(QLatin1String("Switch 4x-LIGHTIFY")) || // Osram 4 button remote
                     sensor->modelId().startsWith(QLatin1String("Switch-LIGHTIFY")) || // Osram 4 button remote
                     sensor->modelId().startsWith(QLatin1String("moisturev4")) || // SmartThings water leak sensor
                     sensor->modelId() == QLatin1String("Remote switch") ||
                     sensor->modelId() == QLatin1String("Shutters central remote switch") ||
                     sensor->modelId() == QLatin1String("Double gangs remote switch") ||
                     sensor->modelId() == QLatin1String("Pocket remote") ||
                     sensor->modelId().startsWith(QLatin1String("ZHMS101")) ||
                     sensor->modelId().startsWith(QLatin1String("3AFE14010402000D")) || //konke presence sensor
                     sensor->modelId().startsWith(QLatin1String("3AFE28010402000D")) || //konke presence sensor
                     sensor->modelId().startsWith(QLatin1String("TS0202")) || //Tuya presence sensor
                     sensor->modelId().endsWith(QLatin1String("86opcn01")) || // Aqara Opple
                     sensor->modelId() == QLatin1String("lumi.motion.agl04") || // Xiaomi Aqara RTCGQ13LM high precision motion sensor
                     sensor->modelId().startsWith(QLatin1String("1116-S")) ||
                     sensor->modelId().startsWith(QLatin1String("1117-S")) ||
                     sensor->modelId().startsWith(QLatin1String("3323")) ||
                     sensor->modelId().startsWith(QLatin1String("3326-L")) ||
                     sensor->modelId().startsWith(QLatin1String("3305-S")) ||
                     sensor->modelId().startsWith(QLatin1String("3157100")) ||
                     sensor->modelId().startsWith(QLatin1String("4655BC0")) ||
                     sensor->modelId() == QLatin1String("URC4450BC0-X-R") || // Xfinity Keypad XHK1-UE
                     sensor->modelId() == QLatin1String("3405-L") || // IRIS 3405-L Keypad
                     sensor->modelId() == QLatin1String("113D"))
            {
                val = sensor->getZclValue(*i, 0x0020); // battery voltage
            }
            else
            {
                val = sensor->getZclValue(*i, 0x0021); // battery percentage remaining
            }

            if (val.timestampLastConfigured.isValid() && val.timestampLastConfigured.secsTo(now) < (val.maxInterval * 1.5))
            {
                continue;
            }

            // assume reporting is working
            if (val.isValid() && val.timestamp.isValid() && val.timestamp.secsTo(now) < (val.maxInterval * 3 / 2))
            {
                continue;
            }
        }
        else if (*i == IAS_ZONE_CLUSTER_ID)
        {
            val = sensor->getZclValue(*i, IAS_ZONE_CLUSTER_ATTR_ZONE_STATUS_ID); // zone status

            if (sensor->manufacturer() == QLatin1String("CentraLite"))
            {
                const ResourceItem *item = sensor->item(RConfigDuration);
                // update max reporting interval according to config.duration
                if (item && item->toNumber() > 15 && item->toNumber() <= UINT16_MAX)
                {
                    val.maxInterval = static_cast<quint16>(item->toNumber());
                    val.maxInterval -= 5; // report before going presence: false
                }
            }
        }
        else if (*i == BOSCH_AIR_QUALITY_CLUSTER_ID && sensor->modelId() == QLatin1String("AIR"))
        {
            if (sensor->type() != QLatin1String("ZHAAirQuality"))
            {
                continue; // only bind once
            }
            val = sensor->getZclValue(*i, 0x4004, 0x02); // air quality
        }
        else if (*i == METERING_CLUSTER_ID)
        {
            val = sensor->getZclValue(*i, 0x0000); // Curent Summation Delivered

        }
        else if (*i == ELECTRICAL_MEASUREMENT_CLUSTER_ID)
        {
            val = sensor->getZclValue(*i, 0x050b); // Active power
        }
        else if (*i == BINARY_INPUT_CLUSTER_ID)
        {
            val = sensor->getZclValue(*i, 0x0055); // Present value
        }
        else if (*i == THERMOSTAT_CLUSTER_ID)
        {
            val = sensor->getZclValue(*i, 0x0000); // Local temperature
        }
        else if (*i == FAN_CONTROL_CLUSTER_ID)
        {
            val = sensor->getZclValue(*i, 0x0000); // Fan mode
        }
        else if (*i == THERMOSTAT_UI_CONFIGURATION_CLUSTER_ID)
        {
            val = sensor->getZclValue(*i, 0x0001); // Keypad lockout
        }
        else if (*i == SAMJIN_CLUSTER_ID)
        {
            val = sensor->getZclValue(*i, 0x0012); // Acceleration X
        }

        quint16 maxInterval = val.maxInterval > 0 && val.maxInterval < 65535 ? (val.maxInterval * 3 / 2) : (60 * 15);

        if (!DEV_TestManaged())
        {
            if (val.timestampLastReport.isValid() && val.timestampLastReport.secsTo(now) < maxInterval) // got update in timely manner
            {
                DBG_Printf(DBG_INFO_L2, "binding for attribute reporting of ep: 0x%02X cluster 0x%04X seems to be active\n", val.endpoint, *i);
                continue;
            }

            if (!sensor->node()->nodeDescriptor().receiverOnWhenIdle() && sensor->lastRx().secsTo(now) > 6)
            {
                DBG_Printf(DBG_INFO, "skip binding for attribute reporting of ep: 0x%02X cluster 0x%04X (end-device might sleep)\n", val.endpoint, *i);
                return false;
            }
        }

        quint8 srcEndpoint = sensor->fingerPrint().endpoint;

        {  // some clusters might not be on fingerprint endpoint (power configuration), search in other simple descriptors
            deCONZ::SimpleDescriptor *sd = sensor->node()->getSimpleDescriptor(srcEndpoint);
            if (!sd || !sd->cluster(*i, deCONZ::ServerCluster))
            {
                for (auto &sd2 : sensor->node()->simpleDescriptors())
                {
                    if (sd2.cluster(*i, deCONZ::ServerCluster))
                    {
                        srcEndpoint = sd2.endpoint();
                        break;
                    }
                }
            }
        }

        switch (*i)
        {
        case POWER_CONFIGURATION_CLUSTER_ID:
        case OCCUPANCY_SENSING_CLUSTER_ID:
        case IAS_ZONE_CLUSTER_ID:
        case ILLUMINANCE_MEASUREMENT_CLUSTER_ID:
        case TEMPERATURE_MEASUREMENT_CLUSTER_ID:
        case RELATIVE_HUMIDITY_CLUSTER_ID:
        case PRESSURE_MEASUREMENT_CLUSTER_ID:
        case SOIL_MOISTURE_CLUSTER_ID:
        case METERING_CLUSTER_ID:
        case ELECTRICAL_MEASUREMENT_CLUSTER_ID:
        case VENDOR_CLUSTER_ID:
        case BASIC_CLUSTER_ID:
        case BINARY_INPUT_CLUSTER_ID:
        case THERMOSTAT_CLUSTER_ID:
        case FAN_CONTROL_CLUSTER_ID:
        case THERMOSTAT_UI_CONFIGURATION_CLUSTER_ID:
        case DIAGNOSTICS_CLUSTER_ID:
        case APPLIANCE_EVENTS_AND_ALERTS_CLUSTER_ID:
        case SAMJIN_CLUSTER_ID:
        case DOOR_LOCK_CLUSTER_ID:
        case BOSCH_AIR_QUALITY_CLUSTER_ID:
        {
            // For the moment reserved to doorlock device
            if (*i == DOOR_LOCK_CLUSTER_ID && sensor->type() != QLatin1String("ZHADoorLock"))
            {
                break;
            }

            DBG_Printf(DBG_INFO_L2, "0x%016llX (%s) create binding for attribute reporting of cluster 0x%04X on endpoint 0x%02X\n",
                       sensor->address().ext(), qPrintable(sensor->modelId()), (*i), srcEndpoint);

            BindingTask bindingTask;

            if (sensor->mgmtBindSupported())
            {
                bindingTask.state = BindingTask::StateCheck;
                checkBindingTable = true;
            }
            else
            {
                bindingTask.state = BindingTask::StateIdle;
            }

            if (DEV_TestManaged())
            {
                bindingTask.state = BindingTask::StateFinished; // don't actually send anything
            }

            bindingTask.action = action;
            bindingTask.restNode = sensor;
            bindingTask.timeout = BindingTask::TimeoutEndDevice;
            Binding &bnd = bindingTask.binding;
            bnd.srcAddress = sensor->address().ext();
            bnd.dstAddrMode = deCONZ::ApsExtAddress;
            bnd.srcEndpoint = srcEndpoint;
            bnd.clusterId = *i;
            bnd.dstAddress.ext = apsCtrl->getParameter(deCONZ::ParamMacAddress);
            bnd.dstEndpoint = endpoint();

            if (bnd.dstEndpoint > 0) // valid gateway endpoint?
            {
                ret = queueBindingTask(bindingTask);
            }

            if (*i == IAS_ZONE_CLUSTER_ID)
            {
                checkIasEnrollmentStatus(sensor);
            }
        }
            break;

        default:
            break;
        }
    }

    if (checkBindingTable)
    {
        sensor->enableRead(READ_BINDING_TABLE);
        sensor->setNextReadTime(READ_BINDING_TABLE, queryTime);
        queryTime = queryTime.addSecs(5);
        Q_Q(DeRestPlugin);
        q->startZclAttributeTimer(1000);
    }

    if (ret)
    {
        // fast iteration
        bindingTimer->start(0);
    }
    else if (!bindingTimer->isActive())
    {
        bindingTimer->start(1000);
    }

    return ret;
}

/*! Creates binding for group control (switches, motion sensor, ...). */
bool DeRestPluginPrivate::checkSensorBindingsForClientClusters(Sensor *sensor)
{
    if (!apsCtrl || !sensor || !sensor->node() || !sensor->address().hasExt() || !sensor->toBool(RConfigReachable))
    {
        return false;
    }

    if (searchSensorsState != SearchSensorsActive &&
        idleTotalCounter < (IDLE_READ_LIMIT + (60 * 15))) // wait for some input before fire bindings
    {
        return false;
    }

    Device *device = DEV_GetDevice(m_devices, sensor->address().ext());
    if (device && device->managed())
    {
        return false;
    }

    Q_Q(DeRestPlugin);
    QDateTime now = QDateTime::currentDateTime();
    if (!sensor->node()->nodeDescriptor().receiverOnWhenIdle() && sensor->lastRx().secsTo(now) > 10)
    {
        DBG_Printf(DBG_INFO_L2, "skip check bindings for client clusters (end-device might sleep)\n");
        return false;
    }

    ResourceItem *item = sensor->item(RConfigGroup);

    if (!item || item->toString().isEmpty())
    {
        DBG_Printf(DBG_INFO_L2, "skip check bindings for client clusters (no group)\n");
        return false;
    }

    std::vector<quint8> srcEndpoints;
    QStringList gids = item->toString().split(',', SKIP_EMPTY_PARTS);

    //quint8 srcEndpoint = sensor->fingerPrint().endpoint;
    std::vector<quint16> clusters;

    if (sensor->modelId().startsWith(QLatin1String("ElkoDimmer")) || // Elko dimmer
        sensor->modelId().startsWith(QLatin1String("E1E-"))) // Sengled smart light switch
    {
        srcEndpoints.push_back(0x01);
        clusters.push_back(ONOFF_CLUSTER_ID);
        clusters.push_back(LEVEL_CLUSTER_ID);
    }
    // Busch-Jaeger
    else if (sensor->modelId() == QLatin1String("RB01") ||
             sensor->modelId() == QLatin1String("RM01"))
    {
        quint8 firstEp = 0x0A;

        // the model RM01 might have an relais or dimmer switch on endpoint 0x12
        // in that case the endpoint 0x0A has no function
        if (getLightNodeForAddress(sensor->address(), 0x12))
        {
            firstEp = 0x0B;
        }

        if (sensor->fingerPrint().endpoint == firstEp)
        {
            clusters.push_back(LEVEL_CLUSTER_ID);
        }
        else if (sensor->fingerPrint().endpoint > firstEp)
        {
            clusters.push_back(SCENE_CLUSTER_ID);
        }
        srcEndpoints.push_back(sensor->fingerPrint().endpoint);
        sensor->setMgmtBindSupported(false);
    }
    // GE on/off switch 45856GE
    else if (sensor->manufacturer() == QLatin1String("Jasco Products") && sensor->modelId() == QLatin1String("45856"))
    {
        clusters.push_back(ONOFF_CLUSTER_ID);
        srcEndpoints.push_back(sensor->fingerPrint().endpoint);
    }
    // IKEA Trådfri dimmer
    else if (sensor->modelId() == QLatin1String("TRADFRI wireless dimmer"))
    {
        clusters.push_back(LEVEL_CLUSTER_ID);
        srcEndpoints.push_back(sensor->fingerPrint().endpoint);
    }
    // IKEA Trådfri remote
    else if (sensor->modelId().startsWith(QLatin1String("TRADFRI remote")))
    {
        clusters.push_back(ONOFF_CLUSTER_ID);
        clusters.push_back(LEVEL_CLUSTER_ID);
        clusters.push_back(SCENE_CLUSTER_ID);
        srcEndpoints.push_back(sensor->fingerPrint().endpoint);
    }
    // IKEA
    else if (sensor->modelId().startsWith(QLatin1String("TRADFRI on/off switch")) ||
             sensor->modelId().startsWith(QLatin1String("TRADFRI SHORTCUT Button")))
    {
        clusters.push_back(ONOFF_CLUSTER_ID);
        srcEndpoints.push_back(sensor->fingerPrint().endpoint);
    }
    // IKEA Trådfri open/close remote
    else if (sensor->modelId().startsWith(QLatin1String("TRADFRI open/close remote")))
    {
        clusters.push_back(WINDOW_COVERING_CLUSTER_ID);
        srcEndpoints.push_back(sensor->fingerPrint().endpoint);
    }
    // IKEA Trådfri motion sensor
    else if (sensor->modelId().startsWith(QLatin1String("TRADFRI motion sensor")))
    {
        clusters.push_back(ONOFF_CLUSTER_ID);
        clusters.push_back(LEVEL_CLUSTER_ID);
        srcEndpoints.push_back(sensor->fingerPrint().endpoint);
    }
    else if (sensor->modelId().startsWith(QLatin1String("WB01")) ||
             sensor->modelId().startsWith(QLatin1String("WB-01")))
    {
        clusters.push_back(ONOFF_CLUSTER_ID);
        srcEndpoints.push_back(sensor->fingerPrint().endpoint);
    }
    // OSRAM 3 button remote
    else if (sensor->modelId().startsWith(QLatin1String("Lightify Switch Mini")) )
    {
        clusters.push_back(ONOFF_CLUSTER_ID);
        clusters.push_back(LEVEL_CLUSTER_ID);
        clusters.push_back(COLOR_CLUSTER_ID);

        // We bind all endpoints to a single group, so we need to trick the for loop by
        // creating dummy group entries that point to the first group so all endpoints are bound properly.
        QString gid0 = gids[0];
        gids.append(gid0);
        gids.append(gid0);

        srcEndpoints.push_back(0x01);
        srcEndpoints.push_back(0x02);
        srcEndpoints.push_back(0x03);
    }
    // OSRAM 4 button remote
    else if (sensor->modelId().startsWith(QLatin1String("Switch 4x EU-LIGHTIFY")) ||
             sensor->modelId().startsWith(QLatin1String("Switch 4x-LIGHTIFY")) ||
             sensor->modelId().startsWith(QLatin1String("Switch-LIGHTIFY")) )
    {
        clusters.push_back(ONOFF_CLUSTER_ID);
        clusters.push_back(LEVEL_CLUSTER_ID);
        clusters.push_back(COLOR_CLUSTER_ID);

        // We bind all endpoints to a single group, so we need to trick the for loop by
        // creating dummy group entries that point to the first group so all endpoints are bound properly.
        QString gid0 = gids[0];
        gids.append(gid0);
        gids.append(gid0);
        gids.append(gid0);

        srcEndpoints.push_back(0x01);
        srcEndpoints.push_back(0x02);
        srcEndpoints.push_back(0x03);
        srcEndpoints.push_back(0x04);
    }
    // LEGRAND Remote switch, simple and double
    else if (sensor->modelId() == QLatin1String("Remote switch") ||
             sensor->modelId() == QLatin1String("Double gangs remote switch"))
    {
        clusters.push_back(ONOFF_CLUSTER_ID);
        clusters.push_back(LEVEL_CLUSTER_ID);
        srcEndpoints.push_back(sensor->fingerPrint().endpoint);
    }
    // LEGRAND Remote switch 4 scene
    else if (sensor->modelId() == QLatin1String("Pocket remote"))
    {
        clusters.push_back(SCENE_CLUSTER_ID);
        srcEndpoints.push_back(sensor->fingerPrint().endpoint);
    }
    else if (sensor->modelId() == QLatin1String("ZBT-CCTSwitch-D0001"))
    {
        clusters.push_back(ONOFF_CLUSTER_ID);
        clusters.push_back(LEVEL_CLUSTER_ID);
        clusters.push_back(COLOR_CLUSTER_ID);
        srcEndpoints.push_back(sensor->fingerPrint().endpoint);
    }
    // LEGRAND Remote shutter switch
    else if (sensor->modelId() == QLatin1String("Shutters central remote switch"))
    {
        clusters.push_back(WINDOW_COVERING_CLUSTER_ID);
        srcEndpoints.push_back(sensor->fingerPrint().endpoint);
    }
    else if (sensor->modelId() == QLatin1String("Remote toggle switch") || // LEGRAND switch micro module
             sensor->modelId() == QLatin1String("Remote motion sensor"))  //Legrand motion sensor
    {
        clusters.push_back(ONOFF_CLUSTER_ID);
        srcEndpoints.push_back(sensor->fingerPrint().endpoint);
    }
    else if (sensor->modelId().startsWith(QLatin1String("RC 110")))
    {
        clusters.push_back(ONOFF_CLUSTER_ID);
        clusters.push_back(LEVEL_CLUSTER_ID);
        gids.removeFirst(); // Remote doesn't support bindings on first endpoint.
        srcEndpoints.push_back(0x03);
        srcEndpoints.push_back(0x04);
        srcEndpoints.push_back(0x05);
        srcEndpoints.push_back(0x06);
        srcEndpoints.push_back(0x07);
        srcEndpoints.push_back(0x08);
    }
    else if (sensor->modelId().startsWith(QLatin1String("ZGRC-TEUR-")))
    {
        clusters.push_back(ONOFF_CLUSTER_ID);
        clusters.push_back(LEVEL_CLUSTER_ID);
        clusters.push_back(SCENE_CLUSTER_ID);
        clusters.push_back(COLOR_CLUSTER_ID);
        srcEndpoints.push_back(sensor->fingerPrint().endpoint);
    }
    else if (sensor->modelId().startsWith(QLatin1String("ICZB-RM")) ||
             sensor->modelId().startsWith(QLatin1String("ZGR904-S")) ||
             sensor->modelId().startsWith(QLatin1String("ZGRC-KEY-013")) ||
             sensor->modelId().startsWith(QLatin1String("RGBgenie ZB-5001")))
    {
        clusters.push_back(ONOFF_CLUSTER_ID);
        clusters.push_back(LEVEL_CLUSTER_ID);
        clusters.push_back(SCENE_CLUSTER_ID);
        srcEndpoints.push_back(0x01);
        srcEndpoints.push_back(0x02);
        srcEndpoints.push_back(0x03);
        srcEndpoints.push_back(0x04);
    }
    else if (sensor->modelId().startsWith(QLatin1String("ZG2833PAC"))) // Sunricher C4
    {
        clusters.push_back(ONOFF_CLUSTER_ID);
        srcEndpoints.push_back(0x01);
        srcEndpoints.push_back(0x02);
        srcEndpoints.push_back(0x03);
        srcEndpoints.push_back(0x04);
    }
    else if (sensor->modelId() == QLatin1String("4512705") || // Namron remote control
             sensor->modelId() == QLatin1String("4512726") || // Namron rotary switch
             sensor->modelId().startsWith(QLatin1String("S57003")))
    {
        clusters.push_back(ONOFF_CLUSTER_ID);
        clusters.push_back(LEVEL_CLUSTER_ID);
        clusters.push_back(SCENE_CLUSTER_ID);
        srcEndpoints.push_back(0x01);
        srcEndpoints.push_back(0x02);
        srcEndpoints.push_back(0x03);
        srcEndpoints.push_back(0x04);
    }
    else if (sensor->modelId().startsWith(QLatin1String("D1")))
    {
        clusters.push_back(ONOFF_CLUSTER_ID);
        clusters.push_back(LEVEL_CLUSTER_ID);
        srcEndpoints.push_back(0x02);
        srcEndpoints.push_back(0x03);
        sensor->setMgmtBindSupported(true);
    }
    else if (sensor->modelId().startsWith(QLatin1String("S1-R")))
    {
        clusters.push_back(ONOFF_CLUSTER_ID);
        clusters.push_back(LEVEL_CLUSTER_ID);
        srcEndpoints.push_back(0x02);
        if (sensor->modelId().startsWith(QLatin1String("S1-R")))
        {
            srcEndpoints.push_back(0x03);
        }
        sensor->setMgmtBindSupported(true);
    }
    else if (sensor->modelId().startsWith(QLatin1String("S2-R")))
    {
        clusters.push_back(ONOFF_CLUSTER_ID);
        clusters.push_back(LEVEL_CLUSTER_ID);
        srcEndpoints.push_back(0x03);
        srcEndpoints.push_back(0x04);
        sensor->setMgmtBindSupported(true);
    }
    // Bitron remote control
    else if (sensor->modelId().startsWith(QLatin1String("902010/23")))
    {
        clusters.push_back(ONOFF_CLUSTER_ID);
        clusters.push_back(LEVEL_CLUSTER_ID);
        srcEndpoints.push_back(sensor->fingerPrint().endpoint);
    }
    // Heiman remote control
    else if (sensor->modelId().startsWith(QLatin1String("TS0215")) ||
             sensor->modelId().startsWith(QLatin1String("RC_V14")) ||
             sensor->modelId().startsWith(QLatin1String("RC-EM")) ||
             sensor->modelId() == QLatin1String("URC4450BC0-X-R") ||
             sensor->modelId() == QLatin1String("3405-L") ||
             sensor->modelId().startsWith(QLatin1String("RC-EF-3.0")))
    {
        clusters.push_back(IAS_ACE_CLUSTER_ID);
        srcEndpoints.push_back(sensor->fingerPrint().endpoint);
    }
    else if (sensor->modelId().startsWith(QLatin1String("RGBgenie ZB-5")) || // RGBgenie remote control
             sensor->manufacturer() == QLatin1String("_TZ3000_xabckq1v") || // 4 Gang Tuya ZigBee Wireless 12 Scene Switch
             sensor->modelId().startsWith(QLatin1String("ZBT-DIMController-D0800"))) // Mueller-Licht tint dimmer
    {
        clusters.push_back(ONOFF_CLUSTER_ID);
        clusters.push_back(LEVEL_CLUSTER_ID);
        clusters.push_back(SCENE_CLUSTER_ID);
        srcEndpoints.push_back(sensor->fingerPrint().endpoint);
    }
    // RGBgenie remote control
    else if (sensor->modelId().startsWith(QLatin1String("ZGRC-KEY-012")))
    {
        clusters.push_back(ONOFF_CLUSTER_ID);
        clusters.push_back(LEVEL_CLUSTER_ID);
        srcEndpoints.push_back(0x01);
        srcEndpoints.push_back(0x02);
        srcEndpoints.push_back(0x03);
        srcEndpoints.push_back(0x04);
        srcEndpoints.push_back(0x05);
    }
    // Sage doorbell sensor
    else if (sensor->modelId().startsWith(QLatin1String("Bell")))
    {
        clusters.push_back(ONOFF_CLUSTER_ID);
        clusters.push_back(LEVEL_CLUSTER_ID);
        srcEndpoints.push_back(sensor->fingerPrint().endpoint);
    }
    // Linkind 1 key Remote Control / ZS23000178
    // SR-ZG2835 Zigbee Rotary Switch
    else if (sensor->modelId().startsWith(QLatin1String("ZBT-DIMSwitch")) ||
             sensor->modelId().startsWith(QLatin1String("ZG2835")) ||
             sensor->modelId().startsWith(QLatin1String("Adurolight_NCC")))
    {
        clusters.push_back(ONOFF_CLUSTER_ID);
        clusters.push_back(LEVEL_CLUSTER_ID);
        srcEndpoints.push_back(sensor->fingerPrint().endpoint);
    }
    else
    {
        return false;
    }

    // prevent binding action if otau was busy recently
    if (otauLastBusyTimeDelta() < OTA_LOW_PRIORITY_TIME)
    {
        return false;
    }

    bool ret = false;
    for (int j = 0; j < (int)srcEndpoints.size() && j < gids.size(); j++)
    {
        quint8 srcEndpoint = srcEndpoints[j];
        Group *group = getGroupForId(gids[j]);

        if (!group)
        {
            continue;
        }

        std::vector<quint16>::const_iterator i = clusters.begin();
        std::vector<quint16>::const_iterator end = clusters.end();

        for (; i != end; ++i)
        {
            DBG_Printf(DBG_ZDP, "0x%016llX [%s] create binding for client cluster 0x%04X on endpoint 0x%02X\n",
                       sensor->address().ext(), qPrintable(sensor->modelId()), (*i), srcEndpoint);

            BindingTask bindingTask;

            bindingTask.state = BindingTask::StateIdle;
            bindingTask.action = BindingTask::ActionBind;
            bindingTask.timeout = BindingTask::TimeoutEndDevice;
            bindingTask.restNode = sensor;
            Binding &bnd = bindingTask.binding;
            bnd.srcAddress = sensor->address().ext();
            bnd.dstAddrMode = deCONZ::ApsGroupAddress;
            bnd.srcEndpoint = srcEndpoint;
            bnd.clusterId = *i;
            bnd.dstAddress.group = group->address();

            if (sensor->mgmtBindSupported())
            {
                bindingTask.state = BindingTask::StateCheck;
            }

            if (queueBindingTask(bindingTask))
            {
                ret = true;
            }

            // group addressing has no destination endpoint
//            bnd.dstEndpoint = endpoint();

//            if (bnd.dstEndpoint > 0) // valid gateway endpoint?
//            {
//                queueBindingTask(bindingTask);
//            }
        }
    }

    if (sensor->mgmtBindSupported())
    {
        if (!sensor->mustRead(READ_BINDING_TABLE))
        {
            sensor->enableRead(READ_BINDING_TABLE);
            sensor->setNextReadTime(READ_BINDING_TABLE, queryTime);
            queryTime = queryTime.addSecs(1);
        }
        q->startZclAttributeTimer(1000);
    }

    if (!bindingTimer->isActive())
    {
        bindingTimer->start();
    }

    return ret;
}

/*! Creates groups for \p sensor if needed. */
void DeRestPluginPrivate::checkSensorGroup(Sensor *sensor)
{
    if (!sensor)
    {
        return;
    }

    Device *device = DEV_GetDevice(m_devices, sensor->address().ext());
    if (device && device->managed())
    {
        return;
    }

    Group *group = nullptr;

    {
        std::vector<Group>::iterator i = groups.begin();
        std::vector<Group>::iterator end = groups.end();

        for (; i != end; ++i)
        {
            if (i->address() == 0)
            {
                continue;
            }

            if (i->state() == Group::StateNormal &&
                (i->deviceIsMember(sensor->uniqueId()) || i->deviceIsMember(sensor->id())))
            {
                group = &*i;
                break;
            }
        }
    }

    if (sensor->modelId().startsWith(QLatin1String("TRADFRI on/off switch")) ||
        sensor->modelId().startsWith(QLatin1String("TRADFRI SHORTCUT Button")) ||
        sensor->modelId().startsWith(QLatin1String("Remote Control N2")) || // STYRBAR
        sensor->modelId().startsWith(QLatin1String("TRADFRI open/close remote")) ||
        sensor->modelId().startsWith(QLatin1String("TRADFRI motion sensor")) ||
        sensor->modelId().startsWith(QLatin1String("TRADFRI wireless dimmer")) ||
        // sensor->modelId().startsWith(QLatin1String("SYMFONISK")) ||
        sensor->modelId().startsWith(QLatin1String("902010/23")) || // bitron remote
        sensor->modelId().startsWith(QLatin1String("Adurolight_NCC")) || // Eria Adurosmart Wireless Dimming Switch
        sensor->modelId().startsWith(QLatin1String("WB01")) || // Sonoff SNZB-01
        sensor->modelId().startsWith(QLatin1String("WB-01")) || // Sonoff SNZB-01
        sensor->modelId().startsWith(QLatin1String("Bell")) || // Sage doorbell sensor
        sensor->modelId().startsWith(QLatin1String("ZBT-CCTSwitch-D0001")) || //LDS Remote
        sensor->modelId().startsWith(QLatin1String("ZBT-DIMSwitch")) || // Linkind 1 key Remote Control / ZS23000178
        sensor->modelId().startsWith(QLatin1String("ZBT-DIMController-D0800")) || // Mueller-Licht tint dimmer
        sensor->modelId().startsWith(QLatin1String("ElkoDimmer")) || // Elko dimmer
        sensor->modelId().startsWith(QLatin1String("E1E-")) || // Sengled smart light switch
        sensor->modelId().startsWith(QLatin1String("ZG2835")) || // SR-ZG2835 Zigbee Rotary Switch
        sensor->modelId().startsWith(QLatin1String("RGBgenie ZB-5121"))) // RGBgenie ZB-5121 remote
    {

    }
    else if (sensor->modelId() == QLatin1String("Remote switch") ||
         sensor->modelId() == QLatin1String("Double gangs remote switch") ||
	     sensor->modelId() == QLatin1String("Shutters central remote switch") ||
         sensor->modelId() == QLatin1String("Remote toggle switch") ||
         sensor->modelId() == QLatin1String("Remote motion sensor"))
    {
        //Make group but without uniqueid
    }
    else if (sensor->modelId().startsWith(QLatin1String("Lightify Switch Mini")) ||  // Osram 3 button remote
             sensor->modelId().startsWith(QLatin1String("Switch 4x EU-LIGHTIFY")) || // Osram 4 button remote
             sensor->modelId().startsWith(QLatin1String("Switch 4x-LIGHTIFY")) || // Osram 4 button remote
             sensor->modelId().startsWith(QLatin1String("Switch-LIGHTIFY")) ) // Osram 4 button remote
    {
        quint8 maxEp = 0x03;
        if (sensor->modelId().startsWith(QLatin1String("Switch 4x EU-LIGHTIFY")) ||
            sensor->modelId().startsWith(QLatin1String("Switch 4x-LIGHTIFY")) ||
            sensor->modelId().startsWith(QLatin1String("Switch-LIGHTIFY")) )
        {
            maxEp = 0x04;
        }
        for (quint8 ep = 0x01; !group && ep <= maxEp; ep++)
        {
            Sensor *s = getSensorNodeForAddressAndEndpoint(sensor->address(), ep);
            if (s && s->deletedState() == Sensor::StateNormal && s != sensor)
            {
                ResourceItem *item = s->item(RConfigGroup);
                if (item && item->lastSet().isValid())
                {
                    const QString &gid = item->toString();

                    std::vector<Group>::iterator i = groups.begin();
                    std::vector<Group>::iterator end = groups.end();

                    for (; i != end; ++i)
                    {
                        if (!gid.isEmpty() && i->state() == Group::StateNormal && i->id() == gid)
                        {
                            group = &*i;
                            break;
                        }
                    }
                }
            }
        }
    }
    else if (sensor->modelId() == QLatin1String("RB01") ||
             sensor->modelId() == QLatin1String("RM01"))
    {
        // check if group is created for other endpoint
        for (quint8 ep = 0x0A; !group && ep < 0x0F; ep++)
        {
            Sensor *s = getSensorNodeForAddressAndEndpoint(sensor->address(), ep);
            if (s && s->deletedState() == Sensor::StateNormal && s != sensor)
            {
                ResourceItem *item = s->item(RConfigGroup);
                if (item && item->lastSet().isValid())
                {
                    const QString &gid = item->toString();

                    std::vector<Group>::iterator i = groups.begin();
                    std::vector<Group>::iterator end = groups.end();

                    for (; i != end; ++i)
                    {
                        if (!gid.isEmpty() && i->state() == Group::StateNormal && i->id() == gid)
                        {
                            group = &*i;
                            break;
                        }
                    }
                }
            }
        }
    }
    else
    {
        return;
    }

    ResourceItem *item = sensor->item(RConfigGroup);

    if (!item)
    {
        item = sensor->addItem(DataTypeString, RConfigGroup);
    }
    else if (!group && item->lastSet().isValid())
    {
        const QString &gid = item->toString(); //FIXME: handle list of groups

        std::vector<Group>::iterator i = groups.begin();
        std::vector<Group>::iterator end = groups.end();

        for (; i != end; ++i)
        {
            if (i->address() == 0)
            {
                continue;
            }

            if (!gid.isEmpty() && i->state() == Group::StateNormal && i->id() == gid)
            {
                group = &*i;
                break;
            }
        }
    }

    if (!group)
    {
        group = addGroup();
        group->setName(sensor->name());
        ResourceItem *item2 = group->addItem(DataTypeString, RAttrUniqueId);
        DBG_Assert(item2);
        if (item2)
        {
            const QString uid = generateUniqueId(sensor->address().ext(), 0, 0);
            item2->setValue(uid);
        }
    }

    DBG_Assert(group);
    if (!group)
    {
        return;
    }

    if (group->addDeviceMembership(sensor->id()))
    {

    }

    if (item->toString() != group->id()) // FIXME: handle list of groups
    {
        item->setValue(group->id()); // FIXME: handle list of groups
        sensor->setNeedSaveDatabase(true);
        queSaveDb(DB_SENSORS, DB_SHORT_SAVE_DELAY);
        Event e(RSensors, RConfigGroup, sensor->id(), item);
        enqueueEvent(e);
    }
}

/*! Checks if there are any orphan groups for \p sensor and removes them. */
void DeRestPluginPrivate::checkOldSensorGroups(Sensor *sensor)
{
    if (!sensor)
    {
        return;
    }

    ResourceItem *item = sensor->item(RConfigGroup);

    if (!item || !item->lastSet().isValid() || item->toString().isEmpty())
    {
        return;
    }

    QStringList gids = item->toString().split(',', SKIP_EMPTY_PARTS);

    {
        auto i = groups.begin();
        const auto end = groups.end();

        for (; i != end; ++i)
        {
            if (gids.contains(i->id())) // current group
            {
                if (i->state() != Group::StateNormal)
                {
                    DBG_Printf(DBG_INFO, "reanimate group %u for sensor %s\n", i->address(), qPrintable(sensor->name()));
                    i->setState(Group::StateNormal);
                    updateGroupEtag(&*i);
                    queSaveDb(DB_GROUPS, DB_SHORT_SAVE_DELAY);
                }
            }
            else if (i->deviceIsMember(sensor->uniqueId()) || i->deviceIsMember(sensor->id()))
            {
                i->removeDeviceMembership(sensor->uniqueId());
                i->removeDeviceMembership(sensor->id());

                if (!i->item(RAttrUniqueId) || i->item(RAttrUniqueId)->toString().isEmpty())
                {
                    continue; // don't remove ordinary groups
                }

                if (i->address() != 0 && i->state() == Group::StateNormal && !i->hasDeviceMembers())
                {
                    DBG_Printf(DBG_INFO, "delete old group %u of sensor %s\n", i->address(), qPrintable(sensor->name()));
                    i->setState(Group::StateDeleted);
                    updateGroupEtag(&*i);
                    queSaveDb(DB_GROUPS | DB_LIGHTS, DB_SHORT_SAVE_DELAY);

                    // for each node which is part of this group send a remove group request (will be unicast)
                    // note: nodes which are curently switched off will not be removed!
                    auto j = nodes.begin();
                    const auto jend = nodes.end();

                    for (; j != jend; ++j)
                    {
                        GroupInfo *groupInfo = getGroupInfo(&*j, i->address());

                        if (groupInfo)
                        {
                            j->setNeedSaveDatabase(true);
                            groupInfo->actions &= ~GroupInfo::ActionAddToGroup; // sanity
                            groupInfo->actions |= GroupInfo::ActionRemoveFromGroup;
                            groupInfo->state = GroupInfo::StateNotInGroup;
                        }
                    }
                }
            }
        }
    }
}

/*! Remove groups which are controlled by device \p id. */
void DeRestPluginPrivate::deleteGroupsWithDeviceMembership(const QString &id)
{
    auto i = groups.begin();
    const auto end = groups.end();
    for (; i != end; ++i)
    {
        if (i->deviceIsMember(id) && i->state() == Group::StateNormal)
        {
            i->removeDeviceMembership(id);

            updateGroupEtag(&*i);
            queSaveDb(DB_GROUPS | DB_LIGHTS, DB_SHORT_SAVE_DELAY);

            if (i->hasDeviceMembers())
            {
                continue;
            }

            if (!i->item(RAttrUniqueId) || i->item(RAttrUniqueId)->toString().isEmpty())
            {
                continue; // don't remove ordinary groups
            }

            i->setState(Group::StateDeleted);

            // for each node which is part of this group send a remove group request (will be unicast)
            // note: nodes which are curently switched off will not be removed!
            auto j = nodes.begin();
            const auto jend = nodes.end();

            for (; j != jend; ++j)
            {
                GroupInfo *groupInfo = getGroupInfo(&*j, i->address());

                if (groupInfo)
                {
                    j->setNeedSaveDatabase(true);
                    groupInfo->actions &= ~GroupInfo::ActionAddToGroup; // sanity
                    groupInfo->actions |= GroupInfo::ActionRemoveFromGroup;
                    groupInfo->state = GroupInfo::StateNotInGroup;
                }
            }
        }
    }
}

/*! Process binding related tasks queue every one second. */
void DeRestPluginPrivate::bindingTimerFired()
{
    if (bindingQueue.empty())
    {
        return;
    }

    Q_Q(DeRestPlugin);
    if (!q->pluginActive())
    {
        bindingQueue.clear();
        return;
    }

    int active = 0;
    std::list<BindingTask>::iterator i = bindingQueue.begin();
    std::list<BindingTask>::iterator end = bindingQueue.end();

    for (; i != end; ++i)
    {
        if (i->state == BindingTask::StateIdle)
        {
            if (active >= MAX_ACTIVE_BINDING_TASKS)
            { /* do nothing */ }
            else if (sendBindRequest(*i))
            {
                i->state = BindingTask::StateInProgress;
                break;
            }
            else if (i->retries < 5)
            {
                i->retries++;
            }
            else
            {
                // too harsh?
                DBG_Printf(DBG_INFO_L2, "failed to send bind/unbind request to 0x%016llX cluster 0x%04X. drop\n", i->binding.srcAddress, i->binding.clusterId);
                i->state = BindingTask::StateFinished;
            }
        }
        else if (i->state == BindingTask::StateInProgress)
        {
            i->timeout--;
            if (i->timeout < 0)
            {
                i->retries--;
                if (i->retries > 0)
                {
                    if (i->restNode && !i->restNode->isAvailable())
                    {
                        DBG_Printf(DBG_INFO_L2, "giveup binding srcAddr: 0x%016llX (not available)\n", i->binding.srcAddress);
                        i->state = BindingTask::StateFinished;
                    }
                    else
                    {
                        DBG_Printf(DBG_INFO_L2, "binding/unbinding timeout srcAddr: 0x%016llX, retry\n", i->binding.srcAddress);
                        i->state = BindingTask::StateIdle;
                        i->timeout = BindingTask::Timeout;
                        if (i->restNode && i->restNode->node() && !i->restNode->node()->nodeDescriptor().receiverOnWhenIdle())
                        {
                            i->timeout = BindingTask::TimeoutEndDevice;
                        }
                    }
                }
                else
                {
                    DBG_Printf(DBG_INFO_L2, "giveup binding srcAddr: 0x%016llX\n", i->binding.srcAddress);
                    i->state = BindingTask::StateFinished;
                }
            }
            else
            {
                active++;
            }
        }
        else if (i->state == BindingTask::StateFinished)
        {
            bindingQueue.erase(i);
            break;
        }
        else if (i->state == BindingTask::StateCheck)
        {
            i->timeout--;
            if (i->timeout < 0)
            {
                i->retries--;
                if (i->retries > 0 && i->restNode)
                {
                    if (i->restNode->mgmtBindSupported())
                    {
                        if (!i->restNode->mustRead(READ_BINDING_TABLE))
                        {
                            i->restNode->enableRead(READ_BINDING_TABLE);
                            i->restNode->setNextReadTime(READ_BINDING_TABLE, queryTime);
                            queryTime = queryTime.addSecs(5);
                        }
                        q->startZclAttributeTimer(1000);

                        i->state = BindingTask::StateCheck;
                    }
                    else
                    {
                        i->state = BindingTask::StateIdle;
                    }
                    i->timeout = BindingTask::Timeout;
                    if (i->restNode && i->restNode->node() && !i->restNode->node()->nodeDescriptor().receiverOnWhenIdle())
                    {
                        i->timeout = BindingTask::TimeoutEndDevice;
                    }

                    DBG_Printf(DBG_INFO_L2, "%s check timeout, retries = %d (srcAddr: 0x%016llX cluster: 0x%04X)\n",
                               (i->action == BindingTask::ActionBind ? "bind" : "unbind"), i->retries, i->binding.srcAddress, i->binding.clusterId);

                    bindingQueue.push_back(*i);
                    bindingQueue.pop_front();
                    break;
                }
                else
                {
                    DBG_Printf(DBG_INFO_L2, "giveup binding\n");
                    DBG_Printf(DBG_INFO_L2, "giveup %s (srcAddr: 0x%016llX cluster: 0x%04X)\n",
                               (i->action == BindingTask::ActionBind ? "bind" : "unbind"), i->binding.srcAddress, i->binding.clusterId);
                    i->state = BindingTask::StateFinished;
                }
            }
        }
    }

    if (!bindingQueue.empty())
    {
        bindingTimer->start(1000);
    }
}

/*! Process ongoing binding table queries.
*/
void DeRestPluginPrivate::bindingTableReaderTimerFired()
{
    std::vector<BindingTableReader>::iterator i = bindingTableReaders.begin();

    for (; i != bindingTableReaders.end(); )
    {
        if (i->state == BindingTableReader::StateIdle)
        {
            deCONZ::ApsDataRequest &apsReq = i->apsReq;

            i->apsReq.setDstAddressMode(deCONZ::ApsExtAddress);
            i->apsReq.setProfileId(ZDP_PROFILE_ID);
            i->apsReq.setClusterId(ZDP_MGMT_BIND_REQ_CLID);
            i->apsReq.setDstEndpoint(ZDO_ENDPOINT);
            i->apsReq.setSrcEndpoint(ZDO_ENDPOINT);
#if QT_VERSION < QT_VERSION_CHECK(5,15,0)
            i->apsReq.setTxOptions(0);
#endif
            i->apsReq.setRadius(0);

            QDataStream stream(&apsReq.asdu(), QIODevice::WriteOnly);
            stream.setByteOrder(QDataStream::LittleEndian);

            QTime now = QTime::currentTime();
            stream << (uint8_t)now.second(); // seqno
            stream << i->index;

            // send
            if (apsCtrlWrapper.apsdeDataRequest(apsReq) == deCONZ::Success)
            {
                DBG_Printf(DBG_ZDP, "Mgmt_Bind_req id: %d to 0x%016llX send\n", i->apsReq.id(), i->apsReq.dstAddress().ext());
                i->time.start();
                i->state = BindingTableReader::StateWaitConfirm;
                break;
            }
            else
            {
                DBG_Printf(DBG_ZDP, "failed to send Mgmt_Bind_req to 0x%016llX\n", i->apsReq.dstAddress().ext());
                i->state = BindingTableReader::StateFinished;
            }
        }
        else if (i->state == BindingTableReader::StateWaitConfirm)
        {
            if (i->time.elapsed() > BindingTableReader::MaxConfirmTime)
            {
                DBG_Printf(DBG_ZDP, "timeout for Mgmt_Bind_req id %d to 0x%016llX\n", i->apsReq.id(), i->apsReq.dstAddress().ext());
                i->state = BindingTableReader::StateFinished;
            }
        }
        else if (i->state == BindingTableReader::StateWaitResponse)
        {
            const int maxResponseTime = i->isEndDevice ? BindingTableReader::MaxEndDeviceResponseTime
                                                 : BindingTableReader::MaxResponseTime;
            if (i->time.elapsed() > maxResponseTime)
            {
                DBG_Printf(DBG_ZDP, "timeout for response to Mgmt_Bind_req id %d to 0x%016llX\n", i->apsReq.id(), i->apsReq.dstAddress().ext());
                i->state = BindingTableReader::StateFinished;
            }
        }

        if (i->state == BindingTableReader::StateFinished)
        {
            *i = bindingTableReaders.back();
            bindingTableReaders.pop_back();
        }
        else
        {
            ++i;
        }
    }

    if (!bindingTableReaders.empty())
    {
        bindingTableReaderTimer->start();
    }
}

/*! Add a binding task to the queue and prevent double entries.
    \param bindingTask - the binding task
    \return true - when enqueued
 */
bool DeRestPluginPrivate::queueBindingTask(const BindingTask &bindingTask)
{
    if (!apsCtrl || apsCtrl->networkState() != deCONZ::InNetwork)
    {
        return false;
    }

    const std::list<BindingTask>::const_iterator i = std::find(bindingQueue.begin(), bindingQueue.end(), bindingTask);

    if (i == bindingQueue.end())
    {
        DBG_Printf(DBG_INFO_L2, "queue binding task for 0x%016llX, cluster 0x%04X\n", bindingTask.binding.srcAddress, bindingTask.binding.clusterId);

        Device *device = DEV_GetDevice(m_devices, bindingTask.binding.srcAddress);

        if (device && !device->managed())
        {
            DDF_Binding ddfBinding;

            ddfBinding.isUnicastBinding = bindingTask.binding.dstAddrMode == deCONZ::ApsExtAddress;
            ddfBinding.isGroupBinding = bindingTask.binding.dstAddrMode == deCONZ::ApsGroupAddress;
            if (ddfBinding.isUnicastBinding)
            {
                ddfBinding.dstExtAddress = bindingTask.binding.dstAddress.ext;
            }
            else if (ddfBinding.isGroupBinding)
            {
                ddfBinding.dstGroup = bindingTask.binding.dstAddress.group;
            }

            ddfBinding.clusterId = bindingTask.binding.clusterId;
            ddfBinding.dstEndpoint =  bindingTask.binding.dstEndpoint;
            ddfBinding.srcEndpoint = bindingTask.binding.srcEndpoint;

            device->addBinding(ddfBinding);

            auto ddf = deviceDescriptions->get(device);
            if (ddf.status == QLatin1String("Draft"))
            {
                if (ddf.bindings != device->bindings())
                {
                    ddf.bindings = device->bindings();
                    deviceDescriptions->put(ddf);
                }
            }

            if (bindingTask.state == BindingTask::StateFinished) // dummy
            {
                bindingQueue.push_back(bindingTask);
                sendConfigureReportingRequest(bindingQueue.back());
                return false;
            }
        }

        bindingQueue.push_back(bindingTask);
    }
    else
    {
        DBG_Printf(DBG_INFO, "discard double entry in binding queue (size: %zu) for for 0x%016llX, cluster 0x%04X\n", bindingQueue.size(), bindingTask.binding.srcAddress, bindingTask.binding.clusterId);
    }

    return true;
}
