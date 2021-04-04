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
#include <deconz/dbg_trace.h>
#include <deconz/node.h>
#include "device.h"
#include "device_access_fn.h"
#include "event.h"
#include "zdp.h"

// TODO move external declaration in de_web_plugin_private.h into utils.h
QString generateUniqueId(quint64 extAddress, quint8 endpoint, quint16 clusterId);

typedef void (*DeviceStateHandler)(Device *, const Event &);

void DEV_InitStateHandler(Device *device, const Event &event);
void DEV_IdleStateHandler(Device *device, const Event &event);
void DEV_NodeDescriptorStateHandler(Device *device, const Event &event);
void DEV_ActiveEndpointsStateHandler(Device *device, const Event &event);
void DEV_SimpleDescriptorStateHandler(Device *device, const Event &event);
void DEV_BasicClusterStateHandler(Device *device, const Event &event);
void DEV_GetDeviceDescriptionHandler(Device *device, const Event &event);
void DEV_BindingHandler(Device *device, const Event &event);
void DEV_BindingTableVerifyHandler(Device *device, const Event &event);
void DEV_PollIdleStateHandler(Device *device, const Event &event);
void DEV_PollBusyStateHandler(Device *device, const Event &event);
void DEV_DeadStateHandler(Device *device, const Event &event);

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
    void setState(DeviceStateHandler newState, DEV_StateLevel level = StateLevel0);
    void startStateTimer(int IntervalMs);
    void stopStateTimer();

    Device *q = nullptr; //! reference to public interface
    deCONZ::ApsController *apsCtrl = nullptr; //! opaque instance pointer forwarded to external functions

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
    std::array<DeviceStateHandler, 3> state{0};

    QBasicTimer timer; //! internal single shot timer
    QElapsedTimer awake; //! time to track when an end-device was last awake
    QElapsedTimer bindingVerify; //! time to track last binding table verification
    QElapsedTimer pollTimeout; //! time to track poll timeout
    int pollItemIter = 0;
    size_t bindingIter = 0;
    bool mgmtBindSupported = false;
    bool managed = false; //! a managed device doesn't rely on legacy implementation of polling etc.
    ZDP_Result zdpResult; //! keep track of a running ZDP request
    DA_ReadResult readResult; //! keep track of a running "read" request
};

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
    DevicePrivate *d = device->d;

    if (event.what() != RAttrLastSeen)
    {
        DBG_Printf(DBG_INFO, "DEV Init event %s/0x%016llX/%s\n", event.resource(), event.deviceKey(), event.what());
    }

    if (event.what() == REventStateEnter)
    {
        d->zdpResult = { };
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
            d->node = DEV_GetCoreNode(device->key());
        }

        if (device->node())
        {
            device->item(RAttrExtAddress)->setValue(device->node()->address().ext());
            device->item(RAttrNwkAddress)->setValue(device->node()->address().nwk());

            if (device->node()->address().nwk() == 0x0000)
            {
                d->setState(DEV_DeadStateHandler);
                return; // ignore coordinaor for now
            }

            // got a node, jump to verification
            if (!device->node()->nodeDescriptor().isNull() || device->reachable())
            {
                d->setState(DEV_NodeDescriptorStateHandler);
            }
        }
        else
        {
            DBG_Printf(DBG_INFO, "DEV Init no node found: 0x%016llX\n", event.deviceKey());

            if ((device->key() & 0xffffffff00000000LLU) == 0)
            {
                d->setState(DEV_DeadStateHandler);
                return; // ignore ZGP for now
            }
        }
    }
}

void DEV_CheckItemChanges(Device *device, const Event &event)
{
    DevicePrivate *d = device->d;
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
                change.tick(sub, d->apsCtrl);
            }

            sub->cleanupStateChanges();
        }
    }
}

/*! #2 This state checks that a valid NodeDescriptor is available.
 */
void DEV_NodeDescriptorStateHandler(Device *device, const Event &event)
{
    DevicePrivate *d = device->d;

    if (event.what() == REventStateEnter)
    {
        if (!device->node()->nodeDescriptor().isNull())
        {
            DBG_Printf(DBG_INFO, "ZDP node descriptor verified: 0x%016llX\n", device->key());
            d->setState(DEV_ActiveEndpointsStateHandler);
        }
        else if (!device->reachable()) // can't be queried, go back to #1 init
        {
            d->setState(DEV_InitStateHandler);
        }
        else
        {
            d->zdpResult = ZDP_NodeDescriptorReq(device->item(RAttrNwkAddress)->toNumber(), d->apsCtrl);
            if (d->zdpResult.isEnqueued)
            {
                d->startStateTimer(MinMacPollRxOn);
            }
            else
            {
                d->setState(DEV_InitStateHandler);
            }
        }
    }
    else if (event.what() == REventApsConfirm)
    {
        Q_ASSERT(event.deviceKey() == device->key());
        if (d->zdpResult.apsReqId == EventApsConfirmId(event) && EventApsConfirmStatus(event) != deCONZ::ApsSuccessStatus)
        {
            d->setState(DEV_InitStateHandler);
        }
    }
    else if (event.what() == REventNodeDescriptor) // received the node descriptor
    {
        d->stopStateTimer();
        d->setState(DEV_InitStateHandler); // evaluate egain from state #1 init
        DEV_EnqueueEvent(device, REventAwake);
    }
    else if (event.what() == REventStateTimeout)
    {
        DBG_Printf(DBG_INFO, "read ZDP node descriptor timeout: 0x%016llX\n", device->key());
        d->setState(DEV_InitStateHandler);
    }
}

/*! #3 This state checks that active endpoints are known.
 */
void DEV_ActiveEndpointsStateHandler(Device *device, const Event &event)
{
    DevicePrivate *d = device->d;

    if (event.what() == REventStateEnter)
    {
        if (!device->node()->endpoints().empty())
        {
            DBG_Printf(DBG_INFO, "ZDP active endpoints verified: 0x%016llX\n", device->key());
            d->setState(DEV_SimpleDescriptorStateHandler);
        }
        else if (!device->reachable())
        {
            d->setState(DEV_InitStateHandler);
        }
        else
        {
            d->zdpResult = ZDP_ActiveEndpointsReq(device->item(RAttrNwkAddress)->toNumber(), d->apsCtrl);
            if (d->zdpResult.isEnqueued)
            {
                d->startStateTimer(MinMacPollRxOn);
            }
            else
            {
                d->setState(DEV_InitStateHandler);
            }
        }
    }
    else if (event.what() == REventApsConfirm)
    {
        Q_ASSERT(event.deviceKey() == device->key());
        if (d->zdpResult.apsReqId == EventApsConfirmId(event) && EventApsConfirmStatus(event) != deCONZ::ApsSuccessStatus)
        {
            d->setState(DEV_InitStateHandler);
        }
    }
    else if (event.what() == REventActiveEndpoints)
    {
        d->stopStateTimer();
        d->setState(DEV_InitStateHandler);
        DEV_EnqueueEvent(device, REventAwake);
    }
    else if (event.what() == REventStateTimeout)
    {
        DBG_Printf(DBG_INFO, "read ZDP active endpoints timeout: 0x%016llX\n", device->key());
        d->setState(DEV_InitStateHandler);
    }
}

/*! #4 This state checks that for all active endpoints simple descriptors are known.
 */
void DEV_SimpleDescriptorStateHandler(Device *device, const Event &event)
{
    DevicePrivate *d = device->d;

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
            d->setState(DEV_BasicClusterStateHandler);
        }
        else if (!device->reachable())
        {
            d->setState(DEV_InitStateHandler);
        }
        else
        {
            d->zdpResult = ZDP_SimpleDescriptorReq(device->item(RAttrNwkAddress)->toNumber(), needFetchEp, d->apsCtrl);
            if (d->zdpResult.isEnqueued)
            {
                d->startStateTimer(MinMacPollRxOn);
            }
            else
            {
                d->setState(DEV_InitStateHandler);
            }
        }
    }
    else if (event.what() == REventApsConfirm)
    {
        Q_ASSERT(event.deviceKey() == device->key());
        if (d->zdpResult.apsReqId == EventApsConfirmId(event) && EventApsConfirmStatus(event) != deCONZ::ApsSuccessStatus)
        {
            d->setState(DEV_InitStateHandler);
        }
    }
    else if (event.what() == REventSimpleDescriptor)
    {
        d->stopStateTimer();
        d->setState(DEV_InitStateHandler);
        DEV_EnqueueEvent(device, REventAwake);
    }
    else if (event.what() == REventStateTimeout)
    {
        DBG_Printf(DBG_INFO, "read ZDP simple descriptor timeout: 0x%016llX\n", device->key());
        d->setState(DEV_InitStateHandler);
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

    DevicePrivate *d = device->d;

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

    if (readFunction && readFunction(device, item, d->apsCtrl, &d->readResult))
    {
        return d->readResult.isEnqueued;
    }

    return false;
}

/*! #5 This state reads all common basic cluster attributes needed to match a DDF,
    e.g. modelId, manufacturer name, application version, etc.
 */
void DEV_BasicClusterStateHandler(Device *device, const Event &event)
{
    DevicePrivate *d = device->d;

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
                d->startStateTimer(MinMacPollRxOn);
                return; // keep state and wait for REventStateTimeout or response
            }

            DBG_Printf(DBG_INFO, "Failed to read %s: 0x%016llX\n", it.suffix, device->key());
            break;
        }

        if (okCount != items.size())
        {
            d->setState(DEV_InitStateHandler);
        }
        else
        {
            DBG_Printf(DBG_INFO, "DEV modelId: %s, 0x%016llX\n", qPrintable(device->item(RAttrModelId)->toString()), device->key());
            d->setState(DEV_GetDeviceDescriptionHandler);
        }
    }
    else if (event.what() == RAttrManufacturerName || event.what() == RAttrModelId)
    {
        DBG_Printf(DBG_INFO, "DEV received %s: 0x%016llX\n", event.what(), device->key());
        d->stopStateTimer();
        d->setState(DEV_InitStateHandler); // ok re-evaluate
        DEV_EnqueueEvent(device, REventAwake);
    }
    else if (event.what() == REventStateTimeout)
    {
        DBG_Printf(DBG_INFO, "DEV read basic cluster timeout: 0x%016llX\n", device->key());
        d->setState(DEV_InitStateHandler);
    }
}

/*! #6 This state checks if for the device a device description file (DDF) is available.

    In that case the device is initialised (or updated) based on the JSON description.
    The actual processing is delegated to \c DeviceDescriptions class. This is done async
    so thousands of DDF files can be lazy loaded.
 */
void DEV_GetDeviceDescriptionHandler(Device *device, const Event &event)
{
    DevicePrivate *d = device->d;

    if (event.what() == REventStateEnter)
    {
        DEV_EnqueueEvent(device, REventDDFInitRequest);
    }
    else if (event.what() == REventDDFInitResponse)
    {
        if (event.num() == 1)
        {
            d->setState(DEV_IdleStateHandler);
        }
        else
        {
            d->setState(DEV_DeadStateHandler);
        }
    }
}

/*! #7 In this state the device is operational and runs sub states
    In parallel.

    IdleState : Bindings | ItemChange
 */
void DEV_IdleStateHandler(Device *device, const Event &event)
{
    DevicePrivate *d = device->d;

    if (event.what() == RAttrLastSeen || event.what() == REventPoll)
    {
         // don't print logs
    }
    else if (event.what() == REventStateEnter)
    {
//        d->setState(DEV_BindingHandler, StateLevel1);
        d->setState(DEV_PollIdleStateHandler, StateLevel2);
    }
    else if (event.what() == REventStateLeave)
    {
        d->setState(nullptr, StateLevel1);
        d->setState(nullptr, StateLevel2);
    }
    else if (event.what() == REventDDFReload)
    {
        d->setState(DEV_InitStateHandler);
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
        device->handleEvent(event, StateLevel2);
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
    d->setState(DEV_BindingTableVerifyHandler, StateLevel1);
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
        d->setState(DEV_BindingHandler, StateLevel1);
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

void DEV_PollIdleStateHandler(Device *device, const Event &event)
{
    DevicePrivate *d = device->d;

    if (event.what() == REventStateEnter)
    {
        DBG_Printf(DBG_INFO, "DEV Poll Idle enter %s/0x%016llX\n", event.resource(), event.deviceKey());
    }
    else if (event.what() == REventPoll)
    {
        const auto *resource = device->subDevices().front();
        d->pollItemIter %= resource->itemCount();

        const auto *item = resource->itemForIndex(d->pollItemIter);
        d->pollItemIter++;

        auto fn = DA_GetReadFunction(item->readParameters());

        if (fn && fn(resource, item, d->apsCtrl, &d->readResult))
        {
            if (d->readResult.isEnqueued)
            {
                d->pollTimeout.start();
                d->setState(DEV_PollBusyStateHandler, StateLevel2);
            }
        }
    }
}

void DEV_PollBusyStateHandler(Device *device, const Event &event)
{
    DevicePrivate *d = device->d;

    if (event.what() == REventStateEnter)
    {
        DBG_Printf(DBG_INFO, "DEV Poll Busy enter %s/0x%016llX\n", event.resource(), event.deviceKey());
    }
    else if (event.what() == REventApsConfirm && EventApsConfirmId(event) == d->readResult.apsReqId)
    {
        DBG_Printf(DBG_INFO, "DEV Poll Busy %s/0x%016llX APS confirm status: 0x%02X\n", event.resource(), event.deviceKey(), EventApsConfirmStatus(event));
        d->setState(DEV_PollIdleStateHandler, StateLevel2);
    }
    else if (event.what() == REventPoll && d->pollTimeout.elapsed() > 10000)
    {
        d->setState(DEV_PollIdleStateHandler, StateLevel2);
    }
}

/*! Empty handler to stop processing of the device.
 */
void DEV_DeadStateHandler(Device *device, const Event &event)
{
    Q_UNUSED(device)

    if (event.what() == REventStateEnter)
    {
        DBG_Printf(DBG_INFO, "DEV enter dead state 0x%016llX\n", event.deviceKey());
    }
}

Device::Device(DeviceKey key, deCONZ::ApsController *apsCtrl, QObject *parent) :
    QObject(parent),
    Resource(RDevices),
    d(new DevicePrivate)
{
    Q_ASSERT(parent);
    d->q = this;
    d->apsCtrl = apsCtrl;
    d->deviceKey = key;
    d->managed = DEV_TestManaged();
    connect(this, SIGNAL(eventNotify(Event)), parent, SLOT(enqueueEvent(Event)));
    addItem(DataTypeBool, RStateReachable);
    addItem(DataTypeUInt64, RAttrExtAddress);
    addItem(DataTypeUInt16, RAttrNwkAddress);
    addItem(DataTypeString, RAttrUniqueId)->setValue(generateUniqueId(key, 0, 0));
    addItem(DataTypeString, RAttrManufacturerName);
    addItem(DataTypeString, RAttrModelId);

    d->setState(DEV_InitStateHandler);

    static int initTimer = 1000;
    d->startStateTimer(initTimer);
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

void DevicePrivate::setState(DeviceStateHandler newState, DEV_StateLevel level)
{
    if (state[level] != newState)
    {
        if (state[level])
        {
            state[level](q, Event(q->prefix(), REventStateLeave, level, q->key()));
        }

        state[level] = newState;

        if (state[level])
        {
            if (level == StateLevel0)
            {
                // invoke the handler in the next event loop iteration
                emit q->eventNotify(Event(q->prefix(), REventStateEnter, level, q->key()));
            }
            else
            {
                // invoke sub-states directly
                state[level](q, Event(q->prefix(), REventStateEnter, level, q->key()));
            }
        }
    }
}

void DevicePrivate::startStateTimer(int IntervalMs)
{
    timer.start(IntervalMs, q);
}

void DevicePrivate::stopStateTimer()
{
    timer.stop();
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
    auto d = std::find_if(devices.begin(), devices.end(),
                          [key](const std::unique_ptr<Device> &device) { return device->key() == key; });

    if (d != devices.end())
    {
        return d->get();
    }

    return nullptr;
}

Device *DEV_GetOrCreateDevice(QObject *parent, deCONZ::ApsController *apsCtrl, DeviceContainer &devices, DeviceKey key)
{
    Q_ASSERT(key != 0);
    Q_ASSERT(apsCtrl);
    auto d = std::find_if(devices.begin(), devices.end(),
                          [key](const std::unique_ptr<Device> &device) { return device->key() == key; });

    if (d == devices.end())
    {
        devices.emplace_back(new Device(key, apsCtrl, parent));
        return devices.back().get();
    }

    Q_ASSERT(d != devices.end());

    return d->get();
}

/*! Is used to test full Device control over: Device and sub-device creation, read, write, parse of Zigbee commands.
 */
bool DEV_TestManaged()
{
    return (deCONZ::appArgumentNumeric("--dev-test-managed", 0) > 0);
}
