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

// artifical attribute ids for some OTAU related parameters
#define OTAU_SWVERSION_ID     0x1000

#define DONT_CARE_FILE_VERSION                 0xFFFFFFFFUL

#define OTAU_IMAGE_TYPE_QJ             0x00 // Query jitter
#define OTAU_IMAGE_TYPE_QJ_MFC         0x01 // Query jitter, manufacturer code
#define OTAU_IMAGE_TYPE_QJ_MFC_IT      0x02 // Query jitter, manufacturer code, image type
#define OTAU_IMAGE_TYPE_QJ_MFC_IT_FV   0x03 // Query jitter, manufacturer code, image type, file version

#define OTAU_NOTIFY_INTERVAL      (1000 * 60 * 30)
#define OTAU_IDLE_TICKS_NOTIFY    60  // seconds
#define OTAU_BUSY_TICKS           60  // seconds

/*! Inits the otau manager.
 */
void DeRestPluginPrivate::initOtau()
{
    otauIdleTicks = 0;
    otauBusyTicks = 0;
    otauNotifyIter = 0;
    otauIdleTotalCounter = 0;
    otauUnbindIdleTotalCounter = 0;
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
        if (lightNode)
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

            deCONZ::NumericUnion val = {0};
            val.u32 = swVersion;

            lightNode->setZclValue(NodeValue::UpdateByZclRead, ind.srcEndpoint(), OTAU_CLUSTER_ID, OTAU_SWVERSION_ID, val);

            if (lightNode->swBuildId().isEmpty())
            {
                QString version = "0x" + QString("%1").arg(swVersion, 8, 16, QLatin1Char('0')).toUpper();

                lightNode->setSwBuildId(version);
                lightNode->setNeedSaveDatabase(true);
                updateEtag(lightNode->etag);

                // read real sw build id
                lightNode->setLastRead(READ_SWBUILD_ID, idleTotalCounter);
                lightNode->enableRead(READ_SWBUILD_ID);
                lightNode->setNextReadTime(READ_SWBUILD_ID, queryTime);
                queryTime = queryTime.addSecs(5);
            }
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
    else if ((ind.clusterId() == OTAU_CLUSTER_ID) && ((zclFrame.commandId() == OTAU_IMAGE_PAGE_REQUEST_CMD_ID) || (zclFrame.commandId() == OTAU_IMAGE_BLOCK_REQUEST_CMD_ID)))
    {
        // remember last activity time
        otauIdleTotalCounter = idleTotalCounter;

        LightNode *lightNode = getLightNodeForAddress(ind.srcAddress(), ind.srcEndpoint());
        storeRecoverOnOffBri(lightNode);
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

    if (apsCtrlWrapper.apsdeDataRequest(req) != deCONZ::Success)
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

int DeRestPluginPrivate::otauLastBusyTimeDelta() const
{
    if (otauIdleTotalCounter == 0)
    {
        return INT_MAX; // not valid
    }

    if (idleTotalCounter >= otauIdleTotalCounter)
    {
        return idleTotalCounter - otauIdleTotalCounter;
    }

    return INT_MAX;
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

    if (otauNotifyIter >= nodes.size())
    {
        otauNotifyIter = 0;
    }

    // dont do anything if sensors are triggering group commands
    if ((idleTotalCounter - sensorIndIdleTotalCounter) < (60 * 10))
    {
        return;
    }

    LightNode *lightNode = &nodes[otauNotifyIter];
    otauNotifyIter++;

    if (!lightNode->isAvailable() &&
        lightNode->otauClusterId() != OTAU_CLUSTER_ID)
    {
        return;
    }

    // filter vendor
    if (lightNode->manufacturerCode() != VENDOR_DDEL)
    {
        return;
    }

    // whitelist active notify to some devices
    if (lightNode->modelId().startsWith("FLS-NB"))
    { }
    else if (lightNode->modelId().startsWith("FLS-PP3"))
    { }
    else if (lightNode->modelId().startsWith("FLS-A"))
    { }
    else
    {
        return;
    }

    QDateTime now = QDateTime::currentDateTime();
    NodeValue &val = lightNode->getZclValue(OTAU_CLUSTER_ID, OTAU_SWVERSION_ID);

    if (val.updateType == NodeValue::UpdateByZclRead)
    {
        if (val.timestamp.isValid() && val.timestamp.secsTo(now) < OTAU_NOTIFY_INTERVAL)
        {
            return;
        }

        if (val.timestampLastReadRequest.isValid() && val.timestampLastReadRequest.secsTo(now) < OTAU_NOTIFY_INTERVAL)
        {
            return;
        }

        val.timestampLastReadRequest = now;
    }

    otauSendStdNotify(lightNode);
    otauIdleTicks = 0;
}
