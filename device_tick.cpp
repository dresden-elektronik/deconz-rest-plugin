/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QElapsedTimer>
#include <QTimer>
#include <deconz/dbg_trace.h>
#include "event.h"
#include "resource.h"
#include "device_tick.h"

#define DEV_TICK_BOOT_TIME 8000
#define TICK_INTERVAL_JOIN 500
#define TICK_INTERVAL_IDLE 1000

struct JoinDevice
{
    DeviceKey deviceKey;
    quint8 macCapabilities;
};

static const char *RLocal = nullptr;

typedef void (*DT_StateHandler)(DeviceTickPrivate *d, const Event &event);

static void DT_StateInit(DeviceTickPrivate *d, const Event &event);
static void DT_StateJoin(DeviceTickPrivate *d, const Event &event);
static void DT_StateIdle(DeviceTickPrivate *d, const Event &event);

class DeviceTickPrivate
{
public:
    DT_StateHandler stateHandler = DT_StateInit;
    std::vector<JoinDevice> joinDevices;
    DeviceTick *q = nullptr;
    QTimer *timer = nullptr;
    size_t devIter = 0;
    const DeviceContainer *devices = nullptr;
};

/*! Constructor.
 */
DeviceTick::DeviceTick(const DeviceContainer &devices, QObject *parent) :
    QObject(parent),
    d(new DeviceTickPrivate)
{
    d->devices = &devices;
    d->q = this;
    d->timer = new QTimer(this);
    d->timer->setSingleShot(true);
    connect(d->timer, &QTimer::timeout, this, &DeviceTick::timoutFired);
    d->timer->start(DEV_TICK_BOOT_TIME);
}

/*! Destructor.
 */
DeviceTick::~DeviceTick()
{
    Q_ASSERT(d);
    delete d;
    d = nullptr;
}

/*! Public event entry.
 */
void DeviceTick::handleEvent(const Event &event)
{
    d->stateHandler(d, event);
}

/*! State timer callback.
 */
void DeviceTick::timoutFired()
{
    d->stateHandler(d, Event(RLocal, REventStateTimeout, 0));
}

/*! Sets a new DeviceTick state.
    The events REventStateLeave and REventStateEnter will be triggered accordingly.
 */
static void DT_SetState(DeviceTickPrivate *d, DT_StateHandler state)
{
    if (d->stateHandler != state)
    {
        d->stateHandler(d, Event(RLocal, REventStateLeave, 0));
        d->stateHandler = state;
        d->stateHandler(d, Event(RLocal, REventStateEnter, 0));
    }
}

/*! Starts the state timer. Free function to be mocked by test code.
 */
static void DT_StartTimer(DeviceTickPrivate *d, int timeoutMs)
{
    d->timer->start(timeoutMs);
}

/*! Stops the state timer. Free function to be mocked by test code.
 */
static void DT_StopTimer(DeviceTickPrivate *d)
{
    d->timer->stop();
}

/*! Initial state to wait DEV_TICK_BOOT_TIME seconds before starting normal operation.
 */
static void DT_StateInit(DeviceTickPrivate *d, const Event &event)
{
    if (event.resource() == RLocal && event.what() == REventStateTimeout)
    {
        DBG_Printf(DBG_INFO, "DEV Tick.Init: booted after %lld seconds\n", DEV_TICK_BOOT_TIME);
        DT_SetState(d, DT_StateIdle);
    }
}

/*! Emits REventPoll to the next device in DT_StateIdle.
 */
static void DT_PollNextIdleDevice(DeviceTickPrivate *d)
{
    const auto devCount = d->devices->size();

    if (devCount == 0)
    {
        return;
    }

    d->devIter %= devCount;

    const auto &device = d->devices->at(d->devIter);
    Q_ASSERT(device);
    if (device->reachable())
    {
        emit d->q->eventNotify(Event(device->prefix(), REventPoll, 0, device->key()));
    }
    d->devIter++;
}

/*! This state is active while Permit Join is disabled for normal idle operation.

    It walks over all Devices in TICK_INTERVAL_IDLE spacing.
    The state transitions to DT_StateJoin when REventPermitjoinEnabled is received.
 */
static void DT_StateIdle(DeviceTickPrivate *d, const Event &event)
{
    if (event.what() == REventPermitjoinEnabled)
    {
        DT_SetState(d, DT_StateJoin);
    }
    else if (event.resource() == RLocal)
    {
        if (event.what() == REventStateTimeout)
        {
            DT_PollNextIdleDevice(d);
            DT_StartTimer(d, TICK_INTERVAL_IDLE);
        }
        else if (event.what() == REventStateEnter)
        {
            DT_StartTimer(d, TICK_INTERVAL_IDLE);
        }
        else if (event.what() == REventStateLeave)
        {
            DT_StopTimer(d);
        }
    }
}

/*! Adds a joining device entry to the queue if not already present.
 */
static void DT_RegisterJoiningDevice(DeviceTickPrivate *d, DeviceKey deviceKey, quint8 macCapabilities)
{
    Q_ASSERT(deviceKey != 0); // if this triggers we have problems elsewhere

    auto i = std::find_if(d->joinDevices.cbegin(), d->joinDevices.cend(), [&deviceKey](const JoinDevice &dev)
    {
        return deviceKey == dev.deviceKey;
    });

    if (i == d->joinDevices.cend())
    {
        JoinDevice dev;
        dev.deviceKey = deviceKey;
        dev.macCapabilities = macCapabilities;
        d->joinDevices.push_back(dev);
        DBG_Printf(DBG_INFO, "DEV Tick: fast poll 0x%016llX, mac capabilities: 0x%02X\n", deviceKey, macCapabilities);
    }
}

/*! Emits REventPoll to the next device in DT_StateJoin.
 */
static void DT_PollNextJoiningDevice(DeviceTickPrivate *d)
{
    if (d->joinDevices.empty())
    {
        return;
    }

    d->devIter %= d->joinDevices.size();
    Q_ASSERT(d->devIter < d->joinDevices.size());

    const JoinDevice &device = d->joinDevices.at(d->devIter);
    emit d->q->eventNotify(Event(RDevices, REventAwake, 0, device.deviceKey));
    d->devIter++;
}

/*! This state is active while Permit Join is enabled.

    When a REventDeviceAnnounce event is received, the device is added to a joining
    queue and processed exclusivly and quickly.

    The state transitions to DT_StateIdle when Permit Join is disabled.
 */
static void DT_StateJoin(DeviceTickPrivate *d, const Event &event)
{
    if (event.what() == REventPermitjoinDisabled)
    {
        DT_SetState(d, DT_StateIdle);
    }
    else if (event.what() == REventDeviceAnnounce)
    {
        DBG_Printf(DBG_INFO, "DEV Tick.Join: %s\n", event.what());
        DT_RegisterJoiningDevice(d, event.deviceKey(), static_cast<quint8>(event.num()));
    }
    else if (event.resource() == RLocal)
    {
        if (event.what() == REventStateTimeout)
        {
            DT_PollNextJoiningDevice(d);
            DT_StartTimer(d, TICK_INTERVAL_JOIN);
        }
        else if (event.what() == REventStateEnter)
        {
            DT_StartTimer(d, TICK_INTERVAL_JOIN);
        }
        else if (event.what() == REventStateLeave)
        {
            DT_StopTimer(d);
            d->joinDevices.clear();
        }
    }
}
