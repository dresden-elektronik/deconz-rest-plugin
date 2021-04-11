/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "device.h"
#include "device_compat.h"
#include "device_descriptions.h"
#include "device_ddf_init.h"
#include "database.h"

// TODO move external declaration in de_web_plugin_private.h into utils.h
QString generateUniqueId(quint64 extAddress, quint8 endpoint, quint16 clusterId);

static QString uniqueIdFromTemplate(const QStringList &templ, const quint64 extAddress)
{
    bool ok = false;
    quint8 endpoint = 0;
    quint16 clusterId = 0;

    // <mac>-<endpoint>
    // <mac>-<endpoint>-<cluster>
    if (templ.size() > 1 && templ.first() == QLatin1String("$address.ext"))
    {
        endpoint = templ.at(1).toUInt(&ok, 0);

        if (ok && templ.size() > 2)
        {
            clusterId = templ.at(2).toUInt(&ok, 0);
        }
    }

    if (ok)
    {
        return generateUniqueId(extAddress, endpoint, clusterId);
    }

    return {};
}

/*! Creates a ResourceItem if not exist, initialized with \p ddfItem content.
 */
static ResourceItem *DEV_InitDeviceDescriptionItem(const DeviceDescription::Item &ddfItem, Resource *rsub)
{
    Q_ASSERT(rsub);
    Q_ASSERT(ddfItem.isValid());

    auto *item = rsub->item(ddfItem.descriptor.suffix);
    const auto uniqueId = rsub->item(RAttrUniqueId)->toString();

    if (item)
    {
        DBG_Printf(DBG_INFO, "sub-device: %s, has item: %s\n", qPrintable(uniqueId), ddfItem.descriptor.suffix);
    }
    else
    {
        DBG_Printf(DBG_INFO, "sub-device: %s, create item: %s\n", qPrintable(uniqueId), ddfItem.descriptor.suffix);
        item = rsub->addItem(ddfItem.descriptor.type, ddfItem.descriptor.suffix);

        if (!item)
        {
            return nullptr;
        }

        if (ddfItem.defaultValue.isValid())
        {
            item->setValue(ddfItem.defaultValue);
        }
    }

    Q_ASSERT(item);

    // check updates
    item->setIsPublic(ddfItem.isPublic);
    item->setAwake(ddfItem.awake);

    if (ddfItem.refreshInterval >= 0)
    {
        item->setRefreshInterval(ddfItem.refreshInterval);
    }

    if (item->parseParameters() != ddfItem.parseParameters)
    {
        item->setParseFunction(nullptr);
        item->setParseParameters(ddfItem.parseParameters);
    }

    if (item->readParameters() != ddfItem.readParameters)
    {
        item->setReadParameters(ddfItem.readParameters);
    }

    if (item->writeParameters() != ddfItem.writeParameters)
    {
        item->setWriteParameters(ddfItem.writeParameters);
    }

    return item;
}

/*! Creates and initialises sub-device Resources and ResourceItems if not already present.

    This function can replace database and joining device initialisation.
 */
bool DEV_InitDeviceFromDescription(Device *device, const DeviceDescription &description)
{
    Q_ASSERT(device);
    Q_ASSERT(description.isValid());

    size_t subCount = 0;

    for (const auto &sub : description.subDevices)
    {
        Q_ASSERT(sub.isValid());

        const auto uniqueId = uniqueIdFromTemplate(sub.uniqueId, device->item(RAttrExtAddress)->toNumber());
        Resource *rsub = DEV_GetSubDevice(device, nullptr, uniqueId);

        if (!rsub)
        {
            rsub = DEV_InitCompatNodeFromDescription(device, sub, uniqueId);
        }

        if (!rsub)
        {
            DBG_Printf(DBG_INFO, "sub-device: %s, failed to setup: %s\n", qPrintable(uniqueId), qPrintable(sub.type));
            return false;
        }

        subCount++;

        auto *mf = rsub->item(RAttrManufacturerName);
        if (mf && mf->toString().isEmpty())
        {
            mf->setValue(DeviceDescriptions::instance()->constantToString(description.manufacturer));
        }

        for (const auto &ddfItem : sub.items)
        {
            auto *item = DEV_InitDeviceDescriptionItem(ddfItem, rsub);
            if (!item)
            {
                continue;
            }

            if (item->descriptor().suffix == RConfigCheckin)
            {
                StateChange stateChange(StateChange::StateWaitSync, SC_WriteZclAttribute, sub.uniqueId.at(1).toUInt());
                stateChange.addTargetValue(RConfigCheckin, ddfItem.defaultValue);
                stateChange.setChangeTimeoutMs(1000 * 60 * 60);
                rsub->addStateChange(stateChange);
            }
        }

        DB_StoreSubDevice(rsub);
    }

    if (description.sleeper >= 0)
    {
        device->item(RAttrSleeper)->setValue(description.sleeper == 1);
    }

    return subCount == description.subDevices.size();
}

