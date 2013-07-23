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
#include <QTextCodec>
#include <QTcpSocket>
#include <QHttpRequestHeader>
#include <QVariantMap>
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "json.h"

/*! Lights REST API broker.
    \param req - request data
    \param rsp - response data
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::handleLightsApi(ApiRequest &req, ApiResponse &rsp)
{
    if (req.path[2] != "lights")
    {
        return REQ_NOT_HANDLED;
    }

    if(!checkApikeyAuthentification(req, rsp))
    {
        return REQ_READY_SEND;
    }

    // GET /api/<apikey>/lights
    if ((req.path.size() == 3) && (req.hdr.method() == "GET"))
    {
        return getAllLights(req, rsp);
    }
    // POST /api/<apikey>/lights
    else if ((req.path.size() == 3) && (req.hdr.method() == "POST"))
    {
        return searchLights(req, rsp);
    }
    // GET /api/<apikey>/lights/new
    else if ((req.path.size() == 4) && (req.hdr.method() == "GET") && (req.path[3] == "new"))
    {
        return getNewLights(req, rsp);
    }
    // GET /api/<apikey>/lights/<id>
    else if ((req.path.size() == 4) && (req.hdr.method() == "GET"))
    {
        return getLightState(req, rsp);
    }
    // PUT /api/<apikey>/lights/<id>/state
    else if ((req.path.size() == 5) && (req.hdr.method() == "PUT") && (req.path[4] == "state"))
    {
        return setLightState(req, rsp);
    }
    // PUT /api/<apikey>/lights/<id> (rename)
    else if ((req.path.size() == 4) && (req.hdr.method() == "PUT"))
    {
        return renameLight(req, rsp);
    }
    // PUT /api/<apikey>/lights/<id>/name (rename)
    // same as above but this is that the hue app sends
    else if ((req.path.size() == 5) && (req.hdr.method() == "PUT") && (req.path[4] == "name"))
    {
        return renameLight(req, rsp);
    }

    return REQ_NOT_HANDLED;
}

/*! GET /api/<apikey>/lights
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::getAllLights(const ApiRequest &req, ApiResponse &rsp)
{
    Q_UNUSED(req);
    rsp.httpStatus = HttpStatusOk;

    std::vector<LightNode>::const_iterator i = nodes.begin();
    std::vector<LightNode>::const_iterator end = nodes.end();

    for (; i != end; ++i)
    {
        QVariantMap mnode;

        mnode["name"] = i->name();
        QString etag = i->etag;
        etag.remove('"'); // no quotes allowed in string
        mnode["etag"] = etag;
        rsp.map[i->id()] = mnode;
    }

    if (rsp.map.isEmpty())
    {
        rsp.str = "{}"; // return empty object
    }

    return REQ_READY_SEND;
}

/*! POST /api/<apikey>/lights
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::searchLights(const ApiRequest &req, ApiResponse &rsp)
{
    Q_UNUSED(req);

    if (isInNetwork())
    {
        setPermitJoinDuration(60);
        QVariantMap map1;
        QVariantMap map2;
        map1["/lights"] = "Searching for new devices";
        map2["success"] = map1;
        rsp.list.append(map2);
        rsp.httpStatus = HttpStatusOk;
    }
    else
    {
        rsp.list.append(errorToMap(ERR_NOT_CONNECTED, "/lights/search", "Not connected"));
        rsp.httpStatus = HttpStatusServiceUnavailable;
    }

    return REQ_READY_SEND;
}

/*! GET /api/<apikey>/lights/new
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::getNewLights(const ApiRequest &req, ApiResponse &rsp)
{
    // TODO: implement
    Q_UNUSED(req);
    Q_UNUSED(rsp);
    rsp.map["lastscan"] = "2012-10-29T12:00:00";
    return REQ_NOT_HANDLED; // TODO
}

/*! Put all parameters in a map for later json serialization.
    \return true - on success
            false - on error
 */
bool DeRestPluginPrivate::lightToMap(const ApiRequest &req, const LightNode *lightNode, QVariantMap &map)
{
    if (!lightNode)
    {
        return false;
    }

    QVariantMap state;
    //state["hue"] = (double)((uint16_t)(lightNode->hueReal * 65535));
    state["hue"] = (double)lightNode->enhancedHue();
    state["on"] = lightNode->isOn();
    state["effect"] = "none"; // TODO
    state["alert"] = "none"; // TODO
    state["bri"] = (double)lightNode->level();
    state["sat"] = (double)lightNode->saturation();
    state["ct"] = (double)500; // TODO
    QVariantList xy;
    uint16_t colorX = lightNode->colorX();
    uint16_t colorY = lightNode->colorY();
    // sanity for colorX
    if (colorX > 65279)
    {
        colorX = 65279;
    }
    // sanity for colorY
    if (colorY > 65279)
    {
        colorY = 65279;
    }
    double x = (double)colorX / 65279.0f; // normalize 0 .. 65279 to 0 .. 1
    double y = (double)colorY / 65279.0f; // normalize 0 .. 65279 to 0 .. 1
    xy.append(x);
    xy.append(y);
    state["xy"] = xy;
    state["reachable"] = lightNode->isAvailable();
    state["colormode"] = "hs"; // TODO
    map["type"] = lightNode->type();
    map["name"] = lightNode->name();
    map["modelid"] = lightNode->modelId(); // real model id

    if ((req.apiVersion() == ApiVersion_1_DDEL) || (lightNode->manufacturerCode() != VENDOR_DDEL))
    {
        map["type"] = lightNode->type();
    }
    else
    {
        // quirks mode to mimic Philips Hue
        // ... some apps wrongly think the light has no color otherwise
        //map["modelid"] = "LCT001"; // a hue
        map["type"] = "Extended color light";
    }
    map["swversion"] = lightNode->swBuildId();
    map["manufacturer"] = lightNode->manufacturer();
    QVariantMap pointsymbol;
    map["pointsymbol"] = pointsymbol; // dummy
    pointsymbol["1"] = QString("none");
    pointsymbol["2"] = QString("none");
    pointsymbol["3"] = QString("none");
    pointsymbol["4"] = QString("none");
    pointsymbol["5"] = QString("none");
    pointsymbol["6"] = QString("none");
    pointsymbol["7"] = QString("none");
    pointsymbol["8"] = QString("none");
    QString etag = lightNode->etag;
    etag.remove('"'); // no quotes allowed in string
    map["etag"] = etag;
    map["state"] = state;
    return true;
}

/*! GET /api/<apikey>/lights/<id>
    \return 0 - on success
           -1 - on error
 */
int DeRestPluginPrivate::getLightState(const ApiRequest &req, ApiResponse &rsp)
{
    DBG_Assert(req.path.size() == 4);

    if (req.path.size() != 4)
    {
        return -1;
    }

    const QString &id = req.path[3];

    LightNode *webNode = getLightNodeForId(id);

    if (!webNode)
    {
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/lights/%1").arg(id), QString("resource, /lights/%1, not available").arg(id)));
        rsp.httpStatus = HttpStatusNotFound;
        return REQ_READY_SEND;
    }

    // handle ETag
    if (req.hdr.hasKey("If-None-Match"))
    {
        QString etag = req.hdr.value("If-None-Match");

        if (webNode->etag == etag)
        {
            rsp.httpStatus = HttpStatusNotModified;
            rsp.etag = etag;
            return REQ_READY_SEND;
        }
    }

    lightToMap(req, webNode, rsp.map);
    rsp.httpStatus = HttpStatusOk;
    rsp.etag = webNode->etag;

    return REQ_READY_SEND;
}

/*! PUT /api/<apikey>/lights/<id>/state
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::setLightState(const ApiRequest &req, ApiResponse &rsp)
{
    TaskItem task;
    QString id = req.path[3];
    task.lightNode = getLightNodeForId(id);
    uint hue = UINT_MAX;
    uint sat = UINT_MAX;

    if (!task.lightNode)
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/lights/%1").arg(id), QString("resource, /lights/%1, not available").arg(id)));
        return REQ_READY_SEND;
    }

    rsp.httpStatus = HttpStatusOk;

    if (!task.lightNode->isAvailable())
    {
        rsp.httpStatus = HttpStatusOk;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/lights/%1").arg(id), QString("resource, /lights/%1, not available").arg(id)));
        return REQ_READY_SEND;
    }

    // set destination parameters
    task.req.dstAddress() = task.lightNode->address();
    task.req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
    task.req.setDstEndpoint(task.lightNode->haEndpoint().endpoint());
    task.req.setSrcEndpoint(getSrcEndpoint(task.lightNode, task.req));
    task.req.setDstAddressMode(deCONZ::ApsExtAddress);

    bool ok;
    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();

    if (!ok || map.isEmpty())
    {
        rsp.list.append(errorToMap(ERR_INVALID_JSON, QString("/lights/%1/state").arg(id), QString("body contains invalid JSON")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    // transition time
    if (map.contains("transitiontime"))
    {
        uint tt = map["transitiontime"].toUInt(&ok);

        if (ok && tt < 0xFFFFUL)
        {
            task.transitionTime = tt;
        }
    }

    // on/off
    if (map.contains("on"))
    {
        if (map["on"].type() == QVariant::Bool)
        {
            bool on = map["on"].toBool();
            if (map.contains("bri") ||
                // map.contains("transitionTime") || // FIXME: use bri if transitionTime is given
                addTaskSetOnOff(task, on)) // onOff task only if no bri or transitionTime is given
            {
                QVariantMap rspItem;
                QVariantMap rspItemState;
                rspItemState[QString("/lights/%1/state/on").arg(id)] = on;
                rspItem["success"] = rspItemState;
                rsp.list.append(rspItem);
                taskToLocalData(task);
            }
            else
            {
                rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
            }
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1/state/on").arg(id), QString("invalid value, %1, for parameter, on").arg(map["on"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }

    // brightness
    if (map.contains("bri"))
    {
        uint bri = map["bri"].toUInt(&ok);

        if (map.contains("on") && map["on"].type() == QVariant::Bool)
        {
            bool on = map["on"].toBool();
            if (!on)
            {
                bri = 0; // assume the caller wanted to switch the light off
            }
        }

        if (!task.lightNode->isOn() && !map.contains("on"))
        {
            rsp.list.append(errorToMap(ERR_DEVICE_OFF, QString("/lights/%1").arg(id), QString("parameter, /lights/%1/bri, is not modifiable. Device is set to off.").arg(id)));
        }
        else if (ok && (map["bri"].type() == QVariant::Double) && (bri < 256))
        {
            if (addTaskSetBrightness(task, bri, map.contains("on")))
            {
                QVariantMap rspItem;
                QVariantMap rspItemState;
                rspItemState[QString("/lights/%1/state/bri").arg(id)] = map["bri"];
                rspItem["success"] = rspItemState;
                rsp.list.append(rspItem);
                taskToLocalData(task);
            }
            else
            {
                rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
            }
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1/state/bri").arg(id), QString("invalid value, %1, for parameter, bri").arg(map["bri"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }

    // hue
    if (map.contains("hue")) // TODO check if map has no xy, ct ...
    {
        uint hue2 = map["hue"].toUInt(&ok);

        if (!task.lightNode->isOn())
        {
            rsp.list.append(errorToMap(ERR_DEVICE_OFF, QString("/lights/%1").arg(id), QString("parameter, /lights/%1/hue, is not modifiable. Device is set to off.").arg(id)));
        }
        else if (ok && (map["hue"].type() == QVariant::Double) && (hue2 < (MAX_ENHANCED_HUE + 1)))
        {
            hue = hue2;

            { // TODO: this is needed if saturation is set and addTaskSetEnhancedHue() will not be called
                task.hueReal = (double)hue / (360.0f * 182.04444f);

                if (task.hueReal < 0.0f)
                {
                    task.hueReal = 0.0f;
                }
                else if (task.hueReal > 1.0f)
                {
                    task.hueReal = 1.0f;
                }
                task.hue = task.hueReal * 254.0f;
                task.enhancedHue = hue;
                task.taskType = TaskSetEnhancedHue;
                taskToLocalData(task);
            }

            if (map.contains("sat") || // merge later to set hue and saturation
                addTaskSetEnhancedHue(task, hue)) // will only be evaluated if no sat is set
            {
                QVariantMap rspItem;
                QVariantMap rspItemState;
                rspItemState[QString("/lights/%1/state/hue").arg(id)] = map["hue"];
                rspItem["success"] = rspItemState;
                rsp.list.append(rspItem);
            }
            else
            {
                rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
            }
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1/state/hue").arg(id), QString("invalid value, %1, for parameter, hue").arg(map["hue"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }

    // saturation
    if (map.contains("sat")) // TODO check if map has no xy, ct ...
    {
        uint sat2 = map["sat"].toUInt(&ok);

        if (!task.lightNode->isOn())
        {
            rsp.list.append(errorToMap(ERR_DEVICE_OFF, QString("/lights/%1").arg(id), QString("parameter, /lights/%1/sat, is not modifiable. Device is set to off.").arg(id)));
        }
        else if (ok && (map["sat"].type() == QVariant::Double) && (sat2 < 256))
        {
            if (sat2 == 255)
            {
                sat2 = 254; // max valid value for level attribute
            }

            sat = sat2;
            task.sat = sat;
            task.taskType = TaskSetSat;
            taskToLocalData(task);

            if ((map.contains("hue") && (hue != UINT_MAX)) // merge later to set hue and saturation
               || addTaskSetSaturation(task, sat)) // will only be evaluated if no hue is set
            {
                QVariantMap rspItem;
                QVariantMap rspItemState;
                rspItemState[QString("/lights/%1/state/sat").arg(id)] = map["sat"];
                rspItem["success"] = rspItemState;
                rsp.list.append(rspItem);
            }
            else
            {
                rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
            }
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1/state/sat").arg(id), QString("invalid value, %1, for parameter, sat").arg(map["sat"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }

    // hue and saturation
    if (map.contains("hue") && map.contains("sat"))
    {

        if (!task.lightNode->isOn())
        {
            // no error here
        }
        else if ((hue != UINT_MAX) && (sat != UINT_MAX))
        {
            // need 8 bit hue
            qreal f = (qreal)hue / 182.04444f;

            f /= 360.0f;

            if (f > 1.0f)
            {
                f = 1.0f;
            }

            hue = f * 254.0f;

            DBG_Printf(DBG_INFO, "hue: %u, sat: %u\n", hue, sat);
            if (!addTaskSetHueAndSaturation(task, hue, sat))
            {
                DBG_Printf(DBG_INFO, "cant send task set hue and saturation\n");
            }
        }
        else
        {
            DBG_Printf(DBG_INFO, "cant merge hue and saturation: invalid value(s) hue: %u, sat: %u\n", hue, sat);
        }
    }

    // xy
    if (map.contains("xy"))
    {
        QVariantList ls = map["xy"].toList();

        if (!task.lightNode->isOn())
        {
            rsp.list.append(errorToMap(ERR_DEVICE_OFF, QString("/lights/%1").arg(id), QString("parameter, /lights/%1/sat, is not modifiable. Device is set to off.").arg(id)));
        }
        else if ((ls.size() == 2) && (ls[0].type() == QVariant::Double) && (ls[1].type() == QVariant::Double))
        {
            double x = ls[0].toDouble();
            double y = ls[1].toDouble();

            if ((x < 0.0f) || (x > 1.0f) || (y < 0.0f) || (y > 1.0f))
            {
                rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1").arg(id), QString("invalid value, [%1,%2], for parameter, /lights/%3/xy").arg(x).arg(y).arg(id)));
            }
            else if (addTaskSetXyColor(task, x, y))
            {
                QVariantMap rspItem;
                QVariantMap rspItemState;
                rspItemState[QString("/lights/%1/state/xy").arg(id)] = map["xy"];
                rspItem["success"] = rspItemState;
                rsp.list.append(rspItem);
                taskToLocalData(task);
            }
            else
            {
                rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
            }
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1/state/xy").arg(id), QString("invalid value, %1, for parameter, xy").arg(map["xy"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }

    if (task.lightNode)
    {
        updateEtag(task.lightNode->etag);
        updateEtag(gwConfigEtag);
        rsp.etag = task.lightNode->etag;
    }

    processTasks();
    // TODO ct, alert, effect

    return REQ_READY_SEND;
}

/*! PUT /api/<apikey>/lights/<id>
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::renameLight(const ApiRequest &req, ApiResponse &rsp)
{
    bool ok;
    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();
    QString id = req.path[3];
    LightNode *lightNode = getLightNodeForId(id);
    rsp.httpStatus = HttpStatusOk;

    if (!lightNode)
    {
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/lights/%1").arg(id), QString("resource, /lights/%1, not available").arg(id)));
        rsp.httpStatus = HttpStatusNotFound;
        return REQ_READY_SEND;
    }

    if (!ok || map.isEmpty())
    {
        rsp.list.append(errorToMap(ERR_INVALID_JSON, QString("/lights/%1").arg(id), QString("body contains invalid JSON")));
        return REQ_READY_SEND;
    }

    // name
    if (map.contains("name"))
    {
        QString name = map["name"].toString();

        if (name.size() <= 32)
        {
            // if zero length set default name
            // TODO use model description from basic cluster
            if (name.size() == 0)
            {
                name = lightNode->id();
            }

            if (lightNode->node())
            {
                lightNode->node()->setUserDescriptor(name);
            }
            if (lightNode->name() != name)
            {
                lightNode->setName(name);
                updateEtag(gwConfigEtag);
                updateEtag(lightNode->etag);
                needSaveDatabase = true;
            }

            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/lights/%1/name").arg(id)] = map["name"];
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);
            rsp.etag = lightNode->etag;
            return REQ_READY_SEND;
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1").arg(id), QString("invalid value, %1, for parameter, /lights/%2/name").arg(name).arg(id)));
            return REQ_READY_SEND;
        }
    }
    else
    {
        rsp.list.append(errorToMap(ERR_MISSING_PARAMETER, QString("/lights/%1").arg(id), QString("missing parameters in body")));
        return REQ_READY_SEND;
    }

    return REQ_NOT_HANDLED;
}
