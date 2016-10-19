/*
 * Copyright (c) 2016 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "de_web_plugin_private.h"

// de otau specific
#define OTAU_IMAGE_NOTIFY_CLID                 0x0201
#define OTAU_QUERY_NEXT_IMAGE_REQUEST_CLID     0x0202
#define OTAU_QUERY_NEXT_IMAGE_RESPONSE_CLID    0x8202
#define OTAU_IMAGE_BLOCK_REQUEST_CLID          0x0203
#define OTAU_IMAGE_BLOCK_RESPONSE_CLID         0x8203
#define OTAU_REPORT_STATUS_CLID                0x0205

// std otau specific
#define OTAU_IMAGE_NOTIFY_CMD_ID             0x00
#define OTAU_QUERY_NEXT_IMAGE_REQUEST_CMD_ID 0x01
#define OTAU_IMAGE_BLOCK_REQUEST_CMD_ID      0x03
#define OTAU_IMAGE_PAGE_REQUEST_CMD_ID       0x04
#define OTAU_UPGRADE_END_REQUEST_CMD_ID      0x06

#define DONT_CARE_FILE_VERSION                 0xFFFFFFFFUL

#define OTAU_IMAGE_TYPE_QJ             0x00 // Query jitter
#define OTAU_IMAGE_TYPE_QJ_MFC         0x01 // Query jitter, manufacturer code
#define OTAU_IMAGE_TYPE_QJ_MFC_IT      0x02 // Query jitter, manufacturer code, image type
#define OTAU_IMAGE_TYPE_QJ_MFC_IT_FV   0x03 // Query jitter, manufacturer code, image type, file version

#define OTAU_IDLE_TICKS_NOTIFY    60  // seconds
#define OTAU_BUSY_TICKS           60  // seconds

/*! Inits the otau manager.
 */
void DeRestPluginPrivate::initOtau()
{
    otauIdleTicks = 0;
    otauBusyTicks = 0;
    otauNotifyIter = 0;
    otauNotifyDelay = deCONZ::appArgumentNumeric("--otau-notify-delay", OTAU_IDLE_TICKS_NOTIFY);

    otauTimer = new QTimer(this);
    otauTimer->setSingleShot(false);
    connect(otauTimer, SIGNAL(timeout()),
            this, SLOT(otauTimerFired()));

    if (otauNotifyDelay > 0)
    {
        otauTimer->start(1000);
    }
}

/*! Handler for incoming otau packets.
 */
void DeRestPluginPrivate::otauDataIndication(const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame)
{
    if ((ind.clusterId() == OTAU_CLUSTER_ID) && (zclFrame.commandId() == OTAU_QUERY_NEXT_IMAGE_REQUEST_CMD_ID))
    {
        LightNode *lightNode = getLightNodeForAddress(ind.srcAddress(), ind.srcEndpoint());

        // extract software version from request
        if (lightNode && lightNode->swBuildId().isEmpty())
        {
            QDataStream stream(zclFrame.payload());
            stream.setByteOrder(QDataStream::LittleEndian);

            quint8 fieldControl;
            quint16 manufacturerId;
            quint16 imageType;
            quint32 swVersion;
            quint16 hwVersion;

            stream >> fieldControl;
            stream >> manufacturerId;
            stream >> imageType;
            stream >> swVersion;

            if (fieldControl & 0x01)
            {
                stream >> hwVersion;
            }

            QString version;
            version.sprintf("%08X", swVersion);

            lightNode->setSwBuildId(version);
            updateEtag(lightNode->etag);

            // read real sw build id
            lightNode->setLastRead(READ_SWBUILD_ID, idleTotalCounter);
            lightNode->enableRead(READ_SWBUILD_ID);
            lightNode->setNextReadTime(READ_SWBUILD_ID, queryTime);
            queryTime = queryTime.addSecs(5);
        }
    }
    else if ((ind.clusterId() == OTAU_CLUSTER_ID) && (zclFrame.commandId() == OTAU_UPGRADE_END_REQUEST_CMD_ID))
    {
        LightNode *lightNode = getLightNodeForAddress(ind.srcAddress(), ind.srcEndpoint());

        if (lightNode)
        {
            lightNode->setLastRead(READ_SWBUILD_ID, idleTotalCounter);
            lightNode->enableRead(READ_SWBUILD_ID);
            lightNode->setNextReadTime(READ_SWBUILD_ID, queryTime.addSecs(120));
        }
    }

    if (!isOtauActive())
    {
        return;
    }

    if (((ind.profileId() == DE_PROFILE_ID) && (ind.clusterId() == OTAU_IMAGE_BLOCK_REQUEST_CLID)) ||
        ((ind.clusterId() == OTAU_CLUSTER_ID) && (zclFrame.commandId() == OTAU_IMAGE_BLOCK_REQUEST_CMD_ID)) ||
        ((ind.clusterId() == OTAU_CLUSTER_ID) && (zclFrame.commandId() == OTAU_IMAGE_PAGE_REQUEST_CMD_ID)))
    {
        if (otauIdleTicks > 0)
        {
            otauIdleTicks = 0;
        }

        if (otauBusyTicks <= 0)
        {
            updateEtag(gwConfigEtag);
        }

        otauBusyTicks = OTAU_BUSY_TICKS;
    }
}

/*! Sends otau notifcation (de specific otau cluster) to \p node.
 */
void DeRestPluginPrivate::otauSendNotify(LightNode *node)
{
    if (!node->isAvailable())
    {
        return;
    }

    deCONZ::ApsDataRequest req;

    req.setDstAddressMode(deCONZ::ApsExtAddress);
    req.dstAddress() = node->address();
    req.setDstEndpoint(DE_OTAU_ENDPOINT);
    req.setSrcEndpoint(DE_OTAU_ENDPOINT);
    req.setProfileId(DE_PROFILE_ID);
    req.setClusterId(OTAU_IMAGE_NOTIFY_CLID);

    req.setTxOptions(0);
    req.setRadius(0);

    QByteArray arr;
    QDataStream stream(&arr, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    uint8_t reqType = OTAU_IMAGE_TYPE_QJ_MFC_IT_FV;
    uint8_t queryJitter = 100;
    uint16_t manufacturerCode = VENDOR_ATMEL;
    uint16_t imageType = 0x0000;
    uint32_t fileVersion = DONT_CARE_FILE_VERSION; // any node shall answer

    stream << reqType;
    stream << queryJitter;
    stream << manufacturerCode;
    stream << imageType;
    stream << fileVersion;

    req.setAsdu(arr);

    if (deCONZ::ApsController::instance()->apsdeDataRequest(req) == 0)
    {
        DBG_Printf(DBG_INFO, "otau send image notify\n");
    }
    else
    {
        DBG_Printf(DBG_INFO, "otau send image notify failed\n");
    }
}

/*! Sends otau notifcation (std otau cluster) to \p node.
    The node will then send a query next image request.
 */
void DeRestPluginPrivate::otauSendStdNotify(LightNode *node)
{
    deCONZ::ApsDataRequest req;
    deCONZ::ZclFrame zclFrame;

    req.setProfileId(HA_PROFILE_ID);
    req.setClusterId(OTAU_CLUSTER_ID);
    req.setDstAddressMode(deCONZ::ApsExtAddress);
    req.dstAddress().setExt(node->address().ext());
    req.setDstEndpoint(node->haEndpoint().endpoint());
    req.setSrcEndpoint(endpoint());
    req.setState(deCONZ::FireAndForgetState);

    zclFrame.setSequenceNumber(zclSeq++);
    zclFrame.setCommandId(OTAU_IMAGE_NOTIFY_CMD_ID);

    zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionServerToClient |
                             deCONZ::ZclFCDisableDefaultResponse);

    { // payload
        QDataStream stream(&zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        uint8_t payloadType = 0x00; // query jitter
        uint8_t queryJitter = 100;

        stream << payloadType;
        stream << queryJitter;
    }

    { // ZCL frame
        QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        zclFrame.writeToStream(stream);
    }

    if (apsCtrl && apsCtrl->apsdeDataRequest(req) != deCONZ::Success)
    {
        DBG_Printf(DBG_INFO, "otau failed to send image notify request\n");
    }
}

/*! Returns true if otau is busy with uploading data.
 */
bool DeRestPluginPrivate::isOtauBusy()
{
    if (isInNetwork() && isOtauActive() && (otauBusyTicks > 0))
    {
        return true;
    }

    return false;
}

/*! Returns true if otau is activated.
 */
bool DeRestPluginPrivate::isOtauActive()
{
    if (apsCtrl)
    {
        return apsCtrl->getParameter(deCONZ::ParamOtauActive) == 1;
    }

    return false;
}

/*! Unicasts otau notify packets to the nodes.
 */
void DeRestPluginPrivate::otauTimerFired()
{
    if (!isOtauActive())
    {
        return;
    }

    if (otauNotifyDelay == 0)
    {
        return;
    }

    if (!isInNetwork())
    {
        return;
    }

    if (nodes.empty())
    {
        return;
    }

    if (otauIdleTicks < INT_MAX)
    {
        otauIdleTicks++;
    }

    if (otauBusyTicks > 0)
    {
        otauBusyTicks--;

        if (otauBusyTicks == 0)
        {
            updateEtag(gwConfigEtag);
        }
    }

    if (otauIdleTicks < otauNotifyDelay)
    {
        return;
    }

    otauIdleTicks = 0;

    if (otauNotifyIter >= nodes.size())
    {
        otauNotifyIter = 0;
    }

    LightNode *lightNode = &nodes[otauNotifyIter];

    if (lightNode->isAvailable() &&
        lightNode->otauClusterId() == OTAU_CLUSTER_ID)
    {
        // std otau
        if (lightNode->manufacturerCode() == VENDOR_DDEL)
        {
            // whitelist active notify to some devices
            if (lightNode->modelId().startsWith("FLS-NB"))
            {
                otauSendStdNotify(lightNode);
            }
        }
    }
    else
    {
        // de specific otau
        //otauSendNotify(lightNode);
    }

    otauNotifyIter++;
}
