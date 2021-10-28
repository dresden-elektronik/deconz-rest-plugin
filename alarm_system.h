/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef ALARM_SYSTEM_H
#define ALARM_SYSTEM_H

#include <QObject>
#include <vector>
#include "resource.h"

/*! \class AlarmSystem

    This class implements a stateful alarm system. The system is controlled with events from physical devices and the REST API.

    A general overview can be found at https://dresden-elektronik.github.io/deconz-rest-doc/endpoints/alarmsystems

    The state machine mimics a typical alarm system, similar to implementations in home automation systems.

        https://www.npmjs.com/package/homebridge-alarm-panel
        https://www.home-assistant.io/integrations/manual

    There are four target states:
      disarmed
      armed_away
      armed_stay
      armed_night

    A: alarm system id
    M: alarm mask
       0000 0001 Away
       0000 0010 Stay
       0000 0100 Night

    RConfigAlarmSystemId:  uint32   0000 0000 0000 0000 0000 0000 0000 0000
                                    0000 0000 0000 0000 0000 0000 AAAA AAAA

    REventDeviceAlarm        uint32   0000 0000 0000 0000 MMMM MMMM AAAA AAAA

*/

#define AS_ARM_MASK_ARMED_AWAY  0x0100
#define AS_ARM_MASK_ARMED_STAY  0x0200
#define AS_ARM_MASK_ARMED_NIGHT 0x0400

class Event;
class EventEmitter;
class AS_DeviceTable;
class AlarmSystemPrivate;

enum AS_ArmMode
{
    AS_ArmModeDisarmed    = 0,
    AS_ArmModeArmedStay   = 1,
    AS_ArmModeArmedNight  = 2,
    AS_ArmModeArmedAway   = 3,

    AS_ArmModeMax
};

using AlarmSystemId = quint32;

class AlarmSystem : public QObject,
                    public Resource
{
    Q_OBJECT

public:
    AlarmSystem(AlarmSystemId id, EventEmitter *eventEmitter, AS_DeviceTable *devTable, QObject *parent = nullptr);
    ~AlarmSystem();
    void handleEvent(const Event &event);
    void didSetValue(ResourceItem *i) override;
    bool isValidCode(const QString &code, quint64 srcExtAddress);
    AlarmSystemId id() const;
    const QString &idString() const;
    quint8 iasAcePanelStatus() const;
    uint secondsRemaining() const;
    QLatin1String armStateString() const;
    AS_ArmMode targetArmMode() const;
    bool setTargetArmMode(AS_ArmMode targetArmMode);
    bool addDevice(const QString &uniqueId, quint32 flags);
    bool removeDevice(const QLatin1String &uniqueId);
    const AS_DeviceTable *deviceTable() const;
    bool setCode(int index, const QString &code);
    void start();

Q_SIGNALS:
    void eventNotify(const Event&);

private Q_SLOTS:
    void timerFired();

private:
    AlarmSystemPrivate *d = nullptr;
};

/*! \class AlarmSystems

    RAII wrapper to hold \c AlarmSystem objects.
 */
class AlarmSystems
{
public:
    AlarmSystems();
    ~AlarmSystems();

    std::vector<AlarmSystem*> alarmSystems;
};

void DB_LoadAlarmSystems(AlarmSystems &alarmSystems, AS_DeviceTable *devTable, EventEmitter *eventEmitter);
void AS_InitDefaultAlarmSystem(AlarmSystems &alarmSystems, AS_DeviceTable *devTable, EventEmitter *eventEmitter);

QLatin1String AS_ArmModeToString(AS_ArmMode armMode);
AS_ArmMode AS_ArmModeFromString(const QString &armMode);
AlarmSystem *AS_GetAlarmSystemForDevice(quint64 extAddress, AlarmSystems &alarmSystems);
const AlarmSystem *AS_GetAlarmSystem(AlarmSystemId alarmSystemId, const AlarmSystems &alarmSystems);
AlarmSystem *AS_GetAlarmSystem(AlarmSystemId alarmSystemId, AlarmSystems &alarmSystems);

#endif // ALARM_SYSTEM_H
