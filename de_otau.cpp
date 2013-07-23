/*
 * Copyright (C) 2013 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "de_web_plugin_private.h"

#define DE_OTAU_ENDPOINT             0x50

#define OTAU_IMAGE_NOTIFY_CLID                 0x0201
#define OTAU_QUERY_NEXT_IMAGE_REQUEST_CLID     0x0202
#define OTAU_QUERY_NEXT_IMAGE_RESPONSE_CLID    0x8202
#define OTAU_IMAGE_BLOCK_REQUEST_CLID          0x0203
#define OTAU_IMAGE_BLOCK_RESPONSE_CLID         0x8203
#define OTAU_REPORT_STATUS_CLID                0x0205

#define DONT_CARE_FILE_VERSION                 0xFFFFFFFFUL

#define OTAU_IMAGE_TYPE_QJ             0x00 // Query jitter
#define OTAU_IMAGE_TYPE_QJ_MFC         0x01 // Query jitter, manufacturer code
#define OTAU_IMAGE_TYPE_QJ_MFC_IT      0x02 // Query jitter, manufacturer code, image type
#define OTAU_IMAGE_TYPE_QJ_MFC_IT_FV   0x03 // Query jitter, manufacturer code, image type, file version

#define OTAU_IDLE_TICKS_NOTIFY    15  // seconds
#define OTAU_BUSY_TICKS           120 // seconds

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
    otauTimer->start(1000);
}

/*! Handler for incoming otau packets.
 */
void DeRestPluginPrivate::otauDataIndication(const deCONZ::ApsDataIndication &ind)
{
    if (!gwOtauActive)
    {
        return;
    }

    DBG_Assert(ind.profileId() == DE_PROFILE_ID);

    if (ind.profileId() != DE_PROFILE_ID)
    {
        return;
    }

    if (otauIdleTicks > 0)
    {
        otauIdleTicks = 0;
    }

    if (ind.clusterId() == OTAU_IMAGE_BLOCK_REQUEST_CLID)
    {
        if (otauBusyTicks <= 0)
        {
            updateEtag(gwConfigEtag);
        }

        otauBusyTicks = OTAU_BUSY_TICKS;
    }
}

/*! Sends otau notifcation to \p node.
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
    uint16_t manufacturerCode = VENDOR_DDEL;
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

/*! Returns true if otau is busy with uploading data.
 */
bool DeRestPluginPrivate::isOtauBusy()
{
    if (isInNetwork() && gwOtauActive && (otauBusyTicks > 0))
    {
        return true;
    }

    return false;
}

/*! Unicasts otau notify packets to the nodes.
 */
void DeRestPluginPrivate::otauTimerFired()
{
    if (!gwOtauActive)
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

    otauIdleTicks++;

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

    otauSendNotify(&nodes[otauNotifyIter]);

    otauNotifyIter++;
}
