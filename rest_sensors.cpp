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

    if (!type.startsWith(QLatin1String("CLIP")))
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
            QVariantMap state = map["state"].toMap();

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

            if (state.contains("buttonevent"))
            {
                if (type != "CLIPSwitch")
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors"), QString("parameter, buttonevent, not available")));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }

                ResourceItem *item = sensor.addItem(DataTypeInt32, RStateButtonEvent);

                if (!item || !item->setValue(state["buttonevent"]))
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors/state"), QString("invalid value, %1, for parameter buttonevent").arg(state["buttonevent"].toString())));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }                
            }
            if (state.contains("flag"))
            {
                if (type != "CLIPGenericFlag")
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors"), QString("parameter, flag, not available")));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }

                ResourceItem *item = sensor.addItem(DataTypeBool, RStateFlag);

                if (!item || !item->setValue(state["flag"]))
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors/state"), QString("invalid value, %1, for parameter flag").arg(state["flag"].toString())));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }
            }
            if (state.contains("status"))
            {
                if (type != "CLIPGenericStatus")
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors"), QString("parameter, status, not available")));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }

                ResourceItem *item = sensor.addItem(DataTypeInt32, RStateStatus);

                if (!item || !item->setValue(state["status"]))
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors/state"), QString("invalid value, %1, for parameter status").arg(state["status"].toString())));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }
            }
            if (state.contains("presence"))
            {
                if (type != "CLIPPresence")
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors"), QString("parameter, presence, not available")));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }

                ResourceItem *item = sensor.addItem(DataTypeBool, RStatePresence);

                if (!item || !item->setValue(state["presence"]))
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors/state"), QString("invalid value, %1, for parameter presence").arg(state["presence"].toString())));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }
            }
            if (state.contains("open"))
            {
                if (type != "CLIPOpenClose")
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors"), QString("parameter, open, not available")));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }

                ResourceItem *item = sensor.addItem(DataTypeBool, RStateOpen);

                if (!item || !item->setValue(state["open"]))
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors/state"), QString("invalid value, %1, for parameter open").arg(state["open"].toString())));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }
            }
            if (state.contains("temperature"))
            {
                if (type != "CLIPTemperature")
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors"), QString("parameter, temperature, not available")));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }

                ResourceItem *item = sensor.addItem(DataTypeInt32, RStateTemperature);

                if (!item || !item->setValue(state["temperature"]))
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors/state"), QString("invalid value, %1, for parameter temperature").arg(state["temperature"].toString())));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }
            }
            if (state.contains("humidity"))
            {
                if (type != "CLIPHumidity")
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors"), QString("parameter, humidity, not available")));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }

                ResourceItem *item = sensor.addItem(DataTypeInt32, RStateHumidity);

                if (!item || !item->setValue(state["humidity"]))
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors/state"), QString("invalid value, %1, for parameter humidity").arg(state["humidity"].toString())));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }
            }
        }

        //setConfig optional
        if (map.contains("config"))
        {
            QVariantMap config = map["config"].toMap();

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

            if (config.contains("on"))
            {
                ResourceItem *item = sensor.addItem(DataTypeBool, RConfigOn);
                item->setValue(config["on"]);
            }
            if (config.contains("reachable"))
            {
                ResourceItem *item = sensor.addItem(DataTypeBool, RConfigReachable);
                item->setValue(config["reachable"]);
            }
            if (config.contains("url"))
            {
                ResourceItem *item = sensor.addItem(DataTypeString, RConfigUrl);
                item->setValue(config["url"]);
            }
            if (config.contains("battery"))
            {
                ResourceItem *item = sensor.addItem(DataTypeUInt8, RConfigBattery);

                if (!item || !item->setValue(config["battery"]))
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors/config"), QString("invalid value, %1, for parameter battery").arg(config["battery"].toString())));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }
            }
        }
        updateSensorEtag(&sensor);
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
                sensor->setNeedSaveDatabase(true);
                queSaveDb(DB_SENSORS, DB_SHORT_SAVE_DELAY);
                updateSensorEtag(sensor);
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
                queSaveDb(DB_SENSORS, DB_SHORT_SAVE_DELAY);
                updateSensorEtag(sensor);
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
    bool ok;
    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();
    QVariantMap rspItem;
    QVariantMap rspItemState;

//    QRegExp latitude("^\\d{3,3}\\.\\d{4,4}(W|E)$");
//    QRegExp longitude("^\\d{3,3}\\.\\d{4,4}(N|S)$");

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

    userActivity();

    //check invalid parameter
    QVariantMap::const_iterator pi = map.begin();
    QVariantMap::const_iterator pend = map.end();

    for (; pi != pend; ++pi)
    {
        ResourceItemDescriptor rid;
        if (getResourceItemDescriptor(QString("config/%1").arg(pi.key()), rid))
        {
            ResourceItem *item = sensor->item(rid.suffix);
            if (!item)
            {
                break; // not found
            }

            QVariant val = map[pi.key()];
            if (item->setValue(val))
            {
                rspItemState[QString("/sensors/%1/config/%2").arg(id).arg(pi.key())] = val;
                rspItem["success"] = rspItemState;

                Event e(RSensors, rid.suffix, id);
                enqueueEvent(e);
            }
            else // invalid
            {
                rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors/%1/config/%2").arg(id).arg(pi.key()),
                                                              QString("invalid value, %1, for parameter %2").arg(val.toString()).arg(pi.key())));
                rsp.httpStatus = HttpStatusBadRequest;
                return REQ_READY_SEND;
            }
        }

        // not found
        rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/sensors/%1/config/%2").arg(id).arg(pi.key()), QString("parameter, %1, not available").arg(pi.key())));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }


    // TODO handle this in event, this is relevant for FLS-NB.

//    if (config.duration() != duration)
//    {
//        config.setDuration(duration);
//        DBG_Printf(DBG_INFO, "Force read/write of occupaction delay for sensor %s\n", qPrintable(sensor->address().toStringExt()));
//        sensor->enableRead(WRITE_OCCUPANCY_CONFIG);
//        sensor->setNextReadTime(WRITE_OCCUPANCY_CONFIG, QTime::currentTime());
//        Q_Q(DeRestPlugin);
//        q->startZclAttributeTimer(0);
//    }

    sensor->setNeedSaveDatabase(true);
    rsp.list.append(rspItem);
    updateSensorEtag(sensor);

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
    bool ok;
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

    bool isClip = sensor->type().startsWith(QLatin1String("CLIP"));

    userActivity();

    //check invalid parameter
    QVariantMap::const_iterator pi = map.begin();
    QVariantMap::const_iterator pend = map.end();

    for (; pi != pend; ++pi)
    {
        ResourceItemDescriptor rid;
        if (isClip && getResourceItemDescriptor(QString("state/%1").arg(pi.key()), rid))
        {
            ResourceItem *item = sensor->item(rid.suffix);
            if (!item)
            {
                break; // not found
            }

            QVariant val = map[pi.key()];
            if (item->setValue(val))
            {
                rspItemState[QString("/sensors/%1/state/%2").arg(id).arg(pi.key())] = val;
                rspItem["success"] = rspItemState;

                Event e(RSensors, rid.suffix, id);
                enqueueEvent(e);
            }
            else // invalid
            {
                rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors/%1/state/%2").arg(id).arg(pi.key()),
                                                              QString("invalid value, %1, for parameter %2").arg(val.toString()).arg(pi.key())));
                rsp.httpStatus = HttpStatusBadRequest;
                return REQ_READY_SEND;
            }
        }

        // not found
        rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/sensors/%1/state/%2").arg(id).arg(pi.key()), QString("parameter, %1, not available").arg(pi.key())));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    sensor->setNeedSaveDatabase(true);
    rsp.list.append(rspItem);
    updateSensorEtag(sensor);
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

    if (!findSensorResult.isEmpty() &&
        (findSensorsState == FindSensorsActive || findSensorsState == FindSensorsDone))
    {

        rsp.map = findSensorResult;
    }

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

    for (int i = 0; i < sensor->itemCount(); i++)
    {
        const ResourceItem *item = sensor->itemForIndex(i);
        const ResourceItemDescriptor &rid = item->descriptor();

        if (!item->lastSet().isValid())
        {
            continue;
        }

        if (strncmp(rid.suffix, "config/", 7) == 0)
        {
            const char *key = item->descriptor().suffix + 7;
            config[key] = item->toVariant();
        }
    }

    QDateTime lastUpdated;

    for (int i = 0; i < sensor->itemCount(); i++)
    {
        const ResourceItem *item = sensor->itemForIndex(i);
        const ResourceItemDescriptor &rid = item->descriptor();

        if (!item->lastSet().isValid())
        {
            continue;
        }

        if (strncmp(rid.suffix, "state/", 6) == 0)
        {
            const char *key = item->descriptor().suffix + 6;
            state[key] = item->toVariant();

            if (item->lastSet().isValid() &&
                (!lastUpdated.isValid() || lastUpdated < item->lastSet()))
            {
                lastUpdated = item->lastSet();
            }
        }
    }

    //state
    if (lastUpdated.isValid())
    {
        state["lastupdated"] = lastUpdated.toString("yyyy-MM-ddTHH:mm:ss");
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
        findSensorResult.clear();
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
//    if (findSensorsState != FindSensorsActive)
//    {
//        return;
//    }

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

//        const quint64 philipsMacPrefix = 0x0017880000000000ULL;

//        if ((ext & philipsMacPrefix) == philipsMacPrefix)
//        {
//            //fastProbePhilips(ext, nwk, macCapabilities);
//            //return;
//        }

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

        fastProbeAddr.setExt(ext);
        fastProbeAddr.setNwk(nwk);
        if (!fastProbeTimer->isActive())
        {
            fastProbeTimer->start(1000);
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
    else if (ind.profileId() == ZDP_PROFILE_ID)
    {

        std::vector<SensorCandidate>::const_iterator i = findSensorCandidates.begin();
        std::vector<SensorCandidate>::const_iterator end = findSensorCandidates.end();

        for (; i != end; ++i)
        {
            if (i->address.ext() == fastProbeAddr.ext())
            {
                if (!fastProbeTimer->isActive())
                {
                    fastProbeTimer->start(100);
                }
            }
        }
        return;
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
            sensorNode.setUniqueId(generateUniqueId(sensorNode.address().ext(), sensorNode.fingerPrint().endpoint, COMMISSIONING_CLUSTER_ID));
            sensorNode.setManufacturer(QLatin1String("dresden elektronik"));

            ResourceItem *item;
            item = sensorNode.addItem(DataTypeBool, RConfigOn);
            item->setValue(true);

            item = sensorNode.addItem(DataTypeBool, RConfigReachable);
            item->setValue(true);

            sensorNode.addItem(DataTypeInt32, RStateButtonEvent);

            sensorNode.setNeedSaveDatabase(true);
            updateSensorEtag(&sensorNode);

            bool update = false;

            if (!s1 && isSceneSwitch && findSensorsState == FindSensorsActive)
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
                Event e(RSensors, REventAdded, sensorNode.id());
                enqueueEvent(e);
            }
            else if (isLightingSwitch)
            {
                if (!s1 && findSensorsState == FindSensorsActive)
                {
                    openDb();
                    sensorNode.setId(QString::number(getFreeSensorId()));
                    closeDb();
                    sensorNode.setMode(Sensor::ModeTwoGroups);
                    sensorNode.setModelId(QLatin1String("Lighting Switch"));
                    sensorNode.setName(QString("Lighting Switch %1").arg(sensorNode.id()));
                    sensorNode.setNeedSaveDatabase(true);
                    sensors.push_back(sensorNode);
                    s1 = &sensors.back();
                    update = true;
                    Event e(RSensors, REventAdded, sensorNode.id());
                    enqueueEvent(e);
                }

                if (!s2 && findSensorsState == FindSensorsActive)
                {
                    openDb();
                    sensorNode.setId(QString::number(getFreeSensorId()));
                    closeDb();
                    sensorNode.setMode(Sensor::ModeTwoGroups);
                    sensorNode.setName(QString("Lighting Switch %1").arg(sensorNode.id()));
                    sensorNode.setNeedSaveDatabase(true);
                    sensorNode.fingerPrint().endpoint = 0x02;
                    sensorNode.setUniqueId(generateUniqueId(sensorNode.address().ext(), sensorNode.fingerPrint().endpoint, COMMISSIONING_CLUSTER_ID));
                    sensors.push_back(sensorNode);
                    s2 = &sensors.back();
                    update = true;
                    Event e(RSensors, REventAdded, sensorNode.id());
                    enqueueEvent(e);
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
                updateGroupEtag(&group);
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
                    updateGroupEtag(g);
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
                updateGroupEtag(&group);
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
                    updateGroupEtag(g);
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
