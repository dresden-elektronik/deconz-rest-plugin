/*
 * Copyright (c) 2016-2024 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "de_web_plugin_private.h"
#include "device_descriptions.h"
#include "database.h"


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
    otauIdleTotalCounter = 0;

    otauTimer = new QTimer(this);
    otauTimer->setSingleShot(false);
    connect(otauTimer, SIGNAL(timeout()),
            this, SLOT(otauTimerFired()));

    otauTimer->start(1000);
}

/*! Handler for incoming otau packets.
 */
void DeRestPluginPrivate::otauDataIndication(const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame, Device *device)
{
    if (!device || ind.clusterId() != OTAU_CLUSTER_ID)
    {
        return;
    }

    quint8 fieldControl;
    quint16 manufacturerId;
    quint16 imageType;
    quint32 swVersion = 0;
    quint16 hwVersion;
    bool updateOtaTicks = false;

    if (zclFrame.isProfileWideCommand() && zclFrame.commandId() == deCONZ::ZclReadAttributesResponseId)
    {
        // TODO(mpi): parsing the attribute response can likely be removed here
        // since this is already done by the read function.
        // Below the item->needPushChange() check is used to capture the change in any case.
        QDataStream stream(zclFrame.payload());
        stream.setByteOrder(QDataStream::LittleEndian);

        quint16 attrId;
        quint8 status;
        quint8 dataType;

        stream >> attrId;
        stream >> status;
        stream >> dataType;

        if (status == deCONZ::ZclSuccessStatus && attrId == 0x0002 && dataType == deCONZ::Zcl32BitUint && stream.status() == QDataStream::Ok)
        {
            deCONZ::ZclAttribute attr(attrId, dataType, QLatin1String(""), deCONZ::ZclReadWrite, true);

            if (attr.readFromStream(stream))
            {
                swVersion = attr.numericValue().u32;
            }
        }
    }
    else if (zclFrame.isClusterCommand() && zclFrame.commandId() == OTAU_QUERY_NEXT_IMAGE_REQUEST_CMD_ID)
    {
        QDataStream stream(zclFrame.payload());
        stream.setByteOrder(QDataStream::LittleEndian);

        stream >> fieldControl;
        stream >> manufacturerId;
        stream >> imageType;
        stream >> swVersion;

        if (fieldControl & 0x01)
        {
            stream >> hwVersion;
        }

        if (swVersion == 0 || stream.status() != QDataStream::Ok)
        {
            return;
        }
    }

    if (swVersion != 0)
    {
        // the OTA cluster 0x0002 attribute isn't always present, but it can be extracted from the Query Next Image Request
        // store the OTA versions for DDF and non DDF devices, so it can be used in DDF matchexpr

        DB_ZclValue zclVal;
        zclVal.deviceId = device->deviceId();
        zclVal.endpoint = ind.srcEndpoint();
        zclVal.clusterId = ind.clusterId();
        zclVal.attrId = 0x0002; // OTA current file version
        zclVal.data = swVersion;

        DB_StoreZclValue(&zclVal); // does only write if the value is already there

        ResourceItem *item = device->item(RAttrOtaVersion);

        if (item)
        {
            if (item->toNumber() != swVersion)
            {
                item->setValue(swVersion, ResourceItem::SourceDevice);
            }

            if (device->managed() && item->needPushChange())
            {
                // the known OTA version has changed (or initially set)
                // there might be a different DDF to match, trigger reload
                const auto &ddf = DeviceDescriptions::instance()->get(device, DDF_EvalMatchExpr);
                if (ddf.isValid() && !ddf.matchExpr.isEmpty())
                {
                    Event e(device->prefix(), REventDDFReload, 1, device->key());
                    enqueueEvent(e);
                }
            }
        }

        if (device->managed())
        {
            return;
        }

        LightNode *lightNode = getLightNodeForAddress(ind.srcAddress(), ind.srcEndpoint());
        // extract software version from request
        if (lightNode)
        {
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
    else if (zclFrame.isProfileWideCommand())
    {
        return; // all done here
    }
    else if (zclFrame.commandId() == OTAU_UPGRADE_END_REQUEST_CMD_ID)
    {
        LightNode *lightNode = getLightNodeForAddress(ind.srcAddress(), ind.srcEndpoint());

        if (lightNode)
        {
            lightNode->setLastRead(READ_SWBUILD_ID, idleTotalCounter);
            lightNode->enableRead(READ_SWBUILD_ID);
            lightNode->setNextReadTime(READ_SWBUILD_ID, queryTime.addSecs(160));
            storeRecoverOnOffBri(lightNode);
        }
    }
    else if (zclFrame.commandId() == OTAU_IMAGE_BLOCK_REQUEST_CMD_ID)
    {
        // remember last activity time
        otauIdleTotalCounter = idleTotalCounter;
        updateOtaTicks = true;
    }
    else if (zclFrame.commandId() == OTAU_IMAGE_PAGE_REQUEST_CMD_ID)
    {
        updateOtaTicks = true;
    }
    else
    {
        return;
    }

    if (!isOtauActive())
    {
        return;
    }

    if (updateOtaTicks)
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

bool DEV_OtauBusy()
{
    return plugin->isOtauBusy();
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

    if (!isInNetwork())
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
}
