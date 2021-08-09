 /*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <string.h>
#include "alarm_system.h"
#include "alarm_system_device_table.h"
#include "alarm_system_event_handler.h"
#include "de_web_plugin_private.h"
#include "ias_ace.h"
#include "websocket_server.h"

// TODO remove dependency on plugin

/*! For keypads mirror panel state and seconds remaining.
 */
static void mirrorKeypadAlarmSystemState(const AlarmSystem *alarmSys, EventEmitter *eventEmitter)
{
    const AS_DeviceTable *devTable = alarmSys->deviceTable();
    for (size_t i = 0; i < devTable->size(); i++)
    {
        const AS_DeviceEntry &entry = devTable->at(i);
        if (isValid(entry) && entry.flags & AS_ENTRY_FLAG_IAS_ACE)
        {
            Resource *r = plugin->getResource(RSensors, QLatin1String(entry.uniqueId, entry.uniqueIdSize));

            if (!r) { continue; }

            ResourceItem *panel = r->item(RStatePanel);
            ResourceItem *secondsRemaining = r->item(RStateSecondsRemaining);

            if (!panel || !secondsRemaining) { continue; }

            secondsRemaining->setValue(alarmSys->secondsRemaining());
            panel->setValue(QString(alarmSys->armStateString()));

            if (panel->needPushChange())
            {
                eventEmitter->enqueueEvent(Event(r->prefix(), panel->descriptor().suffix, r->item(RAttrId)->toString()));
            }

            if (secondsRemaining->needPushChange())
            {
                eventEmitter->enqueueEvent(Event(r->prefix(), secondsRemaining->descriptor().suffix, r->item(RAttrId)->toString()));
            }
        }
    }
}

static void pushEventToWebsocket(const Event &event, AlarmSystem *alarmSys, WebSocketServer *webSocket)
{
    Q_ASSERT(event.what());
    {
        char type = event.what()[0];
        if (!(type == 's' || type == 'c' || type == 'a'))
        {
            return; // only interested in attr/*, state/* and config/*
        }
    }

    int suffixOffset = 0;

    {
        const char *delim = strchr(event.what(), '/');
        if (!delim)
        {
            return;
        }

        suffixOffset = delim - event.what() + 1;
    }


    ResourceItem *item = alarmSys->item(event.what());

    if (!item)
    {
        return;
    }

    if (!(item->needPushSet() || item->needPushChange()))
    {
        return; // already pushed
    }

    QVariantMap map;
    map[QLatin1String("t")] = QLatin1String("event");
    map[QLatin1String("e")] = QLatin1String("changed");
    map[QLatin1String("r")] = QLatin1String("alarmsystems");
    map[QLatin1String("id")] = alarmSys->idString();

    {
        QVariantMap map2;

        for (int i = 0; i < alarmSys->itemCount(); i++)
        {
            item = alarmSys->itemForIndex(size_t(i));
            Q_ASSERT(item);
            const char *suffix = item->descriptor().suffix;
            if (*suffix == *event.what() && item->isPublic())
            {
                item->clearNeedPush();
                if (suffix == RStateArmState)
                {
                    map2[QLatin1String(suffix + suffixOffset)] = alarmSys->armStateString();
                }
                else
                {
                    map2[QLatin1String(suffix + suffixOffset)] = item->toVariant();
                }
            }
        }

        map[QLatin1String(event.what(), suffixOffset - 1)] = map2;
    }

    webSocket->broadcastTextMessage(Json::serialize(map));
}


/*! Global handler for alarm system related events.
 */
void AS_HandleAlarmSystemEvent(const Event &event, AlarmSystems &alarmSystems, EventEmitter *eventEmitter, WebSocketServer *webSocket)
{
    for (auto *alarmSys : alarmSystems.alarmSystems)
    {
        alarmSys->handleEvent(event);

        if (event.what() == RStateArmState || event.what() == RStateSecondsRemaining)
        {
            mirrorKeypadAlarmSystemState(alarmSys, eventEmitter);
        }

        if (event.resource() == RAlarmSystems && event.id() == alarmSys->idString())
        {
            pushEventToWebsocket(event, alarmSys, webSocket);
        }
    }
}

/*! Filter for events which are interesting for the alarm system.
 */
static bool isAlarmSystemDeviceEvent(const Event &event)
{
    if (event.what()[0] != 's') // only interested in state/*
    {
        return false;
    }

    if (event.resource() == RSensors)
    {
        if (event.what() == RStatePresence)    { return event.num() > 0; }
        if (event.what() == RStateOpen)        { return event.num() > 0; }
        if (event.what() == RStateVibration)   { return event.num() > 0; }
        if (event.what() == RStateButtonEvent) { return true; }
        if (event.what() == RStateAction)
        {
            if (event.num() >= IAS_ACE_CMD_EMERGENCY && event.num() <= IAS_ACE_CMD_PANIC)
            {
                return true;
            }
        }
    }
    else if (event.resource() == RLights)
    {
        if (event.what() == RStateOn) { return event.num() > 0; }
    }

    return false;
}

/*! For devices which are added to an alarm system transform matching events into REventDeviceAlarm events.

    When armed, the alarm system enters the entry delay state.
 */
void AS_HandleAlarmSystemDeviceEvent(const Event &event, const AS_DeviceTable *devTable, EventEmitter *eventEmitter)
{
    if (!isAlarmSystemDeviceEvent(event))
    {
        return;
    }

    Resource *r = plugin->getResource(event.resource(), event.id());
    if (!r)
    {
        return;
    }

    ResourceItem *uniqueId = r->item(RAttrUniqueId);
    if (!uniqueId)
    {
        return;
    }

    const AS_DeviceEntry &entry = devTable->get(uniqueId->toString());
    if (!isValid(entry))
    {
        return;
    }

    ResourceItem *item = r->item(event.what());
    if (!item)
    {
        return;
    }

    int eventData = entry.alarmSystemId;
     // arm mask
    eventData |= entry.flags & (AS_ENTRY_FLAG_ARMED_AWAY | AS_ENTRY_FLAG_ARMED_STAY | AS_ENTRY_FLAG_ARMED_NIGHT);

    eventEmitter->enqueueEvent(Event(RAlarmSystems, REventDeviceAlarm, eventData));
}
