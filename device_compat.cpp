/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <memory>
#include "database.h"
#include "device.h"
#include "device_compat.h"
#include "resource.h"
#include "sensor.h"
#include "light_node.h"

int getFreeSensorId();
int getFreeLightId();

#define READ_GROUPS            (1 << 5) // from web_plugin_private.h

/*! Overloads to add specific resources in higher layer.
    Since Device class doesn't know anything about web plugin or testing code,
    this is a free standing function which needs to be implemented in the higher layer.
*/
Resource *DEV_AddResource(const Sensor &sensor);
Resource *DEV_AddResource(const LightNode &lightNode);

/*! V1 compatibility function to create SensorNodes based on sub-device description.
 */
static Resource *DEV_InitSensorNodeFromDescription(Device *device, const DeviceDescription::SubDevice &sub, const QString &uniqueId)
{
    Sensor sensor;

    sensor.fingerPrint() = sub.fingerPrint;
    sensor.address().setExt(device->item(RAttrExtAddress)->toNumber());
    sensor.address().setNwk(device->item(RAttrNwkAddress)->toNumber());
    sensor.setModelId(device->item(RAttrModelId)->toCString());
    sensor.setManufacturer(device->item(RAttrManufacturerName)->toCString());
    sensor.setType(DeviceDescriptions::instance()->constantToString(sub.type));
    sensor.setUniqueId(uniqueId);
    sensor.setNode(const_cast<deCONZ::Node*>(device->node()));
    R_SetValue(&sensor, RConfigOn, true, ResourceItem::SourceApi);

    auto dbItem = std::make_unique<DB_LegacyItem>();
    dbItem->uniqueId = sensor.item(RAttrUniqueId)->toCString();

    {
        dbItem->column = "sid";

        if (DB_LoadLegacySensorValue(dbItem.get()))
        {
            sensor.setId(toLatin1String(dbItem->value));
        }
        else
        {
            sensor.setId(QString::number(getFreeSensorId()));
        }
    }

    {
        dbItem->column = "name";
        if (DB_LoadLegacySensorValue(dbItem.get()))
        {
            sensor.setName(dbItem->value.c_str());
        }
        else
        {
            QString friendlyName = sensor.type();
            if (friendlyName.startsWith("ZHA") || friendlyName.startsWith("ZLL"))
            {
                friendlyName = friendlyName.mid(3);
            }
            sensor.setName(QString("%1 %2").arg(friendlyName, sensor.id()));
        }
    }

    sensor.setNeedSaveDatabase(true);
    sensor.rx();

    auto *r = DEV_AddResource(sensor);
    Q_ASSERT(r);

    device->addSubDevice(r);

    return r;
}

/*! V1 compatibility function to create LightsNode based on sub-device description.
 */
static Resource *DEV_InitLightNodeFromDescription(Device *device, const DeviceDescription::SubDevice &sub, const QString &uniqueId)
{
    LightNode lightNode;

    {
        const auto ls = uniqueId.split('-', SKIP_EMPTY_PARTS);
        if (ls.size() >= 2 && device->node())
        {
            bool ok;
            const uint ep = ls[1].toUInt(&ok, 16);
            deCONZ::SimpleDescriptor sd;
            if (device->node()->copySimpleDescriptor(ep, &sd) == 0)
            {
                lightNode.setHaEndpoint(sd);
            }
        }
    }

    lightNode.address().setExt(device->item(RAttrExtAddress)->toNumber());
    lightNode.address().setNwk(device->item(RAttrNwkAddress)->toNumber());
    lightNode.setModelId(device->item(RAttrModelId)->toCString());
    lightNode.setManufacturerName(device->item(RAttrManufacturerName)->toCString());
    lightNode.setManufacturerCode(device->node()->nodeDescriptor().manufacturerCode());
    lightNode.setNode(const_cast<deCONZ::Node*>(device->node())); // TODO this is evil

    lightNode.item(RAttrType)->setValue(DeviceDescriptions::instance()->constantToString(sub.type));
    lightNode.setUniqueId(uniqueId);
    lightNode.enableRead(READ_GROUPS);

    auto dbItem = std::make_unique<DB_LegacyItem>();
    dbItem->uniqueId = lightNode.item(RAttrUniqueId)->toCString();

    {
        dbItem->column = "id";

        if (DB_LoadLegacyLightValue(dbItem.get()))
        {
            lightNode.setId(toLatin1String(dbItem->value));
        }
        else
        {
            lightNode.setId(QString::number(getFreeLightId()));
        }
    }

    {
        dbItem->column = "name";
        if (DB_LoadLegacyLightValue(dbItem.get()))
        {
            lightNode.setName(dbItem->value.c_str());
        }
        else
        {
            lightNode.setName(QString("%1 %2").arg(lightNode.type(), lightNode.id()));
        }
    }

    {
        dbItem->column = "groups";
        if (DB_LoadLegacyLightValue(dbItem.get()))
        {
            const auto groupList = QString(static_cast<QLatin1String>(dbItem->value)).split(',', SKIP_EMPTY_PARTS);

            for (const auto &g : groupList)
            {
                bool ok = false;
                uint gid = g.toUShort(&ok, 0);
                if (!ok) { continue; }

                auto i = std::find_if(lightNode.groups().cbegin(), lightNode.groups().cend(), [gid](const auto &group)
                {
                    return gid == group.id;
                });

                if (i == lightNode.groups().cend())
                {
                    GroupInfo groupInfo;
                    groupInfo.id = gid;
                    groupInfo.state = GroupInfo::StateInGroup;
                    lightNode.groups().push_back(groupInfo);
                }
            }
        }
    }

    // remove some items which need to be specified via DDF
    lightNode.removeItem(RStateOn);
    lightNode.removeItem(RStateBri);
    lightNode.removeItem(RStateHue);
    lightNode.removeItem(RStateSat);
    lightNode.removeItem(RStateAlert);

    lightNode.setNeedSaveDatabase(true);
    lightNode.rx();

    auto *r = DEV_AddResource(lightNode);
    Q_ASSERT(r);

    device->addSubDevice(r);

    return r;
}

/*! Creates Sensor and LightNode based on sub-device description.

    The purpose of this function is to hide Sensor and LightNode classes from Device code.

    \returns Resource pointer of the related node.
 */
Resource *DEV_InitCompatNodeFromDescription(Device *device, const DeviceDescription::SubDevice &sub, const QString &uniqueId)
{
    if (sub.restApi == QLatin1String("/sensors"))
    {
        return DEV_InitSensorNodeFromDescription(device, sub, uniqueId);
    }
    else if (sub.restApi == QLatin1String("/lights"))
    {
        return DEV_InitLightNodeFromDescription(device, sub, uniqueId);
    }

    return nullptr;
}
