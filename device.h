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
#include <QObject>
#include "resource.h"

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
    void setDeviceId(int id);
    int deviceId() const;
    void addSubDevice(Resource *sub);
    DeviceKey key() const;
    const deCONZ::Node *node() const;
    bool managed() const;
    void setManaged(bool managed);
    void setSupportsMgmtBind(bool supported);
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

Device *DEV_ParentDevice(Resource *r);

/*! Helper to forward attributes to core (modelid, battery, etc.). */
void DEV_ForwardNodeChange(Device *device, const QString &key, const QString &value);

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
uint8_t DEV_ResolveDestinationEndpoint(uint64_t extAddr, uint8_t hintEp, uint16_t cluster, uint8_t frameControl);

void DEV_CheckReachable(Device *device);

void DEV_SetTestManaged(int enabled);
bool DEV_TestManaged();
bool DEV_TestStrict();

#endif // DEVICE_H
