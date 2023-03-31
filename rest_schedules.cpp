/*
 * Copyright (c) 2016-2019 dresden elektronik ingenieurtechnik gmbh.
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
int DeRestPluginPrivate::handleSchedulesApi(const ApiRequest &req, ApiResponse &rsp)
{
    if (req.path[2] != QLatin1String("schedules"))
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
            if (!i->localtime.isEmpty())
            {
                mnode["localtime"] = i->localtime;
            }
            if (i->type == Schedule::TypeTimer)
            {
                mnode["starttime"] = i->starttime;
            }
            if (i->jsonMap.contains("created"))
            {
                mnode["created"] = i->jsonMap["created"];
            }
            mnode["status"] = i->status;
            mnode["activation"] = i->activation;
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
            if (!i->localtime.isEmpty())
            {
                rsp.map["localtime"] = i->localtime;
            }
            if (i->type == Schedule::TypeTimer)
            {
                rsp.map["starttime"] = i->starttime;
            }
            if (i->jsonMap.contains("created"))
            {
                rsp.map["created"] = i->jsonMap["created"];
            }
            rsp.map["status"] = i->status;
            rsp.map["activation"] = i->activation;
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

        if (i->state != Schedule::StateNormal || i->id != id)
        {
            continue;
        }

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
                rsp.httpStatus = HttpStatusOk;
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
                rsp.httpStatus = HttpStatusOk;
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

                if (i->status == QLatin1String("disabled"))
                {
                    i->endtime = QDateTime();
                }

                // randomize time again
                if (status == "enabled" && i->time.contains("A"))
                {
                    map["time"] = i->time;
                }
                if (status == "enabled" && i->localtime.contains("A"))
                {
                    map["localtime"] = i->localtime;
                }

                QVariantMap rspItem;
                QVariantMap rspItemState;
                rspItemState[QString("/schedules/%1/status").arg(id)] = map["status"];
                rspItem["success"] = rspItemState;
                rsp.list.append(rspItem);
                rsp.httpStatus = HttpStatusOk;
            }
            else
            {
                rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/schedules/%1").arg(id), QString("invalid value, %1, for parameter status").arg(map["status"].toString())));
            }
        }

        if (map.contains("activation") && (map["activation"].type() == QVariant::String))
        {
            QString activation = map["activation"].toString();

            if ((activation == "start") || (activation == "end"))
            {
                i->activation = activation;
                i->jsonMap["activation"] = map["activation"];

                QVariantMap rspItem;
                QVariantMap rspItemState;
                rspItemState[QString("/schedules/%1/activation").arg(id)] = map["activation"];
                rspItem["success"] = rspItemState;
                rsp.list.append(rspItem);
                rsp.httpStatus = HttpStatusOk;
            }
            else
            {
                rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/schedules/%1").arg(id), QString("invalid value, %1, for parameter activation").arg(map["activation"].toString())));
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
                rsp.httpStatus = HttpStatusOk;
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
                rsp.httpStatus = HttpStatusOk;
            }
            else
            {
                rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/schedules/%1").arg(id), QString("invalid value, %1, for parameter autodelete").arg(map["autodelete"].toString())));
            }
        }

        // time
        QString time;
        Qt::TimeSpec timeSpec = Qt::UTC;
        int randomTime = 0;
        int randomMax = 0;

        // time (deprecated)
        if (map.contains("time") && (map["time"].type() == QVariant::String))
        {
            time = map["time"].toString();
            timeSpec = Qt::UTC;
        }

        // localtime (overwrites UTC)
        if (map.contains("localtime") && (map["localtime"].type() == QVariant::String))
        {
            time = map["localtime"].toString();
            timeSpec = Qt::LocalTime;
        }

        if (!time.isEmpty())
        {
            i->lastTriggerDatetime = QDateTime(); // reset

            if (time.contains("A"))
            {
                // cutoff random part, A[hh]:[mm]:[ss] (it will be added later)
                QStringList ls = time.split("A");

                if (ls.size() == 2)
                {
                    DBG_Printf(DBG_INFO, "random part: %s\n", qPrintable(ls[1]));
                    time = ls.first();
                }

                QRegExp rnd("(\\d\\d):(\\d\\d):(\\d\\d)");
                if (rnd.exactMatch(ls[1]))
                {
                    randomMax = rnd.cap(1).toInt() * 60 * 60 + // h
                        rnd.cap(2).toInt() * 60 +   // m
                        rnd.cap(3).toInt(); // s

                    if (randomMax == 0) {
                        randomTime = 0;
                    } else {
                        randomTime = (qrand() % ((randomMax + 1) - 1) + 1);
                    }
                }
                else
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/schedules"), QString("invalid value, %1, for random part of parameter time").arg(map["time"].toString())));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }
            }
            // Recurring timer
            // R[nn]/PT[hh]:[mm]:[ss]
            if (time.startsWith("R") && time.contains("PT"))
            {
                QRegExp rx("R([0-9]{0,2})/PT(\\d\\d):(\\d\\d):(\\d\\d)");

                if (rx.exactMatch(time))
                {
                    // offset always from current localtime?
                    i->timeout = rx.cap(2).toInt() * 60 * 60 + // h
                            rx.cap(3).toInt() * 60 +   // m
                            rx.cap(4).toInt() + // s
                            randomTime; // randomTime in seconds
                    i->currentTimeout = i->timeout;
                    QDateTime now = QDateTime::currentDateTimeUtc();
                    i->starttime = now.toString("yyyy-MM-ddThh:mm:ss");
                    if (i->status == QLatin1String("enabled"))
                    {
                        if (timeSpec == Qt::UTC)
                        {
                            i->endtime = QDateTime::currentDateTimeUtc().addSecs(i->timeout);
                            int toffset = QDateTime::currentDateTime().offsetFromUtc();
                            i->endtime = i->endtime.addSecs(toffset);
                            i->endtime.setOffsetFromUtc(toffset);
                            i->endtime.setTimeSpec(Qt::LocalTime);
                        }
                        else if (timeSpec == Qt::LocalTime)
                        {
                            i->endtime = QDateTime::currentDateTime().addSecs(i->timeout);
                        }
                    }

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
            // Timer expiring after given time
            // PT[hh]:[mm]:[ss]
            else if (time.startsWith("PT"))
            {
                QRegExp rx("PT(\\d\\d):(\\d\\d):(\\d\\d)");

                if (rx.exactMatch(time))
                {
                    // offset always from current localtime?
                    i->timeout = rx.cap(1).toInt() * 60 * 60 + // h
                                 rx.cap(2).toInt() * 60 +   // m
                                 rx.cap(4).toInt() + // s
                                 randomTime; // randomTime in seconds
                    i->currentTimeout = i->timeout;
                    i->recurring = 1;
                    QDateTime now = QDateTime::currentDateTimeUtc();
                    i->starttime = now.toString("yyyy-MM-ddThh:mm:ss");
                    if (i->status == QLatin1String("enabled"))
                    {
                        if (timeSpec == Qt::UTC)
                        {
                            i->endtime = QDateTime::currentDateTimeUtc().addSecs(i->timeout);
                            int toffset = QDateTime::currentDateTime().offsetFromUtc();
                            i->endtime = i->endtime.addSecs(toffset);
                            i->endtime.setOffsetFromUtc(toffset);
                            i->endtime.setTimeSpec(Qt::LocalTime);
                        }
                        else if (timeSpec == Qt::LocalTime)
                        {
                            i->endtime = QDateTime::currentDateTime().addSecs(i->timeout);
                        }
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

                if (randomTime > 43200) {
                    randomTime = 43200; // random time has to be smaller than 12 hours
                }

                if (rx.exactMatch(time))
                {
                    i->type = Schedule::TypeRecurringTime;
                    i->weekBitmap = rx.cap(1).toUInt();
                    //dummy date needed when recurring alarm timout fired
                    if (timeSpec == Qt::UTC)
                    {
                        i->datetime = QDateTime::currentDateTimeUtc();
                    }
                    else
                    {
                        i->datetime = QDateTime::currentDateTime();
                    }
                    i->datetime.setTime(QTime(rx.cap(2).toUInt(),   // h
                                              rx.cap(3).toUInt(),   // m
                                              rx.cap(4).toUInt())); // s
                    i->datetime = i->datetime.addSecs(randomTime);

                    // conversion to localtime
                    if (timeSpec == Qt::UTC)
                    {
                        int toffset = QDateTime::currentDateTime().offsetFromUtc();
                        i->datetime = i->datetime.addSecs(toffset);
                        i->datetime.setOffsetFromUtc(toffset);
                        i->datetime.setTimeSpec(Qt::LocalTime);
                    }
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
                checkTime.setTimeSpec(timeSpec);
                checkTime = checkTime.addSecs(randomTime);

                // conversion to localtime
                if (checkTime.isValid() && timeSpec == Qt::UTC)
                {
                    int toffset = QDateTime::currentDateTime().offsetFromUtc();
                    checkTime = checkTime.addSecs(toffset);
                    checkTime.setOffsetFromUtc(toffset);
                    checkTime.setTimeSpec(Qt::LocalTime);
                }

                if (checkTime.isValid())
                {
                    i->datetime = checkTime;
                    i->endtime = checkTime;
                    if (!map.contains(QLatin1String("time")))
                    {
                        i->time = checkTime.toUTC().toString(QLatin1String("yyyy-MM-ddTHH:mm:ss"));
                    }
                    if (!map.contains(QLatin1String("localtime")))
                    {
                        i->localtime = checkTime.toString(QLatin1String("yyyy-MM-ddTHH:mm:ss"));
                    }
                    i->type = Schedule::TypeAbsoluteTime;
                }
                else
                {
                    if (map.contains(QLatin1String("localtime")))
                    {
                        rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/schedules"), QString("invalid value, %1, for parameter localtime").arg(map["localtime"].toString())));
                    }
                    else
                    {
                        rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/schedules"), QString("invalid value, %1, for parameter time").arg(map["time"].toString())));
                    }
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }
            }

            QVariantMap rspItem;
            if (map.contains(QLatin1String("localtime")))
            {
                i->localtime = map["localtime"].toString();
                i->jsonMap["localtime"] = i->localtime;

                QVariantMap rspItemState;
                rspItemState[QString("/schedules/%1/localtime").arg(id)] = map["localtime"];
                rspItem["success"] = rspItemState;
                rsp.list.append(rspItem);
            }

            if (map.contains(QLatin1String("time")))
            {
                i->time = map["time"].toString();
                i->jsonMap["time"] = i->time;

                QVariantMap rspItemState;
                rspItemState[QString("/schedules/%1/time").arg(id)] = map["time"];
                rspItem["success"] = rspItemState;
                rsp.list.append(rspItem);
            }

            rsp.httpStatus = HttpStatusOk;
        }

        updateEtag(i->etag);

        i->jsonMap["etag"] = i->etag.remove('"'); // no quotes allowed in string;
        i->jsonString = Json::serialize(i->jsonMap);
        queSaveDb(DB_SCHEDULES, DB_SHORT_SAVE_DELAY);

        return REQ_READY_SEND;
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
    if (!(map.contains("command") && (map.contains("time") || map.contains("localtime"))))
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
        if (schedule.status == QLatin1String("disabled"))
        {
            schedule.endtime = QDateTime();
        }
    }// else status enabled is used

    // activation
    if (map.contains("activation") && (map["activation"].type() == QVariant::String) && ((map["activation"].toString() == "start") || (map["activation"].toString() == "end")))
    {
        schedule.activation = map["activation"].toString();
    }// else activation start is used

    // autodelete
    if (map.contains("autodelete") && (map["autodelete"].type() == QVariant::Bool))
    {
        schedule.autodelete = map["autodelete"].toBool();
    }// else autodelete true is used

    // time
    QString time;
    Qt::TimeSpec timeSpec = Qt::UTC;
    int randomTime = 0;
    int randomMax = 0;

    // time (deprecated)
    if (map.contains("time") && (map["time"].type() == QVariant::String))
    {
        schedule.time = map["time"].toString();
        time = schedule.time;
        timeSpec = Qt::UTC;
    }

    // localtime (overwrites UTC)
    if (map.contains("localtime") && (map["localtime"].type() == QVariant::String))
    {
        schedule.localtime = map["localtime"].toString();
        time = schedule.localtime;
        timeSpec = Qt::LocalTime;
    }

    if (time.isEmpty())
    {
        DBG_Assert(map.contains("time") || map.contains("localtime"));
        if (rsp)
        {
            rsp->list.append(errorToMap(ERR_MISSING_PARAMETER, QString("/schedules"), QString("missing parameter time or localtime")));
            rsp->httpStatus = HttpStatusBadRequest;
        }
        return false;
    }
    else
    {
        //schedule.time = map["time"].toString();

        // Timer with random element
        // PT[hh]:[mm]:[ss]A[hh]:[mm]:[ss]
        // if (time.startsWith("PT") && time.contains("A"))
        //{
//            schedule.type = Schedule::TypeTimer;
        //}
        // Recurring timer with random element
        // R[nn]/PT[hh]:[mm]:[ss]A[hh]:[mm]:[ss]
        //else if (time.startsWith("R") && time.contains("PT") && time.contains("A"))
        //{
//            schedule.type = Schedule::TypeTimer;
        //}
        // Recurring timer
        // R[nn]/PT[hh]:[mm]:[ss]
        if (time.contains("A"))
        {
            // cutoff random part, A[hh]:[mm]:[ss] (it will be added later)
            QStringList ls = time.split("A");

            if (ls.size() == 2)
            {
                DBG_Printf(DBG_INFO, "random part: %s\n", qPrintable(ls[1]));
                time = ls.first();
            }

            QRegExp rnd("(\\d\\d):(\\d\\d):(\\d\\d)");
            if (rnd.exactMatch(ls[1]))
            {
                randomMax = rnd.cap(1).toInt() * 60 * 60 + // h
                    rnd.cap(2).toInt() * 60 +   // m
                    rnd.cap(3).toInt(); // s

                if (randomMax == 0) {
                    randomTime = 0;
                } else {
                    randomTime = (qrand() % ((randomMax + 1) - 1) + 1);
                }
            }
            else
            {
                if (rsp)
                {
                    rsp->list.append(errorToMap(ERR_INVALID_VALUE, QString("/schedules"), QString("invalid value, %1, for random part of parameter time").arg(map["time"].toString())));
                    rsp->httpStatus = HttpStatusBadRequest;
                }
                return false;
            }
        }
        if (time.startsWith("R") && time.contains("PT"))
        {
            QRegExp rx("R([0-9]{0,2})/PT(\\d\\d):(\\d\\d):(\\d\\d)");

            if (rx.exactMatch(time))
            {
                // offset always from current localtime?
                schedule.timeout = rx.cap(2).toInt() * 60 * 60 + // h
                                   rx.cap(3).toInt() * 60 +   // m
                                   rx.cap(4).toInt() + // s
                                   randomTime; // randomTime in seconds
                schedule.currentTimeout = schedule.timeout;
                QDateTime now = QDateTime::currentDateTimeUtc();
                schedule.starttime = now.toString(QLatin1String("yyyy-MM-ddThh:mm:ss"));
                if (schedule.status == QLatin1String("enabled"))
                {
                    if (timeSpec == Qt::UTC)
                    {
                        schedule.endtime = QDateTime::currentDateTimeUtc().addSecs(schedule.timeout);
                        int toffset = QDateTime::currentDateTime().offsetFromUtc();
                        schedule.endtime = schedule.endtime.addSecs(toffset);
                        schedule.endtime.setOffsetFromUtc(toffset);
                        schedule.endtime.setTimeSpec(Qt::LocalTime);
                    }
                    else if (timeSpec == Qt::LocalTime)
                    {
                        schedule.endtime = QDateTime::currentDateTime().addSecs(schedule.timeout);
                    }
                }

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
                // offset always from current localtime?
                schedule.timeout = rx.cap(1).toInt() * 60 * 60 + // h
                                   rx.cap(2).toInt() * 60 +   // m
                                   rx.cap(3).toInt() + // s
                                   randomTime; // randomTime in seconds
                schedule.currentTimeout = schedule.timeout;
                schedule.recurring = 1;
                QDateTime now = QDateTime::currentDateTimeUtc();
                schedule.starttime = now.toString("yyyy-MM-ddThh:mm:ss");
                if (schedule.status == QLatin1String("enabled"))
                {
                    if (timeSpec == Qt::UTC)
                    {
                        schedule.endtime = QDateTime::currentDateTimeUtc().addSecs(schedule.timeout);
                        int toffset = QDateTime::currentDateTime().offsetFromUtc();
                        schedule.endtime = schedule.endtime.addSecs(toffset);
                        schedule.endtime.setOffsetFromUtc(toffset);
                        schedule.endtime.setTimeSpec(Qt::LocalTime);
                    }
                    else if (timeSpec == Qt::LocalTime)
                    {
                        schedule.endtime = QDateTime::currentDateTime().addSecs(schedule.timeout);
                    }
                }

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

            if (randomTime > 43200) {
                randomTime = 43200; // random time has to be smaller than 12 hours
            }

            if (rx.exactMatch(time))
            {
                schedule.type = Schedule::TypeRecurringTime;
                schedule.weekBitmap = rx.cap(1).toUInt();

                if (timeSpec == Qt::UTC)
                {
                    schedule.datetime = QDateTime::currentDateTimeUtc();
                }
                else
                {
                    schedule.datetime = QDateTime::currentDateTime();
                }
                schedule.datetime.setTime(QTime(rx.cap(2).toUInt(),   // h
                                                rx.cap(3).toUInt(),   // m
                                                rx.cap(4).toUInt())); // s
                schedule.datetime = schedule.datetime.addSecs(randomTime);

                // conversion to localtime
                if (timeSpec == Qt::UTC)
                {
                    int toffset = QDateTime::currentDateTime().offsetFromUtc();
                    schedule.datetime = schedule.datetime.addSecs(toffset);
                    schedule.datetime.setOffsetFromUtc(toffset);
                    schedule.datetime.setTimeSpec(Qt::LocalTime);
                }
            }
        }
        // Absolute time
        else
        {
            schedule.datetime = QDateTime::fromString(time, Qt::ISODate);
            schedule.datetime.setTimeSpec(timeSpec);
            schedule.datetime = schedule.datetime.addSecs(randomTime);

            // conversion to localtime
            if (timeSpec == Qt::UTC)
            {
                int toffset = QDateTime::currentDateTime().offsetFromUtc();
                schedule.datetime = schedule.datetime.addSecs(toffset);
                schedule.datetime.setOffsetFromUtc(toffset);
                schedule.datetime.setTimeSpec(Qt::LocalTime);
            }

            if (schedule.time.isEmpty())
            {
                schedule.time = schedule.datetime.toUTC().toString(QLatin1String("yyyy-MM-ddTHH:mm:ss"));
            }

            if (schedule.localtime.isEmpty())
            {
                schedule.localtime = schedule.datetime.toString(QLatin1String("yyyy-MM-ddTHH:mm:ss"));
            }

            schedule.endtime = QDateTime();
            if (schedule.datetime.isValid())
            {
                schedule.endtime = schedule.datetime;
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

    updateEtag(schedule.etag);
    map["etag"] = schedule.etag.remove('"'); // no quotes allowed in string

    if (rsp)
    {
        map["created"] = QDateTime::currentDateTimeUtc().toString(QLatin1String("yyyy-MM-ddTHH:mm:ss"));
    }
    schedule.jsonString = Json::serialize(map);
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

    QDateTime now = QDateTime::currentDateTime();

    for (; i != end; ++i)
    {
        if (i->state != Schedule::StateNormal ||
            i->status != QLatin1String("enabled"))
        {
            continue;
        }

        qint64 diff = 0;

        if (i->type == Schedule::TypeAbsoluteTime)
        {
            if (i->endtime.isValid())
            {
                diff = now.secsTo(i->datetime);
            }
        }
        else if (i->type == Schedule::TypeTimer)
        {
            if (i->endtime.isValid() && i->endtime > now)
            {
                DBG_Printf(DBG_INFO, "schedule %s timeout in %d s\n", qPrintable(i->id), now.secsTo(i->endtime));
                continue;
            }
            else if (i->endtime.isValid())
            {
                diff = now.secsTo(i->endtime);
                if (i->recurring != 1)
                {
                    i->endtime = now.addSecs(i->timeout);
                }
            }

            if (i->recurring == 1)
            {
                // last trigger
                if (i->autodelete)
                {
                    DBG_Printf(DBG_INFO, "schedule %s deleted\n", qPrintable(i->name));
                    i->state = Schedule::StateDeleted;
                }
                else
                {
                    DBG_Printf(DBG_INFO, "schedule %s disabled\n", qPrintable(i->name));
                    i->status = QLatin1String("disabled");
                    i->jsonMap["status"] = i->status;
                    i->jsonString = Json::serialize(i->jsonMap);
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
            // bbb = 0MTWTFSS – So only Tuesdays is 00100000 = 32
            quint8 day = now.date().dayOfWeek(); // Mon-Sun: 1-7
            quint8 bit = (1 << (7 - day));

            if (i->weekBitmap & bit)
            {
                //DBG_Printf(DBG_INFO, "actual day\n");

                if (i->lastTriggerDatetime.date() .isValid() &&
                    i->lastTriggerDatetime.date() == now.date())
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
                    DBG_Printf(DBG_INFO_L2, "schedule %s diff %lld, %s\n", qPrintable(i->id), diff, qPrintable(i->datetime.toString()));
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
            i->status = QLatin1String("disabled");
            i->jsonMap["status"] = i->status;
            i->jsonString = deCONZ::jsonStringFromMap(i->jsonMap);
            if (i->autodelete)
            {
                DBG_Printf(DBG_INFO, "schedule %s: %s deleted (too old)\n", qPrintable(i->id), qPrintable(i->name));
                i->state = Schedule::StateDeleted;
            }
            else
            {
                DBG_Printf(DBG_INFO, "schedule %s: %s disabled (too old)\n", qPrintable(i->id), qPrintable(i->name));
            }
            queSaveDb(DB_SCHEDULES, DB_SHORT_SAVE_DELAY);
            continue;
        }

        if (diff <= -5 && i->type == Schedule::TypeRecurringTime) //do nothing and trigger alarm next week
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
                    i->status = QLatin1String("disabled");
                    i->jsonMap["status"] = i->status;
                    i->jsonString = deCONZ::jsonStringFromMap(i->jsonMap);
                }
                queSaveDb(DB_SCHEDULES, DB_SHORT_SAVE_DELAY);
            }

            // randomize time again
            if (i->type == Schedule::TypeRecurringTime || i->type == Schedule::TypeTimer)
            {
                if (i->time.contains(("A")) || i->localtime.contains("A"))
                {
                    QString time = "";
                    Qt::TimeSpec timeSpec = Qt::UTC;
                    int randomMax = 0;
                    int randomTime = 0;
                    if (i->time.contains("A"))
                    {
                        time = i->time;
                        timeSpec = Qt::UTC;
                    }
                    if (i->localtime.contains("A"))
                    {
                        time = i->localtime;
                        timeSpec = Qt::LocalTime;
                    }

                    // cutoff random part, A[hh]:[mm]:[ss] (it will be added later)
                    QStringList ls = time.split("A");

                    if (ls.size() == 2)
                    {
                        DBG_Printf(DBG_INFO, "random part: %s\n", qPrintable(ls[1]));
                        time = ls.first();
                    }

                    QRegExp rnd("(\\d\\d):(\\d\\d):(\\d\\d)");
                    if (rnd.exactMatch(ls[1]))
                    {
                        randomMax = rnd.cap(1).toInt() * 60 * 60 + // h
                            rnd.cap(2).toInt() * 60 +   // m
                            rnd.cap(3).toInt(); // s

                        if (randomMax == 0) {
                            randomTime = 0;
                        } else {
                            randomTime = (qrand() % ((randomMax + 1) - 1) + 1);
                        }
                    }

                    if (randomTime > 43200) {
                        randomTime = 43200; // random time has to be smaller than 12 hours
                    }

                    if (timeSpec == Qt::UTC)
                    {
                        i->datetime = QDateTime::currentDateTimeUtc();
                    }
                    else
                    {
                        i->datetime = QDateTime::currentDateTime();
                    }

                    QRegExp rx("W([0-9]{1,3})/T(\\d\\d):(\\d\\d):(\\d\\d)");

                    if (rx.exactMatch(time))
                    {
                        i->datetime.setTime(QTime(rx.cap(2).toUInt(),   // h
                                                  rx.cap(3).toUInt(),   // m
                                                  rx.cap(4).toUInt())); // s
                        i->datetime = i->datetime.addSecs(randomTime);

                        // conversion to localtime
                        if (timeSpec == Qt::UTC)
                        {
                            int toffset = QDateTime::currentDateTime().offsetFromUtc();
                            i->datetime = i->datetime.addSecs(toffset);
                            i->datetime.setOffsetFromUtc(toffset);
                            i->datetime.setTimeSpec(Qt::LocalTime);
                        }
                    }
                }
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
            QStringList path = QString(hdr.path()).split('/', SKIP_EMPTY_PARTS);

            ApiRequest req(hdr, path, nullptr, content);
            ApiResponse rsp; // dummy
            rsp.httpStatus = HttpStatusOk;

            DBG_Printf(DBG_INFO, "schedule %s body: %s\n",  qPrintable(i->id), qPrintable(content));

            if (handleLightsApi(req, rsp) == REQ_NOT_HANDLED)
            {
                if (handleGroupsApi(req, rsp) == REQ_NOT_HANDLED)
                {
                    if (handleSensorsApi(req, rsp) == REQ_NOT_HANDLED)
                    {
                        DBG_Printf(DBG_INFO, "schedule was neither light nor group nor sensor request.\n");
                    }
                }
            }

            if (rsp.httpStatus != HttpStatusOk && DBG_IsEnabled(DBG_INFO) && rsp.list.size() > 0)
            {
                QString err = Json::serialize(rsp.list);
                DBG_Printf(DBG_INFO, "schedule failed: %s %s\n", rsp.httpStatus, qPrintable(err));
            }

            return;
        }
        else
        {
            DBG_Printf(DBG_INFO, "schedule %s diff %lld, %s\n", qPrintable(i->id), diff, qPrintable(i->datetime.toString()));
        }
    }
}
