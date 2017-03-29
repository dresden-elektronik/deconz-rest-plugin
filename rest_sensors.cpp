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
#include <QTextCodec>
#include <QTcpSocket>
#include <QVariantMap>
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "json.h"

/*! Sensors REST API broker.
    \param req - request data
    \param rsp - response data
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::handleSensorsApi(ApiRequest &req, ApiResponse &rsp)
{
    if (req.path[2] != "sensors")
    {
        return REQ_NOT_HANDLED;
    }

    if(!checkApikeyAuthentification(req, rsp))
    {
        return REQ_READY_SEND;
    }

    // GET /api/<apikey>/sensors
    if ((req.path.size() == 3) && (req.hdr.method() == "GET"))
    {
        return getAllSensors(req, rsp);
    }
    // GET /api/<apikey>/sensors/new
    else if ((req.path.size() == 4) && (req.hdr.method() == "GET") && (req.path[3] == "new"))
    {
        return getNewSensors(req, rsp);
    }
    // GET /api/<apikey>/sensors/<id>
    else if ((req.path.size() == 4) && (req.hdr.method() == "GET"))
    {
        return getSensor(req, rsp);
    }
    // POST /api/<apikey>/sensors
    else if ((req.path.size() == 3) && (req.hdr.method() == "POST"))
    {
        bool ok;
        QVariant var = Json::parse(req.content, ok);
        QVariantMap map = var.toMap();

        if (map.isEmpty())
        {
            return findNewSensors(req, rsp);
        }
        else
        {
            return createSensor(req, rsp);
        }
    }
    // PUT, PATCH /api/<apikey>/sensors/<id>
    else if ((req.path.size() == 4) && (req.hdr.method() == "PUT" || req.hdr.method() == "PATCH"))
    {
        return updateSensor(req, rsp);
    }
    // DELETE /api/<apikey>/sensors/<id>
    else if ((req.path.size() == 4) && (req.hdr.method() == "DELETE"))
    {
        return deleteSensor(req, rsp);
    }
    // PUT, PATCH /api/<apikey>/sensors/<id>/config
    else if ((req.path.size() == 5) && (req.hdr.method() == "PUT" || req.hdr.method() == "PATCH") && (req.path[4] == "config"))
    {
        return changeSensorConfig(req, rsp);
    }
    // PUT, PATCH /api/<apikey>/sensors/<id>/state
    else if ((req.path.size() == 5) && (req.hdr.method() == "PUT" || req.hdr.method() == "PATCH") && (req.path[4] == "state"))
    {
        return changeSensorState(req, rsp);
    }

    return REQ_NOT_HANDLED;
}

/*! GET /api/<apikey>/sensors
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::getAllSensors(const ApiRequest &req, ApiResponse &rsp)
{
    Q_UNUSED(req);
    rsp.httpStatus = HttpStatusOk;

    // handle ETag
    if (req.hdr.hasKey("If-None-Match"))
    {
        QString etag = req.hdr.value("If-None-Match");

        if (gwSensorsEtag == etag)
        {
            rsp.httpStatus = HttpStatusNotModified;
            rsp.etag = etag;
            return REQ_READY_SEND;
        }
    }

    std::vector<Sensor>::iterator i = sensors.begin();
    std::vector<Sensor>::iterator end = sensors.end();

    for (; i != end; ++i)
    {
        // ignore deleted sensors
        if (i->deletedState() == Sensor::StateDeleted)
        {
            continue;
        }

        QVariantMap map;
        sensorToMap(&*i, map);
        rsp.map[i->id()] = map;
    }

    if (rsp.map.isEmpty())
    {
        rsp.str = "{}"; // return empty object
    }

    rsp.etag = gwSensorsEtag;

    return REQ_READY_SEND;
}

/*! GET /api/<apikey>/sensors/<id>
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::getSensor(const ApiRequest &req, ApiResponse &rsp)
{
    DBG_Assert(req.path.size() == 4);

    if (req.path.size() != 4)
    {
        return REQ_NOT_HANDLED;
    }


    const QString &id = req.path[3];

    Sensor *sensor = getSensorNodeForId(id);

    if (!sensor || (sensor->deletedState() == Sensor::StateDeleted))
    {
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/sensors/%1").arg(id), QString("resource, /sensors/%1, not available").arg(id)));
        rsp.httpStatus = HttpStatusNotFound;
        return REQ_READY_SEND;
    }

    // handle ETag
    if (req.hdr.hasKey("If-None-Match"))
    {
        QString etag = req.hdr.value("If-None-Match");

        if (sensor->etag == etag)
        {
            rsp.httpStatus = HttpStatusNotModified;
            rsp.etag = etag;
            return REQ_READY_SEND;
        }
    }

    sensorToMap(sensor, rsp.map);
    rsp.httpStatus = HttpStatusOk;
    rsp.etag = sensor->etag;

    return REQ_READY_SEND;
}

/*! POST /api/<apikey>/sensors
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::createSensor(const ApiRequest &req, ApiResponse &rsp)
{   
    rsp.httpStatus = HttpStatusOk;

    bool ok;
    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();
    QVariantMap state;
    QVariantMap config;

    QString type = map["type"].toString();
    Sensor sensor;

    if (!ok)
    {
        rsp.list.append(errorToMap(ERR_INVALID_JSON, QString("/sensors"), QString("body contains invalid JSON")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    userActivity();

    if (sensors.size() >= MAX_SENSORS)
    {
        rsp.list.append(errorToMap(ERR_SENSOR_LIST_FULL , QString("/sensors/"), QString("The Sensor List has reached its maximum capacity of %1 sensors").arg(MAX_SENSORS)));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    //check required parameter
    if ((!(map.contains("name")) || !(map.contains("modelid")) || !(map.contains("swversion")) || !(map.contains("type")) || !(map.contains("uniqueid")) || !(map.contains("manufacturername"))))
    {
        rsp.list.append(errorToMap(ERR_MISSING_PARAMETER, QString("/sensors"), QString("invalid/missing parameters in body")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    //check invalid parameter
    QVariantMap::const_iterator pi = map.begin();
    QVariantMap::const_iterator pend = map.end();

    for (; pi != pend; ++pi)
    {
        if(!((pi.key() == "name") || (pi.key() == "modelid") || (pi.key() == "swversion") || (pi.key() == "type")  || (pi.key() == "uniqueid")  || (pi.key() == "manufacturername")  || (pi.key() == "state")  || (pi.key() == "config")))
        {
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/sensors/%2").arg(pi.key()), QString("parameter, %1, not available").arg(pi.key())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }

    //check valid sensortype
    if (!sensorTypes.contains(type))
    {
        rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors"), QString("invalid value, %1, for parameter, type").arg(type)));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    if ((type == "Daylight") || (type == "ZGPSwitch"))
    {
        rsp.list.append(errorToMap(ERR_NOT_ALLOWED_SENSOR_TYPE, QString("/sensors"), QString("Not allowed to create sensor type")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

        QVariantMap rspItem;
        QVariantMap rspItemState;

        // create a new sensor id
        sensor.setId("1");

        do {
            ok = true;
            std::vector<Sensor>::const_iterator i = sensors.begin();
            std::vector<Sensor>::const_iterator end = sensors.end();

            for (; i != end; ++i)
            {
                if (i->id() == sensor.id())
                {
                    sensor.setId(QString::number(i->id().toInt() + 1));
                    ok = false;
                }
            }
        } while (!ok);

        sensor.setName(map["name"].toString());
        sensor.setManufacturer(map["manufacturername"].toString());
        sensor.setModelId(map["modelid"].toString());
        sensor.setUniqueId(map["uniqueid"].toString());
        sensor.setSwVersion(map["swversion"].toString());
        sensor.setType(type);

        //setState optional
        if (map.contains("state"))
        {
            SensorState newState;
            state = map["state"].toMap();

            //check invalid parameter
            QVariantMap::const_iterator pi = state.begin();
            QVariantMap::const_iterator pend = state.end();

            for (; pi != pend; ++pi)
            {
                if(!((pi.key() == "buttonevent") || (pi.key() == "flag") || (pi.key() == "status") || (pi.key() == "presence")  || (pi.key() == "open")  || (pi.key() == "temperature")  || (pi.key() == "humidity")))
                {
                    rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/sensors/%2").arg(pi.key()), QString("parameter, %1, not available").arg(pi.key())));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }
            }

            if (!state["buttonevent"].isNull())
            {
                if (type != "CLIPSwitch" && type != "ZGPSwitch")
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors"), QString("parameter, buttonevent, not available")));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }
                if ((state["buttonevent"].type() != QVariant::Double))
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors/state"), QString("invalid value, %1, for parameter buttonevent").arg(state["buttonevent"].toString())));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }


                double buttonevent = state["buttonevent"].toDouble(&ok);

                if (ok && (buttonevent >= 0) && (buttonevent <= INT_MAX))
                {
                    newState.setButtonevent(buttonevent);
                }
            }
            if (!state["flag"].isNull())
            {
                if (type != "CLIPGenericFlag")
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors"), QString("parameter, flag, not available")));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }
                if ((state["flag"].type() != QVariant::Bool))
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors/state"), QString("invalid value, %1, for parameter flag").arg(state["flag"].toString())));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }
                newState.setFlag(state["flag"].toString());
            }
            if (!state["status"].isNull())
            {
                if (type != "CLIPGenericStatus")
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors"), QString("parameter, status, not available")));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }
                if ((state["status"].type() == QVariant::String) || (state["status"].type() == QVariant::Bool))
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors/state"), QString("invalid value, %1, for parameter status").arg(state["status"].toString())));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }
                newState.setStatus(state["status"].toString());
            }
            if (!state["presence"].isNull())
            {
                if (type != "CLIPPresence")
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors"), QString("parameter, presence, not available")));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }
                if ((state["presence"].type() != QVariant::Bool))
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors/state"), QString("invalid value, %1, for parameter presence").arg(state["presence"].toString())));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }
                newState.setPresence(state["presence"].toBool());
            }
            if (!state["open"].isNull())
            {
                if (type != "CLIPOpenClose")
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors"), QString("parameter, open, not available")));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }
                if ((state["open"].type() != QVariant::Bool))
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors/state"), QString("invalid value, %1, for parameter open").arg(state["open"].toString())));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }
                newState.setOpen(state["open"].toString());
            }
            if (!state["temperature"].isNull())
            {
                if (type != "CLIPTemperature")
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors"), QString("parameter, temperature, not available")));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }
                if ((state["temperature"].type() == QVariant::String) || (state["temperature"].type() == QVariant::Bool))
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors/state"), QString("invalid value, %1, for parameter temperature").arg(state["temperature"].toString())));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }
                newState.setTemperature(state["temperature"].toString());
            }
            if (!state["humidity"].isNull())
            {
                if (type != "CLIPHumidity")
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors"), QString("parameter, humidity, not available")));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }
                if ((state["humidity"].type() == QVariant::String) || (state["humidity"].type() == QVariant::Bool))
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors/state"), QString("invalid value, %1, for parameter humidity").arg(state["humidity"].toString())));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }
                newState.setHumidity(state["humidity"].toString());
            }

            sensor.setState(newState);
        }

        //setConfig optional
        if (map.contains("config"))
        {
            SensorConfig newConfig;
            config = map["config"].toMap();

            //check invalid parameter
            QVariantMap::const_iterator pi = config.begin();
            QVariantMap::const_iterator pend = config.end();

            for (; pi != pend; ++pi)
            {
                if(!((pi.key() == "on") || (pi.key() == "reachable") || (pi.key() == "url") || (pi.key() == "battery")))
                {
                    rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/sensors/%2").arg(pi.key()), QString("parameter, %1, not available").arg(pi.key())));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }
            }

            if (!config["on"].isNull())
            {
                newConfig.setOn(config["on"].toBool());
            }
            if (!config["reachable"].isNull())
            {
                newConfig.setReachable(config["reachable"].toBool());
            }
            if (!config["url"].isNull())
            {
                newConfig.setUrl(config["url"].toString());
            }
            if (!config["battery"].isNull())
            {
                int battery = config["battery"].toInt(&ok);
                if (!ok || (battery < 0) || (battery > 100))
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors/config"), QString("invalid value, %1, for parameter battery").arg(config["battery"].toString())));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }
                newConfig.setBattery(battery);
            }

            sensor.setConfig(newConfig);
        }
        updateEtag(sensor.etag);
        updateEtag(gwConfigEtag);
        sensor.setNeedSaveDatabase(true);
        sensors.push_back(sensor);
        queSaveDb(DB_SENSORS, DB_SHORT_SAVE_DELAY);

        rspItemState["id"] = sensor.id();
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
        rsp.httpStatus = HttpStatusOk;
        return REQ_READY_SEND;


    return REQ_READY_SEND;
}

/*! PUT, PATCH /api/<apikey>/sensors/<id>
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::updateSensor(const ApiRequest &req, ApiResponse &rsp)
{
    QString id = req.path[3];
    Sensor *sensor = getSensorNodeForId(id);
    QString name;
    bool ok;
    bool error = false;
    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();
    QVariantMap rspItem;
    QVariantMap rspItemState;

    rsp.httpStatus = HttpStatusOk;

    if (!ok)
    {
        rsp.list.append(errorToMap(ERR_INVALID_JSON, QString("/sensors"), QString("body contains invalid JSON")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    if (!sensor || (sensor->deletedState() == Sensor::StateDeleted))
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/sensors/%1").arg(id), QString("resource, /sensors/%1, not available").arg(id)));
        return REQ_READY_SEND;
    }

    userActivity();

    //check invalid parameter
    QVariantMap::const_iterator pi = map.begin();
    QVariantMap::const_iterator pend = map.end();

    for (; pi != pend; ++pi)
    {
        if(!((pi.key() == "name") || (pi.key() == "modelid") || (pi.key() == "swversion")
             || (pi.key() == "type")  || (pi.key() == "uniqueid")  || (pi.key() == "manufacturername")
             || (pi.key() == "state")  || (pi.key() == "config") || (pi.key() == "mode" && sensor->modelId() == "Lighting Switch")))
        {
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/sensors/%2").arg(pi.key()), QString("parameter, %1, not available").arg(pi.key())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }

    if (map.contains("modelid"))
    {
        error = true;
        rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/sensors/modelid"), QString("parameter, modelid, not modifiable")));
    }
    if (map.contains("swversion"))
    {
        error = true;
        rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/sensors/swversion"), QString("parameter, swversion, not modifiable")));
    }
    if (map.contains("type"))
    {
        error = true;
        rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/sensors/type"), QString("parameter, type, not modifiable")));
    }
    if (map.contains("uniqueid"))
    {
        error = true;
        rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/sensors/uniqueid"), QString("parameter, uniqueid, not modifiable")));
    }
    if (map.contains("manufacturername"))
    {
        error = true;
        rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/sensors/manufacturername"), QString("parameter, manufacturername, not modifiable")));
    }
    if (map.contains("state"))
    {
        error = true;
        rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/sensors/state"), QString("parameter, state, not modifiable")));
    }
    if (map.contains("config"))
    {
        error = true;
        rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/sensors/config"), QString("parameter, config, not modifiable")));
    }

    if (error)
    {
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    if (map.contains("name")) // optional
    {
        name = map["name"].toString();

        if ((map["name"].type() == QVariant::String) && !(name.isEmpty()) && (name.size() <= MAX_SENSOR_NAME_LENGTH))
        {
            if (sensor->name() != name)
            {
                sensor->setName(name);
                queSaveDb(DB_SENSORS, DB_SHORT_SAVE_DELAY);
                updateEtag(sensor->etag);
                updateEtag(gwConfigEtag);
            }
            rspItemState[QString("/sensors/%1/name:").arg(id)] = name;
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors/%1/name").arg(id), QString("invalid value, %1, for parameter, /sensors/%2/name").arg(name).arg(id)));
            rsp.httpStatus = HttpStatusBadRequest;
        }
    }

    if (map.contains("mode")) // optional
    {
        Sensor::SensorMode mode = (Sensor::SensorMode)map["mode"].toUInt(&ok);

        if (ok && (map["mode"].type() == QVariant::Double) && (mode == Sensor::ModeScenes || mode == Sensor::ModeTwoGroups || mode == Sensor::ModeColorTemperature))
        {
            if (sensor->mode() != mode)
            {
                sensor->setNeedSaveDatabase(true);
                sensor->setMode(mode);
            }

           if (mode == Sensor::ModeTwoGroups)
           {
               std::vector<Sensor>::iterator s = sensors.begin();
               std::vector<Sensor>::iterator send = sensors.end();

               for (; s != send; ++s)
               {
                   if (s->uniqueId() == sensor->uniqueId() && s->id() != sensor->id() && s->deletedState() == Sensor::StateDeleted)
                   {
                       s->setDeletedState(Sensor::StateNormal);
                       s->setNeedSaveDatabase(true);
                       updateEtag(s->etag);

                       std::vector<Group>::iterator g = groups.begin();
                       std::vector<Group>::iterator gend = groups.end();

                       for (; g != gend; ++g)
                       {
                           std::vector<QString> &v = g->m_deviceMemberships;

                           if ((std::find(v.begin(), v.end(), s->id()) != v.end()) && (g->state() == Group::StateDeleted))
                           {
                               g->setState(Group::StateNormal);
                               updateEtag(g->etag);
                               break;
                           }
                       }

                   }
               }
           }
           rspItemState[QString("/sensors/%1/mode:").arg(id)] = (double)mode;
           rspItem["success"] = rspItemState;
           rsp.list.append(rspItem);
           updateEtag(sensor->etag);
           updateEtag(gwConfigEtag);
           queSaveDb(DB_SENSORS | DB_GROUPS, DB_SHORT_SAVE_DELAY);
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors/%1/mode").arg(id), QString("invalid value, %1, for parameter, /sensors/%2/mode").arg((int)mode).arg(id)));
            rsp.httpStatus = HttpStatusBadRequest;
        }
    }

    return REQ_READY_SEND;
}

/*! PUT, PATCH /api/<apikey>/sensors/<id>/config
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::changeSensorConfig(const ApiRequest &req, ApiResponse &rsp)
{
    QString id = req.path[3];
    Sensor *sensor = getSensorNodeForId(id);
    SensorConfig config;
    bool ok;
    bool error = false;
    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();
    QVariantMap rspItem;
    QVariantMap rspItemState;

    QRegExp latitude("^\\d{3,3}\\.\\d{4,4}(W|E)$");
    QRegExp longitude("^\\d{3,3}\\.\\d{4,4}(N|S)$");

    rsp.httpStatus = HttpStatusOk;

    if (!ok)
    {
        rsp.list.append(errorToMap(ERR_INVALID_JSON, QString("/sensors/config"), QString("body contains invalid JSON")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    if (!sensor || (sensor->deletedState() == Sensor::StateDeleted))
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/sensors/%1").arg(id), QString("resource, /sensors/%1, not available").arg(id)));
        return REQ_READY_SEND;
    }

    config = sensor->config();

    userActivity();

    //check invalid parameter
    QVariantMap::const_iterator pi = map.begin();
    QVariantMap::const_iterator pend = map.end();

    for (; pi != pend; ++pi)
    {
        if(!((pi.key() == "duration") || (pi.key() == "battery") || (pi.key() == "url") || (pi.key() == "on") || (pi.key() == "reachable") || (pi.key() == "long")
            || (pi.key() == "lat") || (pi.key() == "sunriseoffset") || (pi.key() == "sunsetoffset")))
        {
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/sensors/config/%2").arg(pi.key()), QString("parameter, %1, not available").arg(pi.key())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }

    //check if values are modifiable
    if (map.contains("reachable"))
    {
        error = true;
        rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/sensors/%1/config").arg(id), QString("parameter, reachable, not modifiable")));
    }
    if (map.contains("daylight"))
    {
        error = true;
        rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/sensors/%1/config").arg(id), QString("parameter, daylight, not modifiable")));
    }
    if (map.contains("on"))
    {
        if(sensor->type() == "Daylight")
        {
            error = true;
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/sensors/%1/config").arg(id), QString("parameter, on, not modifiable")));
        }
    }
    if (map.contains("url"))
    {
        if(sensor->type() == "ZGPSwitch")
        {
            error = true;
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/sensors/%1/config").arg(id), QString("parameter, url, not modifiable")));
        }
    }
    if (map.contains("battery"))
    {
        if(sensor->type() == "ZGPSwitch")
        {
            error = true;
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/sensors/%1/config").arg(id), QString("parameter, battery, not modifiable")));
        }
    }
    if (map.contains("long"))
    {
        if(sensor->type() != "Daylight")
        {
            error = true;
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/sensors/%1/config").arg(id), QString("parameter, long not modifiable")));
        }
    }
    if (map.contains("lat"))
    {
        if(sensor->type() != "Daylight")
        {
            error = true;
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/sensors/%1/config").arg(id), QString("parameter, lat, not modifiable")));
        }
    }
    if (map.contains("sunsetoffset"))
    {
        if(sensor->type() != "Daylight")
        {
            error = true;
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/sensors/%1/config").arg(id), QString("parameter, sunsetoffset, not modifiable")));
        }
    }
    if (map.contains("sunriseoffset"))
    {
        if(sensor->type() != "Daylight")
        {
            error = true;
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/sensors/%1/config").arg(id), QString("parameter, sunriseoffset, not modifiable")));
        }
    }
    if (error)
    {
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    //update values
    if (map.contains("on"))
    {
        rspItemState[QString("/sensors/%1/config/on").arg(id)] = map["on"].toString();
        rspItem["success"] = rspItemState;
        config.setOn(map["on"].toBool());
    }
    if (map.contains("url"))
    {
        rspItemState[QString("/sensors/%1/config/url").arg(id)] = map["url"].toString();
        rspItem["success"] = rspItemState;
        config.setUrl(map["url"].toString());
    }
    if (map.contains("battery"))
    {
        int battery = map["battery"].toInt(&ok);

        if (!ok || (battery < 0) || (battery > 100))
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors/%1/config").arg(id), QString("invalid value, %1, for parameter battery").arg(map["battery"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
        rspItemState[QString("/sensors/%1/config/battery").arg(id)] = map["battery"];
        rspItem["success"] = rspItemState;
        config.setBattery(battery);
    }
    if (map.contains("long"))
    {
        if ((map["long"].type() != QVariant::String) && (!map["long"].toString().contains(longitude)))
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors/%1/config").arg(id), QString("invalid value, %1, for parameter long").arg(map["long"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
        rspItemState[QString("/sensors/%1/config/on").arg(id)] = map["long"].toString();
        rspItem["success"] = rspItemState;
        config.setLongitude(map["long"].toString());
    }
    if (map.contains("lat"))
    {
        if ((map["lat"].type() != QVariant::String) && (!map["lat"].toString().contains(latitude)))
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors/%1/config").arg(id), QString("invalid value, %1, for parameter lat").arg(map["lat"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
        rspItemState[QString("/sensors/%1/config/lat").arg(id)] = map["lat"].toString();
        rspItem["success"] = rspItemState;
        config.setLat(map["lat"].toString());
    }
    if (map.contains("sunriseoffset"))
    {
        if (((map["sunriseoffset"].toInt() < -120) || (map["sunriseoffset"].toInt() > 120)) || (map["sunriseoffset"].type() == QVariant::String) || (map["sunriseoffset"].type() == QVariant::Bool))
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors/%1/config").arg(id), QString("invalid value, %1, for parameter sunriseoffset").arg(map["sunriseoffset"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
        rspItemState[QString("/sensors/%1/config/sunriseoffset").arg(id)] = map["sunriseoffset"];
        rspItem["success"] = rspItemState;
        config.setSunriseoffset(map["sunriseoffset"].toString());
    }
    if (map.contains("sunsetoffset"))
    {
        if (((map["sunsetoffset"].toInt() < -120) || (map["sunsetoffset"].toInt() > 120)) || (map["sunsetoffset"].type() == QVariant::String) || (map["sunsetoffset"].type() == QVariant::Bool))
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors/%1/config").arg(id), QString("invalid value, %1, for parameter sunsetoffset").arg(map["sunsetoffset"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
        rspItemState[QString("/sensors/%1/config/sunsetoffset").arg(id)] = map["sunsetoffset"];
        rspItem["success"] = rspItemState;
        config.setSunsetoffset(map["sunsetoffset"].toString());
    }
    if (map.contains("duration"))
    {
        double duration = map["duration"].toDouble(&ok);
        if (!ok || (map["duration"].type() != QVariant::Double) || (duration < 0) || (duration > 65535))
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors/%1/config").arg(id), QString("invalid value, %1, for parameter duration").arg(map["duration"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
        rspItemState[QString("/sensors/%1/config/duration").arg(id)] = map["duration"].toString();
        rspItem["success"] = rspItemState;

        if (config.duration() != duration)
        {
            config.setDuration(duration);
            DBG_Printf(DBG_INFO, "Force read/write of occupaction delay for sensor %s\n", qPrintable(sensor->address().toStringExt()));
            sensor->enableRead(WRITE_OCCUPANCY_CONFIG);
            sensor->setNextReadTime(WRITE_OCCUPANCY_CONFIG, QTime::currentTime());
            Q_Q(DeRestPlugin);
            q->startZclAttributeTimer(0);
        }
    }

    sensor->setConfig(config);
    sensor->setNeedSaveDatabase(true);
    rsp.list.append(rspItem);
    updateEtag(sensor->etag);
    updateEtag(gwConfigEtag);
    queSaveDb(DB_SENSORS, DB_SHORT_SAVE_DELAY);

    return REQ_READY_SEND;
}

/*! PUT, PATCH /api/<apikey>/sensors/<id>/state
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::changeSensorState(const ApiRequest &req, ApiResponse &rsp)
{
    QString id = req.path[3];
    Sensor *sensor = getSensorNodeForId(id);
    SensorState state;
    bool ok;
    bool error = false;
    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();
    QVariantMap rspItem;
    QVariantMap rspItemState;

    rsp.httpStatus = HttpStatusOk;

    if (!ok)
    {
        rsp.list.append(errorToMap(ERR_INVALID_JSON, QString("/sensors/%1/state").arg(id), QString("body contains invalid JSON")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    if (!sensor || (sensor->deletedState() == Sensor::StateDeleted))
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/sensors/%1").arg(id), QString("resource, /sensors/%1, not available").arg(id)));
        return REQ_READY_SEND;
    }

    state = sensor->state();

    userActivity();

    //check invalid parameter
    QVariantMap::const_iterator pi = map.begin();
    QVariantMap::const_iterator pend = map.end();

    for (; pi != pend; ++pi)
    {
        if(!((pi.key() == "lastupdated") || (pi.key() == "flag") || (pi.key() == "status") || (pi.key() == "presence")
            || (pi.key() == "open") || (pi.key() == "buttonevent") || (pi.key() == "temperature") || (pi.key() == "humidity") || (pi.key() == "daylight")))
        {
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/sensors/config/%2").arg(pi.key()), QString("parameter, %1, not available").arg(pi.key())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }

    //check if values are modifiable
    if (map.contains("lastupdated"))
    {
        error = true;
        rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/sensors/%1/state/lastupdated").arg(id), QString("parameter, lastupdated, not modifiable")));
    }
    if (map.contains("flag"))
    {
        if(sensor->type() != "CLIPGenericFlag")
        {
            error = true;
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/sensors/%1/state/flag"), QString("parameter, flag, not modifiable")));
        }
    }
    if (map.contains("status"))
    {
        if(sensor->type() != "CLIPGenericStatus")
        {
            error = true;
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/sensors/%1/state/status"), QString("parameter, status, not modifiable")));
        }
    }
    if (map.contains("presence"))
    {
        if(sensor->type() != "CLIPPresence")
        {
            error = true;
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/sensors/%1/state/presence"), QString("parameter, presence, not modifiable")));
        }
    }
    if (map.contains("open"))
    {
        if(sensor->type() != "CLIPOpenClose")
        {
            error = true;
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/sensors/%1/state/open"), QString("parameter, open, not modifiable")));
        }
    }
    if (map.contains("buttonevent"))
    {
        if((sensor->type() != "CLIPSwitch"))
        {
            error = true;
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/sensors/%1/state/buttonevent"), QString("parameter, buttonevent, not modifiable")));
        }
    }
    if (map.contains("temperature"))
    {
        if(sensor->type() != "CLIPTemperature")
        {
            error = true;
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/sensors/%1/state/temperature"), QString("parameter, temperature, not modifiable")));
        }
    }
    if (map.contains("humidity"))
    {
        if(sensor->type() != "CLIPHumidity")
        {
            error = true;
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/sensors/%1/state/humidity"), QString("parameter, humidity, not modifiable")));
        }
    }
    if (map.contains("daylight"))
    {
        error = true;
        rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/sensors/%1/state/lastupdated").arg(id), QString("parameter, daylight, not modifiable")));
    }
    if (error)
    {
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    //update values
    if (map.contains("flag"))
    {
        if ((map["flag"].type() != QVariant::Bool))
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors/%1/state").arg(id), QString("invalid value, %1, for parameter flag").arg(map["flag"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
            rspItemState[QString("/sensors/%1/state/flag").arg(id)] = map["flag"];
            rspItem["success"] = rspItemState;
            state.setFlag(map["flag"].toString());
    }
    if (map.contains("status"))
    {
        if ((map["status"].type() == QVariant::String) || (map["status"].type() == QVariant::Bool))
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors/%1/state").arg(id), QString("invalid value, %1, for parameter status").arg(map["status"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
            rspItemState[QString("/sensors/%1/state/status").arg(id)] = map["status"];
            rspItem["success"] = rspItemState;
            state.setStatus(map["status"].toString());
    }
    if (map.contains("presence"))
    {
        if ((map["presence"].type() != QVariant::Bool))
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors/%1/state").arg(id), QString("invalid value, %1, for parameter presence").arg(map["presence"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
            rspItemState[QString("/sensors/%1/state/presence").arg(id)] = map["presence"];
            rspItem["success"] = rspItemState;
            state.setPresence(map["presence"].toBool());
    }
    if (map.contains("open"))
    {
        if ((map["open"].type() != QVariant::Bool))
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors/%1/state").arg(id), QString("invalid value, %1, for parameter open").arg(map["open"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
            rspItemState[QString("/sensors/%1/state/open").arg(id)] = map["open"];
            rspItem["success"] = rspItemState;
            state.setOpen(map["open"].toString());
    }
    if (map.contains("buttonevent"))
    {
        if (map["buttonevent"].type() != QVariant::Double)
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors/%1/state").arg(id), QString("invalid value, %1, for parameter buttonevent").arg(map["buttonevent"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

        double buttonevent = map["buttonevent"].toDouble(&ok);

        if (ok && (buttonevent >= 0) && (buttonevent <= INT_MAX))
        {
            state.setButtonevent(buttonevent);
            rspItemState[QString("/sensors/%1/state/buttonevent").arg(id)] = map["buttonevent"];
            rspItem["success"] = rspItemState;
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors/%1/state").arg(id), QString("invalid value, %1, for parameter buttonevent").arg(map["buttonevent"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }
    if (map.contains("temperature"))
    {
        if ((map["temperature"].type() == QVariant::String) || (map["temperature"].type() == QVariant::Bool))
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors/%1/state").arg(id), QString("invalid value, %1, for parameter temperature").arg(map["temperature"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
            rspItemState[QString("/sensors/%1/state/temperature").arg(id)] = map["temperature"];
            rspItem["success"] = rspItemState;
            state.setTemperature(map["temperature"].toString());
    }
    if (map.contains("humidity"))
    {
        if ((map["humidity"].type() == QVariant::String) || (map["humidity"].type() == QVariant::Bool))
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors/%1/state").arg(id), QString("invalid value, %1, for parameter humidity").arg(map["humidity"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
            rspItemState[QString("/sensors/%1/state/humidity").arg(id)] = map["humidity"];
            rspItem["success"] = rspItemState;
            state.setHumidity(map["humidity"].toString());
    }

    sensor->setState(state);
    sensor->setNeedSaveDatabase(true);
    rsp.list.append(rspItem);
    updateEtag(sensor->etag);
    updateEtag(gwConfigEtag);
    queSaveDb(DB_SENSORS, DB_SHORT_SAVE_DELAY);

    return REQ_READY_SEND;
}

/*! DELETE /api/<apikey>/sensors/<id>
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::deleteSensor(const ApiRequest &req, ApiResponse &rsp)
{
    QString id = req.path[3];
    Sensor *sensor = getSensorNodeForId(id);

    userActivity();

    if (!sensor || (sensor->deletedState() == Sensor::StateDeleted))
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/sensors/%1").arg(id), QString("resource, /sensors/%1, not available").arg(id)));
        return REQ_READY_SEND;
    }

    bool ok;
    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();

    if (!ok)
    {
        rsp.list.append(errorToMap(ERR_INVALID_JSON, QString("/sensors/%1").arg(id), QString("body contains invalid JSON")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    sensor->setDeletedState(Sensor::StateDeleted);
    sensor->setNeedSaveDatabase(true);

    bool hasReset = map.contains("reset");

    if (hasReset)
    {
        if (map["reset"].type() == QVariant::Bool)
        {
            bool reset = map["reset"].toBool();

            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/sensors/%1/reset").arg(id)] = reset;
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);

            if (reset)
            {
                sensor->setResetRetryCount(10);
            }
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors/%1/reset").arg(id), QString("invalid value, %1, for parameter, reset").arg(map["reset"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }
    else
    {
        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState["id"] = id;
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
        rsp.httpStatus = HttpStatusOk;
    }

    queSaveDb(DB_SENSORS, DB_SHORT_SAVE_DELAY);

    updateEtag(gwConfigEtag);
    rsp.httpStatus = HttpStatusOk;

    return REQ_READY_SEND;
}

/*! POST /api/<apikey>/sensors
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::findNewSensors(const ApiRequest &req, ApiResponse &rsp)
{
    Q_UNUSED(req);

    if (!isInNetwork())
    {
        rsp.list.append(errorToMap(ERR_NOT_CONNECTED, QLatin1String("/sensors"), QLatin1String("Not connected")));
        rsp.httpStatus = HttpStatusServiceUnavailable;
        return REQ_READY_SEND;
    }

    startFindSensors();
    {
        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState[QLatin1String("/sensors")] = QLatin1String("Searching for new devices");
        rspItemState[QLatin1String("/sensors/duration")] = (double)findSensorsTimeout;
        rspItem[QLatin1String("success")] = rspItemState;
        rsp.list.append(rspItem);
    }

    rsp.httpStatus = HttpStatusOk;

    return REQ_READY_SEND;
}

/*! GET /api/<apikey>/sensors/new
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::getNewSensors(const ApiRequest &req, ApiResponse &rsp)
{
    Q_UNUSED(req);

    if (findSensorsState == FindSensorsActive)
    {
        rsp.map["lastscan"] = QLatin1String("active");
    }
    else if (findSensorsState == FindSensorsDone)
    {
        rsp.map["lastscan"] = lastSensorsScan;
    }
    else
    {
        rsp.map["lastscan"] = QLatin1String("none");
    }

    rsp.httpStatus = HttpStatusOk;
    return REQ_READY_SEND;
}

/*! Put all sensor parameters in a map.
    \return true - on success
            false - on error
 */
bool DeRestPluginPrivate::sensorToMap(const Sensor *sensor, QVariantMap &map)
{
    if (!sensor)
    {
        return false;
    }

    QVariantMap state;
    QVariantMap config;

    //state
    if (!sensor->state().lastupdated().isEmpty())
    {
        state["lastupdated"] = sensor->state().lastupdated();
    }
    if (sensor->type().endsWith(QLatin1String("Light")))
    {
        state["lux"] = (double)sensor->state().lux();
    }
    else if (sensor->type().endsWith(QLatin1String("Presence")))
    {
        state["presence"] = sensor->state().presence();

        if (sensor->config().duration() >= 0)
        {
            config["duration"] = sensor->config().duration();
        }
    }
    else if (sensor->type().endsWith(QLatin1String("Temperature")))
    {
        if (!sensor->state().temperature().isEmpty())
        {
            state["temperature"] = sensor->state().temperature().toInt();
        }
    }
    else if (sensor->type().endsWith(QLatin1String("Switch")))
    {
        if (sensor->state().buttonevent() >= 0)
        {
            state["buttonevent"] = (double)sensor->state().buttonevent();
        }
    }
    else
    {
        if (!sensor->state().flag().isEmpty())
        {
            state["flag"] = (sensor->state().flag() == "true")?true:false;
        }
        if (!sensor->state().status().isEmpty())
        {
            state["status"] = sensor->state().status().toInt();
        }
        if (!sensor->state().open().isEmpty())
        {
            state["open"] = (sensor->state().open() == "true")?true:false;
        }
        if (!sensor->state().humidity().isEmpty())
        {
            state["humidity"] = sensor->state().humidity().toInt();
        }
        if (!sensor->state().daylight().isEmpty())
        {
            state["daylight"] = (sensor->state().daylight() == "true")?true:false;
        }
    }

    //config
    config["on"] = sensor->config().on();

//config
    config["on"] = sensor->config().on();

    if (sensor->type() != QLatin1String("ZGPSwitch"))
    {
        config["reachable"] = sensor->config().reachable();
    }

    if (sensor->config().battery() <= 100) // valid value?
    {
        config["battery"] = (double)sensor->config().battery();
    }

    if (!sensor->config().url().isEmpty())
    {
        config["url"] = sensor->config().url();
    }
    if (!sensor->config().longitude().isEmpty())
    {
        config["long"] = sensor->config().longitude();
    }
    if (!sensor->config().lat().isEmpty())
    {
        config["lat"] = sensor->config().lat();
    }
    if (!sensor->config().sunriseoffset().isEmpty())
    {
        config["sunriseoffset"] = sensor->config().sunriseoffset().toInt();
    }
    if (!sensor->config().sunsetoffset().isEmpty())
    {
        config["sunsetoffset"] = sensor->config().sunsetoffset().toInt();
    }


    //sensor
    map["name"] = sensor->name();
    map["type"] = sensor->type();
    if (!sensor->modelId().isEmpty())
    {
        map["modelid"] = sensor->modelId();
    }
    if (sensor->fingerPrint().endpoint != INVALID_ENDPOINT)
    {
        map["ep"] = sensor->fingerPrint().endpoint;
    }
    if (!sensor->swVersion().isEmpty())
    {
        map["swversion"] = sensor->swVersion();
    }
    if (sensor->mode() != Sensor::ModeNone &&
        sensor->type().endsWith(QLatin1String("Switch")))
    {
        map["mode"] = (double)sensor->mode();
    }
    if (!sensor->manufacturer().isEmpty())
    {
        map["manufacturername"] = sensor->manufacturer();
    }
    map["uniqueid"] = sensor->uniqueId();
    map["state"] = state;
    map["config"] = config;

    QString etag = sensor->etag;
    etag.remove('"'); // no quotes allowed in string
    map["etag"] = etag;
    return true;
}

/*! Starts the search for new sensors.
 */
void DeRestPluginPrivate::startFindSensors()
{
    if (findSensorsState == FindSensorsIdle || findSensorsState == FindSensorsDone)
    {
        findSensorCandidates.clear();
        lastSensorsScan = QDateTime::currentDateTimeUtc().toString(QLatin1String("yyyy-MM-ddTHH:mm:ss"));
        QTimer::singleShot(1000, this, SLOT(findSensorsTimerFired()));
        findSensorsState = FindSensorsActive;
    }
    else
    {
        Q_ASSERT(findSensorsState == FindSensorsActive);
    }

    findSensorsTimeout = gwNetworkOpenDuration;
    gwPermitJoinResend = findSensorsTimeout;
    if (!resendPermitJoinTimer->isActive())
    {
        resendPermitJoinTimer->start(100);
    }
}

/*! Handler for find sensors active state.
 */
void DeRestPluginPrivate::findSensorsTimerFired()
{
    if (gwPermitJoinResend == 0)
    {
        if (gwPermitJoinDuration == 0)
        {
            findSensorsTimeout = 0; // done
        }
    }

    if (findSensorsTimeout > 0)
    {
        findSensorsTimeout--;
        QTimer::singleShot(1000, this, SLOT(findSensorsTimerFired()));
    }

    if (findSensorsTimeout == 0)
    {
        findSensorsState = FindSensorsDone;
    }
}

/*! Check insta mac address to model identifier.
 */
void DeRestPluginPrivate::checkInstaModelId(Sensor *sensor)
{
    const quint64 instaMacPrefix = 0x000f171241000000ULL;
    if (sensor && (sensor->address().ext() & instaMacPrefix) == instaMacPrefix)
    {
        if (!sensor->modelId().endsWith(QLatin1String("_1")))
        {   // extract model identifier from mac address 6th byte
            const quint64 model = (sensor->address().ext() >> 16) & 0xff;
            QString modelId;
            if      (model == 0x01) { modelId = QLatin1String("HS_4f_GJ_1"); }
            else if (model == 0x02) { modelId = QLatin1String("WS_4f_J_1"); }
            else if (model == 0x03) { modelId = QLatin1String("WS_3f_G_1"); }

            if (!modelId.isEmpty() && sensor->modelId() != modelId)
            {
                sensor->setModelId(modelId);
                sensor->setNeedSaveDatabase(true);
                updateSensorEtag(sensor);
            }
        }
    }
}

/*! Heuristic to detect the type and configuration of devices.
 */
void DeRestPluginPrivate::handleIndicationFindSensors(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    if (ind.profileId() == ZDP_PROFILE_ID && ind.clusterId() == ZDP_DEVICE_ANNCE_CLID)
    {
        QDataStream stream(ind.asdu());
        stream.setByteOrder(QDataStream::LittleEndian);

        quint8 seq;
        quint16 nwk;
        quint64 ext;
        quint8 macCapabilities;

        stream >> seq;
        stream >> nwk;
        stream >> ext;
        stream >> macCapabilities;

        const quint64 philipsMacPrefix = 0x0017880000000000ULL;

        if ((ext & philipsMacPrefix) == philipsMacPrefix)
        {
            fastProbePhilips(ext, nwk, macCapabilities);
            return;
        }

        const quint64 bjeMacPrefix = 0xd85def0000000000ULL;

        if ((ext & bjeMacPrefix) == bjeMacPrefix)
        {
            fastProbeBuschJaeger(ext, nwk, macCapabilities);
        }

        // currently only end-devices are supported
        if (macCapabilities == 0 || (macCapabilities & deCONZ::MacDeviceIsFFD))
        {
            return;
        }

        std::vector<SensorCandidate>::const_iterator i = findSensorCandidates.begin();
        std::vector<SensorCandidate>::const_iterator end = findSensorCandidates.end();

        for (; i != end; ++i)
        {
            if (i->address.ext() == ext || i->address.nwk() == nwk)
            {
                return;
            }
        }

        SensorCandidate sc;
        sc.address.setExt(ext);
        sc.address.setNwk(nwk);
        sc.macCapabilities = macCapabilities;
        findSensorCandidates.push_back(sc);
        return;
    }
    if (ind.profileId() == ZDP_PROFILE_ID && ind.clusterId() == ZDP_ACTIVE_ENDPOINTS_RSP_CLID)
    {

    }
    else if (ind.profileId() == ZLL_PROFILE_ID || ind.profileId() == HA_PROFILE_ID)
    {
        switch (ind.clusterId())
        {
        case ONOFF_CLUSTER_ID:
        case SCENE_CLUSTER_ID:
            if ((zclFrame.frameControl() & deCONZ::ZclFCClusterCommand) == 0)
            {
                return;
            }

            if (zclFrame.frameControl() & deCONZ::ZclFCDirectionServerToClient)
            {
                return;
            }
            break; // ok

        default:
            return;
        }
    }
    else
    {
        return;
    }

    if (ind.dstAddressMode() != deCONZ::ApsGroupAddress && ind.dstAddressMode() != deCONZ::ApsNwkAddress)
    {
        return;
    }

    SensorCandidate *sc = 0;
    {
        std::vector<SensorCandidate>::iterator i = findSensorCandidates.begin();
        std::vector<SensorCandidate>::iterator end = findSensorCandidates.end();

        for (; i != end; ++i)
        {
            if (ind.srcAddress().hasExt() && i->address.ext() == ind.srcAddress().ext())
            {
                sc = &*i;
                break;
            }

            if (ind.srcAddress().hasNwk() && i->address.nwk() == ind.srcAddress().nwk())
            {
                sc = &*i;
                break;
            }
        }
    }

    quint8 macCapabilities = 0;
    deCONZ::Address indAddress;
    if (!sc)
    {
        Sensor *sensor = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), ind.srcEndpoint());

        if (sensor)
        {
            indAddress = sensor->address();
            if (sensor->node())
            {
                macCapabilities = (int)sensor->node()->macCapabilities();
            }
        }

        if (apsCtrl && (!sensor || (macCapabilities == 0)))
        {
            int i = 0;
            const deCONZ::Node *node;

            while (apsCtrl->getNode(i, &node) == 0)
            {
                /*if (node->macCapabilities() == 0)
                {
                    // ignore
                }
                else*/ if (node->address().hasExt() && ind.srcAddress().hasExt() &&
                    ind.srcAddress().ext() == node->address().ext())
                {
                    indAddress = node->address();
                    macCapabilities = node->macCapabilities();
                    break;
                }
                else if (node->address().hasNwk() && ind.srcAddress().hasNwk() &&
                    ind.srcAddress().nwk() == node->address().nwk())
                {
                    indAddress = node->address();
                    macCapabilities = node->macCapabilities();
                    break;
                }
                i++;
            }
        }
    }

    // currently only end-devices are supported
    if (!sc && (macCapabilities == 0 || (macCapabilities & deCONZ::MacDeviceIsFFD)))
    {
        return;
    }

    if (!sc && indAddress.hasExt() && indAddress.hasNwk())
    {
        SensorCandidate sc2;
        sc2.address = indAddress;
        sc2.macCapabilities = macCapabilities;
        findSensorCandidates.push_back(sc2);
        sc = &findSensorCandidates.back();
    }

    if (!sc) // we need a valid candidate from device announce or cache
    {
        return;
    }

    const quint64 deMacPrefix = 0x00212effff000000ULL;

    // check for dresden elektronik devices
    if ((sc->address.ext() & deMacPrefix) == deMacPrefix)
    {
        if (sc->macCapabilities & deCONZ::MacDeviceIsFFD) // end-devices only
            return;

        if (ind.profileId() != HA_PROFILE_ID)
            return;

        SensorCommand cmd;
        cmd.cluster = ind.clusterId();
        cmd.endpoint = ind.srcEndpoint();
        cmd.dstGroup = ind.dstAddress().group();
        cmd.zclCommand = zclFrame.commandId();
        cmd.zclCommandParameter = 0;

        // filter
        if (cmd.endpoint == 0x01 && cmd.cluster == ONOFF_CLUSTER_ID)
        {
            // on: Lighting and Scene Switch left button
            DBG_Printf(DBG_INFO, "Lighting or Scene Switch left button\n");
        }
        else if (cmd.endpoint == 0x02 && cmd.cluster == ONOFF_CLUSTER_ID)
        {
            // on: Lighting Switch right button
            DBG_Printf(DBG_INFO, "Lighting Switch right button\n");
        }
        else if (cmd.endpoint == 0x01 && cmd.cluster == SCENE_CLUSTER_ID && cmd.zclCommand == 0x05
                 && zclFrame.payload().size() >= 3 && zclFrame.payload().at(2) == 0x04)
        {
            // recall scene: Scene Switch
            cmd.zclCommandParameter = zclFrame.payload()[2]; // sceneId
            DBG_Printf(DBG_INFO, "Scene Switch scene %u\n", cmd.zclCommandParameter);
        }
        else
        {
            return;
        }

        bool found = false;
        for (size_t i = 0; i < sc->rxCommands.size(); i++)
        {
            if (sc->rxCommands[i] == cmd)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            sc->rxCommands.push_back(cmd);
        }

        bool isLightingSwitch = false;
        bool isSceneSwitch = false;
        quint16 group1 = 0;
        quint16 group2 = 0;

        for (size_t i = 0; i < sc->rxCommands.size(); i++)
        {
            const SensorCommand &c = sc->rxCommands[i];
            if (c.cluster == SCENE_CLUSTER_ID && c.zclCommandParameter == 0x04 && c.endpoint == 0x01)
            {
                group1 = c.dstGroup;
                isSceneSwitch = true;
                DBG_Printf(DBG_INFO, "Scene Switch group1 0x%04X\n", group1);
                break;
            }
            else if (c.cluster == ONOFF_CLUSTER_ID && c.endpoint == 0x01)
            {
                group1 = c.dstGroup;
            }
            else if (c.cluster == ONOFF_CLUSTER_ID && c.endpoint == 0x02)
            {
                group2 = c.dstGroup;
            }

            if (!isSceneSwitch && group1 != 0 && group2 != 0)
            {
                if (group1 > group2)
                {
                    std::swap(group1, group2); // reorder
                }
                isLightingSwitch = true;
                DBG_Printf(DBG_INFO, "Lighting Switch group1 0x%04X, group2 0x%04X\n", group1, group2);
                break;
            }
        }

        Sensor *s1 = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), 0x01);
        Sensor *s2 = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), 0x02);

        if (isSceneSwitch || isLightingSwitch)
        {
            Sensor sensorNode;
            SensorFingerprint &fp = sensorNode.fingerPrint();
            fp.endpoint = 0x01;
            fp.deviceId = DEV_ID_ZLL_COLOR_CONTROLLER;
            fp.profileId = HA_PROFILE_ID;
            fp.inClusters.push_back(BASIC_CLUSTER_ID);
            fp.inClusters.push_back(COMMISSIONING_CLUSTER_ID);
            fp.outClusters.push_back(ONOFF_CLUSTER_ID);
            fp.outClusters.push_back(LEVEL_CLUSTER_ID);
            fp.outClusters.push_back(SCENE_CLUSTER_ID);

            sensorNode.setIsAvailable(true);
            sensorNode.setNode(0);
            sensorNode.address() = sc->address;
            sensorNode.setType("ZHASwitch");
            sensorNode.fingerPrint() = fp;
            sensorNode.setUniqueId(generateUniqueId(sensorNode.address().ext(), sensorNode.fingerPrint().endpoint));
            sensorNode.setManufacturer(QLatin1String("dresden elektronik"));

            SensorConfig sensorConfig;
            sensorConfig.setReachable(true);
            sensorNode.setConfig(sensorConfig);

            sensorNode.setNeedSaveDatabase(true);
            updateEtag(sensorNode.etag);

            bool update = false;

            if (!s1 && isSceneSwitch)
            {
                openDb();
                sensorNode.setId(QString::number(getFreeSensorId()));
                closeDb();
                sensorNode.setMode(Sensor::ModeScenes);
                sensorNode.setModelId(QLatin1String("Scene Switch"));
                sensorNode.setName(QString("Scene Switch %1").arg(sensorNode.id()));
                sensorNode.setNeedSaveDatabase(true);
                sensors.push_back(sensorNode);
                s1 = &sensors.back();
                update = true;
            }
            else if (isLightingSwitch)
            {
                if (!s1)
                {
                    openDb();
                    sensorNode.setId(QString::number(getFreeSensorId()));
                    closeDb();
                    sensorNode.setMode(Sensor::ModeTwoGroups);
                    sensorNode.setModelId("Lighting Switch");
                    sensorNode.setName(QString("Lighting Switch %1").arg(sensorNode.id()));
                    sensorNode.setNeedSaveDatabase(true);
                    sensors.push_back(sensorNode);
                    s1 = &sensors.back();
                    update = true;
                }

                if (!s2)
                {
                    openDb();
                    sensorNode.setId(QString::number(getFreeSensorId()));
                    closeDb();
                    sensorNode.setMode(Sensor::ModeTwoGroups);
                    sensorNode.setName(QString("Lighting Switch %1").arg(sensorNode.id()));
                    sensorNode.setNeedSaveDatabase(true);
                    sensorNode.fingerPrint().endpoint = 0x02;
                    sensorNode.setUniqueId(generateUniqueId(sensorNode.address().ext(), sensorNode.fingerPrint().endpoint));
                    sensors.push_back(sensorNode);
                    s2 = &sensors.back();
                    update = true;
                }
            }

            // check updated data
            if (s1 && s1->modelId().isEmpty())
            {
                if      (isSceneSwitch)    { s1->setModelId(QLatin1String("Scene Switch")); }
                else if (isLightingSwitch) { s1->setModelId(QLatin1String("Lighting Switch")); }
                s1->setNeedSaveDatabase(true);
                update = true;
            }

            if (s2 && s2->modelId().isEmpty())
            {
                if (isLightingSwitch) { s2->setModelId(QLatin1String("Lighting Switch")); }
                s2->setNeedSaveDatabase(true);
                update = true;
            }

            if (s1 && s1->manufacturer().isEmpty())
            {
                s1->setManufacturer(QLatin1String("dresden elektronik"));
                s1->setNeedSaveDatabase(true);
                update = true;
            }

            if (s2 && s2->manufacturer().isEmpty())
            {
                s2->setManufacturer(QLatin1String("dresden elektronik"));
                s2->setNeedSaveDatabase(true);
                update = true;
            }

            // create or update first group
            Group *g = (s1 && group1 != 0) ? getGroupForId(group1) : 0;
            if (!g && s1 && group1 != 0)
            {
                // delete older groups of this switch permanently
                deleteOldGroupOfSwitch(s1, group1);

                //create new switch group
                Group group;
                group.setAddress(group1);
                group.addDeviceMembership(s1->id());
                group.setName(QString("%1").arg(s1->name()));
                updateEtag(group.etag);
                groups.push_back(group);
                update = true;
            }
            else if (g && s1)
            {
                if (g->state() == Group::StateDeleted)
                {
                    g->setState(Group::StateNormal);
                }

                // check for changed device memberships
                if (!g->m_deviceMemberships.empty())
                {
                    if (isLightingSwitch || isSceneSwitch) // only support one device member per group
                    {
                        if (g->m_deviceMemberships.size() > 1 || g->m_deviceMemberships.front() != s1->id())
                        {
                            g->m_deviceMemberships.clear();
                        }
                    }
                }

                if (g->addDeviceMembership(s1->id()))
                {
                    updateEtag(g->etag);
                    update = true;
                }
            }

            // create or update second group (if needed)
            g = (s2 && group2 != 0) ? getGroupForId(group2) : 0;
            if (!g && s2 && group2 != 0)
            {
                // delete older groups of this switch permanently
                deleteOldGroupOfSwitch(s2, group2);

                //create new switch group
                Group group;
                group.setAddress(group2);
                group.addDeviceMembership(s2->id());
                group.setName(QString("%1").arg(s2->name()));
                updateEtag(group.etag);
                groups.push_back(group);
            }
            else if (g && s2)
            {
                if (g->state() == Group::StateDeleted)
                {
                    g->setState(Group::StateNormal);
                }

                // check for changed device memberships
                if (!g->m_deviceMemberships.empty())
                {
                    if (isLightingSwitch || isSceneSwitch) // only support one device member per group
                    {
                        if (g->m_deviceMemberships.size() > 1 || g->m_deviceMemberships.front() != s2->id())
                        {
                            g->m_deviceMemberships.clear();
                        }
                    }
                }

                if (g->addDeviceMembership(s2->id()))
                {
                    updateEtag(g->etag);
                    update = true;
                }
            }

            if (update)
            {
                queSaveDb(DB_GROUPS | DB_SENSORS, DB_SHORT_SAVE_DELAY);
            }
        }
    }
}
