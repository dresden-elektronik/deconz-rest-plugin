/*
 * Copyright (c) 2017 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "de_web_plugin.h"
#include "de_web_plugin_private.h"

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

    Resource *r = dynamic_cast<Resource*>(node);

    // whitelist
    if ((node->address().ext() & macPrefixMask) == deMacPrefix)
    {
    }
    else if (r && r->item(RAttrModelId)->toString().startsWith(QLatin1String("FLS-")))
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

    return false;
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

        if (ind.srcAddress().hasExt())
        {
            for (; i != end; ++i)
            {
                if (i->apsReq.dstAddress().ext() == ind.srcAddress().ext())
                {
                    btReader = &(*i);
                    break;
                }
            }
        }
        else if (ind.srcAddress().hasNwk())
        {
            for (; i != end; ++i)
            {
                if (i->apsReq.dstAddress().nwk() == ind.srcAddress().nwk())
                {
                    btReader = &(*i);
                    break;
                }
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

    if (btReader)
    {
        DBG_Printf(DBG_ZDP, "MgmtBind_rsp id: %d %s seq: %u, status 0x%02X \n", btReader->apsReq.id(),
                   qPrintable(node->address().toStringExt()), seqNo, status);
    }
    else
    {
        DBG_Printf(DBG_ZDP, "MgmtBind_rsp (no BTR) %s seq: %u, status 0x%02X \n", qPrintable(node->address().toStringExt()), seqNo, status);
    }

    if (status != deCONZ::ZdpSuccess)
    {
        if (status == deCONZ::ZdpNotPermitted ||
            status == deCONZ::ZdpNotSupported)
        {
            if (node->mgmtBindSupported())
            {
                DBG_Printf(DBG_ZDP, "MgmtBind_req/rsp %s not supported, deactivate \n", qPrintable(node->address().toStringExt()));
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

            if (std::find(bindingToRuleQueue.begin(), bindingToRuleQueue.end(), bnd) == bindingToRuleQueue.end())
            {
                DBG_Printf(DBG_ZDP, "add binding to check rule queue size: %d\n", static_cast<int>(bindingToRuleQueue.size()));
                bindingToRuleQueue.push_back(bnd);
            }
            else
            {
                DBG_Printf(DBG_ZDP, "binding already in binding to rule queue\n");
            }

            std::list<BindingTask>::iterator i = bindingQueue.begin();
            std::list<BindingTask>::iterator end = bindingQueue.end();

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

    if (!bindingToRuleTimer->isActive() && !bindingToRuleQueue.empty())
    {
        bindingToRuleTimer->start();
    }
}

/*! Handle incoming ZCL configure reporting response.
 */
void DeRestPluginPrivate::handleZclConfigureReportingResponseIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
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

        QDataStream stream(zclFrame.payload());
        stream.setByteOrder(QDataStream::LittleEndian);

        while (!stream.atEnd())
        {
            quint8 status;
            quint8 direction;
            quint16 attrId;
            stream >> status;
            if (stream.status() == QDataStream::ReadPastEnd)
            {
                break;
            }

            // optional fields
            stream >> direction;
            stream >> attrId;

            for (NodeValue &val : restNode->zclValues())
            {
                if (val.zclSeqNum != zclFrame.sequenceNumber())
                {
                    continue;
                }

                if (val.minInterval == 0 && val.maxInterval == 0)
                {
                    continue;
                }

                DBG_Printf(DBG_INFO, "ZCL configure reporting rsp seq: %u 0x%016llX for cluster 0x%04X attr 0x%04X status 0x%02X\n", zclFrame.sequenceNumber(), ind.srcAddress().ext(), ind.clusterId(), val.attributeId, status);
                // mark as succefully configured
                val.timestampLastConfigured = now;
            }
        }
    }
}

/*! Handle bind/unbind response.
    \param ind a ZDP Bind/Unbind_rsp
 */
void DeRestPluginPrivate::handleBindAndUnbindRspIndication(const deCONZ::ApsDataIndication &ind)
{
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
                DBG_Printf(DBG_INFO, "%s response success\n", what);
                if (ind.clusterId() == ZDP_BIND_RSP_CLID)
                {
                    sendConfigureReportingRequest(*i);
                }
            }
            else
            {
                DBG_Printf(DBG_INFO, "%s response failed with status 0x%02X\n", what, status);
            }

            i->state = BindingTask::StateFinished;
            return;
        }
    }
}

/*! Sends a ZDP bind request.
    \param bt a binding task
 */
bool DeRestPluginPrivate::sendBindRequest(BindingTask &bt)
{
    DBG_Assert(apsCtrl != 0);

    if (!apsCtrl)
    {
        return false;
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
    bt.zdpSeqNum = (uint8_t)qrand();

    stream << bt.zdpSeqNum; // ZDP transaction sequence number

    if (!bnd.writeToStream(stream))
    {
        return false;
    }

    if (apsCtrl && (apsCtrl->apsdeDataRequest(apsReq) == deCONZ::Success))
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

    LightNode *lightNode = dynamic_cast<LightNode*>(bt.restNode);
    QDateTime now = QDateTime::currentDateTime();
    std::vector<ConfigureReportingRequest> out;

    for (const ConfigureReportingRequest &rq : requests)
    {
        NodeValue &val = bt.restNode->getZclValue(bt.binding.clusterId, rq.attributeId);
        if (val.clusterId == bt.binding.clusterId)
        {
            // value exists
            if (val.timestampLastReport.isValid() &&
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
                out.push_back(rq);
            }
        }
        else if (lightNode)
        {
            // wait for value is created via polling
            DBG_Printf(DBG_INFO, "skip configure report for cluster: 0x%04X attr: 0x%04X of node 0x%016llX (wait reading or unsupported)\n",
                       bt.binding.clusterId, rq.attributeId, bt.restNode->address().ext());
        }
        else // sensors
        {
            // values doesn't exist, create
            deCONZ::NumericUnion dummy;
            dummy.u64 = 0;
            bt.restNode->setZclValue(NodeValue::UpdateByZclReport, bt.binding.clusterId, rq.attributeId, dummy);
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
    zclFrame.setSequenceNumber(requests.front().zclSeqNum);
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
            DBG_Printf(DBG_INFO_L2, "configure reporting for 0x%016llX, attribute 0x%04X/0x%04X\n", bt.restNode->address().ext(), bt.binding.clusterId, rq.attributeId);
        }
    }

    { // ZCL frame
        QDataStream stream(&apsReq.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        zclFrame.writeToStream(stream);
    }


    if (apsCtrl && apsCtrl->apsdeDataRequest(apsReq) == deCONZ::Success)
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

    QDateTime now = QDateTime::currentDateTime();
    ConfigureReportingRequest rq;

    rq.zclSeqNum = zclSeq++; // to match in configure reporting response handler

    if (bt.binding.clusterId == OCCUPANCY_SENSING_CLUSTER_ID)
    {
        // add values if not already present
        deCONZ::NumericUnion dummy;
        dummy.u64 = 0;
        if (bt.restNode->getZclValue(bt.binding.clusterId, 0x0000).clusterId != bt.binding.clusterId)
        {
            bt.restNode->setZclValue(NodeValue::UpdateInvalid, bt.binding.clusterId, 0x0000, dummy);
        }

        NodeValue &val = bt.restNode->getZclValue(bt.binding.clusterId, 0x0000);
        val.zclSeqNum = rq.zclSeqNum;

        rq.dataType = deCONZ::Zcl8BitBitMap;
        rq.attributeId = 0x0000; // occupancy
        val.minInterval = 1;     // value used by Hue bridge
        val.maxInterval = 300;   // value used by Hue bridge
        rq.minInterval = val.minInterval;
        rq.maxInterval = val.maxInterval;

        if (sendConfigureReportingRequest(bt, {rq}))
        {
            Sensor *sensor = static_cast<Sensor *>(bt.restNode);
            if (sensor && sensor->modelId() == QLatin1String("SML001")) // Hue motion sensor
            {
                rq = ConfigureReportingRequest();
                rq.dataType = deCONZ::Zcl8BitUint;
                rq.attributeId = 0x0030;      // sensitivity
                rq.minInterval = 5;           // value used by Hue bridge
                rq.maxInterval = 7200;        // value used by Hue bridge
                rq.reportableChange8bit = 1;  // value used by Hue bridge
                rq.manufacturerCode = VENDOR_PHILIPS;
                return sendConfigureReportingRequest(bt, {rq});
            }
            return true;
        }
        return false;
    }
    else if (bt.binding.clusterId == ILLUMINANCE_MEASUREMENT_CLUSTER_ID)
    {
        rq.dataType = deCONZ::Zcl16BitUint;
        rq.attributeId = 0x0000;         // measured value
        rq.minInterval = 5;              // value used by Hue bridge
        rq.maxInterval = 300;            // value used by Hue bridge
        rq.reportableChange16bit = 2000; // value used by Hue bridge
        return sendConfigureReportingRequest(bt, {rq});
    }
    else if (bt.binding.clusterId == TEMPERATURE_MEASUREMENT_CLUSTER_ID)
    {
        rq.dataType = deCONZ::Zcl16BitInt;
        rq.attributeId = 0x0000;       // measured value
        rq.minInterval = 10;           // value used by Hue bridge
        rq.maxInterval = 300;          // value used by Hue bridge
        rq.reportableChange16bit = 20; // value used by Hue bridge
        return sendConfigureReportingRequest(bt, {rq});
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
    else if (bt.binding.clusterId == POWER_CONFIGURATION_CLUSTER_ID)
    {
        Sensor *sensor = dynamic_cast<Sensor *>(bt.restNode);

        // add values if not already present
        deCONZ::NumericUnion dummy;
        dummy.u64 = 0;
        if (bt.restNode->getZclValue(POWER_CONFIGURATION_CLUSTER_ID, 0x0021).attributeId != 0x0021)
        {
            bt.restNode->setZclValue(NodeValue::UpdateInvalid, BASIC_CLUSTER_ID, 0x0021, dummy);
        }

        NodeValue &val = bt.restNode->getZclValue(POWER_CONFIGURATION_CLUSTER_ID, 0x0021);
        val.zclSeqNum = rq.zclSeqNum;

        rq.dataType = deCONZ::Zcl8BitUint;
        rq.attributeId = 0x0021;   // battery percentage remaining
        if (sensor && sensor->modelId() == QLatin1String("SML001")) // Hue motion sensor
        {
            val.minInterval = 7200;       // value used by Hue bridge
            val.maxInterval = 7200;       // value used by Hue bridge
            rq.reportableChange8bit = 0;  // value used by Hue bridge
        }
        else if (sensor && sensor->modelId().startsWith(QLatin1String("RWL02"))) // Hue dimmer switch
        {
            val.minInterval = 300;        // value used by Hue bridge
            val.maxInterval = 300;        // value used by Hue bridge
            rq.reportableChange8bit = 0;  // value used by Hue bridge
        }
        else
        {
            val.minInterval = 300;
            val.maxInterval = 60 * 45;
            rq.reportableChange8bit = 1;
        }

        if (val.timestampLastReport.isValid() && (val.timestampLastReport.secsTo(now) < val.maxInterval * 1.5))
        {
            return false;
        }

        rq.minInterval = val.minInterval;
        rq.maxInterval = val.maxInterval;

        return sendConfigureReportingRequest(bt, {rq});
    }
    else if (bt.binding.clusterId == ONOFF_CLUSTER_ID)
    {
        rq.dataType = deCONZ::ZclBoolean;
        rq.attributeId = 0x0000; // on/off

        if ((bt.restNode->address().ext() & macPrefixMask) == deMacPrefix)
        {
            rq.minInterval = 5;
            rq.maxInterval = 180;
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
        rq.reportableChange48bit = 10; // 0.01 kWh

        ConfigureReportingRequest rq2;
        rq2.dataType = deCONZ::Zcl24BitInt;
        rq2.attributeId = 0x0400; // Instantaneous Demand
        rq2.minInterval = 1;
        rq2.maxInterval = 300;
        rq2.reportableChange24bit = 10; // 1 W

        return sendConfigureReportingRequest(bt, {rq, rq2});
    }
    else if (bt.binding.clusterId == ELECTRICAL_MEASUREMENT_CLUSTER_ID)
    {
        rq.dataType = deCONZ::Zcl16BitInt;
        rq.attributeId = 0x050B; // Active power
        rq.minInterval = 1;
        rq.maxInterval = 300;
        rq.reportableChange16bit = 10; // 1 W

        ConfigureReportingRequest rq2;
        rq2.dataType = deCONZ::Zcl16BitUint;
        rq2.attributeId = 0x0505; // RMS Voltage
        rq2.minInterval = 1;
        rq2.maxInterval = 300;
        rq2.reportableChange16bit = 100; // 1 V

        ConfigureReportingRequest rq3;
        rq3.dataType = deCONZ::Zcl16BitUint;
        rq3.attributeId = 0x0508; // RMS Current
        rq3.minInterval = 1;
        rq3.maxInterval = 300;
        rq3.reportableChange16bit = 1; // 0.1 A

        return sendConfigureReportingRequest(bt, {rq, rq2, rq3});
    }
    else if (bt.binding.clusterId == LEVEL_CLUSTER_ID)
    {
        rq.dataType = deCONZ::Zcl8BitUint;
        rq.attributeId = 0x0000; // current level

        if ((bt.restNode->address().ext() & macPrefixMask) == deMacPrefix)
        {
            rq.minInterval = 5;
            rq.maxInterval = 180;
            rq.reportableChange8bit = 5;
        }
        else // default configuration
        {
            rq.minInterval = 1;
            rq.maxInterval = 300;
            rq.reportableChange8bit = 1;
        }
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

        return sendConfigureReportingRequest(bt, {rq, rq2, rq3, rq4});
    }
    else if (bt.binding.clusterId == BASIC_CLUSTER_ID &&
            (bt.restNode->address().ext() & macPrefixMask) == philipsMacPrefix)
    {
        Sensor *sensor = dynamic_cast<Sensor*>(bt.restNode);
        if (!sensor)
        {
            return false;
        }

        // only process for presence sensor: don't issue configuration for temperature and illuminance sensors
        // TODO check if just used for SML001 sensor or also hue dimmer switch?
        if (sensor->type() != QLatin1String("ZHAPresence"))
        {
            return false;
        }

        deCONZ::NumericUnion dummy;
        dummy.u64 = 0;
        // add usertest value if not already present
        if (bt.restNode->getZclValue(BASIC_CLUSTER_ID, 0x0032).attributeId != 0x0032)
        {
            bt.restNode->setZclValue(NodeValue::UpdateInvalid, BASIC_CLUSTER_ID, 0x0032, dummy);
        }
        // ledindication value if not already present
        if (bt.restNode->getZclValue(BASIC_CLUSTER_ID, 0x0033).attributeId != 0x0033)
        {
            bt.restNode->setZclValue(NodeValue::UpdateInvalid, BASIC_CLUSTER_ID, 0x0033, dummy);
        }

        NodeValue &val = bt.restNode->getZclValue(BASIC_CLUSTER_ID, 0x0032);

        val.zclSeqNum = rq.zclSeqNum;
        val.minInterval = 5;
        val.maxInterval = 7200;

        if (val.timestampLastReport.isValid() && (val.timestampLastReport.secsTo(now) < val.maxInterval * 1.5))
        {
            return false;
        }

        // already configured? wait for report ...
        if (val.timestampLastConfigured.isValid() && (val.timestampLastConfigured.secsTo(now) < val.maxInterval * 1.5))
        {
            return false;
        }

        rq.dataType = deCONZ::ZclBoolean;
        rq.attributeId = 0x0032; // usertest
        rq.minInterval = val.minInterval;   // value used by Hue bridge
        rq.maxInterval = val.maxInterval;   // value used by Hue bridge
        rq.manufacturerCode = VENDOR_PHILIPS;

        NodeValue &val2 = bt.restNode->getZclValue(BASIC_CLUSTER_ID, 0x0033);
        val2.zclSeqNum = rq.zclSeqNum;
        val2.minInterval = 5;
        val2.maxInterval = 7200;

        ConfigureReportingRequest rq2;
        rq2 = ConfigureReportingRequest();
        rq2.dataType = deCONZ::ZclBoolean;
        rq2.attributeId = 0x0033; // ledindication
        rq2.minInterval = val2.minInterval; // value used by Hue bridge
        rq2.maxInterval = val2.maxInterval; // value used by Hue bridge
        rq2.manufacturerCode = VENDOR_PHILIPS;

        return sendConfigureReportingRequest(bt, {rq, rq2});
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

    // prevent binding action if otau was busy recently
    if (otauLastBusyTimeDelta() < OTA_LOW_PRIORITY_TIME)
    {
        if (lightNode->modelId().startsWith(QLatin1String("FLS-")))
        {
            DBG_Printf(DBG_INFO, "don't check binding for attribute reporting of %s (otau busy)\n", qPrintable(lightNode->name()));
            return;
        }
    }

    BindingTask::Action action = BindingTask::ActionUnbind;

    // whitelist
    if (gwReportingEnabled)
    {
        action = BindingTask::ActionBind;
        if (lightNode->modelId().startsWith(QLatin1String("FLS-NB")))
        {
        }
        else if (lightNode->manufacturer() == QLatin1String("OSRAM"))
        {
        }
        else if (lightNode->manufacturerCode() == VENDOR_UBISYS)
        {
        }
        else if (lightNode->manufacturerCode() == VENDOR_IKEA)
        {
        }
        else if (lightNode->manufacturerCode() == VENDOR_EMBER)
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

    QList<deCONZ::ZclCluster>::const_iterator i = lightNode->haEndpoint().inClusters().begin();
    QList<deCONZ::ZclCluster>::const_iterator end = lightNode->haEndpoint().inClusters().end();

    int tasksAdded = 0;
    QDateTime now = QDateTime::currentDateTime();

    for (; i != end; ++i)
    {
        switch (i->id())
        {
        case ONOFF_CLUSTER_ID:
        case LEVEL_CLUSTER_ID:
        case COLOR_CLUSTER_ID:
        {
            bool bindingExists = false;
            for (const NodeValue &val : lightNode->zclValues())
            {
                if (val.clusterId != i->id() || !val.timestampLastReport.isValid())
                {
                    continue;
                }

                if (val.timestampLastReport.secsTo(now) < (10 * 60))
                {
                    bindingExists = true;
                    break;
                }
            }

            BindingTask bt;
            if ((lightNode->address().ext() & macPrefixMask) == deMacPrefix)
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

    if ((lightNode->address().ext() & macPrefixMask) == deMacPrefix)
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


/*! Creates binding for attribute reporting to gateway node. */
void DeRestPluginPrivate::checkSensorBindingsForAttributeReporting(Sensor *sensor)
{
    if (!apsCtrl || !sensor || !sensor->address().hasExt() || !sensor->node() || !sensor->toBool(RConfigReachable))
    {
        return;
    }

    if (findSensorsState != FindSensorsActive &&
        idleTotalCounter < (IDLE_READ_LIMIT + 120)) // wait for some input before fire bindings
    {
        return;
    }

    bool endDeviceSupported = false;
    // whitelist
        // Climax
    if (sensor->modelId().startsWith(QLatin1String("LM_")) ||
        sensor->modelId().startsWith(QLatin1String("LMHT_")) ||
        sensor->modelId().startsWith(QLatin1String("IR_")) ||
        sensor->modelId().startsWith(QLatin1String("DC_")) ||
        // Philips
        sensor->modelId() == QLatin1String("SML001") ||
        sensor->modelId().startsWith(QLatin1String("RWL02")) ||
        // ubisys
        sensor->modelId().startsWith(QLatin1String("D1")) ||
        // IKEA
        sensor->modelId().startsWith(QLatin1String("TRADFRI")) ||
        // Heiman
        sensor->modelId().startsWith(QLatin1String("SmartPlug")) ||
        sensor->modelId().startsWith(QLatin1String("CO_")) ||
        sensor->modelId().startsWith(QLatin1String("DOOR_")) ||
        sensor->modelId().startsWith(QLatin1String("PIR_")) ||
        sensor->modelId().startsWith(QLatin1String("GAS_")) ||
        sensor->modelId().startsWith(QLatin1String("TH-H_")) ||
        sensor->modelId().startsWith(QLatin1String("TH-T_")) ||
        sensor->modelId().startsWith(QLatin1String("SMOK_")) ||
        sensor->modelId().startsWith(QLatin1String("WATER_")))
    {
        endDeviceSupported = true;
        sensor->setMgmtBindSupported(false);
    }

    if (!endDeviceSupported)
    {
        DBG_Printf(DBG_INFO_L2, "don't create binding for attribute reporting of end-device %s\n", qPrintable(sensor->name()));
        return;
    }

    // prevent binding action if otau was busy recently
    if (otauLastBusyTimeDelta() < OTA_LOW_PRIORITY_TIME)
    {
        if (sensor->modelId().startsWith(QLatin1String("FLS-")))
        {
            DBG_Printf(DBG_INFO_L2, "don't check binding for attribute reporting of %s (otau busy)\n", qPrintable(sensor->name()));
            return;
        }
    }

    BindingTask::Action action = BindingTask::ActionUnbind;

    // whitelist by Model ID
    if (gwReportingEnabled)
    {
        if (sensor->modelId().startsWith(QLatin1String("FLS-NB")) || endDeviceSupported)
        {
            action = BindingTask::ActionBind;
        }
    }

    bool checkBindingTable = false;
    QDateTime now = QDateTime::currentDateTime();
    std::vector<quint16>::const_iterator i = sensor->fingerPrint().inClusters.begin();
    std::vector<quint16>::const_iterator end = sensor->fingerPrint().inClusters.end();

    for (; i != end; ++i)
    {
        NodeValue val;

        if (*i == ILLUMINANCE_MEASUREMENT_CLUSTER_ID)
        {
            val = sensor->getZclValue(*i, 0x0000); // measured value
        }
        else if (*i == TEMPERATURE_MEASUREMENT_CLUSTER_ID)
        {
            val = sensor->getZclValue(*i, 0x0000); // measured value
        }
        else if (*i == RELATIVE_HUMIDITY_CLUSTER_ID)
        {
            val = sensor->getZclValue(*i, 0x0000); // measured value
        }
        else if (*i == PRESSURE_MEASUREMENT_CLUSTER_ID)
        {
            val = sensor->getZclValue(*i, 0x0000); // measured value
        }
        else if (*i == OCCUPANCY_SENSING_CLUSTER_ID)
        {
            if (sensor->modelId() == QLatin1String("SML001")) // Hue motion sensor
            {
                val = sensor->getZclValue(*i, 0x0030); // sensitivity
            }
            else
            {
                val = sensor->getZclValue(*i, 0x0000); // occupied state
            }
        }
        else if (*i == POWER_CONFIGURATION_CLUSTER_ID)
        {
            val = sensor->getZclValue(*i, 0x0021); // battery percentage remaining

            if (val.timestampLastConfigured.isValid() && val.timestampLastConfigured.secsTo(now) < (val.maxInterval * 1.5))
            {
                continue;
            }
        }
        else if (*i == VENDOR_CLUSTER_ID)
        {
            if (sensor->modelId().startsWith(QLatin1String("RWL02"))) // Hue dimmer switch
            {
                val = sensor->getZclValue(*i, 0x0000); // button event
            }
        }
        else if (*i == BASIC_CLUSTER_ID)
        {
            if (sensor->modelId() == QLatin1String("SML001") && // Hue motion sensor
                sensor->type() == QLatin1String("ZHAPresence"))
            {
                val = sensor->getZclValue(*i, 0x0032); // usertest
                // val = sensor->getZclValue(*i, 0x0033); // ledindication

                if (val.timestampLastConfigured.isValid() && val.timestampLastConfigured.secsTo(now) < (val.maxInterval * 1.5))
                {
                    continue;
                }
            }
            else
            {
                continue;
            }
        }
        else if (*i == METERING_CLUSTER_ID)
        {
            val = sensor->getZclValue(*i, 0x0000); // Curent Summation Delivered

        }
        else if (*i == ELECTRICAL_MEASUREMENT_CLUSTER_ID)
        {
            val = sensor->getZclValue(*i, 0x050b); // Active power
        }

        quint16 maxInterval = (val.maxInterval > 0) ? (val.maxInterval * 1.5) : (60 * 45);

        if (val.timestampLastReport.isValid() &&
            val.timestampLastReport.secsTo(now) < maxInterval) // got update in timely manner
        {
            DBG_Printf(DBG_INFO_L2, "binding for attribute reporting of cluster 0x%04X seems to be active\n", (*i));
            continue;
        }

        if (!sensor->node()->nodeDescriptor().receiverOnWhenIdle() && sensor->lastRx().secsTo(now) > 3)
        {
            DBG_Printf(DBG_INFO, "skip binding for attribute reporting of cluster 0x%04X (end-device might sleep)\n", (*i));
            return;
        }

        quint8 srcEndpoint = sensor->fingerPrint().endpoint;

        {  // some clusters might not be on fingerprint endpoint (power configuration), search in other simple descriptors
            deCONZ::SimpleDescriptor *sd= sensor->node()->getSimpleDescriptor(srcEndpoint);
            if (!sd || !sd->cluster(*i, deCONZ::ServerCluster))
            {
                for (int j = 0; j < sensor->node()->simpleDescriptors().size(); j++)
                {
                    sd = &sensor->node()->simpleDescriptors()[j];

                    if (sd && sd->cluster(*i, deCONZ::ServerCluster))
                    {
                        srcEndpoint = sd->endpoint();
                        break;
                    }
                }
            }
        }

        switch (*i)
        {
        case POWER_CONFIGURATION_CLUSTER_ID:
        case OCCUPANCY_SENSING_CLUSTER_ID:
        case ILLUMINANCE_MEASUREMENT_CLUSTER_ID:
        case TEMPERATURE_MEASUREMENT_CLUSTER_ID:
        case RELATIVE_HUMIDITY_CLUSTER_ID:
        case PRESSURE_MEASUREMENT_CLUSTER_ID:
        case METERING_CLUSTER_ID:
        case ELECTRICAL_MEASUREMENT_CLUSTER_ID:
        case VENDOR_CLUSTER_ID:
        case BASIC_CLUSTER_ID:
        {
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

            bindingTask.action = action;
            bindingTask.restNode = sensor;
            Binding &bnd = bindingTask.binding;
            bnd.srcAddress = sensor->address().ext();
            bnd.dstAddrMode = deCONZ::ApsExtAddress;
            bnd.srcEndpoint = srcEndpoint;
            bnd.clusterId = *i;
            bnd.dstAddress.ext = apsCtrl->getParameter(deCONZ::ParamMacAddress);
            bnd.dstEndpoint = endpoint();

            if (bnd.dstEndpoint > 0) // valid gateway endpoint?
            {
                queueBindingTask(bindingTask);
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

    if (!bindingTimer->isActive())
    {
        bindingTimer->start();
    }
}

/*! Creates binding for group control (switches, motion sensor, ...). */
void DeRestPluginPrivate::checkSensorBindingsForClientClusters(Sensor *sensor)
{
    if (!apsCtrl || !sensor || !sensor->node() || !sensor->address().hasExt() || !sensor->toBool(RConfigReachable))
    {
        return;
    }

    if (findSensorsState != FindSensorsActive &&
        idleTotalCounter < (IDLE_READ_LIMIT + 60)) // wait for some input before fire bindings
    {
        return;
    }

    QDateTime now = QDateTime::currentDateTime();
    if (!sensor->node()->nodeDescriptor().receiverOnWhenIdle() && sensor->lastRx().secsTo(now) > 10)
    {
        DBG_Printf(DBG_INFO, "skip check bindings for client clusters (end-device might sleep)\n");
        return;
    }

    ResourceItem *item = sensor->item(RConfigGroup);

    if (!item || !item->lastSet().isValid())
    {
        return;
    }

    quint8 srcEndpoint = sensor->fingerPrint().endpoint;
    std::vector<quint16> clusters;

    if (sensor->modelId().startsWith(QLatin1String("RWL02"))) // Hue dimmer switch
    {
        srcEndpoint = 1;
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
    }
    // IKEA Trådfri dimmer
    else if (sensor->modelId() == QLatin1String("TRADFRI wireless dimmer"))
    {
        clusters.push_back(LEVEL_CLUSTER_ID);
    }
    // IKEA Trådfri remote
    else if (sensor->modelId().startsWith(QLatin1String("TRADFRI remote")))
    {
        clusters.push_back(ONOFF_CLUSTER_ID);
        clusters.push_back(LEVEL_CLUSTER_ID);
        clusters.push_back(SCENE_CLUSTER_ID);
    }
    else if (sensor->modelId().startsWith(QLatin1String("D1")))
    {
        clusters.push_back(ONOFF_CLUSTER_ID);
        clusters.push_back(LEVEL_CLUSTER_ID);
    }
    else
    {
        return;
    }

    // prevent binding action if otau was busy recently
    if (otauLastBusyTimeDelta() < OTA_LOW_PRIORITY_TIME)
    {
        return;
    }

    Group *group = getGroupForId(item->toString());

    if (!group)
    {
        return;
    }

    std::vector<quint16>::const_iterator i = clusters.begin();
    std::vector<quint16>::const_iterator end = clusters.end();

    for (; i != end; ++i)
    {
        DBG_Printf(DBG_INFO_L2, "0x%016llX (%s) create binding for client cluster 0x%04X on endpoint 0x%02X\n",
                   sensor->address().ext(), qPrintable(sensor->modelId()), (*i), sensor->fingerPrint().endpoint);

        BindingTask bindingTask;

        bindingTask.state = BindingTask::StateIdle;
        bindingTask.action = BindingTask::ActionBind;
        bindingTask.restNode = sensor;
        Binding &bnd = bindingTask.binding;
        bnd.srcAddress = sensor->address().ext();
        bnd.dstAddrMode = deCONZ::ApsGroupAddress;
        bnd.srcEndpoint = srcEndpoint;
        bnd.clusterId = *i;
        bnd.dstAddress.group = group->address();
        bnd.dstEndpoint = endpoint();

        if (bnd.dstEndpoint > 0) // valid gateway endpoint?
        {
            queueBindingTask(bindingTask);
        }
    }

    if (!bindingTimer->isActive())
    {
        bindingTimer->start();
    }
}

/*! Creates groups for \p sensor if needed. */
void DeRestPluginPrivate::checkSensorGroup(Sensor *sensor)
{
    if (!sensor)
    {
        return;
    }

    Group *group = 0;

    {
        std::vector<Group>::iterator i = groups.begin();
        std::vector<Group>::iterator end = groups.end();

        for (; i != end; ++i)
        {
            if (i->state() == Group::StateNormal &&
                i->deviceIsMember(sensor->id()))
            {
                group = &*i;
                break;
            }
        }
    }

    if (sensor->modelId().startsWith(QLatin1String("RWL02"))) // Hue dimmer switch
    {
        if (!group)
        {
            getGroupIdentifiers(sensor, 0x01, 0x00);
            return;
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
                        if (i->state() == Group::StateNormal && i->id() == gid)
                        {
                            group = &*i;
                            break;
                        }
                    }
                }
            }
        }
    }
    else if (sensor->modelId() == QLatin1String("ZGPSWITCH"))
    {
        // go on
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
        const QString &gid = item->toString();

        std::vector<Group>::iterator i = groups.begin();
        std::vector<Group>::iterator end = groups.end();

        for (; i != end; ++i)
        {
            if (i->state() == Group::StateNormal && i->id() == gid)
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
    }

    if (group->addDeviceMembership(sensor->id()))
    {

    }

    if (item->toString() != group->id())
    {
        item->setValue(group->id());
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

    if (!item || !item->lastSet().isValid())
    {
        return;
    }

    const QString &gid = item->toString();

    {
        std::vector<Group>::iterator i = groups.begin();
        std::vector<Group>::iterator end = groups.end();

        for (; i != end; ++i)
        {
            if (gid == i->id()) // current group
            {
                if (i->state() != Group::StateNormal)
                {
                    DBG_Printf(DBG_INFO, "reanimate group %u for sensor %s\n", i->address(), qPrintable(sensor->name()));
                    i->setState(Group::StateNormal);
                    updateGroupEtag(&*i);
                    queSaveDb(DB_GROUPS, DB_SHORT_SAVE_DELAY);
                }
            }
            else if (i->deviceIsMember(sensor->id()))
            {
                if (i->state() == Group::StateNormal)
                {
                    DBG_Printf(DBG_INFO, "delete old group %u of sensor %s\n", i->address(), qPrintable(sensor->name()));
                    i->setState(Group::StateDeleted);
                    updateGroupEtag(&*i);
                    queSaveDb(DB_GROUPS | DB_LIGHTS, DB_SHORT_SAVE_DELAY);

                    // for each node which is part of this group send a remove group request (will be unicast)
                    // note: nodes which are curently switched off will not be removed!
                    std::vector<LightNode>::iterator j = nodes.begin();
                    std::vector<LightNode>::iterator jend = nodes.end();

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
    std::vector<Group>::iterator i = groups.begin();
    std::vector<Group>::iterator end = groups.end();
    for (; i != end; ++i)
    {
        if (i->deviceIsMember(id) && i->state() == Group::StateNormal)
        {
            i->setState(Group::StateDeleted);
            i->removeDeviceMembership(id);

            updateGroupEtag(&*i);
            queSaveDb(DB_GROUPS | DB_LIGHTS, DB_SHORT_SAVE_DELAY);

            // for each node which is part of this group send a remove group request (will be unicast)
            // note: nodes which are curently switched off will not be removed!
            std::vector<LightNode>::iterator j = nodes.begin();
            std::vector<LightNode>::iterator jend = nodes.end();

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
            }
            else
            {
                // too harsh?
                DBG_Printf(DBG_INFO_L2, "failed to send bind/unbind request. drop\n");
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
                        DBG_Printf(DBG_INFO_L2, "giveup binding srcAddr: %llX (not available)\n", i->binding.srcAddress);
                        i->state = BindingTask::StateFinished;
                    }
                    else
                    {
                        DBG_Printf(DBG_INFO_L2, "binding/unbinding timeout srcAddr: %llX, retry\n", i->binding.srcAddress);
                        i->state = BindingTask::StateIdle;
                        i->timeout = BindingTask::Timeout;
                    }
                }
                else
                {
                    DBG_Printf(DBG_INFO_L2, "giveup binding srcAddr: %llX\n", i->binding.srcAddress);
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
        bindingTimer->start();
    }
}

/*
QString Binding::toString()
{
    //        QString dbg = QString("srcAddr: 0x%1 srcEp: %2 clusterId: %3")
    //            .arg(i->srcAddress, 16, 16, QChar('0'))
    //            .arg(i->srcEndpoint)
    //            .arg(i->clusterId);

    //        if (i->dstAddrMode == 0x01) // group address
    //        {
    //            dbg.append(QString(" dstGroup: 0x%1").arg(i->dstAddress.group, 2, 16, QChar('0')));
    //        }
    //        else if (i->dstAddrMode == 0x03) // ext address + dst endpoint
    //        {
    //            dbg.append(QString(" dstExt: 0x%1 dstEp: %2").arg(i->dstAddress.ext, 16, 16, QChar('0'))
    //                                                         .arg(i->dstEndpoint));
    //        }

    //        DBG_Printf(DBG_INFO, "Binding %s\n", qPrintable(dbg));
}
*/

/*! Process binding to rule conversion.
    For bindings found via binding table query, check if there exists already
    a rule representing it. If such a rule does not exist it will be created.
*/
void DeRestPluginPrivate::bindingToRuleTimerFired()
{
    if (bindingToRuleQueue.empty())
    {
        return;
    }

    Binding bnd = bindingToRuleQueue.front();
    bindingToRuleQueue.pop_front();

    if (!bindingToRuleQueue.empty())
    {
        bindingToRuleTimer->start();
    }

    if (!apsCtrl)
    {
        return;
    }

    // binding table maintenance
    // check if destination node exist and remove binding if not
    if (bnd.dstAddrMode == deCONZ::ApsExtAddress)
    {
        bool found = false;
        int idx = 0;
        const deCONZ::Node *node = 0;
        while (apsCtrl->getNode(idx, &node) == 0)
        {
            if (bnd.dstAddress.ext == node->address().ext())
            {
                found = true;
                break;
            }
            idx++;
        }

        if (!found)
        {
            DBG_Printf(DBG_INFO, "remove binding from 0x%016llX cluster 0x%04X to non existing node 0x%016llX\n", bnd.srcAddress, bnd.clusterId, bnd.dstAddress.ext);
            BindingTask bindingTask;
            bindingTask.state = BindingTask::StateIdle;
            bindingTask.action = BindingTask::ActionUnbind;
            bindingTask.binding = bnd;
            queueBindingTask(bindingTask);
            if (!bindingTimer->isActive())
            {
                bindingTimer->start();
            }
            return;
        }
    }


    std::vector<Sensor>::iterator i = sensors.begin();
    std::vector<Sensor>::iterator end = sensors.end();

    Sensor *sensor = 0;

    for (; i != end; ++i)
    {
        if (!i->modelId().startsWith(QLatin1String("FLS-NB")))
        {
            continue;
        }

        if (bnd.srcAddress == i->address().ext())
        {
            if (bnd.srcEndpoint == i->fingerPrint().endpoint)
            {
                // match only valid sensors
                switch (bnd.clusterId)
                {
                case ONOFF_CLUSTER_ID:
                case LEVEL_CLUSTER_ID:
                case SCENE_CLUSTER_ID:
                {
                    if (i->type() == "ZHASwitch")
                    {
                        sensor = &(*i);
                    }
                }
                    break;

                case ILLUMINANCE_MEASUREMENT_CLUSTER_ID:
                {
                    if (i->type() == "ZHALightLevel")
                    {
                        sensor = &(*i);
                    }
                }
                    break;

                case OCCUPANCY_SENSING_CLUSTER_ID:
                {
                    if (i->type() == "ZHAPresence")
                    {
                        sensor = &(*i);
                    }
                }
                    break;

                default:
                    break;
                }
            }
        }

        if (sensor)
        {
            break;
        }
    }

    // only proceed if the sensor (binding source) is known
    if (!sensor)
    {
//        deCONZ::Address addr;
//        addr.setExt(bnd.srcAddress);
//        DBG_Printf(DBG_INFO, "Binding to Rule unsupported sensor %s\n", qPrintable(addr.toStringExt()));
        return;
    }

    Rule rule;
    RuleCondition cond;
    RuleAction action;

    if (bnd.dstAddrMode == Binding::ExtendedAddressMode)
    {
        deCONZ::Address addr;
        addr.setExt(bnd.dstAddress.ext);
        LightNode *lightNode = getLightNodeForAddress(addr, bnd.dstEndpoint);

        if (lightNode)
        {
            action.setAddress(QString("/lights/%1/state").arg(lightNode->id()));
        }
        else
        {
            DBG_Printf(DBG_INFO_L2, "Binding to Rule no LightNode found for dstAddress: %s\n",
                       qPrintable(QString("0x%1").arg(bnd.dstAddress.ext, 16,16, QChar('0'))));
            return;
        }

    }
    else if (bnd.dstAddrMode == Binding::GroupAddressMode)
    {
        action.setAddress(QString("/groups/%1/action").arg(bnd.dstAddress.group));
    }
    else
    {
        DBG_Printf(DBG_INFO, "Binding to Rule unsupported dstAddrMode 0x%02X\n", bnd.dstAddrMode);
        return;
    }

    action.setMethod("BIND");

    QVariantMap body;
    QString item;

    if (bnd.clusterId == ONOFF_CLUSTER_ID)
    {
        body["on"] = true;
        item = "buttonevent";
    }
    else if (bnd.clusterId == LEVEL_CLUSTER_ID)
    {
        body["bri"] = (double)1;
        item = "buttonevent";
    }
    else if (bnd.clusterId == ILLUMINANCE_MEASUREMENT_CLUSTER_ID)
    {
        body["illum"] = QString("report");
        item = "illuminance";
    }
    else if (bnd.clusterId == OCCUPANCY_SENSING_CLUSTER_ID)
    {
        body["occ"] = QString("report");
        item = "presence";
    }
    else if (bnd.clusterId == SCENE_CLUSTER_ID)
    {
        body["scene"] = QString("S%1").arg(bnd.srcEndpoint);
        item = "buttonevent";
    }
    else
    {
        return;
    }

    action.setBody(deCONZ::jsonStringFromMap(body));

    cond.setAddress(QString("/sensors/%1/state/%2").arg(sensor->id()).arg(item));
    cond.setOperator("eq");
    cond.setValue(bnd.srcEndpoint);

    // check if a rule for that binding already exists
    bool foundRule = false;

    std::vector<Rule>::const_iterator ri = rules.begin();
    std::vector<Rule>::const_iterator rend = rules.end();

    for (; !foundRule && (ri != rend); ++ri) // rule loop
    {
        std::vector<RuleCondition>::const_iterator ci = ri->conditions().begin();
        std::vector<RuleCondition>::const_iterator cend = ri->conditions().end();
        for (; !foundRule && (ci != cend); ++ci) // rule.conditions loop
        {
            // found matching condition
            if ((ci->address()   == cond.address()) &&
                (ci->ooperator() == cond.ooperator()) &&
                (ci->value()     == cond.value()))
            {
                std::vector<RuleAction>::const_iterator ai = ri->actions().begin();
                std::vector<RuleAction>::const_iterator aend = ri->actions().end();

                for (; !foundRule && (ai != aend); ++ai) // rule.actions loop
                {
                    if ((ai->method() == action.method()) && (ai->address() == action.address()))
                    {
                        // search action body which covers the binding clusterId
                        if (bnd.clusterId == ONOFF_CLUSTER_ID)
                        {
                            if (ai->body().contains("on"))
                            {
                                rule = *ri;
                                foundRule = true;
                            }
                        }
                        else if (bnd.clusterId == ILLUMINANCE_MEASUREMENT_CLUSTER_ID)
                        {
                            if (ai->body().contains("illum"))
                            {
                                rule = *ri;
                                foundRule = true;
                            }
                        }
                        else if (bnd.clusterId == OCCUPANCY_SENSING_CLUSTER_ID)
                        {
                            if (ai->body().contains("occ"))
                            {
                                rule = *ri;
                                foundRule = true;
                            }
                        }
                        else if (bnd.clusterId == LEVEL_CLUSTER_ID)
                        {
                            if (ai->body().contains("bri"))
                            {
                                rule = *ri;
                                foundRule = true;
                            }
                        }
                        else if (bnd.clusterId == SCENE_CLUSTER_ID)
                        {
                            if (ai->body().contains("scene"))
                            {
                                rule = *ri;
                                foundRule = true;
                            }
                        }
                        else
                        {
                            DBG_Printf(DBG_INFO, "Binding to Rule unhandled clusterId 0x%04X\n", bnd.clusterId);
                        }
                    }
                }
            }
        }
    }

    DBG_Printf(DBG_INFO, "cond.address: %s\n", qPrintable(cond.address()));
    DBG_Printf(DBG_INFO, "cond.value: %s\n", qPrintable(cond.value().toString()));
    DBG_Printf(DBG_INFO, "action.address: %s\n", qPrintable(action.address()));
    DBG_Printf(DBG_INFO, "action.body: %s\n", qPrintable(action.body()));

    if (!foundRule)
    {
        if (sensor && sensor->item(RConfigOn)->toBool())
        {
            std::vector<RuleAction> actions;
            std::vector<RuleCondition> conditions;

            actions.push_back(action);
            conditions.push_back(cond);

            updateEtag(rule.etag);
            rule.setOwner("deCONZ");
            rule.setCreationtime(QDateTime::currentDateTimeUtc().toString("yyyy-MM-ddTHH:mm:ss"));
            rule.setActions(actions);
            rule.setConditions(conditions);

            // create a new rule id // don't overwrite already existing rules
            rule.setId("1");

            bool ok;
            do {
                ok = true;
                std::vector<Rule>::const_iterator i = rules.begin();
                std::vector<Rule>::const_iterator end = rules.end();

                for (; i != end; ++i)
                {
                    if (i->id() == rule.id())
                    {
                        rule.setId(QString::number(i->id().toInt() + 1));
                        ok = false;
                    }
                }
            } while (!ok);

            rule.setName(QString("Rule %1").arg(rule.id()));
            rules.push_back(rule);

            queSaveDb(DB_RULES, DB_SHORT_SAVE_DELAY);

            DBG_Printf(DBG_INFO, "Rule %s created from Binding\n", qPrintable(rule.id()));
        }
        else if (gwDeleteUnknownRules)
        {

            DBG_Printf(DBG_INFO, "Rule for Binding doesn't exists start unbind 0x%04X\n", bnd.clusterId);
            BindingTask bt;
            bt.state = BindingTask::StateIdle;
            bt.restNode = sensor; // might be 0
            bt.action = BindingTask::ActionUnbind;
            bt.binding = bnd;
            queueBindingTask(bt);
        }
    }
    else
    {
        if (rule.state() == Rule::StateDeleted || rule.status() == "disabled")
        {
            DBG_Printf(DBG_INFO, "Rule for Binding already exists (inactive), start unbind 0x%04X\n", bnd.clusterId);
            BindingTask bt;
            bt.state = BindingTask::StateIdle;
            bt.restNode = sensor; // might be 0
            bt.action = BindingTask::ActionUnbind;
            bt.binding = bnd;
            queueBindingTask(bt);
        }
        else
        {
            DBG_Printf(DBG_INFO, "Rule for Binding 0x%04X already exists\n", bnd.clusterId);
        }
    }

    if (!bindingTimer->isActive())
    {
        bindingTimer->start();
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
            i->apsReq.setTxOptions(0);
            i->apsReq.setRadius(0);

            QDataStream stream(&apsReq.asdu(), QIODevice::WriteOnly);
            stream.setByteOrder(QDataStream::LittleEndian);

            QTime now = QTime::currentTime();
            stream << (uint8_t)now.second(); // seqno
            stream << i->index;

            // send
            if (apsCtrl && apsCtrl->apsdeDataRequest(apsReq) == deCONZ::Success)
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
