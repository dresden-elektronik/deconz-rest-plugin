/*
 * Copyright (c) 2021-2024 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "deconz/u_assert.h"
#include "device.h"
#include "device_compat.h"
#include "device_descriptions.h"
#include "device_ddf_init.h"
#include "deconz/u_sstream_ex.h"
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
    U_ASSERT(rsub);
    U_ASSERT(ddfItem.isValid());

    auto *item = rsub->item(ddfItem.descriptor.suffix);
    const char *uniqueId = rsub->item(RAttrUniqueId)->toCString();
    U_ASSERT(uniqueId);

    if (item)
    {
        if (DBG_IsEnabled(DBG_INFO_L2))
        {
            DBG_Printf(DBG_DDF, "sub-device: %s, has item: %s\n", uniqueId, ddfItem.descriptor.suffix);
        }
    }
    else
    {
        if (DBG_IsEnabled(DBG_INFO_L2))
        {
            DBG_Printf(DBG_DDF, "sub-device: %s, create item: %s\n", uniqueId, ddfItem.descriptor.suffix);
        }
        item = rsub->addItem(ddfItem.descriptor.type, ddfItem.descriptor.suffix);

        DBG_Assert(item);
        if (!item)
        {
            return nullptr;
        }
    }

    U_ASSERT(item);

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
        else if (item->lastSet().isValid() && item->toVariant() == dbItem->value)
        {
            // nothing to do
        }
        else
        {
            item->setValue(dbItem->value);
            item->setTimeStamps(QDateTime::fromMSecsSinceEpoch(dbItem->timestampMs));
        }
        item->clearNeedStore(); // already in DB
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
            item->clearNeedStore(); // already in DB
        }
    }

    U_ASSERT(ddfItem.handle != DeviceDescription::Item::InvalidItemHandle);
    item->setDdfItemHandle(ddfItem.handle);

    // check updates
    item->setIsPublic(ddfItem.isPublic);
    item->setAwake(ddfItem.awake);

    if (ddfItem.refreshInterval != DeviceDescription::Item::NoRefreshInterval)
    {
        item->setRefreshInterval(deCONZ::TimeSeconds{ddfItem.refreshInterval});
    }

    if (item->refreshInterval().val == 0 && !ddfItem.readParameters.isNull())
    {
        // If a DDF doesn't specify a refresh.interval and also not the generic item
        // default to 30 seconds to relax polling a bit.
        // Note: ideally this should be specified in a DDF/generic item.
        const auto m = ddfItem.readParameters.toMap();
        if (m.value(QLatin1String("fn")) != QLatin1String("none"))
        {
            item->setRefreshInterval(deCONZ::TimeSeconds{30});
        }
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

    if (ddf.storageLocation == deCONZ::DdfBundleLocation || ddf.storageLocation == deCONZ::DdfBundleUserLocation)
    {
        ResourceItem *ddfhashItem = device->item(RAttrDdfHash);
        U_ASSERT(ddfhashItem);

        char buf[72];
        U_SStream ss;
        U_sstream_init(&ss, buf, sizeof(buf));
        U_sstream_put_hex(&ss, &ddf.sha256Hash[0], sizeof(ddf.sha256Hash));

        for (unsigned i = 0; i < ss.pos; i++) // workaround to convert to lower case
        {
            if (buf[i] >= 'A' && buf[i] <= 'F')
                buf[i] = buf[i] + ('a' - 'A');
        }

        U_ASSERT(ss.pos == 64);
        ddfhashItem->setValue(buf, 64);
    }

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
        DB_StoreSubDevice(rsub->item(RAttrUniqueId)->toCString());

        const auto dbItems = DB_LoadSubDeviceItems(rsub->item(RAttrUniqueId)->toLatin1String());

        for (const auto &ddfItem : sub.items)
        {
            auto *item = DEV_InitDeviceDescriptionItem(ddfItem, dbItems, rsub);
            if (!item)
            {
                continue;
            }
            
            if (item->descriptor().suffix == RStatePresence && item->toBool())
            {
                DBG_Printf(DBG_DDF, "sub-device: %s, presence state is true, reverting to false\n", qPrintable(uniqueId));
                item->setValue(false);
                item->clearNeedStore();
            }

            if (item->descriptor().suffix == RConfigGroup)
            {
                DEV_AllocateGroup(device, rsub, item);
            }

            if (!ddfItem.defaultValue.isNull() && !ddfItem.writeParameters.isNull())
            {
                QString writeFunction;
                const auto writeParam = ddfItem.writeParameters.toMap();

                if (writeParam.contains(QLatin1String("fn")))
                {
                    writeFunction = writeParam.value(QLatin1String("fn")).toString();
                }

                if (writeFunction.isEmpty() || writeFunction.startsWith(QLatin1String("zcl")))
                {
                    bool ok;

                    QVariant value = item->toVariant();
                    if (!value.isValid())
                    {
                        value = ddfItem.defaultValue;
                    }

                    StateChange stateChange(StateChange::StateWaitSync, SC_WriteZclAttribute, sub.uniqueId.at(1).toUInt());
                    stateChange.addTargetValue(item->descriptor().suffix, value);
                    stateChange.setChangeTimeoutMs(1000 * 60 * 60);

                    if (writeParam.contains(QLatin1String("state.timeout")))
                    {
                        int stateTimeout = writeParam.value(QLatin1String("state.timeout")).toInt(&ok);

                        if (ok && stateTimeout > 0)
                        {
                            stateChange.setStateTimeoutMs(1000 * stateTimeout);
                        }
                    }

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

        DB_StoreSubDeviceItem(rsub, rsub->item(RAttrManufacturerName));
        DB_StoreSubDeviceItem(rsub, rsub->item(RAttrModelId));
    }

    if (ddf.sleeper >= 0)
    {
        device->item(RCapSleeper)->setValue(ddf.sleeper == 1);
    }

    if (ddf.supportsMgmtBind >= 0)
    {
        device->setSupportsMgmtBind(ddf.supportsMgmtBind == 1);
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

            DeviceDescription::Item ddfItem = dd->getGenericItem(item->descriptor().suffix);

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
    {
        ResourceItem *ddfPolicy = device->item(RAttrDdfPolicy);
        U_ASSERT(ddfPolicy);

        {
            // load attr/ddf_policy and attr/ddf_hash from database if exists

            std::vector<DB_ResourceItem2> dbItems2;

            if (DB_LoadDeviceItems(device->deviceId(), dbItems2))
            {
                for (const auto &dbItem : dbItems2)
                {
                    U_ASSERT(dbItem.valueSize != 0);
                    if (dbItem.valueSize == 0)
                    {
                        continue;
                    }

                    if (dbItem.name == RAttrDdfPolicy && ddfPolicy)
                    {
                        ddfPolicy->setValue(dbItem.value, (int)dbItem.valueSize);
                        ddfPolicy->setTimeStamps(QDateTime::fromMSecsSinceEpoch(dbItem.timestampMs));
                        ddfPolicy->clearNeedStore();
                    }
                    else if (dbItem.name == RAttrDdfHash)
                    {
                        U_ASSERT(dbItem.valueSize == 64);
                        if (dbItem.valueSize == 64)
                        {
                            ResourceItem *ddfHash = device->item(RAttrDdfHash);
                            ddfHash->setValue(dbItem.value, (int)dbItem.valueSize);
                            ddfHash->setTimeStamps(QDateTime::fromMSecsSinceEpoch(dbItem.timestampMs));
                            ddfHash->clearNeedStore();
                        }
                    }
                }
            }
        }

        // if no attr/ddf_policy is set, use the default
        if (ddfPolicy && ddfPolicy->toLatin1String().size() == 0)
        {
            ddfPolicy->setValue("latest_prefer_stable", -1);

            // DB_ResourceItem2 dbItem;

            // if (DB_ResourceItem2DbItem(ddfPolicy, &dbItem))
            // {
            //     if (DB_StoreDeviceItem(device->deviceId(), dbItem))
            //     {

            //     }
            // }
        }
    }

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
                if (dbItem.value.toBool())
                {
                    auto dt = QDateTime::fromMSecsSinceEpoch(dbItem.timestampMs);
                    if (86400 < dt.secsTo(QDateTime::currentDateTime()))
                    {
                        reachable->setValue(false);
                        continue;
                    }
                }

                reachable->setValue(dbItem.value.toBool());
                reachable->setTimeStamps(QDateTime::fromMSecsSinceEpoch(dbItem.timestampMs));
                reachable->clearNeedStore();
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
                item->clearNeedStore();
                found++;
            }

            break;
        }
    }

    DB_ZclValue zclVal;
    zclVal.deviceId = device->deviceId();
    zclVal.endpoint = 0;
    zclVal.clusterId = 0x0019; // OTA cluster
    zclVal.attrId = 0x0002; // OTA current file version
    zclVal.data = 0;

    if (DB_LoadZclValue(&zclVal) && zclVal.data != 0)
    {
        ResourceItem *item = device->item(RAttrOtaVersion);
        if (item && item->toNumber() != zclVal.data)
        {
            item->setValue(zclVal.data, ResourceItem::SourceDevice);
            item->clearNeedPush();
        }
    }

    zclVal.clusterId = 0x0500; // IAS Zone cluster
    zclVal.attrId = 0x0001; // IAS Zone Type
    zclVal.data = 0;

    if (DB_LoadZclValue(&zclVal) && zclVal.data != 0)
    {
        ResourceItem *item = device->addItem(DataTypeUInt16, RAttrZoneType);
        if (item && item->toNumber() != zclVal.data)
        {
            item->setValue(zclVal.data, ResourceItem::SourceDevice);
            item->clearNeedPush();
        }
    }

    return found == poi.size();
}
