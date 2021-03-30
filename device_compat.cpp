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
#include "resource.h"
#include "sensor.h"
#include "light_node.h"

int getFreeSensorId();

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
    sensor.setModelId(device->item(RAttrModelId)->toString());
    sensor.setType(DeviceDescriptions::instance()->constantToString(sub.type));
    sensor.setUniqueId(uniqueId);
    sensor.setNode(const_cast<deCONZ::Node*>(device->node()));
    R_SetValue(&sensor, RConfigOn, true, ResourceItem::SourceApi);

    QString friendlyName = sensor.type();
    if (friendlyName.startsWith("ZHA") || friendlyName.startsWith("ZLL"))
    {
        friendlyName = friendlyName.mid(3);
    }

    sensor.setId(QString::number(getFreeSensorId()));
    sensor.setName(QString("%1 %2").arg(friendlyName, sensor.id()));

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

    lightNode.address().setExt(device->item(RAttrExtAddress)->toNumber());
    lightNode.address().setNwk(device->item(RAttrNwkAddress)->toNumber());
    lightNode.setModelId(device->item(RAttrModelId)->toString());
    lightNode.setManufacturerName(device->item(RAttrManufacturerName)->toString());
    lightNode.setManufacturerCode(device->node()->nodeDescriptor().manufacturerCode());
    lightNode.setNode(const_cast<deCONZ::Node*>(device->node())); // TODO this is evil

    lightNode.item(RAttrType)->setValue(DeviceDescriptions::instance()->constantToString(sub.type));
    lightNode.setUniqueId(uniqueId);
    lightNode.setNode(const_cast<deCONZ::Node*>(device->node()));

    lightNode.setId(QString::number(getFreeSensorId()));
    lightNode.setName(QString("%1 %2").arg(lightNode.type(), lightNode.id()));

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
