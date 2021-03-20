/*
 * Copyright (c) 2017-2018 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "poll_manager.h"
#include "de_web_plugin_private.h"

/*! Constructor.
 */
PollManager::PollManager(QObject *parent) :
    QObject(parent)
{
    pollState = StateIdle;
    timer = new QTimer(this);
    timer->setSingleShot(true);
    connect(timer, SIGNAL(timeout()), this, SLOT(pollTimerFired()));
    plugin = qobject_cast<DeRestPluginPrivate*>(parent);
}

/*! Queues polling of the node.
    \param restNode - the node to poll
 */
void PollManager::poll(RestNodeBase *restNode, const QDateTime &tStart)
{
    Resource *r = dynamic_cast<Resource*>(restNode);
    DBG_Assert(r);
    if (!r || !restNode->node())
    {
        return;
    }

    DBG_Assert(!hasItems());

    PollItem pitem;

    if (!restNode->node()->nodeDescriptor().receiverOnWhenIdle())
    {
        return;
    }

    LightNode *lightNode = nullptr;
    Sensor *sensor = nullptr;

    if (r->prefix() == RLights)
    {
        lightNode = dynamic_cast<LightNode*>(restNode);
        DBG_Assert(lightNode);
        if (!lightNode || lightNode->state() != LightNode::StateNormal)
        {
            return;
        }
        pitem.endpoint = lightNode->haEndpoint().endpoint();
        DBG_Printf(DBG_INFO_L2, "Poll light node %s\n", qPrintable(lightNode->name()));
    }
    else if (r->prefix() == RSensors)
    {
        sensor = dynamic_cast<Sensor*>(restNode);
        DBG_Assert(sensor);
        if (!sensor || sensor->deletedState() != Sensor::StateNormal)
        {
            return;
        }
        pitem.endpoint = sensor->fingerPrint().endpoint;
        DBG_Printf(DBG_INFO_L2, "Poll %s sensor node %s\n", qPrintable(sensor->type()), qPrintable(sensor->name()));
    }
    else
    {
        return;
    }

    pitem.id = restNode->id();
    pitem.prefix = r->prefix();
    pitem.address = restNode->address();
    pitem.tStart = tStart;

    for (int i = 0; i < r->itemCount(); i++)
    {
        const ResourceItem *item = r->itemForIndex(i);
        const char *suffix = item ? item->descriptor().suffix : nullptr;

        if (plugin->permitJoinFlag)
        {
            // limit queries during joining
            if (suffix == RAttrModelId || suffix == RAttrSwVersion)
            {
                pitem.items.push_back(suffix);
            }
        }
        else if (suffix == RStateOn ||
            suffix == RStateBri ||
            suffix == RStateColorMode ||
            (suffix == RStateConsumption && sensor && sensor->type() == QLatin1String("ZHAConsumption")) ||
            (suffix == RStatePower && sensor && sensor->type() == QLatin1String("ZHAPower")) ||
            (suffix == RStatePresence && sensor && sensor->type() == QLatin1String("ZHAPresence")) ||
            (suffix == RStateLightLevel && sensor && sensor->type() == QLatin1String("ZHALightLevel")) ||
            suffix == RAttrModelId ||
            suffix == RAttrSwVersion)
        {
            // DBG_Printf(DBG_INFO_L2, "    attribute %s\n", suffix);
            pitem.items.push_back(suffix);
        }
    }

    for (PollItem &i : items)
    {
        if (i.prefix == r->prefix() && i.id == restNode->id())
        {
            i.items = pitem.items; // update
            if (tStart.isValid())
            {
                i.tStart = tStart;
            }
            return;
        }
    }

    items.push_back(pitem);

    if (!timer->isActive())
    {
        timer->start(100);
    }
}

/*! Delays polling for \p ms milliseconds.
 */
void PollManager::delay(int ms)
{
    timer->stop();
    timer->start(ms);
}

/*! Handle APS confirm if related to polling.
 */
void PollManager::apsdeDataConfirm(const deCONZ::ApsDataConfirm &conf)
{
    if (pollState != StateWait)
    {
        return;
    }

    if (apsReqId != conf.id())
    {
        return;
    }

    if (dstAddr.hasExt() && conf.dstAddress().hasExt()
        && dstAddr.ext() != conf.dstAddress().ext())
    {

    }

    else if (dstAddr.hasNwk() && conf.dstAddress().hasNwk()
        && dstAddr.nwk() != conf.dstAddress().nwk())
    {

    }

    DBG_Printf(DBG_INFO_L2, "Poll APS confirm %u status: 0x%02X\n", conf.id(), conf.status());

    if (!items.empty() && conf.status() != deCONZ::ApsSuccessStatus)
    {
        PollItem &pitem = items.front();

        for (auto &i : pitem.items)
        {
            if (i)
            {
                DBG_Printf(DBG_INFO_L2, "\t drop item %s\n", i);
                i = nullptr; // clear
            }
        }
    }

    pollState = StateIdle;
    timer->stop();
    timer->start(1);
}

/*! Timer callback to proceed polling.
 */
void PollManager::pollTimerFired()
{
    if (pollState == StateDone)
    {
        pollState = StateIdle;
        timer->start(50);
        emit done();
        return;
    }

    if (pollState == StateWait)
    {
        DBG_Printf(DBG_INFO, "timeout on poll APS confirm\n");
        pollState = StateIdle;
    }

    DBG_Assert(pollState == StateIdle);

    if (items.empty())
    {
        pollState = StateDone;
        timer->start(500);
        return;
    }

    QDateTime now = QDateTime::currentDateTime();
    PollItem &pitem = items.front();
    Resource *r = plugin->getResource(pitem.prefix, pitem.id);
    ResourceItem *item = nullptr;
    RestNodeBase *restNode = nullptr;
    const LightNode *lightNode = nullptr;
    if (r && r->prefix() == RLights)
    {
        restNode = plugin->getLightNodeForId(pitem.id);
        lightNode = dynamic_cast<LightNode*>(restNode);
        item = r->item(RStateReachable);
    }
    else if (r && r->prefix() == RSensors)
    {
        restNode = plugin->getSensorNodeForId(pitem.id);
        item = r->item(RConfigReachable);
    }

    if (pitem.tStart.isValid() && pitem.tStart > now)
    {
        if (items.size() > 1)
        {
            PollItem tmp = pitem;
            items.front() = items.back();
            items.back() = tmp;
        }
        timer->start(1);
        return;
    }

    if (!r || pitem.items.empty() ||
        !restNode ||
        //!restNode->lastRx().isValid() ||
        !item || !item->toBool()) // not reachable
    {
        items.front() = items.back();
        items.pop_back();
        timer->start(1);
        return;
    }

    //const auto dtReachable = item->lastSet().secsTo(now);

    quint16 clusterId = 0xffff; // invalid
    std::vector<quint16> attributes;

    item = r->item(RStateOn);
    bool isOn = item ? item->toBool() : false;
    const char *&suffix = pitem.items[0];

    for (size_t i = 0; pitem.items[0] == nullptr && i < pitem.items.size(); i++)
    {
        if (pitem.items[i] != nullptr)
        {
            pitem.items[0] = pitem.items[i]; // move to front
            pitem.items[i] = nullptr; // clear
            break;
        }
    }

    if (!suffix)
    {
        pitem.items.clear(); // all done
    }

    if (suffix == RStateOn && lightNode)
    {
        item = r->item(RAttrModelId);

        if (UseTuyaCluster(lightNode->manufacturer()))
        {
            //Thoses devices haven't cluster 0006, and use Cluster specific
        }
        else
        {
            clusterId = ONOFF_CLUSTER_ID;
            attributes.push_back(0x0000); // onOff
        }
    }
    else if (suffix == RStateBri && isOn)
    {
        NodeValue &val = restNode->getZclValue(LEVEL_CLUSTER_ID, 0x0000);

        if (isOn || !val.timestamp.isValid())
        {
            clusterId = LEVEL_CLUSTER_ID;
            attributes.push_back(0x0000); // current level
        }
    }
    else if (suffix == RStateColorMode && lightNode)
    {
        clusterId = COLOR_CLUSTER_ID;
        item = r->item(RConfigColorCapabilities);

        if ((!item || item->toNumber() <= 0) && (lightNode->haEndpoint().profileId() == ZLL_PROFILE_ID || lightNode->manufacturerCode() == VENDOR_XIAOMI || lightNode->manufacturerCode() == VENDOR_MUELLER || lightNode->manufacturerCode() == VENDOR_XAL || lightNode->manufacturerCode() == VENDOR_LEDVANCE))
        {
            if (item && lightNode->modelId() == QLatin1String("lumi.light.aqcn02"))
            {
                item->setValue(0x0010); // color capabilities are not supported, set here
            }

            attributes.push_back(0x0008); // color mode
            attributes.push_back(0x4001); // enhanced color mode
            attributes.push_back(0x400a); // color capabilities
            attributes.push_back(0x400b); // color temperature min
            attributes.push_back(0x400c); // color temperature max
        }
        else
        {
            quint16 cap = item ? static_cast<quint16>(item->toNumber()) : 0;
            std::vector<quint16> toCheck;

            if (cap == 0 && lightNode->haEndpoint().profileId() == HA_PROFILE_ID)
            {
                // e.g. OSRAM US version
                // DEV_ID_HA_COLOR_DIMMABLE_LIGHT
                cap  = (0x0001 | 0x0008 | 0x0010); // hue, saturation, color mode, xy, ct
            }

            toCheck.push_back(0x0008); // color mode
            toCheck.push_back(0x4001); // enhanced color mode

            // if reading 0x400x attributes fail with response 0x86 they will be marked
            // as not available and will be ignored in further poll cycles

            if (cap & 0x0002) // enhanced hue supported
            {
                toCheck.push_back(0x4000); // enhanced hue
                toCheck.push_back(0x0001); // saturation
            }
            else if (cap & 0x0001)
            {
                toCheck.push_back(0x0000); // hue
                toCheck.push_back(0x0001); // saturation
            }

            if (cap & 0x0004)
            {
                toCheck.push_back(0x4002); // Color loop active
            }

            if (cap & 0x0008)
            {
                toCheck.push_back(0x0003); // currentX
                toCheck.push_back(0x0004); // currentY
            }

            if (cap & 0x0010)
            {
                toCheck.push_back(0x0007); // color temperature
            }

            for (const deCONZ::ZclCluster &cl : lightNode->haEndpoint().inClusters())
            {
                if (cl.id() != COLOR_CLUSTER_ID)
                {
                    continue;
                }

                for (const deCONZ::ZclAttribute &attr : cl.attributes())
                {
                    for (quint16 attrId : toCheck)
                    {
                        // discard attributes which are not be available
                        if (attrId == attr.id() && attr.isAvailable())
                        {
                            NodeValue &val = restNode->getZclValue(clusterId, attrId);
                            if (isOn || !val.timestamp.isValid())
                            {
                                attributes.push_back(attrId);
                            }
                        }
                    }
                }

                break;
            }
        }
    }
    else if (suffix == RStatePresence)
    {
        clusterId = OCCUPANCY_SENSING_CLUSTER_ID;
        attributes.push_back(0x0000); // Occupancy
        attributes.push_back(0x0010); // PIR Occupied To Unoccupied Delay
    }
    else if (suffix == RStateLightLevel)
    {
        clusterId = ILLUMINANCE_MEASUREMENT_CLUSTER_ID;
        attributes.push_back(0x0000); // Measured Value
    }
    else if (suffix == RStateConsumption)
    {
        clusterId = METERING_CLUSTER_ID;
        attributes.push_back(0x0000); // Current Summation Delivered
        attributes.push_back(0x0400); // Instantaneous Demand
    }
    else if (suffix == RStatePower)
    {
        clusterId = ELECTRICAL_MEASUREMENT_CLUSTER_ID;
        attributes.push_back(0x050b); // Active Power
        attributes.push_back(0x0505); // RMS Voltage
        attributes.push_back(0x0508); // RMS Current
    }
    else if (suffix == RAttrModelId)
    {
        item = r->item(RAttrModelId);
        if (item && (item->toString().isEmpty() || item->toString() == QLatin1String("unknown") ||
             (item->lastSet().secsTo(now) > READ_MODEL_ID_INTERVAL && item->toString().startsWith("FLS-A")) // dynamic model ids
            ))
        {
            clusterId = BASIC_CLUSTER_ID;
            //attributes.push_back(0x0004); // manufacturer
            attributes.push_back(0x0005); // model id
        }
    }
    else if (suffix == RAttrSwVersion && lightNode)
    {
        item = r->item(RAttrSwVersion);
        if (item && (item->toString().isEmpty() ||
             (item->lastSet().secsTo(now) > READ_SWBUILD_ID_INTERVAL))) // dynamic
        {
            if (lightNode->manufacturerCode() == VENDOR_EMBER && lightNode->modelId() == QLatin1String("TS011F")) // LIDL plugs
            {
                if (item->toString().isEmpty())
                {
                    attributes.push_back(0x0001);  // application version
                    clusterId = BASIC_CLUSTER_ID;
                }
            }
            else if (lightNode->manufacturerCode() == VENDOR_UBISYS ||
                lightNode->manufacturerCode() == VENDOR_EMBER ||
                lightNode->manufacturerCode() == VENDOR_HEIMAN ||
                lightNode->manufacturerCode() == VENDOR_XIAOMI ||
                lightNode->manufacturerCode() == VENDOR_DEVELCO ||
                lightNode->manufacturer().startsWith(QLatin1String("Climax")) ||
                lightNode->manufacturer() == QLatin1String("SZ"))
            {
                if (item->toString().isEmpty())
                {
                    attributes.push_back(0x0006); // date code
                    clusterId = BASIC_CLUSTER_ID;
                }
            }
            else
            {
                if (item->toString().isEmpty() ||
                    lightNode->manufacturerCode() == VENDOR_IKEA ||
                    lightNode->manufacturerCode() == VENDOR_OSRAM ||
                    lightNode->manufacturerCode() == VENDOR_OSRAM_STACK ||
                    lightNode->manufacturerCode() == VENDOR_XAL ||
                    lightNode->manufacturerCode() == VENDOR_PHILIPS ||
                    lightNode->manufacturerCode() == VENDOR_DDEL)
                {
                    attributes.push_back(0x4000); // sw build id
                    clusterId = BASIC_CLUSTER_ID;
                }
            }
        }
    }

    size_t fresh = 0;
    const int reportWaitTime = 360;
    const int reportWaitTimeXAL = 60 * 30;

    // check that cluster exists on endpoint
    if (clusterId != 0xffff)
    {
        bool found = false;
        deCONZ::SimpleDescriptor sd;
        if (restNode->node()->copySimpleDescriptor(pitem.endpoint, &sd) == 0)
        {
            for (const auto &cl : sd.inClusters())  // Loop through clusters
            {
                if (cl.id() == clusterId)
                {
                    found = true;
                    
                    std::vector<quint16> check;

                    for (const deCONZ::ZclAttribute &attr : cl.attributes())    // Loop through cluster attributes
                    {
                        for (quint16 attrId : attributes)   // Loop through poll candidates
                        {
                            // discard attributes which are not be available
                            if (attrId == attr.id() && attr.isAvailable())
                            {
                                if (attr.dataType_t() == deCONZ::ZclCharacterString && attr.toString().isEmpty() && attr.lastRead() != static_cast<time_t>(-1))
                                {
                                    continue; // skip empty string attributes which are available, read only once
                                }

                                check.push_back(attr.id());     // Only use available attributes

                                if (cl.id() == BASIC_CLUSTER_ID)
                                {
                                    continue; // don't rely on reporting
                                }

                                NodeValue &val = restNode->getZclValue(clusterId, attrId);

                                if (lightNode && lightNode->manufacturerCode() == VENDOR_IKEA && val.timestamp.isValid())
                                {
                                    fresh++; // rely on reporting for ikea lights
                                }
                                else if (val.timestampLastReport.isValid() && val.timestampLastReport.secsTo(now) < reportWaitTime)
                                {
                                    fresh++;
                                }
                                else if (lightNode && lightNode->manufacturerCode() == VENDOR_XAL && val.timestamp.isValid() && val.timestamp.secsTo(now) < reportWaitTimeXAL)
                                {
                                    fresh++; // rely on reporting for XAL lights
                                }

                            }
                        }
                    }

                    attributes = check;     // reassign filtered attributes
                    break;
                }
            }
        }

        if (!found)
        {
            DBG_Printf(DBG_INFO_L2, "Poll APS request to 0x%016llX cluster: 0x%04X dropped, cluster doesn't exist\n", pitem.address.ext(), clusterId);
            clusterId = 0xffff;
        }
    }

    if (clusterId != 0xffff && fresh > 0 && fresh == attributes.size())
    {
        DBG_Printf(DBG_INFO_L2, "Poll APS request to 0x%016llX cluster: 0x%04X dropped, values are fresh enough\n", pitem.address.ext(), clusterId);
        suffix = nullptr; // clear
        timer->start(100);
    }
    else if (!attributes.empty() && clusterId != 0xffff &&
        plugin->readAttributes(restNode, pitem.endpoint, clusterId, attributes))
    {
        pollState = StateWait;
        // TODO this hack to get aps request id
        DBG_Assert(plugin->tasks.back().taskType == TaskReadAttributes);
        apsReqId = plugin->tasks.back().req.id();
        dstAddr = pitem.address;
        timer->start(60 * 1000); // wait for confirm
        suffix = nullptr; // clear
        DBG_Printf(DBG_INFO_L2, "Poll APS request %u to 0x%016llX cluster: 0x%04X\n", apsReqId, dstAddr.ext(), clusterId);
    }
    else if (suffix)
    {
        suffix = nullptr; // clear
        timer->start(100);
    }
    else
    {
        if (clusterId != 0xffff)
        {
            DBG_Printf(DBG_INFO_L2, "Poll APS request to 0x%016llX cluster: 0x%04X dropped\n", pitem.address.ext(), clusterId);
        }
        timer->start(100);
        items.front() = items.back();
        items.pop_back();
    }
}
