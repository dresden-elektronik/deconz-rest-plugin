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
#include "poll_control.h"
#include "utils/utils.h"

void DEV_AllocateGroup(const Device *device, Resource *rsub, ResourceItem *item);

static QString uniqueIdFromTemplate(const QStringList &templ, const Device *device)
{
    bool ok = false;
    quint8 endpoint = 0;
    quint16 clusterId = 0;
    quint64 extAddress;

    extAddress = device->item(RAttrExtAddress)->toNumber();

    // <mac>-<endpoint>
    // <mac>-<endpoint>-<cluster>

    /* supported templates

       ["$address.ext", <endpoint>]
       ["$address.ext", <endpoint>, <cluster>]
       ["$address.ext", <endpoint>, "out.cluster", <cluster1>, <cluster2>, ...]
       ["$address.ext", <endpoint>, "in.cluster", <cluster1>, <cluster2>, ...]
    */

    if (templ.size() > 1 && templ.first() == QLatin1String("$address.ext"))
    {
        endpoint = templ.at(1).toUInt(&ok, 0);

        if (ok && templ.size() > 2)
        {
            ok = false;
            const QString pos2 = templ.at(2);
            if (pos2.at(0).isDigit())
            {
                clusterId = pos2.toUInt(&ok, 0);
            }
            else if (device->node() && (pos2 == QLatin1String("out.cluster") || pos2 == QLatin1String("in.cluster")))
            {
                // select clusterId if endpoint contains cluster from list
                // the cluster list, ordered by priority
                const auto clusterSide = pos2.at(0) == 'o' ? deCONZ::ClientCluster : deCONZ::ServerCluster;
                const  auto &simpleDescriptors = device->node()->simpleDescriptors();
                const auto sd = std::find_if(simpleDescriptors.cbegin(), simpleDescriptors.cend(),
                                             [endpoint](const auto &x) { return x.endpoint() == endpoint; });
                if (sd != simpleDescriptors.cend())
                {
                    const auto &clusters = sd->clusters(clusterSide);

                    for (int i = 3; i < templ.size(); i++)
                    {
                        clusterId = templ.at(i).toUInt(&ok, 0);
                        if (!ok) { break; } // no clusterId, maybe in future other commands follow (doh)

                        const auto cl = std::find_if(clusters.cbegin(), clusters.cend(),
                                                     [clusterId](const auto &x){ return x.id() == clusterId; });

                        ok = cl != clusters.cend();
                        if (ok)
                        {
                            break;
                        }
                    }
                }
            }
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
static ResourceItem *DEV_InitDeviceDescriptionItem(const DeviceDescription::Item &ddfItem, const std::vector<DB_ResourceItem> &dbItems, Resource *rsub)
{
    Q_ASSERT(rsub);
    Q_ASSERT(ddfItem.isValid());

    auto *item = rsub->item(ddfItem.descriptor.suffix);
    const char *uniqueId = rsub->item(RAttrUniqueId)->toCString();
    Q_ASSERT(uniqueId);

    if (item)
    {
        DBG_Printf(DBG_DDF, "sub-device: %s, has item: %s\n", uniqueId, ddfItem.descriptor.suffix);
    }
    else
    {
        DBG_Printf(DBG_DDF, "sub-device: %s, create item: %s\n", uniqueId, ddfItem.descriptor.suffix);
        item = rsub->addItem(ddfItem.descriptor.type, ddfItem.descriptor.suffix);

        DBG_Assert(item);
        if (!item)
        {
            return nullptr;
        }
    }

    Q_ASSERT(item);

    const auto dbItem = std::find_if(dbItems.cbegin(), dbItems.cend(), [&ddfItem](const auto &dbItem)
    {
        return ddfItem.name == dbItem.name;
    });

    if (!ddfItem.isStatic && dbItem != dbItems.cend())
    {
        if (item->descriptor().suffix == RAttrId && !item->toString().isEmpty())
        {
            // keep 'id', it might have been loaded from legacy db
            // and will be updated in 'resource_items' table on next write
        }
        else
        {
            item->setValue(dbItem->value);
            item->setTimeStamps(QDateTime::fromMSecsSinceEpoch(dbItem->timestampMs));
        }
    }
    else if (!ddfItem.isStatic && dbItem == dbItems.cend() && !item->lastSet().isValid())
    {
        // try load from legacy sensors/nodes db tables
        auto dbLegacyItem = std::make_unique<DB_LegacyItem>();
        dbLegacyItem->uniqueId = uniqueId;
        dbLegacyItem->column.setString(item->descriptor().suffix);

        if (rsub->prefix() == RSensors)
        {
            DB_LoadLegacySensorValue(dbLegacyItem.get());
        }
        // TODO lights needs some more investigation, might not be needed..
//        else if (rsub->prefix() == RLights)
//        {
//            DB_LoadLegacyLightValue(dbLegacyItem.get());
//        }

        if (!dbLegacyItem->value.empty())
        {
            item->setValue(QVariant(QString(dbLegacyItem->value.c_str())));
            item->setTimeStamps(item->lastSet().addSecs(-120)); // TODO extract from 'lastupdated'?
        }
    }

    if (ddfItem.defaultValue.isValid())
    {
        if (ddfItem.isStatic || !item->lastSet().isValid())
        {
            item->setValue(ddfItem.defaultValue);
        }
    }

    assert(ddfItem.handle != DeviceDescription::Item::InvalidItemHandle);
    item->setDdfItemHandle(ddfItem.handle);

    // check updates
    item->setIsPublic(ddfItem.isPublic);
    item->setAwake(ddfItem.awake);

    if (ddfItem.refreshInterval != DeviceDescription::Item::NoRefreshInterval)
    {
        item->setRefreshInterval(deCONZ::TimeSeconds{ddfItem.refreshInterval});
    }

    item->setParseFunction(nullptr);

    return item;
}

/*! Creates and initializes sub-device Resources and ResourceItems if not already present.

    This function replaces the legacy database loading and joining device initialization.
 */
bool DEV_InitDeviceFromDescription(Device *device, const DeviceDescription &ddf)
{
    Q_ASSERT(device);
    Q_ASSERT(ddf.isValid());

    size_t subCount = 0;
    auto *dd = DeviceDescriptions::instance();

    for (const auto &sub : ddf.subDevices)
    {
        Q_ASSERT(sub.isValid());

        const auto uniqueId = uniqueIdFromTemplate(sub.uniqueId, device);
        if (uniqueId.isEmpty())
        {
            DBG_Printf(DBG_DDF, "failed to init sub-device uniqueid: %s, %s\n", qPrintable(sub.uniqueId.join('-')), qPrintable(sub.type));
            return false;
        }

        Resource *rsub = DEV_GetSubDevice(device, nullptr, uniqueId);

        if (!rsub)
        {
            rsub = DEV_InitCompatNodeFromDescription(device, ddf, sub, uniqueId);
        }

        if (!rsub)
        {
            DBG_Printf(DBG_DDF, "sub-device: %s, failed to setup: %s\n", qPrintable(uniqueId), qPrintable(sub.type));
            return false;
        }

        subCount++;

        auto *mf = rsub->item(RAttrManufacturerName);
        if (mf && mf->toLatin1String().size() == 0)
        {
            mf->setValue(dd->constantToString(device->item(RAttrManufacturerName)->toString()));
        }

        // TODO storing should be done else where, since this is init code
        DB_StoreSubDevice(device->item(RAttrUniqueId)->toLatin1String(), rsub->item(RAttrUniqueId)->toString());
        DB_StoreSubDeviceItem(rsub, rsub->item(RAttrManufacturerName));
        DB_StoreSubDeviceItem(rsub, rsub->item(RAttrModelId));

        const auto dbItems = DB_LoadSubDeviceItems(rsub->item(RAttrUniqueId)->toLatin1String());

        for (const auto &ddfItem : sub.items)
        {
            auto *item = DEV_InitDeviceDescriptionItem(ddfItem, dbItems, rsub);
            if (!item)
            {
                continue;
            }

            if (!ddfItem.defaultValue.isNull() && !ddfItem.writeParameters.isNull())
            {
                QString writeFunction;
                {
                    const auto writeParam = ddfItem.writeParameters.toMap();
                    if (writeParam.contains(QLatin1String("fn")))
                    {
                        writeFunction = writeParam.value(QLatin1String("fn")).toString();
                    }
                }

                if (writeFunction.isEmpty() || writeFunction == QLatin1String("zcl"))
                {
                    StateChange stateChange(StateChange::StateWaitSync, SC_WriteZclAttribute, sub.uniqueId.at(1).toUInt());
                    stateChange.addTargetValue(item->descriptor().suffix, item->toVariant());
                    stateChange.setChangeTimeoutMs(1000 * 60 * 60);
                    rsub->addStateChange(stateChange);
                }
            }

            // DDF enforce sub device "type" (enables overwriting the type from C++ code)
            if (item->descriptor().suffix == RAttrType)
            {
                const QString type = DeviceDescriptions::instance()->constantToString(sub.type);
                if (type != item->toString() && !type.startsWith('$'))
                {
                    item->setValue(type);
                }
            }

            if (item->descriptor().suffix == RConfigGroup)
            {
                DEV_AllocateGroup(device, rsub, item);
            }

            if (item->descriptor().suffix == RConfigBattery || item->descriptor().suffix == RStateBattery)
            {
                DEV_ForwardNodeChange(device, QLatin1String(item->descriptor().suffix), QString::number(item->toNumber()));
            }

            if (item->descriptor().suffix == RConfigCheckin)
            {
                if (PC_GetPollControlEndpoint(device->node()) > 0)
                {
                    auto *itemPending = rsub->item(RConfigPending);
                    if (itemPending) // TODO long poll interval via StateChange
                    {
                        itemPending->setValue(itemPending->toNumber() | R_PENDING_SET_LONG_POLL_INTERVAL);
                    }
                }
            }
        }
    }

    if (ddf.sleeper >= 0)
    {
        device->item(RAttrSleeper)->setValue(ddf.sleeper == 1);
    }

    device->clearBindings();
    for (const auto &bnd : ddf.bindings)
    {
        device->addBinding(bnd);
    }

    return subCount == ddf.subDevices.size();
}

bool DEV_InitBaseDescriptionForDevice(Device *device, DeviceDescription &ddf)
{
    ddf.status = QLatin1String("Draft");
    ddf.manufacturerNames.push_back(device->item(RAttrManufacturerName)->toString());
    ddf.modelIds.push_back(device->item(RAttrModelId)->toString());

    if (ddf.manufacturerNames.last().isEmpty() || ddf.modelIds.isEmpty() || ddf.modelIds.front().isEmpty())
    {
        return false;
    }

    const auto *dd = DeviceDescriptions::instance();

    for (const Resource *r : device->subDevices())
    {
        DeviceDescription::SubDevice sub;

        sub.type = dd->stringToConstant(r->item(RAttrType)->toString());
        sub.restApi = QLatin1String(r->prefix());

        if (ddf.product.isEmpty())
        {
            const ResourceItem *productId = r->item(RAttrProductId);
            if (productId && !productId->toString().isEmpty())
            {
                ddf.product = productId->toString();
            }
        }

        {
            QString uniqueId = r->item(RAttrUniqueId)->toString();
            QStringList ls = uniqueId.split('-', SKIP_EMPTY_PARTS);

            // this is a bit fugly but uniqueid template must be valid
            DBG_Assert(ls.size() > 1); // lights uniqueid
            if (ls.size() > 1)
            {
                ls[0] = QLatin1String("$address.ext");

                ls[1] = "0x" + ls[1];
                if (ls.size() > 2) // sensor uniqueid
                {
                    ls[2] = "0x" + ls[2];
                }
                sub.uniqueId = ls;
            }
            else
            {
                return false;
            }
        }

        if (r->prefix() == RSensors)
        {
            const Sensor *sensor = dynamic_cast<const Sensor*>(r);
            if (sensor)
            {
                sub.fingerPrint = sensor->fingerPrint();
            }
        }

        for (int i = 0; i < r->itemCount(); i++)
        {
            const ResourceItem *item = r->itemForIndex(i);

            DeviceDescription::Item ddfItem = DeviceDescriptions::instance()->getItem(item);

            if (!ddfItem.isValid())
            {
                // create some
                ddfItem.name = item->descriptor().suffix;
                ddfItem.descriptor = item->descriptor();
            }

            ddfItem.isPublic = item->isPublic();

            sub.items.push_back(ddfItem);
        }

        ddf.subDevices.push_back(sub);
    }

    return true;
}


bool DEV_InitDeviceBasic(Device *device)
{
    const auto dbItems = DB_LoadSubDeviceItemsOfDevice(device->item(RAttrUniqueId)->toLatin1String());

    size_t found = 0;
    std::array<const char*, 2> poi = { RAttrManufacturerName, RAttrModelId };
    for (const auto &dbItem : dbItems)
    {
        if (dbItem.name == RStateReachable || dbItem.name == RConfigReachable)
        {
            ResourceItem *reachable = device->item(RStateReachable);
            DBG_Assert(reachable);
            if (reachable)
            {
                reachable->setValue(dbItem.value.toBool());
                reachable->setTimeStamps(QDateTime::fromMSecsSinceEpoch(dbItem.timestampMs));
            }
            continue;
        }

        for (const char *suffix: poi)
        {
            if (dbItem.name != suffix)
            {
                continue;
            }

            auto *item = device->item(suffix);

            if (item)
            {
                item->setValue(dbItem.value);
                item->setTimeStamps(QDateTime::fromMSecsSinceEpoch(dbItem.timestampMs));
                found++;
            }

            break;
        }
    }

    return found == poi.size();
}
