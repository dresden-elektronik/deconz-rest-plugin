/*
 * Copyright (c) 2016 dresden elektronik ingenieurtechnik gmbh.
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

    if (!node)
    {
        return false;
    }

    deCONZ::ApsDataRequest apsReq;

    apsReq.setDstAddressMode(deCONZ::ApsExtAddress);
    apsReq.dstAddress() = node->address();
    apsReq.setProfileId(ZDP_PROFILE_ID);
    apsReq.setClusterId(ZDP_MGMT_BIND_REQ_CLID);
    apsReq.setDstEndpoint(ZDO_ENDPOINT);
    apsReq.setSrcEndpoint(ZDO_ENDPOINT);
    apsReq.setTxOptions(0);
    apsReq.setRadius(0);

    QDataStream stream(&apsReq.asdu(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    QTime now = QTime::currentTime();
    stream << (uint8_t)now.second(); // seqno
    stream << startIndex;

    DBG_Assert(apsCtrl != 0);

    if (!apsCtrl)
    {
        return false;
    }

    // send
    if (apsCtrl->apsdeDataRequest(apsReq) == deCONZ::Success)
    {
        return true;
    }

    return false;
}

/*! Handle bind table response.
    \param ind a ZDP MgmtBind_rsp
 */
void DeRestPluginPrivate::handleMgmtBindRspIndication(const deCONZ::ApsDataIndication &ind)
{
    if (!ind.srcAddress().hasExt())
    {
        return;
    }

    if (ind.asdu().size() < 2)
    {
        // at least seq number and status
        return;
    }

    RestNodeBase *node = getSensorNodeForAddress(ind.srcAddress().ext());

    if (!node)
    {
        node = getLightNodeForAddress(ind.srcAddress().ext());
    }

    if (!node)
    {
        return;
    }

    QDataStream stream(ind.asdu());
    stream.setByteOrder(QDataStream::LittleEndian);


    quint8 seqNo;
    quint8 status;

    stream >> seqNo;
    stream >> status;

    DBG_Printf(DBG_INFO, "MgmtBind_rsp %s seq: %u, status 0x%02X \n", qPrintable(node->address().toStringExt()), seqNo, status);

    if (status != deCONZ::ZdpSuccess)
    {
        return;
    }

    quint8 entries;
    quint8 startIndex;
    quint8 listCount;
    bool end = false;

    stream >> entries;
    stream >> startIndex;
    stream >> listCount;

    if (entries > (startIndex + listCount))
    {
        // read more
        readBindingTable(node, startIndex + listCount);
    }
    else
    {
        end = true;
    }

    while (listCount && !stream.atEnd())
    {
        Binding bnd;

        if (bnd.readFromStream(stream))
        {
            if (bnd.dstAddrMode == deCONZ::ApsExtAddress)
            {
                DBG_Printf(DBG_INFO, "found binding 0x%04X, 0x%02X -> 0x%016llX : 0x%02X\n", bnd.clusterId, bnd.srcEndpoint, bnd.dstAddress.ext, bnd.dstEndpoint);
            }
            else if (bnd.dstAddrMode == deCONZ::ApsGroupAddress)
            {
                DBG_Printf(DBG_INFO, "found binding 0x%04X, 0x%02X -> 0x%04X\n", bnd.clusterId, bnd.srcEndpoint, bnd.dstAddress.group);
            }
            else
            {
                continue;
            }

            if (std::find(bindingToRuleQueue.begin(), bindingToRuleQueue.end(), bnd) == bindingToRuleQueue.end())
            {
                bindingToRuleQueue.push_back(bnd);
            }
            else
            {
                DBG_Printf(DBG_INFO, "binding already in binding to rule queue\n");
            }

            std::list<BindingTask>::iterator i = bindingQueue.begin();
            std::list<BindingTask>::iterator end = bindingQueue.end();

            for (;i != end; ++i)
            {
                if (i->state == BindingTask::StateCheck &&
                    i->binding == bnd)
                {
                    if (i->action == BindingTask::ActionBind)
                    {
                        DBG_Printf(DBG_INFO, "binding 0x%04X, 0x%02X already exists, drop task\n", bnd.clusterId, bnd.dstEndpoint);
                        i->state = BindingTask::StateFinished; // already existing
                    }
                    else if (i->action == BindingTask::ActionUnbind)
                    {
                        DBG_Printf(DBG_INFO, "binding 0x%04X, 0x%02X exists, start unbind task\n", bnd.clusterId, bnd.dstEndpoint);
                        i->state = BindingTask::StateIdle; // exists -> unbind
                    }
                    break;
                }
            }
        }
        else // invalid
        {
            DBG_Printf(DBG_INFO, "invalid binding entry");
            break;
        }

        listCount--;
    }

    // end, check remaining tasks
    if (end)
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
                    DBG_Printf(DBG_INFO, "binding 0x%04X, 0x%02X not found, start bind task\n", i->binding.clusterId, i->binding.dstEndpoint);
                    i->state = BindingTask::StateIdle;
                }
                else if (i->action == BindingTask::ActionUnbind)
                {
                    // nothing to unbind
                    DBG_Printf(DBG_INFO, "binding 0x%04X, 0x%02X not found, remove unbind task\n", i->binding.clusterId, i->binding.dstEndpoint);
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

    // write payload according to ZigBee specification (2.4.3.1.7 Match_Descr_req)
    // here we search for ZLL device which provides a OnOff server cluster
    // NOTE: explicit castings ensure correct size of the fields
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

/*! Creates binding for attribute reporting to gateway node. */
void DeRestPluginPrivate::checkLightBindingsForAttributeReporting(LightNode *lightNode)
{
    if (!apsCtrl || !lightNode || !lightNode->address().hasExt())
    {
        return;
    }

    BindingTask::Action action = BindingTask::ActionUnbind;

    // whitelist by Model ID
    if (gwReportingEnabled)
    {
        if (lightNode->modelId().startsWith("FLS-NB"))
        {
            action = BindingTask::ActionBind;
        }
    }

    QList<deCONZ::ZclCluster>::const_iterator i = lightNode->haEndpoint().inClusters().begin();
    QList<deCONZ::ZclCluster>::const_iterator end = lightNode->haEndpoint().inClusters().end();

    for (; i != end; ++i)
    {
        switch (i->id())
        {
        case ONOFF_CLUSTER_ID:
        case LEVEL_CLUSTER_ID:
        {
            DBG_Printf(DBG_INFO, "create binding for attribute reporting of cluster 0x%04X\n", i->id());

            BindingTask bindingTask;
            bindingTask.state = BindingTask::StateCheck;
            bindingTask.action = action;
            bindingTask.restNode = lightNode;
            Binding &bnd = bindingTask.binding;
            bnd.srcAddress = lightNode->address().ext();
            bnd.dstAddrMode = deCONZ::ApsExtAddress;
            bnd.srcEndpoint = lightNode->haEndpoint().endpoint();
            bnd.clusterId = i->id();
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

    lightNode->enableRead(READ_BINDING_TABLE);
    lightNode->setNextReadTime(QTime::currentTime());
    Q_Q(DeRestPlugin);
    q->startZclAttributeTimer(1000);

    if (!bindingTimer->isActive())
    {
        bindingTimer->start();
    }
}


/*! Creates binding for attribute reporting to gateway node. */
void DeRestPluginPrivate::checkSensorBindingsForAttributeReporting(Sensor *sensor)
{
    if (!apsCtrl || !sensor || !sensor->address().hasExt())
    {
        return;
    }

    if (sensor->node() && sensor->node()->isEndDevice())
    {
        DBG_Printf(DBG_INFO, "don't create binding for attribute reporting of end-device %s\n", qPrintable(sensor->name()));
        return;
    }

    BindingTask::Action action = BindingTask::ActionUnbind;

    // whitelist by Model ID
    if (gwReportingEnabled)
    {
        if (sensor->modelId().startsWith("FLS-NB"))
        {
            action = BindingTask::ActionBind;
        }
    }

    std::vector<quint16>::const_iterator i = sensor->fingerPrint().inClusters.begin();
    std::vector<quint16>::const_iterator end = sensor->fingerPrint().inClusters.end();

    for (; i != end; ++i)
    {
        switch (*i)
        {
        case OCCUPANCY_SENSING_CLUSTER_ID:
        case ILLUMINANCE_MEASUREMENT_CLUSTER_ID:
        {
            DBG_Printf(DBG_INFO, "create binding for attribute reporting of cluster 0x%04X\n", (*i));

            BindingTask bindingTask;
            bindingTask.state = BindingTask::StateCheck;
            bindingTask.action = action;
            bindingTask.restNode = sensor;
            Binding &bnd = bindingTask.binding;
            bnd.srcAddress = sensor->address().ext();
            bnd.dstAddrMode = deCONZ::ApsExtAddress;
            bnd.srcEndpoint = sensor->fingerPrint().endpoint;
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

    sensor->enableRead(READ_BINDING_TABLE);
    sensor->setNextReadTime(QTime::currentTime());
    Q_Q(DeRestPlugin);
    q->startZclAttributeTimer(1000);

    if (!bindingTimer->isActive())
    {
        bindingTimer->start();
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
                DBG_Printf(DBG_INFO, "failed to send bind/unbind request. drop\n");
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
                        DBG_Printf(DBG_INFO, "giveup binding srcAddr: %llX (not available)\n", i->binding.srcAddress);
                        i->state = BindingTask::StateFinished;
                    }
                    else
                    {
                        DBG_Printf(DBG_INFO, "binding/unbinding timeout srcAddr: %llX, retry\n", i->binding.srcAddress);
                        i->state = BindingTask::StateIdle;
                        i->timeout = BindingTask::Timeout;
                    }
                }
                else
                {
                    DBG_Printf(DBG_INFO, "giveup binding srcAddr: %llX\n", i->binding.srcAddress);
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
                    i->restNode->enableRead(READ_BINDING_TABLE);
                    i->restNode->setNextReadTime(QTime::currentTime());
                    q->startZclAttributeTimer(1000);

                    i->state = BindingTask::StateCheck;
                    i->timeout = BindingTask::Timeout;

                    DBG_Printf(DBG_INFO, "%s check timeout, retries = %d (srcAddr: 0x%016llX cluster: 0x%04X)\n",
                               (i->action == BindingTask::ActionBind ? "bind" : "unbind"), i->retries, i->binding.srcAddress, i->binding.clusterId);

                    bindingQueue.push_back(*i);
                    bindingQueue.pop_front();
                    break;
                }
                else
                {
                    DBG_Printf(DBG_INFO, "giveup binding\n");
                    DBG_Printf(DBG_INFO, "giveup %s (srcAddr: 0x%016llX cluster: 0x%04X)\n",
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
                    if (i->type() == "ZHALight")
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
        LightNode *lightNode = getLightNodeForAddress(bnd.dstAddress.ext, bnd.dstEndpoint);

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
    cond.setValue(QString::number(bnd.srcEndpoint));

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
    DBG_Printf(DBG_INFO, "cond.value: %s\n", qPrintable(cond.value()));
    DBG_Printf(DBG_INFO, "action.address: %s\n", qPrintable(action.address()));
    DBG_Printf(DBG_INFO, "action.body: %s\n", qPrintable(action.body()));

    if (!foundRule)
    {
        if (sensor && sensor->config().on())
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
