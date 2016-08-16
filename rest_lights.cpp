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
#include "connectivity.h"

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
    // GET /api/<apikey>/lights/<id>/connectivity
    if ((req.path.size() == 5) && (req.hdr.method() == "GET") && (req.path[4] == "connectivity"))
    {
        return getConnectivity(req, rsp, false);
    }
    // GET /api/<apikey>/lights/<id>/connectivity
    if ((req.path.size() == 5) && (req.hdr.method() == "GET") && (req.path[4] == "connectivity2"))
    {
        return getConnectivity(req, rsp, true);
    }
    // DELETE /api/<apikey>/lights/<id>
    else if ((req.path.size() == 4) && (req.hdr.method() == "DELETE"))
    {
        return deleteLight(req, rsp);
    }
    // DELETE /api/<apikey>/lights/<id>/scenes
    else if ((req.path.size() == 5) && (req.path[4] == "scenes") && (req.hdr.method() == "DELETE"))
    {
        return removeAllScenes(req, rsp);
    }
    // DELETE /api/<apikey>/lights/<id>/groups
    else if ((req.path.size() == 5) && (req.path[4] == "groups") && (req.hdr.method() == "DELETE"))
    {
        return removeAllGroups(req, rsp);
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
        if (i->state() == LightNode::StateDeleted)
        {
            continue;
        }

        QVariantMap mnode;
        QVariantMap state;
        state["on"] = i->isOn();
        state["effect"] = "none";
        state["alert"] = "none"; // TODO
        state["bri"] = (double)i->level();
        state["reachable"] = i->isAvailable();

        if (i->hasColor())
        {
            state["hue"] = (double)i->enhancedHue();
            state["sat"] = (double)i->saturation();
            state["ct"] = (double)i->colorTemperature();
            state["effect"] = (i->isColorLoopActive() ? "colorloop" : "none");
            QVariantList xy;
            uint16_t colorX = i->colorX();
            uint16_t colorY = i->colorY();
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
            state["colormode"] = i->colorMode();
        }

        mnode["uniqueid"] = i->uniqueId();
        mnode["type"] = i->type();
        mnode["name"] = i->name();
        mnode["modelid"] = i->modelId(); // real model id
        //mnode["modelid"] = "LCT001"; // for Amazon Echo
        mnode["hascolor"] = i->hasColor();
        mnode["type"] = i->type();
        mnode["swversion"] = i->swBuildId();
        mnode["manufacturer"] = i->manufacturer();
        QVariantMap pointsymbol;
        mnode["pointsymbol"] = pointsymbol; // dummy
        pointsymbol["1"] = QString("none");
        pointsymbol["2"] = QString("none");
        pointsymbol["3"] = QString("none");
        pointsymbol["4"] = QString("none");
        pointsymbol["5"] = QString("none");
        pointsymbol["6"] = QString("none");
        pointsymbol["7"] = QString("none");
        pointsymbol["8"] = QString("none");
        QString etag = i->etag;
        etag.remove('"'); // no quotes allowed in string
        mnode["etag"] = etag;
        mnode["state"] = state;
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

    userActivity();

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
    Q_UNUSED(req);

    if (!lightNode)
    {
        return false;
    }

    QVariantMap state;
    state["on"] = lightNode->isOn();
    state["effect"] = "none";
    state["alert"] = "none"; // TODO
    state["bri"] = (double)lightNode->level();
    state["reachable"] = lightNode->isAvailable();

    if (lightNode->hasColor())
    {
        state["hue"] = (double)lightNode->enhancedHue();
        state["sat"] = (double)lightNode->saturation();
        state["ct"] = (double)lightNode->colorTemperature();
        state["effect"] = (lightNode->isColorLoopActive() ? "colorloop" : "none");
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
        state["colormode"] = lightNode->colorMode();
    }

    map["uniqueid"] = lightNode->uniqueId();
    map["type"] = lightNode->type();
    map["name"] = lightNode->name();
    map["modelid"] = lightNode->modelId(); // real model id
    map["hascolor"] = lightNode->hasColor();
    map["type"] = lightNode->type();
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

    LightNode *lightNode = getLightNodeForId(id);

    if (!lightNode || lightNode->state() == LightNode::StateDeleted)
    {
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/lights/%1").arg(id), QString("resource, /lights/%1, not available").arg(id)));
        rsp.httpStatus = HttpStatusNotFound;
        return REQ_READY_SEND;
    }

    // handle request to force query light state
    if (req.hdr.hasKey("Query-State"))
    {
        bool enabled = false;
        int diff = idleTotalCounter - lightNode->lastRead(READ_ON_OFF);
        QString attrs = req.hdr.value("Query-State");

        // only read if time since last read is not too short
        if (diff > 3)
        {
            if (attrs.contains("on"))
            {
                lightNode->enableRead(READ_ON_OFF);
                lightNode->setLastRead(READ_ON_OFF, idleTotalCounter);
                enabled = true;
            }

            if (attrs.contains("bri"))
            {
                lightNode->enableRead(READ_LEVEL);
                lightNode->setLastRead(READ_LEVEL, idleTotalCounter);
                enabled = true;
            }

            if (attrs.contains("color") && lightNode->hasColor())
            {
                lightNode->enableRead(READ_COLOR);
                lightNode->setLastRead(READ_COLOR, idleTotalCounter);
                enabled = true;
            }
        }

        if (enabled)
        {
            DBG_Printf(DBG_INFO, "Force read the attributes %s, for node %s\n", qPrintable(attrs), qPrintable(lightNode->address().toStringExt()));
            processZclAttributes(lightNode);
        }
    }

    // handle ETag
    if (req.hdr.hasKey("If-None-Match"))
    {
        QString etag = req.hdr.value("If-None-Match");

        if (lightNode->etag == etag)
        {
            rsp.httpStatus = HttpStatusNotModified;
            rsp.etag = etag;
            return REQ_READY_SEND;
        }
    }

    lightToMap(req, lightNode, rsp.map);
    rsp.httpStatus = HttpStatusOk;
    rsp.etag = lightNode->etag;

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

    userActivity();

    if (!task.lightNode || task.lightNode->state() == LightNode::StateDeleted)
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

    bool hasOn = map.contains("on");
    bool hasBri = map.contains("bri");
    bool hasHue = map.contains("hue");
    bool hasSat = map.contains("sat");
    bool hasXy = map.contains("xy");
    bool hasCt = map.contains("ct");
    bool hasEffect = map.contains("effect");
    bool hasEffectColorLoop = false;
    bool hasAlert = map.contains("alert");

    if (task.lightNode->manufacturerCode() == VENDOR_ATMEL)
    {
        hasXy = false;
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
    if (hasOn)
    {
        if (map["on"].type() == QVariant::Bool)
        {
            bool on = map["on"].toBool();

            if (!on && task.lightNode->isColorLoopActive())
            {
                addTaskSetColorLoop(task, false, 15);
                task.lightNode->setColorLoopActive(false); // deactivate colorloop if active
            }

            if (hasBri ||
                // map.contains("transitiontime") || // FIXME: use bri if transitionTime is given
                addTaskSetOnOff(task, on ? ONOFF_COMMAND_ON : ONOFF_COMMAND_OFF, 0)) // onOff task only if no bri or transitionTime is given
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
    if (hasBri)
    {
        uint bri = map["bri"].toUInt(&ok);

        if (hasOn && map["on"].type() == QVariant::Bool)
        {
            bool on = map["on"].toBool();
            if (!on)
            {
                bri = 0; // assume the caller wanted to switch the light off
            }
            else if (on && (bri == 0))
            {
                bri = 1; // don't turn off light is on is true
            }
        }

        if (!task.lightNode->isOn() && !map.contains("on"))
        {
            rsp.list.append(errorToMap(ERR_DEVICE_OFF, QString("/lights/%1").arg(id), QString("parameter, /lights/%1/bri, is not modifiable. Device is set to off.").arg(id)));
        }
        else if ((map["bri"].type() == QVariant::String) && map["bri"].toString() == "stop")
        {
            if (addTaskStopBrightness(task))
            {
                QVariantMap rspItem;
                QVariantMap rspItemState;
                rspItemState[QString("/groups/%1/action/bri").arg(id)] = map["bri"];
                rspItem["success"] = rspItemState;
                rsp.list.append(rspItem);
                taskToLocalData(task);
            }
            else
            {
                rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
            }
        }
        else if (ok && (map["bri"].type() == QVariant::Double) && (bri < 256))
        {
            if (addTaskSetBrightness(task, bri, hasOn))
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

    // colorloop
    if (hasEffect)
    {
        QString effect = map["effect"].toString();

        if (!task.lightNode->isOn())
        {
            rsp.list.append(errorToMap(ERR_DEVICE_OFF, QString("/lights/%1").arg(id), QString("parameter, /lights/%1/effect, is not modifiable. Device is set to off.").arg(id)));
        }
        else if ((effect == "none") || (effect == "colorloop"))
        {
            hasEffectColorLoop = effect == "colorloop";
            uint16_t speed = 15;

            if (hasEffectColorLoop)
            {
                if (map.contains("colorloopspeed"))
                {
                    speed = map["colorloopspeed"].toUInt(&ok);
                    if (ok && (map["colorloopspeed"].type() == QVariant::Double) && (speed < 256) && (speed > 0))
                    {
                        // ok
                        task.lightNode->setColorLoopSpeed(speed);
                    }
                    else
                    {
                        rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1/state/colorloopspeed").arg(id), QString("invalid value, %1, for parameter, colorloopspeed").arg(map["colorloopspeed"].toString())));
                    }
                }
            }

            if (addTaskSetColorLoop(task, hasEffectColorLoop, speed))
            {
                QVariantMap rspItem;
                QVariantMap rspItemState;
                rspItemState[QString("/lights/%1/state/effect").arg(id)] = map["effect"];
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
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1/state/effect").arg(id), QString("invalid value, %1, for parameter, effect").arg(map["effect"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }

    // hue
    if (hasHue)
    {
        uint hue2 = map["hue"].toUInt(&ok);

        if (!task.lightNode->isOn())
        {
            rsp.list.append(errorToMap(ERR_DEVICE_OFF, QString("/lights/%1").arg(id), QString("parameter, /lights/%1/hue, is not modifiable. Device is set to off.").arg(id)));
        }
        else if (ok && (map["hue"].type() == QVariant::Double) && (hue2 <= MAX_ENHANCED_HUE))
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

            if (hasSat || // merge later to set hue and saturation
                hasXy || hasCt || hasEffectColorLoop ||
                addTaskSetEnhancedHue(task, hue)) // will only be evaluated if no sat, xy, ct or colorloop is set
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
    if (hasSat)
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

            if (hasXy || hasCt
               || (!hasEffectColorLoop && hasHue && (hue != UINT_MAX)) // merge later to set hue and saturation
               || addTaskSetSaturation(task, sat)) // will only be evaluated if no hue, xy, ct is set
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
    if (hasHue && hasSat && !hasXy && !hasCt)
    {

        if (!task.lightNode->isOn())
        {
            // no error here
        }
        else if (!hasEffectColorLoop && (hue != UINT_MAX) && (sat != UINT_MAX))
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
                DBG_Printf(DBG_INFO, "can't send task set hue and saturation\n");
            }
        }
        else
        {
            DBG_Printf(DBG_INFO, "can't merge hue and saturation: invalid value(s) hue: %u, sat: %u\n", hue, sat);
        }
    }

    // xy
    if (hasXy)
    {
        QVariantList ls = map["xy"].toList();

        if (!task.lightNode->isOn())
        {
            rsp.list.append(errorToMap(ERR_DEVICE_OFF, QString("/lights/%1").arg(id), QString("parameter, /lights/%1/xy, is not modifiable. Device is set to off.").arg(id)));
        }
        else if ((ls.size() == 2) && (ls[0].type() == QVariant::Double) && (ls[1].type() == QVariant::Double))
        {
            double x = ls[0].toDouble();
            double y = ls[1].toDouble();

            if ((x < 0.0f) || (x > 1.0f) || (y < 0.0f) || (y > 1.0f))
            {
                rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1").arg(id), QString("invalid value, [%1,%2], for parameter, /lights/%3/xy").arg(x).arg(y).arg(id)));
            }
            else if (hasEffectColorLoop ||
                     addTaskSetXyColor(task, x, y)) // will only be evaluated if no color loop is set
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

    // color temperature
    if (hasCt)
    {
        uint16_t ct = map["ct"].toUInt(&ok);

        if (!task.lightNode->isOn())
        {
            rsp.list.append(errorToMap(ERR_DEVICE_OFF, QString("/lights/%1").arg(id), QString("parameter, /lights/%1/ct, is not modifiable. Device is set to off.").arg(id)));
        }
        else if (ok && (map["ct"].type() == QVariant::Double))
        {
            if (hasXy || hasEffectColorLoop ||
                addTaskSetColorTemperature(task, ct)) // will only be evaluated if no xy and color loop is set
            {
                QVariantMap rspItem;
                QVariantMap rspItemState;
                rspItemState[QString("/lights/%1/state/ct").arg(id)] = map["ct"];
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
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1/state/ct").arg(id), QString("invalid value, %1, for parameter, ct").arg(map["ct"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }

    // alert
    if (hasAlert)
    {
        QString alert = map["alert"].toString();

        if (alert == "none")
        {
            task.identifyTime = 0;
        }
        else if (alert == "select")
        {
            task.identifyTime = 1;
        }
        else if (alert == "lselect")
        {
            task.identifyTime = 30;
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1/state/alert").arg(id), QString("invalid value, %1, for parameter, alert").arg(map["alert"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

        task.taskType = TaskIdentify;
        taskToLocalData(task);

        if (addTaskIdentify(task, task.identifyTime))
        {
            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/lights/%1/state/alert").arg(id)] = map["alert"];
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
        }
    }

    if (task.lightNode)
    {
        updateEtag(task.lightNode->etag);
        updateEtag(gwConfigEtag);
        rsp.etag = task.lightNode->etag;
    }

    processTasks();

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

    if (!lightNode || lightNode->state() == LightNode::StateDeleted)
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
                queSaveDb(DB_LIGHTS, DB_SHORT_SAVE_DELAY);
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

/*! DELETE /api/<apikey>/lights/<id>
    \return 0 - on success
           -1 - on error
 */
int DeRestPluginPrivate::deleteLight(const ApiRequest &req, ApiResponse &rsp)
{
    DBG_Assert(req.path.size() == 4);

    if (req.path.size() != 4)
    {
        return -1;
    }

    const QString &id = req.path[3];

    LightNode *lightNode = getLightNodeForId(id);

    if (!lightNode)
    {
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/lights/%1").arg(id), QString("resource, /lights/%1, not available").arg(id)));
        rsp.httpStatus = HttpStatusNotFound;
        return REQ_READY_SEND;
    }

    bool ok;
    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();

    if (!ok)
    {
        rsp.list.append(errorToMap(ERR_INVALID_JSON, QString("/lights/%1").arg(id), QString("body contains invalid JSON")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    bool hasReset = map.contains("reset");

    if (hasReset)
    {
        if (map["reset"].type() == QVariant::Bool)
        {
            bool reset = map["reset"].toBool();

            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/lights/%1/reset").arg(id)] = reset;
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);

            if (reset)
            {
                lightNode->setResetRetryCount(10);
            }
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1/reset").arg(id), QString("invalid value, %1, for parameter, reset").arg(map["reset"].toString())));
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
    }

    // delete all group membership from light
    std::vector<GroupInfo>::iterator g = lightNode->groups().begin();
    std::vector<GroupInfo>::iterator gend = lightNode->groups().end();

    for (; g != gend; ++g)
    {
        //delete Light from all scenes.
        deleteLightFromScenes(id, g->id);

        //delete Light from all groups
        g->actions &= ~GroupInfo::ActionAddToGroup;
        g->actions |= GroupInfo::ActionRemoveFromGroup;
        if (g->state != GroupInfo::StateNotInGroup)
        {
            g->state = GroupInfo::StateNotInGroup;
        }
    }

    lightNode->setState(LightNode::StateDeleted);
    updateEtag(gwConfigEtag);
    updateEtag(lightNode->etag);
    queSaveDb(DB_LIGHTS, DB_SHORT_SAVE_DELAY);

    rsp.httpStatus = HttpStatusOk;
    rsp.etag = lightNode->etag;

    return REQ_READY_SEND;
}

/*! DELETE /api/<apikey>/lights/<id>/scenes
    \return 0 - on success
           -1 - on error
 */
int DeRestPluginPrivate::removeAllScenes(const ApiRequest &req, ApiResponse &rsp)
{
    DBG_Assert(req.path.size() == 5);

    if (req.path.size() != 5)
    {
        return -1;
    }

    const QString &id = req.path[3];

    LightNode *lightNode = getLightNodeForId(id);

    if (!lightNode)
    {
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/lights/%1").arg(id), QString("resource, /lights/%1, not available").arg(id)));
        rsp.httpStatus = HttpStatusNotFound;
        return REQ_READY_SEND;
    }

    else
    {
        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState["id"] = id;
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
    }

    //delete Light from all scenes.
    std::vector<GroupInfo>::iterator g = lightNode->groups().begin();
    std::vector<GroupInfo>::iterator gend = lightNode->groups().end();

    for (; g != gend; ++g)
    {
        deleteLightFromScenes(id, g->id);
    }

    queSaveDb(DB_SCENES, DB_SHORT_SAVE_DELAY);
    rsp.httpStatus = HttpStatusOk;
    rsp.etag = lightNode->etag;

    return REQ_READY_SEND;
}

/*! DELETE /api/<apikey>/lights/<id>/groups
    \return 0 - on success
           -1 - on error
 */
int DeRestPluginPrivate::removeAllGroups(const ApiRequest &req, ApiResponse &rsp)
{
    DBG_Assert(req.path.size() == 5);

    if (req.path.size() != 5)
    {
        return -1;
    }

    const QString &id = req.path[3];

    LightNode *lightNode = getLightNodeForId(id);

    if (!lightNode)
    {
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/lights/%1").arg(id), QString("resource, /lights/%1, not available").arg(id)));
        rsp.httpStatus = HttpStatusNotFound;
        return REQ_READY_SEND;
    }

    QVariantMap rspItem;
    QVariantMap rspItemState;
    rspItemState["id"] = id;
    rspItem["success"] = rspItemState;
    rsp.list.append(rspItem);

    // delete all group membership from light
    std::vector<GroupInfo>::iterator g = lightNode->groups().begin();
    std::vector<GroupInfo>::iterator gend = lightNode->groups().end();

    for (; g != gend; ++g)
    {
        //delete Light from all scenes.
        deleteLightFromScenes(id, g->id);

        //delete Light from all groups
        g->actions &= ~GroupInfo::ActionAddToGroup;
        g->actions |= GroupInfo::ActionRemoveFromGroup;
        if (g->state != GroupInfo::StateNotInGroup)
        {
            g->state = GroupInfo::StateNotInGroup;
        }
    }

    updateEtag(gwConfigEtag);
    updateEtag(lightNode->etag);
    queSaveDb(DB_LIGHTS, DB_SHORT_SAVE_DELAY);

    rsp.httpStatus = HttpStatusOk;
    rsp.etag = lightNode->etag;

    return REQ_READY_SEND;
}

/*! GET /api/<apikey>/lights/<id>/connectivity
    \return 0 - on success
           -1 - on error
 */
int DeRestPluginPrivate::getConnectivity(const ApiRequest &req, ApiResponse &rsp, bool alt)
{
    Connectivity newConn;
    uint64_t coordinatorAddress = 0;
    newConn.targets.clear();
    std::list<quint8> rlqiListTemp = newConn.getRLQIList();
    rlqiListTemp.clear();
    newConn.setRLQIList(rlqiListTemp);
    quint16 sumLQI = 0;
    quint8 meanLQI = 0;

    DBG_Assert(req.path.size() == 5);

    if (req.path.size() != 5)
    {
        return -1;
    }

    const QString &id = req.path[3];

    //Rest LightNode
    LightNode *lightNode = getLightNodeForId(id);

    if (!lightNode)
    {
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/lights/%1").arg(id), QString("resource, /lights/%1, not available").arg(id)));
        rsp.httpStatus = HttpStatusNotFound;
        return REQ_READY_SEND;
    }

    //deCONZ Node
    uint n = 0;
    const deCONZ::Node *node = 0;

    while (apsCtrl->getNode(n, &node) == 0)
    {
        if (node->isCoordinator())
        {
            coordinatorAddress = node->address().ext();

            //set start node
            DeRestPluginPrivate::nodeVisited nv;
            nv.node = node;
            nv.visited = false;
            newConn.start = nv;
        }
        else
        {
            //set target nodes
            if (!(node->isZombie()))
            {
                DeRestPluginPrivate::nodeVisited nv;
                nv.node = node;
                nv.visited = false;
                newConn.targets.push_back(nv);
            }
        }
        n++;
    }

    //start route search
    std::vector<DeRestPluginPrivate::nodeVisited> resultList;
    std::vector<deCONZ::NodeNeighbor> neighborList;

    for (uint r = 0; r < newConn.targets.size(); r++)
    {
        if (lightNode->address().ext() == newConn.targets[r].node->address().ext())
        {
            // first get neighbours of target node
            // TODO: philips strip doesn't recognize fls as neighbours.
            const std::vector<deCONZ::NodeNeighbor> &neighbors = newConn.targets[r].node->neighbors();

            std::vector<deCONZ::NodeNeighbor>::const_iterator nb = neighbors.begin();
            std::vector<deCONZ::NodeNeighbor>::const_iterator nb_end = neighbors.end();

            DBG_Printf(DBG_INFO,"Node: %s\n",qPrintable(newConn.targets[r].node->address().toStringExt()));
            for (; nb != nb_end; ++nb)
            {
                DBG_Printf(DBG_INFO,"neighbour: %s, LQI %u\n",qPrintable(nb->address().toStringExt()),nb->lqi());
                neighborList.push_back(*nb);
                sumLQI = sumLQI + (nb->lqi());
                DBG_Printf(DBG_INFO,"sum: %u\n",sumLQI);
            }

            //-- first approach: start a search for all possible routes --//
            if (!alt)
            {
                newConn.searchAllPaths(resultList, newConn.start, newConn.targets[r]);

                // result RLQI list
                rlqiListTemp = newConn.getRLQIList();
                rlqiListTemp.sort();
                newConn.setRLQIList(rlqiListTemp);

                DBG_Printf(DBG_INFO,"gateway connectivity: %u\n",newConn.getRLQIList().back());
                DBG_Printf(DBG_INFO,"number of routes: %u\n",newConn.getRLQIList().size());

                resultList.clear();
            }
            else
            //-- alternative approach: compute mean lqi of neighbors for each node --//
            {
                if (neighbors.size() == 0)
                {
                    meanLQI = 0;
                }
                else
                {
                    meanLQI = sumLQI / neighbors.size();
                }
                DBG_Printf(DBG_INFO,"sum: %u, neighbors: %i, mean LQI: %u\n",sumLQI,neighbors.size(),meanLQI);
            }

            break;
        }
    }
    rsp.httpStatus = HttpStatusOk;

    // Neighbours to Map

    QVariantMap connectivityMap;
    QVariantMap neighborsMap;
    QVariantMap nbNode;
    quint8 lqi1 = 0;
    quint8 lqi2 = 0;

    for (uint nl = 0; nl < neighborList.size(); nl++)
    {
        if (neighborList[nl].address().ext() != coordinatorAddress)
        {
            LightNode *nl_neighbor = getLightNodeForAddress(neighborList[nl].address().ext());
            if ((nl_neighbor != NULL) && (neighborList[nl].lqi() != 0) && nl_neighbor->isAvailable())
            {
                //lqi value from actual node to his neighbor
                lqi1 = neighborList[nl].lqi();
                //DBG_Printf(DBG_INFO, "LQI %s -> %s = %u\n",qPrintable(lightNode->address().toStringExt()),qPrintable(neighborList.at(nl).address().toStringExt()),lqi1);

                //lqi value from the opposite direction
                DeRestPluginPrivate::nodeVisited oppositeNode = newConn.getNodeWithAddress(neighborList[nl].address().ext());

                for(uint y = 0; y < oppositeNode.node->neighbors().size(); y++)
                {
                    if(oppositeNode.node->neighbors()[y].address().ext() == lightNode->address().ext())
                    {
                        lqi2 = oppositeNode.node->neighbors()[y].lqi();
                        //DBG_Printf(DBG_INFO, "LQI %s -> %s = %u\n",qPrintable(nodeXY.node->address().toStringExt()),qPrintable((nodeXY.node->neighbors().at(y).address().toStringExt())),lqi2);
                        break;
                    }
                }

                if (!alt)
                {
                    //take the lower lqi value
                    //if (lqi1 < lqi2)
                    //take lqi from current node if it is not 0
                    if (lqi1 != 0)
                    {
                        nbNode["connectivity"] = lqi1;
                    }
                    //else if (lqi2 != 0)
                    else
                    {
                        nbNode["connectivity"] = lqi2;
                    }
                }
                else
                {
                    // alternative approach: take the lqi value of actual node
                    nbNode["connectivity"] = lqi1;
                }

                nbNode["name"] = nl_neighbor->name();
                nbNode["reachable"] = nl_neighbor->isAvailable();
                neighborsMap[nl_neighbor->id()] = nbNode;
            }
        }
    }

    //connectivity to Map

    connectivityMap["name"] = lightNode->name();
    connectivityMap["reachable"] = lightNode->isAvailable();
    connectivityMap["extAddress"] = lightNode->address().toStringExt();
    if (!alt)
    {
        connectivityMap["connectivity"] = newConn.getRLQIList().back();
    }
    else
    {
        connectivityMap["connectivity"] = meanLQI;
    }
    connectivityMap["routesToGateway"] = (double)newConn.getRLQIList().size();
    connectivityMap["neighbours"] = neighborsMap;

    updateEtag(lightNode->etag);
    rsp.httpStatus = HttpStatusOk;
    rsp.etag = lightNode->etag;
    rsp.map = connectivityMap;

    return REQ_READY_SEND;
}
