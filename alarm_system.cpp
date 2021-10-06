/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QTimer>
#include <array>
#include <deconz/timeref.h>
#include <deconz/dbg_trace.h>
#include "alarm_system.h"
#include "alarm_system_device_table.h"
#include "crypto/scrypt.h"
#include "database.h"
#include "event_emitter.h"
#include "ias_ace.h"

#define AS_ID_MASK 0xFF
#define AS_ID_CODE0 "as_%1_code0"

using AS_StateFunction = void (AlarmSystemPrivate::*)(const Event&);

// Event arm mask for each target state.
// The mask is used to filter incoming device events to trigger alarms.
static const quint16 targetArmMask[4] = {0,
                                         AS_ARM_MASK_ARMED_STAY,
                                         AS_ARM_MASK_ARMED_NIGHT,
                                         AS_ARM_MASK_ARMED_AWAY};

static const std::array<QLatin1String, 4> armModeStrings = {
    QLatin1String("disarmed"),
    QLatin1String("armed_stay"),
    QLatin1String("armed_night"),
    QLatin1String("armed_away")
};

class AlarmSystemPrivate
{
public:
    void setState(AS_StateFunction state);
    void startStateTimer();
    void stopTimer();

    void stateDisarmed(const Event &event);
    void stateArmed(const Event &event);
    void stateExitDelay(const Event &event);
    void stateEntryDelay(const Event &event);
    void stateInAlarm(const Event &event);
    void stateArming(const Event &event);

    void updateArmStateAndPanelStatus();
    void updateTargetStateValues();
    void setSecondsRemaining(uint secs);

    AlarmSystem *q = nullptr;
    AS_DeviceTable *devTable = nullptr;

    AS_ArmMode targetState = AS_ArmModeDisarmed;

    // target state parmeteres are set when a state is entered
    int exitDelay = 0;
    int entryDelay = 0;
    int triggerDuration = 0;
    int armMask = 0;

    QTimer *timer = nullptr;
    deCONZ::SteadyTimeRef tState{0};
    AS_StateFunction curState = &AlarmSystemPrivate::stateDisarmed;
};

void AlarmSystemPrivate::setState(AS_StateFunction state)
{
    if (state != curState)
    {
        curState = state;
    }

    updateArmStateAndPanelStatus();
}

void AlarmSystemPrivate::startStateTimer()
{
    tState = deCONZ::steadyTimeRef();
    timer->stop();
    timer->start(1000);
}

void AlarmSystemPrivate::stopTimer()
{
    timer->stop();
}

/*! Event handler for disarmed state.
 */
void AlarmSystemPrivate::stateDisarmed(const Event &event)
{
    Q_UNUSED(event)
}

/*! Event handler for exit state.
 */
void AlarmSystemPrivate::stateExitDelay(const Event &event)
{
    if (event.what() == REventTimerFired)
    {
        const deCONZ::TimeMs dt = deCONZ::steadyTimeRef() - tState;

        if (deCONZ::TimeSeconds{exitDelay} < dt)
        {
            setSecondsRemaining(0);
            stopTimer();

            if (targetState == AS_ArmModeDisarmed)
            {
                setState(&AlarmSystemPrivate::stateDisarmed);
            }
            else
            {
                startStateTimer();
                setState(&AlarmSystemPrivate::stateArming);
            }
        }
        else
        {
            setSecondsRemaining((exitDelay * 1000 - dt.val) / 1000);
        }
    }
}

/*! Event handler for entry state.
 */
void AlarmSystemPrivate::stateEntryDelay(const Event &event)
{
    if (event.what() == REventTimerFired)
    {
        const deCONZ::TimeMs dt = deCONZ::steadyTimeRef() - tState;

        if (deCONZ::TimeSeconds{entryDelay} < dt)
        {
            setSecondsRemaining(0);
            startStateTimer();
            setState(&AlarmSystemPrivate::stateInAlarm);
        }
        else
        {
            setSecondsRemaining((entryDelay * 1000 - dt.val) / 1000);
        }
    }
}

/*! Event handler for armed states.
 */
void AlarmSystemPrivate::stateArmed(const Event &event)
{
    if (event.what() == REventDeviceAlarm)
    {
        DBG_Printf(DBG_INFO, "[AS] received device alarm, data: 0x%08X\n", event.num());
        if ((event.num() & armMask) == 0)
        {
        }
        else if ((event.num() & AS_ID_MASK) == q->id())
        {
            setSecondsRemaining(entryDelay);
            setState(&AlarmSystemPrivate::stateEntryDelay);
            startStateTimer();
        }
    }
}

/*! Event handler for in_alarm state.
 */
void AlarmSystemPrivate::stateInAlarm(const Event &event)
{
    if (event.what() == REventTimerFired)
    {
        if (deCONZ::TimeSeconds{triggerDuration} < deCONZ::steadyTimeRef() - tState)
        {
            stopTimer();
            setState(&AlarmSystemPrivate::stateArmed);
        }
        else
        {
            DBG_Printf(DBG_INFO, "AS in alarm\n");
            // TODO emit event
        }
    }
}

/*! Event handler for arming states.
 */
void AlarmSystemPrivate::stateArming(const Event &event)
{
    if (event.what() == REventTimerFired)
    {
        stopTimer();
        setState(&AlarmSystemPrivate::stateArmed);
    }
}

/*! Update 'state/armstate', which maps directly to IAS ACE panel status.
 */
void AlarmSystemPrivate::updateArmStateAndPanelStatus()
{
    ResourceItem *item = q->item(RStateArmState);
    DBG_Assert(item);

    if (!item)
    {
        return;
    }

    quint8 status = item->toNumber() & 0xFF;

    if (curState == &AlarmSystemPrivate::stateDisarmed)
    {
        status = IAS_ACE_PANEL_STATUS_PANEL_DISARMED;
    }
    else if (curState == &AlarmSystemPrivate::stateArmed)
    {
        if      (targetState == AS_ArmModeArmedAway)  { status = IAS_ACE_PANEL_STATUS_ARMED_AWAY; }
        else if (targetState == AS_ArmModeArmedStay)  { status = IAS_ACE_PANEL_STATUS_ARMED_STAY; }
        else if (targetState == AS_ArmModeArmedNight) { status = IAS_ACE_PANEL_STATUS_ARMED_NIGHT; }
    }
    else if (curState == &AlarmSystemPrivate::stateArming)
    {
        if      (targetState == AS_ArmModeArmedAway)  { status = IAS_ACE_PANEL_STATUS_ARMING_AWAY; }
        else if (targetState == AS_ArmModeArmedStay)  { status = IAS_ACE_PANEL_STATUS_ARMING_STAY; }
        else if (targetState == AS_ArmModeArmedNight) { status = IAS_ACE_PANEL_STATUS_ARMING_NIGHT; }
    }
    else if (curState == &AlarmSystemPrivate::stateInAlarm)
    {
        status = IAS_ACE_PANEL_STATUS_IN_ALARM;
    }
    else if (curState == &AlarmSystemPrivate::stateEntryDelay)
    {
        status = IAS_ACE_PANEL_STATUS_ENTRY_DELAY;
    }
    else if (curState == &AlarmSystemPrivate::stateExitDelay)
    {
        status = IAS_ACE_PANEL_STATUS_EXIT_DELAY;
    }

    if (status != item->toNumber())
    {
        item->setValue(status);
        emit q->eventNotify(Event(q->prefix(), item->descriptor().suffix, q->idString(), item));
    }
}

/*! Each arm mode has it's own configuration parameters, this function
    updates exit, entry and trigger durations as well as the arm mask.
 */
void AlarmSystemPrivate::updateTargetStateValues()
{
    DBG_Assert(targetState <= AS_ArmModeArmedAway);
    if (targetState > AS_ArmModeArmedAway)
    {
        return;
    }

    {
        const char* exitSuffix[4]    = { RConfigDisarmedExitDelay,
                                         RConfigArmedStayExitDelay,
                                         RConfigArmedNightExitDelay,
                                         RConfigArmedAwayExitDelay };

        exitDelay = q->item(exitSuffix[targetState])->toNumber();
    }

    {
        const char* entrySuffix[4]   = { RConfigDisarmedEntryDelay,
                                         RConfigArmedStayEntryDelay,
                                         RConfigArmedNightEntryDelay,
                                         RConfigArmedAwayEntryDelay };

        entryDelay = q->item(entrySuffix[targetState])->toNumber();
    }

    if (targetState > AS_ArmModeDisarmed)
    {
        const char* triggerSuffix[4] = { RInvalidSuffix,  // no trigger duration in disarmed state
                                         RConfigArmedStayExitDelay,
                                         RConfigArmedNightExitDelay,
                                         RConfigArmedAwayTriggerDuration };

        triggerDuration = q->item(triggerSuffix[targetState])->toNumber();
    }
    else
    {
        triggerDuration = 0;
    }

    armMask = targetArmMask[targetState];
}

/*! Sets the timout \p secs for states which have a duration.
 */
void AlarmSystemPrivate::setSecondsRemaining(uint secs)
{
    DBG_Assert(secs <= UINT8_MAX);
    ResourceItem *item = q->item(RStateSecondsRemaining);
    if (item && item->toNumber() != secs && secs <= UINT8_MAX)
    {
        item->setValue(secs);
        emit q->eventNotify(Event(q->prefix(), item->descriptor().suffix, q->idString(), item));
    }
}

AlarmSystem::AlarmSystem(AlarmSystemId id, EventEmitter *eventEmitter, AS_DeviceTable *devTable, QObject *parent) :
    QObject(parent),
    Resource(RAlarmSystems)
{
    d = new AlarmSystemPrivate;
    d->q = this;
    d->devTable = devTable;

    d->timer = new QTimer(this);
    d->timer->setSingleShot(false);
    connect(d->timer, &QTimer::timeout, this, &AlarmSystem::timerFired);

    {
        ResourceItem *item = addItem(DataTypeUInt8, RConfigAlarmSystemId);
        item->setIsPublic(false);
        item->setValue(id);
    }
    addItem(DataTypeUInt32, RStateArmState)->setValue(IAS_ACE_PANEL_STATUS_NOT_READY_TO_ARM);
    addItem(DataTypeBool, RConfigConfigured)->setValue(false);
    addItem(DataTypeString, RConfigArmMode)->setValue(QString(armModeStrings[AS_ArmModeDisarmed]));
    addItem(DataTypeString, RAttrId)->setValue(QString::number(id));
    addItem(DataTypeString, RAttrName);

    addItem(DataTypeUInt32, RStateSecondsRemaining)->setValue(0);

    addItem(DataTypeUInt8, RConfigDisarmedEntryDelay)->setValue(0);
    addItem(DataTypeUInt8, RConfigDisarmedExitDelay)->setValue(0);

    addItem(DataTypeUInt8, RConfigArmedStayEntryDelay)->setValue(120);
    addItem(DataTypeUInt8, RConfigArmedStayExitDelay)->setValue(120);
    addItem(DataTypeUInt8, RConfigArmedStayTriggerDuration)->setValue(120);

    addItem(DataTypeUInt8, RConfigArmedNightEntryDelay)->setValue(120);
    addItem(DataTypeUInt8, RConfigArmedNightExitDelay)->setValue(120);
    addItem(DataTypeUInt8, RConfigArmedNightTriggerDuration)->setValue(120);

    addItem(DataTypeUInt8, RConfigArmedAwayEntryDelay)->setValue(120);
    addItem(DataTypeUInt8, RConfigArmedAwayExitDelay)->setValue(120);
    addItem(DataTypeUInt8, RConfigArmedAwayTriggerDuration)->setValue(120);

    d->updateTargetStateValues();

    connect(this, &AlarmSystem::eventNotify, eventEmitter, &EventEmitter::enqueueEvent);
}

AlarmSystem::~AlarmSystem()
{
    delete d;
}

/*! Main entry point to handle internal and external events.

    The \p event gets forwarded to the current state handler.
 */
void AlarmSystem::handleEvent(const Event &event)
{
    if (event.resource() == RAlarmSystems && event.what() == RConfigArmMode) // target state changed?
    {
        const QString &armMode = item(event.what())->toString();

        const AS_ArmMode oldTargetState = d->targetState;
        const AS_ArmMode mode = AS_ArmModeFromString(armMode);

        if (mode != AS_ArmModeMax)
        {
            d->targetState = mode;
        }
        else
        {
            return; // invalid target arm mode request, should not happen
        }

        d->updateTargetStateValues();

        if (oldTargetState == d->targetState)
        {
            return;
        }

        d->setSecondsRemaining(d->exitDelay); // set early for correct numbers in state/panel events
        d->setState(&AlarmSystemPrivate::stateExitDelay);
        d->startStateTimer();
    }
    else
    {
        (d->*d->curState)(event);
    }
}

void AlarmSystem::didSetValue(ResourceItem *i)
{
    if (!i || !i->descriptor().suffix)
    {
        return;
    }

    emit eventNotify(Event(prefix(), i->descriptor().suffix, idString(), i));

    // only attr/* and config/*
    if (i->descriptor().suffix[0] != 'c' && i->descriptor().suffix[0] != 'a')
    {
        return;
    }

    const std::array<const char*, 13> store = {
        RAttrName,
        RConfigArmMode,
        RConfigDisarmedEntryDelay, RConfigDisarmedExitDelay,
        RConfigArmedAwayEntryDelay, RConfigArmedAwayExitDelay, RConfigArmedAwayTriggerDuration,
        RConfigArmedStayEntryDelay, RConfigArmedStayExitDelay, RConfigArmedStayTriggerDuration,
        RConfigArmedNightEntryDelay, RConfigArmedNightExitDelay, RConfigArmedNightTriggerDuration
    };

    if (std::find(store.cbegin(), store.cend(), i->descriptor().suffix) != store.cend())
    {
        DB_AlarmSystemResourceItem dbItem;
        dbItem.alarmSystemId = id();
        dbItem.timestamp = deCONZ::systemTimeRef().ref;
        dbItem.suffix = i->descriptor().suffix;
        if (i->descriptor().type == DataTypeString)
        {
            dbItem.value = i->toString().toStdString();
        }
        else
        {
            dbItem.value = std::to_string(i->toNumber());
        }
        DB_StoreAlarmSystemResourceItem(dbItem);
    }
}

/*! Returns true if the \p code can be verified.

    The verification is only done if an entry for \p srcExtAddress exists
    in the alarm system device table.
 */
bool AlarmSystem::isValidCode(const QString &code, quint64 srcExtAddress)
{
    if (srcExtAddress != 0)
    {
        const AS_DeviceEntry &entry = d->devTable->get(srcExtAddress);

        if (!isValid(entry) || entry.alarmSystemId != id())
        {
            return false;
        }
    }

    DB_Secret sec;
    sec.uniqueId = QString(AS_ID_CODE0).arg(id()).toStdString();

    if (DB_LoadSecret(sec))
    {
        if (CRYPTO_ScryptVerify(sec.secret, code.toStdString()))
        {
            return true;
        }
    }

    return false;
}

AlarmSystemId AlarmSystem::id() const
{
    return item(RConfigAlarmSystemId)->toNumber();
}

const QString &AlarmSystem::idString() const
{
    return item(RAttrId)->toString();
}

/*! Returnes the response status for IAS ACE device panel status request.
 */
quint8 AlarmSystem::iasAcePanelStatus() const
{
    return item(RStateArmState)->toNumber() & 0xFF;
}

/*! Returns the remaining time in seconds, for entry and exit states, 0 for all other states.
 */
uint AlarmSystem::secondsRemaining() const
{
    if (d->curState == &AlarmSystemPrivate::stateEntryDelay || d->curState == &AlarmSystemPrivate::stateExitDelay)
    {
        return item(RStateSecondsRemaining)->toNumber();
    }

    return 0;
}

/*! Returns 'state/armstate', which matches the panel IAS ACE panel status.
 */
QLatin1String AlarmSystem::armStateString() const
{
    return IAS_PanelStatusToString(iasAcePanelStatus());
}

/*! Returns the configured target arm mode.

    note that the current state can be different if the state machine is in a transition.
 */
AS_ArmMode AlarmSystem::targetArmMode() const
{
    return d->targetState;
}

/*! Sets the target arm mode.

    The state machine will pick up a changed mode and transition accordingly.
 */
bool AlarmSystem::setTargetArmMode(AS_ArmMode targetArmMode)
{
    if (targetArmMode >= AS_ArmModeMax)
    {
        return false;
    }

    if (targetArmMode == d->targetState)
    {
        return true;
    }

    setValue(RConfigArmMode, QString(AS_ArmModeToString(targetArmMode)));

    return true;
}

bool AlarmSystem::addDevice(const QString &uniqueId, quint32 flags)
{
    return d->devTable->put(uniqueId, flags, quint8(id()));
}

bool AlarmSystem::removeDevice(const QLatin1String &uniqueId)
{
    return d->devTable->erase(uniqueId);
}

const AS_DeviceTable *AlarmSystem::deviceTable() const
{
    return d->devTable;
}

/*! Sets or updated the PIN code for \p index.

    The code is stored encryped in the database.
 */
bool AlarmSystem::setCode(int index, const QString &code)
{
    if (code.isEmpty())
    {
        return false;
    }

    const std::string code0 = code.toStdString();

    DB_Secret sec;
    sec.uniqueId = QString("as_%1_code%2").arg(id()).arg(index).toStdString();
    sec.secret = CRYPTO_ScryptPassword(code0, CRYPTO_GenerateSalt());
    sec.state = 1;

    if (sec.secret.empty())
    {
        return false;
    }

    if (DB_StoreSecret(sec))
    {
        setValue(RConfigConfigured, true);
        return true;
    }

    return false;
}

/*! Starts the alarm system operational mode.
 */
void AlarmSystem::start()
{
    const QString &armMode = item(RConfigArmMode)->toString();

    if      (armMode == armModeStrings[AS_ArmModeDisarmed])   { d->targetState = AS_ArmModeDisarmed;   d->setState(&AlarmSystemPrivate::stateDisarmed); }
    else if (armMode == armModeStrings[AS_ArmModeArmedAway])  { d->targetState = AS_ArmModeArmedAway;  d->setState(&AlarmSystemPrivate::stateArmed); }
    else if (armMode == armModeStrings[AS_ArmModeArmedStay])  { d->targetState = AS_ArmModeArmedStay;  d->setState(&AlarmSystemPrivate::stateArmed); }
    else if (armMode == armModeStrings[AS_ArmModeArmedNight]) { d->targetState = AS_ArmModeArmedNight; d->setState(&AlarmSystemPrivate::stateArmed); }

    d->updateArmStateAndPanelStatus();
    d->updateTargetStateValues();

    DB_Secret sec;
    sec.uniqueId = QString(AS_ID_CODE0).arg(id()).toStdString();

    bool configured = DB_LoadSecret(sec);
    item(RConfigConfigured)->setValue(configured);
}

void AlarmSystem::timerFired()
{
    handleEvent(Event(RAlarmSystems, REventTimerFired, 0));
}

AlarmSystems::AlarmSystems()
{
}

AlarmSystems::~AlarmSystems()
{
    for (auto *alarmSys : alarmSystems)
    {
        alarmSys->deleteLater();
    }

    alarmSystems.clear();
}

QLatin1String AS_ArmModeToString(AS_ArmMode armMode)
{
    Q_ASSERT(size_t(armMode) < armModeStrings.size());
    return armModeStrings[armMode];
}

AS_ArmMode AS_ArmModeFromString(const QString &armMode)
{
    const auto i = std::find_if(armModeStrings.cbegin(), armModeStrings.cend(), [&armMode](const auto &str) {
        return str == armMode;
    });

    if (i != armModeStrings.cend())
    {
        return static_cast<AS_ArmMode>(std::distance(armModeStrings.cbegin(), i));
    }

    return AS_ArmModeMax;
}

AlarmSystem *AS_GetAlarmSystemForDevice(quint64 extAddress, AlarmSystems &alarmSystems)
{
    AlarmSystem *result = nullptr;

    for (auto *alarmSys : alarmSystems.alarmSystems)
    {
        const AS_DeviceEntry &entry = alarmSys->deviceTable()->get(extAddress);
        if (isValid(entry) && entry.alarmSystemId == alarmSys->id())
        {
            result = alarmSys;
            break;
        }
    }

    return result;
}

const AlarmSystem *AS_GetAlarmSystem(AlarmSystemId alarmSystemId, const AlarmSystems &alarmSystems)
{
    auto i = std::find_if(alarmSystems.alarmSystems.begin(), alarmSystems.alarmSystems.end(), [alarmSystemId](auto &as){
        return as->item(RConfigAlarmSystemId)->toNumber() == alarmSystemId;
    });

    if (i != alarmSystems.alarmSystems.end())
    {
        return *i;
    }

    return nullptr;
}

AlarmSystem *AS_GetAlarmSystem(AlarmSystemId alarmSystemId, AlarmSystems &alarmSystems)
{
    auto i = std::find_if(alarmSystems.alarmSystems.begin(), alarmSystems.alarmSystems.end(), [alarmSystemId](auto &as){
        return as->id() == alarmSystemId;
    });

    if (i != alarmSystems.alarmSystems.end())
    {
        return *i;
    }

    return nullptr;
}

void DB_LoadAlarmSystems(AlarmSystems &alarmSystems, AS_DeviceTable *devTable, EventEmitter *eventEmitter)
{
    for (AlarmSystemId alarmSystemId = 0; alarmSystemId < 4; alarmSystemId++)
    {
        const auto ritems = DB_LoadAlarmSystemResourceItems(alarmSystemId);

        if (ritems.empty())
        {
            continue;
        }

        AlarmSystem *alarmSys = new AlarmSystem(alarmSystemId, eventEmitter, devTable);

        alarmSystems.alarmSystems.push_back(alarmSys);

        for (const auto &dbItem : ritems)
        {
            if (dbItem.value.empty())
            {
                continue;
            }

            ResourceItem *item = alarmSys->item(dbItem.suffix);

            if (!item)
            {
                continue;
            }

            if (item->descriptor().type == DataTypeString)
            {
                item->setValue(QString::fromStdString(dbItem.value));
            }
            else if (item->descriptor().type == DataTypeUInt8)
            {
                qint64 num = strtol(dbItem.value.c_str(), nullptr, 10);
                item->setValue(num);
            }
            else
            {
                DBG_Printf(DBG_INFO, "[AS] database load item, %s, not supported\n", dbItem.suffix);
            }
        }

        alarmSys->start();
    }
}

/*! Creates a "default" alarm system with id "1", which is always present.
 */
void AS_InitDefaultAlarmSystem(AlarmSystems &alarmSystems, AS_DeviceTable *devTable, EventEmitter *eventEmitter)
{
    if (AS_GetAlarmSystem(1, alarmSystems)) // already exists
    {
        return;
    }

    AlarmSystemId id = 1;
    AlarmSystem *alarmSys = new AlarmSystem(id, eventEmitter, devTable);

    alarmSystems.alarmSystems.push_back(alarmSys);


    {
        DB_AlarmSystem dbAlarmSys;
        dbAlarmSys.id = id;
        dbAlarmSys.timestamp = deCONZ::systemTimeRef().ref;

        DB_StoreAlarmSystem(dbAlarmSys);
    }

    alarmSys->setValue(RAttrName, QString("default"));
}
