/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef DEVICE_H
#define DEVICE_H

#include <memory>
#include <QBasicTimer>
#include <QObject>
#include "resource.h"
#include "zcl/zcl.h"
#include "zdp/zdp.h"

#define STATE_LEVEL_BINDING  StateLevel1
#define STATE_LEVEL_POLL     StateLevel2

#define MGMT_BIND_SUPPORT_UNKNOWN -1
#define MGMT_BIND_SUPPORTED        1
#define MGMT_BIND_NOT_SUPPORTED    0

class Event;
class EventEmitter;
class Device;
class DDF_Binding;

namespace deCONZ
{
    class ApsController;
    class Node;
}

using DeviceKey = uint64_t; //! uniqueId for an Device, MAC address for physical devices

/*! Indexes in m_state[] array.
    Level 0   is the top level state
    Level >0  are for parallel states in a compound level 0 state
*/
enum DEV_StateLevel {
    StateLevel0 = 0,
    StateLevel1 = 1,
    StateLevel2 = 2,

    StateLevelMax
};

// enable domain specific string literals
using namespace deCONZ::literals;

constexpr int RxOnWhenIdleResponseTime = 2000; // Expect shorter response delay for rxOnWhenIdle devices
constexpr int RxOffWhenIdleResponseTime = 8000; // 7680 ms + some space for timeout
constexpr int MaxConfirmTimeout = 20000; // If for some reason no APS-DATA.confirm is received (should almost
constexpr int BindingAutoCheckInterval = 1000 * 60 * 60;
constexpr int MaxPollItemRetries = 3;
constexpr int MaxSubResources = 8;

struct DEV_PollItem
{
    explicit DEV_PollItem(const Resource *r, const ResourceItem *i, const QVariant &p) :
        resource(r), item(i), readParameters(p) {}
    size_t retry = 0;
    const Resource *resource = nullptr;
    const ResourceItem *item = nullptr;
    QVariant readParameters;
};

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
    int mgmtBindSupported = MGMT_BIND_SUPPORT_UNKNOWN;
    uint8_t mgmtBindStartIndex = 0;
    std::vector<BindingTracker> bindingTrackers;
    std::vector<DDF_Binding> bindings;
    std::vector<ReportTracker> reportTrackers;
    ZCL_ReadReportConfigurationParam readReportParam;
    ZCL_Result zclResult;
    ZDP_Result zdpResult;
};

/*! \class Device

    A generic per device supervisor for routers and end-devices.

    This class doesn't and MUST not know anything specific about devices.
    Device specific details are defined in device description files.

    As a starting point a Device only knows the MAC address called `DeviceKey` in this context.

    Each Device has a event driven state machine. As a side effect it is self healing, meaning that
    every missing piece like ZCL attributes or ZDP descriptors will be queried automatically.
    If an error or timeout occurs the process is retried later on.

    For sleeping end-devices the event/awake notifies to the state machine that the
    device is listening. Based on this even deep sleepers can be reached. Note that the event
    is not emitted on each Rx from an device but only when it is certain that the devices is listening
    e.g. MAC Data Requests, specific commands, Poll Control Cluster Checkings, etc.

    Currently implemented:

    - ZDP Node Descriptor
    - ZDP Active Endpoints
    - ZDP Simple Descriptors
    - ZCL Basic cluster modelid and manufacturer name


    Configurations like bindings and ZCL attribute reporting are maintained and verified continously. These may
    be specified in device description files or are configured dynamically via REST-API, e.g. a switch controls
    a certain group.

    TODO

    A Device maintains sub-resources which may represent lights, sensors or any other device. The device state given
    by the REST-API like on/off, brightness or thermostat configuration is kept in RecourceItems per sub-device.
    The state machine continiously verifies that a given state will be set, this is different from the former and
    common approach of fire commands and hope for the best.

    Commands MAY have a time to life (TTL) for example to not switch off a light after 3 hours when it becomes reachable.

    This class doesn't know much about ZCL attributes, it should operate mainly on ResourceItems which encapsulate
    how underlying ZCL attributes are written, parsed or queried. The same Resource item, like state/temperature might
    have a different configuration between devices. All this class sees is a state/temperature item or more precisely
    — just a ResouceItem in a certain state.
 */

class DevicePrivate;

class Device : public QObject,
               public Resource
{
    Q_OBJECT

public:
    DevicePrivate *d = nullptr; //! Public Pimpl pointer so that free functions in device.cpp can use it.

    Device() = delete;
    Device(const Device &) = delete;
    explicit Device(DeviceKey key, deCONZ::ApsController*apsCtrl, QObject *parent = nullptr);
    ~Device();
    void addSubDevice(Resource *sub);
    DeviceKey key() const;
    const deCONZ::Node *node() const;
    bool managed() const;
    void setManaged(bool managed);
    void handleEvent(const Event &event, DEV_StateLevel level = StateLevel0);
    void timerEvent(QTimerEvent *event) override;
    qint64 lastAwakeMs() const;
    bool reachable() const;
    const std::vector<Resource *> &subDevices();
    void clearBindings();
    void addBinding(const DDF_Binding &bnd);
    const std::vector<DDF_Binding> &bindings() const;

Q_SIGNALS:
    void eventNotify(const Event&); //! The device emits an event, which needs to be enqueued in a higher layer.
};

typedef void (*DeviceStateHandler)(Device *, const Event &);

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
    DeviceKey deviceKey = 0; //! for physical devices this is the MAC address

    /*! The currently active state handler function(s).
        Indexes >0 represent sub states of StateLevel0 running in parallel.
    */
    std::array<DeviceStateHandler, StateLevelMax> state{};

    std::array<QBasicTimer, StateLevelMax> timer; //! internal single shot timer one for each state level
    QElapsedTimer awake; //! time to track when an end-device was last awake
    BindingContext binding; //! only used by binding sub state machine
    std::vector<DEV_PollItem> pollItems; //! queue of items to poll
    bool managed = false; //! a managed device doesn't rely on legacy implementation of polling etc.
    ZDP_Result zdpResult; //! keep track of a running ZDP request
    DA_ReadResult readResult; //! keep track of a running "read" request

    int maxResponseTime = RxOffWhenIdleResponseTime;

    struct
    {
        unsigned char hasDdf : 1;
        unsigned char initialRun : 1;
        unsigned char reserved : 6;
    } flags{};
};

using DeviceContainer = std::vector<std::unique_ptr<Device>>;

Resource *DEV_GetSubDevice(Device *device, const char *prefix, const QString &identifier);

/*! Returns a device for a given \p key.

    \param devices - the container which contains the device
    \param key - unique identifier for a device (MAC address for physical devices)
 */
Device *DEV_GetDevice(DeviceContainer &devices, DeviceKey key);

/*! Returns a device for a given \p key.

    If the device doesn't exist yet it will be created.

    \param parent - must be DeRestPluginPrivate instance
    \param eventEmitter - emitter to enqueue events
    \param devices - the container which contains the device
    \param key - unique identifier for a device (MAC address for physical devices)
 */
Device *DEV_GetOrCreateDevice(QObject *parent, deCONZ::ApsController *apsCtrl, EventEmitter *eventEmitter, DeviceContainer &devices, DeviceKey key);

/*! Removes a device with \p key.

    \param devices - the container which contains the device
    \param key - unique identifier for a device (MAC address for physical devices)
 */
bool DEV_RemoveDevice(DeviceContainer &devices, DeviceKey key);

/*! Returns \c Resource for a given \p identifier.

    \param resource - RSensors | RLights | RGroups | RConfig
    \param identifier - id | uniqueid | empty (for RConfig)
*/
Resource *DEV_GetResource(const char *resource, const QString &identifier);
Resource *DEV_GetResource(Resource::Handle hnd);

/*! Returns deCONZ core node for a given \p extAddress.
 */
const deCONZ::Node *DEV_GetCoreNode(uint64_t extAddress);

void DEV_CheckReachable(Device *device);

void DEV_SetTestManaged(int enabled);
bool DEV_TestManaged();
bool DEV_TestStrict();

#endif // DEVICE_H
