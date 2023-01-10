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
#include "database.h"
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "device_descriptions.h"
#include "json.h"
#include "connectivity.h"
#include "colorspace.h"
#include "product_match.h"

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
    // PUT, PATCH /api/<apikey>/lights/<id>/config
    else if ((req.path.size() == 5) && (req.hdr.method() == "PUT" || req.hdr.method() == "PATCH") && (req.path[4] == "config"))
    {
        return setLightConfig(req, rsp);
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
    if (req.hdr.hasKey(QLatin1String("If-None-Match")))
    {
        QString etag = req.hdr.value(QLatin1String("If-None-Match"));

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
    if (!isInNetwork())
    {
        rsp.list.append(errorToMap(ERR_NOT_CONNECTED, QLatin1String("/lights"), QLatin1String("Not connected")));
        rsp.httpStatus = HttpStatusServiceUnavailable;
        return REQ_READY_SEND;
    }

    permitJoinApiKey = req.apikey();
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

static void toXy(double x,  double y, QVariantList &xy)
{
    if (x > 0xFEFF) x = 0xFEFF;
    if (y > 0xFEFF) y = 0xFEFF;
    xy.append(round(x / 6.5535) / 10000.0);
    xy.append(round(y / 6.5535) / 10000.0);
}

/*! Put all parameters in a map for later json serialization.
    \return true - on success
            false - on error
 */
bool DeRestPluginPrivate::lightToMap(const ApiRequest &req, const LightNode *lightNode, QVariantMap &attr)
{
    Q_UNUSED(req);

    if (!lightNode)
    {
        return false;
    }

    QVariantMap capabilities;
    QVariantMap capabilitiesBri;
    QVariantMap capabilitiesColor;
    const ResourceItem *icc = nullptr;
    QVariantMap capabilitiesColorCt;
    QVariantMap capabilitiesColorGradient;
    QVariantMap capabilitiesColorXy;
    const ResourceItem *ibluex = nullptr;
    const ResourceItem *ibluey = nullptr;
    const ResourceItem *igreenx = nullptr;
    const ResourceItem *igreeny = nullptr;
    const ResourceItem *iredx = nullptr;
    const ResourceItem *iredy = nullptr;
    QStringList effectList = RStateEffectValues;

    QVariantMap config;
    bool groups = true;
    QVariantMap configBri;
    QVariantMap configColor;
    QVariantMap configColorCt;
    QVariantMap configColorGradient;
    QVariantMap configColorXy;
    const ResourceItem *isx = nullptr;
    const ResourceItem *isy = nullptr;
    QVariantMap configOn;

    QVariantMap state;
    const ResourceItem *ialert = nullptr;
    const QStringList *capabilitiesAlerts = &RStateAlertValues;
    const ResourceItem *ix = nullptr;
    const ResourceItem *iy = nullptr;

    for (int i = 0; i < lightNode->itemCount(); i++)
    {
        const ResourceItem *item = lightNode->itemForIndex(static_cast<size_t>(i));
        DBG_Assert(item);
        const ResourceItemDescriptor &rid = item->descriptor();

        if      (rid.suffix == RAttrConfigId) { attr["configid"] = item->toNumber(); }
        else if (rid.suffix == RAttrLastAnnounced) { attr["lastannounced"] = item->toString(); }
        else if (rid.suffix == RAttrLastSeen) { attr["lastseen"] = item->toString(); }
        else if (rid.suffix == RAttrLevelMin) { attr["levelmin"] = item->toNumber(); }
        else if (rid.suffix == RAttrName) { attr["name"] = item->toString(); }
        else if (rid.suffix == RAttrPowerOnLevel) { attr["poweronlevel"] = item->toNumber(); }
        else if (rid.suffix == RAttrPowerOnCt) { attr["poweronct"] = item->toNumber(); }
        else if (rid.suffix == RAttrPowerup) { attr["powerup"] = item->toNumber(); }
        else if (rid.suffix == RAttrProductId) { attr["productid"] = item->toString(); }
        else if (rid.suffix == RAttrProductName) { attr["productname"] = item->toString(); }
        else if (rid.suffix == RAttrSwconfigid) {attr["swconfigid"] = item->toString(); }
        else if (rid.suffix == RAttrType) { attr["type"] = item->toString(); }
        else if (rid.suffix == RAttrUniqueId) { attr["uniqueid"] = item->toString(); }
        else if (rid.suffix == RCapAlertTriggerEffect) { capabilitiesAlerts = &RStateAlertValuesTriggerEffect; }
        else if (rid.suffix == RCapBriMinDimLevel) { capabilitiesBri["min_dim_level"] = round(item->toNumber() / 10.0) / 100.0; }
        else if (rid.suffix == RCapColorCapabilities) { icc = item; }
        else if (rid.suffix == RCapColorCtComputesXy) { capabilitiesColorCt["computes_xy"] = item->toBool(); }
        else if (rid.suffix == RCapColorCtMax)
        {
            if (req.apiVersion() < ApiVersion_3_DDEL)
            {
                attr["ctmax"] = item->toNumber();
            }
            capabilitiesColorCt["max"] = item->toNumber();
        }
        else if (rid.suffix == RCapColorCtMin)
        {
            if (req.apiVersion() < ApiVersion_3_DDEL)
            {
                attr["ctmin"] = item->toNumber();
            }
            capabilitiesColorCt["min"] = item->toNumber();
        }
        else if (rid.suffix == RCapColorEffects) { effectList = getHueEffectNames(item->toNumber()); }
        else if (rid.suffix == RCapColorGamutType) { capabilitiesColor["gamut_type"] = item->toString(); }
        else if (rid.suffix == RCapColorGradientMaxSegments) { capabilitiesColorGradient["max_segments"] = item->toNumber(); }
        else if (rid.suffix == RCapColorGradientPixelCount) { capabilitiesColorGradient["pixel_count"] = item->toNumber(); }
        else if (rid.suffix == RCapColorGradientPixelLength) { capabilitiesColorGradient["pixel_length"] = item->toNumber(); }
        else if (rid.suffix == RCapColorGradientStyles) { capabilitiesColorGradient["styles"] = getHueGradientStyleNames(item->toNumber()); }
        else if (rid.suffix == RCapColorXyBlueX) { ibluex = item; }
        else if (rid.suffix == RCapColorXyBlueY) { ibluey = item; }
        else if (rid.suffix == RCapColorXyGreenX) { igreenx = item; }
        else if (rid.suffix == RCapColorXyGreenY) { igreeny = item; }
        else if (rid.suffix == RCapColorXyRedX) { iredx = item; }
        else if (rid.suffix == RCapColorXyRedY) { iredy = item; }
        else if (rid.suffix == RCapGroupsNotSupported) { groups = false; }
        else if (rid.suffix == RCapSleeper) { capabilities["sleeper"] = true; }
        else if (rid.suffix == RCapTransitionBlock) { capabilities["transition_block"] = true; }
        else if (rid.suffix == RConfigBriExecuteIfOff) { configBri["execute_if_off"] = item->toBool(); }
        else if (rid.suffix == RConfigBriMax) { configBri["max"] = item->toNumber(); }
        else if (rid.suffix == RConfigBriMin) { configBri["min"] = item->toNumber(); }
        else if (rid.suffix == RConfigBriOnLevel) { configBri["on_level"] = item->toNumber() == 0xFF ? QVariant(QLatin1String("previous")) : item->toNumber(); }
        else if (rid.suffix == RConfigBriMin) { configBri["onoff_transition_time"] = item->toNumber(); }
        else if (rid.suffix == RConfigBriStartup) { configBri["startup"] = item->toNumber() == 0xFF ? QVariant(QLatin1String("previous")) : item->toNumber(); }
        else if (rid.suffix == RConfigColorCtStartup) { configColorCt["startup"] = item->toNumber() == 0xFFFF ? QVariant(QLatin1String("previous")) : item->toNumber(); }
        else if (rid.suffix == RConfigColorExecuteIfOff) { configColor["execute_if_off"] = item->toBool(); }
        else if (rid.suffix == RConfigColorGradientReversed) { configColorGradient["reversed"] = item->toBool(); }
        else if (rid.suffix == RConfigColorXyStartupX) { isx = item; }
        else if (rid.suffix == RConfigColorXyStartupY) { isy = item; }
        else if (rid.suffix == RConfigOnStartup) { configOn["startup"] = item->toNumber() == 0xFF ? QVariant(QLatin1String("previous")) : item->toBool(); }
        else if (rid.suffix == RStateAlert) { ialert = item; }
        else if (rid.suffix == RStateBri) { state["bri"] = static_cast<double>(item->toNumber()); }
        else if (rid.suffix == RStateColorMode) { state["colormode"] = item->toString(); }
        else if (rid.suffix == RStateCt) { state["ct"] = static_cast<double>(item->toNumber()); }
        else if (rid.suffix == RStateEffect) { state["effect"] = item->toString(); }
        else if (rid.suffix == RStateGradient)
        {
            bool ok;
            QVariant var = Json::parse(item->toString(), ok);
            if (ok)
            {
                QVariantMap map = var.toMap();
                state["gradient"] = map;
            }
        }
        else if (rid.suffix == RStateHue) { state["hue"] = static_cast<double>(item->toNumber()); }
        else if (rid.suffix == RStateLift) { state["lift"] = item->toNumber(); }
        else if (rid.suffix == RStateOn) { state["on"] = item->toBool(); }
        else if (rid.suffix == RStateOpen) { state["open"] = item->toBool(); }
        else if (rid.suffix == RStateReachable) { state["reachable"] = item->toBool(); }
        else if (rid.suffix == RStateSat) { state["sat"] = static_cast<double>(item->toNumber()); }
        else if (rid.suffix == RStateSpeed) { state["speed"] = item->toNumber(); }
        else if (rid.suffix == RStateTilt) { state["tilt"] = item->toNumber(); }
        else if (rid.suffix == RStateX) { ix = item; }
        else if (rid.suffix == RStateY) { iy = item; }
    }

    if (groups)
    {
        QStringList groups;
        std::vector<GroupInfo>::const_iterator g = lightNode->groups().begin();
        std::vector<GroupInfo>::const_iterator gend = lightNode->groups().end();
        for (; g != gend; ++g)
        {
            if (g->state == GroupInfo::StateInGroup)
            {
                groups.append(QVariant(g->id == gwGroup0 ? 0 : g->id).toString());
            }
        }
        config["groups"] = groups;
    }

    if (icc)
    {
        const int cc = icc->toNumber();
        QStringList colorModes;

        if (cc & 0x10) colorModes.push_back(QLatin1String("ct"));
        if (cc & 0x04)
        {
            colorModes.push_back(QLatin1String("effect"));
            if (lightNode->manufacturerCode() == VENDOR_MUELLER)
            {
                effectList = RStateEffectValuesMueller;
            }
            capabilitiesColor["effects"] = effectList;
        }
        else if (isXmasLightStrip(lightNode))
        {
            colorModes.push_back(QLatin1String("effect"));
            capabilitiesColor["effects"] = RStateEffectValuesXmasLightStrip;
        }
        if (!capabilitiesColorGradient.isEmpty()) colorModes.push_back(QLatin1String("gradient"));
        if (cc & 0x01 || cc & 0x02) colorModes.push_back(QLatin1String("hs"));
        if (cc & 0x08) colorModes.push_back(QLatin1String("xy"));

        if (req.apiVersion() <= ApiVersion_1_DDEL)
        {
            attr["colorcapabilities"] = cc;
        }
        else if (req.apiVersion() < ApiVersion_3_DDEL)
        {
            attr["colorcapabilities"] = colorModes;
        }
        capabilitiesColor["modes"] = colorModes;
    }

    if (ibluex && ibluey && igreenx && igreeny && iredx && iredy)
    {
        QVariantList blue, green, red;

        toXy(ibluex->toNumber(), ibluey->toNumber(), blue);
        toXy(igreenx->toNumber(), igreeny->toNumber(), green);
        toXy(iredx->toNumber(), iredy->toNumber(), red);
        capabilitiesColorXy["blue"] = blue;
        capabilitiesColorXy["green"] = green;
        capabilitiesColorXy["red"] = red;
    }

    if (isx && isy)
    {
        double colorX = isx->toNumber();
        double colorY = isy->toNumber();

        if (colorX == 0xFFFF && colorY == 0xFFFF)
        {
            configColorXy["startup"] = QLatin1String("previous");
        }
        else
        {
            QVariantList xy;

            toXy(colorX, colorY, xy);
            configColorXy["startup"] = xy;
        }
    }

    if (ialert)
    {
        state["alert"] = QLatin1String("none");
        capabilities["alerts"] = *capabilitiesAlerts;
    }

    if (ix && iy)
    {
        QVariantList xy;

        toXy(ix->toNumber(), iy->toNumber(), xy);
        state["xy"] = xy;
    }

    // Amazon Echo quirks mode
    if (req.mode == ApiModeEcho)
    {
        // OSRAM plug + Ubisys S1/S2
        if (lightNode->type().startsWith(QLatin1String("On/Off")))
        {
            attr["modelid"] = QLatin1String("LWB010");
            attr["manufacturername"] = QLatin1String("Philips");
            attr["type"] = QLatin1String("Dimmable light");
            state["bri"] = (double) 254;
        }
    }

    if (req.path.size() > 2 && req.path[2] == QLatin1String("devices"))
    {
        // don't add in sub device
    }
    else
    {
        if (req.mode != ApiModeEcho && req.apiVersion() < ApiVersion_3_DDEL)
        {
            attr["hascolor"] = lightNode->hasColor();
        }

        attr["manufacturername"] = lightNode->manufacturer();
        attr["modelid"] = lightNode->modelId(); // real model id
        attr["swversion"] = lightNode->swBuildId();
        QString etag = lightNode->etag;
        etag.remove('"'); // no quotes allowed in string
        attr["etag"] = etag;

        if (req.apiVersion() >= ApiVersion_2_DDEL)
        {
            QVariantMap links;
            QVariantMap self;
            self["href"] = QString("%1/%2").arg(req.hdr.path()).arg(lightNode->uniqueId());
            links["self"] = self;
            attr["_links"] = links;
        }
    }
    if (!state.isEmpty()) attr["state"] = state;
    if (!capabilitiesBri.isEmpty()) capabilities["bri"] = capabilitiesBri;
    if (!capabilitiesColorCt.isEmpty()) capabilitiesColor["ct"] = capabilitiesColorCt;
    if (!capabilitiesColorGradient.isEmpty()) capabilitiesColor["gradient"] = capabilitiesColorGradient;
    if (!capabilitiesColorXy.isEmpty()) capabilitiesColor["xy"] = capabilitiesColorXy;
    if (!capabilitiesColor.isEmpty()) capabilities["color"] = capabilitiesColor;
    if (!capabilities.isEmpty()) attr["capabilities"] = capabilities;
    if (!configBri.isEmpty()) config["bri"] = configBri;
    if (!configColorCt.isEmpty()) configColor["ct"] = configColorCt;
    if (!configColorGradient.isEmpty()) configColor["gradient"] = configColorGradient;
    if (!configColorXy.isEmpty()) configColor["xy"] = configColorXy;
    if (!configColor.isEmpty()) config["color"] = configColor;
    if (!configOn.isEmpty()) config["on"] = configOn;
    if (!config.isEmpty()) attr["config"] = config;

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

    // handle ETag
    if (req.hdr.hasKey(QLatin1String("If-None-Match")))
    {
        QString etag = req.hdr.value(QLatin1String("If-None-Match"));

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

    Device *device = static_cast<Device*>(taskRef.lightNode->parentResource());
    rsp.httpStatus = HttpStatusOk;

    if (!taskRef.lightNode->isAvailable())
    {
        rsp.httpStatus = HttpStatusOk;
        rsp.list.append(errorToMap(ERR_DEVICE_NOT_REACHABLE, QString("/lights/%1/state").arg(id), QString("resource, /lights/%1/state, is not modifiable. Device is not reachable.").arg(id)));
        return REQ_READY_SEND;
    }

    // set destination parameters
    taskRef.req.dstAddress() = taskRef.lightNode->address();
    taskRef.req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
    taskRef.req.setDstEndpoint(taskRef.lightNode->haEndpoint().endpoint());
    taskRef.req.setSrcEndpoint(getSrcEndpoint(taskRef.lightNode, taskRef.req));
    taskRef.req.setDstAddressMode(deCONZ::ApsExtAddress);
    taskRef.transitionTime = 4;
    taskRef.onTime = 0;

    bool ok;
    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();

    if (!ok || map.isEmpty())
    {
        rsp.list.append(errorToMap(ERR_INVALID_JSON, QString("/lights/%1/state").arg(id), QString("body contains invalid JSON")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    // FIXME: use cluster instead of device type.
    if (taskRef.lightNode->type() == QLatin1String("Window covering controller") ||
        taskRef.lightNode->type() == QLatin1String("Window covering device"))
    {
        return setWindowCoveringState(req, rsp, taskRef, map);
    }
    else if (isXmasLightStrip(taskRef.lightNode))
    {
        return setXmasLightStripState(req, rsp, taskRef, map);
    }
    else if (UseTuyaCluster(taskRef.lightNode->manufacturer()))
    {
        //tuya window covering
        if (R_GetProductId(taskRef.lightNode).startsWith(QLatin1String("Tuya_COVD")))
        {
            return setWindowCoveringState(req, rsp, taskRef, map);
        }
        // light, don't use tuya stuff (for the moment)
        else if (taskRef.lightNode->item(RStateColorMode))
        {
        }
        // handle by device code
        else if (device && device->managed())
        {
        }
        //switch and siren
        else
        {
            return setTuyaDeviceState(req, rsp, taskRef, map);
        }
    }
    else if (taskRef.lightNode->type() == QLatin1String("Warning device")) // Put it here because some tuya device are Warning device but need to be process by tuya part
    {
        return setWarningDeviceState(req, rsp, taskRef, map);
    }
    else if (taskRef.lightNode->type() == QLatin1String("Door Lock"))
    {
        return setDoorLockState(req, rsp, taskRef, map);
    }

    const QStringList *alertList = &RStateAlertValuesTriggerEffect; // TODO: check RCapAlertTriggerEffect
    QStringList effectList = RStateEffectValues;
    if (taskRef.lightNode->item(RCapColorEffects))
    {
        effectList = getHueEffectNames(taskRef.lightNode->item(RCapColorEffects)->toNumber());
    }
    if (taskRef.lightNode->manufacturerCode() == VENDOR_MUELLER)
    {
        effectList = RStateEffectValuesMueller;
    }

    bool hasCmd = false;
    bool isOn = false;
    bool hasOn = false;
    bool targetOn = false;
    bool hasBri = false;
    quint8 targetBri = 0;
    bool hasBriInc = false;
    qint16 targetBriInc = 0;
    bool hasWrap = false;
    bool wrap = false;
    bool hasXy = false;
    double targetX = 0.0;
    double targetY = 0.0;
    bool hasCt = false;
    quint16 targetCt = 0;
    bool hasCtInc = false;
    qint16 targetCtInc = 0;
    bool hasHue = false;
    quint16 targetHue = 0;
    bool hasSat = false;
    quint8 targetSat = 0;
    int effect = -1;
    bool hasColorloopSpeed = false;
    quint16 colorloopSpeed = 25;
    bool hasGradient = false;
    QVariantMap gradient;
    QString alert;
    bool hasSpeed = false;
    quint8 targetSpeed = 0;
    bool hasTransitionTime = false;

    // Check parameters.
    for (QVariantMap::const_iterator p = map.begin(); p != map.end(); p++)
    {
        bool paramOk = false;
        bool valueOk = false;
        QString param = p.key();
        if (param == "on" && taskRef.lightNode->item(RStateOn))
        {
            paramOk = true;
            hasCmd = true;
            if (map[param].type() == QVariant::Bool)
            {
                valueOk = true;
                hasOn = true;
                targetOn = map[param].toBool();
            }
        }
        else if (param == "bri" && taskRef.lightNode->item(RStateBri))
        {
            paramOk = true;
            hasCmd = true;
            if (map[param].type() == QVariant::Double)
            {
                const uint bri = map[param].toUInt(&ok);
                if (ok && bri <= 0xFF)
                {
                    valueOk = true;
                    hasBri = true;
                    targetBri = bri > 0xFE ? 0xFE : bri;
                }
            }
        }
        else if (param == "bri_inc"  && taskRef.lightNode->item(RStateBri))
        {
            paramOk = true;
            hasCmd = true;
            if (map[param].type() == QVariant::Double)
            {
                const int briInc = map[param].toInt(&ok);
                if (ok && briInc >= -0xFF && briInc <= 0xFF)
                {
                    valueOk = true;
                    hasBriInc = true;
                    targetBriInc = briInc < -0xFE ? -0xFE : briInc > 0xFE ? 0xFE : briInc;
                }
            }
        }
        else if (param == "xy"  && taskRef.lightNode->item(RStateX) && taskRef.lightNode->item(RStateY) &&
                 taskRef.lightNode->modelId() != QLatin1String("FLS-PP"))
        {
            // @manup: is check for FLS-PP needed, or is this already handled by check for state.x and state.y?
            paramOk = true;
            hasCmd = true;
            if (map[param].type() == QVariant::List)
            {
                QVariantList xy = map["xy"].toList();
                if (xy[0].type() == QVariant::Double && xy[1].type() == QVariant::Double)
                {
                    const double x = xy[0].toDouble(&ok);
                    const double y = ok ? xy[1].toDouble(&ok) : 0;
                    if (ok && x >= 0.0 && x <= 1.0 && y >= 0.0 && y <= 1.0)
                    {
                        valueOk = true;
                        hasXy = true;
                        targetX = x > 0.9961 ? 0.9961 : x;
                        targetY = y > 0.9961 ? 0.9961 : y;
                    }
                    else
                    {
                        valueOk = true;
                        rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1/state/xy").arg(id), QString("invalid value, [%1,%2], for parameter, xy").arg(xy[0].toString()).arg(xy[1].toString())));
                    }
                }
            }
        }
        else if (param == "ct") // FIXME workaround for lights that support color tempeature, but API doesn't expose ct.
        // else if (param == "ct"  && (taskRef.lightNode->item(RStateCt))
        {
            paramOk = true;
            hasCmd = true;
            if (map[param].type() == QVariant::Double)
            {
                const quint16 ctMin = taskRef.lightNode->toNumber(RCapColorCtMin);
                const quint16 ctMax = taskRef.lightNode->toNumber(RCapColorCtMax);
                const uint ct = map[param].toUInt(&ok);
                if (ok && ct <= 0xFFFF)
                {
                    valueOk = true;
                    hasCt = true;
                    targetCt = (ctMin < 500 && ct < ctMin) ? ctMin : (ctMax > ctMin && ct > ctMax) ? ctMax : ct;
                }
            }
        }
        else if (param == "ct_inc"  && taskRef.lightNode->item(RStateCt))
        {
            paramOk = true;
            hasCmd = true;
            if (map[param].type() == QVariant::Double)
            {
                int ct = taskRef.lightNode->toNumber(RStateCt);
                const quint16 ctMin = taskRef.lightNode->toNumber(RCapColorCtMin);
                const quint16 ctMax = taskRef.lightNode->toNumber(RCapColorCtMax);
                const int ctInc = map[param].toInt(&ok);
                if (ok && ctInc >= -0xFFFF && ctInc <= 0xFFFF)
                {
                    valueOk = true;
                    hasCtInc = true;
                    targetCtInc = ctInc;
                    ct += ctInc;
                    ct = ct < 0 ? 0 : ct > 0xFEFF ? 0xFEFF : ct;
                    targetCt = (ctMin < 500 && ct < ctMin) ? ctMin : (ctMax > ctMin && ct > ctMax) ? ctMax : ct;
                }
            }
        }
        else if (param == "hue" && taskRef.lightNode->item(RStateHue) && taskRef.lightNode->item(RStateSat))
        {
            paramOk = true;
            hasCmd = true;
            const uint hue = map[param].toUInt(&ok);
            if (ok && hue <= 0xFFFF)
            {
                valueOk = true;
                hasHue = true;
                targetHue = hue; // Funny: max CurrentHue is 0xFE, max EnhancedCurrentHue is 0xFFFF
            }
        }
        else if (param == "sat" && taskRef.lightNode->item(RStateHue) && taskRef.lightNode->item(RStateSat))
        {
            paramOk = true;
            hasCmd = true;
            const uint sat = map[param].toUInt(&ok);
            if (ok && sat <= 0xFF)
            {
                valueOk = true;
                hasSat = true;
                targetSat = sat > 0xFE ? 0xFE : sat;
            }
        }
        else if (param == "effect" && taskRef.lightNode->item(RStateEffect))
        {
            paramOk = true;
            hasCmd = true;
            if (map[param].type() == QVariant::String)
            {
                effect = effectList.indexOf(map[param].toString());
                valueOk = effect >= 0;
            }
        }
        else if (param == "colorloopspeed" && taskRef.lightNode->item(RStateEffect))
        {
            paramOk = true;
            const uint speed = map[param].toUInt(&ok);
            if (ok && speed <= 0xFFFF)
            {
                valueOk = true;
                hasColorloopSpeed = true;
                colorloopSpeed = speed < 1 ? 1 : speed;
            }
        }
        else if (param == "gradient" && taskRef.lightNode->item(RStateGradient))
        {
            quint16 styleBitmap = taskRef.lightNode->toNumber(RCapColorGradientStyles);
            paramOk = true;
            if (map[param].type() == QVariant::Map)
            {
                gradient = map[param].toMap();
                if (validateHueGradient(req, rsp, gradient, styleBitmap))
                {
                    hasCmd = true;
                    hasGradient = true;
                }
                valueOk = true;
            }
        }
        else if (param == "colormode" && taskRef.lightNode->item(RStateColorMode))
        {
            paramOk = true;
            valueOk = true;
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_MODIFIABLE, QString("/lights/%1/state/colormode").arg(id).arg(param), QString("parameter, colormode, is not modifiable.")));
        }
        else if (param == "alert" && taskRef.lightNode->item(RStateAlert))
        {
            paramOk = true;
            hasCmd = true;
            if (map[param].type() == QVariant::String)
            {
                alert = map[param].toString();
                valueOk = alertList->contains(alert);
            }
        }
        else if (param == "speed" && taskRef.lightNode->item(RStateSpeed))
        {
            paramOk = true;
            hasCmd = true;
            if (map[param].type() == QVariant::Double)
            {
                const uint speed = map[param].toUInt(&ok);
                if (ok && speed <= 0xFF)
                {
                    valueOk = true;
                    hasSpeed = true;
                    targetSpeed = speed > 6 ? 6 : speed;
                }
            }
        }
        else if (param == "transitiontime")
        {
            paramOk = true;
            if (map[param].type() == QVariant::Double)
            {
                const uint tt = map[param].toUInt(&ok);
                if (ok && tt <= 0xFFFF)
                {
                    valueOk = true;
                    hasTransitionTime = true;
                    taskRef.transitionTime = tt > 0xFFFE ? 0xFFFE : tt;
                }
            }
        }
        else if (param == "ontime")
        {
            paramOk = true;
            if (map[param].type() == QVariant::Double)
            {
                const uint ot = map[param].toUInt(&ok);
                if (ok && ot <= 0xFFFF)
                {
                    valueOk = true;
                    taskRef.onTime = ot;
                }
            }
        }
        else if (param == "wrap")
        {
            paramOk = true;
            if (map[param].type() == QVariant::Bool)
            {
                valueOk = true;
                hasWrap = true;
                wrap = map[param].toBool();
            }
        }
        if (!paramOk)
        {
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/lights/%1/state/%2").arg(id).arg(param), QString("parameter, %1, not available").arg(param)));
        }
        else if (!valueOk)
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1/state/%2").arg(id).arg(param), QString("invalid value, %1, for parameter, %2").arg(map[param].toString()).arg(param)));
        }
    }
    if (taskRef.onTime > 0 && !hasOn && alert.isEmpty())
    {
        rsp.list.append(errorToMap(ERR_MISSING_PARAMETER, QString("/lights/%1/state").arg(id), QString("missing parameter, on or alert, for parameter, ontime")));
    }
    if (hasWrap && !hasBriInc)
    {
        rsp.list.append(errorToMap(ERR_MISSING_PARAMETER, QString("/lights/%1/state").arg(id), QString("missing parameter, bri_inc, for parameter, wrap")));
    }
    if (hasColorloopSpeed && effect != R_EFFECT_COLORLOOP)
    {
        rsp.list.append(errorToMap(ERR_MISSING_PARAMETER, QString("/lights/%1/state").arg(id), QString("missing parameter, effect, for parameter, colorloopspeed")));
    }
    if (!hasCmd)
    {
        rsp.list.append(errorToMap(ERR_MISSING_PARAMETER, QString("/lights/%1/state").arg(id), QString("missing parameter to set light state")));
    }

    // Check whether light is on.
    isOn = taskRef.lightNode->toBool(RStateOn);

    // Special part for Profalux device
    // This device is a shutter but is used as a dimmable light, so need some hack
    if (taskRef.lightNode->modelId() == QLatin1String("PFLX Shutter"))
    {
        if (hasOn && !hasBri)
        {
            hasBri = true;
            targetBri = targetOn ? 0xFE : 0x00;
        }
        hasOn = false; // use bri instead

        if (hasBriInc)
        {
            targetBriInc = 0; // only use for stop
            hasBri = false;
        }
        else
        {
            isOn = true; // to force bri even when off
            if (targetBri > 0xFE) { targetBri = 0xFE; }
            if (targetBri < 1) { targetBri = 0x01; }
        }
    }

    // state.on: true
    if (hasOn && targetOn)
    {
        TaskItem task;
        copyTaskReq(taskRef, task);

        if (taskRef.lightNode->toBool(RCapBriMoveWithOnOff) && !isOn && hasBri && taskRef.onTime == 0)
        {
            // if a light is off and should transition from 0 to new brightness
            // turn light on at lowest brightness first
            TaskItem task;
            copyTaskReq(taskRef, task);
            task.transitionTime = 0;

            ok = addTaskSetBrightness(task, 2, true);
        }
        else
        {
            const quint8 cmd = taskRef.onTime > 0
                    ? ONOFF_COMMAND_ON_WITH_TIMED_OFF
                    : ONOFF_COMMAND_ON;
            ok = addTaskSetOnOff(task, cmd, taskRef.onTime, 0);
        }

        if (ok)
        {
            isOn = true;
            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/lights/%1/state/on").arg(id)] = targetOn;
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);
            taskRef.lightNode->setValue(RStateOn, targetOn);
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1/state/on").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
        }
    }

    // state.bri has priority over state.bri_inc
    if (hasBri)
    {
        TaskItem task;
        copyTaskReq(taskRef, task);

        if (!isOn && !taskRef.lightNode->toBool(RConfigBriExecuteIfOff))
        {
            rsp.list.append(errorToMap(ERR_DEVICE_OFF, QString("/lights/%1/state/bri").arg(id), QString("parameter, bri, is not modifiable. Device is set to off.")));
        }
        else if (hasOn && !targetOn && hasTransitionTime)
        {
            // Handled by state.on: false
        }
        else if (addTaskSetBrightness(task, targetBri, false))
        {
            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/lights/%1/state/bri").arg(id)] = targetBri;
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);

            taskRef.lightNode->setValue(RStateBri, targetBri);
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1/state/bri").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
        }
    }
    else if (hasBriInc)
    {
        TaskItem task;
        copyTaskReq(taskRef, task);
        int bri = taskRef.lightNode->toNumber(RStateBri);

        if (wrap)
        {
            if (bri + targetBriInc < 1)
            {
                targetBriInc += 254;
            }
            else if (bri + targetBriInc > 254)
            {
                targetBriInc -= 254;
            }
        }
        bri += targetBriInc;
        targetBri = bri < 0 ? 0 : bri > 254 ? 254 : bri;

        if (!isOn && !taskRef.lightNode->toBool(RConfigBriExecuteIfOff))
        {
            rsp.list.append(errorToMap(ERR_DEVICE_OFF, QString("/lights/%1/state/bri_inc").arg(id), QString("parameter, bri_inc, is not modifiable. Device is set to off.")));
        }
        else if (addTaskIncBrightness(task, targetBriInc))
        {
            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/lights/%1/state/bri").arg(id)] = targetBri;
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);

            taskRef.lightNode->setValue(RStateBri, targetBri);
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1/state/bri_inc").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
        }
    }

    // state.effect: "none"
    if (effect == R_EFFECT_NONE)
    {
        TaskItem task;
        copyTaskReq(taskRef, task);

        if (!isOn && !taskRef.lightNode->toBool(RConfigColorExecuteIfOff))
        {
            rsp.list.append(errorToMap(ERR_DEVICE_OFF, QString("/lights/%1/state/effect").arg(id), QString("parameter, effect, is not modifiable. Device is set to off.")));
        }
        else if (addTaskSetColorLoop(task, false, colorloopSpeed))
        {
            if (taskRef.lightNode->manufacturerCode() == VENDOR_MUELLER)
            {
                quint64 value = 0;
                deCONZ::ZclAttribute attr(0x4005, deCONZ::Zcl8BitUint, "scene", deCONZ::ZclReadWrite, true);
                attr.setValue(value);
                writeAttribute(taskRef.lightNode, taskRef.lightNode->haEndpoint().endpoint(), BASIC_CLUSTER_ID, attr, VENDOR_MUELLER);
            }

            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/lights/%1/state/effect").arg(id)] = effectList[effect];
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);

            taskRef.lightNode->setValue(RStateEffect, effectList[effect]);
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1/state/effect").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
        }
    }

    // state.xy trumps state.ct trumps state.ct_inc trumps state.hue, state.sat
    if (hasXy)
    {
        TaskItem task;
        copyTaskReq(taskRef, task);

        if (!isOn && !taskRef.lightNode->toBool(RConfigColorExecuteIfOff))
        {
            rsp.list.append(errorToMap(ERR_DEVICE_OFF, QString("/lights/%1/state/xy").arg(id), QString("parameter, xy, is not modifiable. Device is set to off.")));
        }
        else if (taskRef.lightNode->isColorLoopActive())
        {
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_MODIFIABLE, QString("/lights/%1/state/xy").arg(id), QString("parameter, xy, is not modifiable. Colorloop is active.")));
        }
        else if (addTaskSetXyColor(task, targetX, targetY))
        {
            QVariantMap rspItem;
            QVariantMap rspItemState;
            QVariantList xy;
            xy.append(targetX);
            xy.append(targetY);
            rspItemState[QString("/lights/%1/state/xy").arg(id)] = xy;
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);

            taskRef.lightNode->setValue(RStateX, targetX * 65535);
            taskRef.lightNode->setValue(RStateY, targetY * 65535);
            taskRef.lightNode->setValue(RStateColorMode, QString("xy"));
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1/state/xy").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
        }
    }
    else if (hasCt)
    {
        TaskItem task;
        copyTaskReq(taskRef, task);

        if (!isOn && !taskRef.lightNode->toBool(RConfigColorExecuteIfOff))
        {
            rsp.list.append(errorToMap(ERR_DEVICE_OFF, QString("/lights/%1/state/ct").arg(id), QString("parameter, ct, is not modifiable. Device is set to off.")));
        }
        else if (taskRef.lightNode->isColorLoopActive())
        {
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_MODIFIABLE, QString("/lights/%1/state/ct").arg(id), QString("parameter, ct, is not modifiable. Colorloop is active.")));
        }
        else if (addTaskSetColorTemperature(task, targetCt))
        {
            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/lights/%1/state/ct").arg(id)] = targetCt;
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);

            taskRef.lightNode->setValue(RStateCt, targetCt);
            taskRef.lightNode->setValue(RStateColorMode, QString("ct"));
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1/state/ct").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
        }
    }
    else if (hasCtInc)
    {
        TaskItem task;
        copyTaskReq(taskRef, task);

        if (!isOn && !taskRef.lightNode->toBool(RConfigColorExecuteIfOff))
        {
            rsp.list.append(errorToMap(ERR_DEVICE_OFF, QString("/lights/%1/state/ct_inc").arg(id), QString("parameter, ct_inc, is not modifiable. Device is set to off.")));
        }
        else if (taskRef.lightNode->isColorLoopActive())
        {
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_MODIFIABLE, QString("/lights/%1/state/ct_inc").arg(id), QString("parameter, ct_inc, is not modifiable. Colorloop is active.")));
        }
        else if (addTaskIncColorTemperature(task, targetCtInc))
        {
            taskToLocalData(task);
            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/lights/%1/state/ct").arg(id)] = targetCt;
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);

            taskRef.lightNode->setValue(RStateCt, targetCt);
            taskRef.lightNode->setValue(RStateColorMode, QString("ct"));
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1/state/ct_inc").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
        }
    }
    else if (hasHue || hasSat)
    {
        TaskItem task;
        copyTaskReq(taskRef, task);

        if (!isOn && !taskRef.lightNode->toBool(RConfigColorExecuteIfOff))
        {
            if (hasHue)
            {
                rsp.list.append(errorToMap(ERR_DEVICE_OFF, QString("/lights/%1/state/hue").arg(id), QString("parameter, hue, is not modifiable. Device is set to off.")));
            }
            if (hasSat)
            {
                rsp.list.append(errorToMap(ERR_DEVICE_OFF, QString("/lights/%1/state/sat").arg(id), QString("parameter, sat, is not modifiable. Device is set to off.")));
            }
        }
        else if (taskRef.lightNode->isColorLoopActive())
        {
            if (hasHue)
            {
                rsp.list.append(errorToMap(ERR_PARAMETER_NOT_MODIFIABLE, QString("/lights/%1/state/hue").arg(id), QString("parameter, hue, is not modifiable. Colorloop is active.")));
            }
            if (hasSat)
            {
                rsp.list.append(errorToMap(ERR_PARAMETER_NOT_MODIFIABLE, QString("/lights/%1/state/sat").arg(id), QString("parameter, sat, is not modifiable. Colorloop is active.")));
            }
        }
        else if (!hasSat) // only state.hue
        {
            ok = addTaskSetEnhancedHue(task, targetHue);
            // FIXME: handle lights that don't support Enhanced Current Hue (like Müller)
        }
        else if (!hasHue) // only state.sat
        {
            ok = addTaskSetSaturation(task, targetSat);
        }
        else // both state.hue and state.sat
        {
            const quint8 hue = targetHue / 256;
            ok = addTaskSetHueAndSaturation(task, hue, targetSat); // FIXME
            // ok = addTaskSetEnhancedHueAndSaturation(task, targetHue, targetSat);
        }
        if (ok)
        {
            if (hasHue)
            {
                QVariantMap rspItem;
                QVariantMap rspItemState;
                rspItemState[QString("/lights/%1/state/hue").arg(id)] = targetHue;
                rspItem["success"] = rspItemState;
                rsp.list.append(rspItem);

                taskRef.lightNode->setValue(RStateHue, targetHue);
            }
            if (hasSat)
            {
                QVariantMap rspItem;
                QVariantMap rspItemState;
                rspItemState[QString("/lights/%1/state/sat").arg(id)] = targetSat;
                rspItem["success"] = rspItemState;
                rsp.list.append(rspItem);

                taskRef.lightNode->setValue(RStateSat, targetSat);
            }
            taskRef.lightNode->setValue(RStateColorMode, QString("hs"));
        }
        else
        {
            if (hasHue)
            {
                rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1/state/hue").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
            }
            if (hasSat)
            {
                rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1/state/sat").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
            }
        }
    }

    // state.effect: "colorloop"
    if (effect == R_EFFECT_COLORLOOP)
    {
        TaskItem task;
        copyTaskReq(taskRef, task);

        if (!isOn && !taskRef.lightNode->toBool(RConfigColorExecuteIfOff))
        {
            rsp.list.append(errorToMap(ERR_DEVICE_OFF, QString("/lights/%1/state/effect").arg(id), QString("parameter, effect, is not modifiable. Device is set to off.")));
        }
        else if (addTaskSetColorLoop(task, true, colorloopSpeed))
        {
            if (taskRef.lightNode->manufacturerCode() == VENDOR_MUELLER)
            {
                quint64 value = 0;
                deCONZ::ZclAttribute attr(0x4005, deCONZ::Zcl8BitUint, "scene", deCONZ::ZclReadWrite, true);
                attr.setValue(value);
                writeAttribute(taskRef.lightNode, taskRef.lightNode->haEndpoint().endpoint(), BASIC_CLUSTER_ID, attr, VENDOR_MUELLER);
            }

            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/lights/%1/state/effect").arg(id)] = effectList[effect];
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);

            taskRef.lightNode->setValue(RStateEffect, effectList[effect]);
            taskRef.lightNode->setValue(RStateColorMode, QString("hs"));
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1/state/effect").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
        }
    }
    else if (effect > 0)
    {
        if (!isOn && !taskRef.lightNode->toBool(RConfigColorExecuteIfOff))
        {
            rsp.list.append(errorToMap(ERR_DEVICE_OFF, QString("/lights/%1/state/effect").arg(id), QString("parameter, effect, is not modifiable. Device is set to off.")));
        }
        else if (taskRef.lightNode->item(RCapColorEffects))
        {
            ok = addTaskHueEffect(taskRef, effectList[effect]);
        }
        else if (taskRef.lightNode->manufacturerCode() == VENDOR_MUELLER)
        {
            const quint64 value = effect - 1;
            deCONZ::ZclAttribute attr(0x4005, deCONZ::Zcl8BitUint, "scene", deCONZ::ZclReadWrite, true);
            attr.setValue(value);
            ok = writeAttribute(taskRef.lightNode, taskRef.lightNode->haEndpoint().endpoint(), BASIC_CLUSTER_ID, attr, VENDOR_MUELLER);
        }
        if (ok)
        {
            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/lights/%1/state/effect").arg(id)] = effectList[effect];
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);

            taskRef.lightNode->setValue(RStateEffect, effectList[effect]);
            taskRef.lightNode->setValue(RStateColorMode, QString("effect"));
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1/state/effect").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
        }
    }

    if (hasGradient)
    {
        TaskItem task;
        copyTaskReq(taskRef, task);

        if (taskRef.lightNode->manufacturerCode() == VENDOR_PHILIPS)
        {
            ok = addTaskHueGradient(taskRef, gradient);
        }
        ok = true;

        if (ok)
        {
            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/lights/%1/state/gradient").arg(id)] = gradient;
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);

            // taskRef.lightNode->setValue(RStateGradient, QString(Json::serialize(gradient)));
            taskRef.lightNode->setValue(RStateColorMode, QString("gradient"));
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1/state/gradient").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
        }
    }

    // state.alert
    if (!alert.isEmpty())
    {
        TaskItem task;
        copyTaskReq(taskRef, task);

        if (alert == "none")
        {
            task.taskType = TaskIdentify;
            task.identifyTime = 0;
        }
        else if (alert == "select")
        {
            task.taskType = TaskIdentify;
            task.identifyTime = 2;    // Hue lights don't react to 1.
        }
        else if (alert == "lselect")
        {
            task.taskType = TaskIdentify;
            task.identifyTime = taskRef.onTime > 0 ? taskRef.onTime : 15; // Default for Philips Hue bridge
        }
        else if (alert == "blink")
        {
            task.taskType = TaskTriggerEffect;
            task.effectIdentifier = 0x00;
        }
        else if (alert == "breathe")
        {
            task.taskType = TaskTriggerEffect;
            task.effectIdentifier = 0x01;
        }
        else if (alert == "okay")
        {
            task.taskType = TaskTriggerEffect;
            task.effectIdentifier = 0x02;
        }
        else if (alert == "channelchange")
        {
            task.taskType = TaskTriggerEffect;
            task.effectIdentifier = 0x0b;
        }
        else if (alert == "finish")
        {
            task.taskType = TaskTriggerEffect;
            task.effectIdentifier = 0xfe;
        }
        else if (alert == "stop")
        {
            task.taskType = TaskTriggerEffect;
            task.effectIdentifier = 0xff;
        }

        if ((task.taskType == TaskIdentify && addTaskIdentify(task, task.identifyTime)) ||
            (task.taskType == TaskTriggerEffect && addTaskTriggerEffect(task, task.effectIdentifier)))
        {
            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/lights/%1/state/alert").arg(id)] = alert;
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);

            // Don't update write-only state.alert.
        }
        else if (task.taskType == TaskIdentify || task.taskType == TaskTriggerEffect)
        {
            rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1/state/alert").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
        }
    }

    // state.speed
    if (hasSpeed)
    {
        TaskItem task;
        copyTaskReq(taskRef, task);

        deCONZ::ZclAttribute attr(0x0000, deCONZ::Zcl8BitEnum, "speed", deCONZ::ZclReadWrite, true);
        attr.setValue(QVariant(targetSpeed));

        if (writeAttribute(taskRef.lightNode, taskRef.lightNode->haEndpoint().endpoint(), FAN_CONTROL_CLUSTER_ID, attr))
        {
            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/lights/%1/state/speed").arg(id)] = targetSpeed;
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);

            taskRef.lightNode->setValue(RStateSpeed, targetSpeed);
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1/state/speed").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
        }
    }

    // state.on: false
    if (hasOn && !targetOn)
    {
        if (taskRef.lightNode->isColorLoopActive())
        {
            TaskItem task;
            copyTaskReq(taskRef, task);
            addTaskSetColorLoop(task, false, colorloopSpeed);
        }

        TaskItem task;
        copyTaskReq(taskRef, task);
        if (hasBri && hasTransitionTime && taskRef.lightNode->toBool(RCapBriMoveWithOnOff))
        {
            ok = addTaskSetBrightness(task, 0, true);
        }
        else
        {
            const quint8 cmd = taskRef.lightNode->toBool(RCapOnOffWithEffect)
                    ? ONOFF_COMMAND_OFF_WITH_EFFECT
                    : ONOFF_COMMAND_OFF;
            ok = addTaskSetOnOff(task, cmd, 0, 0);
        }

        if (ok)
        {
            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/lights/%1/state/on").arg(id)] = targetOn;
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);
            taskRef.lightNode->setValue(RStateOn, targetOn);
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1/state/on").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
        }
    }

    if (!taskRef.lightNode->stateChanges().empty())
    {
        DBG_Printf(DBG_INFO, "emit event/tick: %s\n", qPrintable(taskRef.lightNode->address().toStringExt()));
        enqueueEvent({taskRef.lightNode->prefix(), REventTick, taskRef.lightNode->uniqueId(), taskRef.lightNode->address().ext()});
    }

    rsp.etag = taskRef.lightNode->etag;
    processTasks();
    return REQ_READY_SEND;
}

/*! PUT, PATCH /api/<apikey>/lights/<id>/config
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::setLightConfig(const ApiRequest &req, ApiResponse &rsp)
{
    QString id = req.path[3];
    LightNode *lightNode = getLightNodeForId(id);

    rsp.httpStatus = HttpStatusOk;

    bool ok;
    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();
    if (!ok || map.isEmpty())
    {
        rsp.list.append(errorToMap(ERR_INVALID_JSON, QString("/lights/%1/state").arg(id), QString("body contains invalid JSON")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    if (!lightNode || lightNode->state() == LightNode::StateDeleted)
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/lights/%1").arg(id), QString("resource, /lights/%1, not available").arg(id)));
        return REQ_READY_SEND;
    }

    ok = false;
    Device *device = static_cast<Device *>(lightNode->parentResource());
    Resource *rsub;
    if (device && device->managed())
    {
        rsub = DEV_GetSubDevice(device, nullptr, lightNode->uniqueId());
        if (rsub)
        {
            ok = true;
        }
    }
    if (!ok)
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/lights/%1/config").arg(id), QString("resource, /lights/%1/config, is not modifyable. Device is not managed through DDF.").arg(id)));
        return REQ_READY_SEND;
    }

    if (!lightNode->isAvailable())
    {
        rsp.httpStatus = HttpStatusOk;
        rsp.list.append(errorToMap(ERR_DEVICE_NOT_REACHABLE, QString("/lights/%1/config").arg(id), QString("resource, /lights/%1/config, is not modifiable. Device is not reachable.").arg(id)));
        return REQ_READY_SEND;
    }

    if (req.sock)
    {
        userActivity();
    }

    StateChange change(StateChange::StateCallFunction, SC_WriteZclAttribute, lightNode->haEndpoint().endpoint());

    for (QVariantMap::const_iterator p = map.begin(); p != map.end(); p++)
    {
        bool paramOk = false;
        bool valueOk = false;
        QString key = p.key();
        QString path = QString("/lights/%1/config/%2").arg(id).arg(key);
        QVariant value = map[key];

        if (key == "bri")
        {
            paramOk = true;
            if (value.type() == QVariant::Map)
            {
                valueOk = true;
                QVariantMap map1 = value.toMap();
                for (QVariantMap::const_iterator q = map1.begin(); q != map1.end(); q++)
                {
                    bool paramOk = false;
                    bool valueOk = false;
                    key = q.key();
                    QString path1 = QString("%1/%2").arg(path).arg(key);
                    value = map1[key];

                    if (key == "execute_if_off")
                    {
                        ResourceItem *item = lightNode->item(RConfigBriExecuteIfOff);
                        if (item)
                        {
                            paramOk = true;
                            if (value.type() == QVariant::Bool)
                            {
                                valueOk = true;
                                change.addTargetValue(RConfigBriExecuteIfOff, value.toBool());
                                lightNode->setValue(RConfigBriExecuteIfOff, value.toBool());
                                DB_StoreSubDeviceItem(lightNode, item);
                            }
                        }
                    }
                    else if (key == "max")
                    {
                        ResourceItem *item = lightNode->item(RConfigBriMax);
                        if (item)
                        {
                            paramOk = true;
                            if (value.type() == QVariant::Double)
                            {
                                const quint8 bri = value.toUInt(&ok);
                                if (ok)
                                {
                                    valueOk = true;
                                    quint8 targetBriMax = bri > 0xFE ? 0xFE : bri;
                                    value = QVariant(targetBriMax);
                                    change.addTargetValue(RConfigBriMax, targetBriMax);
                                    lightNode->setValue(RConfigBriMax, bri > 0xFE ? 0xFE : bri);
                                    DB_StoreSubDeviceItem(lightNode, item);
                                }
                            }
                        }
                    }
                    else if (key == "min")
                    {
                        ResourceItem *item = lightNode->item(RConfigBriMin);
                        if (item)
                        {
                            paramOk = true;
                            if (value.type() == QVariant::Double)
                            {
                                const quint8 bri = value.toUInt(&ok);
                                if (ok)
                                {
                                    valueOk = true;
                                    quint8 targetBriMin = bri > 0xFE ? 0xFE : bri;
                                    value = QVariant(targetBriMin);
                                    change.addTargetValue(RConfigBriMin, targetBriMin);
                                    lightNode->setValue(RConfigBriMin, targetBriMin);
                                    DB_StoreSubDeviceItem(lightNode, item);
                                }
                            }
                        }
                    }
                    else if (key == "on_level")
                    {
                        ResourceItem *item = lightNode->item(RConfigBriOnLevel);
                        if (item)
                        {
                            paramOk = true;
                            quint8 targetBriOnLevel;
                            if (value.type() == QVariant::Double)
                            {
                                const quint8 bri = value.toUInt(&ok);
                                if (ok)
                                {
                                    valueOk = true;
                                    targetBriOnLevel = bri > 0xFE ? 0xFE : bri;
                                    value = QVariant(targetBriOnLevel);
                                }
                            }
                            else if (value.type() == QVariant::String && value.toString() == QLatin1String("previous"))
                            {
                                valueOk = true;
                                targetBriOnLevel = 0xFF;
                            }
                            if (valueOk)
                            {
                                change.addTargetValue(RConfigBriOnLevel, targetBriOnLevel);
                                lightNode->setValue(RConfigBriOnLevel, targetBriOnLevel);
                                DB_StoreSubDeviceItem(lightNode, item);
                            }
                        }
                    }
                    else if (key == "onoff_transitiontime")
                    {
                        ResourceItem *item = lightNode->item(RConfigBriOnOffTransitiontime);
                        if (item)
                        {
                            paramOk = true;
                            if (value.type() == QVariant::Double)
                            {
                                const quint16 tt = value.toUInt(&ok);
                                if (ok)
                                {
                                    valueOk = true;
                                    change.addTargetValue(RConfigBriOnOffTransitiontime, tt);
                                    lightNode->setValue(RConfigBriOnOffTransitiontime, tt);
                                    DB_StoreSubDeviceItem(lightNode, item);
                                }
                            }
                        }
                    }
                    else if (key == "startup")
                    {
                        ResourceItem *item = lightNode->item(RConfigBriStartup);
                        if (item)
                        {
                            paramOk = true;
                            quint8 targetBriStartup;
                            if (value.type() == QVariant::Double)
                            {
                                const quint8 bri = value.toUInt(&ok);
                                if (ok)
                                {
                                    valueOk = true;
                                    targetBriStartup = bri > 0xFE ? 0xFE : bri;
                                    value = QVariant(targetBriStartup);
                                }
                            }
                            else if (value.type() == QVariant::String && value.toString() == QLatin1String("previous"))
                            {
                                valueOk = true;
                                targetBriStartup = 0xFF;
                            }
                            if (valueOk)
                            {
                                change.addTargetValue(RConfigBriStartup, targetBriStartup);
                                lightNode->setValue(RConfigBriStartup, targetBriStartup);
                                DB_StoreSubDeviceItem(lightNode, item);
                            }
                        }
                    }

                    if (!paramOk)
                    {
                        rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, path1, QString("parameter, %1, not available").arg(key)));
                    }
                    else if (!valueOk)
                    {
                        rsp.list.append(errorToMap(ERR_INVALID_VALUE, path1, QString("invalid value, %1, for parameter, %2").arg(value.toString()).arg(key)));
                    }
                    else
                    {
                        QVariantMap rspItem;
                        QVariantMap rspItemState;
                        rspItemState[path1] = value;
                        rspItem["success"] = rspItemState;
                        rsp.list.append(rspItem);
                    }
                }
            }
        }
        else if (key == "color")
        {
            paramOk = true;
            if (value.type() == QVariant::Map)
            {
                valueOk = true;
                QVariantMap map1 = value.toMap();
                for (QVariantMap::const_iterator q = map1.begin(); q != map1.end(); q++)
                {
                    bool paramOk = false;
                    bool valueOk = false;
                    bool isMap = false;
                    key = q.key();
                    QString path1 = QString("%1/%2").arg(path).arg(key);
                    value = map1[key];

                    if (key == "ct")
                    {
                        paramOk = true;
                        if (value.type() == QVariant::Map)
                        {
                            valueOk = true;
                            isMap = true;
                            QVariantMap map2 = value.toMap();
                            for (QVariantMap::const_iterator r = map2.begin(); r != map2.end(); r++)
                            {
                                bool paramOk = false;
                                bool valueOk = false;
                                key = r.key();
                                QString path2 = QString("%1/%2").arg(path1).arg(key);
                                value = map2[key];

                                if (key == "startup")
                                {
                                    ResourceItem *item = lightNode->item(RConfigColorCtStartup);
                                    if (item)
                                    {
                                        paramOk = true;
                                        quint16 targetCtStartup;
                                        if (value.type() == QVariant::Double)
                                        {
                                            const quint16 ctMin = lightNode->toNumber(RCapColorCtMin);
                                            const quint16 ctMax = lightNode->toNumber(RCapColorCtMax);
                                            const quint16 ct = value.toUInt(&ok);
                                            if (ok)
                                            {
                                                valueOk = true;
                                                targetCtStartup = (ctMin < 500 && ct < ctMin) ? ctMin : (ctMax > ctMin && ct > ctMax) ? ctMax : ct;
                                                value = QVariant(targetCtStartup);
                                            }
                                        }
                                        else if (value.type() == QVariant::String && value.toString() == QLatin1String("previous"))
                                        {
                                            valueOk = true;
                                            targetCtStartup = 0xFFFF;
                                        }
                                        if (valueOk)
                                        {
                                            change.addTargetValue(RConfigColorCtStartup, targetCtStartup);
                                            lightNode->setValue(RConfigColorCtStartup, targetCtStartup);
                                            DB_StoreSubDeviceItem(lightNode, item);
                                        }
                                    }
                                }

                                if (!paramOk)
                                {
                                    rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, path2, QString("parameter, %1, not available").arg(key)));
                                }
                                else if (!valueOk)
                                {
                                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, path2, QString("invalid value, %1, for parameter, %2").arg(value.toString()).arg(key)));
                                }
                                else
                                {
                                    QVariantMap rspItem;
                                    QVariantMap rspItemState;
                                    rspItemState[path2] = value;
                                    rspItem["success"] = rspItemState;
                                    rsp.list.append(rspItem);
                                }
                            }
                        }
                    }
                    else if (key == "execute_if_off")
                    {
                        ResourceItem *item = lightNode->item(RConfigColorExecuteIfOff);
                        if (item)
                        {
                            paramOk = true;
                            if (value.type() == QVariant::Bool)
                            {
                                valueOk = true;
                                change.addTargetValue(RConfigColorExecuteIfOff, value.toBool());
                                lightNode->setValue(RConfigColorExecuteIfOff, value.toBool());
                                DB_StoreSubDeviceItem(lightNode, item);
                            }
                        }
                    }
                    else if (key == "gradient")
                    {
                        paramOk = true;
                        if (value.type() == QVariant::Map)
                        {
                            valueOk = true;
                            isMap = true;
                            QVariantMap map2 = value.toMap();
                            for (QVariantMap::const_iterator r = map2.begin(); r != map2.end(); r++)
                            {
                                bool paramOk = false;
                                bool valueOk = false;
                                key = r.key();
                                QString path2 = QString("%1/%2").arg(path1).arg(key);
                                value = map2[key];

                                if (key == "reversed")
                                {
                                    ResourceItem *item = lightNode->item(RConfigColorGradientReversed);
                                    if (item)
                                    {
                                        paramOk = true;
                                        if (value.type() == QVariant::Bool)
                                        {
                                            valueOk = true;
                                            change.addTargetValue(RConfigColorGradientReversed, value.toBool());
                                            lightNode->setValue(RConfigColorGradientReversed, value.toBool());
                                            DB_StoreSubDeviceItem(lightNode, item);
                                        }
                                    }
                                }

                                if (!paramOk)
                                {
                                    rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, path2, QString("parameter, %1, not available").arg(key)));
                                }
                                else if (!valueOk)
                                {
                                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, path2, QString("invalid value, %1, for parameter, %2").arg(value.toString()).arg(key)));
                                }
                                else
                                {
                                    QVariantMap rspItem;
                                    QVariantMap rspItemState;
                                    rspItemState[path2] = value;
                                    rspItem["success"] = rspItemState;
                                    rsp.list.append(rspItem);
                                }
                            }
                        }
                    }
                    else if (key == "xy")
                    {
                        paramOk = true;
                        if (value.type() == QVariant::Map)
                        {
                            valueOk = true;
                            isMap = true;
                            QVariantMap map2 = value.toMap();
                            for (QVariantMap::const_iterator r = map2.begin(); r != map2.end(); r++)
                            {
                                bool paramOk = false;
                                bool valueOk = false;
                                key = r.key();
                                QString path2 = QString("%1/%2").arg(path1).arg(key);
                                value = map2[key];

                                if (key == "startup")
                                {
                                    ResourceItem *itemX = lightNode->item(RConfigColorXyStartupX);
                                    ResourceItem *itemY = lightNode->item(RConfigColorXyStartupY);
                                    if (itemX && itemY)
                                    {
                                        paramOk = true;
                                        quint16 targetXyStartupX, targetXyStartupY;
                                        if (value.type() == QVariant::List)
                                        {
                                            QVariantList xy = value.toList();
                                            if (xy[0].type() == QVariant::Double && xy[1].type() == QVariant::Double)
                                            {
                                                const double x = xy[0].toDouble(&ok);
                                                const double y = ok ? xy[1].toDouble(&ok) : 0;
                                                if (ok && x >= 0.0 && x <= 1.0 && y >= 0.0 && y <= 1.0)
                                                {
                                                    valueOk = true;
                                                    targetXyStartupX = static_cast<quint16>(x * 65535.0);
                                                    if (targetXyStartupX > 0xFEFF) { targetXyStartupX = 0xFEFF; }
                                                    else if (targetXyStartupX == 0) { targetXyStartupX = 1; }
                                                    targetXyStartupY = static_cast<quint16>(y * 65535.0);
                                                    if (targetXyStartupY > 0xFEFF) { targetXyStartupY = 0xFEFF; }
                                                    else if (targetXyStartupY == 0) { targetXyStartupY = 1; }
                                                    QVariantList xy;
                                                    toXy(targetXyStartupX, targetXyStartupY, xy);
                                                    value = xy;
                                                }
                                            }
                                        }
                                        else if (value.type() == QVariant::String && value.toString() == QLatin1String("previous"))
                                        {
                                            valueOk = true;
                                            targetXyStartupX = 0xFFFF;
                                            targetXyStartupY = 0xFFFF;
                                        }
                                        if (valueOk)
                                        {
                                            change.addTargetValue(RConfigColorXyStartupX, targetXyStartupX);
                                            change.addTargetValue(RConfigColorXyStartupY, targetXyStartupY);
                                            lightNode->setValue(RConfigColorXyStartupX, targetXyStartupX);
                                            lightNode->setValue(RConfigColorXyStartupY, targetXyStartupY);
                                            DB_StoreSubDeviceItem(lightNode, itemX);
                                            DB_StoreSubDeviceItem(lightNode, itemY);
                                        }
                                    }
                                }

                                if (!paramOk)
                                {
                                    rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, path2, QString("parameter, %1, not available").arg(key)));
                                }
                                else if (!valueOk)
                                {
                                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, path2, QString("invalid value, %1, for parameter, %2").arg(value.toString()).arg(key)));
                                }
                                else
                                {
                                    QVariantMap rspItem;
                                    QVariantMap rspItemState;
                                    rspItemState[path2] = value;
                                    rspItem["success"] = rspItemState;
                                    rsp.list.append(rspItem);
                                }
                            }
                        }
                    }

                    if (!paramOk)
                    {
                        rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, path1, QString("parameter, %1, not available").arg(key)));
                    }
                    else if (!valueOk)
                    {
                        rsp.list.append(errorToMap(ERR_INVALID_VALUE, path1, QString("invalid value, %1, for parameter, color/%2").arg(value.toString()).arg(key)));
                    }
                    else if (!isMap)
                    {
                        QVariantMap rspItem;
                        QVariantMap rspItemState;
                        rspItemState[path1] = value;
                        rspItem["success"] = rspItemState;
                        rsp.list.append(rspItem);
                    }
                }
            }
        }
        else if (key == "groups")
        {
            paramOk = true;
            valueOk = true;
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_MODIFIABLE, path, QString("parameter, %1, is not modifiable.").arg(key)));
        }
        else if (key == "on")
        {
            paramOk = true;
            if (value.type() == QVariant::Map)
            {
                valueOk = true;
                QVariantMap map1 = value.toMap();
                for (QVariantMap::const_iterator q = map1.begin(); q != map1.end(); q++)
                {
                    bool paramOk = false;
                    bool valueOk = false;
                    key = q.key();
                    QString path1 = QString("%1/%2").arg(path).arg(key);
                    value = map1[key];

                    if (key == "startup")
                    {
                        ResourceItem *item = lightNode->item(RConfigOnStartup);
                        if (item)
                        {
                            paramOk = true;
                            quint8 targetOnStartup;
                            if (value.type() == QVariant::Bool)
                            {
                                valueOk = true;
                                targetOnStartup = value.toBool() ? 1 : 0;
                            }
                            else if (value.type() == QVariant::String && value.toString() == QLatin1String("previous"))
                            {
                                valueOk = true;
                                targetOnStartup = 0xFF;
                            }
                            if (valueOk)
                            {
                                change.addTargetValue(RConfigOnStartup, targetOnStartup);
                                lightNode->setValue(RConfigOnStartup, targetOnStartup);
                                DB_StoreSubDeviceItem(lightNode, item);
                            }
                        }
                    }

                    if (!paramOk)
                    {
                        rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, path1, QString("parameter, %1, not available").arg(key)));
                    }
                    else if (!valueOk)
                    {
                        rsp.list.append(errorToMap(ERR_INVALID_VALUE, path1, QString("invalid value, %1, for parameter, %2").arg(value.toString()).arg(key)));
                    }
                    else
                    {
                        QVariantMap rspItem;
                        QVariantMap rspItemState;
                        rspItemState[path1] = value;
                        rspItem["success"] = rspItemState;
                        rsp.list.append(rspItem);
                    }
                }
            }
        }

        if (!paramOk)
        {
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, path, QString("parameter, %1, not available").arg(key)));
        }
        else if (!valueOk)
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, path, QString("invalid value, %1, for parameter, %2").arg(value.toString()).arg(key)));
        }
    }

    rsub->addStateChange(change);
    processTasks();
    return REQ_READY_SEND;
}

enum MultiStateOutputValue {
  Down = 0,
  Up = 1,
  Stop = 2,
  Toggle = 3,
  Blocked = 4,
  StepDown = 5,
  StepUp = 6
};

/*! PUT, PATCH /api/<apikey>/lights/<id>/state for Window covering "lights".
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::setWindowCoveringState(const ApiRequest &req, ApiResponse &rsp, TaskItem &taskRef, QVariantMap &map)
{
    static const QStringList alertList({
        "none", "select"
    });
    bool ok;
    bool supportsLiftInc = false;
    QString id = req.path[3];
    quint16 cluster = WINDOW_COVERING_CLUSTER_ID;
    // if (taskRef.lightNode->modelId().startsWith(QLatin1String("lumi.curtain"))) // FIXME - for testing only.
    if (taskRef.lightNode->modelId().startsWith(QLatin1String("lumi.curtain.")))
    {
        cluster = ANALOG_OUTPUT_CLUSTER_ID;
        supportsLiftInc = taskRef.lightNode->modelId().startsWith(QLatin1String("lumi.curtain.acn002"));
    }

    if (R_GetProductId(taskRef.lightNode).startsWith(QLatin1String("Tuya_COVD")))
    {
        cluster = TUYA_CLUSTER_ID;
    }

    bool requestOk = true;
    bool hasOpen = false;
    bool targetOpen = false;
    bool hasLift = false;
    bool hasLiftInc = false;
    bool hasStop = false;
    quint8 targetLift = 0;
    quint8 targetLiftZigBee = 0;
    qint8 targetLiftInc = 0;
    bool hasTilt = false;
    quint8 targetTilt = 0;
    QString alert;
    bool hasSpeed = false;
    quint8 targetSpeed = 0;

    // Check parameters.
    const auto mapEnd = map.cend();
    for (auto p = map.cbegin(); p != mapEnd; ++p)
    {
        bool paramOk = false;
        bool valueOk = false;
        QString param = p.key();
        if (param == "open" && taskRef.lightNode->item(RStateOpen))
        {
            paramOk = true;
            if (map[param].type() == QVariant::Bool)
            {
                valueOk = true;
                hasOpen = true;
                targetOpen = map[param].toBool();
            }
        }
        else if (param == "on" && taskRef.lightNode->item(RStateOn))
        {
            paramOk = true;
            if (map[param].type() == QVariant::Bool)
            {
                valueOk = true;
                hasOpen = true;
                targetOpen = !(map[param].toBool());
            }
        }
        else if (param == "stop" && taskRef.lightNode->item(RStateOpen))
        {
            paramOk = true;
            if (map[param].type() == QVariant::Bool)
            {
                valueOk = true;
                hasStop = true;
            }
        }
        else if (param == "lift" && taskRef.lightNode->item(RStateLift))
        {
            paramOk = true;
            if (map[param].type() == QVariant::String && map[param].toString() == "stop")
            {
                valueOk = true;
                hasStop = true;
            }
            else if (map[param].type() == QVariant::Double)
            {
                const uint lift = map[param].toUInt(&ok);
                if (ok && lift <= 100)
                {
                    valueOk = true;
                    hasLift = true;
                    targetLift = lift;
                }
            }
        }
        else if (param == "lift_inc" && taskRef.lightNode->item(RStateLift))
        {
            paramOk = true;
            if (map[param].type() == QVariant::Double)
            {
                const int liftInc = map[param].toUInt(&ok);
                if (ok && liftInc == 0)
                {
                    valueOk = true;
                    hasStop = true;
                }
                else if (ok && liftInc >= -100 && liftInc <= 100 && supportsLiftInc)
                {
                    valueOk = true;
                    hasLiftInc = true;
                    targetLiftInc = liftInc;
                }
            }
        }
        else if (param == "bri" && taskRef.lightNode->item(RStateBri))
        {
            paramOk = true;
            if (map[param].type() == QVariant::String && map[param].toString() == "stop")
            {
                valueOk = true;
                hasStop = true;
            }
            else if (map[param].type() == QVariant::Double)
            {
                const uint bri = map[param].toUInt(&ok);
                if (ok && bri <= 0xFF)
                {
                    valueOk = true;
                    hasLift = true;
                    targetLift = bri * 100 / 254;
                }
            }
        }
        else if (param == "bri_inc" && taskRef.lightNode->item(RStateBri))
        {
            paramOk = true;
            if (map[param].type() == QVariant::Double)
            {
                const int bri_inc = map[param].toInt(&ok);
                if (ok && bri_inc == 0)
                {
                    valueOk = true;
                    hasStop = true;
                }
            }
        }
        else if (param == "tilt" && taskRef.lightNode->item(RStateTilt))
        {
            paramOk = true;
            if (map[param].type() == QVariant::Double)
            {
                const uint tilt = map[param].toUInt(&ok);
                if (ok && tilt <= 100)
                {
                    valueOk = true;
                    hasTilt = true;
                    targetTilt = tilt;
                }
            }
        }
        else if (param == "sat" && taskRef.lightNode->item(RStateSat))
        {
            paramOk = true;
            if (map[param].type() == QVariant::Double)
            {
                const uint sat = map[param].toUInt(&ok);
                if (ok && sat <= 255)
                {
                    valueOk = true;
                    hasTilt = true;
                    targetTilt = sat * 100 / 254;
                }
            }
        }
        else if (param == "alert" && taskRef.lightNode->item(RStateAlert))
        {
            paramOk = true;
            if (map[param].type() == QVariant::String)
            {
                alert = map[param].toString();
                valueOk = alertList.contains(alert);
            }
        }
        else if (param == "speed" && taskRef.lightNode->item(RStateSpeed))
        {
            paramOk = true;
            if (map[param].type() == QVariant::Double)
            {
                const uint speed = map[param].toUInt(&ok);
                if (ok && speed <= 0xFF)
                {
                    valueOk = true;
                    hasSpeed = true;
                    targetSpeed = speed > 2 ? 2 : speed;
                }
            }
        }

        if (!paramOk)
        {
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/lights/%1/state").arg(id), QString("parameter, %1, not available").arg(param)));
            requestOk = false;
        }
        else if (!valueOk)
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1/state").arg(id), QString("invalid value, %1, for parameter, %2").arg(map[param].toString()).arg(param)));
            requestOk = false;
        }
    }
    if (!requestOk)
    {
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    // Some devices invert LiftPct.
    if (hasLift)
    {
        if (taskRef.lightNode->modelId().startsWith(QLatin1String("lumi.curtain")) ||
            R_GetProductId(taskRef.lightNode) == QLatin1String("11830304 Switch") ||
            R_GetProductId(taskRef.lightNode) == QLatin1String("Zigbee dual curtain switch") ||
            R_GetProductId(taskRef.lightNode) == QLatin1String("Covering Switch ESW-2ZAD-EU") ||
            R_GetProductId(taskRef.lightNode) == QLatin1String("QS-Zigbee-C01 Module") ||
            R_GetProductId(taskRef.lightNode) == QLatin1String("Zigbee curtain switch") ||
            R_GetProductId(taskRef.lightNode) == QLatin1String("Tuya_COVD YS-MT750") ||
            R_GetProductId(taskRef.lightNode) == QLatin1String("Tuya_COVD DS82") ||
            R_GetProductId(taskRef.lightNode) == QLatin1String("Tuya_COVD AM43-0.45/40-ES-EZ(TY)") ||
            taskRef.lightNode->modelId() == QLatin1String("Motor Controller"))
        {
            targetLiftZigBee = 100 - targetLift;
        }
        else if (taskRef.lightNode->modelId() == QLatin1String("Shutter switch with neutral") ||
                 taskRef.lightNode->modelId() == QLatin1String("Shutter SW with level control"))
        {
            // Legrand invert bri and don't support other value than 0
            bool bStatus = false;
            uint nHex = taskRef.lightNode->swBuildId().toUInt(&bStatus, 16);
            if (bStatus && nHex < 28)
            {
                targetLiftZigBee = targetLift == 0 ? 100 : 0;
            }
            else
            {
                targetLiftZigBee = targetLift == 100 ? 100 : 0;
            }
        }
        else
        {
            targetLiftZigBee = targetLift;
        }
    }

    //Some device don't support lift, but third app can use it
    if (hasLift)
    {
        if (taskRef.lightNode->manufacturer() == QLatin1String("_TYZB01_dazsid15") ||
            taskRef.lightNode->modelId() == QLatin1String("FB56+CUR17SB2.2"))
        {
            hasLift = false;
            hasOpen = true;
            if (targetLiftZigBee > 0)
            {
                targetOpen = false;
            }
            else
            {
                targetOpen = true;
            }
        }
    }

    // Send command(s) to device.  stop trumps lift trumps lift_inc trumps open.
    if (hasStop)
    {
        bool ok;
        TaskItem task;
        copyTaskReq(taskRef, task);

        if (cluster == TUYA_CLUSTER_ID)
        {
            if (R_GetProductId(taskRef.lightNode) == QLatin1String("Tuya_COVD AM43-0.45/40-ES-EZ(TY)"))
            {
                //This device use bad command
                ok = sendTuyaRequest(task, TaskTuyaRequest, DP_TYPE_ENUM, DP_IDENTIFIER_CONTROL, QByteArray("\x00", 1));
            }
            else
            {
                ok = sendTuyaRequest(task, TaskTuyaRequest, DP_TYPE_ENUM, DP_IDENTIFIER_CONTROL, QByteArray("\x01", 1));
            }
        }
        else if (cluster == ANALOG_OUTPUT_CLUSTER_ID)
        {
            quint16 value = MultiStateOutputValue::Stop;

            deCONZ::ZclAttribute attr(0x0055, deCONZ::Zcl16BitUint, "value", deCONZ::ZclReadWrite, true);
            attr.setValue(QVariant(value));
            taskRef.lightNode->rx(); // Tell writeAttribute() device is awake.
            ok = writeAttribute(taskRef.lightNode, taskRef.lightNode->haEndpoint().endpoint(), MULTISTATE_OUTPUT_CLUSTER_ID, attr);
        }
        else
        {
            ok = addTaskWindowCovering(task, WINDOW_COVERING_COMMAND_STOP, 0, 0);
        }

        if (ok)
        {
            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/lights/%1/state/stop").arg(id)] = true;
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);

            // Rely on attribute reporting to update the light state.
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1/state/stop").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
        }
    }
    else if (hasLift)
    {
        bool ok;
        TaskItem task;
        copyTaskReq(taskRef, task);

        if (cluster == TUYA_CLUSTER_ID)
        {
            QByteArray lev = QByteArray("\x00\x00\x00", 3);
            lev.append(targetLiftZigBee);
            ok = sendTuyaRequest(task, TaskTuyaRequest, DP_TYPE_VALUE, DP_IDENTIFIER_PERCENT_CONTROL, lev);
        }
        else if (cluster == ANALOG_OUTPUT_CLUSTER_ID)
        {
            float value = targetLiftZigBee;

            deCONZ::ZclAttribute attr(0x0055, deCONZ::ZclSingleFloat, "value", deCONZ::ZclReadWrite, true);
            attr.setValue(QVariant(value));
            taskRef.lightNode->rx(); // Tell writeAttribute() device is awake.
            ok = writeAttribute(taskRef.lightNode, taskRef.lightNode->haEndpoint().endpoint(), cluster, attr);
        }
        else
        {
            ok = addTaskWindowCovering(task, WINDOW_COVERING_COMMAND_GOTO_LIFT_PCT, 0, targetLiftZigBee);
        }

        if (ok)
        {
            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/lights/%1/state/lift").arg(id)] = targetLift;
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);


            // I m using this code only for Legrand ATM but can be used for other device.
            // Because the attribute reporting take realy long time to be done, can be 2 minutes
            // Or it can be changed only after this time, so using an read attribute don't give usable value
            // And can cause issue on some third app
            if (taskRef.lightNode->modelId() == QLatin1String("Shutter switch with neutral") ||
                taskRef.lightNode->modelId() == QLatin1String("Shutter SW with level control"))
            {
                taskRef.lightNode->setValue(RStateLift, 50);
                taskRef.lightNode->setValue(RStateBri, 127);
            }

            // Rely on attribute reporting to update the light state.
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1/state/lift").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
        }
    }
    else if (hasLiftInc)
    {
        TaskItem task;
        copyTaskReq(taskRef, task);

        if (cluster == ANALOG_OUTPUT_CLUSTER_ID)
        {
            quint16 value;
            if (targetLiftInc == 0)
            {
                value = MultiStateOutputValue::Stop;
            } else if (targetLiftInc > 0)
            {
                value = MultiStateOutputValue::StepDown;
                targetLiftInc = 1;
            } else {
                value = MultiStateOutputValue::StepUp;
                targetLiftInc = -1;
            }
            deCONZ::ZclAttribute attr(0x0055, deCONZ::Zcl16BitUint, "value", deCONZ::ZclReadWrite, true);
            attr.setValue(QVariant(value));
            taskRef.lightNode->rx(); // Tell writeAttribute() device is awake.
            if (writeAttribute(taskRef.lightNode, taskRef.lightNode->haEndpoint().endpoint(), MULTISTATE_OUTPUT_CLUSTER_ID, attr))
            {
                QVariantMap rspItem;
                QVariantMap rspItemState;
                rspItemState[QString("/lights/%1/state/lift_inc").arg(id)] = targetLiftInc;
                rspItem["success"] = rspItemState;
                rsp.list.append(rspItem);
            }
            else
            {
                rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1/state/lift_inc").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
            }
        }
    }
    else if (hasOpen)
    {
        bool ok;
        TaskItem task;
        copyTaskReq(taskRef, task);

        if (cluster == TUYA_CLUSTER_ID)
        {
            // Reverse side for open/close command
            bool targetOpen2 = targetOpen;
            if (R_GetProductId(taskRef.lightNode) == QLatin1String("Tuya_COVD M515EGB"))
            {
                targetOpen2 = !targetOpen;
            }

            if (targetOpen2)
            {
                ok = sendTuyaRequest(task, TaskTuyaRequest, DP_TYPE_ENUM, DP_IDENTIFIER_CONTROL, QByteArray("\x02", 1));
            }
            else
            {
                if (R_GetProductId(taskRef.lightNode) == QLatin1String("Tuya_COVD AM43-0.45/40-ES-EZ(TY)"))
                {
                    //This device use bad command
                    ok = sendTuyaRequest(task, TaskTuyaRequest, DP_TYPE_ENUM, DP_IDENTIFIER_CONTROL, QByteArray("\x01", 1));
                }
                else
                {
                    ok = sendTuyaRequest(task, TaskTuyaRequest, DP_TYPE_ENUM, DP_IDENTIFIER_CONTROL, QByteArray("\x00", 1));
                }
            }
        }
        else if (cluster == ANALOG_OUTPUT_CLUSTER_ID)
        {
            quint16 value = targetOpen ? MultiStateOutputValue::Up : MultiStateOutputValue::Down;

            deCONZ::ZclAttribute attr(0x0055, deCONZ::Zcl16BitUint, "value", deCONZ::ZclReadWrite, true);
            attr.setValue(QVariant(value));
            taskRef.lightNode->rx(); // Tell writeAttribute() device is awake.
            ok = writeAttribute(taskRef.lightNode, taskRef.lightNode->haEndpoint().endpoint(), MULTISTATE_OUTPUT_CLUSTER_ID, attr);
        }
        else
        {
            ok = addTaskWindowCovering(task, targetOpen ? WINDOW_COVERING_COMMAND_OPEN : WINDOW_COVERING_COMMAND_CLOSE, 0, 0);
        }

        if (ok)
        {
            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/lights/%1/state/open").arg(id)] = targetOpen;
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);

            // I m using this code only for Legrand ATM but can be used for other device.
            // Because the attribute reporting take realy long time to be done, can be 2 minutes
            // Or it can be changed only after this time, so using an read attribute don't give usable value
            // And can cause issue on some third app
            if (taskRef.lightNode->modelId() == QLatin1String("Shutter switch with neutral") ||
                taskRef.lightNode->modelId() == QLatin1String("Shutter SW with level control"))
            {
                taskRef.lightNode->setValue(RStateLift, 50);
                taskRef.lightNode->setValue(RStateBri, 127);
            }

            // Rely on attribute reporting to update the light state.
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1/state/open").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
        }
    }

    // Handle tilt independently from stop/lift/lift_inc/open.
    if (hasTilt)
    {
        TaskItem task;
        copyTaskReq(taskRef, task);

        if (addTaskWindowCovering(task, WINDOW_COVERING_COMMAND_GOTO_TILT_PCT, 0, targetTilt))
        {
            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/lights/%1/state/tilt").arg(id)] = targetTilt;
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);

            // Rely on attribute reporting to update the light state.
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1/state/tilt").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
        }
    }

    if (!alert.isEmpty())
    {
        TaskItem task;
        copyTaskReq(taskRef, task);
        task.taskType = TaskIdentify;
        task.identifyTime = alert == "select" ? 2 : 0;

        if (addTaskIdentify(task, task.identifyTime))
        {
            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/lights/%1/state/alert").arg(id)] = alert;
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);

            // Don't update write-only state.alert.
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1/state/alert").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
        }
    }

    if (hasSpeed)
    {
        TaskItem task;
        copyTaskReq(taskRef, task);

        deCONZ::ZclAttribute attr(0x0408, deCONZ::Zcl8BitUint, "speed", deCONZ::ZclReadWrite, true);
        attr.setValue(QVariant(targetSpeed));
        taskRef.lightNode->rx(); // Tell writeAttribute() device is awake.
        if (writeAttribute(taskRef.lightNode, taskRef.lightNode->haEndpoint().endpoint(), XIAOMI_CLUSTER_ID, attr, VENDOR_XIAOMI))
        {
            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/lights/%1/state/speed").arg(id)] = targetSpeed;
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);

            // Rely on attribute reporting to update the light state.
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1/state/speed").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
        }
    }

    rsp.etag = taskRef.lightNode->etag;
    processTasks();
    return REQ_READY_SEND;
}

//
// Tuya Devices
//
int DeRestPluginPrivate::setTuyaDeviceState(const ApiRequest &req, ApiResponse &rsp, TaskItem &taskRef, QVariantMap &map)
{
    QString id = req.path[3];

    bool targetOn = false;
    bool hasOn = false;
    bool hasBri = false;
    bool hasAlert = false;
    uint targetBri = 0;

    bool ok = false;

    //Parse all parameters
    for (QVariantMap::const_iterator p = map.begin(); p != map.end(); p++)
    {
        if (p.key() == "bri" && R_GetProductId(taskRef.lightNode).startsWith(QLatin1String("Tuya_DIMSWITCH")))
        {
            if (map[p.key()].type() == QVariant::Double)
            {
                targetBri = map["bri"].toUInt(&ok);
                if (ok && targetBri <= 0xFF)
                {
                    hasBri = true;
                }
            }

            if (!hasBri)
            {
                rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1").arg(id), QString("invalid value, %1, for parameter, bri").arg(map["bri"].toString())));
            }
        }

        else if (p.key() == "on" && taskRef.lightNode->item(RStateOn))
        {
            if (map[p.key()].type() == QVariant::Bool)
            {
                hasOn = true;
                targetOn = map["on"].toBool();
            }
            else
            {
                rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1/state").arg(id), QString("invalid value, %1, for parameter, on").arg(map["on"].toString())));
            }
        }

        else if (p.key() == "alert")
        {
            if (map[p.key()].type() == QVariant::String)
            {
                hasAlert = true;
            }
        }

        //Not used but can cause error
        else if (p.key() == "transitiontime")
        {
        }

        else
        {
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/lights/%1/state").arg(id), QString("parameter, %1, not available").arg(p.key())));
        }
    }

    // Return direct if there is already error
    if (!rsp.list.isEmpty())
    {
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    if (hasBri)
    {
        quint16 bri = targetBri * 1000 / 254;
        QByteArray data = QByteArray("\x00\x00", 2);
        data.append(static_cast<qint8>((bri >> 8) & 0xff));
        data.append(static_cast<qint8>(bri & 0xff));

        if (R_GetProductId(taskRef.lightNode) == QLatin1String("Tuya_DIMSWITCH Earda Dimmer") ||
            R_GetProductId(taskRef.lightNode) == QLatin1String("Tuya_DIMSWITCH MS-105Z") ||
            R_GetProductId(taskRef.lightNode) == QLatin1String("Tuya_DIMSWITCH EDM-1ZAA-EU"))
        {
            ok = sendTuyaRequest(taskRef, TaskTuyaRequest, DP_TYPE_VALUE, DP_IDENTIFIER_DIMMER_LEVEL_MODE2, data);
        }
        else
        {
            ok = sendTuyaRequest(taskRef, TaskTuyaRequest, DP_TYPE_VALUE, DP_IDENTIFIER_DIMMER_LEVEL_MODE1, data);
        }

        if (ok)
        {
            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/lights/%1/state/bri").arg(id)] = targetBri;
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
        }
    }

    if (hasOn)
    {
        qint8 button = DP_IDENTIFIER_BUTTON_1;
        QByteArray data;

        //Retreive Fake endpoint, and change button value
        const auto ep = taskRef.lightNode->haEndpoint().endpoint();
        if      (ep == 0x02) { button = DP_IDENTIFIER_BUTTON_2; }
        else if (ep == 0x03) { button = DP_IDENTIFIER_BUTTON_3; }

        //Use only the first endpoint for command
        taskRef.req.setDstEndpoint(0x01);

        DBG_Printf(DBG_INFO, "Tuya debug 10: EP: %d ID : %s\n", ep, qPrintable(id));

        if (targetOn)
        {
            data = QByteArray("\x01", 1);
        }
        else
        {
            data = QByteArray("\x00", 1);
        }

        ok = sendTuyaRequest(taskRef, TaskTuyaRequest, DP_TYPE_BOOL, button, data);

        if (ok)
        {
            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/lights/%1/state/on").arg(id)] = targetOn;
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
        }

    }

    if (hasAlert)
    {
        QByteArray data("\x00", 1);

        if (map["alert"].toString() == "lselect")
        {
            data = QByteArray("\x01",1);
        }

        if (sendTuyaRequest(taskRef, TaskTuyaRequest, DP_TYPE_BOOL, DP_IDENTIFIER_ALARM, data))
        {
            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/lights/%1/state/alert").arg(id)] = map["alert"].toString();
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
        }
    }

    return REQ_READY_SEND;
}

/*! PUT, PATCH /api/<apikey>/lights/<id>/state for Warning device "lights".
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::setWarningDeviceState(const ApiRequest &req, ApiResponse &rsp, TaskItem &taskRef, QVariantMap &map)
{
    bool ok;
    QString id = req.path[3];

    bool requestOk = true;
    bool hasCmd = false;
    QString alert;
    quint16 onTime = 0;
    static const QStringList alertList({ "none", "select", "lselect", "blink", "all" });

    // Check parameters.
    for (QVariantMap::const_iterator p = map.begin(); p != map.end(); p++)
    {
        bool paramOk = false;
        bool valueOk = false;
        QString param = p.key();

        if (param == "alert" && taskRef.lightNode->item(RStateAlert))
        {
            paramOk = true;
            hasCmd = true;
            if (map[param].type() == QVariant::String)
            {
                alert = map[param].toString();
                valueOk = alertList.contains(alert);
            }
        }
        else if (param == "ontime")
        {
            paramOk = true;
            if (map[param].type() == QVariant::Double)
            {
                const uint ot = map[param].toUInt(&ok);
                if (ok && ot < 0xFFFF)
                {
                    valueOk = true;
                    onTime = ot;
                }
            }
        }
        if (!paramOk)
        {
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/lights/%1/state").arg(id).arg(param), QString("parameter, %1, not available").arg(param)));
            requestOk = false;
        }
        else if (!valueOk)
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1/state/%2").arg(id).arg(param), QString("invalid value, %1, for parameter, %2").arg(map[param].toString()).arg(param)));
            requestOk = false;
        }
    }
    if (onTime > 0 && alert.isEmpty())
    {
        rsp.list.append(errorToMap(ERR_MISSING_PARAMETER, QString("/lights/%1/state").arg(id), QString("missing parameter, alert, for parameter, ontime")));
        requestOk = false;
    }
    if (requestOk && !hasCmd)
    {
        rsp.list.append(errorToMap(ERR_MISSING_PARAMETER, QString("/lights/%1/state").arg(id), QString("missing parameter to set warning device state")));
        requestOk = false;
    }
    if (!requestOk)
    {
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    if (taskRef.lightNode->node()->isZombie() || !taskRef.lightNode->lastRx().isValid())
    {
        DBG_Printf(DBG_INFO,"0x%016llX: resurrecting zombie siren\n", taskRef.lightNode->address().ext());
        taskRef.lightNode->rx(); // FIXME: this incorrectly updates `lastseen`
    }

    TaskItem task;
    copyTaskReq(taskRef, task);
    task.taskType = TaskWarning;

    if (!alert.isEmpty())
    {
        if (alert == "none")
        {
            task.options = 0x00; // Warning mode 0 (no warning), No strobe, Low sound
            task.duration = 0;
        }
        else if (alert == "select")
        {
            task.options = 0x17; // Warning mode 1 (burglar), Strobe, Very high sound
            if (taskRef.lightNode->modelId().startsWith(QLatin1String("902010/24")) ||
                taskRef.lightNode->modelId() == QLatin1String("902010/29"))
            {
                task.options = 0x12;    // Warning mode 1 (burglar), no Strobe, high sound
            }
            else if (taskRef.lightNode->modelId() == QLatin1String("SIRZB-110"))    // Doesn't support strobe
            {
                task.options = 0xC1;    // Warning mode 1 (burglar), no Strobe, Very high sound, Develco uses inversed bit order
            }
	    else if (taskRef.lightNode->modelId() == QLatin1String("TS0219"))
	    {
	        task.options = 0x24;    // Only supported combination
	    }
            task.duration = 1;
        }
        else if (alert == "lselect")
        {
            task.options = 0x17; // Warning mode 1 (burglar), Strobe, Very high sound
            if (taskRef.lightNode->modelId().startsWith(QLatin1String("902010/24")) ||
                taskRef.lightNode->modelId() == QLatin1String("902010/29"))
            {
                task.options = 0x12;    // Warning mode 1 (burglar), no Strobe, high sound
            }
            else if (taskRef.lightNode->modelId() == QLatin1String("SIRZB-110"))    // Doesn't support strobe
            {
                task.options = 0xC1;    // Warning mode 1 (burglar), no Strobe, Very high sound, Develco uses inversed bit order
            }
            task.duration = onTime > 0 ? onTime : 300;
        }
        else if (alert == "blink")
        {
            task.options = 0x04; // Warning mode 0 (no warning), Strobe, Low sound
            task.duration = onTime > 0 ? onTime : 300;
        }
        else if (alert == "all")
        {
            // FXIME: Dirty hack to send a network-wide broadcast to activate all sirens.
            task.req.dstAddress().setNwk(deCONZ::BroadcastAll);
            task.req.setDstAddressMode(deCONZ::ApsNwkAddress);
            task.req.setTxOptions(0);
            task.req.setDstEndpoint(0xFF);
            task.options = 0x17; // Warning mode 1 (burglar), Strobe, Very high sound
            task.duration = onTime > 0 ? onTime : 1;
        }

        if (addTaskWarning(task, task.options, task.duration))
        {
            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/lights/%1/state/alert").arg(id)] = alert;
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);
            // Don't update write-only state.alert.
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1/state/alert").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
        }
    }

    rsp.etag = taskRef.lightNode->etag;
    processTasks();
    return REQ_READY_SEND;
}

/*! PUT, PATCH /api/<apikey>/lights/<id>/state for Door Lock "lights".
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::setDoorLockState(const ApiRequest &req, ApiResponse &rsp, TaskItem &taskRef, QVariantMap &map)
{
    static const QStringList alertList({
        "none", "select"
    });
    QString id = req.path[3];

    bool requestOk = true;
    bool hasCmd = false;
    bool hasOn = false;
    bool targetOn = false;
    QString alert;

    // Check parameters.
    for (QVariantMap::const_iterator p = map.begin(); p != map.end(); p++)
    {
        bool paramOk = false;
        bool valueOk = false;
        QString param = p.key();

        if (param == "on" && taskRef.lightNode->item(RStateOn))
        {
            paramOk = true;
            hasCmd = true;
            if (map[param].type() == QVariant::Bool)
            {
                valueOk = true;
                hasOn = true;
                targetOn = map[param].toBool();
            }
        }
        else if (param == "alert" && taskRef.lightNode->item(RStateAlert))
        {
            paramOk = true;
            hasCmd = true;
            if (map[param].type() == QVariant::String)
            {
                alert = map[param].toString();
                valueOk = alertList.contains(alert);
            }
        }
        if (!paramOk)
        {
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/lights/%1/state").arg(id).arg(param), QString("parameter, %1, not available").arg(param)));
            requestOk = false;
        }
        else if (!valueOk)
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1/state/%2").arg(id).arg(param), QString("invalid value, %1, for parameter, %2").arg(map[param].toString()).arg(param)));
            requestOk = false;
        }
    }
    if (requestOk && !hasCmd)
    {
        rsp.list.append(errorToMap(ERR_MISSING_PARAMETER, QString("/lights/%1/state").arg(id), QString("missing parameter to set door lock state")));
        requestOk = false;
    }
    if (!requestOk)
    {
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    // state.on
    if (hasOn)
    {
        TaskItem task;
        copyTaskReq(taskRef, task);

        if (addTaskDoorLockUnlock(task, targetOn ? 0x00 /*Lock*/ : 0x01 /*Unlock*/))
        {
            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/lights/%1/state/on").arg(id)] = targetOn;
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1/state/on").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
        }
    }

    // state.alert
    if (!alert.isEmpty())
    {
        TaskItem task;
        copyTaskReq(taskRef, task);
        task.taskType = TaskIdentify;

        if (alert == "none")
        {
            task.identifyTime = 0;
        }
        else if (alert == "select")
        {
            task.identifyTime = 2;    // Hue lights don't react to 1.
        }
        else if (alert == "lselect")
        {
            task.identifyTime = taskRef.onTime > 0 ? taskRef.onTime : 15; // Default for Philips Hue bridge
        }

        if (addTaskIdentify(task, task.identifyTime))
        {
            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/lights/%1/state/alert").arg(id)] = alert;
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);

            // Don't update write-only state.alert.
        }
        else if (task.taskType == TaskIdentify || task.taskType == TaskTriggerEffect)
        {
            rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
        }
    }

    rsp.etag = taskRef.lightNode->etag;
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
        ResourceItem *item = lightNode->item(RAttrPowerup);

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
                Event e(RLights, RAttrPowerup, lightNode->id(), item);
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

    // Tuya options
    // Reverse covering
    if (map.contains("reverse"))
    {

        TaskItem taskRef;
        taskRef.lightNode = getLightNodeForId(id);

        if (!taskRef.lightNode || taskRef.lightNode->state() == LightNode::StateDeleted)
        {
            rsp.httpStatus = HttpStatusNotFound;
            rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/lights/%1").arg(id), QString("resource, /lights/%1, not available").arg(id)));
            return REQ_READY_SEND;
        }

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
        //taskRef.transitionTime = 4;
        //taskRef.onTime = 0;

        QByteArray direction = QByteArray("\x00", 1);
        if (map["reverse"].toBool())
        {
            direction = QByteArray("\x01", 1);
        }

        if (sendTuyaRequest(taskRef, TaskTuyaRequest, DP_TYPE_ENUM, DP_IDENTIFIER_WORK_STATE, direction))
        {
            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/lights/%1/reverse").arg(id)] = map["reverse"];
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);
            rsp.etag = lightNode->etag;

            return REQ_READY_SEND;
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1/reverse").arg(id), QString("invalid value, %1, for parameter reverse").arg(map["reverse"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }

    // Calibration command used for covering
    if (map.contains("calibration"))
    {

        TaskItem taskRef;
        taskRef.lightNode = getLightNodeForId(id);

        if (!taskRef.lightNode || taskRef.lightNode->state() == LightNode::StateDeleted)
        {
            rsp.httpStatus = HttpStatusNotFound;
            rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/lights/%1").arg(id), QString("resource, /lights/%1, not available").arg(id)));
            return REQ_READY_SEND;
        }

        if (!taskRef.lightNode->isAvailable())
        {
            rsp.httpStatus = HttpStatusOk;
            rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/lights/%1").arg(id), QString("resource, /lights/%1, not available").arg(id)));
            return REQ_READY_SEND;
        }

        qint64 value = 0x00;
        if (map["calibration"].toBool())
        {
            value = 0x01;
        }

        deCONZ::ZclAttribute attr(0xf001, deCONZ::Zcl8BitEnum, "calibration", deCONZ::ZclReadWrite, true);
        attr.setValue(value);

        if (writeAttribute(taskRef.lightNode, taskRef.lightNode->haEndpoint().endpoint(), WINDOW_COVERING_CLUSTER_ID, attr))
        {
            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/lights/%1/calibration").arg(id)] = map["calibration"];
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);
            rsp.etag = lightNode->etag;

            return REQ_READY_SEND;
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1/calibration").arg(id), QString("invalid value, %1, for parameter calibration").arg(map["calibration"].toString())));
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

    if (strncmp(e.what(), "attr/", 5) == 0 || strncmp(e.what(), "cap/", 4) == 0 ||
        strncmp(e.what(), "config/", 7) == 0 || strncmp(e.what(), "state/", 6) == 0)
    {
        ResourceItem *item = lightNode->item(e.what());
        if (item)
        {
            if (!(item->needPushSet() || item->needPushChange()))
            {
                return; // already pushed
            }

            bool pushAttr = false;
            bool pushCap = false;
            bool pushConfig = false;
            bool pushState = false;

            QVariantMap attr;

            QVariantMap capabilities;
            QVariantMap capabilitiesBri;
            QVariantMap capabilitiesColor;
            ResourceItem *icc = nullptr;
            QVariantMap capabilitiesColorCt;
            QVariantMap capabilitiesColorGradient;
            QVariantMap capabilitiesColorXy;
            ResourceItem *ibluex = nullptr;
            ResourceItem *ibluey = nullptr;
            ResourceItem *igreenx = nullptr;
            ResourceItem *igreeny = nullptr;
            ResourceItem *iredx = nullptr;
            ResourceItem *iredy = nullptr;
            QStringList effectList = RStateEffectValues;

            QVariantMap config;
            QVariantMap configBri;
            QVariantMap configColor;
            QVariantMap configColorCt;
            QVariantMap configColorGradient;
            QVariantMap configColorXy;
            ResourceItem *isx = nullptr;
            ResourceItem *isy = nullptr;
            QVariantMap configOn;

            QVariantMap state;
            ResourceItem *ialert = nullptr;
            const QStringList *capabilitiesAlerts = &RStateAlertValues;
            ResourceItem *ix = nullptr;
            ResourceItem *iy = nullptr;

            for (int i = 0; i < lightNode->itemCount(); i++)
            {
                item = lightNode->itemForIndex(static_cast<size_t>(i));
                DBG_Assert(item);

                const ResourceItemDescriptor &rid = item->descriptor();

                if      (rid.suffix == RCapColorCapabilities) { icc = item; }
                else if (rid.suffix == RCapColorXyBlueX) { ibluex = item; }
                else if (rid.suffix == RCapColorXyBlueY) { ibluey = item; }
                else if (rid.suffix == RCapColorXyGreenX) { igreenx = item; }
                else if (rid.suffix == RCapColorXyGreenY) { igreeny = item; }
                else if (rid.suffix == RCapColorXyRedX) { iredx = item; }
                else if (rid.suffix == RCapColorXyRedY) { iredy = item; }
                else if (rid.suffix == RConfigColorXyStartupX) { isx = item; }
                else if (rid.suffix == RConfigColorXyStartupY) { isy = item; }
                else if (rid.suffix == RStateAlert) { ialert = item; }
                else if (rid.suffix == RStateX) { ix = item; }
                else if (rid.suffix == RStateY) { iy = item; }
                else if (gwWebSocketNotifyAll || item->needPushChange())
                {
                    if      (rid.suffix == RAttrConfigId) { attr["configid"] = item->toNumber(); }
                    else if (rid.suffix == RAttrId) { attr["id"] = item->toString(); }
                    else if (rid.suffix == RAttrLastAnnounced) { attr["lastannounced"] = item->toString(); }
                    else if (rid.suffix == RAttrLastSeen) { attr["lastseen"] = item->toString(); }
                    else if (rid.suffix == RAttrLevelMin) { attr["levelmin"] = item->toNumber(); }
                    else if (rid.suffix == RAttrManufacturerName) { attr["manufacturername"] = item->toString(); }
                    else if (rid.suffix == RAttrModelId) { attr["modelid"] = item->toString(); }
                    else if (rid.suffix == RAttrName) { attr["name"] = item->toString(); }
                    else if (rid.suffix == RAttrPowerOnLevel) { attr["poweronlevel"] = item->toNumber(); }
                    else if (rid.suffix == RAttrPowerOnCt) { attr["poweronct"] = item->toNumber(); }
                    else if (rid.suffix == RAttrPowerup) { attr["powerup"] = item->toNumber(); }
                    else if (rid.suffix == RAttrProductId) { attr["productid"] = item->toString(); }
                    else if (rid.suffix == RAttrProductName) { attr["productname"] = item->toString(); }
                    else if (rid.suffix == RAttrSwconfigid) {attr["swconfigid"] = item->toString(); }
                    else if (rid.suffix == RAttrSwVersion) { attr["swversion"] = item->toString(); }
                    else if (rid.suffix == RAttrType) { attr["type"] = item->toString(); }
                    else if (rid.suffix == RAttrUniqueId) { attr["uniqueid"] = item->toString(); }
                    else if (rid.suffix == RCapAlertTriggerEffect) { capabilitiesAlerts = &RStateAlertValuesTriggerEffect; }
                    else if (rid.suffix == RCapBriMinDimLevel) { capabilitiesBri["min_dim_level"] = round(item->toNumber() / 10.0) / 100.0; }
                    else if (rid.suffix == RCapColorCtComputesXy) { capabilitiesColorCt["computes_xy"] = item->toBool(); }
                    else if (rid.suffix == RCapColorCtMax)
                    {
                        attr["ctmax"] = item->toNumber();
                        if (item->needPushChange()) { pushAttr = true; }
                        capabilitiesColorCt["max"] = item->toNumber();
                    }
                    else if (rid.suffix == RCapColorCtMin)
                    {
                        attr["ctmin"] = item->toNumber();
                        if (item->needPushChange()) { pushAttr = true; }
                        capabilitiesColorCt["min"] = item->toNumber();
                    }
                    else if (rid.suffix == RCapColorEffects) { effectList = getHueEffectNames(item->toNumber()); }
                    else if (rid.suffix == RCapColorGamutType) { capabilitiesColor["gamut_type"] = item->toString(); }
                    else if (rid.suffix == RCapColorGradientMaxSegments) { capabilitiesColorGradient["max_segments"] = item->toNumber(); }
                    else if (rid.suffix == RCapColorGradientPixelCount) { capabilitiesColorGradient["pixel_count"] = item->toNumber(); }
                    else if (rid.suffix == RCapColorGradientPixelLength) { capabilitiesColorGradient["pixel_length"] = item->toNumber(); }
                    else if (rid.suffix == RCapColorGradientStyles) { capabilitiesColorGradient["styles"] = getHueGradientStyleNames(item->toNumber()); }
                    else if (rid.suffix == RCapSleeper) { capabilities["sleeper"] = true; }
                    else if (rid.suffix == RCapTransitionBlock) { capabilities["transition_block"] = true; }
                    else if (rid.suffix == RConfigBriExecuteIfOff) { configBri["execute_if_off"] = item->toBool(); }
                    else if (rid.suffix == RConfigBriMax) { configBri["max"] = item->toNumber(); }
                    else if (rid.suffix == RConfigBriMin) { configBri["min"] = item->toNumber(); }
                    else if (rid.suffix == RConfigBriOnLevel) { configBri["on_level"] = item->toNumber() == 0xFF ? QVariant(QLatin1String("previous")) : item->toNumber(); }
                    else if (rid.suffix == RConfigBriMin) { configBri["onoff_transition_time"] = item->toNumber(); }
                    else if (rid.suffix == RConfigBriStartup) { configBri["startup"] = item->toNumber() == 0xFF ? QVariant(QLatin1String("previous")) : item->toNumber(); }
                    else if (rid.suffix == RConfigColorCtStartup) { configColorCt["startup"] = item->toNumber() == 0xFFFF ? QVariant(QLatin1String("previous")) : item->toNumber(); }
                    else if (rid.suffix == RConfigColorExecuteIfOff) { configColor["execute_if_off"] = item->toBool(); }
                    else if (rid.suffix == RConfigColorGradientReversed) { configColorGradient["reversed"] = item->toBool(); }
                    else if (rid.suffix == RConfigOnStartup) { configOn["startup"] = item->toNumber() == 0xFF ? QVariant(QLatin1String("previous")) : item->toBool(); }
                    else if (rid.suffix == RStateBri) { state["bri"] = static_cast<double>(item->toNumber()); }
                    else if (rid.suffix == RStateColorMode) { state["colormode"] = item->toString(); }
                    else if (rid.suffix == RStateCt) { state["ct"] = static_cast<double>(item->toNumber()); }
                    else if (rid.suffix == RStateEffect) { state["effect"] = item->toString(); }
                    else if (rid.suffix == RStateGradient)
                    {
                        bool ok;
                        QVariant var = Json::parse(item->toString(), ok);
                        if (ok)
                        {
                            QVariantMap map = var.toMap();
                            state["gradient"] = map;
                        }
                    }
                    else if (rid.suffix == RStateHue) { state["hue"] = static_cast<double>(item->toNumber()); }
                    else if (rid.suffix == RStateLift) { state["lift"] = item->toNumber(); }
                    else if (rid.suffix == RStateOn) { state["on"] = item->toBool(); }
                    else if (rid.suffix == RStateOpen) { state["open"] = item->toBool(); }
                    else if (rid.suffix == RStateReachable) { state["reachable"] = item->toBool(); }
                    else if (rid.suffix == RStateSat) { state["sat"] = static_cast<double>(item->toNumber()); }
                    else if (rid.suffix == RStateSpeed) { state["speed"] = item->toNumber(); }
                    else if (rid.suffix == RStateTilt) { state["tilt"] = item->toNumber(); }
                    else
                    {
                        item->clearNeedPush();
                    }

                    // TODO make declarative
                    if ((rid.suffix == RStateOn || rid.suffix == RStateReachable) &&
                        item->needPushChange() && !lightNode->groups().empty())
                    {
                        std::vector<GroupInfo>::const_iterator g = lightNode->groups().begin();
                        std::vector<GroupInfo>::const_iterator gend = lightNode->groups().end();
                        for (; g != gend; ++g)
                        {
                            if (g->state == GroupInfo::StateInGroup)
                            {
                                Event e(RGroups, REventCheckGroupAnyOn, int(g->id));
                                enqueueEvent(e);
                            }
                        }
                    }

                    if (item->needPushChange())
                    {
                        // TODO make declarative
                        if (strncmp(rid.suffix, "attr/", 5) == 0) { pushAttr = true; }
                        if (strncmp(rid.suffix, "cap/", 4) == 0) { pushCap = true; }
                        if (strncmp(rid.suffix, "config/", 7) == 0) { pushConfig = true; }
                        if (strncmp(rid.suffix, "state/", 6) == 0) { pushState = true; }
                    }
                    item->clearNeedPush();
                }
            }

            if (icc)
            {
                if (gwWebSocketNotifyAll || icc->needPushChange())
                {
                    const int cc = icc->toNumber();
                    QStringList colorModes;

                    if (cc & 0x10) colorModes.push_back(QLatin1String("ct"));
                    if (cc & 0x04)
                    {
                        colorModes.push_back(QLatin1String("effect"));
                        if (lightNode->manufacturerCode() == VENDOR_MUELLER)
                        {
                            effectList = RStateEffectValuesMueller;
                        }
                        capabilitiesColor["effects"] = effectList;
                    }
                    else if (isXmasLightStrip(lightNode))
                    {
                        colorModes.push_back(QLatin1String("effect"));
                        capabilitiesColor["effects"] = RStateEffectValuesXmasLightStrip;
                    }
                    if (!capabilitiesColorGradient.isEmpty()) colorModes.push_back(QLatin1String("gradient"));
                    if (cc & 0x01 || cc & 0x02) colorModes.push_back(QLatin1String("hs"));
                    if (cc & 0x08) colorModes.push_back(QLatin1String("xy"));

                    attr["colorcapabilities"] = cc;
                    if (icc->needPushChange()) { pushAttr = true; }
                    capabilitiesColor["modes"] = colorModes;
                    icc->clearNeedPush();
                }
            }

            if (ibluex && ibluey && igreenx && igreeny && iredx && iredy)
            {
                if (gwWebSocketNotifyAll || ibluex->needPushChange() || ibluey->needPushChange() ||
                    igreenx->needPushChange() || igreeny->needPushChange() || iredx->needPushChange() || iredy->needPushChange())
                {
                    QVariantList blue, green, red;

                    toXy(ibluex->toNumber(), ibluey->toNumber(), blue);
                    toXy(igreenx->toNumber(), igreeny->toNumber(), green);
                    toXy(iredx->toNumber(), iredy->toNumber(), red);
                    capabilitiesColorXy["blue"] = blue;
                    capabilitiesColorXy["green"] = green;
                    capabilitiesColorXy["red"] = red;
                    pushCap = true;
                    ibluex->clearNeedPush();
                    ibluey->clearNeedPush();
                    igreenx->clearNeedPush();
                    igreeny->clearNeedPush();
                    iredx->clearNeedPush();
                    iredy->clearNeedPush();
                }
            }

            if (isx && isy)
            {
                if (gwWebSocketNotifyAll || isx->needPushChange() || isy->needPushChange())
                {
                    double colorX = isx->toNumber();
                    double colorY = isy->toNumber();

                    if (colorX == 0xFFFF && colorY == 0xFFFF)
                    {
                        configColorXy["startup"] = QLatin1String("previous");
                    }
                    else
                    {
                        QVariantList xy;

                        toXy(colorX, colorY, xy);
                        configColorXy["startup"] = xy;
                    }
                    isx->clearNeedPush();
                    isy->clearNeedPush();
                }
            }

            if (ialert)
            {
                if (gwWebSocketNotifyAll || ialert->needPushChange())
                {
                    state["alert"] = QLatin1String("none");
                    ialert->clearNeedPush();
                }
            }

            if (ix && iy)
            {
                if (gwWebSocketNotifyAll || ix->needPushChange() || iy->needPushChange())
                {
                    QVariantList xy;

                    toXy(ix->toNumber(), iy->toNumber(), xy);
                    state["xy"] = xy;
                    ix->clearNeedPush();
                    iy->clearNeedPush();
                }
            }

            if (pushAttr)
            {
                QVariantMap map;
                map["t"] = QLatin1String("event");
                map["e"] = QLatin1String("changed");
                map["r"] = QLatin1String("lights");
                map["id"] = e.id();
                map["uniqueid"] = lightNode->uniqueId();
                map["attr"] = attr;
                webSocketServer->broadcastTextMessage(Json::serialize(map));
                updateLightEtag(lightNode);
                plugin->saveDatabaseItems |= DB_LIGHTS;
                plugin->queSaveDb(DB_LIGHTS, DB_SHORT_SAVE_DELAY);
            }

            if (!capabilitiesBri.isEmpty()) capabilities["bri"] = capabilitiesBri;
            if (!capabilitiesColorCt.isEmpty()) capabilitiesColor["ct"] = capabilitiesColorCt;
            if (!capabilitiesColorGradient.isEmpty()) capabilitiesColor["gradient"] = capabilitiesColorGradient;
            if (!capabilitiesColorXy.isEmpty()) capabilitiesColor["xy"] = capabilitiesColorXy;
            if (!capabilitiesColor.isEmpty()) capabilities["color"] = capabilitiesColor;
            if (pushCap)
            {
                if (ialert && gwWebSocketNotifyAll)
                {
                    capabilities["alerts"] = *capabilitiesAlerts;
                }

                QVariantMap map;
                map["t"] = QLatin1String("event");
                map["e"] = QLatin1String("changed");
                map["r"] = QLatin1String("lights");
                map["id"] = e.id();
                map["uniqueid"] = lightNode->uniqueId();
                map["capabilities"] = capabilities;
                webSocketServer->broadcastTextMessage(Json::serialize(map));
                updateLightEtag(lightNode);
                plugin->saveDatabaseItems |= DB_LIGHTS;
                plugin->queSaveDb(DB_LIGHTS, DB_SHORT_SAVE_DELAY);
            }

            if (!configBri.isEmpty()) config["bri"] = configBri;
            if (!configColorCt.isEmpty()) configColor["ct"] = configColorCt;
            if (!configColorGradient.isEmpty()) configColor["gradient"] = configColorGradient;
            if (!configColorXy.isEmpty()) configColor["xy"] = configColorXy;
            if (!configColor.isEmpty()) config["color"] = configColor;
            if (!configOn.isEmpty()) config["on"] = configOn;
            if (pushConfig)
            {
                QVariantMap map;
                map["t"] = QLatin1String("event");
                map["e"] = QLatin1String("changed");
                map["r"] = QLatin1String("lights");
                map["id"] = e.id();
                map["uniqueid"] = lightNode->uniqueId();
                map["config"] = config;
                webSocketServer->broadcastTextMessage(Json::serialize(map));
                updateLightEtag(lightNode);
                plugin->saveDatabaseItems |= DB_LIGHTS;
                plugin->queSaveDb(DB_LIGHTS, DB_SHORT_SAVE_DELAY);
            }

            if (pushState)
            {
                QVariantMap map;
                map["t"] = QLatin1String("event");
                map["e"] = QLatin1String("changed");
                map["r"] = QLatin1String("lights");
                map["id"] = e.id();
                map["uniqueid"] = lightNode->uniqueId();
                map["state"] = state;
                webSocketServer->broadcastTextMessage(Json::serialize(map));
                updateLightEtag(lightNode);
                plugin->saveDatabaseItems |= DB_LIGHTS;
                plugin->queSaveDb(DB_LIGHTS, DB_SHORT_SAVE_DELAY);
            }
        }
    }
    else if (e.what() == REventAdded)
    {
        QVariantMap res;
        res["name"] = lightNode->name();
        searchLightsResult[lightNode->id()] = res;

        QVariantMap lmap;
        QHttpRequestHeader hdr;  // dummy
        QStringList path;  // dummy
        ApiRequest req(hdr, path, nullptr, QLatin1String("")); // dummy
        req.mode = ApiModeNormal;
        lightToMap(req, lightNode, lmap);

        QVariantMap map;
        map["t"] = QLatin1String("event");
        map["e"] = QLatin1String("added");
        map["r"] = QLatin1String("lights");
        map["id"] = e.id();
        map["uniqueid"] = lightNode->uniqueId();
        map["light"] = lmap;

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
    setPermitJoinDuration(searchLightsTimeout);
}

/*! Handler for search lights active state.
 */
void DeRestPluginPrivate::searchLightsTimerFired()
{
    if (gwPermitJoinDuration == 0)
    {
        searchLightsTimeout = 0; // done
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
