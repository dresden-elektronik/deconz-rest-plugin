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
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "json.h"
#include "connectivity.h"
#include "colorspace.h"

/*! Lights REST API broker.
    \param req - request data
    \param rsp - response data
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::handleLightsApi(const ApiRequest &req, ApiResponse &rsp)
{
    if (req.path[2] != QLatin1String("lights"))
    {
        return REQ_NOT_HANDLED;
    }

    // GET /api/<apikey>/lights
    if ((req.path.size() == 3) && (req.hdr.method() == "GET"))
    {
        return getAllLights(req, rsp);
    }
    // POST /api/<apikey>/lights
    else if ((req.path.size() == 3) && (req.hdr.method() == "POST"))
    {
        return searchNewLights(req, rsp);
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
    // GET /api/<apikey>/lights/<id>/data?maxrecords=<maxrecords>&fromtime=<ISO 8601>
    else if ((req.path.size() == 5) && (req.hdr.method() == "GET") && (req.path[4] == "data"))
    {
        return getLightData(req, rsp);
    }
    // PUT, PATCH /api/<apikey>/lights/<id>/state
    else if ((req.path.size() == 5) && (req.hdr.method() == "PUT" || req.hdr.method() == "PATCH") && (req.path[4] == "state"))
    {
        return setLightState(req, rsp);
    }
    // PUT, PATCH /api/<apikey>/lights/<id>
    else if ((req.path.size() == 4) && (req.hdr.method() == "PUT" || req.hdr.method() == "PATCH"))
    {
        return setLightAttributes(req, rsp);
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

    // handle ETag
    if (req.hdr.hasKey("If-None-Match"))
    {
        QString etag = req.hdr.value("If-None-Match");

        if (gwLightsEtag == etag)
        {
            rsp.httpStatus = HttpStatusNotModified;
            rsp.etag = etag;
            return REQ_READY_SEND;
        }
    }

    std::vector<LightNode>::const_iterator i = nodes.begin();
    std::vector<LightNode>::const_iterator end = nodes.end();

    for (; i != end; ++i)
    {
        if (i->state() == LightNode::StateDeleted)
        {
            continue;
        }

        QVariantMap mnode;
        if (lightToMap(req, &*i, mnode))
        {
            rsp.map[i->id()] = mnode;
        }
    }

    if (rsp.map.isEmpty())
    {
        rsp.str = "{}"; // return empty object
    }

    rsp.etag = gwLightsEtag;

    return REQ_READY_SEND;
}

/*! POST /api/<apikey>/lights
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::searchNewLights(const ApiRequest &req, ApiResponse &rsp)
{
    Q_UNUSED(req);

    if (!isInNetwork())
    {
        rsp.list.append(errorToMap(ERR_NOT_CONNECTED, QLatin1String("/lights"), QLatin1String("Not connected")));
        rsp.httpStatus = HttpStatusServiceUnavailable;
        return REQ_READY_SEND;
    }

    startSearchLights();
    {
        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState[QLatin1String("/lights")] = QLatin1String("Searching for new devices");
        rspItemState[QLatin1String("/lights/duration")] = (double)searchLightsTimeout;
        rspItem[QLatin1String("success")] = rspItemState;
        rsp.list.append(rspItem);
    }

    rsp.httpStatus = HttpStatusOk;

    return REQ_READY_SEND;
}

/*! GET /api/<apikey>/lights/new
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::getNewLights(const ApiRequest &req, ApiResponse &rsp)
{
    Q_UNUSED(req);

    if (!searchLightsResult.isEmpty() &&
        (searchLightsState == SearchLightsActive || searchLightsState == SearchLightsDone))
    {

        rsp.map = searchLightsResult;
    }

    if (searchLightsState == SearchLightsActive)
    {
        rsp.map["lastscan"] = QLatin1String("active");
    }
    else if (searchLightsState == SearchLightsDone)
    {
        rsp.map["lastscan"] = lastLightsScan;
    }
    else
    {
        rsp.map["lastscan"] = QLatin1String("none");
    }

    rsp.httpStatus = HttpStatusOk;
    return REQ_READY_SEND;
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
    const ResourceItem *ix = nullptr;
    const ResourceItem *iy = nullptr;

    for (int i = 0; i < lightNode->itemCount(); i++)
    {
        const ResourceItem *item = lightNode->itemForIndex(static_cast<size_t>(i));
        DBG_Assert(item);

        if (!item->isPublic())
        {
            continue;
        }

        if      (item->descriptor().suffix == RStateOn) { state["on"] = item->toBool(); }
        else if (item->descriptor().suffix == RStateBri) { state["bri"] = static_cast<double>(item->toNumber()); }
        else if (item->descriptor().suffix == RStateHue) { state["hue"] = static_cast<double>(item->toNumber()); }
        else if (item->descriptor().suffix == RStateSat) { state["sat"] = static_cast<double>(item->toNumber()); }
        else if (item->descriptor().suffix == RStateCt) { state["ct"] = static_cast<double>(item->toNumber()); }
        else if (item->descriptor().suffix == RStateColorMode) { state["colormode"] = item->toString(); }
        else if (item->descriptor().suffix == RStateSpeed) { state["speed"] = item->toNumber(); }
        else if (item->descriptor().suffix == RStateX) { ix = item; }
        else if (item->descriptor().suffix == RStateY) { iy = item; }
        else if (item->descriptor().suffix == RStateReachable) { state["reachable"] = item->toBool(); }
        else if (item->descriptor().suffix == RConfigCtMin) { map["ctmin"] = item->toNumber(); }
        else if (item->descriptor().suffix == RConfigCtMax) { map["ctmax"] = item->toNumber(); }
        else if (item->descriptor().suffix == RConfigPowerup) { map["powerup"] = item->toNumber(); }
        else if (item->descriptor().suffix == RConfigPowerOnLevel) { map["poweronlevel"] = item->toNumber(); }
        else if (item->descriptor().suffix == RConfigPowerOnCt) { map["poweronct"] = item->toNumber(); }
        else if (item->descriptor().suffix == RConfigLevelMin) { map["levelmin"] = item->toNumber(); }
        else if (item->descriptor().suffix == RConfigId) { map["configid"] = item->toNumber(); }
    }

    state["alert"] = QLatin1String("none"); // TODO

    if (ix && iy)
    {
        state["effect"] = (lightNode->isColorLoopActive() ? "colorloop" : "none");
        QVariantList xy;
        double colorX = ix->toNumber();
        double colorY = iy->toNumber();
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
        // x = CurrentX / 65536 (CurrentX in the range 0 to 65279 inclusive)
        const double x = round(colorX / 6.5535) / 10000.0; // normalize to 0 .. 1
        const double y = round(colorY / 6.5535) / 10000.0; // normalize to 0 .. 1
        xy.append(x);
        xy.append(y);
        state["xy"] = xy;
    }

    map["uniqueid"] = lightNode->uniqueId();
    map["name"] = lightNode->name();
    map["type"] = lightNode->type();

    // Amazon Echo quirks mode
    if (req.mode == ApiModeEcho)
    {
        // OSRAM plug + Ubisys S1/S2
        if (lightNode->type().startsWith(QLatin1String("On/Off")))
        {
            map["modelid"] = QLatin1String("LWB010");
            map["manufacturername"] = QLatin1String("Philips");
            map["type"] = QLatin1String("Dimmable light");
            state["bri"] = (double)254;
        }
    }

    if (req.path.size() > 2 && req.path[2] == QLatin1String("devices"))
    {
        // don't add in sub device
    }
    else
    {
        if (req.mode != ApiModeEcho)
        {
            map["hascolor"] = lightNode->hasColor();
        }

        map["modelid"] = lightNode->modelId(); // real model id
        map["manufacturername"] = lightNode->manufacturer();
        map["swversion"] = lightNode->swBuildId();
        QString etag = lightNode->etag;
        etag.remove('"'); // no quotes allowed in string
        map["etag"] = etag;
    }

    map["state"] = state;
    return true;
}

/*! GET /api/<apikey>/lights/<id>/data?maxrecords=<maxrecords>&fromtime=<ISO 8601>
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::getLightData(const ApiRequest &req, ApiResponse &rsp)
{
    DBG_Assert(req.path.size() == 5);

    if (req.path.size() != 5)
    {
        return REQ_NOT_HANDLED;
    }

    QString id = req.path[3];
    LightNode *lightNode = getLightNodeForId(id);

    if (!lightNode || (lightNode->state() != LightNode::StateNormal))
    {
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/lights/%1/").arg(id), QString("resource, /lights/%1/, not available").arg(id)));
        rsp.httpStatus = HttpStatusNotFound;
        return REQ_READY_SEND;
    }

    bool ok;
    QUrl url(req.hdr.url());
    QUrlQuery query(url);

    const int maxRecords = query.queryItemValue(QLatin1String("maxrecords")).toInt(&ok);
    if (!ok || maxRecords <= 0)
    {
        rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/maxrecords"), QString("invalid value, %1, for parameter, maxrecords").arg(query.queryItemValue("maxrecords"))));
        rsp.httpStatus = HttpStatusNotFound;
        return REQ_READY_SEND;
    }

    QString t = query.queryItemValue(QLatin1String("fromtime"));
    QDateTime dt = QDateTime::fromString(t, QLatin1String("yyyy-MM-ddTHH:mm:ss"));
    if (!dt.isValid())
    {
        rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/fromtime"), QString("invalid value, %1, for parameter, fromtime").arg(query.queryItemValue("fromtime"))));
        rsp.httpStatus = HttpStatusNotFound;
        return REQ_READY_SEND;
    }

    const qint64 fromTime = dt.toMSecsSinceEpoch() / 1000;

    openDb();
    loadLightDataFromDb(lightNode, rsp.list, fromTime, maxRecords);
    closeDb();

    if (rsp.list.isEmpty())
    {
        rsp.str = QLatin1String("[]"); // return empty list
    }

    rsp.httpStatus = HttpStatusOk;

    return REQ_READY_SEND;
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
        return REQ_NOT_HANDLED;
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

/*! Helper to generate a new task with new task and req id based on a reference */
static void copyTaskReq(TaskItem &a, TaskItem &b)
{
    b.req.dstAddress() = a.req.dstAddress();
    b.req.setDstAddressMode(a.req.dstAddressMode());
    b.req.setSrcEndpoint(a.req.srcEndpoint());
    b.req.setDstEndpoint(a.req.dstEndpoint());
    b.req.setRadius(a.req.radius());
    b.req.setTxOptions(a.req.txOptions());
    b.req.setSendDelay(a.req.sendDelay());
    b.transitionTime = a.transitionTime;
    b.onTime = a.onTime;
    b.lightNode = a.lightNode;
}

/*! PUT, PATCH /api/<apikey>/lights/<id>/state
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::setLightState(const ApiRequest &req, ApiResponse &rsp)
{
    TaskItem taskRef;
    QString id = req.path[3];
    taskRef.lightNode = getLightNodeForId(id);
    uint hue = UINT_MAX;
    uint sat = UINT_MAX;

    if (req.sock)
    {
        userActivity();
    }

    if (!taskRef.lightNode || taskRef.lightNode->state() == LightNode::StateDeleted)
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/lights/%1").arg(id), QString("resource, /lights/%1, not available").arg(id)));
        return REQ_READY_SEND;
    }

    rsp.httpStatus = HttpStatusOk;

    if (!taskRef.lightNode->isAvailable())
    {
        rsp.httpStatus = HttpStatusOk;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/lights/%1").arg(id), QString("resource, /lights/%1, not available").arg(id)));
        return REQ_READY_SEND;
    }

    // set destination parameters
    taskRef.req.dstAddress() = taskRef.lightNode->address();
    taskRef.req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
    taskRef.req.setDstEndpoint(taskRef.lightNode->haEndpoint().endpoint());
    taskRef.req.setSrcEndpoint(getSrcEndpoint(taskRef.lightNode, taskRef.req));
    taskRef.req.setDstAddressMode(deCONZ::ApsExtAddress);

    bool ok;
    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();

    if (!ok || map.isEmpty())
    {
        rsp.list.append(errorToMap(ERR_INVALID_JSON, QString("/lights/%1/state").arg(id), QString("body contains invalid JSON")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    // TODO: check for valid attributes in body
    bool isOn = false;
    bool hasOn = map.contains("on");
    bool hasBri = map.contains("bri");
    bool hasHue = map.contains("hue");
    bool hasSat = map.contains("sat");
    bool hasXy = map.contains("xy");
    bool hasCt = map.contains("ct");
    bool hasCtInc = map.contains("ct_inc");
    bool hasBriInc = map.contains("bri_inc");
    bool hasEffect = map.contains("effect");
    bool hasEffectColorLoop = false;
    bool hasAlert = map.contains("alert");
    bool hasWrap = map.contains("wrap");

    {
        ResourceItem *item = taskRef.lightNode->item(RStateOn);
        DBG_Assert(item != nullptr);
        isOn = item ? item->toBool() : false;
    }

    if (taskRef.lightNode->modelId() == QLatin1String("FLS-PP")) // old FLS-PP
    {
        hasXy = false;
    }

    // transition time
    if (map.contains("transitiontime"))
    {
        uint tt = map["transitiontime"].toUInt(&ok);

        if (ok && tt < 0xFFFFUL)
        {
            taskRef.transitionTime = tt;
        }
    }
    if (map.contains("ontime"))
    {
        uint ot = map["ontime"].toUInt(&ok);

        if (ok && ot < 0xFFFFUL)
        {
            taskRef.onTime = ot;
        }
    }

    // FIXME temporary workaround to support window_covering
    bool isWindowCoveringDevice = false;
    if (taskRef.lightNode->type() == QLatin1String("Window covering device"))
    {
    	isWindowCoveringDevice = true;
    }

    // on/off
    if (hasOn)
    {
        if (map["on"].type() == QVariant::Bool)
        {
            isOn = map["on"].toBool();

            if (!isOn && taskRef.lightNode->isColorLoopActive())
            {
                TaskItem task;
                copyTaskReq(taskRef, task);
                addTaskSetColorLoop(task, false, 15);
                taskRef.lightNode->setColorLoopActive(false); // deactivate colorloop if active
            }

            TaskItem task;
            copyTaskReq(taskRef, task);
            //FIXME workaround window_convering
            if (isWindowCoveringDevice
            		&& addTaskWindowCovering(task, isOn ? 0x01 /*down*/ : 0x00 /*up*/, 0, 0))
            {
                QVariantMap rspItem;
                QVariantMap rspItemState;
                rspItemState[QString("/lights/%1/state/on").arg(id)] = isOn;
                rspItem["success"] = rspItemState;
                rsp.list.append(rspItem);
                taskToLocalData(task);
            } // FIXME end workaround window_covering
            else if (isOn && taskRef.onTime > 0 && addTaskSetOnOff(task, ONOFF_COMMAND_ON_WITH_TIMED_OFF, taskRef.onTime))
            {
                QVariantMap rspItem;
                QVariantMap rspItemState;
                rspItemState[QString("/lights/%1/state/on").arg(id)] = isOn;
                rspItem["success"] = rspItemState;
                rsp.list.append(rspItem);
                taskToLocalData(task);
            }
            else if (hasBri ||
                // map.contains("transitiontime") || // FIXME: use bri if transitionTime is given
                addTaskSetOnOff(task, isOn ? ONOFF_COMMAND_ON : ONOFF_COMMAND_OFF, 0)) // onOff task only if no bri or transitionTime is given
            {
                QVariantMap rspItem;
                QVariantMap rspItemState;
                rspItemState[QString("/lights/%1/state/on").arg(id)] = isOn;
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
            if (!isOn)
            {
                bri = 0; // assume the caller wanted to switch the light off
            }
            else if (isOn && (bri == 0))
            {
                bri = 1; // don't turn off light is on is true
            }
        }

        //FIXME workaround window_covering
        if (isWindowCoveringDevice)
        {
        	if ((map["bri"].type() == QVariant::String) && map["bri"].toString() == "stop")
        	{
        		TaskItem task;
        		copyTaskReq(taskRef, task);
        		if (addTaskWindowCovering(task, 0x02 /*stop motion*/, 0, 0))
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
        		TaskItem task;
        		copyTaskReq(taskRef, task);
        		uint8_t moveToPct = 0x00;
        		moveToPct = bri * 100 / 255;  // Percent 0 - 100 (0x00 - 0x64)
                if (taskRef.lightNode->modelId().startsWith(QLatin1String("lumi.curtain")))
                {
                    moveToPct = 100 - moveToPct;
                }
        		if (addTaskWindowCovering(task, 0x05 /*move to Lift Percent*/, 0, moveToPct))
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
        } // FIXME end workaround window_covering
        else if (!isOn && !hasOn)
        {
            rsp.list.append(errorToMap(ERR_DEVICE_OFF, QString("/lights/%1").arg(id), QString("parameter, /lights/%1/bri, is not modifiable. Device is set to off.").arg(id)));
        }
        else if ((map["bri"].type() == QVariant::String) && map["bri"].toString() == "stop")
        {
            TaskItem task;
            copyTaskReq(taskRef, task);
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
            TaskItem task;
            copyTaskReq(taskRef, task);
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

        if (!isOn)
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
                        taskRef.lightNode->setColorLoopSpeed(speed);
                    }
                    else
                    {
                        rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1/state/colorloopspeed").arg(id), QString("invalid value, %1, for parameter, colorloopspeed").arg(map["colorloopspeed"].toString())));
                    }
                }
            }

            TaskItem task;
            copyTaskReq(taskRef, task);
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

        if (!isOn)
        {
            rsp.list.append(errorToMap(ERR_DEVICE_OFF, QString("/lights/%1").arg(id), QString("parameter, /lights/%1/hue, is not modifiable. Device is set to off.").arg(id)));
        }
        else if (ok && (map["hue"].type() == QVariant::Double) && (hue2 <= MAX_ENHANCED_HUE))
        {
            hue = hue2;
            TaskItem task;
            copyTaskReq(taskRef, task);
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
                if (hue > MAX_ENHANCED_HUE_Z)
                {
                    hue = MAX_ENHANCED_HUE_Z;
                }
                task.enhancedHue = hue;
                task.taskType = TaskSetEnhancedHue;
            }

            if (!hasXy && !hasSat)
            {
                ResourceItem *item = task.lightNode->item(RStateSat);
                double r, g, b;
                double x, y;
                double h = ((360.0 / 65535.0) * hue);
                double s = (item ? item->toNumber() : 0) / 255.0;
                double v = 1.0;

                Hsv2Rgb(&r, &g, &b, h, s, v);
                Rgb2xy(&x, &y, r, g, b);

                if (x < 0) { x = 0; }
                else if (x > 1) { x = 1; }

                if (y < 0) { y = 0; }
                else if (y > 1) { y = 1; }

                DBG_Printf(DBG_INFO, "x: %f, y: %f\n", x, y);
                x *= 65535.0;
                y *= 65535.0;

                if (x > 65279) { x = 65279; }
                else if (x < 1) { x = 1; }

                if (y > 65279) { y = 65279; }
                else if (y < 1) { y = 1; }

                item = task.lightNode->item(RStateX);
                if (item && item->toNumber() != static_cast<quint16>(x))
                {
                    item->setValue(static_cast<quint16>(x));
                    Event e(RLights, RStateX, task.lightNode->id(), item);
                    enqueueEvent(e);
                }

                item = task.lightNode->item(RStateY);
                if (item && item->toNumber() != static_cast<quint16>(y))
                {
                    item->setValue(static_cast<quint16>(y));
                    Event e(RLights, RStateY, task.lightNode->id(), item);
                    enqueueEvent(e);
                }
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
                taskToLocalData(task);
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

        //FIXME workaround window_covering
        if (isWindowCoveringDevice)
        {
        	if (ok && (map["sat"].type() == QVariant::Double) && (sat2 < 256))
        	{
        		TaskItem task;
        		copyTaskReq(taskRef, task);
        		uint8_t moveToPct = 0x00;
        		moveToPct = sat2 * 100 / 255;  // Percent 0 - 100 (0x00 - 0x64)
        		if (addTaskWindowCovering(task, 0x08 /*move to Tilt Percent*/, 0, moveToPct))
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
        } //FIXME workaround window_covering
        else if (!isOn)
        {
            rsp.list.append(errorToMap(ERR_DEVICE_OFF, QString("/lights/%1").arg(id), QString("parameter, /lights/%1/sat, is not modifiable. Device is set to off.").arg(id)));
        }
        else if (ok && (map["sat"].type() == QVariant::Double) && (sat2 < 256))
        {
            if (sat2 == 255)
            {
                sat2 = 254; // max valid value for level attribute
            }

            TaskItem task;
            copyTaskReq(taskRef, task);
            sat = sat2;
            task.sat = sat;
            task.taskType = TaskSetSat;

            if (!hasXy && !hasHue)
            {
                ResourceItem *item = task.lightNode->item(RStateHue);
                double r, g, b;
                double x, y;
                double h = ((360.0 / 65535.0) * (item ? item->toNumber() : 0));
                double s = sat / 255.0;
                double v = 1.0;

                Hsv2Rgb(&r, &g, &b, h, s, v);
                Rgb2xy(&x, &y, r, g, b);

                if (x < 0) { x = 0; }
                else if (x > 1) { x = 1; }

                if (y < 0) { y = 0; }
                else if (y > 1) { y = 1; }

                x *= 65535.0;
                y *= 65535.0;

                if (x > 65279) { x = 65279; }
                else if (x < 1) { x = 1; }

                if (y > 65279) { y = 65279; }
                else if (y < 1) { y = 1; }

                item = task.lightNode->item(RStateX);
                if (item && item->toNumber() != static_cast<quint16>(x))
                {
                    item->setValue(static_cast<quint16>(x));
                    Event e(RLights, RStateX, task.lightNode->id(), item);
                    enqueueEvent(e);
                }

                item = task.lightNode->item(RStateY);
                if (item && item->toNumber() != static_cast<quint16>(y))
                {
                    item->setValue(static_cast<quint16>(y));
                    Event e(RLights, RStateY, task.lightNode->id(), item);
                    enqueueEvent(e);
                }
            }

            if (hasXy || hasCt
               || (!hasEffectColorLoop && hasHue && (hue != UINT_MAX)) // merge later to set hue and saturation
               || addTaskSetSaturation(task, sat)) // will only be evaluated if no hue, xy, ct is set
            {
                QVariantMap rspItem;
                QVariantMap rspItemState;
                rspItemState[QString("/lights/%1/state/sat").arg(id)] = map["sat"];
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
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1/state/sat").arg(id), QString("invalid value, %1, for parameter, sat").arg(map["sat"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }

    // ct_inc
    if (hasCtInc)
    {
        ResourceItem *item = taskRef.lightNode->item(RStateCt);

        int ct_inc = map["ct_inc"].toInt(&ok);

        if (!item)
        {
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/lights/%1").arg(id), QString("parameter, /lights/%1/ct_inc, is not available.").arg(id)));
        }
        else if (!isOn)
        {
            rsp.list.append(errorToMap(ERR_DEVICE_OFF, QString("/lights/%1").arg(id), QString("parameter, /lights/%1/ct_inc, is not modifiable. Device is set to off.").arg(id)));
        }
        else if (hasCt)
        {
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_MODIFIEABLE, QString("/lights/%1").arg(id), QString("parameter, /lights/%1/ct_inc, is not modifiable. ct was specified.").arg(id)));
        }
        else if (ok && (map["ct_inc"].type() == QVariant::Double) && (ct_inc >= -65534 && ct_inc <= 65534))
        {
            TaskItem task;
            copyTaskReq(taskRef, task);
            task.inc = ct_inc;
            task.taskType = TaskIncColorTemperature;

            if (addTaskIncColorTemperature(task, ct_inc)) // will only be evaluated if no ct is set
            {
                taskToLocalData(task);
                QVariantMap rspItem;
                QVariantMap rspItemState;
                rspItemState[QString("/lights/%1/state/ct").arg(id)] = item->toNumber();
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
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1/state/ct_inc").arg(id), QString("invalid value, %1, for parameter, ct_inc").arg(map["ct_inc"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }

    if (hasBriInc && !hasBri)
    {
        ResourceItem *item = taskRef.lightNode->item(RStateBri);

        int briInc = map["bri_inc"].toInt(&ok);

        if (ok && hasWrap && map["wrap"].type() == QVariant::Bool && map["wrap"].toBool() == true) {
            const int bri = static_cast<int>(item->toNumber());

            if (briInc < 0 && bri + briInc <= -briInc)
            {
                briInc = 254;
            }
            else if(briInc > 0 && bri + briInc >= 254)
            {
                briInc = -254;
            }
        }

        if (!item)
        {
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/lights/%1").arg(id), QString("parameter, /lights/%1/bri_inc, is not available.").arg(id)));
        }
        //FIXME workaround window_covering
        else if (isWindowCoveringDevice)
        {
            if (ok && (map["bri_inc"].type() == QVariant::Double) && (briInc == 0))
        	{
        		TaskItem task;
        		copyTaskReq(taskRef, task);
        		if (addTaskWindowCovering(task, 0x02 /*stop motion*/, 0, 0))
        		{
        			QVariantMap rspItem;
        			QVariantMap rspItemState;
        			rspItemState[QString("/lights/%1/state/bri").arg(id)] = item->toNumber();
        			rspItem["success"] = rspItemState;
        			rsp.list.append(rspItem);
        			taskToLocalData(task);
        		}
        		else
        		{
        			rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
        		}
        	}
        } // FIXME end workaround window_covering
        else if (!isOn)
        {
            rsp.list.append(errorToMap(ERR_DEVICE_OFF, QString("/lights/%1").arg(id), QString("parameter, /lights/%1/bri, is not modifiable. Device is set to off.").arg(id)));
        }
        else if (ok && (map["bri_inc"].type() == QVariant::Double) && (briInc >= -254 && briInc <= 254))
        {
            TaskItem task;
            copyTaskReq(taskRef, task);
            task.inc = briInc;
            task.taskType = TaskIncBrightness;

            if (addTaskIncBrightness(task, briInc))
            {
                taskToLocalData(task);
                QVariantMap rspItem;
                QVariantMap rspItemState;
                rspItemState[QString("/lights/%1/state/bri").arg(id)] = item->toNumber();
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
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1/state/bri_inc").arg(id), QString("invalid value, %1, for parameter, bri_inc").arg(map["bri_inc"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }

    // hue and saturation
    if (hasHue && hasSat && !hasXy && !hasCt)
    {
        if (!isOn)
        {
            // no error here
        }
        else if (!hasEffectColorLoop && (hue != UINT_MAX) && (sat != UINT_MAX))
        {
            // need 8 bit hue
            qreal f = (qreal)hue / 182.04444;

            f /= 360.0;

            if (f > 1.0)
            {
                f = 1.0;
            }

            hue = f * 254.0;

            DBG_Printf(DBG_INFO, "hue: %u, sat: %u\n", hue, sat);

            double r, g, b;
            double x, y;
            double h = ((360.0 / 65535.0) * hue);
            double s = sat / 254.0;
            double v = 1.0;

            Hsv2Rgb(&r, &g, &b, h, s, v);
            Rgb2xy(&x, &y, r, g, b);
            if (x < 0) { x = 0; }
            else if (x > 1) { x = 1; }

            if (y < 0) { y = 0; }
            else if (y > 1) { y = 1; }

            TaskItem task;
            copyTaskReq(taskRef, task);
            DBG_Printf(DBG_INFO, "x: %f, y: %f\n", x, y);
            task.lightNode->setColorXY(static_cast<quint16>(x * 65535.0), static_cast<quint16>(y * 65535.0));

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

        if (!isOn)
        {
            rsp.list.append(errorToMap(ERR_DEVICE_OFF, QString("/lights/%1").arg(id), QString("parameter, /lights/%1/xy, is not modifiable. Device is set to off.").arg(id)));
        }
        else if ((ls.size() == 2) && (ls[0].type() == QVariant::Double) && (ls[1].type() == QVariant::Double))
        {
            double x = ls[0].toDouble(&ok);
            double y = ok ? ls[1].toDouble(&ok) : 0;
            TaskItem task;
            copyTaskReq(taskRef, task);

            if (!ok || (x < 0) || (x > 1) || (y < 0) || (y > 1))
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

        if (!isOn)
        {
            rsp.list.append(errorToMap(ERR_DEVICE_OFF, QString("/lights/%1").arg(id), QString("parameter, /lights/%1/ct, is not modifiable. Device is set to off.").arg(id)));
        }
        else if (ok && (map["ct"].type() == QVariant::Double))
        {
            TaskItem task;
            copyTaskReq(taskRef, task);
            if (hasXy || hasEffectColorLoop ||
                addTaskSetColorTemperature(task, ct)) // will only be evaluated if no xy and color loop is set
            {
                QVariantMap rspItem;
                QVariantMap rspItemState;
                rspItemState[QString("/lights/%1/state/ct").arg(id)] = map["ct"];
                rspItem["success"] = rspItemState;
                rsp.list.append(rspItem);
                if (task.taskType == TaskSetColorTemperature)
                {
                    taskToLocalData(task); // get through reading
                }
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
        TaskItem task;
        copyTaskReq(taskRef, task);
        QString alert = map["alert"].toString();
        bool isWarningDevice = taskRef.lightNode->type() == QLatin1String("Warning device");

        bool isSmokeDetector = false;
        if (taskRef.lightNode->modelId() == QLatin1String("902010/24")  // Bitron Smoke Detector with siren
                || taskRef.lightNode->modelId() == QLatin1String("SMSZB-120"))  // Develco smoke sensor
        {
            isSmokeDetector = true;
            taskRef.lightNode->rx();  // otherwise device is marked as zombie and task is dropped
        }

        if (alert == "none")
        {
            if (isWarningDevice)
            {
                task.taskType = TaskWarning;
                task.options = 0x00; // Warning mode 0 (no warning), No strobe
                task.duration = 0;
            }
            else
            {
                task.taskType = TaskIdentify;
                task.identifyTime = 0;
            }
        }
        else if (alert == "select")
        {
            if (isWarningDevice && isSmokeDetector)
            {
                task.taskType = TaskWarning;
                task.options = 0x12; // Warning mode 2 (fire), strobe
                task.duration = 1;
            }
            else if (isWarningDevice)
            {
                task.taskType = TaskWarning;
                task.options = 0x14; // Warning mode 1 (burglar), Strobe
                task.duration = 1;
            }
            else
            {
                task.taskType = TaskIdentify;
                task.identifyTime = 2;    // Hue lights don't react to 1.
            }
        }
        else if (alert == "lselect")
        {
            if (isWarningDevice && isSmokeDetector)
            {
                task.taskType = TaskWarning;
                task.options = 0x12; // Warning mode 2 (fire), strobe
                task.duration = 300;
            }
            else if (isWarningDevice)
            {
                task.taskType = TaskWarning;
                task.options = 0x14; // Warning mode 1 (burglar), Strobe
                task.duration = 300;
            }
            else
            {
                task.taskType = TaskIdentify;
                task.identifyTime = 15;   // Default for Philips Hue bridge
            }
        }
        else if (alert == "blink")
        {
            if (isWarningDevice)
            {
                task.taskType = TaskWarning;
                task.options = 0x04; // Warning mode 0 (no warning), Strobe
                task.duration = 300;
            }
            else
            {
                task.taskType = TaskTriggerEffect;
                task.effectIdentifier = 0x00;
            }
        }
        else if (alert == "breathe" && !isWarningDevice)
        {
            task.taskType = TaskTriggerEffect;
            task.effectIdentifier = 0x01;
        }
        else if (alert == "okay" && !isWarningDevice)
        {
            task.taskType = TaskTriggerEffect;
            task.effectIdentifier = 0x02;
        }
        else if (alert == "channelchange" && !isWarningDevice)
        {
            task.taskType = TaskTriggerEffect;
            task.effectIdentifier = 0x0b;
        }
        else if (alert == "finish" && !isWarningDevice)
        {
            task.taskType = TaskTriggerEffect;
            task.effectIdentifier = 0xfe;
        }
        else if (alert == "stop" && !isWarningDevice)
        {
            task.taskType = TaskTriggerEffect;
            task.effectIdentifier = 0xff;
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1/state/alert").arg(id), QString("invalid value, %1, for parameter, alert").arg(map["alert"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

        taskToLocalData(task);

        if ((task.taskType == TaskIdentify && addTaskIdentify(task, task.identifyTime)) ||
            (task.taskType == TaskTriggerEffect && addTaskTriggerEffect(task, task.effectIdentifier)) ||
            (task.taskType == TaskWarning && addTaskWarning(task, task.options, task.duration)))
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

    if (taskRef.lightNode)
    {
        updateLightEtag(taskRef.lightNode);
        rsp.etag = taskRef.lightNode->etag;
    }

    processTasks();

    return REQ_READY_SEND;
}

/*! PUT, PATCH /api/<apikey>/lights/<id>
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::setLightAttributes(const ApiRequest &req, ApiResponse &rsp)
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
        QString name = map["name"].toString().trimmed();

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

                updateLightEtag(lightNode);
                lightNode->setNeedSaveDatabase(true);
                queSaveDb(DB_LIGHTS, DB_SHORT_SAVE_DELAY);

                Event e(RLights, RAttrName, lightNode->id(), lightNode->item(RAttrName));
                enqueueEvent(e);
            }

            Q_Q(DeRestPlugin);
            q->nodeUpdated(lightNode->address().ext(), QLatin1String("name"), name);

            if (lightNode->modelId().startsWith(QLatin1String("FLS-NB"))) // sync names
            {
                for (Sensor &s : sensors)
                {
                    if (s.address().ext() == lightNode->address().ext() &&
                        s.name() != lightNode->name())
                    {
                        updateSensorEtag(&s);
                        s.setName(lightNode->name());
                        s.setNeedSaveDatabase(true);
                        queSaveDb(DB_SENSORS, DB_SHORT_SAVE_DELAY);
                    }
                }
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

    // powerup options
    if (map.contains("powerup"))
    {
        ResourceItem *item = lightNode->item(RConfigPowerup);

        if (!item)
        {
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/lights/%1").arg(id), QString("parameter, /lights/%1/powerup, is not available").arg(id)));
            rsp.httpStatus = HttpStatusNotFound;
            return REQ_READY_SEND;
        }

        if (item->setValue(map["powerup"]))
        {
            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/lights/%1/powerup").arg(id)] = map["powerup"];
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);
            rsp.etag = lightNode->etag;

            if (item->lastSet() == item->lastChanged())
            {
                Event e(RLights, RConfigPowerup, lightNode->id(), item);
                enqueueEvent(e);
                lightNode->setNeedSaveDatabase(true);
                queSaveDb(DB_LIGHTS, DB_SHORT_SAVE_DELAY);
            }

            return REQ_READY_SEND;
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1/powerup").arg(id), QString("invalid value, %1, for parameter powerup").arg(map["powerup"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }

    /*else
    {
        rsp.list.append(errorToMap(ERR_MISSING_PARAMETER, QString("/lights/%1").arg(id), QString("missing parameters in body")));
        return REQ_READY_SEND;
    }*/

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
        return REQ_NOT_HANDLED;
    }

    const QString &id = req.path[3];

    LightNode *lightNode = getLightNodeForId(id);

    if (!lightNode || lightNode->state() == LightNode::StateDeleted)
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

    if (lightNode->state() != LightNode::StateDeleted)
    {
        lightNode->setState(LightNode::StateDeleted);
        lightNode->setNeedSaveDatabase(true);
    }

    {
        Q_Q(DeRestPlugin);
        q->nodeUpdated(lightNode->address().ext(), QLatin1String("deleted"), QLatin1String(""));
    }

    updateLightEtag(lightNode);
    queSaveDb(DB_LIGHTS | DB_GROUPS | DB_SCENES, DB_SHORT_SAVE_DELAY);

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
        return REQ_NOT_HANDLED;
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
        return REQ_NOT_HANDLED;
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
            lightNode->setNeedSaveDatabase(true);
        }
    }

    updateLightEtag(lightNode);
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
        return REQ_NOT_HANDLED;
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
            LightNode *nl_neighbor = getLightNodeForAddress(neighborList[nl].address());
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

    updateLightEtag(lightNode);
    rsp.httpStatus = HttpStatusOk;
    rsp.etag = lightNode->etag;
    rsp.map = connectivityMap;

    return REQ_READY_SEND;
}

void DeRestPluginPrivate::handleLightEvent(const Event &e)
{
    DBG_Assert(e.resource() == RLights);
    DBG_Assert(e.what() != nullptr);

    LightNode *lightNode = getLightNodeForId(e.id());

    if (!lightNode)
    {
        return;
    }
    const QDateTime now = QDateTime::currentDateTime();

    // push state updates through websocket
    if (strncmp(e.what(), "state/", 6) == 0)
    {
        ResourceItem *item = lightNode->item(e.what());
        if (item)
        {

            if (lightNode->lastStatePush.isValid() && item->lastSet() < lightNode->lastStatePush)
            {
                DBG_Printf(DBG_INFO_L2, "discard light state push for %s: %s (already pushed)\n", qPrintable(e.id()), e.what());
                webSocketServer->flush(); // force transmit send buffer
                return; // already pushed
            }

            QVariantMap map;
            map["t"] = QLatin1String("event");
            map["e"] = QLatin1String("changed");
            map["r"] = QLatin1String("lights");
            map["id"] = e.id();
            map["uniqueid"] = lightNode->uniqueId();
            QVariantMap state;
            ResourceItem *ix = nullptr;
            ResourceItem *iy = nullptr;
            QVariantList xy;

            for (int i = 0; i < lightNode->itemCount(); i++)
            {
                item = lightNode->itemForIndex(i);
                const ResourceItemDescriptor &rid = item->descriptor();

                if (strncmp(rid.suffix, "state/", 6) == 0)
                {
                    const char *key = item->descriptor().suffix + 6;

                    if (rid.suffix == RStateX)
                    {
                        ix = item;
                    }
                    else if (rid.suffix == RStateY)
                    {
                        iy = item;
                    }
                    else if (item->lastSet().isValid() && (gwWebSocketNotifyAll || (item->lastChanged().isValid() && item->lastChanged() >= lightNode->lastStatePush)))
                    {
                        state[key] = item->toVariant();
                    }
                }
            }

            if (ix && ix->lastSet().isValid() && iy && iy->lastSet().isValid())
            {
                if (gwWebSocketNotifyAll ||
                    (ix->lastChanged().isValid() && ix->lastChanged() >= lightNode->lastStatePush) ||
                    (iy->lastChanged().isValid() && iy->lastChanged() >= lightNode->lastStatePush))
                  {
                      xy.append(round(ix->toNumber() / 6.5535) / 10000.0);
                      xy.append(round(iy->toNumber() / 6.5535) / 10000.0);
                      state["xy"] = xy;
                  }
            }

            if (!state.isEmpty())
            {
                map["state"] = state;
                webSocketServer->broadcastTextMessage(Json::serialize(map));
                lightNode->lastStatePush = now;
            }

            if ((e.what() == RStateOn || e.what() == RStateReachable) && !lightNode->groups().empty())
            {
                std::vector<GroupInfo>::const_iterator i = lightNode->groups().begin();
                std::vector<GroupInfo>::const_iterator end = lightNode->groups().end();
                for (; i != end; ++i)
                {
                    if (i->state == GroupInfo::StateInGroup)
                    {
                        Event e(RGroups, REventCheckGroupAnyOn, int(i->id));
                        enqueueEvent(e);
                    }
                }
            }
        }
    }
    else if (e.what() == RAttrName)
    {
        QVariantMap map;
        map["t"] = QLatin1String("event");
        map["e"] = QLatin1String("changed");
        map["r"] = QLatin1String("lights");
        map["id"] = e.id();
        map["uniqueid"] = lightNode->uniqueId();

        if (e.what() == RAttrName) // new attributes might be added in future
        {
            map["name"] = lightNode->name();
        }
        webSocketServer->broadcastTextMessage(Json::serialize(map));
    }
    else if (e.what() == REventAdded)
    {
        QVariantMap res;
        res["name"] = lightNode->name();
        searchLightsResult[lightNode->id()] = res;

        QVariantMap map;
        map["t"] = QLatin1String("event");
        map["e"] = QLatin1String("added");
        map["r"] = QLatin1String("lights");
        map["id"] = e.id();
        map["uniqueid"] = lightNode->uniqueId();

        webSocketServer->broadcastTextMessage(Json::serialize(map));
    }
    else if (e.what() == REventDeleted)
    {
        QVariantMap map;
        map["t"] = QLatin1String("event");
        map["e"] = QLatin1String("deleted");
        map["r"] = QLatin1String("lights");
        map["id"] = e.id();
        map["uniqueid"] = lightNode->uniqueId();

        webSocketServer->broadcastTextMessage(Json::serialize(map));
    }
}

/*! Starts the search for new lights.
 */
void DeRestPluginPrivate::startSearchLights()
{
    if (searchLightsState == SearchLightsIdle || searchLightsState == SearchLightsDone)
    {
        pollNodes.clear();
        searchLightsResult.clear();
        lastLightsScan = QDateTime::currentDateTimeUtc().toString(QLatin1String("yyyy-MM-ddTHH:mm:ss"));
        QTimer::singleShot(1000, this, SLOT(searchLightsTimerFired()));
        searchLightsState = SearchLightsActive;
    }
    else
    {
        DBG_Assert(searchLightsState == SearchLightsActive);
    }

    searchLightsTimeout = gwNetworkOpenDuration;
    gwPermitJoinResend = searchLightsTimeout;
    if (!resendPermitJoinTimer->isActive())
    {
        resendPermitJoinTimer->start(100);
    }
}

/*! Handler for search lights active state.
 */
void DeRestPluginPrivate::searchLightsTimerFired()
{
    if (gwPermitJoinResend == 0)
    {
        if (gwPermitJoinDuration == 0)
        {
            searchLightsTimeout = 0; // done
        }
    }

    if (searchLightsTimeout > 0)
    {
        searchLightsTimeout--;
        QTimer::singleShot(1000, this, SLOT(searchLightsTimerFired()));
    }

    if (searchLightsTimeout == 0)
    {
        searchLightsState = SearchLightsDone;
    }
}
