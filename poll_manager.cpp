/*
 * Copyright (c) 2017 dresden elektronik ingenieurtechnik gmbh.
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
void PollManager::poll(RestNodeBase *restNode)
{
    Resource *r = dynamic_cast<Resource*>(restNode);
    DBG_Assert(r != 0);
    if (!r)
    {
        return;
    }

    PollItem pitem;

    if (r->prefix() == RLights)
    {
        LightNode *lightNode = static_cast<LightNode*>(restNode);
        DBG_Assert(lightNode != 0);
        pitem.endpoint = lightNode->haEndpoint().endpoint();
    }
    else
    {
        return;
    }

    pitem.id = restNode->id();
    pitem.prefix = r->prefix();
    pitem.address = restNode->address();

    for (int i = 0; i < r->itemCount(); i++)
    {
        const ResourceItem *item = r->itemForIndex(i);
        const char *suffix = item ? item->descriptor().suffix : 0;

        if (suffix == RStateOn ||
            suffix == RStateBri ||
            suffix == RStateColorMode)
        {
            pitem.items.push_back(suffix);
        }
    }

    for (PollItem &i : items)
    {
        if (i.prefix == r->prefix() && i.id == restNode->id())
        {
            i.items = pitem.items; // update
            return;
        }
    }

    items.push_back(pitem);

    if (!timer->isActive())
    {
        timer->start(1);
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

    DBG_Printf(DBG_INFO, "Poll APS confirm status: 0x%02X\n", conf.status());

    pollState = StateIdle;
    timer->stop();
    timer->start(1);
}

/*! Timer callback to proceed polling.
 */
void PollManager::pollTimerFired()
{
    if (pollState == StateWait)
    {
        DBG_Printf(DBG_INFO, "timout on poll APS confirm\n");
        pollState = StateIdle;
    }

    DBG_Assert(pollState == StateIdle);

    if (items.empty())
    {
        return;
    }

    PollItem &pitem = items.front();
    Resource *r = plugin->getResource(pitem.prefix, pitem.id);
    ResourceItem *item = r ? r->item(RStateReachable) : 0;
    RestNodeBase *restNode = 0;
    const LightNode *lightNode = 0;
    if (r && r->prefix() == RLights)
    {
        restNode = plugin->getLightNodeForId(pitem.id);
        lightNode = static_cast<LightNode*>(restNode);
    }

    if (!r || pitem.items.empty() ||
        !restNode ||
        !item || !item->toBool()) // not reachable
    {
        items.front() = items.back();
        items.pop_back();
        timer->start(1);
        return;
    }

    quint16 clusterId = 0;
    std::vector<quint16> attributes;

    item = r->item(RStateOn);
    bool isOn = item ? item->toBool() : false;
    const char *&suffix = pitem.items[0];

    for (size_t i = 0; pitem.items[0] == 0 && i < pitem.items.size(); i++)
    {
        if (pitem.items[i] != 0)
        {
            pitem.items[0] = pitem.items[i]; // move to front
            pitem.items[i] = 0; // clear
            break;
        }
    }

    if (!suffix)
    {
        pitem.items.clear(); // all done
    }

    if (suffix == RStateOn)
    {
        clusterId = ONOFF_CLUSTER_ID;
        attributes.push_back(0x0000); // onOff
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

        if (!item)
        {
            attributes.push_back(0x0008); // color mode
            attributes.push_back(0x4001); // enhanced color mode
            attributes.push_back(0x400a); // color capabilities
            attributes.push_back(0x400b); // color temperature min
            attributes.push_back(0x400c); // color temperature max
        }
        else
        {
            quint16 cap = item->toNumber();
            std::vector<quint16> toCheck;
            if (cap & 0x0002) // enhanced hue supported
            {
                toCheck.push_back(0x4001); // enhanced color mode
                toCheck.push_back(0x4000); // enhanced hue
                toCheck.push_back(0x0001); // saturation
            }
            else if (cap & 0x0001)
            {
                toCheck.push_back(0x0000); // hue
                toCheck.push_back(0x0001); // saturation
                toCheck.push_back(0x0008); // color mode
            }
            else
            {
                toCheck.push_back(0x0008); // color mode
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

    size_t fresh = 0;
    QDateTime now = QDateTime::currentDateTime();
    for (quint16 attrId : attributes)
    {
        NodeValue &val = restNode->getZclValue(clusterId, attrId);

        if (val.timestampLastReport.isValid() && val.timestampLastReport.secsTo(now) < 240)
        {
            fresh++;
        }
    }

    if (clusterId && fresh > 0 && fresh == attributes.size())
    {
        DBG_Printf(DBG_INFO, "Poll APS request to 0x%016llX cluster: 0x%04X dropped, values are fresh enough\n", pitem.address.ext(), clusterId);
        suffix = 0; // clear
        timer->start(100);
    }
    else if (!attributes.empty() && clusterId &&
        plugin->readAttributes(restNode, pitem.endpoint, clusterId, attributes))
    {
        pollState = StateWait;
        // TODO this hack to get aps request id
        DBG_Assert(plugin->tasks.back().taskType == TaskReadAttributes);
        apsReqId = plugin->tasks.back().req.id();
        dstAddr = pitem.address;
        timer->start(20 * 1000); // wait for confirm
        suffix = 0; // clear
        DBG_Printf(DBG_INFO, "Poll APS request to 0x%016llX cluster: 0x%04X\n", dstAddr.ext(), clusterId);
    }
    else if (suffix)
    {
        suffix = 0; // clear
        timer->start(100);
    }
    else
    {
        if (clusterId)
        {
            DBG_Printf(DBG_INFO, "Poll APS request to 0x%016llX cluster: 0x%04X dropped\n", pitem.address.ext(), clusterId);
        }
        timer->start(100);
        items.front() = items.back();
        items.pop_back();
    }

}
