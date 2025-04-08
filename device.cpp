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
#include <QTimer>
#include <QTimerEvent>
#include <QMetaObject>
#include <array>
#include <deconz/dbg_trace.h>
#include <deconz/node.h>
#include "device.h"
#include "device_access_fn.h"
#include "device_descriptions.h"
#include "event.h"
#include "event_emitter.h"
#include "utils/utils.h"
#include "zcl/zcl.h"
#include "zdp/zdp.h"

#define STATE_LEVEL_BINDING  StateLevel1
#define STATE_LEVEL_POLL     StateLevel2

#define MGMT_BIND_SUPPORT_UNKNOWN -1
#define MGMT_BIND_SUPPORTED        1
#define MGMT_BIND_NOT_SUPPORTED    0

#define DEV_INVALID_DEVICE_ID -1

typedef void (*DeviceStateHandler)(Device *, const Event &);

/*! Device state machine description can be found in the wiki:

       https://github.com/dresden-elektronik/deconz-rest-plugin-v2/wiki/Device-Class#state-machine
*/
void DEV_InitStateHandler(Device *device, const Event &event);
void DEV_IdleStateHandler(Device *device, const Event &event);
void DEV_NodeDescriptorStateHandler(Device *device, const Event &event);
void DEV_ActiveEndpointsStateHandler(Device *device, const Event &event);
void DEV_SimpleDescriptorStateHandler(Device *device, const Event &event);
void DEV_BasicClusterStateHandler(Device *device, const Event &event);
void DEV_GetDeviceDescriptionHandler(Device *device, const Event &event);
static const deCONZ::SimpleDescriptor *DEV_GetSimpleDescriptorForServerCluster(const Device *device, deCONZ::ZclClusterId_t clusterId);
void DEV_BindingHandler(Device *device, const Event &event);
void DEV_BindingTableReadHandler(Device *device, const Event &event);
void DEV_BindingTableVerifyHandler(Device *device, const Event &event);
void DEV_BindingCreateHandler(Device *device, const Event &event);
void DEV_BindingRemoveHandler(Device *device, const Event &event);
void DEV_ReadReportConfigurationHandler(Device *device, const Event &event);
void DEV_ReadNextReportConfigurationHandler(Device *device, const Event &event);
void DEV_ConfigureNextReportConfigurationHandler(Device *device, const Event &event);
void DEV_ConfigureReportingHandler(Device *device, const Event &event);
void DEV_BindingIdleHandler(Device *device, const Event &event);
void DEV_PollIdleStateHandler(Device *device, const Event &event);
void DEV_PollNextStateHandler(Device *device, const Event &event);
void DEV_PollBusyStateHandler(Device *device, const Event &event);
void DEV_DeadStateHandler(Device *device, const Event &event);

// enable domain specific string literals
using namespace deCONZ::literals;

constexpr int RxOnWhenIdleResponseTime = 2000; // Expect shorter response delay for rxOnWhenIdle devices
constexpr int RxOffWhenIdleResponseTime = 8000; // 7680 ms + some space for timeout
constexpr int MaxConfirmTimeout = 20000; // If for some reason no APS-DATA.confirm is received (should almost
constexpr int BindingAutoCheckInterval = 1000 * 60 * 60;
constexpr int MaxPollItemRetries = 3;
constexpr int MaxIdleApsConfirmErrors = 16;
constexpr int MaxSubResources = 8;

static int devManaged = -1;

struct DEV_PollItem
{
    explicit DEV_PollItem(const Resource *r, const ResourceItem *i, const QVariant &p) :
        resource(r), item(i), readParameters(p) {}
    size_t retry = 0;
    const Resource *resource = nullptr;
    const ResourceItem *item = nullptr;
    QVariant readParameters;
};

// special value for ReportTracker::lastConfigureCheck during zcl configure reporting step
constexpr int64_t MarkZclConfigureBusy = 21;

struct ReportTracker
{
    deCONZ::SteadyTimeRef lastReport;
    deCONZ::SteadyTimeRef lastConfigureCheck;
    uint16_t clusterId = 0;
    uint16_t attributeId = 0;
    uint8_t endpoint = 0;
};

struct BindingTracker
{
    deCONZ::SteadyTimeRef tBound;
};

struct BindingContext
{
    size_t bindingCheckRound = 0;
    size_t bindingIter = 0;
    size_t reportIter = 0;
    size_t configIter = 0;
    int mgmtBindSupported = MGMT_BIND_SUPPORT_UNKNOWN;
    uint8_t mgmtBindStartIndex = 0;
    std::vector<BindingTracker> bindingTrackers;
    std::vector<DDF_Binding> bindings;
    std::vector<ReportTracker> reportTrackers;
    ZCL_ReadReportConfigurationParam readReportParam;
    ZCL_Result zclResult;
    ZDP_Result zdpResult;
};

static ReportTracker &DEV_GetOrCreateReportTracker(Device *device, uint16_t clusterId, uint16_t attrId, uint8_t endpoint);

class DevicePrivate
{
public:
    void setState(DeviceStateHandler newState, DEV_StateLevel level = StateLevel0);
    void startStateTimer(int IntervalMs, DEV_StateLevel level);
    void stopStateTimer(DEV_StateLevel level);
    bool hasRxOnWhenIdle() const;

    Device *q = nullptr; //! reference to public interface
    deCONZ::ApsController *apsCtrl = nullptr; //! opaque instance pointer forwarded to external functions

    /*! sub-devices are not yet referenced via pointers since these may become dangling.
        This is a helper to query the actual sub-device Resource* on demand via Resource::Handle.
    */
    std::array<Resource::Handle, MaxSubResources> subResourceHandles;
    std::vector<Resource*> subResources;
    const deCONZ::Node *node = nullptr; //! a reference to the deCONZ core node
    int deviceId = DEV_INVALID_DEVICE_ID;
    DeviceKey deviceKey = 0; //! for physical devices this is the MAC address

    /*! The currently active state handler function(s).
        Indexes >0 represent sub states of StateLevel0 running in parallel.
    */
    std::array<DeviceStateHandler, StateLevelMax> state{};

    std::array<QBasicTimer, StateLevelMax> timer; //! internal single shot timer one for each state level
    QElapsedTimer awake; //! time to track when an end-device was last awake
    BindingContext binding; //! only used by binding sub state machine
    std::vector<DEV_PollItem> pollItems; //! queue of items to poll
    int idleApsConfirmErrors = 0;
    /*! True while a new state waits for the state enter event, which must arrive first.
        This is for debug asserting that the order of events is valid - it doesn't drive logic. */
    bool stateEnterLock[StateLevelMax] = {};
    bool managed = false; //! a managed device doesn't rely on legacy implementation of polling etc.
    ZDP_Result zdpResult; //! keep track of a running ZDP request
    DA_ReadResult readResult; //! keep track of a running "read" request

    uint8_t zdpNeedFetchEndpointIndex = 0xFF; //! used in combination with flags.needReadSimpleDescriptors
    int maxResponseTime = RxOffWhenIdleResponseTime;

    struct
    {
        unsigned char hasDdf : 1;
        unsigned char initialRun : 1;
        unsigned char needZDPMaintenanceOnce : 1;
        unsigned char needReadActiveEndpoints : 1;
        unsigned char needReadSimpleDescriptors : 1;
        unsigned char reserved : 3;
    } flags{};
};

Device *DEV_ParentDevice(Resource *r)
{
    if (r && r->parentResource() && r->parentResource()->prefix() == RDevices)
    {
        return static_cast<Device*>(r->parentResource());
    }

    return nullptr;
}

//! Forward device attribute changes to core.
void DEV_ForwardNodeChange(Device *device, const QString &key, const QString &value)
{
    if (device)
    {
        QMetaObject::invokeMethod(device->d->apsCtrl, "onRestNodeUpdated", Qt::DirectConnection,
                              Q_ARG(quint64, device->key()), Q_ARG(QString, key), Q_ARG(QString, value));
    }
}

void DEV_EnqueueEvent(Device *device, const char *event)
{
    Q_ASSERT(device);
    Q_ASSERT(event);
    emit device->eventNotify(Event(device->prefix(), event, 0, device->key()));
}

Resource *DEV_GetSubDevice(Device *device, const char *prefix, const QString &identifier)
{
    if (!device)
    {
        return nullptr;
    }

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

    if (event.what() == REventStateEnter)
    {
        d->zdpResult = { };
        d->node = DEV_GetCoreNode(device->key()); // always get fresh pointer

        if ((event.deviceKey() & 0x00212E0000000000LLU) == 0x00212E0000000000LLU)
        {
            if (d->node && d->node->isCoordinator())
            {
                d->setState(DEV_DeadStateHandler);
                return; // ignore coordinaor for now
            }
        }
    }
    else if (event.what() == REventStateLeave)
    {
        return;
    }

    if (event.what() == REventPoll ||
        event.what() == REventAwake ||
        event.what() == RConfigReachable ||
        event.what() == RStateReachable ||
        event.what() == REventStateTimeout ||
        event.what() == RStateLastUpdated ||
        d->flags.initialRun == 1)
    {
        d->flags.initialRun = 0;
        d->binding.bindingCheckRound = 0;

        // lazy reference to deCONZ::Node
        if (!device->node())
        {
            d->node = DEV_GetCoreNode(device->key());
        }

        if (device->node())
        {
            {
                const deCONZ::Address a = device->node()->address();
                ResourceItem *ext = device->item(RAttrExtAddress);
                if (!ext->lastSet().isValid() || ext->toNumber() != a.ext())
                {
                    ext->setValue(a.ext());
                }
                ResourceItem *nwk = device->item(RAttrNwkAddress);
                if (!nwk->lastSet().isValid() || nwk->toNumber() != a.nwk())
                {
                    nwk->setValue(a.nwk());
                }
            }

            // got a node, jump to verification
            if (!device->node()->nodeDescriptor().isNull() || device->reachable())
            {
                d->setState(DEV_NodeDescriptorStateHandler);
            }
        }
        else
        {
            DBG_Printf(DBG_DEV, "DEV Init no node found: " FMT_MAC "\n", FMT_MAC_CAST(event.deviceKey()));

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

    int apsEnqueued = 0;
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

                if (device->reachable() && apsEnqueued == 0 && change.tick(d->deviceKey, sub, d->apsCtrl) == 1)
                {
                    apsEnqueued++;
                }
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
            DBG_Printf(DBG_DEV, "DEV ZDP node descriptor verified: " FMT_MAC "\n", FMT_MAC_CAST(device->key()));
            d->maxResponseTime = d->hasRxOnWhenIdle() ? RxOnWhenIdleResponseTime
                                                      : RxOffWhenIdleResponseTime;

            bool isSleeper = !d->hasRxOnWhenIdle();
            ResourceItem *capSleeper = device->item(RCapSleeper);
            if (!capSleeper->lastSet().isValid() || capSleeper->toBool() != isSleeper)
            {
                capSleeper->setValue(isSleeper); // can be overwritten by DDF
            }
            d->setState(DEV_ActiveEndpointsStateHandler);
        }
        else if (!device->reachable()) // can't be queried, go back to #1 init
        {
            d->setState(DEV_InitStateHandler);
        }
        else
        {
            d->zdpResult = ZDP_NodeDescriptorReq(d->node->address(), d->apsCtrl);
            if (d->zdpResult.isEnqueued)
            {
                d->startStateTimer(MaxConfirmTimeout, StateLevel0);
            }
            else
            {
                d->setState(DEV_InitStateHandler);
            }
        }
    }
    else if (event.what() == REventStateLeave)
    {
        d->stopStateTimer(StateLevel0);
    }
    else if (event.what() == REventApsConfirm)
    {
        if (d->zdpResult.apsReqId == EventApsConfirmId(event))
        {
            if (EventApsConfirmStatus(event) == deCONZ::ApsSuccessStatus)
            {
                d->stopStateTimer(StateLevel0);
                d->startStateTimer(d->maxResponseTime, StateLevel0);
            }
            else
            {
                d->setState(DEV_InitStateHandler);
            }
        }
    }
    else if (event.what() == REventNodeDescriptor) // received the node descriptor
    {
        d->setState(DEV_InitStateHandler); // evaluate egain from state #1 init
        DEV_EnqueueEvent(device, REventAwake);
    }
    else if (event.what() == REventStateTimeout)
    {
        DBG_Printf(DBG_DEV, "DEV read ZDP node descriptor timeout: " FMT_MAC "\n", FMT_MAC_CAST(device->key()));
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
        if (!device->node()->endpoints().empty() && !d->flags.needReadActiveEndpoints)
        {
            DBG_Printf(DBG_DEV, "DEV ZDP active endpoints verified: " FMT_MAC "\n", FMT_MAC_CAST(device->key()));
            d->setState(DEV_SimpleDescriptorStateHandler);
        }
        else if (!device->reachable())
        {
            d->setState(DEV_InitStateHandler);
        }
        else
        {
            d->zdpResult = ZDP_ActiveEndpointsReq(d->node->address(), d->apsCtrl);
            if (d->zdpResult.isEnqueued)
            {
                d->startStateTimer(MaxConfirmTimeout, StateLevel0);
            }
            else
            {
                d->setState(DEV_InitStateHandler);
            }
        }
    }
    else if (event.what() == REventStateLeave)
    {
        d->stopStateTimer(StateLevel0);
    }
    else if (event.what() == REventApsConfirm)
    {
        if (d->zdpResult.apsReqId == EventApsConfirmId(event))
        {
            if (EventApsConfirmStatus(event) == deCONZ::ApsSuccessStatus)
            {
                d->stopStateTimer(StateLevel0);
                d->startStateTimer(d->maxResponseTime, StateLevel0);
            }
            else
            {
                d->setState(DEV_InitStateHandler);
            }
        }
    }
    else if (event.what() == REventActiveEndpoints)
    {
        d->flags.needReadActiveEndpoints = 0;
        d->setState(DEV_InitStateHandler);
        DEV_EnqueueEvent(device, REventAwake);
    }
    else if (event.what() == REventStateTimeout)
    {
        DBG_Printf(DBG_DEV, "DEV read ZDP active endpoints timeout: " FMT_MAC "\n", FMT_MAC_CAST(device->key()));
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

        if (d->flags.needReadSimpleDescriptors) // forced read to refresh simple descriptors
        {
            if (d->zdpNeedFetchEndpointIndex < device->node()->endpoints().size())
            {
                needFetchEp = device->node()->endpoints()[d->zdpNeedFetchEndpointIndex];
            }
        }
        else
        {
            for (uint8_t ep : device->node()->endpoints())
            {
                bool ok = false;
                for (size_t i = 0; i < device->node()->simpleDescriptors().size(); i++)
                {
                    const deCONZ::SimpleDescriptor &sd = device->node()->simpleDescriptors()[i];
                    if (sd.endpoint() == ep && sd.deviceId() != 0xffff)
                    {
                        ok = true;
                        break;
                    }
                }

                if (!ok)
                {
                    needFetchEp = ep;
                    break;
                }
            }
        }

        if (needFetchEp == 0x00)
        {
            DBG_Printf(DBG_DEV, "DEV ZDP simple descriptors verified: " FMT_MAC "\n", FMT_MAC_CAST(device->key()));
            d->flags.needReadSimpleDescriptors = 0;
            d->zdpNeedFetchEndpointIndex = 0xFF;
            d->setState(DEV_BasicClusterStateHandler);
        }
        else if (!device->reachable())
        {
            d->setState(DEV_InitStateHandler);
        }
        else
        {
            d->zdpResult = ZDP_SimpleDescriptorReq(d->node->address(), needFetchEp, d->apsCtrl);
            if (d->zdpResult.isEnqueued)
            {
                d->startStateTimer(MaxConfirmTimeout, StateLevel0);
            }
            else
            {
                d->setState(DEV_InitStateHandler);
            }
        }
    }
    else if (event.what() == REventStateLeave)
    {
        d->stopStateTimer(StateLevel0);
    }
    else if (event.what() == REventApsConfirm)
    {
        if (d->zdpResult.apsReqId == EventApsConfirmId(event))
        {
            if (EventApsConfirmStatus(event) == deCONZ::ApsSuccessStatus)
            {
                d->stopStateTimer(StateLevel0);
                d->startStateTimer(d->maxResponseTime, StateLevel0);
            }
            else
            {
                d->setState(DEV_InitStateHandler);
            }
        }
    }
    else if (event.what() == REventSimpleDescriptor)
    {
        if (d->flags.needReadSimpleDescriptors) // forced read to refresh simple descriptors (next EP)
        {
            if (d->zdpNeedFetchEndpointIndex < device->node()->endpoints().size())
            {
                d->zdpNeedFetchEndpointIndex += 1;
            }
        }
        d->setState(DEV_InitStateHandler);
        DEV_EnqueueEvent(device, REventAwake);
    }
    else if (event.what() == REventStateTimeout)
    {
        DBG_Printf(DBG_DEV, "DEV read ZDP simple descriptor timeout: " FMT_MAC "\n", FMT_MAC_CAST(device->key()));
        d->setState(DEV_InitStateHandler);
    }
}

/*! Returns the first Simple Descriptor for a given server \p clusterId or nullptr if not found.
 */
static const deCONZ::SimpleDescriptor *DEV_GetSimpleDescriptorForServerCluster(const Device *device, deCONZ::ZclClusterId_t clusterId)
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

/*! Try to fill \c ResourceItem value from Basic cluster attributes if not already set.
 */
bool DEV_FillItemFromBasicCluster(Device *device, const char *itemSuffix, deCONZ::ZclClusterId_t clusterId,  deCONZ::ZclAttributeId_t attrId)
{
    ResourceItem *ditem = device->item(itemSuffix);

    if (!ditem || !device->node())
    {
        return false;
    }

    if (ditem->lastSet().isValid())
    {
        return true;
    }

    for (const auto &sd : device->node()->simpleDescriptors())
    {
        const auto cl = std::find_if(sd.inClusters().cbegin(), sd.inClusters().cend(),
                                     [clusterId](const auto &x) { return x.id_t() == clusterId; });

        if (cl == sd.inClusters().cend()) { continue; }

        const auto at = std::find_if(cl->attributes().cbegin(), cl->attributes().cend(),
                                     [attrId](const auto &x){ return x.id_t() == attrId; });

        if (at == cl->attributes().cend()) { continue; }

        const QVariant v = at->toVariant();

        if (!v.isNull() && ditem->setValue(v))
        {
            return true;
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
        DBG_Printf(DBG_DEV, "DEV not reachable, skip read %s: " FMT_MAC "\n", item->descriptor().suffix, FMT_MAC_CAST(device->key()));
        return false;
    }

    const auto *sd = DEV_GetSimpleDescriptorForServerCluster(device, clusterId);

    if (!sd)
    {
        DBG_Printf(DBG_DEV, "DEV TODO cluster 0x%04X not found: " FMT_MAC "\n", static_cast<quint16>(clusterId), FMT_MAC_CAST(device->key()));
        return false;
    }

    ZCL_Param param{};
    param.valid = 1;
    param.endpoint = sd->endpoint();
    param.clusterId = static_cast<quint16>(clusterId);
    param.attributes[0] = static_cast<quint16>(attrId);
    param.attributeCount = 1;

    const auto zclResult = ZCL_ReadAttributes(param, device->item(RAttrExtAddress)->toNumber(), device->item(RAttrNwkAddress)->toNumber(), d->apsCtrl);

    d->readResult.isEnqueued = zclResult.isEnqueued;
    d->readResult.apsReqId = zclResult.apsReqId;
    d->readResult.sequenceNumber = zclResult.sequenceNumber;

    return d->readResult.isEnqueued;
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
        const auto &subDevices = device->subDevices();

        for (const auto &it : items)
        {
            if (DEV_FillItemFromSubdevices(device, it.suffix, subDevices))
            {
                okCount++;
                continue;
            }
            else if (DEV_FillItemFromBasicCluster(device, it.suffix, it.clusterId,  it.attrId))
            {
                okCount++;
                continue;
            }

            if (DEV_ZclRead(device, device->item(it.suffix), it.clusterId, it.attrId))
            {
                d->startStateTimer(MaxConfirmTimeout, StateLevel0);
                return; // keep state and wait for REventStateTimeout or response
            }

            DBG_Printf(DBG_DEV, "DEV failed to read %s: " FMT_MAC "\n", it.suffix, FMT_MAC_CAST(device->key()));
            break;
        }

        if (okCount != items.size())
        {
            d->setState(DEV_InitStateHandler);
        }
        else
        {
            DBG_Printf(DBG_DEV, "DEV modelId: %s, " FMT_MAC "\n", qPrintable(device->item(RAttrModelId)->toString()), FMT_MAC_CAST(device->key()));
            d->setState(DEV_GetDeviceDescriptionHandler);
        }
    }
    else if (event.what() == REventStateLeave)
    {
        d->stopStateTimer(StateLevel0);
    }
    else if (event.what() == REventApsConfirm)
    {
        if (d->readResult.apsReqId == EventApsConfirmId(event))
        {
            if (EventApsConfirmStatus(event) == deCONZ::ApsSuccessStatus)
            {
                d->stopStateTimer(StateLevel0);
                d->startStateTimer(d->maxResponseTime, StateLevel0);
            }
            else
            {
                d->setState(DEV_InitStateHandler);
            }
        }
    }
    else if (event.what() == RAttrManufacturerName || event.what() == RAttrModelId)
    {
        DBG_Printf(DBG_DEV, "DEV received %s: " FMT_MAC "\n", event.what(), FMT_MAC_CAST(device->key()));
        d->setState(DEV_InitStateHandler); // ok re-evaluate
        DEV_EnqueueEvent(device, REventAwake);
    }
    else if (event.what() == REventStateTimeout)
    {
        DBG_Printf(DBG_DEV, "DEV read basic cluster timeout: " FMT_MAC "\n", FMT_MAC_CAST(device->key()));
        d->setState(DEV_InitStateHandler);
    }
}

/*! Forward device attributes to core to show it in the GUI.
 */
void DEV_PublishToCore(Device *device)
{

    struct CoreItem
    {
        const char *suffix;
        const char *mapped;
    };

    std::array<CoreItem, 4> coreItems = {
        {
            { RAttrName, "name" },
            { RAttrModelId, "modelid" },
            { RAttrManufacturerName, "vendor" },
            { RAttrSwVersion, "version" }
        }
    };

    const auto subDevices = device->subDevices();
    if (!subDevices.empty())
    {
        for (const CoreItem &i : coreItems)
        {
            const auto *item = subDevices.front()->item(i.suffix);
            if (item && !item->toString().isEmpty())
            {
                DEV_ForwardNodeChange(device, QLatin1String(i.mapped), item->toString());
            }
        }
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
        // if there is a IAS Zone Cluster add the RAttrZoneType
        if (DEV_GetSimpleDescriptorForServerCluster(device, 0x0500_clid))
        {
            device->addItem(DataTypeUInt16, RAttrZoneType);
        }
        DEV_EnqueueEvent(device, REventDDFInitRequest);
    }
    else if (event.what() == REventDDFInitResponse)
    {
        DEV_PublishToCore(device);

        if (event.num() == 1 || event.num() == 3)
        {
            d->managed = true;
            d->flags.hasDdf = 1;
            d->setState(DEV_IdleStateHandler);
            // TODO(mpi): temporary forward this info here, gets replaced by device actor later
            if (event.num() == 1)
            {
                DEV_ForwardNodeChange(device, QLatin1String("hasddf"), QLatin1String("1"));
            }
            else if (event.num() == 3)
            {
                DEV_ForwardNodeChange(device, QLatin1String("hasddf"), QLatin1String("2"));
            }
        }
        else
        {
            d->managed = false;
            d->flags.hasDdf = 0;
            d->setState(DEV_DeadStateHandler);
        }
    }
}

void DEV_CheckReachable(Device *device)
{
    DevicePrivate *d = device->d;
    bool devReachable = device->reachable();

    for (Resource *r : d->subResources)
    {
        ResourceItem *item = r->item(RConfigReachable);
        if (!item)
        {
            item = r->item(RStateReachable);
        }

        if (item && ((item->toBool() != devReachable) || !item->lastSet().isValid()))
        {
            r->setValue(item->descriptor().suffix, devReachable);
        }
    }
}

/*! #7 In this state the device is operational and runs sub states
    In parallel.

    IdleState : Bindings | Polling | ItemChange
 */
void DEV_IdleStateHandler(Device *device, const Event &event)
{
    DevicePrivate *d = device->d;

    if (event.what() == REventStateEnter)
    {
        DEV_CheckReachable(device);
        d->binding.bindingIter = 0;
        d->setState(DEV_BindingHandler, STATE_LEVEL_BINDING);
        d->setState(DEV_PollIdleStateHandler, STATE_LEVEL_POLL);
        return;
    }
    else if (event.what() == REventStateLeave)
    {
        d->setState(nullptr, STATE_LEVEL_BINDING);
        d->setState(nullptr, STATE_LEVEL_POLL);
        d->stopStateTimer(STATE_LEVEL_BINDING);
        d->stopStateTimer(STATE_LEVEL_POLL);
        return;
    }
    else if (event.what() == REventApsConfirm)
    {
        if (EventApsConfirmStatus(event) == deCONZ::ApsSuccessStatus)
        {
            d->idleApsConfirmErrors = 0;
        }
        else
        {
            d->idleApsConfirmErrors++;

            if (d->idleApsConfirmErrors > MaxIdleApsConfirmErrors && device->item(RStateReachable)->toBool())
            {
                d->idleApsConfirmErrors = 0;
                DBG_Printf(DBG_DEV, "DEV Idle max APS confirm errors: " FMT_MAC "\n", FMT_MAC_CAST(device->key()));
                device->item(RStateReachable)->setValue(false);
                DEV_CheckReachable(device);
            }
        }
    }
    else if (event.what() != RAttrLastSeen && event.what() != REventPoll)
    {
        // DBG_Printf(DBG_DEV, "DEV Idle event %s/0x%016llX/%s\n", event.resource(), event.deviceKey(), event.what());
        if (event.what() == RAttrSwVersion || event.what() == RAttrName)
        {
            DEV_PublishToCore(device);
        }
    }

    if (!device->reachable() && !device->item(RCapSleeper)->toBool())
    {
        DBG_Printf(DBG_DEV, "DEV (NOT reachable) Idle event %s/" FMT_MAC "/%s\n", event.resource(), FMT_MAC_CAST(event.deviceKey()), event.what());
    }

    DEV_CheckItemChanges(device, event);

    // process parallel states
    for (int i = StateLevel1; i < StateLevelMax; i++)
    {
        device->handleEvent(event, DEV_StateLevel(i));
    }
}

/*! Bindings sub state machien is described in:

      https://github.com/dresden-elektronik/deconz-rest-plugin-v2/wiki/Device-Class#bindings-sub-state-machine
*/

void DEV_BindingHandler(Device *device, const Event &event)
{
    DevicePrivate *d = device->d;

    if (event.what() == REventStateEnter)
    {
        DBG_Printf(DBG_DEV, "DEV Binding enter %s/" FMT_MAC "\n", event.resource(), FMT_MAC_CAST(event.deviceKey()));
    }
    else if (event.what() == REventPoll || event.what() == REventAwake || event.what() == REventBindingTick)
    {
        if (DA_ApsUnconfirmedRequests() > 4)
        {
            // wait
        }
        else
        {
            d->binding.bindingIter = 0;
            if (d->binding.mgmtBindSupported == MGMT_BIND_NOT_SUPPORTED)
            {
                d->setState(DEV_BindingTableVerifyHandler, STATE_LEVEL_BINDING);
            }
            else
            {
                d->setState(DEV_BindingTableReadHandler, STATE_LEVEL_BINDING);
            }
        }
    }
    else if (event.what() == REventBindingTable)
    {
        if (event.num() == deCONZ::ZdpSuccess)
        {
            d->binding.mgmtBindSupported = MGMT_BIND_SUPPORTED;
        }
        else if (event.num() == deCONZ::ZdpNotSupported)
        {
            d->binding.mgmtBindSupported = MGMT_BIND_NOT_SUPPORTED;
        }
    }
}

deCONZ::Binding DEV_ToCoreBinding(const DDF_Binding &bnd, quint64 srcAddress)
{
    if (bnd.isUnicastBinding)
    {
        return deCONZ::Binding(srcAddress, bnd.dstExtAddress, bnd.clusterId, bnd.srcEndpoint, bnd.dstEndpoint);
    }
    else if (bnd.isGroupBinding)
    {
        return deCONZ::Binding(srcAddress, bnd.dstGroup, bnd.clusterId, bnd.srcEndpoint);
    }

    Q_ASSERT(0);
    return {};
}

void DEV_BindingTableReadHandler(Device *device, const Event &event)
{
    DevicePrivate *d = device->d;

    if (event.what() == REventStateEnter)
    {
        DBG_Printf(DBG_DEV, "DEV Binding read bindings %s/" FMT_MAC "\n", event.resource(), FMT_MAC_CAST(event.deviceKey()));
        d->binding.mgmtBindStartIndex = 0;
        DEV_EnqueueEvent(device, REventBindingTick);
    }
    else if (event.what() == REventBindingTick)
    {
        d->zdpResult = ZDP_MgmtBindReq(d->binding.mgmtBindStartIndex, d->node->address(), d->apsCtrl);

        if (d->zdpResult.isEnqueued)
        {
            d->startStateTimer(MaxConfirmTimeout, STATE_LEVEL_BINDING);
        }
        else
        {
            d->setState(DEV_BindingHandler, STATE_LEVEL_BINDING);
        }
    }
    else if (event.what() == REventStateLeave)
    {
        d->stopStateTimer(STATE_LEVEL_BINDING);
    }
    else if (event.what() == REventApsConfirm)
    {
        if (d->zdpResult.apsReqId == EventApsConfirmId(event))
        {
            if (EventApsConfirmStatus(event) == deCONZ::ApsSuccessStatus)
            {
                d->stopStateTimer(STATE_LEVEL_BINDING);
                d->startStateTimer(d->maxResponseTime, STATE_LEVEL_BINDING);
            }
            else
            {
                d->setState(DEV_BindingHandler, STATE_LEVEL_BINDING);
            }
        }
    }
    else if (event.what() == REventZdpMgmtBindResponse)
    {
        uint8_t buf[128];
        if (event.hasData() && event.dataSize() >= 2 && event.dataSize() < sizeof(buf))
        {
            if (event.getData(buf, event.dataSize()))
            {
                const uint8_t seq = buf[0];
                const uint8_t status = buf[1];

                if (seq != d->zdpResult.zdpSeq)
                {
                    return;
                }

                if (status == deCONZ::ZdpSuccess)
                {
                    d->stopStateTimer(STATE_LEVEL_BINDING);

                    d->binding.mgmtBindSupported = MGMT_BIND_SUPPORTED;

                    uint8_t size = 0;
                    uint8_t index = 0;
                    uint8_t count = 0;

                    if (event.dataSize() >= 5)
                    {
                        size = buf[2];
                        index = buf[3];
                        count = buf[4];
                    }

                    if (size > index + count)
                    {
                        d->binding.mgmtBindStartIndex = index + count;
                        DEV_EnqueueEvent(device, REventBindingTick); // process next
                    }
                    else
                    {
                        d->binding.bindingIter = 0;
                        d->setState(DEV_BindingTableVerifyHandler, STATE_LEVEL_BINDING);
                    }
                }
                else
                {
                    if (status == deCONZ::ZdpNotSupported || status == deCONZ::ZdpNotPermitted)
                    {
                        d->binding.mgmtBindSupported = MGMT_BIND_NOT_SUPPORTED;
                    }
                    else
                    {
                        DBG_Printf(DBG_DEV, "DEV ZDP read binding table error: " FMT_MAC ", status: 0x%02X (TODO handle?)\n", FMT_MAC_CAST(device->key()), status);
                    }
                    d->setState(DEV_BindingHandler, STATE_LEVEL_BINDING);
                }
            }
        }
    }
    else if (event.what() == REventStateTimeout)
    {
        DBG_Printf(DBG_DEV, "DEV ZDP read binding table timeout: " FMT_MAC "\n", FMT_MAC_CAST(device->key()));
        d->setState(DEV_BindingHandler, STATE_LEVEL_BINDING);
    }
}

void DEV_BindingTableVerifyHandler(Device *device, const Event &event)
{
    DevicePrivate *d = device->d;

    if (event.what() == REventStateEnter)
    {
        DBG_Printf(DBG_DEV, "DEV Binding verify bindings %s/" FMT_MAC "\n", event.resource(), FMT_MAC_CAST(event.deviceKey()));
        DEV_EnqueueEvent(device, REventBindingTick);
    }
    else if (event.what() != REventBindingTick)
    {

    }
    else if (d->binding.bindingIter >= d->binding.bindings.size())
    {
        d->binding.bindingCheckRound++;
        d->setState(DEV_BindingRemoveHandler, STATE_LEVEL_BINDING);
    }
    else
    {
        auto &ddfBinding = d->binding.bindings[d->binding.bindingIter];
        auto &tracker = d->binding.bindingTrackers[d->binding.bindingIter];

        if (ddfBinding.dstExtAddress == 0 && ddfBinding.isUnicastBinding)
        {
            ddfBinding.dstExtAddress = d->apsCtrl->getParameter(deCONZ::ParamMacAddress);
            DBG_Assert(ddfBinding.dstExtAddress != 0);

            if (ddfBinding.dstExtAddress == 0)
            {
                d->setState(DEV_BindingHandler, STATE_LEVEL_BINDING);
                return;
            }
        }
        else if (ddfBinding.isGroupBinding)
        {
            bool ok = false;

            // update destination group based on RConfigGroup
            for (const auto &sub : device->subDevices())
            {
                ResourceItem *configGroup = sub->item(RConfigGroup);
                if (!configGroup)
                {
                    continue;
                }

                const auto ls = configGroup->toString().split(',', SKIP_EMPTY_PARTS);
                if (ddfBinding.configGroup >= ls.size())
                {
                    ddfBinding.dstGroup = 0; // clear
                    break;
                }

                uint group = ls[ddfBinding.configGroup].toUShort(&ok, 0);
                if (ok && group != 0)
                {
                    ddfBinding.dstGroup = group;
                }
                break;
            }

            if (!ok)
            {
                d->binding.bindingIter++; // process next
                DEV_EnqueueEvent(device, REventBindingTick);
                return;
            }
        }

        const auto &bindingTable = device->node()->bindingTable();
        const auto bnd = DEV_ToCoreBinding(ddfBinding, d->deviceKey);

        const auto i = std::find(bindingTable.const_begin(), bindingTable.const_end(), bnd);

        bool needBind = false;

        if (i == bindingTable.const_end())
        {
            needBind = true;
        }
        else
        {
            if (tracker.tBound < i->confirmedTimeRef())
            {
                tracker.tBound = i->confirmedTimeRef();
            }
            const auto now = deCONZ::steadyTimeRef();
            const auto dt = isValid(tracker.tBound) ? (now - tracker.tBound).val / 1000 : -1;

            if (i->dstAddressMode() == deCONZ::ApsExtAddress)
            {
                DBG_Printf(DBG_DEV, "DEV BND " FMT_MAC " cl: 0x%04X, dstAddrmode: %u, dst: " FMT_MAC ", dstEp: 0x%02X, dt: %d seconds\n",
                           FMT_MAC_CAST(i->srcAddress()), i->clusterId(), i->dstAddressMode(), FMT_MAC_CAST(i->dstAddress().ext()), i->dstEndpoint(), (int)dt);
            }
            else if (i->dstAddressMode() == deCONZ::ApsGroupAddress)
            {
                DBG_Printf(DBG_DEV, "DEV BND  " FMT_MAC " cl: 0x%04X, dstAddrmode: %u, group: 0x%04X, dstEp: 0x%02X, dt: %d seconds\n",
                           FMT_MAC_CAST(i->srcAddress()), i->clusterId(), i->dstAddressMode(), i->dstAddress().group(), i->dstEndpoint(), (int)dt);
            }

            if (dt < 0 || dt > 1800) // TODO max value
            {
                needBind = true;
            }
        }

        if (needBind)
        {
            d->setState(DEV_BindingCreateHandler, STATE_LEVEL_BINDING);
        }
        else if (i->dstAddressMode() == deCONZ::ApsExtAddress)
        {
            d->binding.configIter = 0;
            d->binding.reportIter = 0;
            d->setState(DEV_ReadReportConfigurationHandler, STATE_LEVEL_BINDING);
        }
        else if (i->dstAddressMode() == deCONZ::ApsGroupAddress)
        {
            d->binding.bindingIter++; // process next
            DEV_EnqueueEvent(device, REventBindingTick);
        }
    }
}

static void DEV_ProcessNextBinding(Device *device)
{
    DevicePrivate *d = device->d;

    d->binding.bindingIter++;
    d->setState(DEV_BindingTableVerifyHandler, STATE_LEVEL_BINDING);
}

void DEV_BindingCreateHandler(Device *device, const Event &event)
{
    DevicePrivate *d = device->d;

    if (event.what() == REventStateEnter)
    {
        const auto &ddfBinding = d->binding.bindings[d->binding.bindingIter];
        auto &tracker = d->binding.bindingTrackers[d->binding.bindingIter];
        tracker.tBound = {};

        const auto bnd = DEV_ToCoreBinding(ddfBinding, d->deviceKey);

        d->zdpResult = ZDP_BindReq(bnd, d->apsCtrl);

        if (d->zdpResult.isEnqueued)
        {
            d->startStateTimer(MaxConfirmTimeout, STATE_LEVEL_BINDING);
        }
        else
        {
            d->setState(DEV_BindingHandler, STATE_LEVEL_BINDING);
        }
    }
    else if (event.what() == REventStateLeave)
    {
        d->stopStateTimer(STATE_LEVEL_BINDING);
    }
    else if (event.what() == REventApsConfirm)
    {
        if (d->zdpResult.apsReqId == EventApsConfirmId(event))
        {
            if (EventApsConfirmStatus(event) == deCONZ::ApsSuccessStatus)
            {
                d->stopStateTimer(STATE_LEVEL_BINDING);
                d->startStateTimer(d->maxResponseTime, STATE_LEVEL_BINDING);
            }
            else
            {
                d->setState(DEV_BindingHandler, STATE_LEVEL_BINDING);
            }
        }
    }
    else if (event.what() == REventZdpResponse)
    {
        if (EventZdpResponseSequenceNumber(event) == d->zdpResult.zdpSeq)
        {
            if (EventZdpResponseStatus(event) == deCONZ::ZdpSuccess)
            {
                BindingTracker &tracker = d->binding.bindingTrackers[d->binding.bindingIter];
                tracker.tBound = deCONZ::steadyTimeRef();
                d->setState(DEV_BindingTableVerifyHandler, STATE_LEVEL_BINDING);
            }
            else
            {
                d->setState(DEV_BindingHandler, STATE_LEVEL_BINDING);
            }
        }
    }
    else if (event.what() == REventStateTimeout)
    {
        DBG_Printf(DBG_DEV, "DEV ZDP create binding timeout: " FMT_MAC "\n", FMT_MAC_CAST(device->key()));
        d->setState(DEV_BindingHandler, STATE_LEVEL_BINDING);
    }
}

void DEV_BindingRemoveHandler(Device *device, const Event &event)
{
    DevicePrivate *d = device->d;

    if (event.what() == REventStateEnter)
    {
        const auto &bindingTable = device->node()->bindingTable();
        auto i = bindingTable.const_begin();
        auto end = bindingTable.const_end();

        for (; i != end; ++i)
        {
            if (i->dstAddressMode() == deCONZ::ApsGroupAddress)
            {
                bool hasDdfBinding = false;
                bool hasDdfGroup = false;

                for (const auto &ddfBinding : d->binding.bindings)
                {
                    if (ddfBinding.isGroupBinding &&
                        i->clusterId() == ddfBinding.clusterId &&
                        i->srcEndpoint() == ddfBinding.srcEndpoint)
                    {
                        hasDdfBinding = true;
                        if (i->dstAddress().group() == ddfBinding.dstGroup)
                        {
                            hasDdfGroup = true;
                            break;
                        }
                    }
                }

                if (hasDdfBinding && !hasDdfGroup)
                {
                    break;
                }
            }
            else if (i->dstAddressMode() == deCONZ::ApsExtAddress)
            {
                const deCONZ::Node *dstNode = DEV_GetCoreNode(i->dstAddress().ext());
                if (!dstNode)
                {
                    DBG_Printf(DBG_DEV, "DEV ZDP remove binding to non existing node: " FMT_MAC "\n", FMT_MAC_CAST(i->dstAddress().ext()));
                    break; // remove
                }
            }
        }

        if (i == bindingTable.const_end())
        {
            d->setState(DEV_BindingIdleHandler, STATE_LEVEL_BINDING);
            return;
        }

        d->zdpResult = ZDP_UnbindReq(*i, d->apsCtrl);

        if (d->zdpResult.isEnqueued)
        {
            d->startStateTimer(MaxConfirmTimeout, STATE_LEVEL_BINDING);
        }
        else
        {
            d->setState(DEV_BindingHandler, STATE_LEVEL_BINDING);
        }
    }
    else if (event.what() == REventStateLeave)
    {
        d->stopStateTimer(STATE_LEVEL_BINDING);
    }
    else if (event.what() == REventApsConfirm)
    {
        if (d->zdpResult.apsReqId == EventApsConfirmId(event))
        {
            if (EventApsConfirmStatus(event) == deCONZ::ApsSuccessStatus)
            {
                d->stopStateTimer(STATE_LEVEL_BINDING);
                d->startStateTimer(d->maxResponseTime, STATE_LEVEL_BINDING);
            }
            else
            {
                d->setState(DEV_BindingHandler, STATE_LEVEL_BINDING);
            }
        }
    }
    else if (event.what() == REventZdpResponse)
    {
        if (EventZdpResponseSequenceNumber(event) == d->zdpResult.zdpSeq)
        {
            d->setState(DEV_BindingHandler, STATE_LEVEL_BINDING);
            DEV_EnqueueEvent(device, REventBindingTick);
        }
    }
    else if (event.what() == REventStateTimeout)
    {
        DBG_Printf(DBG_DEV, "DEV ZDP remove binding timeout: " FMT_MAC "\n", FMT_MAC_CAST(device->key()));
        d->setState(DEV_BindingHandler, STATE_LEVEL_BINDING);
    }
}

static bool reportingConfigurationValid(const Device *device, const Event &event)
{
    DevicePrivate *d = device->d;
    ZCL_ReadReportConfigurationRsp rsp{};

    if (!event.getData(&rsp, sizeof(rsp)))
    {
        return false;
    }

    const auto &bnd = d->binding.bindings[d->binding.bindingIter];

    size_t okCount = 0;

    for (size_t i = 0; i < rsp.recordCount; i++)
    {
        const auto &record = rsp.records[i];

        for (const auto &report : bnd.reporting)
        {
            bool found = false;
            for (auto &rec : d->binding.readReportParam.records) // compare with request
            {
                if (rec.attributeId == report.attributeId && rec.direction == report.direction)
                {
                    found = true;
                    break;
                }
            }

            if (!found || record.status != deCONZ::ZclSuccessStatus) { continue; }
            if (report.manufacturerCode != rsp.manufacturerCode) { continue; }
            if (report.attributeId != record.attributeId) { continue; }
            if (report.minInterval != record.minInterval) { continue; }
            if (report.maxInterval != record.maxInterval) { continue; }
            if (report.reportableChange != record.reportableChange) { continue; }

            okCount++;

            DBG_Printf(DBG_DEV, "DEV ZCL report configuration cl: 0x%04X, at: 0x%04X OK " FMT_MAC "\n", rsp.clusterId, record.attributeId, FMT_MAC_CAST(device->key()));
        }
    }

    if (okCount == d->binding.readReportParam.records.size())
    {
        DBG_Printf(DBG_DEV, "DEV ZCL report configuration cl: 0x%04X, mfcode: 0x%04X verified " FMT_MAC "\n", rsp.clusterId, rsp.manufacturerCode, FMT_MAC_CAST(device->key()));
        return true;
    }
    else
    {
        DBG_Printf(DBG_DEV, "DEV ZCL report configuration cl: 0x%04X, mfcode: 0x%04X needs update " FMT_MAC "\n", rsp.clusterId, rsp.manufacturerCode, FMT_MAC_CAST(device->key()));
        return false;
    }
}

void DEV_ReadReportConfigurationHandler(Device *device, const Event &event)
{
    DevicePrivate *d = device->d;

    if (event.what() == REventStateEnter)
    {
        Q_ASSERT(d->binding.bindingIter < d->binding.bindings.size());
        const auto &bnd = d->binding.bindings[d->binding.bindingIter];

        if (bnd.reporting.empty())
        {
            DEV_ProcessNextBinding(device);
            return;
        }

        ZCL_ReadReportConfigurationParam &param = d->binding.readReportParam;
        param = {};

        param.extAddress = device->node()->address().ext();
        param.nwkAddress = device->node()->address().nwk();
        param.clusterId = bnd.clusterId;
        param.manufacturerCode = 0;
        param.endpoint = bnd.srcEndpoint;

        auto tnow = deCONZ::steadyTimeRef();

        for (; d->binding.reportIter < bnd.reporting.size(); d->binding.reportIter++)
        {
            const DDF_ZclReport &report = bnd.reporting[d->binding.reportIter];
            ReportTracker &tracker = DEV_GetOrCreateReportTracker(device, bnd.clusterId, report.attributeId, bnd.srcEndpoint);

            if (d->binding.bindingCheckRound == 0)
            {
                // always verify on first round (needed for DDF hot reloading)
            }
            else if ((tnow - tracker.lastConfigureCheck) < deCONZ::TimeSeconds{3600})
            {
                DBG_Printf(DBG_DEV, "DEV " FMT_MAC " skip read ZCL report config for 0x%04X / 0x%04X\n", FMT_MAC_CAST(d->deviceKey), bnd.clusterId, report.attributeId);
                continue;
            }

            if (param.records.empty()) // only include matching manufacturer code reports in one frame
            {
                param.manufacturerCode = report.manufacturerCode;
            }
            else if (param.manufacturerCode != report.manufacturerCode)
            {
                break; // proceed later
            }

            tracker.lastConfigureCheck.ref = MarkZclConfigureBusy;

            ZCL_ReadReportConfigurationParam::Record record{};

            record.attributeId = report.attributeId;
            record.direction = report.direction;

            param.records.push_back(record);

            if (param.records.size() == ZCL_ReadReportConfigurationParam::MaxRecords)
            {
                break;
            }
        }

        if (param.records.empty())
        {
            DEV_ProcessNextBinding(device);
            return;
        }

        d->binding.zclResult = ZCL_ReadReportConfiguration(param, d->apsCtrl);

        if (d->binding.zclResult.isEnqueued)
        {
            d->startStateTimer(MaxConfirmTimeout, STATE_LEVEL_BINDING);
        }
        else
        {
            d->setState(DEV_BindingHandler, STATE_LEVEL_BINDING);
        }
    }
    else if (event.what() == REventStateLeave)
    {
        d->stopStateTimer(STATE_LEVEL_BINDING);
    }
    else if (event.what() == REventApsConfirm)
    {
        if (d->binding.zclResult.apsReqId == EventApsConfirmId(event))
        {
            if (EventApsConfirmStatus(event) == deCONZ::ApsSuccessStatus)
            {
                d->stopStateTimer(STATE_LEVEL_BINDING);
                d->startStateTimer(d->maxResponseTime, STATE_LEVEL_BINDING);
            }
            else
            {
                d->setState(DEV_BindingHandler, STATE_LEVEL_BINDING);
            }
        }
    }
    else if (event.what() == REventZclReadReportConfigResponse)
    {
        if (reportingConfigurationValid(device, event))
        {
            const auto tnow = deCONZ::steadyTimeRef();

            for (ReportTracker &tracker : d->binding.reportTrackers)
            {
                if (tracker.lastConfigureCheck.ref == MarkZclConfigureBusy)
                {
                    tracker.lastConfigureCheck = tnow;
                }
            }

            auto &bnd = d->binding.bindings[d->binding.bindingIter];

            if (d->binding.reportIter < bnd.reporting.size())
            {
                d->setState(DEV_ReadNextReportConfigurationHandler, STATE_LEVEL_BINDING);
            }
            else
            {
                DEV_ProcessNextBinding(device);
            }
        }
        else
        {
            d->setState(DEV_ConfigureReportingHandler, STATE_LEVEL_BINDING);
        }
    }
    else if (event.what() == REventStateTimeout)
    {
        DBG_Printf(DBG_DEV, "DEV ZCL read report configuration timeout: " FMT_MAC "\n", FMT_MAC_CAST(device->key()));
        d->setState(DEV_BindingHandler, STATE_LEVEL_BINDING);
    }
}

/*! Helper state to proceed with the next reporting check. */
void DEV_ReadNextReportConfigurationHandler(Device *device, const Event &event)
{
    DevicePrivate *d = device->d;

    if (event.what() == REventStateEnter)
    {
        d->setState(DEV_ReadReportConfigurationHandler, STATE_LEVEL_BINDING);
    }
}

/*! Helper state to proceed with the next configure reporting. */
void DEV_ConfigureNextReportConfigurationHandler(Device *device, const Event &event)
{
    DevicePrivate *d = device->d;

    if (event.what() == REventStateEnter)
    {
        d->setState(DEV_ConfigureReportingHandler, STATE_LEVEL_BINDING);
    }
}

void DEV_ConfigureReportingHandler(Device *device, const Event &event)
{
    DevicePrivate *d = device->d;

    if (event.what() == REventStateEnter)
    {
        const auto &bnd = d->binding.bindings[d->binding.bindingIter];
        Q_ASSERT(!bnd.reporting.empty());

        ZCL_ConfigureReportingParam param{};

        param.extAddress = device->node()->address().ext();
        param.nwkAddress = device->node()->address().nwk();
        param.clusterId = bnd.clusterId;
        param.manufacturerCode = d->binding.readReportParam.manufacturerCode;
        param.endpoint = bnd.srcEndpoint;

        for (size_t i = d->binding.configIter; i < d->binding.reportIter && i < bnd.reporting.size(); i++)
        {
            const DDF_ZclReport &report = bnd.reporting[i];
            d->binding.configIter++;

            if (report.manufacturerCode != param.manufacturerCode)
            {
                continue;
            }

            ZCL_ConfigureReportingParam::Record record{};

            record.attributeId = report.attributeId;
            record.direction = report.direction;
            record.dataType = report.dataType;
            record.minInterval = report.minInterval;
            record.maxInterval = report.maxInterval;
            record.reportableChange = report.reportableChange;
            record.timeout = 0; // TODO

            param.records.push_back(record);

            if (param.records.size() == ZCL_ConfigureReportingParam::MaxRecords)
            {
                break; // prevent too large APS frames
            }
        }

        d->binding.zclResult.isEnqueued = false;

        if (!param.records.empty())
        {
            d->binding.zclResult = ZCL_ConfigureReporting(param, d->apsCtrl);
        }

        if (d->binding.zclResult.isEnqueued)
        {
            d->startStateTimer(MaxConfirmTimeout, STATE_LEVEL_BINDING);
        }
        else
        {
            d->setState(DEV_BindingHandler, STATE_LEVEL_BINDING);
        }
    }
    else if (event.what() == REventStateLeave)
    {
        d->stopStateTimer(STATE_LEVEL_BINDING);
    }
    else if (event.what() == REventApsConfirm)
    {
        if (d->binding.zclResult.apsReqId == EventApsConfirmId(event))
        {
            if (EventApsConfirmStatus(event) == deCONZ::ApsSuccessStatus)
            {
                d->stopStateTimer(STATE_LEVEL_BINDING);
                d->startStateTimer(d->maxResponseTime, STATE_LEVEL_BINDING);
            }
            else
            {
                d->setState(DEV_BindingHandler, STATE_LEVEL_BINDING);
            }
        }
    }
    else if (event.what() == REventZclResponse)
    {
        if (d->binding.zclResult.sequenceNumber == EventZclSequenceNumber(event))
        {
            DBG_Printf(DBG_DEV, "DEV configure reporting %s/" FMT_MAC " ZCL response seq: %u, status: 0x%02X\n",
                   event.resource(), FMT_MAC_CAST(event.deviceKey()), d->binding.zclResult.sequenceNumber, EventZclStatus(event));

            if (EventZclStatus(event) == deCONZ::ZclSuccessStatus)
            {
                auto &bnd = d->binding.bindings[d->binding.bindingIter];

                if (d->binding.configIter < d->binding.reportIter)
                {
                    d->setState(DEV_ConfigureNextReportConfigurationHandler, STATE_LEVEL_BINDING);
                }
                else if (d->binding.reportIter < bnd.reporting.size())
                {
                    d->setState(DEV_ReadNextReportConfigurationHandler, STATE_LEVEL_BINDING);
                }
                else
                {
                    DEV_ProcessNextBinding(device);
                }
            }
            else
            {
                d->setState(DEV_BindingHandler, STATE_LEVEL_BINDING);
            }
        }
    }
    else if (event.what() == REventStateTimeout)
    {
        DBG_Printf(DBG_DEV, "DEV ZCL configure reporting timeout: " FMT_MAC "\n", FMT_MAC_CAST(device->key()));
        d->setState(DEV_BindingHandler, STATE_LEVEL_BINDING);
    }
}

void DEV_BindingIdleHandler(Device *device, const Event &event)
{
    DevicePrivate *d = device->d;

    if (event.what() == REventStateEnter)
    {
        DBG_Printf(DBG_DEV, "DEV Binding idle enter %s/" FMT_MAC "\n", event.resource(), FMT_MAC_CAST(event.deviceKey()));
        d->startStateTimer(BindingAutoCheckInterval, STATE_LEVEL_BINDING);
    }
    else if (event.what() == REventStateLeave)
    {
        d->stopStateTimer(STATE_LEVEL_BINDING);
    }
    else if (event.what() == REventStateTimeout)
    {
        d->setState(DEV_BindingHandler, STATE_LEVEL_BINDING);
    }
}

static ReportTracker &DEV_GetOrCreateReportTracker(Device *device, uint16_t clusterId, uint16_t attrId, uint8_t endpoint)
{
    DevicePrivate *d = device->d;

    auto i = std::find_if(d->binding.reportTrackers.begin(), d->binding.reportTrackers.end(), [&](ReportTracker &tracker) {
        return tracker.endpoint == endpoint &&
               tracker.clusterId == clusterId &&
               tracker.attributeId == attrId;
    });

    if (i != d->binding.reportTrackers.end())
    {
        return *i;
    }

    ReportTracker tracker;

    tracker.endpoint = endpoint;
    tracker.clusterId = clusterId;
    tracker.attributeId = attrId;

    d->binding.reportTrackers.push_back(tracker);

    return d->binding.reportTrackers.back();
}

static void DEV_UpdateReportTracker(Device *device, const ResourceItem *item)
{
    if (!isValid(item->lastZclReport()))
    {
        return;
    }

    const ZCL_Param &zclParam = item->zclParam();
    if (!isValid(zclParam) || zclParam.attributeCount == 0)
    {
        return;
    }

    Q_ASSERT(zclParam.attributeCount < zclParam.attributes.size());

    for (size_t i = 0; i < zclParam.attributeCount && i < zclParam.attributes.size(); i++)
    {
        ReportTracker &tracker = DEV_GetOrCreateReportTracker(device, zclParam.clusterId, zclParam.attributes[i], zclParam.endpoint);
        tracker.lastReport = item->lastZclReport();
    }
}

/*! Returns all items wich are ready for polling.
    The returned vector is reversed to use std::vector::pop_back() when processing the queue.
 */
std::vector<DEV_PollItem> DEV_GetPollItems(Device *device)
{
    DevicePrivate *d = device->d;
    std::vector<DEV_PollItem> result;
    const auto now = QDateTime::currentDateTime();
    const auto tnow = deCONZ::steadyTimeRef();

    for (const auto *r : device->subDevices())
    {
        for (int i = 0; i < r->itemCount(); i++)
        {
            const auto *item = r->itemForIndex(size_t(i));

            if (item->zclUnsupportedAttribute())
            {
                continue;
            }

            DEV_UpdateReportTracker(device, item);

            const auto &ddfItem = DDF_GetItem(item);

            if (ddfItem.readParameters.isNull())
            {
                continue;
            }

            int64_t dt = -1;

            if (item->refreshInterval().val == 0)
            {

            }
            else
            {
                if (isValid(item->lastZclReport()))
                {
                    dt = (tnow - item->lastZclReport()).val / 1000;
                    if (dt < item->refreshInterval().val)
                    {
                        continue;
                    }
                }

                if (item->lastSet().isValid() && item->valueSource() == ResourceItem::SourceDevice)
                {
                    const auto dt2 = item->lastSet().secsTo(now);
                    if (dt2  < item->refreshInterval().val)
                    {
                        continue;
                    }

                    dt = dt2;
                }
            }

            const auto m = ddfItem.readParameters.toMap();
            if (m.empty())
            {
                continue;
            }

            if (m.contains(QLatin1String("fn")) && m.value(QLatin1String("fn")).toString() == QLatin1String("none"))
            {
                continue;
            }

            DBG_Printf(DBG_DEV, "DEV " FMT_MAC " read %s, dt %d sec\n", FMT_MAC_CAST(d->deviceKey), item->descriptor().suffix, int(dt));
            result.emplace_back(DEV_PollItem{r, item, ddfItem.readParameters});
        }
    }

    std::reverse(result.begin(), result.end());
    return result;
}

/*! This state waits for REventPoll (and later REventPollForce).
    It collects all poll worthy items in a queue and moves to the PollNext state.
 */
void DEV_PollIdleStateHandler(Device *device, const Event &event)
{
    DevicePrivate *d = device->d;

    if (event.what() == REventStateEnter)
    {
        DBG_Printf(DBG_DEV, "DEV Poll Idle enter %s/" FMT_MAC "\n", event.resource(), FMT_MAC_CAST(event.deviceKey()));
    }
    else if (event.what() == REventPoll || event.what() == REventAwake)
    {
        if (DA_ApsUnconfirmedRequests() > 4)
        {
            // wait
            return;
        }

        if (device->node()) // update nwk address if needed
        {
            const auto &addr = device->node()->address();
            if (addr.hasNwk() && addr.nwk() != device->item(RAttrNwkAddress)->toNumber())
            {
                device->item(RAttrNwkAddress)->setValue(addr.nwk());
            }
        }

        if (d->flags.needZDPMaintenanceOnce)
        {
            // use some jitter to spread the the one time refresh of ZDP stuff
            static int randomDelay = 0;
            randomDelay++;
            if (randomDelay > (d->deviceKey & 0xFF))
            {
                randomDelay = 0;
                d->flags.needZDPMaintenanceOnce = 0;

                if (!device->item(RCapSleeper)->toBool() && device->reachable())
                {
                    d->flags.needReadActiveEndpoints = 1;
                    d->flags.needReadSimpleDescriptors = 1;
                    d->zdpNeedFetchEndpointIndex = 0;
                    DEV_EnqueueEvent(device, REventZdpReload);
                    return;
                }
            }
        }


        d->pollItems = DEV_GetPollItems(device);

        if (!d->pollItems.empty())
        {
            d->setState(DEV_PollNextStateHandler, STATE_LEVEL_POLL);
            return;
        }
        else
        {
            if (event.what() == REventPoll)
            {
                DBG_Printf(DBG_DEV, "DEV Poll Idle nothing to poll %s/" FMT_MAC "\n", event.resource(), FMT_MAC_CAST(event.deviceKey()));
                // notify DeviceTick to proceed
                DEV_EnqueueEvent(device, REventPollDone);
            }
        }
    }
}

/*! This state processes the next DEV_PollItem and moves to the PollBusy state.
    If no more items are in the queue it moves back to PollIdle state.
 */
void DEV_PollNextStateHandler(Device *device, const Event &event)
{
    DevicePrivate *d = device->d;

    if (event.what() == REventStateEnter || event.what() == REventStateTimeout)
    {
        Q_ASSERT(event.num() == STATE_LEVEL_POLL); // TODO remove
        if (!device->reachable())
        {
            d->pollItems.clear();
        }

        if (d->pollItems.empty())
        {
            d->setState(DEV_PollIdleStateHandler, STATE_LEVEL_POLL);
            // notify DeviceTick to proceed
            DEV_EnqueueEvent(device, REventPollDone);
            return;
        }

        auto &poll = d->pollItems.back();
        const auto readFunction = DA_GetReadFunction(poll.readParameters);

        d->readResult = { };
        if (readFunction)
        {
            d->readResult = readFunction(poll.resource, poll.item, d->apsCtrl, poll.readParameters);
        }
        else
        {
            DBG_Printf(DBG_DEV, "DEV Poll Next no read function for item: %s / " FMT_MAC "\n", poll.item->descriptor().suffix, FMT_MAC_CAST(device->key()));
            d->pollItems.pop_back();
            d->startStateTimer(5, STATE_LEVEL_POLL); // try next
            return;
        }

        if (d->readResult.isEnqueued)
        {
            d->setState(DEV_PollBusyStateHandler, STATE_LEVEL_POLL);
        }
        else
        {
            poll.retry++;

            DBG_Printf(DBG_DEV, "DEV Poll Next failed to enqueue read item: %s / " FMT_MAC "\n", poll.item->descriptor().suffix, FMT_MAC_CAST(device->key()));
            if (poll.retry >= MaxPollItemRetries)
            {
                d->pollItems.pop_back();
            }
            d->startStateTimer(d->maxResponseTime, STATE_LEVEL_POLL); // try again
        }
    }
    else if (event.what() == REventStateLeave)
    {
        d->stopStateTimer(STATE_LEVEL_POLL);
    }
}

/*! Increments retry counter of an item, or throws it away if maximum is reached. */
static void checkPollItemRetry(std::vector<DEV_PollItem> &pollItems)
{
    if (!pollItems.empty())
    {
        auto &pollItem = pollItems.back();
        pollItem.retry++;

        if (pollItem.retry >= MaxPollItemRetries)
        {
            pollItems.pop_back();
        }
    }
}

/*! This state waits for APS confirm or timeout for an ongoing poll request.
    In any case it moves back to PollNext state.
    If the request is successful the DEV_PollItem will be removed from the queue.
 */
void DEV_PollBusyStateHandler(Device *device, const Event &event)
{
    DevicePrivate *d = device->d;

    if (event.what() == REventStateEnter)
    {
        d->startStateTimer(MaxConfirmTimeout, STATE_LEVEL_POLL);
    }
    else if (event.what() == REventStateLeave)
    {
        d->stopStateTimer(STATE_LEVEL_POLL);
    }
    else if (event.what() == REventApsConfirm && EventApsConfirmId(event) == d->readResult.apsReqId)
    {
        DBG_Printf(DBG_DEV, "DEV Poll Busy %s/" FMT_MAC " APS-DATA.confirm id: %u, ZCL seq: %u, status: 0x%02X\n",
                   event.resource(), FMT_MAC_CAST(event.deviceKey()), d->readResult.apsReqId, d->readResult.sequenceNumber, EventApsConfirmStatus(event));

        if (EventApsConfirmStatus(event) == deCONZ::ApsSuccessStatus)
        {
            d->idleApsConfirmErrors = 0;
            d->stopStateTimer(StateLevel0);
            d->startStateTimer(d->maxResponseTime, STATE_LEVEL_POLL);
        }
        else
        {
            checkPollItemRetry(d->pollItems);
            d->setState(DEV_PollNextStateHandler, STATE_LEVEL_POLL);
        }
    }
    else if (event.what() == REventZclResponse)
    {
        if (d->readResult.clusterId != EventZclClusterId(event))
        { }
        else if (d->readResult.sequenceNumber == EventZclSequenceNumber(event) || d->readResult.ignoreResponseSequenceNumber)
        {
            uint8_t status = EventZclStatus(event);
            DBG_Printf(DBG_DEV, "DEV Poll Busy %s/" FMT_MAC " ZCL response seq: %u, status: 0x%02X, cluster: 0x%04X\n",
                   event.resource(), FMT_MAC_CAST(event.deviceKey()), d->readResult.sequenceNumber, status, d->readResult.clusterId);

            DBG_Assert(!d->pollItems.empty());
            if (!d->pollItems.empty())
            {
                if (status == deCONZ::ZclUnsupportedAttributeStatus)
                {
                    const auto &pi = d->pollItems.back();
                    Resource *r = DEV_GetResource(pi.resource->handle());
                    ResourceItem *item = r ? r->item(pi.item->descriptor().suffix) : nullptr;

                    if (item)
                    {
                        item->setZclUnsupportedAttribute();
                    }
                }

                d->pollItems.pop_back();
            }
            d->setState(DEV_PollNextStateHandler, STATE_LEVEL_POLL);
        }
    }
    else if (event.what() == REventStateTimeout)
    {
        DBG_Printf(DBG_DEV, "DEV Poll Busy %s/" FMT_MAC " timeout seq: %u, cluster: 0x%04X\n",
           event.resource(), FMT_MAC_CAST(event.deviceKey()), d->readResult.sequenceNumber, d->readResult.clusterId);
        checkPollItemRetry(d->pollItems);
        d->setState(DEV_PollNextStateHandler, STATE_LEVEL_POLL);
    }
}

/*! Empty handler to stop processing of the device.
 */
void DEV_DeadStateHandler(Device *device, const Event &event)
{
    if (event.what() == REventStateEnter)
    {
        DBG_Printf(DBG_DEV, "DEV enter passive state " FMT_MAC "\n", FMT_MAC_CAST(event.deviceKey()));
    }
    else if (event.what() == REventStateLeave)
    {

    }
    else
    {
        DevicePrivate *d = device->d;
        if (device->managed()) // when DDF handling is enabled again
        {
            d->setState(DEV_InitStateHandler);
        }
        else
        {

            if (event.what() == REventPoll || event.what() == REventAwake)
            {
                extern void DEV_PollLegacy(Device *device); // defined in de_web_plugin.cpp

                if (d->node && d->node->isCoordinator())
                {
                    return;
                }

                DEV_PollLegacy(device);
            }
        }
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
    d->flags.initialRun = 1;
    d->flags.hasDdf = 0;
    d->flags.needZDPMaintenanceOnce = 1;
    d->flags.needReadActiveEndpoints = 0;
    d->flags.needReadSimpleDescriptors = 0;

    addItem(DataTypeBool, RStateReachable);
    addItem(DataTypeBool, RCapSleeper);
    addItem(DataTypeUInt64, RAttrExtAddress)->setIsPublic(false);
    addItem(DataTypeUInt16, RAttrNwkAddress);
    addItem(DataTypeString, RAttrUniqueId)->setValue(generateUniqueId(key, 0, 0));
    addItem(DataTypeString, RAttrManufacturerName);
    addItem(DataTypeString, RAttrModelId);
    addItem(DataTypeString, RAttrDdfPolicy);
    addItem(DataTypeString, RAttrDdfHash);
    addItem(DataTypeUInt32, RAttrOtaVersion)->setIsPublic(false);

    // lazy init since the event handler is connected after the constructor
    QTimer::singleShot(0, this, [this]()
    {
        d->setState(DEV_InitStateHandler);
    });
}

Device::~Device()
{
    for (auto *r : subDevices())
    {
        r->setParentResource(nullptr);
    }

    Q_ASSERT(d);
    delete d;
    d = nullptr;
}

void Device::setDeviceId(int id)
{
    if (id >= 0)
    {
        d->deviceId = id;
    }
}

int Device::deviceId() const
{
    return d->deviceId;
}

void Device::addSubDevice(Resource *sub)
{
    Q_ASSERT(sub);
    Q_ASSERT(sub->item(RAttrUniqueId));

    sub->setParentResource(this);

    Q_ASSERT(isValid(sub->handle()));

    for (auto &hnd : d->subResourceHandles)
    {
        if (hnd == sub->handle())
        {
            hnd = sub->handle(); // refresh (index might be changed)
            return;
        }
    }

    for (auto &hnd : d->subResourceHandles)
    {
        if (!isValid(hnd))
        {
            hnd = sub->handle();
            DEV_CheckReachable(this);

            std::sort(d->subResourceHandles.begin(), d->subResourceHandles.end(), [](const auto &a, const auto &b)
            {
                if (a.order == 0) { return false; }
                return a.order < b.order;
            });

            return;
        }
    }


    Q_ASSERT(0); // too many sub resources, todo raise limit
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
    return d->managed && d->flags.hasDdf;
}

void Device::setManaged(bool managed)
{
    d->managed = managed;
}

void Device::setSupportsMgmtBind(bool supported)
{
    if (supported)
    {
        d->binding.mgmtBindSupported = MGMT_BIND_SUPPORTED;
    }
    else
    {
        d->binding.mgmtBindSupported = MGMT_BIND_NOT_SUPPORTED;
    }
}

void Device::handleEvent(const Event &event, DEV_StateLevel level)
{
    if (event.what() == REventStateEnter || event.what() == REventStateLeave)
    {
        if (event.num() < StateLevel0 || event.num() >= StateLevelMax)
        {
            return;
        }

        const auto level1 = static_cast<unsigned>(event.num());
        const auto fn = d->state[level1];
        if (d->stateEnterLock[level1] && event.what() == REventStateEnter)
        {
            d->stateEnterLock[level1] = false;
        }
        if (fn)
        {
            fn(this, event);
        }
    }
    else if (d->stateEnterLock[level])
    {
        // REventStateEnter must always arrive first via urgend event queue.
        // This branch should never hit!
        DBG_Printf(DBG_DEV, "DEV event before REventStateEnter: " FMT_MAC ", skip: %s\n", FMT_MAC_CAST(d->deviceKey), event.what());
    }
    else if (event.what() == REventDDFReload)
    {
        d->setState(DEV_InitStateHandler);
        d->binding.bindingCheckRound = 0;
        d->startStateTimer(50, StateLevel0);
    }
    else if (event.what() == REventZdpReload)
    {
        d->setState(DEV_ActiveEndpointsStateHandler);
        d->startStateTimer(50, StateLevel0);
    }
    else if (d->state[level])
    {
        if (event.what() == REventAwake && level == StateLevel0)
        {
            d->awake.start();
        }
        else if (event.what() == RStateReachable && event.resource() == RDevices)
        {
            DEV_CheckReachable(this);
        }

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
            stateEnterLock[level] = false;
        }

        state[level] = newState;

        if (state[level])
        {
            stateEnterLock[level] = true;
            Event e(q->prefix(), REventStateEnter, level, q->key());
            e.setUrgent(true);
            emit q->eventNotify(e);
        }
    }
}

void DevicePrivate::startStateTimer(int IntervalMs, DEV_StateLevel level)
{
    timer[level].start(IntervalMs, q);
}

void DevicePrivate::stopStateTimer(DEV_StateLevel level)
{
    if (timer[level].isActive())
    {
        timer[level].stop();
    }
}

bool DevicePrivate::hasRxOnWhenIdle() const
{
    return q->node()->nodeDescriptor().receiverOnWhenIdle();
}

void Device::timerEvent(QTimerEvent *event)
{
    for (int i = 0; i < StateLevelMax; i++)
    {
        if (event->timerId() == d->timer[i].timerId())
        {
            d->timer[i].stop(); // single shot
            if (d->state[i])
            {
                d->state[i](this, Event(prefix(), REventStateTimeout, i, key()));
            }
            break;
        }
    }
}

qint64 Device::lastAwakeMs() const
{
    return d->awake.isValid() ? d->awake.elapsed() : 8640000;
}

bool Device::reachable() const
{
    if (lastAwakeMs() < RxOffWhenIdleResponseTime)
    {
        return true;
    }
    else if (node() && !node()->nodeDescriptor().isNull() && node()->nodeDescriptor().receiverOnWhenIdle())
    {
        return item(RStateReachable)->toBool();
    }
    else if (!item(RCapSleeper)->toBool())
    {
        return item(RStateReachable)->toBool();
    }

    return false;
}

const std::vector<Resource *> &Device::subDevices()
{
    // temp hack to get valid sub device pointers
    d->subResources.clear();

    for (const auto hnd : d->subResourceHandles)
    {
        if (!isValid(hnd))
        {
            continue;
        }

        auto *r = DEV_GetResource(hnd);

        if (r)
        {
            d->subResources.push_back(r);
        }
    }

    return d->subResources;
}

void Device::clearBindings()
{
    d->binding.bindings.clear();
    d->binding.bindingTrackers.clear();
    if (d->state[STATE_LEVEL_BINDING])
    {
        d->setState(DEV_BindingHandler, STATE_LEVEL_BINDING);
    }
}

bool isSame(const DDF_Binding &a, const DDF_Binding &b)
{
    return a.clusterId == b.clusterId &&
           a.srcEndpoint == b.srcEndpoint &&
           (
            (a.isGroupBinding && b.isGroupBinding && a.configGroup == b.configGroup) ||
            (a.isUnicastBinding && b.isUnicastBinding && a.dstExtAddress == b.dstExtAddress)
           );
}

/*! Merges reporting configuration from \p b into \p a if not already existing.
 */
void mergeBindingReportConfigs(DDF_Binding &a, const DDF_Binding &b)
{
    for (const DDF_ZclReport &br : b.reporting)
    {
        const auto i = std::find_if(a.reporting.cbegin(), a.reporting.cend(),
                                    [&br](const DDF_ZclReport &ar) { return ar.attributeId == br.attributeId; });

        if (i == a.reporting.cend())
        {
            DBG_Printf(DBG_DEV, "DEV add reporting cluster: 0x%04X, attr: 0x%04X\n", b.clusterId, br.attributeId);
            a.reporting.push_back(br);
        }
    }
}

void Device::addBinding(const DDF_Binding &bnd)
{

    auto i = std::find_if(d->binding.bindings.begin(), d->binding.bindings.end(),
                          [&bnd](const auto &i) { return isSame(i, bnd); });

    if (i != d->binding.bindings.end())
    {
        mergeBindingReportConfigs(*i, bnd);
    }
    else
    {
        DBG_Printf(DBG_DEV, "DEV add binding cluster: 0x%04X, " FMT_MAC "\n", bnd.clusterId, FMT_MAC_CAST(d->deviceKey));
        BindingTracker tracker{};

        d->binding.bindings.push_back(bnd);
        d->binding.bindingTrackers.push_back(tracker);
        Q_ASSERT(d->binding.bindings.size() == d->binding.bindingTrackers.size());
        if (bnd.dstEndpoint == 0 && bnd.isUnicastBinding)
        {
            d->binding.bindings.back().dstEndpoint = 0x01; // todo query coordinator endpoint
        }
    }
}

const std::vector<DDF_Binding> &Device::bindings() const
{
    return d->binding.bindings;
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

Device *DEV_GetOrCreateDevice(QObject *parent, deCONZ::ApsController *apsCtrl, EventEmitter *eventEmitter, DeviceContainer &devices, DeviceKey key)
{
    Q_ASSERT(key != 0);
    Q_ASSERT(apsCtrl);
    auto d = std::find_if(devices.begin(), devices.end(),
                          [key](const std::unique_ptr<Device> &device) { return device->key() == key; });

    if (d == devices.end())
    {
        devices.emplace_back(new Device(key, apsCtrl, parent));
        Device *device = devices.back().get();
        QObject::connect(device, SIGNAL(eventNotify(Event)), eventEmitter, SLOT(enqueueEvent(Event)));
        device->setHandle(R_CreateResourceHandle(device, devices.size() - 1));
        return device;
    }

    Q_ASSERT(d != devices.end());

    return d->get();
}

bool DEV_RemoveDevice(DeviceContainer &devices, DeviceKey key)
{
    const auto i = std::find_if(devices.cbegin(), devices.cend(),
                          [key](const std::unique_ptr<Device> &device) { return device->key() == key; });
    if (i != devices.cend())
    {
        devices.erase(i);
    }

    return false;
}

void DEV_SetTestManaged(int enabled)
{
    if (enabled >= 0 && enabled <= 2)
    {
        devManaged = enabled ? enabled : 0;
    }
}

/*! Is used to test full Device control over: Device and sub-device creation, read, write, parse of Zigbee commands.
 */
bool DEV_TestManaged()
{
    return devManaged > 0;
}

/*! Is used to test full Device control over: Device and sub-device creation, read, write, parse of Zigbee commands.
    In addition legacy code for these tasks is disabled.
 */
bool DEV_TestStrict()
{
    return devManaged > 1;
}
