/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QBasicTimer>
#include <QElapsedTimer>
#include <QTimerEvent>
#include <array>
#include <tuple>
#include <deconz.h>
#include "device.h"
#include "device_access_fn.h"
#include "device_descriptions.h"
#include "event.h"
#include "zdp.h"
#include "sensor.h"
#include "light_node.h"

// TODO move external declaration in de_web_plugin_private.h into utils.h
int getFreeSensorId();
QString generateUniqueId(quint64 extAddress, quint8 endpoint, quint16 clusterId);

// enable domain specific string literals
using namespace deCONZ::literals;

/* PlantUML state chart

@startuml
hide empty description
state Init
state "Node Descriptor" as NodeDescriptor
state Endpoints as "Endpoints"
state "Simple Descriptors" as SimpleDescriptors
state "Basic Cluster" as BasicCluster
state "Get DDF" as GetDDF

[*] --> Init
Init --> NodeDescriptor : Reachable or\nHas node Descriptor

NodeDescriptor --> Init : Error
NodeDescriptor --> Endpoints : Has Node Descriptor

Endpoints --> SimpleDescriptors : Has Active Endpoints
Endpoints --> Init : Error

SimpleDescriptors --> BasicCluster : Has Simple Descriptors
SimpleDescriptors --> Init : Error

BasicCluster --> GetDDF
BasicCluster --> Init : Error
note bottom of BasicCluster : read common attributes


GetDDF --> Init : Not found
GetDDF --> Operating : Has DDF

state Operating {
  state Bindings
  ||
  state Scenes
  ||
  state "..."
}


Operating --> Init : Not Reachable
@enduml

*/

constexpr int MinMacPollRxOn = 8000; // 7680 ms + some space for timeout

class DevicePrivate
{
public:
    /*! sub-devices are not yet referenced via pointers since these may become dangling.
        This is a helper to query the actual sub-device Resource* on demand.

        {uniqueid, (RSensors | RLights)}
    */
    std::vector<std::tuple<QString, const char*>> subDevices;
    const deCONZ::Node *node = nullptr; //! a reference to the deCONZ core node
    DeviceKey deviceKey = 0; //! for physical devices this is the MAC address

    /*! The currently active state handler function(s).
        Indexes >0 represent sub states of StateLevel0 running in parallel.
    */
    std::array<DeviceStateHandler, 2> state{0};

    QBasicTimer timer; //! internal single shot timer
    QElapsedTimer awake; //! time to track when an end-device was last awake
    QElapsedTimer bindingVerify; //! time to track last binding table verification
    size_t bindingIter = 0;
    bool mgmtBindSupported = false;
    bool managed = false; //! a managed device doesn't rely on legacy implementation of polling etc.
};


/*! Returns deCONZ core node for a given \p extAddress.
 */
const deCONZ::Node *DEV_GetCoreNode(uint64_t extAddress)
{
    int i = 0;
    const deCONZ::Node *node = nullptr;
    deCONZ::ApsController *ctrl = deCONZ::ApsController::instance();

    while (ctrl->getNode(i, &node) == 0)
    {
        if (node->address().ext() == extAddress)
        {
            return node;
        }
        i++;
    }

    return nullptr;
}

void DEV_EnqueueEvent(Device *device, const char *event)
{
    Q_ASSERT(device);
    Q_ASSERT(event);
    emit device->eventNotify(Event(device->prefix(), event, 0, device->key()));
}

Resource *DEV_GetSubDevice(Device *device, const char *prefix, const QString &identifier)
{
    for (auto &sub : device->subDevices())
    {
        if (prefix && sub->prefix() != prefix)
        {
            continue;
        }

        if (sub->item(RAttrUniqueId)->toString() == identifier || sub->item(RAttrId)->toString() == identifier)
        {
            return sub;
        }
    }

    return nullptr;
}

void DEV_InitStateHandler(Device *device, const Event &event)
{
    if (event.what() != RAttrLastSeen)
    {
        DBG_Printf(DBG_INFO, "DEV Init event %s/0x%016llX/%s\n", event.resource(), event.deviceKey(), event.what());
    }

    if (event.what() == REventPoll ||
        event.what() == REventAwake ||
        event.what() == RConfigReachable ||
        event.what() == REventStateTimeout ||
        event.what() == RStateLastUpdated)
    {
        // lazy reference to deCONZ::Node
        if (!device->node())
        {
            device->d->node = DEV_GetCoreNode(device->key());
        }

        if (device->node())
        {
            device->item(RAttrExtAddress)->setValue(device->node()->address().ext());
            device->item(RAttrNwkAddress)->setValue(device->node()->address().nwk());

            if (device->node()->nodeDescriptor().manufacturerCode_t() == 0x1135_mfcode && device->node()->address().nwk() == 0x0000)
            {
                return; // ignore coordinaor for now
            }

            // got a node, jump to verification
            if (!device->node()->nodeDescriptor().isNull() || device->reachable())
            {
                device->setState(DEV_NodeDescriptorStateHandler);
            }
        }
        else
        {
            DBG_Printf(DBG_INFO, "DEV Init no node found: 0x%016llX\n", event.deviceKey());
        }
    }
}

void DEV_CheckItemChanges(Device *device, const Event &event)
{
    std::vector<Resource*> subDevices;

    if (event.what() == REventAwake || event.what() == REventPoll)
    {
        subDevices = device->subDevices();
    }
    else
    {
        auto *sub = DEV_GetSubDevice(device, event.resource(), event.id());
        if (sub)
        {
            subDevices.push_back(sub);
        }
    }


    for (auto *sub : subDevices)
    {
        if (sub && !sub->stateChanges().empty())
        {
            auto *item = sub->item(event.what());
            for (auto &change : sub->stateChanges())
            {
                if (item)
                {
                    change.verifyItemChange(item);
                }
                change.tick(sub, deCONZ::ApsController::instance());
            }

            sub->cleanupStateChanges();
        }
    }
}

/*! #7 In this state the device is operational and runs sub states
    In parallel.

    IdleState : Bindings | ItemChange
 */
void DEV_IdleStateHandler(Device *device, const Event &event)
{   
    if (event.what() == RAttrLastSeen /*|| event.what() == REventPoll*/)
    {
         // don't print logs
    }
    else if (event.what() == REventStateEnter)
    {
        device->setState(DEV_BindingHandler, StateLevel1);
    }
    else if (event.what() == REventStateLeave)
    {
        device->setState(nullptr, StateLevel1);
    }
    else if (event.resource() == device->prefix())
    {
        DBG_Printf(DBG_INFO, "DEV Idle event %s/0x%016llX/%s\n", event.resource(), event.deviceKey(), event.what());
    }
    else
    {
        DBG_Printf(DBG_INFO, "DEV Idle event %s/0x%016llX/%s\n", event.resource(), event.deviceKey(), event.what());
    }

    DEV_CheckItemChanges(device, event);

    // process parallel states
    if (event.what() != REventStateEnter && event.what() != REventStateLeave)
    {
        device->handleEvent(event, StateLevel1);
    }
}

/*! #2 This state checks that a valid NodeDescriptor is available.
 */
void DEV_NodeDescriptorStateHandler(Device *device, const Event &event)
{
    if (event.what() == REventStateEnter)
    {
        if (!device->node()->nodeDescriptor().isNull())
        {
            DBG_Printf(DBG_INFO, "ZDP node descriptor verified: 0x%016llX\n", device->key());
            device->setState(DEV_ActiveEndpointsStateHandler);
        }
        else if (!device->reachable()) // can't be queried, go back to #1 init
        {
            device->setState(DEV_InitStateHandler);
        }
        else
        {
            auto zdpResult = ZDP_NodeDescriptorReq(device->item(RAttrNwkAddress)->toNumber(), deCONZ::ApsController::instance());
            if (zdpResult.isEnqueued)
            {

            }

            device->startStateTimer(MinMacPollRxOn);
        }
    }
    else if (event.what() == REventNodeDescriptor) // received the node descriptor
    {
        device->stopStateTimer();
        device->setState(DEV_InitStateHandler); // evaluate egain from state #1 init
        DEV_EnqueueEvent(device, REventAwake);
    }
    else if (event.what() == REventStateTimeout)
    {
        DBG_Printf(DBG_INFO, "read ZDP node descriptor timeout: 0x%016llX\n", device->key());
        device->setState(DEV_InitStateHandler);
    }
}

/*! #3 This state checks that active endpoints are known.
 */
void DEV_ActiveEndpointsStateHandler(Device *device, const Event &event)
{
    if (event.what() == REventStateEnter)
    {
        if (!device->node()->endpoints().empty())
        {
            DBG_Printf(DBG_INFO, "ZDP active endpoints verified: 0x%016llX\n", device->key());
            device->setState(DEV_SimpleDescriptorStateHandler);
        }
        else if (!device->reachable())
        {
            device->setState(DEV_InitStateHandler);
        }
        else
        {
            ZDP_ActiveEndpointsReq(device->item(RAttrNwkAddress)->toNumber(), deCONZ::ApsController::instance());
            device->startStateTimer(MinMacPollRxOn);
        }
    }
    else if (event.what() == REventActiveEndpoints)
    {
        device->stopStateTimer();
        device->setState(DEV_InitStateHandler);
        DEV_EnqueueEvent(device, REventAwake);
    }
    else if (event.what() == REventStateTimeout)
    {
        DBG_Printf(DBG_INFO, "read ZDP active endpoints timeout: 0x%016llX\n", device->key());
        device->setState(DEV_InitStateHandler);
    }
}

/*! #4 This state checks that for all active endpoints simple descriptors are known.
 */
void DEV_SimpleDescriptorStateHandler(Device *device, const Event &event)
{
    if (event.what() == REventStateEnter)
    {
        quint8 needFetchEp = 0x00;

        for (const auto ep : device->node()->endpoints())
        {
            deCONZ::SimpleDescriptor sd;
            if (device->node()->copySimpleDescriptor(ep, &sd) != 0 || sd.deviceId() == 0xffff)
            {
                needFetchEp = ep;
                break;
            }
        }

        if (needFetchEp == 0x00)
        {
            DBG_Printf(DBG_INFO, "ZDP simple descriptors verified: 0x%016llX\n", device->key());
            device->setState(DEV_BasicClusterStateHandler);
        }
        else if (!device->reachable())
        {
            device->setState(DEV_InitStateHandler);
        }
        else
        {
            ZDP_SimpleDescriptorReq(device->item(RAttrNwkAddress)->toNumber(), needFetchEp, deCONZ::ApsController::instance());
            device->startStateTimer(MinMacPollRxOn);
        }
    }
    else if (event.what() == REventSimpleDescriptor)
    {
        device->stopStateTimer();
        device->setState(DEV_InitStateHandler);
        DEV_EnqueueEvent(device, REventAwake);
    }
    else if (event.what() == REventStateTimeout)
    {
        DBG_Printf(DBG_INFO, "read ZDP simple descriptor timeout: 0x%016llX\n", device->key());
        device->setState(DEV_InitStateHandler);
    }
}

/*! Returns the first Simple Descriptor for a given server \p clusterId or nullptr if not found.
 */
const deCONZ::SimpleDescriptor *DEV_GetSimpleDescriptorForServerCluster(const Device *device, deCONZ::ZclClusterId_t clusterId)
{
    for (const auto &sd : device->node()->simpleDescriptors())
    {
        const auto cluster = std::find_if(sd.inClusters().cbegin(), sd.inClusters().cend(), [clusterId](const deCONZ::ZclCluster &cl)
        {
            return cl.id_t() == clusterId;
        });

        if (cluster != sd.inClusters().cend())
        {
            return &sd;
        }
    }

    return nullptr;
}

/*! Try to fill \c ResourceItem value from \p subDevices if not already set.
 */
bool DEV_FillItemFromSubdevices(Device *device, const char *itemSuffix, const std::vector<Resource*> &subDevices)
{
    auto *ditem = device->item(itemSuffix);
    Q_ASSERT(ditem);

    if (ditem->lastSet().isValid())
    {
        return true;
    }

    for (const auto rsub : subDevices)
    {
        auto *sitem = rsub->item(itemSuffix);
        if (sitem && sitem->lastSet().isValid())
        {
            // copy from sub-device into device
            if (ditem->setValue(sitem->toVariant()))
            {
                return true;
            }
        }
    }

    return false;
}

/*! Sends a ZCL Read Attributes request for \p clusterId and \p attrId.
    This also configures generic read and parse handlers for an \p item if not already set.
 */
bool DEV_ZclRead(Device *device, ResourceItem *item, deCONZ::ZclClusterId_t clusterId, deCONZ::ZclAttributeId_t attrId)
{
    Q_ASSERT(device);
    Q_ASSERT(item);

    if (!device->reachable())
    {
        DBG_Printf(DBG_INFO, "DEV not reachable, skip read %s: 0x%016llX\n", item->descriptor().suffix, device->key());
        return false;
    }

    const auto *sd = DEV_GetSimpleDescriptorForServerCluster(device, clusterId);

    if (!sd)
    {
        DBG_Printf(DBG_INFO, "TODO cluster 0x%04X not found: 0x%016llX\n", device->key(), static_cast<quint16>(clusterId));
        return false;
    }

    if (item->readParameters().empty())
    {
        item->setReadParameters({QLatin1String("readGenericAttribute/4"), sd->endpoint(), static_cast<quint16>(clusterId), static_cast<quint16>(attrId), 0x0000});
    }
    if (item->parseParameters().empty())
    {
        item->setParseParameters({QLatin1String("parseGenericAttribute/4"), sd->endpoint(),
                                  static_cast<quint16>(clusterId),
                                  static_cast<quint16>(attrId),
                                  "Item.val = Attr.val"});
    }
    auto readFunction = DA_GetReadFunction(item->readParameters());

    if (readFunction && readFunction(device, item, deCONZ::ApsController::instance()))
    {
        return true;
    }

    return false;
}

/*! #5 This state reads all common basic cluster attributes needed to match a DDF,
    e.g. modelId, manufacturer name, application version, etc.
 */
void DEV_BasicClusterStateHandler(Device *device, const Event &event)
{
    if (event.what() == REventStateEnter)
    {
        struct _item {
            const char *suffix;
            deCONZ::ZclClusterId_t clusterId;
            deCONZ::ZclAttributeId_t attrId;
        };

        const std::array<_item, 2> items = {
            _item{ RAttrManufacturerName, 0x0000_clid, 0x0004_atid },
            _item{ RAttrModelId,          0x0000_clid, 0x0005_atid }
        };

        size_t okCount = 0;
        const auto subDevices = device->subDevices();

        for (const auto &it : items)
        {
            if (DEV_FillItemFromSubdevices(device, it.suffix, subDevices))
            {
                okCount++;
                continue;
            }

            if (DEV_ZclRead(device, device->item(it.suffix), it.clusterId, it.attrId))
            {
                device->startStateTimer(MinMacPollRxOn);
                return; // keep state and wait for REventStateTimeout or response
            }

            DBG_Printf(DBG_INFO, "Failed to read %s: 0x%016llX\n", it.suffix, device->key());
            break;
        }

        if (okCount != items.size())
        {
            device->setState(DEV_InitStateHandler);
        }
        else
        {
            DBG_Printf(DBG_INFO, "DEV modelId: %s, 0x%016llX\n", qPrintable(device->item(RAttrModelId)->toString()), device->key());
            device->setState(DEV_GetDeviceDescriptionHandler);
        }
    }
    else if (event.what() == RAttrManufacturerName || event.what() == RAttrModelId)
    {
        DBG_Printf(DBG_INFO, "DEV received %s: 0x%016llX\n", event.what(), device->key());
        device->stopStateTimer();
        device->setState(DEV_InitStateHandler); // ok re-evaluate
        DEV_EnqueueEvent(device, REventAwake);
    }
    else if (event.what() == REventStateTimeout)
    {
        DBG_Printf(DBG_INFO, "DEV read basic cluster timeout: 0x%016llX\n", device->key());
        device->setState(DEV_InitStateHandler);
    }
}

QString uniqueIdFromTemplate(const QStringList &templ, const quint64 extAddress)
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

/*! Creates a ResourceItem if not exist, initialized with \p ddfItem content.
 */
ResourceItem *DEV_InitDeviceDescriptionItem(const DeviceDescription::Item &ddfItem, Resource *rsub)
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
        Q_ASSERT(item);

        if (ddfItem.defaultValue.isValid())
        {
            item->setValue(ddfItem.defaultValue);
        }
    }

    return item;
}

/*! Creates and initialises sub-device Resources and ResourceItems if not already present.

    This function can replace database and joining device initialisation.
 */
static bool DEV_InitDeviceFromDescription(Device *device, const DeviceDescription &description)
{
    Q_ASSERT(device);
    Q_ASSERT(description.isValid());

    size_t subCount = 0;

    for (const auto &sub : description.subDevices)
    {
        Q_ASSERT(sub.isValid());

        const auto uniqueId = uniqueIdFromTemplate(sub.uniqueId, device->item(RAttrExtAddress)->toNumber());
        Resource *rsub = DEV_GetSubDevice(device, nullptr, uniqueId);

        if (rsub)
        { }
        else if (sub.restApi == QLatin1String("/sensors"))
        {
            rsub = DEV_InitSensorNodeFromDescription(device, sub, uniqueId);
        }
        else if (sub.restApi == QLatin1String("/lights"))
        {
            rsub = DEV_InitLightNodeFromDescription(device, sub, uniqueId);
        }
        else
        {
            Q_ASSERT(nullptr); // TODO create dynamic Resource*
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

            item->setParseParameters(ddfItem.parseParameters);
            item->setReadParameters(ddfItem.readParameters);
            item->setWriteParameters(ddfItem.writeParameters);

            if (item->descriptor().suffix == RConfigCheckin)
            {
                StateChange stateChange(StateChange::StateWaitSync, SC_WriteZclAttribute, sub.uniqueId.at(1).toUInt());
                stateChange.addTargetValue(RConfigCheckin, ddfItem.defaultValue);
                stateChange.setChangeTimeoutMs(1000 * 60 * 60);
                rsub->addStateChange(stateChange);
            }
        }
    }

    return subCount == description.subDevices.size();
}

/*! #6 This state checks if for the modelId a device description is available.
    In that case the device is initialised (or updated) based on the JSON description.
 */
void DEV_GetDeviceDescriptionHandler(Device *device, const Event &event)
{
    if (event.what() == REventStateEnter)
    {
        const auto modelId = device->item(RAttrModelId)->toString();
        const auto description = DeviceDescriptions::instance()->get(device);

        if (description.isValid())
        {
            DBG_Printf(DBG_INFO, "found device description for 0x%016llX, modelId: %s\n", device->key(), qPrintable(modelId));

            DEV_InitDeviceFromDescription(device, description);
            device->setState(DEV_IdleStateHandler); // TODO
        }
        else
        {
            DBG_Printf(DBG_INFO, "No device description for 0x%016llX, modelId: %s\n", device->key(), qPrintable(modelId));
            device->setState(DEV_IdleStateHandler);
        }
    }
}

/*

@startuml
hide empty description

[*] -> Binding
Binding --> TableVerify : Start
TableVerify --> Binding : Done
TableVerify --> TableVerify : Ok, Next
TableVerify --> AddBinding : Missing
TableVerify --> ReadReportConfig : Stale Reports
AddBinding --> ReadReportConfig : Ok
AddBinding --> Binding : Error
ReadReportConfig --> TableVerify : Error or Done
ReadReportConfig --> ReadReportConfig : Ok, Next
ReadReportConfig --> ConfigReporting : Not Found
ConfigReporting --> ReadReportConfig : Error
@enduml
*/

void DEV_BindingHandler(Device *device, const Event &event)
{
    DevicePrivate *d = device->d;

    if (event.what() == REventStateEnter)
    {
        DBG_Printf(DBG_INFO, "DEV Binding enter %s/0x%016llX\n", event.resource(), event.deviceKey());
    }

    if (event.what() == REventPoll || event.what() == REventAwake)
    {
        if (!d->bindingVerify.isValid() || d->bindingVerify.elapsed() > (1000 * 60 * 5))
        {
            DBG_Printf(DBG_INFO, "DEV Binding verify bindings %s/0x%016llX\n", event.resource(), event.deviceKey());
        }
    }
    else if (event.what() == REventBindingTable)
    {
        if (event.num() == deCONZ::ZdpSuccess)
        {
            d->mgmtBindSupported = true;
        }
        else if (event.num() == deCONZ::ZdpNotSupported)
        {
            d->mgmtBindSupported = false;
        }
    }
    else
    {
        return;
    }

    d->bindingIter = 0;
    device->setState(DEV_BindingTableVerifyHandler, StateLevel1);
    DEV_EnqueueEvent(device, REventBindingTick);
}

void DEV_BindingTableVerifyHandler(Device *device, const Event &event)
{
    DevicePrivate *d = device->d;

    if (event.what() != REventBindingTick)
    {

    }
    else if (d->bindingIter >= device->node()->bindingTable().size())
    {
        d->bindingVerify.start();
        device->setState(DEV_BindingHandler, StateLevel1);
    }
    else
    {
        const auto now = QDateTime::currentMSecsSinceEpoch();
        const auto &bnd = *(device->node()->bindingTable().const_begin() + d->bindingIter);
        const auto dt = bnd.confirmedMsSinceEpoch() > 0 ? (now - bnd.confirmedMsSinceEpoch()) / 1000: -1;

        if (bnd.dstAddressMode() == deCONZ::ApsExtAddress)
        {
            DBG_Printf(DBG_INFO, "BND 0x%016llX cl: 0x%04X, dstAddrmode: %u, dst: 0x%016llX, dstEp: 0x%02X, dt: %lld seconds\n", bnd.srcAddress(), bnd.clusterId(), bnd.dstAddressMode(), bnd.dstAddress().ext(), bnd.dstEndpoint(), dt);
        }
        else if (bnd.dstAddressMode() == deCONZ::ApsGroupAddress)
        {
            DBG_Printf(DBG_INFO, "BND 0x%016llX cl: 0x%04X, dstAddrmode: %u, group: 0x%04X, dstEp: 0x%02X, dt: %lld seconds\n", bnd.srcAddress(), bnd.clusterId(), bnd.dstAddressMode(), bnd.dstAddress().group(), bnd.dstEndpoint(), dt);
        }

        d->bindingIter++;
        DEV_EnqueueEvent(device, REventBindingTick);
    }
}

Device::Device(DeviceKey key, QObject *parent) :
    QObject(parent),
    Resource(RDevices),
    d(new DevicePrivate)
{
    Q_ASSERT(parent);
    d->deviceKey = key;
    d->managed = DEV_TestManaged();
    connect(this, SIGNAL(eventNotify(Event)), parent, SLOT(enqueueEvent(Event)));
    addItem(DataTypeBool, RStateReachable);
    addItem(DataTypeUInt64, RAttrExtAddress);
    addItem(DataTypeUInt16, RAttrNwkAddress);
    addItem(DataTypeString, RAttrUniqueId)->setValue(generateUniqueId(key, 0, 0));
    addItem(DataTypeString, RAttrManufacturerName);
    addItem(DataTypeString, RAttrModelId);

    setState(DEV_InitStateHandler);

    static int initTimer = 1000;
    startStateTimer(initTimer);
    initTimer += 300; // hack for the first round init
}

Device::~Device()
{
    Q_ASSERT(d);
    delete d;
    d = nullptr;
}

void Device::addSubDevice(Resource *sub)
{
    Q_ASSERT(sub);
    Q_ASSERT(sub->item(RAttrUniqueId));
    const auto uniqueId = sub->item(RAttrUniqueId)->toString();

    sub->setParentResource(this);

    for (const auto &s : d->subDevices)
    {
        if (std::get<0>(s) == uniqueId)
            return; // already registered
    }

    d->subDevices.push_back({uniqueId, sub->prefix()});
}

DeviceKey Device::key() const
{
    return d->deviceKey;
}

const deCONZ::Node *Device::node() const
{
    return d->node;
}

bool Device::managed() const
{
    return d->managed;
}

void Device::handleEvent(const Event &event, DEV_StateLevel level)
{
    if (event.what() == REventAwake && level == StateLevel0)
    {
        d->awake.start();
    }

    if (d->state[level])
    {
        d->state[level](this, event);
    }
}

void Device::setState(DeviceStateHandler state, DEV_StateLevel level)
{
    if (d->state[level] != state)
    {
        if (d->state[level])
        {
            d->state[level](this, Event(prefix(), REventStateLeave, level, key()));
        }

        d->state[level] = state;

        if (d->state[level])
        {
            if (level == StateLevel0)
            {
                // invoke the handler in the next event loop iteration
                emit eventNotify(Event(prefix(), REventStateEnter, level, key()));
            }
            else
            {
                // invoke sub-states directly
                d->state[level](this, Event(prefix(), REventStateEnter, level, key()));
            }
        }
    }
}

void Device::startStateTimer(int IntervalMs)
{
    d->timer.start(IntervalMs, this);
}

void Device::stopStateTimer()
{
    d->timer.stop();
}

void Device::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == d->timer.timerId())
    {
        d->timer.stop(); // single shot
        d->state[StateLevel0](this, Event(prefix(), REventStateTimeout, 0, key()));
    }
}

qint64 Device::lastAwakeMs() const
{
    return d->awake.isValid() ? d->awake.elapsed() : 8640000;
}

bool Device::reachable() const
{
    if (lastAwakeMs() < MinMacPollRxOn)
    {
        return true;
    }
    else if (node() && !node()->nodeDescriptor().isNull() && node()->nodeDescriptor().receiverOnWhenIdle())
    {
        return item(RStateReachable)->toBool();
    }

    return false;
}

std::vector<Resource *> Device::subDevices() const
{
    std::vector<Resource *> result;

    // temp hack to get valid sub device pointers
    for (const auto &sub : d->subDevices)
    {
        auto *r = DEV_GetResource(std::get<1>(sub), std::get<0>(sub));

        if (r)
        {
            result.push_back(r);
        }
    }

    return result;
}

Device *DEV_GetDevice(DeviceContainer &devices, DeviceKey key)
{
    auto d = std::find_if(devices.begin(), devices.end(), [key](Device *device){ return device->key() == key; });

    if (d != devices.end())
    {
        return *d;
    }

    return nullptr;
}

Device *DEV_GetOrCreateDevice(QObject *parent, DeviceContainer &devices, DeviceKey key)
{
    Q_ASSERT(key != 0);
    auto d = std::find_if(devices.begin(), devices.end(), [key](Device *device){ return device->key() == key; });

    if (d == devices.end())
    {
        devices.push_back(new Device(key, parent));
        return devices.back();
    }

    Q_ASSERT(d != devices.end());

    return *d;
}

/*! Is used to test full Device control over: Device and sub-device creation, read, write, parse of Zigbee commands.
 */
bool DEV_TestManaged()
{
    return (deCONZ::appArgumentNumeric("--dev-test-managed", 0) > 0);
}
