/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef DEVICE_TICK_H
#define DEVICE_TICK_H

#include <QObject>
#include "device.h"

class Event;
class Device;

class DeviceTickPrivate;


/*! \class DeviceTick

    Coordinates poking the Device state machines.

    It differentiates between normal idle operation and device pairing while
    Permit Join is enabled. While during pairing a faster pace is applied.

    TODO

    Take task queue and APS-DATA.request queue into account.
 */
class DeviceTick : public QObject
{
    Q_OBJECT

public:
    explicit DeviceTick(const DeviceContainer &devices, QObject *parent = nullptr);
    ~DeviceTick();

Q_SIGNALS:
    void eventNotify(const Event&); //! Emitted \p Event needs to be enqueued in a higher layer.

public Q_SLOTS:
    void handleEvent(const Event &event);

private Q_SLOTS:
    void timoutFired();

private:
    DeviceTickPrivate *d = nullptr;
};

#endif // DEVICE_TICK_H
