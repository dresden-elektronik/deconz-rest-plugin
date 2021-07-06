/*
 * Copyright (c) 2013-2019 dresden elektronik ingenieurtechnik gmbh.
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
#include <QUrlQuery>
#include <QVariantMap>
#include <QtCore/qmath.h>
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "json.h"
#include "product_match.h"
#include "fan_control.h"
#include "simple_metering.h"
#include "thermostat.h"
#include "thermostat_ui_configuration.h"
#include "utils/utils.h"

/*! Sensors REST API broker.
    \param req - request data
    \param rsp - response data
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::handleSensorsApi(const ApiRequest &req, ApiResponse &rsp)
{
    if (req.path[2] != QLatin1String("sensors"))
    {
        return REQ_NOT_HANDLED;
    }

    // GET /api/<apikey>/sensors
    if ((req.path.size() == 3) && (req.hdr.method() == QLatin1String("GET")))
    {
        return getAllSensors(req, rsp);
    }
    // GET /api/<apikey>/sensors/new
    else if ((req.path.size() == 4) && (req.hdr.method() == QLatin1String("GET")) && (req.path[3] == QLatin1String("new")))
    {
        return getNewSensors(req, rsp);
    }
    // GET /api/<apikey>/sensors/<id>
    else if ((req.path.size() == 4) && (req.hdr.method() == QLatin1String("GET")))
    {
        return getSensor(req, rsp);
    }
    // GET /api/<apikey>/sensors/<id>/data?maxrecords=<maxrecords>&fromtime=<ISO 8601>
    else if ((req.path.size() == 5) && (req.hdr.method() == QLatin1String("GET")) && (req.path[4] == QLatin1String("data")))
    {
        return getSensorData(req, rsp);
    }
    // POST /api/<apikey>/sensors
    else if ((req.path.size() == 3) && (req.hdr.method() == QLatin1String("POST")))
    {
        bool ok;
        QVariant var = Json::parse(req.content, ok);
        QVariantMap map = var.toMap();

        if (map.isEmpty())
        {
            return searchNewSensors(req, rsp);
        }
        else
        {
            return createSensor(req, rsp);
        }
    }
    // PUT, PATCH /api/<apikey>/sensors/<id>
    else if ((req.path.size() == 4) && (req.hdr.method() == QLatin1String("PUT") || req.hdr.method() == QLatin1String("PATCH")))
    {
        return updateSensor(req, rsp);
    }
    // DELETE /api/<apikey>/sensors/<id>
    else if ((req.path.size() == 4) && (req.hdr.method() == QLatin1String("DELETE")))
    {
        return deleteSensor(req, rsp);
    }
    // PUT, PATCH /api/<apikey>/sensors/<id>/config
    else if ((req.path.size() == 5) && (req.hdr.method() == QLatin1String("PUT") || req.hdr.method() == QLatin1String("PATCH")) && (req.path[4] == QLatin1String("config")))
    {
        return changeSensorConfig(req, rsp);
    }
    // PUT, PATCH /api/<apikey>/sensors/<id>/state
    else if ((req.path.size() == 5) && (req.hdr.method() == QLatin1String("PUT") || req.hdr.method() == QLatin1String("PATCH")) && (req.path[4] == QLatin1String("state")))
    {
        return changeSensorState(req, rsp);
    }
    // POST, DELETE /api/<apikey>/sensors/<id>/config/schedule/Wbbb
    else if ((req.path.size() == 7) && (req.hdr.method() == QLatin1String("POST") || req.hdr.method() == QLatin1String("DELETE")) && (req.path[4] == QLatin1String("config")) && (req.path[5] == QLatin1String("schedule")))
    {
        return changeThermostatSchedule(req, rsp);
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
    if (req.hdr.hasKey(QLatin1String("If-None-Match")))
    {
        QString etag = req.hdr.value(QLatin1String("If-None-Match"));

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

        // ignore sensors without attached node
        if (i->modelId().startsWith(QLatin1String("FLS-NB")) && !i->node())
        {
            continue;
        }

        if (i->modelId().isEmpty())
        {
            continue;
        }

        QVariantMap map;
        if (sensorToMap(&*i, map, req))
        {
            rsp.map[i->id()] = map;
        }
    }

    if (rsp.map.isEmpty())
    {
        rsp.str = QLatin1String("{}"); // return empty object
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

    Sensor *sensor = id.length() < MIN_UNIQUEID_LENGTH ? getSensorNodeForId(id) : getSensorNodeForUniqueId(id);

    if (!sensor || (sensor->deletedState() == Sensor::StateDeleted))
    {
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/sensors/%1").arg(id), QString("resource, /sensors/%1, not available").arg(id)));
        rsp.httpStatus = HttpStatusNotFound;
        return REQ_READY_SEND;
    }

    // handle ETag
    if (req.hdr.hasKey(QLatin1String("If-None-Match")))
    {
        QString etag = req.hdr.value(QLatin1String("If-None-Match"));

        if (sensor->etag == etag)
        {
            rsp.httpStatus = HttpStatusNotModified;
            rsp.etag = etag;
            return REQ_READY_SEND;
        }
    }

    sensorToMap(sensor, rsp.map, req);
    rsp.httpStatus = HttpStatusOk;
    rsp.etag = sensor->etag;

    return REQ_READY_SEND;
}

/*! GET /api/<apikey>/sensors/<id>/data?maxrecords=<maxrecords>&fromtime=<ISO 8601>
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::getSensorData(const ApiRequest &req, ApiResponse &rsp)
{
    DBG_Assert(req.path.size() == 5);

    if (req.path.size() != 5)
    {
        return REQ_NOT_HANDLED;
    }

    QString id = req.path[3];
    Sensor *sensor = id.length() < MIN_UNIQUEID_LENGTH ? getSensorNodeForId(id) : getSensorNodeForUniqueId(id);

    if (!sensor || (sensor->deletedState() == Sensor::StateDeleted))
    {
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/sensors/%1/").arg(id), QString("resource, /sensors/%1/, not available").arg(id)));
        rsp.httpStatus = HttpStatusNotFound;
        return REQ_READY_SEND;
    }

    bool ok;
    QUrl url(req.hdr.url());
    QUrlQuery query(url);

    const int maxRecords = query.queryItemValue(QLatin1String("maxrecords")).toInt(&ok);
    if (!ok || maxRecords <= 0)
    {
        rsp.list.append(errorToMap(ERR_INVALID_VALUE, QLatin1String("/maxrecords"), QString("invalid value, %1, for parameter, maxrecords").arg(query.queryItemValue("maxrecords"))));
        rsp.httpStatus = HttpStatusNotFound;
        return REQ_READY_SEND;
    }

    QString t = query.queryItemValue(QLatin1String("fromtime"));
    QDateTime dt = QDateTime::fromString(t, QLatin1String("yyyy-MM-ddTHH:mm:ss"));
    if (!dt.isValid())
    {
        rsp.list.append(errorToMap(ERR_INVALID_VALUE, QLatin1String("/fromtime"), QString("invalid value, %1, for parameter, fromtime").arg(query.queryItemValue("fromtime"))));
        rsp.httpStatus = HttpStatusNotFound;
        return REQ_READY_SEND;
    }

    const qint64 fromTime = dt.toMSecsSinceEpoch() / 1000;

    openDb();
    loadSensorDataFromDb(sensor, rsp.list, fromTime, maxRecords);
    closeDb();

    if (rsp.list.isEmpty())
    {
        rsp.str = QLatin1String("[]"); // return empty list
    }

    rsp.httpStatus = HttpStatusOk;

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
    const QVariantMap map = var.toMap();
    const QString type = map[QLatin1String("type")].toString();
    Sensor sensor;

    if (!ok)
    {
        rsp.list.append(errorToMap(ERR_INVALID_JSON, QLatin1String("/sensors"), QLatin1String("body contains invalid JSON")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    userActivity();

    if (sensors.size() >= MAX_SENSORS)
    {
        rsp.list.append(errorToMap(ERR_SENSOR_LIST_FULL, QLatin1String("/sensors/"), QString("The Sensor List has reached its maximum capacity of %1 sensors").arg(MAX_SENSORS)));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    //check required parameter
    if (!map.contains(QLatin1String("name")) || !map.contains(QLatin1String("modelid")) || !map.contains(QLatin1String("swversion")) || !map.contains(QLatin1String("type")) || !map.contains(QLatin1String("uniqueid")) || !map.contains(QLatin1String("manufacturername")))
    {
        rsp.list.append(errorToMap(ERR_MISSING_PARAMETER, QLatin1String("/sensors"), QLatin1String("invalid/missing parameters in body")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    //check invalid parameter
    const QStringList allowedAttributes = { "name", "modelid", "swversion", "type", "uniqueid", "manufacturername", "state", "config", "recycle" };

    for (const QString &attr : map.keys())
    {
        if (!allowedAttributes.contains(attr))
        {
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/sensors/%2").arg(attr), QString("parameter, %1, not available").arg(attr)));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }

    if (!type.startsWith(QLatin1String("CLIP")))
    {
        rsp.list.append(errorToMap(ERR_NOT_ALLOWED_SENSOR_TYPE, QLatin1String("/sensors"), QLatin1String("Not allowed to create sensor type")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

        ResourceItem *item = nullptr;
        QVariantMap rspItem;
        QVariantMap rspItemState;

        // create a new sensor id
        openDb();
        sensor.setId(QString::number(getFreeSensorId()));
        closeDb();

        sensor.setName(map[QLatin1String("name")].toString().trimmed());
        sensor.setManufacturer(map[QLatin1String("manufacturername")].toString());
        sensor.setModelId(map[QLatin1String("modelid")].toString());
        sensor.setUniqueId(map[QLatin1String("uniqueid")].toString());
        sensor.setSwVersion(map[QLatin1String("swversion")].toString());
        sensor.setType(type);

        if (getSensorNodeForUniqueId(sensor.uniqueId()))
        {
            rsp.list.append(errorToMap(ERR_DUPLICATE_EXIST, QLatin1String("/sensors"), QString("sensor with uniqueid, %1, already exists").arg(sensor.uniqueId())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

        if      (type == QLatin1String("CLIPAlarm")) { item = sensor.addItem(DataTypeBool, RStateAlarm); item->setValue(false); }
        else if (type == QLatin1String("CLIPBattery")) { item = sensor.addItem(DataTypeUInt8, RStateBattery); item->setValue(100); }
        else if (type == QLatin1String("CLIPCarbonMonoxide")) { item = sensor.addItem(DataTypeBool, RStateCarbonMonoxide); item->setValue(false); }
        else if (type == QLatin1String("CLIPConsumption")) { item = sensor.addItem(DataTypeUInt64, RStateConsumption); item->setValue(0); }
        else if (type == QLatin1String("CLIPDaylightOffset")) { item = sensor.addItem(DataTypeInt16, RConfigOffset); item->setValue(0);
                                                                item = sensor.addItem(DataTypeString, RConfigMode);
                                                                item = sensor.addItem(DataTypeTime, RStateLocaltime); }
        else if (type == QLatin1String("CLIPFire")) { item = sensor.addItem(DataTypeBool, RStateFire); item->setValue(false); }
        else if (type == QLatin1String("CLIPGenericFlag")) { item = sensor.addItem(DataTypeBool, RStateFlag); item->setValue(false); }
        else if (type == QLatin1String("CLIPGenericStatus")) { item = sensor.addItem(DataTypeInt32, RStateStatus); item->setValue(0); }
        else if (type == QLatin1String("CLIPHumidity")) { item = sensor.addItem(DataTypeUInt16, RStateHumidity); item->setValue(0);
                                                          item = sensor.addItem(DataTypeInt16, RConfigOffset); item->setValue(0); }
        else if (type == QLatin1String("CLIPLightLevel")) { item = sensor.addItem(DataTypeUInt16, RStateLightLevel); item->setValue(0);
                                                            item = sensor.addItem(DataTypeUInt32, RStateLux); item->setValue(0);
                                                            item = sensor.addItem(DataTypeBool, RStateDark); item->setValue(true);
                                                            item = sensor.addItem(DataTypeBool, RStateDaylight); item->setValue(false);
                                                            item = sensor.addItem(DataTypeUInt16, RConfigTholdDark); item->setValue(R_THOLDDARK_DEFAULT);
                                                            item = sensor.addItem(DataTypeUInt16, RConfigTholdOffset); item->setValue(R_THOLDOFFSET_DEFAULT); }
        else if (type == QLatin1String("CLIPOpenClose")) { item = sensor.addItem(DataTypeBool, RStateOpen); item->setValue(false); }
        else if (type == QLatin1String("CLIPPower")) { item = sensor.addItem(DataTypeInt16, RStatePower); item->setValue(0);
                                                       item = sensor.addItem(DataTypeUInt16, RStateVoltage); item->setValue(0);
                                                       item = sensor.addItem(DataTypeUInt16, RStateCurrent); item->setValue(0); }
        else if (type == QLatin1String("CLIPPresence")) { item = sensor.addItem(DataTypeBool, RStatePresence); item->setValue(false);
                                                          item = sensor.addItem(DataTypeUInt16, RConfigDuration); item->setValue(60); }
        else if (type == QLatin1String("CLIPPressure")) { item = sensor.addItem(DataTypeInt16, RStatePressure); item->setValue(0); }
        else if (type == QLatin1String("CLIPSwitch")) { item = sensor.addItem(DataTypeInt32, RStateButtonEvent); item->setValue(0); }
        else if (type == QLatin1String("CLIPTemperature")) { item = sensor.addItem(DataTypeInt16, RStateTemperature); item->setValue(0);
                                                             item = sensor.addItem(DataTypeInt16, RConfigOffset); item->setValue(0); }
        else if (type == QLatin1String("CLIPVibration")) { item = sensor.addItem(DataTypeBool, RStateVibration); item->setValue(false); }
        else if (type == QLatin1String("CLIPWater")) { item = sensor.addItem(DataTypeBool, RStateWater); item->setValue(false); }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QLatin1String("/sensors"), QString("invalid value, %1, for parameter, type").arg(type)));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

        //setState optional
        if (map.contains(QLatin1String("state")))
        {
            //check invalid parameter
            const QVariantMap state = map[QLatin1String("state")].toMap();
            const QStringList allowedKeys = { "alarm", "battery", "buttonevent", "carbonmonoxide", "consumption", "current", "fire", "flag", "humidity", "lightlevel", "localtime", "lowbattery",
                                              "open", "presence", "pressure", "power", "status", "tampered", "temperature", "vibration", "voltage", "water" };

            const QStringList optionalKeys = { "lowbattery", "tampered" };

            for  (const auto &key : state.keys())
            {
                if (!allowedKeys.contains(key))
                {
                    rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/sensors/%2").arg(key), QString("parameter, %1, not available").arg(key)));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }

                ResourceItemDescriptor rid;
                item = nullptr;
                if (getResourceItemDescriptor(QString("state/%1").arg(key), rid))
                {
                    item = sensor.item(rid.suffix);

                    if (!item && optionalKeys.contains(key))
                    {
                        item = sensor.addItem(rid.type, rid.suffix);
                    }
                }

                if (!item)
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QLatin1String("/sensors"), QString("parameter, %1, not available").arg(key)));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }

                if (!item->setValue(state.value(key)))
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QLatin1String("/sensors/state"), QString("invalid value, %1, for parameter %2").arg(state.value(key).toString()).arg(key)));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }
            }
        }

        item = sensor.item(RConfigOn);
        item->setValue(true); // default

        item = sensor.item(RConfigReachable);
        item->setValue(true); //default

        //setConfig optional
        if (map.contains(QLatin1String("config")))
        {
            //check invalid parameter
            const QVariantMap config = map[QLatin1String("config")].toMap();
            const QStringList allowedKeys = { "battery", "duration", "delay", "mode", "offset", "on", "reachable", "url" };
            const QStringList optionalKeys = { "battery", "url" };

            for  (const auto &key : config.keys())
            {
                if (!allowedKeys.contains(key))
                {
                    rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/sensors/%2").arg(key), QString("parameter, %1, not available").arg(key)));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }

                ResourceItemDescriptor rid;
                item = nullptr;
                if (getResourceItemDescriptor(QString("config/%1").arg(key), rid))
                {
                    item = sensor.item(rid.suffix);

                    if (!item && optionalKeys.contains(key))
                    {
                        item = sensor.addItem(rid.type, rid.suffix);
                    }
                }

                if (!item)
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QLatin1String("/sensors"), QString("parameter, %1, not available").arg(key)));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }

                if (!item->setValue(config.value(key)))
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QLatin1String("/sensors/config"), QString("invalid value, %1, for parameter %2").arg(config.value(key).toString()).arg(key)));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }
            }
        }
        updateSensorEtag(&sensor);
        sensor.setNeedSaveDatabase(true);
        sensors.push_back(sensor);
        queSaveDb(DB_SENSORS, DB_SHORT_SAVE_DELAY);

        rspItemState[QLatin1String("id")] = sensor.id();
        rspItem[QLatin1String("success")] = rspItemState;
        rsp.list.append(rspItem);
        rsp.httpStatus = HttpStatusOk;
        return REQ_READY_SEND;
}

/*! PUT, PATCH /api/<apikey>/sensors/<id>
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::updateSensor(const ApiRequest &req, ApiResponse &rsp)
{
    const QLatin1String id = req.hdr.pathAt(3);
    Sensor *sensor = id.size() < MIN_UNIQUEID_LENGTH ? getSensorNodeForId(id) : getSensorNodeForUniqueId(id);
    bool ok;
    const QVariantMap map = Json::parse(req.content, ok).toMap();

    rsp.httpStatus = HttpStatusOk;

    if (!ok)
    {
        rsp.list.append(errorToMap(ERR_INVALID_JSON, QLatin1String("/sensors"), QLatin1String("body contains invalid JSON")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    if (!sensor || (sensor->deletedState() == Sensor::StateDeleted))
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/sensors/%1").arg(id), QString("resource, /sensors/%1, not available").arg(id)));
        return REQ_READY_SEND;
    }

    if (req.sock)
    {
        userActivity();
    }

    {   //check invalid parameter
        auto pi = map.cbegin();
        const auto pend = map.cend();

        const std::array<QLatin1String, 3> modifiableAttributes { QLatin1String("name"), QLatin1String("mode"), QLatin1String("config") };

        for (; pi != pend; ++pi)
        {
            const auto i = std::find_if(modifiableAttributes.cbegin(), modifiableAttributes.cend(), [&pi](const auto attr){ return pi.key() == attr; });

            if (i == modifiableAttributes.cend())
            {
                ResourceItemDescriptor rid;

                if (getResourceItemDescriptor(QString("attr/%1").arg(pi.key()), rid))
                {
                    rsp.list.append(errorToMap(ERR_PARAMETER_NOT_MODIFIABLE, QString("/sensors/%1/%2").arg(id, pi.key()), QString("parameter, %1, not modifiable").arg(pi.key())));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }

                rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/sensors/%1/%2").arg(id, pi.key()), QString("parameter, %1, not available").arg(pi.key())));
                rsp.httpStatus = HttpStatusBadRequest;
                return REQ_READY_SEND;
            }

            // TODO further checks and clarification.
            // 'mode' is only available for ZHASwitch sensors and refers to 'attr/mode' (but there is no ResourceItem for it).
            // Consider moving this to 'config/mode' or 'config/devicemode'?
            if (pi.key() == QLatin1String("mode") && !sensor->modelId().startsWith(QLatin1String("SYMFONISK")))
            {
                rsp.list.append(errorToMap(ERR_PARAMETER_NOT_MODIFIABLE, QString("/sensors/%1/%2").arg(id, pi.key()), QString("parameter, %1, not modifiable").arg(pi.key())));
                rsp.httpStatus = HttpStatusBadRequest;
                return REQ_READY_SEND;
            }
        }
    }

    QVariantMap rspItem;
    QVariantMap rspItemState;

    if (map.contains(QLatin1String("name"))) // optional
    {
        const QString name = map[QLatin1String("name")].toString().trimmed();

        if (map[QLatin1String("name")].type() == QVariant::String && !name.isEmpty() && name.size() <= MAX_SENSOR_NAME_LENGTH)
        {
            if (sensor->name() != name)
            {
                sensor->setName(name);
                sensor->setNeedSaveDatabase(true);
                queSaveDb(DB_SENSORS, DB_SHORT_SAVE_DELAY);
                updateSensorEtag(sensor);

                enqueueEvent(Event(RSensors, RAttrName, sensor->id(), sensor->item(RAttrName)));
            }
            if (!sensor->type().startsWith(QLatin1String("CLIP")))
            {
                pushSensorInfoToCore(sensor);
            }
            rspItemState[QString("/sensors/%1/name").arg(id)] = name;
            rspItem[QLatin1String("success")] = rspItemState;
            rsp.list.append(rspItem);
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors/%1/name").arg(id), QString("invalid value, %1, for parameter, /sensors/%2/name").arg(name).arg(id)));
            rsp.httpStatus = HttpStatusBadRequest;
        }
    }

    if (map.contains(QLatin1String("mode"))) // optional
    {
        Sensor::SensorMode mode = (Sensor::SensorMode)map[QLatin1String("mode")].toUInt(&ok);

        if (ok && map[QLatin1String("mode")].type() == QVariant::Double
            && sensor->modelId().startsWith(QLatin1String("SYMFONISK")) && (mode == Sensor::ModeScenes || mode == Sensor::ModeDimmer))
        {
            if (sensor->mode() != mode)
            {
                sensor->setNeedSaveDatabase(true);
                sensor->setMode(mode);
                queSaveDb(DB_SENSORS, DB_SHORT_SAVE_DELAY);
                updateSensorEtag(sensor);
            }

            rspItemState[QString("/sensors/%1/mode").arg(id)] = (double)mode;
            rspItem[QLatin1String("success")] = rspItemState;
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

    // TODO this appears to be a hack, check with REST API clients if this is used at all and consider removal
    if (map.contains(QLatin1String("config"))) // optional
    {
        QStringList path = req.path;
        path.append(QLatin1String("config"));
        QString content = Json::serialize(map[QLatin1String("config")].toMap());
        ApiRequest req2(req.hdr, path, NULL, content);
        return changeSensorConfig(req2, rsp);
    }

    return REQ_READY_SEND;
}

/*! PUT, PATCH /api/<apikey>/sensors/<id>/config
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::changeSensorConfig(const ApiRequest &req, ApiResponse &rsp)
{
    TaskItem task;
    QString id = req.path[3];
    Sensor *sensor = id.length() < MIN_UNIQUEID_LENGTH ? getSensorNodeForId(id) : getSensorNodeForUniqueId(id);
    bool ok;
    bool updated;
    bool save = false;
    quint32 hostFlags = 0;
    bool offsetUpdated = false;
    qint16 offset = 0;
    QMap<quint16, quint32> attributeList;
    bool tholdUpdated = false;
    quint16 pendingMask = 0;
    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();

//    QRegExp latitude("^\\d{3,3}\\.\\d{4,4}(W|E)$");
//    QRegExp longitude("^\\d{3,3}\\.\\d{4,4}(N|S)$");

    rsp.httpStatus = HttpStatusOk;

    if (!ok)
    {
        rsp.list.append(errorToMap(ERR_INVALID_JSON, QLatin1String("/sensors/config"), QLatin1String("body contains invalid JSON")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    if (!sensor || (sensor->deletedState() == Sensor::StateDeleted))
    {
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/sensors/%1").arg(id), QString("resource, /sensors/%1, not available").arg(id)));
        rsp.httpStatus = HttpStatusNotFound;
        return REQ_READY_SEND;
    }

    bool isClip = sensor->type().startsWith(QLatin1String("CLIP"));

    if (req.sock)
    {
        userActivity();
    }

    // set destination parameters
    task.req.dstAddress() = sensor->address();
    task.req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
    task.req.setDstEndpoint(sensor->fingerPrint().endpoint);
    task.req.setSrcEndpoint(getSrcEndpoint(sensor, task.req));
    task.req.setDstAddressMode(deCONZ::ApsExtAddress);

    //check invalid parameter
    auto pi = map.cbegin();
    const auto pend = map.cend();

    for (; pi != pend; ++pi)
    {
        ResourceItemDescriptor rid;
        ResourceItem *item = nullptr;
        if (getResourceItemDescriptor(QString("config/%1").arg(pi.key()), rid))
        {
            updated = false;

            // Changing these values of zigbee sensors is not allowed, read-only.
            if (rid.suffix == RConfigPending || rid.suffix == RConfigSensitivityMax || rid.suffix == RConfigHostFlags || rid.suffix == RConfigLastChangeAmount ||
                rid.suffix == RConfigLastChangeSource || rid.suffix == RConfigLastChangeTime || rid.suffix == RConfigEnrolled ||
                (!isClip && (rid.suffix == RConfigBattery || rid.suffix == RConfigReachable)))
            {
                rsp.list.append(errorToMap(ERR_PARAMETER_NOT_MODIFIABLE, QString("/sensors/%1/config/%2").arg(id).arg(pi.key()),
                                           QString("parameter, %1, not modifiable").arg(pi.key())));
                continue;
            }
            //else if (rid.suffix == RConfigDuration && sensor->modelId() == QLatin1String("TRADFRI motion sensor"))
            //{
                // duration can be written for ikea motion sensor
                // values 0, 60 — 600 will be replaced by hardware settings TODO error message
            //}
            else
            {
                item = sensor->item(rid.suffix);
            }

            if (item)
            {
                QVariant val = map[pi.key()];
                RestData data = verifyRestData(rid, val);

                if (!data.valid)
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors/%1/config/%2").arg(id).arg(pi.key()),
                                               QString("invalid value, %1, for parameter %2").arg(map[pi.key()].toString()).arg(pi.key())));
                    continue;
                }

                if (sensor->modelId().startsWith(QLatin1String("SPZB")) && hostFlags == 0) // Eurotronic Spirit
                {
                    ResourceItem *item = sensor->item(RConfigHostFlags);
                    if (item)
                    {
                        hostFlags = item->toNumber();
                    }
                    else
                    {
                        rsp.list.append(errorToMap(ERR_ACTION_ERROR, QString("/sensors/%1/config/%2").arg(id).arg(pi.key()),
                                                   QLatin1String("Could not set attribute")));
                        continue;
                    }
                }

                if (rid.suffix == RConfigDeviceMode) // String
                {
                    if (RConfigDeviceModeValues.indexOf(data.string) >= 0)
                    {
                        pendingMask |= R_PENDING_DEVICEMODE;
                        sensor->enableRead(WRITE_DEVICEMODE);
                        sensor->setNextReadTime(WRITE_DEVICEMODE, QTime::currentTime());
                        updated = true;
                    }
                }
                else if (rid.suffix == RConfigTholdDark || rid.suffix == RConfigTholdOffset) // Unsigned integer
                {
                    tholdUpdated = true;
                    updated = true;
                }
                else if (rid.suffix == RConfigDelay) // Unsigned integer
                {
                    if (sensor->modelId().startsWith(QLatin1String("SML00")) || // Hue motion sensor
                        sensor->modelId().startsWith(QLatin1String("MOSZB-1"))) // Develco/frient motion sensor
                    {
                        pendingMask |= R_PENDING_DELAY;
                        sensor->enableRead(WRITE_DELAY);
                        sensor->setNextReadTime(WRITE_DELAY, QTime::currentTime());
                    }
                    updated = true;
                }
                else if (rid.suffix == RConfigDuration) // Unsigned integer
                {
                    if (sensor->modelId().startsWith(QLatin1String("FLS-NB")))
                    {
                        DBG_Printf(DBG_INFO, "Force read of occupaction delay for sensor %s\n", qPrintable(sensor->address().toStringExt()));
                        sensor->enableRead(READ_OCCUPANCY_CONFIG);
                        sensor->setNextReadTime(READ_OCCUPANCY_CONFIG, queryTime.addSecs(1));
                        queryTime = queryTime.addSecs(1);
                        Q_Q(DeRestPlugin);
                        q->startZclAttributeTimer(0);
                    }
                    else if (sensor->modelId() == QLatin1String("TRADFRI motion sensor"))
                    {
                        if (data.uinteger < 1 || data.uinteger > 60)
                        {
                            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors/%1/config/%2").arg(id).arg(pi.key()),
                                                       QString("invalid value, %1, for parameter %2").arg(map[pi.key()].toString()).arg(pi.key())));
                            continue;
                        }
                    }

                    updated = true;
                }
                else if (rid.suffix == RConfigLedIndication) // Boolean
                {
                    pendingMask |= R_PENDING_LEDINDICATION;
                    sensor->enableRead(WRITE_LEDINDICATION);
                    sensor->setNextReadTime(WRITE_LEDINDICATION, QTime::currentTime());
                    updated = true;
                }
                else if (rid.suffix == RConfigSensitivity) // Unsigned integer
                {
                    pendingMask |= R_PENDING_SENSITIVITY;
                    sensor->enableRead(WRITE_SENSITIVITY);
                    sensor->setNextReadTime(WRITE_SENSITIVITY, QTime::currentTime());
                    updated = true;
                }
                else if (rid.suffix == RConfigUsertest) // Boolean
                {
                    pendingMask |= R_PENDING_USERTEST;
                    sensor->enableRead(WRITE_USERTEST);
                    sensor->setNextReadTime(WRITE_USERTEST, QTime::currentTime());
                    updated = true;
                }
                else if (rid.suffix == RConfigLat || rid.suffix == RConfigLong) // String
                {
                    updated = true;
                }
                else if (rid.suffix == RConfigOn) // Boolean
                {
                    updated = true;
                }
                else if (rid.suffix == RConfigAlert) // String
                {
                    const std::array<KeyValMap, 3> RConfigAlertValues = { { {QLatin1String("none"), 0}, {QLatin1String("select"), 2}, {QLatin1String("lselect"), 15} } };
                    
                    const auto match = matchKeyValue(data.string, RConfigAlertValues);

                    if (isValid(match))
                    {
                        task.taskType = TaskIdentify;
                        taskToLocalData(task);

                        if (addTaskIdentify(task, match.value))
                        {
                            updated = true;
                        }
                    }
                }
                else if (rid.suffix == RConfigLock) // Boolean
                {
                    data.boolean ^= data.boolean;     // Flip bool value as 0 means lock and 1 means unlock

                    if (addTaskDoorLockUnlock(task, data.boolean))
                    {
                        updated = true;
                    }
                }
                else if (rid.suffix == RConfigMelody) // Unsigned integer
                {
                    if (data.uinteger > 18) { data.uinteger = 18; }
                    if (data.uinteger < 1) { data.uinteger = 1; }
                    
                    QByteArray tuyaData;
                    tuyaData.append(static_cast<qint8>(data.uinteger & 0xff));

                    if (sendTuyaRequest(task, TaskTuyaRequest, DP_TYPE_ENUM, DP_IDENTIFIER_MELODY, tuyaData))
                    {
                        updated = true;
                    }
                }
                else if (rid.suffix == RConfigVolume) // Unsigned integer
                {
                    if (data.uinteger > 2) { data.uinteger = 2; } // Volume level, max = 2

                    QByteArray tuyaData;
                    tuyaData.append(static_cast<qint8>(data.uinteger & 0xff));

                    if (sendTuyaRequest(task, TaskTuyaRequest, DP_TYPE_ENUM, DP_IDENTIFIER_VOLUME, tuyaData))
                    {
                        updated = true;
                    }
                }
                else if (rid.suffix == RConfigTempMinThreshold || rid.suffix == RConfigTempMaxThreshold || rid.suffix == RConfigHumiMinThreshold || rid.suffix == RConfigHumiMaxThreshold)
                // Use signed, a temperature can be negative, but untested.
                {
                    QByteArray tuyaData = QByteArray("\x00\x00\x00",3);
                    tuyaData.append(static_cast<qint8>(data.integer));
                    quint8 dpIdentifier = 0;

                    //if      (data.integer <= -25 || data.integer >= 25) { invalidValue = true; }  // What are the valid boundaries?
                    if      (rid.suffix == RConfigTempMinThreshold) { dpIdentifier = DP_IDENTIFIER_TRESHOLDTEMPMINI; }
                    else if (rid.suffix == RConfigTempMaxThreshold) { dpIdentifier = DP_IDENTIFIER_TRESHOLDTEMPMAXI; }
                    else if (rid.suffix == RConfigHumiMinThreshold) { dpIdentifier = DP_IDENTIFIER_TRESHOLDTHUMIMINI; }
                    else if (rid.suffix == RConfigHumiMaxThreshold) { dpIdentifier = DP_IDENTIFIER_TRESHOLDHUMIMAXI; }

                    if (sendTuyaRequest(task, TaskTuyaRequest, DP_TYPE_VALUE, dpIdentifier, tuyaData))
                    {
                        updated = true;
                    }
                }
                else if (rid.suffix == RConfigOffset) // Signed integer
                {
                    data.integer = data.integer / 10;

                    if ((R_GetProductId(sensor) == QLatin1String("Tuya_THD HY369 TRV") ||
                         R_GetProductId(sensor) == QLatin1String("Tuya_THD Essentials TRV") ||
                         R_GetProductId(sensor) == QLatin1String("Tuya_THD Smart radiator TRV") ||
                         R_GetProductId(sensor) == QLatin1String("Tuya_THD NX-4911-675 TRV") ||
                         R_GetProductId(sensor) == QLatin1String("Tuya_THD SEA801-ZIGBEE TRV")) ||
                         R_GetProductId(sensor) == QLatin1String("Tuya_THD WZB-TRVL TRV"))
                    {
                        QByteArray tuyaData;
                        bool alternative = false;

                        if (R_GetProductId(sensor) == QLatin1String("Tuya_THD WZB-TRVL TRV"))
                        {
                            if (data.integer > 6)  { data.integer = 6;  } // offset, min = -60, max = 60
                            if (data.integer < -6) { data.integer = -6; }

                            alternative = true;
                        }
                        else
                        {
                            if (data.integer > 90)  { data.integer = 90;  } // offset, min = -90, max = 90
                            if (data.integer < -90) { data.integer = -90; }
                        }

                        tuyaData.append((qint8)((offset >> 24) & 0xff));
                        tuyaData.append((qint8)((offset >> 16) & 0xff));
                        tuyaData.append((qint8)((offset >> 8) & 0xff));
                        tuyaData.append((qint8)(offset & 0xff));

                        if (!alternative)
                        {
                            if (sendTuyaRequest(task, TaskThermostat, DP_TYPE_VALUE, 0x2c, tuyaData))
                            {
                                updated = true;
                            }
                        }
                        else
                        {
                            if (sendTuyaRequest(task, TaskThermostat, DP_TYPE_VALUE, 0x1b, tuyaData))
                            {
                                updated = true;
                            }
                        }
                    }
                    else if (sensor->modelId() == QLatin1String("eTRV0100") || sensor->modelId() == QLatin1String("TRV001"))
                    {
                        if (data.integer < -25) { data.integer = -25; }
                        if (data.integer > 25)  { data.integer = 25; }

                        if (addTaskThermostatReadWriteAttribute(task, deCONZ::ZclWriteAttributesId, VENDOR_DANFOSS, THERM_ATTRID_REGULATION_SETPOINT_OFFSET, deCONZ::Zcl8BitInt, data.integer))
                        {
                            updated = true;
                        }
                    }
                    else if (sensor->type() == "ZHAThermostat")
                    {
                        if (data.integer < -25) { data.integer = -25; }
                        if (data.integer > 25)  { data.integer = 25; }

                        if (addTaskThermostatReadWriteAttribute(task, deCONZ::ZclWriteAttributesId, 0x0000, THERM_ATTRID_LOCAL_TEMPERATURE_CALIBRATION, deCONZ::Zcl8BitInt, data.integer))
                        {
                            updated = true;
                        }
                    }
                    else
                    {
                        offsetUpdated = true;   // Consider offset only for temperature, humidity, and pressure cluster
                        updated = true;
                    }
                }
                else if (rid.suffix == RConfigScheduleOn) // Boolean
                {
                    if (sensor->modelId() == QLatin1String("Thermostat")) { data.boolean ^= data.boolean; } // eCozy, flip true and false

                    if (addTaskThermostatReadWriteAttribute(task, deCONZ::ZclWriteAttributesId, 0x0000, THERM_ATTRID_THERMOSTAT_PROGRAMMING_OPERATION_MODE, deCONZ::Zcl8BitBitMap, data.boolean))
                    {
                        updated = true;
                    }
                }
                else if (rid.suffix == RConfigHeatSetpoint) // Signed integer
                {
                    if (sensor->modelId().startsWith(QLatin1String("SPZB"))) // Eurotronic Spirit
                    {
                        // Older models of the Eurotroninc Spirit updated the heat set point via the manufacturer custom attribute 0x4003.
                        // For newer models it is not possible to write to this attribute.
                        // Newer models must use the standard Occupied Heating Setpoint value (0x0012) using a default (or none) manufacturer.
                        // See GitHub issue #1098
                        // UPD 16-11-2020: Since there is no way to reckognize older and newer models correctly and a new firmware version is on its way this
                        //                 'fix' is changed to a more robust but ugly implementation by simply sending both codes to the device. One of the commands
                        //                 will be accepted while the other one will be refused. Let's hope this code can be removed in a future release.

                        if (addTaskThermostatReadWriteAttribute(task, deCONZ::ZclWriteAttributesId, VENDOR_JENNIC, THERM_ATTRID_CURRENT_TEMPERATURE_SETPOINT, deCONZ::Zcl16BitInt, data.integer) &&
                            addTaskThermostatReadWriteAttribute(task, deCONZ::ZclWriteAttributesId, VENDOR_NONE,   THERM_ATTRID_OCCUPIED_HEATING_SETPOINT, deCONZ::Zcl16BitInt, data.integer))
                        {
                            // Setting the heat setpoint disables off/boost modes, but this is not reported back by the thermostat.
                            // Hence, the off/boost flags will be removed here to reflect the actual operating state.
                            hostFlags &= ~0x04; // clear `boost` flag
                            hostFlags |=  0x10; // set `disable off` flag

                            updated = true;
                        }
                    }
                    else if (sensor->modelId() == QLatin1String("eTRV0100") || sensor->modelId() == QLatin1String("TRV001"))
                    {
                        if (addTaskThermostatCmd(task, VENDOR_DANFOSS, 0x40, data.integer, 0))
                        {
                            updated = true;
                        }
                    }
                    else if (R_GetProductId(sensor) == QLatin1String("Tuya_THD HY369 TRV") ||
                             R_GetProductId(sensor) == QLatin1String("Tuya_THD HY368 TRV") ||
                             R_GetProductId(sensor) == QLatin1String("Tuya_THD GS361A-H04 TRV") ||
                             R_GetProductId(sensor) == QLatin1String("Tuya_THD Essentials TRV") ||
                             R_GetProductId(sensor) == QLatin1String("Tuya_THD NX-4911-675 TRV") ||
                             R_GetProductId(sensor) == QLatin1String("Tuya_THD WZB-TRVL TRV") ||
                             R_GetProductId(sensor) == QLatin1String("Tuya_THD BTH-002 Thermostat") ||
                             R_GetProductId(sensor) == QLatin1String("Tuya_THD Smart radiator TRV") ||
                             R_GetProductId(sensor) == QLatin1String("Tuya_THD MOES TRV") ||
                             R_GetProductId(sensor) == QLatin1String("Tuya_THD SEA801-ZIGBEE TRV"))
                    {
                        data.integer = data.integer / 10;
                        QByteArray tuyaData = QByteArray("\x00\x00", 2);
                        qint8 dp = DP_IDENTIFIER_THERMOSTAT_HEATSETPOINT;

                        if (R_GetProductId(sensor) == QLatin1String("Tuya_THD WZB-TRVL TRV") ||
                            R_GetProductId(sensor) == QLatin1String("Tuya_THD Smart radiator TRV") ||
                            R_GetProductId(sensor) == QLatin1String("Tuya_THD SEA801-ZIGBEE TRV"))
                        {
                            dp = DP_IDENTIFIER_THERMOSTAT_HEATSETPOINT_2;
                        }
                        else if (R_GetProductId(sensor) == QLatin1String("Tuya_THD BTH-002 Thermostat"))
                        {
                            dp = DP_IDENTIFIER_THERMOSTAT_HEATSETPOINT_3;
                            data.integer = data.integer / 10;
                        }
                        else if (R_GetProductId(sensor) == QLatin1String("Tuya_THD MOES TRV"))
                        {
                            ResourceItem *item2 = sensor->item(RConfigMode);

                            if (item2 && item2->toString() == QLatin1String("heat"))
                            {
                                dp = DP_IDENTIFIER_THERMOSTAT_HEATSETPOINT_3;
                            }
                            else
                            {
                                dp = DP_IDENTIFIER_THERMOSTAT_HEATSETPOINT_4;
                            }

                            data.integer = data.integer * 2 / 10;
                        }

                        tuyaData.append(static_cast<qint8>((data.integer >> 8) & 0xff));
                        tuyaData.append(static_cast<qint8>(data.integer & 0xff));

                        if (sendTuyaRequest(task, TaskThermostat, DP_TYPE_VALUE, dp, tuyaData))
                        {
                            updated = true;
                        }
                    }
                    else
                    {
                        if (addTaskThermostatReadWriteAttribute(task, deCONZ::ZclWriteAttributesId, 0x0000, THERM_ATTRID_OCCUPIED_HEATING_SETPOINT, deCONZ::Zcl16BitInt, data.integer))
                        {
                            updated = true;
                        }
                    }
                }
                else if (rid.suffix == RConfigCoolSetpoint) // Signed integer
                {
                    if (addTaskThermostatReadWriteAttribute(task, deCONZ::ZclWriteAttributesId, 0x0000, THERM_ATTRID_OCCUPIED_COOLING_SETPOINT, deCONZ::Zcl16BitInt, data.integer))
                    {
                        updated = true;
                    }
                }
                else if (rid.suffix == RConfigMode) // String
                {
                    if (sensor->modelId() == QLatin1String("Cable outlet")) // Legrand cable outlet
                    {
                        const auto match = matchKeyValue(data.string, RConfigModeLegrandValues);

                        if (isValid(match))
                        {
                            if (addTaskControlModeCmd(task, 0x00, match.value))
                            {
                                updated = true;
                            }
                        }
                    }
                    else if (R_GetProductId(sensor) == QLatin1String("Tuya_THD HY369 TRV") ||
                             R_GetProductId(sensor) == QLatin1String("Tuya_THD HY368 TRV") ||
                             R_GetProductId(sensor) == QLatin1String("Tuya_THD GS361A-H04 TRV") ||
                             R_GetProductId(sensor) == QLatin1String("Tuya_THD Essentials TRV") ||
                             R_GetProductId(sensor) == QLatin1String("Tuya_THD NX-4911-675 TRV") ||
                             R_GetProductId(sensor) == QLatin1String("Tuya_THD Smart radiator TRV") ||
                             R_GetProductId(sensor) == QLatin1String("Tuya_THD MOES TRV"))
                    {
                        const auto match = matchKeyValue(data.string, RConfigModeValuesTuya1);

                        if (isValid(match))
                        {
                            QByteArray tuyaData = QByteArray::fromRawData(match.value, 1);
                            quint8 dpIdentifier = DP_IDENTIFIER_THERMOSTAT_MODE_1;

                            if (R_GetProductId(sensor) == QLatin1String("Tuya_THD MOES TRV"))
                            {
                                dpIdentifier = DP_IDENTIFIER_THERMOSTAT_MODE_2;
                            }

                            if (sendTuyaRequest(task, TaskThermostat, DP_TYPE_ENUM, dpIdentifier, tuyaData))
                            {
                                updated = true;
                            }
                        }
                    }
                    else if (R_GetProductId(sensor) == QLatin1String("Tuya_THD BTH-002 Thermostat"))
                    {
                        const auto match = matchKeyValue(data.string, RConfigModeValuesTuya2);

                        if (isValid(match))
                        {
                            QByteArray tuyaData = QByteArray::fromRawData(match.value, 1);

                            if (sendTuyaRequest(task, TaskThermostat, DP_TYPE_BOOL, 0x01, tuyaData))
                            {
                                updated = true;
                            }
                        }
                    }
                    else if (R_GetProductId(sensor) == QLatin1String("Tuya_THD WZB-TRVL TRV") ||
                             R_GetProductId(sensor) == QLatin1String("Tuya_THD SEA801-ZIGBEE TRV"))
                    {
                        const auto match = matchKeyValue(data.string, RConfigModeValuesTuya2);

                        if (isValid(match))
                        {
                            if (match.key == QLatin1String("off"))
                            {
                                if (sendTuyaRequest(task, TaskThermostat, DP_TYPE_BOOL, DP_IDENTIFIER_THERMOSTAT_MODE_3, QByteArray("\x00", 1)))
                                {
                                    updated = true;
                                }
                            }
                            else
                            {
                                QByteArray tuyaData = QByteArray::fromRawData(match.value, 1);

                                if (sendTuyaRequest(task, TaskThermostat, DP_TYPE_BOOL, 0x6c, tuyaData) &&
                                    sendTuyaRequest(task, TaskThermostat, DP_TYPE_BOOL, 0x65, QByteArray("\x01", 1)))
                                {
                                    updated = true;
                                }
                            }
                        }
                    }
                    else if (sensor->modelId().startsWith(QLatin1String("S1")) ||
                             sensor->modelId().startsWith(QLatin1String("S2")) ||
                             sensor->modelId().startsWith(QLatin1String("J1")))
                    {
                        if (addTaskUbisysConfigureSwitch(task))
                        {
                            updated = true;
                        }
                    }
                    else if (sensor->modelId().startsWith(QLatin1String("SPZB"))) // Eurotronic Spirit
                    {
                        const auto match = matchKeyValue(data.string, RConfigModeValuesEurotronic);

                        if (isValid(match))
                        {
                            if (match.key == QLatin1String("off"))
                            {
                                hostFlags |= 0x000020; // set enable off
                                hostFlags &= 0xffffeb; // clear boost, clear disable off
                            }
                            else if (match.key == QLatin1String("heat"))
                            {
                                hostFlags |= 0x000014; // set boost, set disable off
                            }
                            else if (match.key == QLatin1String("auto"))
                            {
                                hostFlags &= 0xfffffb; // clear boost
                                hostFlags |= 0x000010; // set disable off
                            }

                            if (addTaskThermostatReadWriteAttribute(task, deCONZ::ZclWriteAttributesId, VENDOR_JENNIC, THERM_ATTRID_HOST_FLAGS, deCONZ::Zcl24BitUint, hostFlags))
                            {
                                updated = true;
                            }
                        }
                    }
                    else
                    {
                        const auto match = matchKeyValue(data.string, RConfigModeValues);

                        if (isValid(match))
                        {
                            if (sensor->modelId() == QLatin1String("Super TR")) // Set device on/off state through mode via device specific attribute
                            {
                                if (match.value != 0x00 && match.value != 0x04) { }
                                else
                                {
                                    bool data = match.value == 0x00 ? false : true;

                                    if (addTaskThermostatReadWriteAttribute(task, deCONZ::ZclWriteAttributesId, 0x0000, THERM_ATTRID_DEVICE_ON, deCONZ::ZclBoolean, data))
                                    {
                                        updated = true;
                                    }
                                }
                            }
                            else if (sensor->modelId().startsWith(QLatin1String("SLR2")) ||
                                     sensor->modelId() == QLatin1String("SLR1b"))
                            {
                                // Change automatically the Setpoint Hold
                                // Add a timer for Boost mode
                                if      (match.value == 0x00) { attributeList.insert(THERM_ATTRID_TEMPERATURE_SETPOINT_HOLD, (quint32)0x00); }
                                else if (match.value == 0x04) { attributeList.insert(THERM_ATTRID_TEMPERATURE_SETPOINT_HOLD, (quint32)0x01); }
                                else if (match.value == 0x05)
                                {
                                    attributeList.insert(THERM_ATTRID_TEMPERATURE_SETPOINT_HOLD, (quint32)0x01);
                                    attributeList.insert(THERM_ATTRID_TEMPERATURE_SETPOINT_HOLD_DURATION, (quint32)0x003C);
                                }

                                if (!attributeList.isEmpty())
                                {
                                    if (addTaskThermostatWriteAttributeList(task, 0, attributeList))
                                    {
                                     updated = true;
                                    }
                                }
                            }
                            else
                            {
                                if (addTaskThermostatReadWriteAttribute(task, deCONZ::ZclWriteAttributesId, 0x0000, THERM_ATTRID_SYSTEM_MODE, deCONZ::Zcl8BitEnum, match.value))
                                {
                                    updated = true;
                                }
                            }
                        }
                        else if (isClip)
                        {
                            updated = true;
                        }
                    }
                }
                else if (rid.suffix == RConfigPreset) // String
                {
                    if (R_GetProductId(sensor) == QLatin1String("Tuya_THD HY369 TRV") ||
                        R_GetProductId(sensor) == QLatin1String("Tuya_THD HY368 TRV") ||
                        R_GetProductId(sensor) == QLatin1String("Tuya_THD GS361A-H04 TRV") ||
                        R_GetProductId(sensor) == QLatin1String("Tuya_THD NX-4911-675 TRV") ||
                        R_GetProductId(sensor) == QLatin1String("Tuya_THD Smart radiator TRV") ||
                        R_GetProductId(sensor) == QLatin1String("Tuya_THD Essentials TRV") ||
                        R_GetProductId(sensor) == QLatin1String("Tuya_THD SEA801-ZIGBEE TRV"))
                    {
                        const auto match = matchKeyValue(data.string, RConfigPresetValuesTuya);

                        if (isValid(match))
                        {
                            QByteArray tuyaData = QByteArray::fromRawData(match.value, 1);

                            if (sendTuyaRequest(task, TaskThermostat, DP_TYPE_ENUM, 0x04, tuyaData))
                            {
                                updated = true;
                            }
                        }
                    }
                    else if (R_GetProductId(sensor) == QLatin1String("Tuya_THD BTH-002 Thermostat"))
                    {
                        const auto match = matchKeyValue(data.string, RConfigPresetValuesTuya2);

                        if (isValid(match))
                        {
                            if (match.key == QLatin1String("auto"))
                            {
                                if (sendTuyaRequest(task, TaskThermostat, DP_TYPE_ENUM, 0x02, QByteArray("\x01", 1)) &&
                                    sendTuyaRequest(task, TaskThermostat, DP_TYPE_ENUM, 0x03, QByteArray("\x00", 1)))
                                {
                                    updated = true;
                                }
                            }
                            else if (match.key == QLatin1String("program"))
                            {
                                if (sendTuyaRequest(task, TaskThermostat, DP_TYPE_ENUM, 0x02, QByteArray("\x00", 1)) &&
                                    sendTuyaRequest(task, TaskThermostat, DP_TYPE_ENUM, 0x03, QByteArray("\x01", 1)))
                                {
                                    updated = true;
                                }
                            }
                        }
                    }
                    else if (R_GetProductId(sensor) == QLatin1String("NAS-AB02B0 Siren"))
                    {
                        const auto match = matchKeyValue(data.string, RConfigPresetValuesTuya3);

                        if (isValid(match))
                        {
                            QByteArray tuyaData1;
                            QByteArray tuyaData2;
                            quint8 dpIdentifier1 = DP_IDENTIFIER_TEMPERATURE_ALARM;
                            quint8 dpIdentifier2 = DP_IDENTIFIER_HUMIDITY_ALARM;

                            if (match.key == QLatin1String("both"))
                            {
                                tuyaData1 = tuyaData2 = QByteArray("\x01", 1);
                            }
                            else if (match.key == QLatin1String("humidity"))
                            {
                                tuyaData1 = QByteArray("\x00", 1);
                                tuyaData2 = QByteArray("\x01", 1);
                            }
                            else if (match.key == QLatin1String("temperature"))
                            {
                                tuyaData1 = QByteArray("\x01", 1);
                                tuyaData2 = QByteArray("\x00", 1);
                            }
                            else if (match.key == QLatin1String("off"))
                            {
                                tuyaData1 = tuyaData2 = QByteArray("\x00", 1);
                            }

                            if (sendTuyaRequest(task, TaskTuyaRequest, DP_TYPE_BOOL, dpIdentifier1, tuyaData1) &&
                                sendTuyaRequest(task, TaskTuyaRequest, DP_TYPE_BOOL, dpIdentifier2, tuyaData2))
                            {
                                updated = true;
                            }
                        }
                    }
                    
                }
                else if (rid.suffix == RConfigLocked) // Boolean
                {
                    if (R_GetProductId(sensor) == QLatin1String("Tuya_THD SEA801-ZIGBEE TRV") ||
                        R_GetProductId(sensor) == QLatin1String("Tuya_THD NX-4911-675 TRV") ||
                        R_GetProductId(sensor) == QLatin1String("Tuya_THD Essentials TRV") ||
                        R_GetProductId(sensor) == QLatin1String("Tuya_THD WZB-TRVL TRV") ||
                        R_GetProductId(sensor) == QLatin1String("Tuya_THD GS361A-H04 TRV") ||
                        R_GetProductId(sensor) == QLatin1String("Tuya_THD Smart radiator TRV") ||
                        R_GetProductId(sensor) == QLatin1String("Tuya_THD BTH-002 Thermostat") ||
                        R_GetProductId(sensor) == QLatin1String("Tuya_THD MOES TRV") ||
                        R_GetProductId(sensor) == QLatin1String("Tuya_THD HY368 TRV") ||
                        R_GetProductId(sensor) == QLatin1String("Tuya_THD HY369 TRV"))
                    {
                        QByteArray tuyaData = QByteArray("\x00", 1);
                        qint8 dpIdentifier = DP_IDENTIFIER_THERMOSTAT_CHILDLOCK_1;

                        if (data.boolean) { tuyaData = QByteArray("\x01", 1); }

                        if (R_GetProductId(sensor) == QLatin1String("Tuya_THD BTH-002 Thermostat") ||
                            R_GetProductId(sensor) == QLatin1String("Tuya_THD WZB-TRVL TRV"))
                        {
                            dpIdentifier = DP_IDENTIFIER_THERMOSTAT_CHILDLOCK_2;
                        }
                        else if (R_GetProductId(sensor) == QLatin1String("Tuya_THD MOES TRV"))
                        {
                            dpIdentifier = DP_IDENTIFIER_THERMOSTAT_CHILDLOCK_3;
                        }

                        if (sendTuyaRequest(task, TaskThermostat, DP_TYPE_BOOL, dpIdentifier, tuyaData))
                        {
                            updated = true;
                        }
                    }
                    else if (sensor->modelId() == QLatin1String("Super TR"))
                    {
                        if (addTaskThermostatReadWriteAttribute(task, deCONZ::ZclWriteAttributesId, 0x0000, THERM_ATTRID_CHILD_LOCK, deCONZ::ZclBoolean, data.boolean))
                        {
                            updated = true;
                        }
                    }
                    else if (sensor->modelId().startsWith(QLatin1String("SPZB"))) // Eurotronic Spirit
                    {
                        if (data.boolean) { hostFlags |= 0x000080; } // set locked
                        else              { hostFlags &= 0xffff6f; } // clear locked, clear disable off

                        if (addTaskThermostatReadWriteAttribute(task, deCONZ::ZclWriteAttributesId, VENDOR_JENNIC, THERM_ATTRID_HOST_FLAGS, deCONZ::Zcl24BitUint, hostFlags))
                        {
                            updated = true;
                        }
                    }
                    else
                    {
                        data.uinteger = data.boolean; // Use integer representation

                        if (addTaskThermostatUiConfigurationReadWriteAttribute(task, deCONZ::ZclWriteAttributesId, THERM_UI_ATTRID_KEYPAD_LOCKOUT, deCONZ::Zcl8BitEnum, data.uinteger))
                        {
                            updated = true;
                        }
                    }
                }
                else if (rid.suffix == RConfigDisplayFlipped) // Boolean
                {
                    if (sensor->modelId() == QLatin1String("eTRV0100") || sensor->modelId() == QLatin1String("TRV001"))
                    {
                        data.uinteger = data.boolean; // Use integer representation

                        if (addTaskThermostatUiConfigurationReadWriteAttribute(task, deCONZ::ZclWriteAttributesId, THERM_UI_ATTRID_VIEWING_DIRECTION, deCONZ::Zcl8BitEnum, data.uinteger, VENDOR_DANFOSS))
                        {
                            updated = true;
                        }
                    }
                    else if (sensor->modelId().startsWith(QLatin1String("SPZB"))) // Eurotronic Spirit
                    {
                        if (data.boolean) { hostFlags |= 0x000002; } // set flipped
                        else              { hostFlags &= 0xffffed; } // clear flipped, clear disable off

                        if (addTaskThermostatReadWriteAttribute(task, deCONZ::ZclWriteAttributesId, VENDOR_JENNIC, THERM_ATTRID_HOST_FLAGS, deCONZ::Zcl24BitUint, hostFlags))
                        {
                            updated = true;
                        }
                    }
                }
                else if (rid.suffix == RConfigMountingMode) // Boolean
                {
                    data.uinteger = data.boolean; // Use integer representation

                    if (addTaskThermostatReadWriteAttribute(task, deCONZ::ZclWriteAttributesId, VENDOR_DANFOSS, THERM_ATTRID_MOUNTING_MODE_CONTROL, deCONZ::Zcl16BitInt, data.uinteger))
                    {
                        updated = true;
                    }
                }
                else if (rid.suffix == RConfigExternalTemperatureSensor) // Signed integer
                {
                    if (addTaskThermostatReadWriteAttribute(task, deCONZ::ZclWriteAttributesId, VENDOR_DANFOSS, THERM_ATTRID_EXTERNAL_MEASUREMENT, deCONZ::Zcl16BitInt, data.integer))
                    {
                        updated = true;
                    }
                }
                else if (rid.suffix == RConfigExternalWindowOpen) // Boolean
                {
                    if (addTaskThermostatReadWriteAttribute(task, deCONZ::ZclWriteAttributesId, VENDOR_DANFOSS, THERM_ATTRID_EXTERNAL_OPEN_WINDOW_DETECTED, deCONZ::ZclBoolean, data.boolean))
                    {
                        updated = true;
                    }
                }
                else if (rid.suffix == RConfigSetValve) // Boolean
                {
                    QByteArray tuyaData = QByteArray("\x00", 1);

                    if (data.boolean)
                    {
                        tuyaData = QByteArray("\x01", 1);
                    }

                    if (sendTuyaRequest(task, TaskThermostat, DP_TYPE_BOOL, DP_IDENTIFIER_THERMOSTAT_VALVE, tuyaData))
                    {
                        updated = true;
                    }
                }
                else if (rid.suffix == RConfigTemperatureMeasurement) // String
                {
                    if (sensor->modelId() == QLatin1String("Super TR"))
                    {
                        const auto match = matchKeyValue(data.string, RConfigTemperatureMeasurementValues);

                        if (isValid(match))
                        {
                            if (addTaskThermostatReadWriteAttribute(task, deCONZ::ZclWriteAttributesId, 0x0000, THERM_ATTRID_TEMPERATURE_MEASUREMENT, deCONZ::Zcl8BitEnum, match.value))
                            {
                                updated = true;
                            }
                        }
                    }
                }
                else if (rid.suffix == RConfigWindowOpen) // Boolean
                {
                    QByteArray tuyaData = QByteArray("\x00", 1); // Config on / off

                    if (data.boolean) { tuyaData = QByteArray("\x01", 1); }

                    qint8 dpIdentifier = DP_IDENTIFIER_WINDOW_OPEN;

                    if (R_GetProductId(sensor) == QLatin1String("Tuya_THD WZB-TRVL TRV"))
                    {
                        dpIdentifier = DP_IDENTIFIER_WINDOW_OPEN2;
                    }

                    if (sendTuyaRequest(task, TaskThermostat, DP_TYPE_BOOL, dpIdentifier, tuyaData))
                    {
                        updated = true;
                    }
                }
                else if (rid.suffix == RConfigSwingMode) // String
                {
                    const auto match = matchKeyValue(data.string, RConfigSwingModeValues);

                    if (isValid(match))
                    {
                        if (addTaskThermostatReadWriteAttribute(task, deCONZ::ZclWriteAttributesId, 0x0000, THERM_ATTRID_AC_LOUVER_POSITION, deCONZ::Zcl8BitEnum, match.value))
                        {
                            updated = true;
                        }
                    }
                }
                else if (rid.suffix == RConfigFanMode) // String
                {
                    const auto match = matchKeyValue(data.string, RConfigFanModeValues);

                    if (isValid(match))
                    {
                        if (addTaskFanControlReadWriteAttribute(task, deCONZ::ZclWriteAttributesId, FAN_CTRL_ATTRID_FAN_MODE, deCONZ::Zcl8BitEnum, match.value))
                        {
                            updated = true;
                        }
                    }
                }
                else if (rid.suffix == RConfigControlSequence) // Unsigned integer
                {
                    const auto match = matchKeyValue(data.uinteger, RConfigControlSequenceValues);

                    if (match.key)
                    {
                        if (addTaskThermostatReadWriteAttribute(task, deCONZ::ZclWriteAttributesId, 0x0000, THERM_ATTRID_CONTROL_SEQUENCE_OF_OPERATION, deCONZ::Zcl8BitEnum, match.value))
                        {
                            updated = true;
                        }
                    }
                }
                else if (rid.suffix == RConfigPulseConfiguration) // Unsigned integer
                {
                    if (data.uinteger <= UINT16_MAX &&
                        addTaskSimpleMeteringReadWriteAttribute(task, deCONZ::ZclWriteAttributesId, METERING_ATTRID_PULSE_CONFIGURATION, deCONZ::Zcl16BitUint, data.uinteger, VENDOR_DEVELCO))
                    {
                        updated = true;
                    }
                }
                else if (rid.suffix == RConfigInterfaceMode) // Unsigned integer
                {
                    if (sensor->modelId().startsWith(QLatin1String("ZHEMI101")))
                    {
                        const auto match = matchKeyValue(data.uinteger, RConfigInterfaceModeValuesZHEMI);

                        if (match.key)
                        {
                            if (addTaskSimpleMeteringReadWriteAttribute(task, deCONZ::ZclWriteAttributesId, METERING_ATTRID_INTERFACE_MODE, deCONZ::Zcl16BitEnum, match.value, VENDOR_DEVELCO))
                            {
                                updated = true;
                            }
                        }
                    }
                    else if (sensor->modelId().startsWith(QLatin1String("EMIZB-1")))
                    {
                        const auto match = matchKeyValue(data.uinteger, RConfigInterfaceModeValuesEMIZB);

                        if (match.key)
                        {
                            if (addTaskSimpleMeteringReadWriteAttribute(task, deCONZ::ZclWriteAttributesId, METERING_ATTRID_INTERFACE_MODE, deCONZ::Zcl16BitEnum, match.value, VENDOR_DEVELCO))
                            {
                                updated = true;
                            }
                        }
                    }
                }
                else if (rid.suffix == RConfigWindowCoveringType) // Unsigned integer
                {
                    if (sensor->modelId().startsWith(QLatin1String("J1")))
                    {
                        if (addTaskWindowCoveringCalibrate(task, data.uinteger))
                        {
                            updated = true;
                        }
                    }
                }
                else if (rid.suffix == RConfigGroup) // String
                {
                    checkSensorBindingsForClientClusters(sensor);
                    continue;
                }
                else if (QString(rid.suffix).startsWith("config/ubisys_j1_")) // Unsigned integer
                {
                    uint16_t mfrCode = VENDOR_UBISYS;
                    uint16_t attrId = 0xFFFF;
                    uint8_t attrType = deCONZ::Zcl16BitUint;
                    if (rid.suffix == RConfigUbisysJ1Mode)
                    {
                        mfrCode = 0x0000;
                        attrId = 0x0017;
                        attrType = deCONZ::Zcl8BitBitMap;
                    }
                    else if (rid.suffix == RConfigUbisysJ1WindowCoveringType)
                    {
                        attrId = 0x0000;
                        attrType = deCONZ::Zcl8BitEnum;
                    }
                    else if (rid.suffix == RConfigUbisysJ1ConfigurationAndStatus)
                    {
                        attrId = 0x0007;
                        attrType = deCONZ::Zcl8BitBitMap;
                    }
                    else if (rid.suffix == RConfigUbisysJ1InstalledOpenLimitLift)
                    {
                        attrId = 0x0010;
                    }
                    else if (rid.suffix == RConfigUbisysJ1InstalledClosedLimitLift)
                    {
                        attrId = 0x0011;
                    }
                    else if (rid.suffix == RConfigUbisysJ1InstalledOpenLimitTilt)
                    {
                        attrId = 0x0012;
                    }
                    else if (rid.suffix == RConfigUbisysJ1InstalledClosedLimitTilt)
                    {
                        attrId = 0x0013;
                    }
                    else if (rid.suffix == RConfigUbisysJ1TurnaroundGuardTime)
                    {
                        attrId = 0x1000;
                        attrType = deCONZ::Zcl8BitUint;
                    }
                    else if (rid.suffix == RConfigUbisysJ1LiftToTiltTransitionSteps)
                    {
                        attrId = 0x1001;
                    }
                    else if (rid.suffix == RConfigUbisysJ1TotalSteps)
                    {
                        attrId = 0x1002;
                    }
                    else if (rid.suffix == RConfigUbisysJ1LiftToTiltTransitionSteps2)
                    {
                        attrId = 0x1003;
                    }
                    else if (rid.suffix == RConfigUbisysJ1TotalSteps2)
                    {
                        attrId = 0x1004;
                    }
                    else if (rid.suffix == RConfigUbisysJ1AdditionalSteps)
                    {
                        attrId = 0x1005;
                        attrType = deCONZ::Zcl8BitUint;
                    }
                    else if (rid.suffix == RConfigUbisysJ1InactivePowerThreshold)
                    {
                        attrId = 0x1006;
                    }
                    else if (rid.suffix == RConfigUbisysJ1StartupSteps)
                    {
                        attrId = 0x1007;
                    }

                    if (attrId != 0xFFFF)
                    {
                        if (addTaskWindowCoveringSetAttr(task, mfrCode, attrId, attrType, data.uinteger))
                        {
                            updated = true;
                        }
                    }
                    else
                    {
                        rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/sensors/%1/config/%2").arg(id).arg(pi.key()),
                                                   QString("parameter, %1, not available").arg(pi.key())));
                    }
                }

                if (updated)
                {
                    if (item->setValue(val))
                    {
                        QVariantMap rspItem;
                        QVariantMap rspItemState;
                        rspItemState[QString("/sensors/%1/config/%2").arg(id).arg(pi.key())] = val;
                        rspItem[QLatin1String("success")] = rspItemState;
                        rsp.list.append(rspItem);
                        Event e(RSensors, rid.suffix, id, item);
                        enqueueEvent(e);
                    }

                    save = true;
                }
                else
                {
                    rsp.list.append(errorToMap(ERR_ACTION_ERROR, QString("/sensors/%1/config/%2").arg(id).arg(pi.key()),
                                               QLatin1String("Could not set attribute")));
                }
            }
            else // Resource item not found ifor sensor
            {
                rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/sensors/%1/config/%2").arg(id).arg(pi.key()),
                                           QString("parameter, %1, not available").arg(pi.key())));
            }
        }
        else // Resource item not found in general
        {
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/sensors/%1/config/%2").arg(id).arg(pi.key()),
                                       QString("parameter, %1, not available").arg(pi.key())));
        }
    }

    if (tholdUpdated)
    {
        ResourceItem *item = sensor->item(RStateLightLevel);
        if (item)
        {
            quint16 lightlevel = item->toNumber();

            item = sensor->item(RConfigTholdDark);
            if (item)
            {
                quint16 tholddark = item->toNumber();

                item = sensor->item(RConfigTholdOffset);
                if (item)
                {
                    quint16 tholdoffset = item->toNumber();

                    bool dark = lightlevel <= tholddark;
                    bool daylight = lightlevel >= tholddark + tholdoffset;

                    item = sensor->item(RStateDark);
                    if (!item)
                    {
                        item = sensor->addItem(DataTypeBool, RStateDark);
                    }
                    if (item && item->setValue(dark))
                    {
                        if (item->lastChanged() == item->lastSet())
                        {
                            Event e(RSensors, RStateDark, sensor->id(), item);
                            enqueueEvent(e);
                        }
                    }

                    item = sensor->item(RStateDaylight);
                    if (!item)
                    {
                        item = sensor->addItem(DataTypeBool, RStateDaylight);
                    }
                    if (item && item->setValue(daylight))
                    {
                        if (item->lastChanged() == item->lastSet())
                        {
                            Event e(RSensors, RStateDaylight, sensor->id(), item);
                            enqueueEvent(e);
                        }
                    }
                }
            }
        }
    }

    if (offsetUpdated)
    {
        ResourceItem *item = sensor->item(RStateTemperature);
        if (item)
        {
            qint16 temp = item->toNumber();
            temp += offset;
            if (item->setValue(temp))
            {
                Event e(RSensors, RStateTemperature, sensor->id(), item);
                enqueueEvent(e);
            }
        }

        item = sensor->item(RStateHumidity);
        if (item)
        {
            quint16 humidity = item->toNumber();
            qint16 _humidity = humidity + offset;
            humidity = _humidity < 0 ? 0 : _humidity > 10000 ? 10000 : _humidity;
            if (item->setValue(humidity))
            {
                Event e(RSensors, RStateHumidity, sensor->id(), item);
                enqueueEvent(e);
            }
        }
    }

    if (pendingMask)
    {
        ResourceItem *item = sensor->item(RConfigPending);
        if (item)
        {
            quint16 mask = item->toNumber();
            mask |= pendingMask;
            item->setValue(mask);
            Event e(RSensors, RConfigPending, sensor->id(), item);
            enqueueEvent(e);
        }
    }

    updateSensorEtag(sensor);

    if (save)
    {
        sensor->setNeedSaveDatabase(true);
        queSaveDb(DB_SENSORS, DB_SHORT_SAVE_DELAY);
    }

    processTasks();

    return REQ_READY_SEND;
}

/*! POST, DELETE /api/<apikey>/sensors/<id>/config/schedule/Wbbb
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::changeThermostatSchedule(const ApiRequest &req, ApiResponse &rsp)
{
    rsp.httpStatus = HttpStatusOk;

    // Get the /sensors/id resource.
    QString id = req.path[3];
    Sensor *sensor = id.length() < MIN_UNIQUEID_LENGTH ? getSensorNodeForId(id) : getSensorNodeForUniqueId(id);
    if (!sensor || (sensor->deletedState() == Sensor::StateDeleted))
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/sensors/%1").arg(id), QString("resource, /sensors/%1, not available").arg(id)));
        return REQ_READY_SEND;
    }

    // Check that it has config/schedule.
    ResourceItem *item = sensor->item(RConfigSchedule);
    if (!item)
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/sensors/%1/config/schedule").arg(id), QString("resource, /sensors/%1/config/schedule, not available").arg(id)));
        return REQ_READY_SEND;
    }

    // Check valid weekday pattern
    bool ok;
    uint bbb = req.path[6].mid(1).toUInt(&ok);
    if (req.path[6].left(1) != "W" || !ok || bbb < 1 || bbb > 127)
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/sensors/%1/config/schedule/%2").arg(id).arg(req.path[6]), QString("resource, /sensors/%1/config/schedule/%2, not available").arg(id).arg(req.path[6])));
        return REQ_READY_SEND;
    }
    quint8 weekdays = bbb;

    // Check body
    QString transitions = QString("");
    if (req.hdr.method() == QLatin1String("POST"))
    {
        QVariant var = Json::parse(req.content, ok);
        if (!ok)
        {
            rsp.list.append(errorToMap(ERR_INVALID_JSON, QString("/sensors/%1/config/schedule/%2").arg(id).arg(req.path[6]), QLatin1String("body contains invalid JSON")));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
        QVariantList list = var.toList();
        // QString transitions = QString("");
        if (!serialiseThermostatTransitions(list, &transitions))
        {
            rsp.list.append(errorToMap(ERR_INVALID_JSON, QString("/sensors/%1/config/schedule/%2").arg(id).arg(req.path[6]), QLatin1String("body contains invalid list of transitions")));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }

    if (req.sock)
    {
        userActivity();
    }

    bool ok2 = false;
    // Queue task.
    TaskItem task;
    task.req.dstAddress() = sensor->address();
    task.req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
    task.req.setDstEndpoint(sensor->fingerPrint().endpoint);
    task.req.setSrcEndpoint(getSrcEndpoint(sensor, task.req));
    task.req.setDstAddressMode(deCONZ::ApsExtAddress);

    if (R_GetProductId(sensor) == QLatin1String("Tuya_THD HY369 TRV") ||
        R_GetProductId(sensor) == QLatin1String("Tuya_THD HY368 TRV") ||
        R_GetProductId(sensor) == QLatin1String("Tuya_THD GS361A-H04 TRV") ||
        R_GetProductId(sensor) == QLatin1String("Tuya_THD Essentials TRV") ||
        R_GetProductId(sensor) == QLatin1String("Tuya_THD Smart radiator TRV") ||
        R_GetProductId(sensor) == QLatin1String("Tuya_THD NX-4911-675 TRV") ||
        R_GetProductId(sensor) == QLatin1String("Tuya_THD SEA801-ZIGBEE TRV"))
    {
        ok2 = sendTuyaRequestThermostatSetWeeklySchedule(task, weekdays, transitions, DP_IDENTIFIER_THERMOSTAT_SCHEDULE_2);
    }
    else if (R_GetProductId(sensor) == QLatin1String("Tuya_THD BTH-002 Thermostat"))
    {
        ok2 = sendTuyaRequestThermostatSetWeeklySchedule(task, weekdays, transitions, DP_IDENTIFIER_THERMOSTAT_SCHEDULE_1);
    }
    else if (R_GetProductId(sensor) == QLatin1String("Tuya_THD WZB-TRVL TRV"))
    {
        ok2 = sendTuyaRequestThermostatSetWeeklySchedule(task, weekdays, transitions, DP_IDENTIFIER_THERMOSTAT_SCHEDULE_4);
    }
    else
    {
        ok2 = addTaskThermostatSetWeeklySchedule(task, weekdays, transitions);
    }

    if (!ok2)
    {
        rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors/%1/config/schedule/%2").arg(id).arg(req.path[6]), QLatin1String("could not set schedule")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    QVariantMap rspItem;
    QVariantMap rspItemState;
    if (req.hdr.method() == QLatin1String("POST"))
    {
        QVariantList l;
        deserialiseThermostatTransitions(transitions, &l);
        rspItemState[QString("/config/schedule/W%1").arg(weekdays)] = l;
        rspItem[QLatin1String("success")] = rspItemState;
    }
    else
    {
        rspItem[QLatin1String("success")] = QString("/sensors/%1/config/schedule/W%2 deleted.").arg(id).arg(weekdays);
    }
    rsp.list.append(rspItem);

    updateThermostatSchedule(sensor, weekdays, transitions);

    processTasks();

    return REQ_READY_SEND;
}

/*! PUT, PATCH /api/<apikey>/sensors/<id>/state
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::changeSensorState(const ApiRequest &req, ApiResponse &rsp)
{
    QString id = req.path[3];
    Sensor *sensor = id.length() < MIN_UNIQUEID_LENGTH ? getSensorNodeForId(id) : getSensorNodeForUniqueId(id);
    bool ok;
    bool updated = false;
    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();
    QVariantMap rspItem;
    QVariantMap rspItemState;

    rsp.httpStatus = HttpStatusOk;

    if (!ok)
    {
        rsp.list.append(errorToMap(ERR_INVALID_JSON, QString("/sensors/%1/state").arg(id), QLatin1String("body contains invalid JSON")));
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

    if (req.sock)
    {
        userActivity();
    }

    //check invalid parameter
    QVariantMap::const_iterator pi = map.begin();
    QVariantMap::const_iterator pend = map.end();

    for (; pi != pend; ++pi)
    {
        ResourceItem *item = nullptr;
        ResourceItemDescriptor rid;
        if (getResourceItemDescriptor(QString("state/%1").arg(pi.key()), rid))
        {
            if (rid.suffix == RStateButtonEvent)
            {
                // allow modify physical switch buttonevent via api
            }
            else if (!isClip)
            {
                continue;
            }

            if (rid.suffix != RStateLux && rid.suffix != RStateDark && rid.suffix != RStateDaylight)
            {
                item = sensor->item(rid.suffix);
            }
            if (item)
            {
                QVariant val = map[pi.key()];
                if (rid.suffix == RStateTemperature || rid.suffix == RStateHumidity || rid.suffix == RStatePressure)
                {
                    ResourceItem *item2 = sensor->item(RConfigOffset);
                    if (item2 && item2->toNumber() != 0) {
                        val = val.toInt() + item2->toNumber();
                        if (rid.suffix == RStateHumidity)
                        {
                            val = val < 0 ? 0 : val > 10000 ? 10000 : val;
                        }
                    }
                }
                if (item->setValue(val))
                {
                    rspItemState[QString("/sensors/%1/state/%2").arg(id).arg(pi.key())] = val;
                    rspItem[QLatin1String("success")] = rspItemState;

                    if (rid.suffix == RStateButtonEvent ||  // always fire events for buttons
                        item->lastChanged() == item->lastSet())
                    {
                        updated = true;
                        Event e(RSensors, rid.suffix, id, item);
                        enqueueEvent(e);
                    }
                    sensor->updateStateTimestamp();
                    enqueueEvent(Event(RSensors, RStateLastUpdated, id));

                    if (rid.suffix == RStateLightLevel)
                    {
                        ResourceItem *item2 = 0;
                        quint16 measuredValue = val.toUInt();

                        quint16 tholddark = R_THOLDDARK_DEFAULT;
                        quint16 tholdoffset = R_THOLDOFFSET_DEFAULT;
                        item2 = sensor->item(RConfigTholdDark);
                        if (item2)
                        {
                            tholddark = item2->toNumber();
                        }
                        item2 = sensor->item(RConfigTholdOffset);
                        if (item2)
                        {
                            tholdoffset = item2->toNumber();
                        }
                        bool dark = measuredValue <= tholddark;
                        bool daylight = measuredValue >= tholddark + tholdoffset;

                        item2 = sensor->item(RStateDark);
                        if (!item2)
                        {
                            item2 = sensor->addItem(DataTypeBool, RStateDark);
                        }
                        if (item2->setValue(dark))
                        {
                            if (item2->lastChanged() == item2->lastSet())
                            {
                                Event e(RSensors, RStateDark, id, item2);
                                enqueueEvent(e);
                            }
                        }

                        item2 = sensor->item(RStateDaylight);
                        if (!item2)
                        {
                            item2 = sensor->addItem(DataTypeBool, RStateDaylight);
                        }
                        if (item2->setValue(daylight))
                        {
                            if (item2->lastChanged() == item2->lastSet())
                            {
                                Event e(RSensors, RStateDaylight, id, item2);
                                enqueueEvent(e);
                            }
                        }

                        item2 = sensor->item(RStateLux);
                        if (!item2)
                        {
                            item2 = sensor->addItem(DataTypeUInt32, RStateLux);
                        }
                        quint32 lux = 0;
                        if (measuredValue > 0 && measuredValue < 0xffff)
                        {
                            // valid values are 1 - 0xfffe
                            // 0, too low to measure
                            // 0xffff invalid value

                            // ZCL Attribute = 10.000 * log10(Illuminance (lx)) + 1
                            // lux = 10^((ZCL Attribute - 1)/10.000)
                            qreal exp = measuredValue - 1;
                            qreal l = qPow(10, exp / 10000.0f);
                            l += 0.5;   // round value
                            lux = static_cast<quint32>(l);
                        }
                        item2->setValue(lux);
                        if (item2->lastChanged() == item2->lastSet())
                        {
                            Event e(RSensors, RStateLux, id, item2);
                            enqueueEvent(e);
                        }
                    }
                    else if (rid.suffix == RStatePresence)
                    {
                        ResourceItem *item2 = sensor->item(RConfigDuration);
                        if (item2 && item2->toNumber() > 0)
                        {
                            sensor->durationDue = QDateTime::currentDateTime().addSecs(item2->toNumber()).addMSecs(-500);
                        }
                    }
                }
                else // invalid
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors/%1/state/%2").arg(id).arg(pi.key()),
                                               QString("invalid value, %1, for parameter %2").arg(val.toString()).arg(pi.key())));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }
            }
        }

        if (!item)
        {
            // not found
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/sensors/%1/state/%2").arg(id).arg(pi.key()), QString("parameter, %1, not available").arg(pi.key())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }

    rsp.list.append(rspItem);
    updateSensorEtag(sensor);
    if (updated)
    {
        sensor->setNeedSaveDatabase(true);
        queSaveDb(DB_SENSORS, DB_HUGE_SAVE_DELAY);
    }

    return REQ_READY_SEND;
}

/*! DELETE /api/<apikey>/sensors/<id>
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::deleteSensor(const ApiRequest &req, ApiResponse &rsp)
{
    QString id = req.path[3];
    Sensor *sensor = id.length() < MIN_UNIQUEID_LENGTH ? getSensorNodeForId(id) : getSensorNodeForUniqueId(id);

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
        rsp.list.append(errorToMap(ERR_INVALID_JSON, QString("/sensors/%1").arg(id), QLatin1String("body contains invalid JSON")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    sensor->setDeletedState(Sensor::StateDeleted);
    sensor->setNeedSaveDatabase(true);

    Event e(RSensors, REventDeleted, sensor->id());
    enqueueEvent(e);

    bool hasReset = map.contains(QLatin1String("reset"));

    if (hasReset)
    {
        if (map[QLatin1String("reset")].type() == QVariant::Bool)
        {
            bool reset = map[QLatin1String("reset")].toBool();

            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/sensors/%1/reset").arg(id)] = reset;
            rspItem[QLatin1String("success")] = rspItemState;
            rsp.list.append(rspItem);

            if (reset)
            {
                sensor->setResetRetryCount(10);
            }
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/sensors/%1/reset").arg(id), QString("invalid value, %1, for parameter, reset").arg(map[QLatin1String("reset")].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }
    else
    {
        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState[QLatin1String("id")] = id;
        rspItem[QLatin1String("success")] = rspItemState;
        rsp.list.append(rspItem);
        rsp.httpStatus = HttpStatusOk;
    }

    {
        Q_Q(DeRestPlugin);
        q->nodeUpdated(sensor->address().ext(), QLatin1String("deleted"), QLatin1String(""));
    }

    queSaveDb(DB_SENSORS, DB_SHORT_SAVE_DELAY);

    updateSensorEtag(sensor);
    rsp.httpStatus = HttpStatusOk;

    return REQ_READY_SEND;
}

/*! POST /api/<apikey>/sensors
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::searchNewSensors(const ApiRequest &req, ApiResponse &rsp)
{
    Q_UNUSED(req);

    if (!isInNetwork())
    {
        rsp.list.append(errorToMap(ERR_NOT_CONNECTED, QLatin1String("/sensors"), QLatin1String("Not connected")));
        rsp.httpStatus = HttpStatusServiceUnavailable;
        return REQ_READY_SEND;
    }

    startSearchSensors();
    {
        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState[QLatin1String("/sensors")] = QLatin1String("Searching for new devices");
        rspItemState[QLatin1String("/sensors/duration")] = (double)searchSensorsTimeout;
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

    if (!searchSensorsResult.isEmpty() &&
        (searchSensorsState == SearchSensorsActive || searchSensorsState == SearchSensorsDone))
    {

        rsp.map = searchSensorsResult;
    }

    if (searchSensorsState == SearchSensorsActive)
    {
        rsp.map[QLatin1String("lastscan")] = QLatin1String("active");
    }
    else if (searchSensorsState == SearchSensorsDone)
    {
        rsp.map[QLatin1String("lastscan")] = lastSensorsScan;
    }
    else
    {
        rsp.map[QLatin1String("lastscan")] = QLatin1String("none");
    }

    rsp.httpStatus = HttpStatusOk;
    return REQ_READY_SEND;
}

/*! Put all sensor parameters in a map.
    \return true - on success
            false - on error
 */
bool DeRestPluginPrivate::sensorToMap(const Sensor *sensor, QVariantMap &map, const ApiRequest &req)
{
    if (!sensor)
    {
        return false;
    }

    QVariantMap state;
    const ResourceItem *iox = nullptr;
    const ResourceItem *ioy = nullptr;
    const ResourceItem *ioz = nullptr;
    QVariantList orientation;
    const ResourceItem *ix = nullptr;
    const ResourceItem *iy = nullptr;
    QVariantList xy;
    QVariantMap config;
    const ResourceItem *ilcs = nullptr;
    const ResourceItem *ilca = nullptr;
    const ResourceItem *ilct = nullptr;
    QVariantMap lastchange;

    for (int i = 0; i < sensor->itemCount(); i++)
    {
        const ResourceItem *item = sensor->itemForIndex(static_cast<size_t>(i));
        DBG_Assert(item);
        const ResourceItemDescriptor &rid = item->descriptor();

        if (!item->isPublic())
        {
            continue;
        }

        if (rid.suffix == RConfigReachable &&
            sensor->type().startsWith(QLatin1String("ZGP")))
        {
            continue; // don't provide reachable for green power devices
        }

        if (strncmp(rid.suffix, "config/", 7) == 0)
        {
            const char *key = item->descriptor().suffix + 7;
            if (rid.suffix == RConfigPending)
            {
                QVariantList pending;
                auto value = item->toNumber();

                if (value & R_PENDING_DELAY)
                {
                    pending.append(QLatin1String("delay"));
                }
                if (value & R_PENDING_LEDINDICATION)
                {
                    pending.append(QLatin1String("ledindication"));
                }
                if (value & R_PENDING_SENSITIVITY)
                {
                    pending.append(QLatin1String("sensitivity"));
                }
                if (value & R_PENDING_USERTEST)
                {
                    pending.append(QLatin1String("usertest"));
                }
                if (value & R_PENDING_DEVICEMODE)
                {
                    pending.append(QLatin1String("devicemode"));
                }
                config[key] = pending;
            }
            else if (rid.suffix == RConfigLastChangeSource)
            {
                ilcs = item;
            }
            else if (rid.suffix == RConfigLastChangeAmount)
            {
                ilca = item;
            }
            else if (rid.suffix == RConfigLastChangeTime)
            {
                ilct = item;
            }
            else if (rid.suffix == RConfigSchedule)
            {
                QVariantMap schedule;
                deserialiseThermostatSchedule(item->toString(), &schedule);
                config[key] = schedule;
            }
            else
            {
                config[key] = item->toVariant();
            }
        }

        if (strncmp(rid.suffix, "state/", 6) == 0)
        {
            const char *key = item->descriptor().suffix + 6;

            if (rid.suffix == RStateLastUpdated)
            {
                if (!item->lastSet().isValid() || item->lastSet().date().year() < 2000)
                {
                    state[key] = QLatin1String("none");
                }
                else
                {
                    state[key] = item->toVariant().toDateTime().toString("yyyy-MM-ddTHH:mm:ss.zzz");
                }
            }
            else if (rid.suffix == RStateOrientationX)
            {
                iox = item;
            }
            else if (rid.suffix == RStateOrientationY)
            {
                ioy = item;
            }
            else if (rid.suffix == RStateOrientationZ)
            {
                ioz = item;
            }
            else if (rid.suffix == RStateX)
            {
                ix = item;
            }
            else if (rid.suffix == RStateY)
            {
                iy = item;
            }
            else
            {
                state[key] = item->toVariant();
            }
        }
    }
    if (iox && ioy && ioz)
    {
        orientation.append(iox->toNumber());
        orientation.append(ioy->toNumber());
        orientation.append(ioz->toNumber());
        state[QLatin1String("orientation")] = orientation;
    }
    if (ix && iy)
    {
        xy.append(round(ix->toNumber() / 6.5535) / 10000.0);
        xy.append(round(iy->toNumber() / 6.5535) / 10000.0);
        state[QLatin1String("xy")] = xy;
    }
    if (ilcs && ilca && ilct)
    {
        lastchange[QLatin1String("source")] = RConfigLastChangeSourceValues[ilcs->toNumber()];
        lastchange[QLatin1String("amount")] = ilca->toNumber();
        lastchange[QLatin1String("time")] = ilct->toVariant().toDateTime().toString("yyyy-MM-ddTHH:mm:ssZ");
        config[QLatin1String("lastchange")] = lastchange;
    }

    //sensor
    map[QLatin1String("name")] = sensor->name();
    map[QLatin1String("type")] = sensor->type();
    if (sensor->type().startsWith(QLatin1String("Z"))) // ZigBee sensor
    {
        map[QLatin1String("lastseen")] = sensor->lastRx().toUTC().toString("yyyy-MM-ddTHH:mmZ");
    }

    if (req.path.size() > 2 && req.path[2] == QLatin1String("devices"))
    {
        // don't add in sub device
    }
    else
    {
        if (!sensor->modelId().isEmpty())
        {
            map[QLatin1String("modelid")] = sensor->modelId();
        }
        if (!sensor->manufacturer().isEmpty())
        {
            map[QLatin1String("manufacturername")] = sensor->manufacturer();
        }
        if (!sensor->swVersion().isEmpty() && !sensor->type().startsWith(QLatin1String("ZGP")))
        {
            map[QLatin1String("swversion")] = sensor->swVersion();
        }
        if (sensor->fingerPrint().endpoint != INVALID_ENDPOINT)
        {
            map[QLatin1String("ep")] = sensor->fingerPrint().endpoint;
        }
        QString etag = sensor->etag;
        etag.remove('"'); // no quotes allowed in string
        map[QLatin1String("etag")] = etag;
    }

    // whitelist, HueApp crashes on ZHAAlarm and ZHAPressure
    if (req.mode == ApiModeHue)
    {
        if (!(sensor->type() == QLatin1String("Daylight") ||
              sensor->type() == QLatin1String("CLIPGenericFlag") ||
              sensor->type() == QLatin1String("CLIPGenericStatus") ||
              sensor->type() == QLatin1String("CLIPSwitch") ||
              sensor->type() == QLatin1String("CLIPOpenClose") ||
              sensor->type() == QLatin1String("CLIPPresence") ||
              sensor->type() == QLatin1String("CLIPTemperature") ||
              sensor->type() == QLatin1String("CLIPHumidity") ||
              sensor->type() == QLatin1String("CLIPLightlevel") ||
              sensor->type() == QLatin1String("ZGPSwitch") ||
              sensor->type() == QLatin1String("ZHASwitch") ||
              sensor->type() == QLatin1String("ZHAOpenClose") ||
              sensor->type() == QLatin1String("ZHAPresence") ||
              sensor->type() == QLatin1String("ZHATemperature") ||
              sensor->type() == QLatin1String("ZHAHumidity") ||
              sensor->type() == QLatin1String("ZHALightLevel")))
        {
            return false;
        }
        // mimic Hue Dimmer Switch
        if (sensor->modelId() == QLatin1String("TRADFRI wireless dimmer") ||
            sensor->modelId() == QLatin1String("lumi.sensor_switch.aq2"))
        {
            map[QLatin1String("manufacturername")] = QLatin1String("Philips");
            map[QLatin1String("modelid")] = QLatin1String("RWL021");
        }
        // mimic Hue motion sensor
        else if (false)
        {
            map[QLatin1String("manufacturername")] = QLatin1String("Philips");
            map[QLatin1String("modelid")] = QLatin1String("SML001");
        }
    }

    if (req.mode != ApiModeNormal &&
        sensor->manufacturer().startsWith(QLatin1String("Philips")) &&
        sensor->type().startsWith(QLatin1String("ZHA")))
    {
        QString type = sensor->type();
        type.replace(QLatin1String("ZHA"), QLatin1String("ZLL"));
        map[QLatin1String("type")] = type;
    }

    if (sensor->mode() != Sensor::ModeNone &&
        sensor->type().endsWith(QLatin1String("Switch")))
    {
        map[QLatin1String("mode")] = (double)sensor->mode();
    }

    const ResourceItem *item = sensor->item(RAttrUniqueId);
    if (item)
    {
        map[QLatin1String("uniqueid")] = item->toString();
    }
    map[QLatin1String("state")] = state;
    map[QLatin1String("config")] = config;

    return true;
}

void DeRestPluginPrivate::handleSensorEvent(const Event &e)
{
    DBG_Assert(e.resource() == RSensors);
    DBG_Assert(e.what() != nullptr);

    Sensor *sensor = getSensorNodeForId(e.id());

    if (!sensor)
    {
        return;
    }
    const QDateTime now = QDateTime::currentDateTime();

    // speedup sensor state check
    if ((e.what() == RStatePresence || e.what() == RStateButtonEvent) &&
        sensor && sensor->durationDue.isValid())
    {
        sensorCheckFast = CHECK_SENSOR_FAST_ROUNDS;
    }

    // push sensor state updates through websocket
    if (strncmp(e.what(), "state/", 6) == 0)
    {
        ResourceItem *item = sensor->item(e.what());
        if (item && item->isPublic())
        {
            if (item->descriptor().suffix == RStatePresence && item->toBool())
            {
                globalLastMotion = item->lastSet(); // remember
            }

            if (!(item->needPushSet() || item->needPushChange()))
            {
                DBG_Printf(DBG_INFO_L2, "discard sensor state push for %s: %s (already pushed)\n", qPrintable(e.id()), e.what());
                webSocketServer->flush(); // force transmit send buffer
                return; // already pushed
            }

            QVariantMap map;
            map[QLatin1String("t")] = QLatin1String("event");
            map[QLatin1String("e")] = QLatin1String("changed");
            map[QLatin1String("r")] = QLatin1String("sensors");
            map[QLatin1String("id")] = e.id();
            map[QLatin1String("uniqueid")] = sensor->uniqueId();
            QVariantMap state;
            ResourceItem *iox = nullptr;
            ResourceItem *ioy = nullptr;
            ResourceItem *ioz = nullptr;
            ResourceItem *ix = nullptr;
            ResourceItem *iy = nullptr;

            for (int i = 0; i < sensor->itemCount(); i++)
            {
                item = sensor->itemForIndex(i);
                const ResourceItemDescriptor &rid = item->descriptor();

                if (strncmp(rid.suffix, "state/", 6) == 0)
                {
                    const char *key = item->descriptor().suffix + 6;

                    if (rid.suffix == RStateOrientationX)
                    {
                        iox = item;
                    }
                    else if (rid.suffix == RStateOrientationY)
                    {
                        ioy = item;
                    }
                    else if (rid.suffix == RStateOrientationZ)
                    {
                        ioz = item;
                    }
                    else if (rid.suffix == RStateX)
                    {
                        ix = item;
                    }
                    else if (rid.suffix == RStateY)
                    {
                        iy = item;
                    }
                    else if (item->isPublic() && item->lastSet().isValid() && (gwWebSocketNotifyAll || rid.suffix == RStateButtonEvent || item->needPushChange()))
                    {
                        state[key] = item->toVariant();
                        item->clearNeedPush();
                    }
                }
            }

            if (iox && iox->lastSet().isValid() && ioy && ioy->lastSet().isValid() && ioz && ioz->lastSet().isValid())
            {
                if (gwWebSocketNotifyAll || iox->needPushChange() || ioy->needPushChange() || ioz->needPushChange())
                {
                    iox->clearNeedPush();
                    ioy->clearNeedPush();
                    ioz->clearNeedPush();

                    QVariantList orientation;
                    orientation.append(iox->toNumber());
                    orientation.append(ioy->toNumber());
                    orientation.append(ioz->toNumber());
                    state[QLatin1String("orientation")] = orientation;
                }
            }

            if (ix && ix->lastSet().isValid() && iy && iy->lastSet().isValid())
            {
                if (gwWebSocketNotifyAll || ix->needPushChange() || iy->needPushChange())
                {
                    ix->clearNeedPush();
                    iy->clearNeedPush();

                    QVariantList xy;
                    xy.append(round(ix->toNumber() / 6.5535) / 10000.0);
                    xy.append(round(iy->toNumber() / 6.5535) / 10000.0);
                    state[QLatin1String("xy")] = xy;
                }
            }

            if (!state.isEmpty())
            {
                map[QLatin1String("state")] = state;
                webSocketServer->broadcastTextMessage(Json::serialize(map));
            }
        }
    }
    else if (strncmp(e.what(), "config/", 7) == 0)
    {
        ResourceItem *item = sensor->item(e.what());
        if (item && item->isPublic())
        {
            if (!(item->needPushSet() || item->needPushChange()))
            {
                DBG_Printf(DBG_INFO_L2, "discard sensor config push for %s (already pushed)\n", e.what());
                return; // already pushed
            }

            QVariantMap map;
            map[QLatin1String("t")] = QLatin1String("event");
            map[QLatin1String("e")] = QLatin1String("changed");
            map[QLatin1String("r")] = QLatin1String("sensors");
            map[QLatin1String("id")] = e.id();
            map[QLatin1String("uniqueid")] = sensor->uniqueId();
            QVariantMap config;
            ResourceItem *ilcs = nullptr;
            ResourceItem *ilca = nullptr;
            ResourceItem *ilct = nullptr;

            for (int i = 0; i < sensor->itemCount(); i++)
            {
                item = sensor->itemForIndex(i);
                const ResourceItemDescriptor &rid = item->descriptor();

                if (strncmp(rid.suffix, "config/", 7) == 0)
                {
                    const char *key = item->descriptor().suffix + 7;

                    if (rid.suffix == RConfigLastChangeSource)
                    {
                        ilcs = item;
                    }
                    else if (rid.suffix == RConfigLastChangeAmount)
                    {
                        ilca = item;
                    }
                    else if (rid.suffix == RConfigLastChangeTime)
                    {
                        ilct = item;
                    }
                    else if (item->isPublic() && item->lastSet().isValid() && (gwWebSocketNotifyAll || item->needPushChange()))
                    {
                        if (rid.suffix == RConfigSchedule)
                        {
                            QVariantMap schedule;
                            deserialiseThermostatSchedule(item->toString(), &schedule);
                            config[key] = schedule;
                        }
                        else if (rid.suffix == RConfigPending)
                        {
                            QVariantList pending;
                            auto value = item->toNumber();

                            if (value & R_PENDING_DELAY)
                            {
                                pending.append(QLatin1String("delay"));
                            }
                            if (value & R_PENDING_LEDINDICATION)
                            {
                                pending.append(QLatin1String("ledindication"));
                            }
                            if (value & R_PENDING_SENSITIVITY)
                            {
                                pending.append(QLatin1String("sensitivity"));
                            }
                            if (value & R_PENDING_USERTEST)
                            {
                                pending.append(QLatin1String("usertest"));
                            }
                            if (value & R_PENDING_DEVICEMODE)
                            {
                                pending.append(QLatin1String("devicemode"));
                            }
                            config[key] = pending;
                        }
                        else
                        {
                            config[key] = item->toVariant();
                        }
                        item->clearNeedPush();
                    }
                }
            }
            if (ilcs && ilcs->lastSet().isValid() && ilca && ilca->lastSet().isValid() && ilct && ilct->lastSet().isValid())
            {
                if (gwWebSocketNotifyAll || ilcs->needPushChange() || ilca->needPushChange() || ilct->needPushChange())
                {
                    ilcs->clearNeedPush();
                    ilca->clearNeedPush();
                    ilct->clearNeedPush();

                    QVariantMap lastchange;
                    lastchange[QLatin1String("source")] = RConfigLastChangeSourceValues[ilcs->toNumber()];
                    lastchange[QLatin1String("amount")] = ilca->toNumber();
                    lastchange[QLatin1String("time")] = ilct->toVariant().toDateTime().toString("yyyy-MM-ddTHH:mm:ssZ");
                    config[QLatin1String("lastchange")] = lastchange;
                }
            }

            if (!config.isEmpty())
            {
                map[QLatin1String("config")] = config;
                webSocketServer->broadcastTextMessage(Json::serialize(map));
            }
        }
    }
    else if (strncmp(e.what(), "attr/", 5) == 0)
    {
        ResourceItem *item = sensor->item(e.what());
        if (item && item->isPublic())
        {
            QVariantMap map;
            map[QLatin1String("t")] = QLatin1String("event");
            map[QLatin1String("e")] = QLatin1String("changed");
            map[QLatin1String("r")] = QLatin1String("sensors");
            map[QLatin1String("id")] = e.id();
            map[QLatin1String("uniqueid")] = sensor->uniqueId();
            QVariantMap config;

            // For now, don't collect top-level attributes into a single event.
            const char *key = item->descriptor().suffix + 5;
            map[key] = item->toVariant();

            webSocketServer->broadcastTextMessage(Json::serialize(map));
        }
    }
    else if (e.what() == REventAdded)
    {
        checkSensorGroup(sensor);
        checkSensorBindingsForAttributeReporting(sensor);
        checkSensorBindingsForClientClusters(sensor);

        pushSensorInfoToCore(sensor);

        QVariantMap res;
        res[QLatin1String("name")] = sensor->name();
        searchSensorsResult[sensor->id()] = res;

        QVariantMap map;
        map[QLatin1String("t")] = QLatin1String("event");
        map[QLatin1String("e")] = QLatin1String("added");
        map[QLatin1String("r")] = QLatin1String("sensors");

        QVariantMap smap;

        QHttpRequestHeader hdr;  // dummy
        QStringList path;  // dummy
        ApiRequest req(hdr, path, nullptr, QLatin1String("")); // dummy

        req.mode = ApiModeNormal;
        sensorToMap(sensor, smap, req);
        map[QLatin1String("id")] = sensor->id();
        map[QLatin1String("uniqueid")] = sensor->uniqueId();
        smap[QLatin1String("id")] = sensor->id();
        map[QLatin1String("sensor")] = smap;

        webSocketServer->broadcastTextMessage(Json::serialize(map));
    }
    else if (e.what() == REventDeleted)
    {
        deleteGroupsWithDeviceMembership(e.id());

        QVariantMap map;
        map[QLatin1String("t")] = QLatin1String("event");
        map[QLatin1String("e")] = QLatin1String("deleted");
        map[QLatin1String("r")] = QLatin1String("sensors");

        QVariantMap smap;
        map[QLatin1String("id")] = e.id();
        map[QLatin1String("uniqueid")] = sensor->uniqueId();
        smap[QLatin1String("id")] = e.id();
        map[QLatin1String("sensor")] = smap;

        webSocketServer->broadcastTextMessage(Json::serialize(map));
    }
    else if (e.what() == REventValidGroup)
    {
        checkOldSensorGroups(sensor);

        ResourceItem *item = sensor->item(RConfigGroup);
        DBG_Assert(item != nullptr);
        if (!item)
        {
            return;
        }

        QStringList gids = item->toString().split(',', QString::SkipEmptyParts);

        for (int j = 0; j < gids.size(); j++) {
            const QString gid = gids[j];

            if (gid == QLatin1String("0"))
            {
                continue;
            }

            Group *group = getGroupForId(gid);

            if (group && group->state() != Group::StateNormal)
            {
                DBG_Printf(DBG_INFO, "reanimate group %s for sensor %s\n", qPrintable(gid), qPrintable(sensor->id()));
                group->setState(Group::StateNormal);
                group->setName(sensor->modelId() + QLatin1String(" ") + sensor->id());
                updateGroupEtag(group);
                queSaveDb(DB_GROUPS, DB_SHORT_SAVE_DELAY);
            }

            if (group && group->addDeviceMembership(sensor->id()))
            {
                DBG_Printf(DBG_INFO, "attach group %s to sensor %s\n", qPrintable(gid), qPrintable(sensor->id()));
                queSaveDb(DB_GROUPS, DB_LONG_SAVE_DELAY);
                updateGroupEtag(group);
            }

            if (!group) // create
            {
                DBG_Printf(DBG_INFO, "create group %s for sensor %s\n", qPrintable(gid), qPrintable(sensor->id()));
                Group g;
                g.setAddress(gid.toUInt());
                g.setName(sensor->modelId() + QLatin1String(" ") + sensor->id());
                g.addDeviceMembership(sensor->id());
                ResourceItem *item2 = g.addItem(DataTypeString, RAttrUniqueId);
                DBG_Assert(item2);
                if (item2)
                {
                    // FIXME: use the endpoint from which the group command was sent.
                    const QString uid = generateUniqueId(sensor->address().ext(), 0, 0);
                    item2->setValue(uid);
                }
                groups.push_back(g);
                updateGroupEtag(&groups.back());
                queSaveDb(DB_GROUPS, DB_SHORT_SAVE_DELAY);
                checkSensorBindingsForClientClusters(sensor);
            }
        }
    }
}

/*! Starts the search for new sensors.
 */
void DeRestPluginPrivate::startSearchSensors()
{
    if (searchSensorsState == SearchSensorsIdle || searchSensorsState == SearchSensorsDone)
    {
        pollNodes.clear();
        bindingQueue.clear();
        sensors.reserve(sensors.size() + 10);
        searchSensorsCandidates.clear();
        searchSensorsResult.clear();
        lastSensorsScan = QDateTime::currentDateTimeUtc().toString(QLatin1String("yyyy-MM-ddTHH:mm:ss"));
        QTimer::singleShot(1000, this, SLOT(searchSensorsTimerFired()));
        searchSensorGppPairCounter = 0;
        searchSensorsState = SearchSensorsActive;
    }
    else
    {
        Q_ASSERT(searchSensorsState == SearchSensorsActive);
    }

    searchSensorsTimeout = gwNetworkOpenDuration;
    gwPermitJoinResend = searchSensorsTimeout;
    if (!resendPermitJoinTimer->isActive())
    {
        resendPermitJoinTimer->start(100);
    }
}

/*! Handler for search sensors active state.
 */
void DeRestPluginPrivate::searchSensorsTimerFired()
{
    if (gwPermitJoinResend == 0)
    {
        if (gwPermitJoinDuration == 0)
        {
            searchSensorsTimeout = 0; // done
        }
    }

    if (searchSensorsTimeout > 0)
    {
        searchSensorsTimeout--;
        QTimer::singleShot(1000, this, SLOT(searchSensorsTimerFired()));
    }

    if (searchSensorsTimeout == 0)
    {
        DBG_Printf(DBG_INFO, "Search sensors done\n");
        fastProbeAddr = deCONZ::Address();
        fastProbeIndications.clear();
        searchSensorsState = SearchSensorsDone;
    }
}

/*! Validate sensor states. */
void DeRestPluginPrivate::checkSensorStateTimerFired()
{
    if (sensors.empty())
    {
        return;
    }

    if (sensorCheckIter >= sensors.size())
    {
        sensorCheckIter = 0;
        sensorCheckFast = (sensorCheckFast > 0) ? sensorCheckFast - 1 : 0;
    }

    for (int i = 0; i < CHECK_SENSORS_MAX; i++)
    {
        if (sensorCheckIter >= sensors.size())
        {
            break;
        }

        Sensor *sensor = &sensors[sensorCheckIter];
        sensorCheckIter++;

        if (sensor->deletedState() != Sensor::StateNormal)
        {
            continue;
        }

        if (sensor->durationDue.isValid())
        {
            QDateTime now = QDateTime::currentDateTime();

            if (sensor->modelId() == QLatin1String("TY0202")) // Lidl/SILVERCREST motion sensor
            {
                continue; // will be only reset via IAS Zone status
            }

            if (sensor->durationDue <= now)
            {
                // automatically set presence to false, if not triggered in config.duration
                ResourceItem *item = sensor->item(RStatePresence);
                if (item && item->toBool())
                {
                    DBG_Printf(DBG_INFO, "sensor %s (%s): disable presence\n", qPrintable(sensor->id()), qPrintable(sensor->modelId()));
                    item->setValue(false);
                    sensor->updateStateTimestamp();
                    enqueueEvent(Event(RSensors, RStatePresence, sensor->id(), item));
                    enqueueEvent(Event(RSensors, RStateLastUpdated, sensor->id()));
                    updateSensorEtag(sensor);
                    for (quint16 clusterId : sensor->fingerPrint().inClusters)
                    {
                        if (sensor->modelId().startsWith(QLatin1String("TRADFRI")))
                        {
                            clusterId = OCCUPANCY_SENSING_CLUSTER_ID; // workaround
                        }

                        if (clusterId == IAS_ZONE_CLUSTER_ID || clusterId == OCCUPANCY_SENSING_CLUSTER_ID)
                        {
                            pushZclValueDb(sensor->address().ext(), sensor->fingerPrint().endpoint, clusterId, 0x0000, 0);
                            break;
                        }
                    }
                }
                else if (!item && sensor->modelId() == QLatin1String("lumi.sensor_switch"))
                {
                    // Xiaomi round button (WXKG01LM)
                    // generate artificial hold event
                    item = sensor->item(RStateButtonEvent);
                    if (item && item->toNumber() == (S_BUTTON_1 + S_BUTTON_ACTION_INITIAL_PRESS))
                    {
                        item->setValue(S_BUTTON_1 + S_BUTTON_ACTION_HOLD);
                        DBG_Printf(DBG_INFO, "[INFO] - Button %u Hold %s\n", item->toNumber(), qPrintable(sensor->modelId()));
                        sensor->updateStateTimestamp();
                        enqueueEvent(Event(RSensors, RStateButtonEvent, sensor->id(), item));
                        enqueueEvent(Event(RSensors, RStateLastUpdated, sensor->id()));
                        updateSensorEtag(sensor);
                    }
                }
                else if (sensor->modelId() == QLatin1String("FOHSWITCH"))
                {
                    // Friends of Hue switch
                    // generate artificial hold event
                    item = sensor->item(RStateButtonEvent);
                    quint32 btn = item ? static_cast<quint32>(item->toNumber()) : 0;
                    const quint32 action = btn & 0x03;
                    if (btn >= S_BUTTON_1 && btn <= S_BUTTON_6 && action == S_BUTTON_ACTION_INITIAL_PRESS)
                    {
                        btn &= ~0x03;
                        item->setValue(btn + S_BUTTON_ACTION_HOLD);
                        DBG_Printf(DBG_INFO, "FoH switch button %d Hold %s\n", item->toNumber(), qPrintable(sensor->modelId()));
                        sensor->updateStateTimestamp();
                        enqueueEvent(Event(RSensors, RStateButtonEvent, sensor->id(), item));
                        enqueueEvent(Event(RSensors, RStateLastUpdated, sensor->id()));
                        updateSensorEtag(sensor);
                    }
                }
                else if (!item && sensor->modelId().startsWith(QLatin1String("lumi.vibration")) && sensor->type() == QLatin1String("ZHAVibration"))
                {
                    item = sensor->item(RStateVibration);
                    if (item && item->toBool())
                    {
                        DBG_Printf(DBG_INFO, "sensor %s (%s): disable vibration\n", qPrintable(sensor->id()), qPrintable(sensor->modelId()));
                        item->setValue(false);
                        sensor->updateStateTimestamp();
                        enqueueEvent(Event(RSensors, RStateVibration, sensor->id(), item));
                        enqueueEvent(Event(RSensors, RStateLastUpdated, sensor->id()));
                        updateSensorEtag(sensor);
                    }
                }

                sensor->durationDue = QDateTime();
            }
            else
            {
                sensorCheckFast = CHECK_SENSOR_FAST_ROUNDS;
            }
        }
    }

    // adjust check speed if needed
    int interval = (sensorCheckFast > 0) ? CHECK_SENSOR_FAST_INTERVAL
                                         : CHECK_SENSOR_INTERVAL;
    if (interval != checkSensorsTimer->interval())
    {
        DBG_Printf(DBG_INFO, "Set sensor check interval to %d milliseconds\n", interval);
        checkSensorsTimer->setInterval(interval);
    }
}

/*! Check insta mac address to model identifier.
 */
void DeRestPluginPrivate::checkInstaModelId(Sensor *sensor)
{
    if (sensor && existDevicesWithVendorCodeForMacPrefix(sensor->address(), VENDOR_INSTA))
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
void DeRestPluginPrivate::handleIndicationSearchSensors(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    if (searchSensorsState != SearchSensorsActive)
    {
        return;
    }

    if (isSameAddress(ind.srcAddress(), fastProbeAddr))
    {
        DBG_Printf(DBG_INFO, "FP indication 0x%04X / 0x%04X (0x%016llX / 0x%04X)\n", ind.profileId(), ind.clusterId(), ind.srcAddress().ext(), ind.srcAddress().nwk());
        DBG_Printf(DBG_INFO, "                      ...     (0x%016llX / 0x%04X)\n", fastProbeAddr.ext(), fastProbeAddr.nwk());
    }

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

        DBG_Printf(DBG_INFO, "device announce 0x%016llX (0x%04X) mac capabilities 0x%02X\n", ext, nwk, macCapabilities);

        // filter supported devices

        // Busch-Jaeger
        if (existDevicesWithVendorCodeForMacPrefix(ext, VENDOR_BUSCH_JAEGER))
        {
        }
        else if (existDevicesWithVendorCodeForMacPrefix(ext, VENDOR_UBISYS))
        {
        }
        else if (existDevicesWithVendorCodeForMacPrefix(ext, VENDOR_SUNRICHER))
        {
        }
        else if (existDevicesWithVendorCodeForMacPrefix(ext, VENDOR_BOSCH))
        { // macCapabilities == 0
        }
        else if (existDevicesWithVendorCodeForMacPrefix(ext, VENDOR_DEVELCO))
        { // macCapabilities == 0
        }
        else if (macCapabilities & deCONZ::MacDeviceIsFFD)
        {
            if (existDevicesWithVendorCodeForMacPrefix(ext, VENDOR_LDS))
            { //  Fix to allow Samsung SmartThings plug sensors to be created (7A-PL-Z-J3, modelId ZB-ONOFFPlug-D0005)
            }
            else if (existDevicesWithVendorCodeForMacPrefix(ext, VENDOR_JASCO))
            { //  Fix to support GE mains powered switches
            }
            else
            {
                return;
            }
        }
        else if (macCapabilities == 0)
        {
            return;
        }

        if (fastProbeAddr.hasExt())
        {
            return;
        }

        DBG_Printf(DBG_INFO, "set fast probe address to 0x%016llX (0x%04X)\n", ext, nwk);
        fastProbeAddr.setExt(ext);
        fastProbeAddr.setNwk(nwk);
        if (!fastProbeTimer->isActive())
        {
            fastProbeTimer->start(900);
        }

        fastProbeIndications.clear();
        fastProbeIndications.push_back(ind);

        std::vector<SensorCandidate>::iterator i = searchSensorsCandidates.begin();
        std::vector<SensorCandidate>::iterator end = searchSensorsCandidates.end();

        for (; i != end; ++i)
        {
            if (i->address.ext() == ext || i->address.nwk() == nwk)
            {
                i->waitIndicationClusterId = 0xffff;
                i->timeout.invalidate();
                i->address = deCONZ::Address(); // clear
            }
        }

        SensorCandidate sc;
        sc.waitIndicationClusterId = 0xffff;
        sc.address.setExt(ext);
        sc.address.setNwk(nwk);
        sc.macCapabilities = macCapabilities;
        searchSensorsCandidates.push_back(sc);
        return;
    }
    else if (ind.profileId() == ZDP_PROFILE_ID)
    {
        if (ind.clusterId() == ZDP_MATCH_DESCRIPTOR_CLID)
        {
            return;
        }

        if (!isSameAddress(ind.srcAddress(), fastProbeAddr))
        {
            return;
        }

        std::vector<SensorCandidate>::iterator i = searchSensorsCandidates.begin();
        std::vector<SensorCandidate>::iterator end = searchSensorsCandidates.end();

        for (; i != end; ++i)
        {
            if (i->address.ext() == fastProbeAddr.ext())
            {
                DBG_Printf(DBG_INFO, "ZDP indication search sensors 0x%016llX (0x%04X) cluster 0x%04X\n", ind.srcAddress().ext(), ind.srcAddress().nwk(), ind.clusterId());

                if (ind.clusterId() == i->waitIndicationClusterId && i->timeout.isValid())
                {
                    DBG_Printf(DBG_INFO, "ZDP indication search sensors 0x%016llX (0x%04X) clear timeout on cluster 0x%04X\n", ind.srcAddress().ext(), ind.srcAddress().nwk(), ind.clusterId());
                    i->timeout.invalidate();
                    i->waitIndicationClusterId = 0xffff;
                }

                if (ind.clusterId() & 0x8000)
                {
                    fastProbeIndications.push_back(ind); // remember responses
                }

                fastProbeTimer->stop();
                fastProbeTimer->start(5);
                break;
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
        case LEVEL_CLUSTER_ID:
        case VENDOR_CLUSTER_ID:
            if ((zclFrame.frameControl() & deCONZ::ZclFCClusterCommand) == 0)
            {
                return;
            }

            if (zclFrame.frameControl() & deCONZ::ZclFCDirectionServerToClient)
            {
                return;
            }
            break; // ok

        case BASIC_CLUSTER_ID:
            if (!zclFrame.isProfileWideCommand())
            {
                return;
            }

            if (zclFrame.commandId() != deCONZ::ZclReadAttributesResponseId && zclFrame.commandId() != deCONZ::ZclReportAttributesId)
            {
                return;
            }
            break; // ok

        case IAS_ZONE_CLUSTER_ID:
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

    SensorCandidate *sc = nullptr;
    {
        std::vector<SensorCandidate>::iterator i = searchSensorsCandidates.begin();
        std::vector<SensorCandidate>::iterator end = searchSensorsCandidates.end();

        for (; i != end; ++i)
        {
            if (isSameAddress(ind.srcAddress(), i->address))
            {
                sc = &*i;
                break;
            }
        }
    }

    if (sc && fastProbeAddr.hasExt() && sc->address.ext() == fastProbeAddr.ext())
    {
        if (zclFrame.manufacturerCode() == VENDOR_XIAOMI || zclFrame.manufacturerCode() == VENDOR_DSR)
        {
            DBG_Printf(DBG_INFO, "Remember Xiaomi special for 0x%016llX\n", ind.srcAddress().ext());
            fastProbeIndications.push_back(ind); // remember Xiaomi special report
        }

        if (!fastProbeTimer->isActive())
        {
            fastProbeTimer->start(5);
        }

        if (ind.profileId() == ZLL_PROFILE_ID || ind.profileId() == HA_PROFILE_ID)
        {
            if (ind.clusterId() == sc->waitIndicationClusterId && sc->timeout.isValid())
            {
                DBG_Printf(DBG_INFO, "Clear fast probe timeout for cluster 0x%04X, 0x%016llX\n", ind.clusterId(), ind.srcAddress().ext());
                sc->timeout.invalidate();
                sc->waitIndicationClusterId = 0xffff;
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
                else*/
                
                if (isSameAddress(node->address(), ind.srcAddress()))
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
        searchSensorsCandidates.push_back(sc2);
        sc = &searchSensorsCandidates.back();
    }

    if (!sc) // we need a valid candidate from device announce or cache
    {
        return;
    }

    // check for dresden elektronik devices
    if (existDevicesWithVendorCodeForMacPrefix(sc->address, VENDOR_DDEL))
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

            sensorNode.setNode(0);
            sensorNode.address() = sc->address;
            sensorNode.setType(QLatin1String("ZHASwitch"));
            sensorNode.fingerPrint() = fp;
            sensorNode.setUniqueId(generateUniqueId(sensorNode.address().ext(), sensorNode.fingerPrint().endpoint, COMMISSIONING_CLUSTER_ID));
            sensorNode.setManufacturer(QLatin1String("dresden elektronik"));

            ResourceItem *item;
            item = sensorNode.item(RConfigOn);
            item->setValue(true);

            item = sensorNode.item(RConfigReachable);
            item->setValue(true);

            sensorNode.addItem(DataTypeInt32, RStateButtonEvent);
            sensorNode.updateStateTimestamp();

            sensorNode.setNeedSaveDatabase(true);
            updateSensorEtag(&sensorNode);

            bool update = false;

            if (!s1 && isSceneSwitch && searchSensorsState == SearchSensorsActive)
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
                updateSensorEtag(s1);
                update = true;
                Event e(RSensors, REventAdded, sensorNode.id());
                enqueueEvent(e);
            }
            else if (isLightingSwitch)
            {
                if (!s1 && searchSensorsState == SearchSensorsActive)
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
                    updateSensorEtag(s1);
                    update = true;
                    Event e(RSensors, REventAdded, sensorNode.id());
                    enqueueEvent(e);
                }

                if (!s2 && searchSensorsState == SearchSensorsActive)
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
                    updateSensorEtag(s2);
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
    else if (existDevicesWithVendorCodeForMacPrefix(sc->address, VENDOR_IKEA))
    {
        if (sc->macCapabilities & deCONZ::MacDeviceIsFFD) // end-devices only
            return;

        if (ind.profileId() != HA_PROFILE_ID)
            return;

        // filter for remote control toggle command (large button)
        if (ind.srcEndpoint() == 0x01 && ind.clusterId() == SCENE_CLUSTER_ID  && zclFrame.manufacturerCode() == VENDOR_IKEA &&
                 zclFrame.commandId() == 0x07 && zclFrame.payload().at(0) == 0x02)
        {
            // TODO move following legacy cleanup code in Phoscon App / switch editor
            DBG_Printf(DBG_INFO, "ikea remote setup button\n");

            Sensor *s = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), ind.srcEndpoint());
            if (!s)
            {
                return;
            }

            std::vector<Rule>::iterator ri = rules.begin();
            std::vector<Rule>::iterator rend = rules.end();

            QString sensorAddress(QLatin1String("/sensors/"));
            sensorAddress.append(s->id());

            bool changed = false;

            for (; ri != rend; ++ri)
            {
                if (ri->state() != Rule::StateNormal)
                {
                    continue;
                }

                std::vector<RuleCondition>::const_iterator ci = ri->conditions().begin();
                std::vector<RuleCondition>::const_iterator cend = ri->conditions().end();

                for (; ci != cend; ++ci)
                {
                    if (ci->address().startsWith(sensorAddress))
                    {
                        if (ri->name().startsWith(QLatin1String("default-ct")) && ri->owner() == QLatin1String("deCONZ"))
                        {
                            DBG_Printf(DBG_INFO, "ikea remote delete legacy rule %s\n", qPrintable(ri->name()));
                            ri->setState(Rule::StateDeleted);
                            changed = true;
                        }
                    }
                }
            }

            if (changed)
            {
                indexRulesTriggers();
                queSaveDb(DB_RULES, DB_SHORT_SAVE_DELAY);
            }
        }
    }
}
