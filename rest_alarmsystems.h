/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef REST_ALARMSYSTEMS_H
#define REST_ALARMSYSTEMS_H

#include <QVariant>

class ApiRequest;
class ApiResponse;
class EventEmitter;
class AlarmSystems;

/*! Alarm systems REST API broker.
    \param req - request data
    \param rsp - response data
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int AS_handleAlarmSystemsApi(const ApiRequest &req, ApiResponse &rsp, AlarmSystems &alarmSystems, EventEmitter *eventEmitter);
QVariantMap AS_AlarmSystemsToMap(const AlarmSystems &alarmSystems);

#endif // REST_ALARMSYSTEMS_H
