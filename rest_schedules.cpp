/*
 * Copyright (c) 2016 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QString>
#include <QTcpSocket>
#include <QVariantMap>
#include <QRegExp>
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
    // PUT, PATCH /api/<apikey>/schedules/<id>
    else if ((req.path.size() == 4) && (req.hdr.method() == "PUT" || req.hdr.method() == "PATCH"))
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
        if (i->state == Schedule::StateNormal)
        {
            QVariantMap mnode;

            mnode["name"] = i->name;
            mnode["description"] = i->description;
            mnode["command"] = i->jsonMap["command"];
            mnode["time"] = i->time;
            if (i->type == Schedule::TypeTimer)
            {
                mnode["starttime"] = i->starttime;
            }
            mnode["status"] = i->status;
            mnode["autodelete"] = i->autodelete;
            QString etag = i->etag;
            etag.remove('"'); // no quotes allowed in string
            mnode["etag"] = etag;
            rsp.map[i->id] = mnode;
        }
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

    Schedule schedule;

    if (!jsonToSchedule(req.content, schedule, &rsp))
    {
        return REQ_READY_SEND;
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
            rsp.map["command"] = i->jsonMap["command"];
            rsp.map["time"] = i->time;
            if (i->type == Schedule::TypeTimer)
            {
                rsp.map["starttime"] = i->starttime;
            }
            rsp.map["status"] = i->status;
            rsp.map["autodelete"] = i->autodelete;
            QString etag = i->etag;
            etag.remove('"'); // no quotes allowed in string
            rsp.map["etag"] = etag;
            rsp.httpStatus = HttpStatusOk;
            return REQ_READY_SEND;
        }
    }

    rsp.httpStatus = HttpStatusNotFound;
    rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/schedules/%1").arg(id), QString("resource, /schedules/%1, not available").arg(id)));

    return REQ_READY_SEND;
}

/*! PUT, PATCH /api/<apikey>/schedules/<id>
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::setScheduleAttributes(const ApiRequest &req, ApiResponse &rsp)
{
    QString id = req.path[3];

    std::vector<Schedule>::iterator i = schedules.begin();
    std::vector<Schedule>::iterator end = schedules.end();

    for (; i != end; ++i)
    {
        if ((i->id == id) && (i->state == Schedule::StateNormal))
        {
            bool ok;
            QVariant var = Json::parse(req.content, ok);
            QVariantMap map = var.toMap();

            if (!ok || map.isEmpty())
            {
                rsp.list.append(errorToMap(ERR_INVALID_JSON, QString("/schedules/%1").arg(id), QString("body contains invalid JSON")));
                rsp.httpStatus = HttpStatusBadRequest;
                return false;
            }

            if (map.contains("name") && (map["name"].type() == QVariant::String))
            {
                QString name = map["name"].toString();

                if (name.size() > 0 && name.size() <= 32)
                {
                    i->name = name;
                    i->jsonMap["name"] = map["name"];

                    QVariantMap rspItem;
                    QVariantMap rspItemState;
                    rspItemState[QString("/schedules/%1/name").arg(id)] = map["name"];
                    rspItem["success"] = rspItemState;
                    rsp.list.append(rspItem);
                }
                else
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/schedules/%1").arg(id), QString("invalid value, %1, for parameter name").arg(map["name"].toString())));
                }
            }

            if (map.contains("description") && (map["description"].type() == QVariant::String))
            {
                QString description = map["description"].toString();

                if (description.size() > 0 && description.size() <= 32)
                {
                    i->description = description;
                    i->jsonMap["description"] = map["description"];

                    QVariantMap rspItem;
                    QVariantMap rspItemState;
                    rspItemState[QString("/schedules/%1/description").arg(id)] = map["description"];
                    rspItem["success"] = rspItemState;
                    rsp.list.append(rspItem);
                }
                else
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/schedules/%1").arg(id), QString("invalid value, %1, for parameter description").arg(map["description"].toString())));
                }
            }

            if (map.contains("status") && (map["status"].type() == QVariant::String))
            {
                QString status = map["status"].toString();

                if ((status == "enabled") || (status == "disabled"))
                {
                    i->status = status;
                    i->jsonMap["status"] = map["status"];

                    QVariantMap rspItem;
                    QVariantMap rspItemState;
                    rspItemState[QString("/schedules/%1/status").arg(id)] = map["status"];
                    rspItem["success"] = rspItemState;
                    rsp.list.append(rspItem);
                }
                else
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/schedules/%1").arg(id), QString("invalid value, %1, for parameter status").arg(map["status"].toString())));
                }
            }

            if (map.contains("command") && (map["command"].type() == QVariant::Map))
            {
                QVariantMap cmd = map["command"].toMap();

                if (!cmd.isEmpty() && cmd.contains("address") && cmd.contains("method") && cmd.contains("body"))
                {
                    i->command = deCONZ::jsonStringFromMap(cmd);
                    i->jsonMap["command"] = map["command"];

                    QVariantMap rspItem;
                    QVariantMap rspItemState;
                    rspItemState[QString("/schedules/%1/command").arg(id)] = map["command"];
                    rspItem["success"] = rspItemState;
                    rsp.list.append(rspItem);
                }
                else
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/schedules/%1").arg(id), QString("invalid value, %1, for parameter command").arg(map["command"].toString())));
                }
            }

            if (map.contains("autodelete"))
            {
                if (map["autodelete"].type() == QVariant::Bool)
                {
                    bool autodelete = map["autodelete"].toBool();
                    i->autodelete = autodelete;
                    i->jsonMap["autodelete"] = map["autodelete"];

                    QVariantMap rspItem;
                    QVariantMap rspItemState;
                    rspItemState[QString("/schedules/%1/autodelete").arg(id)] = map["autodelete"];
                    rspItem["success"] = rspItemState;
                    rsp.list.append(rspItem);
                }
                else
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/schedules/%1").arg(id), QString("invalid value, %1, for parameter autodelete").arg(map["autodelete"].toString())));
                }
            }

            // time
            if (map.contains("time") && (map["time"].type() == QVariant::String))
            {
                QString time = map["time"].toString();

                { // cutoff random part, A[hh]:[mm]:[ss], because this is not supported yet
                    QStringList ls = time.split("A");

                    if (ls.size() == 2)
                    {
                        DBG_Printf(DBG_INFO, "cut off random part %s\n", qPrintable(ls[1]));
                        time = ls.first();
                    }
                }

                // Timer with random element
                // PT[hh]:[mm]:[ss]A[hh]:[mm]:[ss]
                if (time.startsWith("PT") && time.contains("A"))
                {
        //            schedule.type = Schedule::TypeTimer;
                }
                // Recurring timer with random element
                // R[nn]/PT[hh]:[mm]:[ss]A[hh]:[mm]:[ss]
                else if (time.startsWith("R") && time.contains("PT") && time.contains("A"))
                {
        //            schedule.type = Schedule::TypeTimer;
                }
                // Recurring timer
                // R[nn]/PT[hh]:[mm]:[ss]
                else if (time.startsWith("R") && time.contains("PT"))
                {
                    QRegExp rx("R([0-9]{0,2})/PT(\\d\\d):(\\d\\d):(\\d\\d)");

                    if (rx.exactMatch(time))
                    {
                        i->timeout = rx.cap(2).toInt() * 60 * 60 + // h
                                           rx.cap(3).toInt() * 60 +   // m
                                           rx.cap(4).toInt(); // s
                        i->currentTimeout = i->timeout;
                        QDateTime now = QDateTime::currentDateTimeUtc();
                        i->starttime = now.toString("yyyy-MM-ddThh:mm:ss");

                        QString R = rx.cap(1);
                        if (!R.isEmpty())
                        {
                            i->recurring = R.toUInt();
                        }
                        else
                        {
                            i->recurring = 0; // runs forever
                        }

                        if (i->timeout > 0)
                        {
                            i->type = Schedule::TypeTimer;
                        }
                    }
                    else
                    {
                        rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/schedules"), QString("invalid value, %1, for parameter time").arg(map["time"].toString())));
                        rsp.httpStatus = HttpStatusBadRequest;
                        return REQ_READY_SEND;
                    }
                }
                // Timer expiring at given time
                // PT[hh]:[mm]:[ss]
                else if (time.startsWith("PT"))
                {
                    QRegExp rx("PT(\\d\\d):(\\d\\d):(\\d\\d)");

                    if (rx.exactMatch(time))
                    {
                        i->timeout = rx.cap(1).toInt() * 60 * 60 + // h
                                           rx.cap(2).toInt() * 60 +   // m
                                           rx.cap(3).toInt(); // s
                        i->currentTimeout = i->timeout;
                        i->recurring = 1;
                        QDateTime now = QDateTime::currentDateTimeUtc();
                        i->starttime = now.toString("yyyy-MM-ddThh:mm:ss");

                        if (i->timeout > 0)
                        {
                            i->type = Schedule::TypeTimer;
                        }
                    }
                    else
                    {
                        rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/schedules"), QString("invalid value, %1, for parameter time").arg(map["time"].toString())));
                        rsp.httpStatus = HttpStatusBadRequest;
                        return REQ_READY_SEND;
                    }
                }
                // Every weekday given by bbb at given left side time, randomized by right side time.
                // Right side time has to be smaller than 12 hours
                // W[bbb]/T[hh]:[mm]:[ss]A[hh]:[mm]:[ss]
                else if (time.startsWith("W") && time.contains("T") && time.contains("A"))
                {
        //            schedule.type = Schedule::TypeRecurringTime;
                }
                // Every day of the week  given by bbb at given time
                // W[bbb]/T[hh]:[mm]:[ss]
                else if (time.startsWith("W") && time.contains("T"))
                {
                    QRegExp rx("W([0-9]{1,3})/T(\\d\\d):(\\d\\d):(\\d\\d)");

                    if (rx.exactMatch(time))
                    {
                        i->type = Schedule::TypeRecurringTime;
                        i->weekBitmap = rx.cap(1).toUInt();
                        //dummy date needed when recurring alarm timout fired
                        i->datetime = QDateTime();
                        i->datetime.setTime(QTime(rx.cap(2).toUInt(),   // h
                                                        rx.cap(3).toUInt(),   // m
                                                        rx.cap(4).toUInt())); // s
                    }
                    else
                    {
                        rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/schedules"), QString("invalid value, %1, for parameter time").arg(map["time"].toString())));
                        rsp.httpStatus = HttpStatusBadRequest;
                        return REQ_READY_SEND;
                    }
                }
                // Absolute time
                else
                {
                    QDateTime checkTime = QDateTime::fromString(time, Qt::ISODate);
                    checkTime.setTimeSpec(Qt::UTC);

                    if (checkTime.isValid())
                    {
                        i->datetime = checkTime;
                        i->type = Schedule::TypeAbsoluteTime;
                    }
                    else
                    {
                        rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/schedules"), QString("invalid value, %1, for parameter time").arg(map["time"].toString())));
                        rsp.httpStatus = HttpStatusBadRequest;
                        return REQ_READY_SEND;
                    }
                }
                    i->time = time;
                    i->jsonMap["time"] = map["time"];

                    QVariantMap rspItem;
                    QVariantMap rspItemState;
                    rspItemState[QString("/schedules/%1/time").arg(id)] = map["time"];
                    rspItem["success"] = rspItemState;
                    rsp.list.append(rspItem);
                    rsp.httpStatus = HttpStatusOk;
            }

            updateEtag(i->etag);

            i->jsonMap["etag"] = i->etag.remove('"'); // no quotes allowed in string;
            i->jsonString = deCONZ::jsonStringFromMap(i->jsonMap);
            queSaveDb(DB_SCHEDULES, DB_SHORT_SAVE_DELAY);

            return REQ_READY_SEND;
        }
    }

    rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/schedules/%1").arg(id), QString("resource, /schedules/%1, not available").arg(id)));
    rsp.httpStatus = HttpStatusNotFound;
    return REQ_READY_SEND;
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
        if ((i->id == id) && (i->state == Schedule::StateNormal))
        {
            QVariantMap rspItem;
            rspItem["success"] = QString("/schedules/%1 deleted.").arg(id);
            rsp.list.append(rspItem);
            rsp.httpStatus = HttpStatusOk;

            DBG_Printf(DBG_INFO, "/schedules/%s deleted\n", qPrintable(id));
            i->state = Schedule::StateDeleted;
            queSaveDb(DB_SCHEDULES, DB_SHORT_SAVE_DELAY);
            return REQ_READY_SEND;
        }
    }

    rsp.httpStatus = HttpStatusNotFound;
    rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/schedules/%1").arg(id), QString("resource, /schedules/%1, not available").arg(id)));

    return REQ_NOT_HANDLED;
}

/*! Parses a JSON string into Schedule object.
    \return true on success
            false on failure
 */
bool DeRestPluginPrivate::jsonToSchedule(const QString &jsonString, Schedule &schedule, ApiResponse *rsp)
{
    bool ok;
    QVariant var = Json::parse(jsonString, ok);
    QVariantMap map = var.toMap();

    if (!ok || map.isEmpty())
    {
        if (rsp)
        {
            rsp->list.append(errorToMap(ERR_INVALID_JSON, QString("/schedules"), QString("body contains invalid JSON")));
            rsp->httpStatus = HttpStatusBadRequest;
        }
        return false;
    }

    // check required parameters
    if (!(map.contains("command") && map.contains("time")))
    {
        if (rsp)
        {
            rsp->list.append(errorToMap(ERR_MISSING_PARAMETER, QString("/schedules"), QString("missing parameters in body")));
            rsp->httpStatus = HttpStatusBadRequest;
        }
        return false;
    }

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
        schedule.description = map["description"].toString();
    } // else ignore use empty description

    // command
    DBG_Assert(map.contains("command"));
    if (map.contains("command") && (map["command"].type() == QVariant::Map))
    {
        QVariantMap cmd = map["command"].toMap();

        if (cmd.isEmpty() || !cmd.contains("address") || !cmd.contains("method") || !cmd.contains("body"))
        {
            if (rsp)
            {
                rsp->list.append(errorToMap(ERR_INVALID_VALUE, QString("/schedules"), QString("invalid value, %1, for parameter command").arg(map["command"].toString())));
                rsp->httpStatus = HttpStatusBadRequest;
            }
            return false;
        }

        schedule.command = deCONZ::jsonStringFromMap(cmd);
    }
    else
    {
        if (rsp)
        {
            rsp->list.append(errorToMap(ERR_INVALID_VALUE, QString("/schedules"), QString("invalid value, %1, for parameter command").arg(map["command"].toString())));
            rsp->httpStatus = HttpStatusBadRequest;
        }
        return false;
    }

    // status
    if (map.contains("status") && (map["status"].type() == QVariant::String) && ((map["status"].toString() == "enabled") || (map["status"].toString() == "disabled")))
    {
        schedule.status = map["status"].toString();
    }// else status enabled is used

    // autodelete
    if (map.contains("autodelete") && (map["autodelete"].type() == QVariant::Bool))
    {
        schedule.autodelete = map["autodelete"].toBool();
    }// else autodelete true is used

    // time
    DBG_Assert(map.contains("time"));
    if (map.contains("time") && (map["time"].type() == QVariant::String))
    {
        schedule.time = map["time"].toString();

        QString time = schedule.time;

        { // cutoff random part, A[hh]:[mm]:[ss], because this is not supported yet
            QStringList ls = schedule.time.split("A");

            if (ls.size() == 2)
            {
                DBG_Printf(DBG_INFO, "cut off random part %s\n", qPrintable(ls[1]));
                time = ls.first();
            }
        }

        // Timer with random element
        // PT[hh]:[mm]:[ss]A[hh]:[mm]:[ss]
        if (time.startsWith("PT") && time.contains("A"))
        {
//            schedule.type = Schedule::TypeTimer;
        }
        // Recurring timer with random element
        // R[nn]/PT[hh]:[mm]:[ss]A[hh]:[mm]:[ss]
        else if (time.startsWith("R") && time.contains("PT") && time.contains("A"))
        {
//            schedule.type = Schedule::TypeTimer;
        }
        // Recurring timer
        // R[nn]/PT[hh]:[mm]:[ss]
        else if (time.startsWith("R") && time.contains("PT"))
        {
            QRegExp rx("R([0-9]{0,2})/PT(\\d\\d):(\\d\\d):(\\d\\d)");

            if (rx.exactMatch(time))
            {
                schedule.timeout = rx.cap(2).toInt() * 60 * 60 + // h
                                   rx.cap(3).toInt() * 60 +   // m
                                   rx.cap(4).toInt(); // s
                schedule.currentTimeout = schedule.timeout;
                QDateTime now = QDateTime::currentDateTimeUtc();
                schedule.starttime = now.toString("yyyy-MM-ddThh:mm:ss");

                QString R = rx.cap(1);
                if (!R.isEmpty())
                {
                    schedule.recurring = R.toUInt();
                }
                else
                {
                    schedule.recurring = 0; // runs forever
                }

                if (schedule.timeout > 0)
                {
                    schedule.type = Schedule::TypeTimer;
                }
            }
        }
        // Timer expiring at given time
        // PT[hh]:[mm]:[ss]
        else if (time.startsWith("PT"))
        {
            QRegExp rx("PT(\\d\\d):(\\d\\d):(\\d\\d)");

            if (rx.exactMatch(time))
            {
                schedule.timeout = rx.cap(1).toInt() * 60 * 60 + // h
                                   rx.cap(2).toInt() * 60 +   // m
                                   rx.cap(3).toInt(); // s
                schedule.currentTimeout = schedule.timeout;
                schedule.recurring = 1;
                QDateTime now = QDateTime::currentDateTimeUtc();
                schedule.starttime = now.toString("yyyy-MM-ddThh:mm:ss");

                if (schedule.timeout > 0)
                {
                    schedule.type = Schedule::TypeTimer;
                }
            }
        }
        // Every weekday given by bbb at given left side time, randomized by right side time.
        // Right side time has to be smaller than 12 hours
        // W[bbb]/T[hh]:[mm]:[ss]A[hh]:[mm]:[ss]
        else if (time.startsWith("W") && time.contains("T") && time.contains("A"))
        {
//            schedule.type = Schedule::TypeRecurringTime;
        }
        // Every day of the week  given by bbb at given time
        // W[bbb]/T[hh]:[mm]:[ss]
        else if (time.startsWith("W") && time.contains("T"))
        {
            QRegExp rx("W([0-9]{1,3})/T(\\d\\d):(\\d\\d):(\\d\\d)");

            if (rx.exactMatch(time))
            {
                schedule.type = Schedule::TypeRecurringTime;
                schedule.weekBitmap = rx.cap(1).toUInt();
                schedule.datetime.setTime(QTime(rx.cap(2).toUInt(),   // h
                                                rx.cap(3).toUInt(),   // m
                                                rx.cap(4).toUInt())); // s
            }
        }
        // Absolute time
        else
        {
            schedule.datetime = QDateTime::fromString(time, Qt::ISODate);
            schedule.datetime.setTimeSpec(Qt::UTC);

            if (schedule.datetime.isValid())
            {
                schedule.type = Schedule::TypeAbsoluteTime;
            }
        }

        if (schedule.type == Schedule::TypeInvalid)
        {
            if (rsp)
            {
                rsp->list.append(errorToMap(ERR_INVALID_VALUE, QString("/schedules"), QString("invalid value, %1, for parameter time").arg(map["time"].toString())));
                rsp->httpStatus = HttpStatusBadRequest;
            }
            return false;
        }
    }
    else
    {
        if (rsp)
        {
            rsp->list.append(errorToMap(ERR_INVALID_VALUE, QString("/schedules"), QString("invalid value, %1, for parameter time").arg(map["time"].toString())));
            rsp->httpStatus = HttpStatusBadRequest;
        }
        return false;
    }

    updateEtag(schedule.etag);
    map["etag"] = schedule.etag.remove('"'); // no quotes allowed in string;;

    schedule.jsonString = jsonString;
    schedule.jsonMap = map;

    return true;
}

/*! Processes any schedules.
 */
void DeRestPluginPrivate::scheduleTimerFired()
{
    if (schedules.empty())
    {
        return;
    }

    std::vector<Schedule>::iterator i = schedules.begin();
    std::vector<Schedule>::iterator end = schedules.end();

    QDateTime now = QDateTime::currentDateTimeUtc();

    for (; i != end; ++i)
    {
        if (i->state == Schedule::StateNormal && i->status == "enabled")
        {
            qint64 diff = 0;

            if (i->type == Schedule::TypeAbsoluteTime)
            {
                diff = now.secsTo((i->datetime));
            }
            else if (i->type == Schedule::TypeTimer)
            {
                if (i->currentTimeout > 0)
                {
                    DBG_Printf(DBG_INFO, "schedule %s timeout in %ds\n", qPrintable(i->id), i->currentTimeout);
                    i->currentTimeout--;
                    continue;
                }
                else if (i->currentTimeout <= 0)
                {
                    i->currentTimeout = i->timeout;
                }

                if (i->recurring == 1)
                {
                    // last trigger
                    if (i->autodelete)
                    {
                        DBG_Printf(DBG_INFO, "schedule %s deleted\n",qPrintable(i->name));
                        i->state = Schedule::StateDeleted;
                    }
                    else
                    {
                        DBG_Printf(DBG_INFO, "schedule %s disabled\n",qPrintable(i->name));
                        i->status = "disabled";
                        i->jsonMap["status"] = "disabled";
                        i->jsonString = deCONZ::jsonStringFromMap(i->jsonMap);
                    }
                    queSaveDb(DB_SCHEDULES, DB_SHORT_SAVE_DELAY);
                }
                else if (i->recurring > 0)
                {
                    i->recurring--;
                }
            }
            else if (i->type == Schedule::TypeRecurringTime)
            {
                int day = now.date().dayOfWeek(); // Mon-Sun: 1-7

                QString dayBitmap;

                switch (day)
                {
                case 1:
                   dayBitmap = "01000000";
                   break;
                case 2:
                   dayBitmap = "00100000";
                   break;
                case 3:
                   dayBitmap = "00010000";
                   break;
                case 4:
                   dayBitmap = "00001000";
                   break;
                case 5:
                   dayBitmap = "00000100";
                   break;
                case 6:
                   dayBitmap = "00000010";
                   break;
                case 7:
                   dayBitmap = "00000001";
                   break;
                }

                // active for today?
                QString weekBitmap = QString::number(i->weekBitmap,2);
                while (weekBitmap.length() < 8)
                {
                    weekBitmap = '0' + weekBitmap;
                }

                //if (i->weekBitmap & (1 << (day - 1))) // does not work e.g.: day=5 (Fr) & wb=4 (Fr)
                if (weekBitmap[day] == dayBitmap[day])
                {
                    //DBG_Printf(DBG_INFO, "actual day\n");

                    if (i->lastTriggerDatetime.date() == now.date())
                    {
                        //recurring alarm should trigger again on same day if updated with future time
                        if (i->datetime.time() <= now.time())
                        {
                            // already fired today
                            continue;
                        }
                    }

                    diff = now.time().secsTo(i->datetime.time());

                    if (diff > 0)
                    {
                        DBG_Printf(DBG_INFO, "schedule %s diff %lld, %s\n", qPrintable(i->id), diff, qPrintable(i->datetime.toString()));
                        continue;
                    }
                }
                else
                {
                    continue;
                }
            }
            else
            {
                // not supported yet
                i->state = Schedule::StateDeleted;
                queSaveDb(DB_SCHEDULES, DB_SHORT_SAVE_DELAY);
                continue;
            }

            if (diff <= -5 && i->type != Schedule::TypeRecurringTime)
            {
                DBG_Printf(DBG_INFO, "schedule %s: %s deleted (too old)\n", qPrintable(i->id), qPrintable(i->name));
                i->state = Schedule::StateDeleted;
                queSaveDb(DB_SCHEDULES, DB_SHORT_SAVE_DELAY);
            }
            if (diff <= -5 && i->type == Schedule::TypeRecurringTime) //do nothing and trigger allarm next week
            {
                continue;
            }
            else if (diff <= 0)
            {             
                i->lastTriggerDatetime = now;
                DBG_Printf(DBG_INFO, "schedule %s: %s trigger\n", qPrintable(i->id), qPrintable(i->name));

                if (i->type == Schedule::TypeAbsoluteTime)
                {
                    if (i->autodelete)
                    {
                        i->state = Schedule::StateDeleted;
                        DBG_Printf(DBG_INFO, "schedule %s removed\n", qPrintable(i->id));
                    }
                    else
                    {
                        i->status = "disabled";
                        i->jsonMap["status"] = "disabled";
                        i->jsonString = deCONZ::jsonStringFromMap(i->jsonMap);
                    }
                    queSaveDb(DB_SCHEDULES, DB_SHORT_SAVE_DELAY);
                }

                QVariantMap cmd = i->jsonMap["command"].toMap();

                // check if fields are given
                if (cmd.isEmpty() || !cmd.contains("address") || !cmd.contains("method") || !cmd.contains("body"))
                {
                    DBG_Printf(DBG_INFO, "schedule %s ignored, invalid command %s\n",  qPrintable(i->id), qPrintable(i->command));
                    return;
                }
                QString method = cmd["method"].toString();
                QString address = cmd["address"].toString();
                QString content = deCONZ::jsonStringFromMap(cmd["body"].toMap());

                // check if fields contain data
                if (method.isEmpty() || address.isEmpty() || content.isEmpty())
                {
                    i->state = Schedule::StateDeleted;
                    queSaveDb(DB_SCHEDULES, DB_SHORT_SAVE_DELAY);
                    DBG_Printf(DBG_INFO, "schedule %s ignored and removed, invalid command %s\n", qPrintable(i->id), qPrintable(i->command));
                    return;
                }

                QHttpRequestHeader hdr(method, address);
                QStringList path = hdr.path().split('/', QString::SkipEmptyParts);

                ApiRequest req(hdr, path, NULL, content);
                ApiResponse rsp; // dummy

                DBG_Printf(DBG_INFO, "schedule %s body: %s\n",  qPrintable(i->id), qPrintable(content));

                // fading not visible when turning lights on and light level was already bright
                if (content.indexOf("on\":true") != -1 && content.indexOf("transitiontime\":0") == -1)
                {
                    QString id = path[3];
                    bool stateOn = true;
                    if (path[2] == "groups")
                    {
                        Group *group = getGroupForId(id);
                        stateOn = group->isOn();
                    }
                    else if (path[2] == "lights")
                    {
                        LightNode *light = getLightNodeForId(id);
                        stateOn = light->isOn();
                    }
                    if (!stateOn)
                    {
                        // activate lights with low brightness then activate schedule with fading
                        // only if lights were off
                        QVariantMap body;
                        body["on"] = true;
                        body["bri"] = (double)2;
                        body["transitiontime"] = (double)0;
                        QString content2 = deCONZ::jsonStringFromMap(body);

                        ApiRequest req2(hdr, path, NULL, content2);
                        ApiResponse rsp2; // dummy

                        if (handleLightsApi(req2, rsp2) == REQ_NOT_HANDLED)
                        {
                            handleGroupsApi(req2, rsp2);
                        }
                    }
                }
                if (handleLightsApi(req, rsp) == REQ_NOT_HANDLED)
                {
                    if (handleGroupsApi(req, rsp) == REQ_NOT_HANDLED)
                    {
                        DBG_Printf(DBG_INFO, "schedule was neigher light nor group request.\n");
                    }
                    else
                    {
                        // Request handled. Activate or deactivate sensor rules if present
                        int begin = address.indexOf("groups/")+7;
                        int end = address.indexOf("/action");
                        QString groupId = address.mid(begin, end-begin);

                        if (content.indexOf("on\":true") != -1)
                        {
                            changeRuleStatusofGroup(groupId,true);
                        }
                        else if (content.indexOf("on\":false") != -1)
                        {
                            changeRuleStatusofGroup(groupId,false);
                        }

                    }
                }

                return;
            }
            else
            {
                DBG_Printf(DBG_INFO, "schedule %s diff %lld, %s\n", qPrintable(i->id), diff, qPrintable(i->datetime.toString()));
            }
        }
    }
}
