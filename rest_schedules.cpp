/*
 * Copyright (C) 2013 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QString>
#include <QTcpSocket>
#include <QHttpRequestHeader>
#include <QVariantMap>
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "json.h"

/*! Inits the schedules manager.
 */
void DeRestPluginPrivate::initSchedules()
{
    scheduleTimer = new QTimer(this);
    scheduleTimer->setSingleShot(false);
    connect(scheduleTimer, SIGNAL(timeout()),
            this, SLOT(scheduleTimerFired()));
    scheduleTimer->start(SCHEDULE_CHECK_PERIOD);
}

/*! Schedules REST API broker.
    \param req - request data
    \param rsp - response data
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::handleSchedulesApi(ApiRequest &req, ApiResponse &rsp)
{
    if (req.path[2] != "schedules")
    {
        return REQ_NOT_HANDLED;
    }

    // GET /api/<apikey>/schedules
    if ((req.path.size() == 3) && (req.hdr.method() == "GET"))
    {
        return getAllSchedules(req, rsp);
    }
    // POST /api/<apikey>/schedules
    else if ((req.path.size() == 3) && (req.hdr.method() == "POST"))
    {
        return createSchedule(req, rsp);
    }
    // GET /api/<apikey>/schedules/<id>
    else if ((req.path.size() == 4) && (req.hdr.method() == "GET"))
    {
        return getScheduleAttributes(req, rsp);
    }
    // PUT /api/<apikey>/schedules/<id>
    else if ((req.path.size() == 4) && (req.hdr.method() == "PUT"))
    {
        return setScheduleAttributes(req, rsp);
    }
    // DELETE /api/<apikey>/schedules/<id>
    else if ((req.path.size() == 4) && (req.hdr.method() == "DELETE"))
    {
        return deleteSchedule(req, rsp);
    }

    return REQ_NOT_HANDLED;
}

/*! GET /api/<apikey>/schedules
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::getAllSchedules(const ApiRequest &req, ApiResponse &rsp)
{
    Q_UNUSED(req);
    rsp.httpStatus = HttpStatusOk;

    std::vector<Schedule>::const_iterator i = schedules.begin();
    std::vector<Schedule>::const_iterator end = schedules.end();

    for (; i != end; ++i)
    {
        QVariantMap mnode;

        mnode["name"] = i->name;
        rsp.map[i->id] = mnode;
    }

    if (rsp.map.isEmpty())
    {
        rsp.str = "{}"; // return empty object
    }

    return REQ_READY_SEND;
}

/*! POST /api/<apikey>/schedules
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::createSchedule(const ApiRequest &req, ApiResponse &rsp)
{
    rsp.httpStatus = HttpStatusOk;

    bool ok;
    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();

    rsp.httpStatus = HttpStatusOk;

    if (!ok || map.isEmpty())
    {
        rsp.list.append(errorToMap(ERR_INVALID_JSON, QString("/schedules"), QString("body contains invalid JSON")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    // check required parameters
    if (!(map.contains("command") && map.contains("time")))
    {
        rsp.list.append(errorToMap(ERR_MISSING_PARAMETER, QString("/schedules"), QString("missing parameters in body")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    Schedule schedule;

    // name
    if (map.contains("name") && (map["name"].type() == QVariant::String) && (map["name"].toString().length() <= 32))
    {
        schedule.name = map["name"].toString();

        std::vector<Schedule>::const_iterator i = schedules.begin();
        std::vector<Schedule>::const_iterator end = schedules.end();

        for (; i != end; ++i)
        {
            if (i->name == schedule.name)
            {   // append a number to prevent duplicated names
                schedule.name.append(QString(" %1").arg(schedules.size() + 1));
                break;
            }
        }
    } // else use default name "schedule"

    // description
    if (map.contains("description") && (map["description"].type() == QVariant::String) && (map["description"].toString().length() <= 64))
    {
        schedule.name = map["description"].toString();
    } // else ignore use empty description

    // command
    if (map.contains("command") && (map["command"].type() == QVariant::Map))
    {
        QVariantMap cmd = map["command"].toMap();

        if (cmd.isEmpty() || !cmd.contains("address") || !cmd.contains("method") || !cmd.contains("body"))
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/schedules"), QString("invalid value, %1, for parameter command").arg(map["command"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

        schedule.command = deCONZ::jsonStringFromMap(cmd);
    }
    else
    {
        rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/schedules"), QString("invalid value, %1, for parameter command").arg(map["command"].toString())));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    // time
    if (map.contains("time"))
    {
        schedule.time = map["time"].toString();
        schedule.datetime = QDateTime::fromString(schedule.time, Qt::ISODate);
        schedule.datetime.setTimeSpec(Qt::UTC);

        if (!schedule.datetime.isValid())
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/schedules"), QString("invalid value, %1, for parameter time").arg(map["time"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }

    // search new id
    std::vector<Schedule>::const_iterator i = schedules.begin();
    std::vector<Schedule>::const_iterator end = schedules.end();

    uint id = 1;
    uint idmax = 0;
    for (; i != end; ++i)
    {
        uint id2 = i->id.toUInt();
        if (idmax < id2)
        {
            idmax = id2;
        }

        if (id == id2)
        {
            id = ++idmax;
        }
    }

    schedule.id = QString::number(id);

    if (schedule.name.isEmpty())
    {
        schedule.name = QString("Schedule %1").arg(schedule.id);
    }

    // append schedule
    schedules.push_back(schedule);

    QVariantMap rspItem;
    QVariantMap rspItemState;
    rspItemState["id"] = schedule.id;
    rspItem["success"] = rspItemState;
    rsp.list.append(rspItem);
    rsp.httpStatus = HttpStatusOk;

    queSaveDb(DB_SCHEDULES, DB_SHORT_SAVE_DELAY);
    return REQ_READY_SEND;
}

/*! GET /api/<apikey>/schedules/<id>
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::getScheduleAttributes(const ApiRequest &req, ApiResponse &rsp)
{
    QString id = req.path[3];

    std::vector<Schedule>::const_iterator i = schedules.begin();
    std::vector<Schedule>::const_iterator end = schedules.end();


    for (; i != end; ++i)
    {
        if (i->id == id)
        {
            rsp.map["name"] = i->name;
            rsp.map["description"] = i->description;
            rsp.map["command"] = i->command;
            rsp.map["time"] = i->time;
            rsp.httpStatus = HttpStatusOk;
            return REQ_READY_SEND;
        }
    }

    rsp.httpStatus = HttpStatusNotFound;
    rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/schedules/%1").arg(id), QString("resource, /schedules/%1, not available").arg(id)));

    return REQ_READY_SEND;
}

/*! PUT /api/<apikey>/schedules/<id>
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::setScheduleAttributes(const ApiRequest &req, ApiResponse &rsp)
{
    Q_UNUSED(req);
    Q_UNUSED(rsp);

    // TODO update schedule attributes

    return REQ_NOT_HANDLED;
}

/*! DELETE /api/<apikey>/schedules/<id>
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::deleteSchedule(const ApiRequest &req, ApiResponse &rsp)
{
    QString id = req.path[3];

    std::vector<Schedule>::iterator i = schedules.begin();
    std::vector<Schedule>::iterator end = schedules.end();


    for (; i != end; ++i)
    {
        if (i->id == id)
        {
            QVariantMap rspItem;
            rspItem["success"] = QString("/schedules/%1 deleted.").arg(id);
            rsp.list.append(rspItem);
            rsp.httpStatus = HttpStatusOk;

            schedules.erase(i);
            queSaveDb(DB_SCHEDULES, DB_SHORT_SAVE_DELAY);
            return REQ_NOT_HANDLED;
        }
    }

    rsp.httpStatus = HttpStatusNotFound;
    rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/schedules/%1").arg(id), QString("resource, /schedules/%1, not available").arg(id)));

    return REQ_NOT_HANDLED;
}

/*! Processes any schedules.
 */
void DeRestPluginPrivate::scheduleTimerFired()
{
    bool ok;
    std::vector<Schedule>::iterator i = schedules.begin();
    std::vector<Schedule>::iterator end = schedules.end();

    QDateTime now = QDateTime::currentDateTime().toUTC();

    for (; i != end; ++i)
    {
        qint64 diff = now.secsTo(i->datetime);
        if (diff <= 0)
        {
            Schedule schedule = *i;
            schedules.erase(i);

            DBG_Printf(DBG_INFO, "Schedule %s triggered at %s\n", qPrintable(i->name), qPrintable(now.toString()));

            QVariant var = Json::parse(schedule.command, ok);
            QVariantMap cmd = var.toMap();


            // check if fields are given
            if (!ok || cmd.isEmpty() || !cmd.contains("address") || !cmd.contains("method") || !cmd.contains("body"))
            {
                DBG_Printf(DBG_INFO, "Schedule ignored, invalid command %s\n", qPrintable(schedule.command));
                return;
            }
            QString method = cmd["method"].toString();
            QString address = cmd["address"].toString();
            QString content = deCONZ::jsonStringFromMap(cmd["body"].toMap());

            // check if fields contain data
            if (method.isEmpty() || address.isEmpty() || content.isEmpty())
            {
                DBG_Printf(DBG_INFO, "Schedule ignored, invalid command %s\n", qPrintable(schedule.command));
                return;
            }

            QHttpRequestHeader hdr(method, address);
            QStringList path = hdr.path().split('/');

            // first element in list is empty because of spli('/'):
            // [/][api] becomes [][api]
            if (!path.isEmpty() && path[0].isEmpty())
            {
                path.removeFirst();
            }

            ApiRequest req(hdr, path, NULL, content);
            ApiResponse rsp; // dummy

            DBG_Printf(DBG_INFO, "body: %s\n", qPrintable(content));

            if (handleLightsApi(req, rsp) == REQ_NOT_HANDLED)
            {
                if (handleGroupsApi(req, rsp) == REQ_NOT_HANDLED)
                {
                    DBG_Printf(DBG_INFO, "Schedule was neigher light nor group request.\n");
                }
            }

            return;
        }
        else
        {
            DBG_Printf(DBG_INFO, "Schedule %s diff %lld\n", qPrintable(i->name), diff);
        }
    }
}
