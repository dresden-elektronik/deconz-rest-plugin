/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef ALARM_SYSTEM_EVENT_HANDLER_H
#define ALARM_SYSTEM_EVENT_HANDLER_H

class Event;
class EventEmitter;
class AlarmSystems;
class AS_DeviceTable;
class WebSocketServer;

void AS_HandleAlarmSystemEvent(const Event &event, AlarmSystems &alarmSystems, EventEmitter *eventEmitter, WebSocketServer *webSocket);
void AS_HandleAlarmSystemDeviceEvent(const Event &event, const AS_DeviceTable *devTable, EventEmitter *eventEmitter);

#endif // ALARM_SYSTEM_EVENT_HANDLER_H
