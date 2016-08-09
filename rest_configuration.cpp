/*
 * Copyright (c) 2016 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QApplication>
#include <QDesktopServices>
#include <QFile>
#include <QString>
#include <QTcpSocket>
#include <QVariantMap>
#include <QNetworkInterface>
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "json.h"
#include <stdlib.h>
#include <QProcess>

/*! Configuration REST API broker.
    \param req - request data
    \param rsp - response data
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::handleConfigurationApi(const ApiRequest &req, ApiResponse &rsp)
{
    // POST /api
    if ((req.path.size() == 1) && (req.hdr.method() == "POST"))
    {
        return createUser(req, rsp);
    }

    // GET /api/<apikey>
    if ((req.path.size() == 2) && (req.hdr.method() == "GET"))
    {
        return getFullState(req, rsp);
    }
    // GET /api/<apikey>/config
    else if ((req.path.size() == 3) && (req.hdr.method() == "GET") && (req.path[2] == "config"))
    {
        return getConfig(req, rsp);
    }
    // PUT /api/<apikey>/config
    else if ((req.path.size() == 3) && (req.hdr.method() == "PUT") && (req.path[2] == "config"))
    {
        return modifyConfig(req, rsp);
    }
    // DELETE /api/<apikey>/config/whitelist/<username2>
    else if ((req.path.size() == 5) && (req.hdr.method() == "DELETE") && (req.path[2] == "config") && (req.path[3] == "whitelist"))
    {
        return deleteUser(req, rsp);
    }
    // POST /api/<apikey>/config/update
    else if ((req.path.size() == 4) && (req.hdr.method() == "POST") && (req.path[2] == "config") && (req.path[3] == "update"))
    {
        return updateSoftware(req, rsp);
    }
    // POST /api/<apikey>/config/updatefirmware
    else if ((req.path.size() == 4) && (req.hdr.method() == "POST") && (req.path[2] == "config") && (req.path[3] == "updatefirmware"))
    {
        return updateFirmware(req, rsp);
    }
    // POST /api/<apikey>/config/export
    else if ((req.path.size() == 4) && (req.hdr.method() == "POST") && (req.path[2] == "config") && (req.path[3] == "export"))
    {
        return exportConfig(req, rsp);
    }
    // POST /api/<apikey>/config/import
    else if ((req.path.size() == 4) && (req.hdr.method() == "POST") && (req.path[2] == "config") && (req.path[3] == "import"))
    {
        return importConfig(req, rsp);
    }
    // POST /api/<apikey>/config/reset
    else if ((req.path.size() == 4) && (req.hdr.method() == "POST") && (req.path[2] == "config") && (req.path[3] == "reset"))
    {
        return resetConfig(req, rsp);
    }
    // PUT /api/<apikey>/config/password
    else if ((req.path.size() == 4) && (req.hdr.method() == "PUT") && (req.path[2] == "config") && (req.path[3] == "password"))
    {
        return changePassword(req, rsp);
    }
    // DELETE /api/config/password
    else if ((req.path.size() == 3) && (req.hdr.method() == "DELETE") && (req.path[1] == "config") && (req.path[2] == "password"))
    {
        return deletePassword(req, rsp);
    }

    return REQ_NOT_HANDLED;
}

/*! POST /api
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::createUser(const ApiRequest &req, ApiResponse &rsp)
{
    bool ok;
    bool found = false; // already exist?
    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();
    ApiAuth auth;

    if (!gwLinkButton)
    {
        if (!allowedToCreateApikey(req))
        {
            rsp.httpStatus = HttpStatusForbidden;
            // rsp.httpStatus = HttpStatusUnauthorized;
            //rsp.hdrFields.append(qMakePair(QString("WWW-Authenticate"), QString("Basic realm=\"Enter Password\"")));
            rsp.list.append(errorToMap(ERR_LINK_BUTTON_NOT_PRESSED, "", "link button not pressed"));
            return REQ_READY_SEND;
        }
    }

    if (!ok || map.isEmpty())
    {
        rsp.httpStatus = HttpStatusBadRequest;
        rsp.list.append(errorToMap(ERR_INVALID_JSON, "", "body contains invalid JSON"));
        return REQ_READY_SEND;
    }

    if (!map.contains("devicetype")) // required
    {
        rsp.list.append(errorToMap(ERR_MISSING_PARAMETER, "", "missing parameters in body"));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    auth.devicetype = map["devicetype"].toString();

    // TODO check for valid devicetype

    if (map.contains("username")) // optional (note username = apikey)
    {
        if ((map["username"].type() != QVariant::String) ||
            (map["username"].toString().length() < 10))
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/"), QString("invalid value, %1, for parameter, username").arg(map["username"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

        auth.apikey = map["username"].toString();

        // check if this apikey is already known
        std::vector<ApiAuth>::const_iterator i = apiAuths.begin();
        std::vector<ApiAuth>::const_iterator end = apiAuths.end();

        for (; i != end; ++i)
        {
            if (auth.apikey == i->apikey)
            {
                found = true;
                break;
            }
        }
    }
    else
    {
        // create a random key (used only if not provided)
        for (int i = 0; i < 5; i++)
        {
            uint8_t rnd = (uint8_t)qrand();
            QString frac;
            frac.sprintf("%02X", rnd);
            auth.apikey.append(frac);
        }
    }

    QVariantMap map1;
    QVariantMap map2;
    map1["username"] = auth.apikey;
    map2["success"] = map1;
    rsp.list.append(map2);
    rsp.httpStatus = HttpStatusOk;

    if (!found)
    {
        auth.createDate = QDateTime::currentDateTimeUtc();
        auth.lastUseDate = QDateTime::currentDateTimeUtc();
        apiAuths.push_back(auth);
        queSaveDb(DB_AUTH, DB_SHORT_SAVE_DELAY);
        updateEtag(gwConfigEtag);
        DBG_Printf(DBG_INFO, "created username: %s, devicetype: %s\n", qPrintable(auth.apikey), qPrintable(auth.devicetype));
    }
    else
    {
        DBG_Printf(DBG_INFO, "apikey username: %s, devicetype: %s already exists\n", qPrintable(auth.apikey), qPrintable(auth.devicetype));
    }

    rsp.etag = gwConfigEtag;

    return REQ_READY_SEND;
}

/*! Puts all parameters in a map for later JSON serialization.
 */
void DeRestPluginPrivate::configToMap(const ApiRequest &req, QVariantMap &map)
{
    bool ok;
    QVariantMap whitelist;
    QVariantMap swupdate;
    QDateTime datetime = QDateTime::currentDateTimeUtc();
    QDateTime localtime = QDateTime::currentDateTime();

    QNetworkInterface eth;

    {
        QList<QNetworkInterface> ifaces = QNetworkInterface::allInterfaces();
        QList<QNetworkInterface>::Iterator i = ifaces.begin();
        QList<QNetworkInterface>::Iterator end = ifaces.end();

        // optimistic approach chose the first available ethernet interface
        for (;i != end; ++i)
        {
            if ((i->flags() & QNetworkInterface::IsUp) &&
                (i->flags() & QNetworkInterface::IsRunning) &&
                !(i->flags() & QNetworkInterface::IsLoopBack))
            {
                QList<QNetworkAddressEntry> addresses = i->addressEntries();

                if (!addresses.isEmpty())
                {
                    eth = *i;
                    break;
                }
            }
        }
    }

    ok = false;
    if (eth.isValid() && !eth.addressEntries().isEmpty())
    {
        QList<QNetworkAddressEntry> addresses = eth.addressEntries();
        QList<QNetworkAddressEntry>::Iterator i = addresses.begin();
        QList<QNetworkAddressEntry>::Iterator end = addresses.end();

        for (; i != end; ++i)
        {
            if (i->ip().protocol() == QAbstractSocket::IPv4Protocol)
            {
                map["ipaddress"] = i->ip().toString();
                map["netmask"] = i->netmask().toString();
                ok = true;
                break;
            }
        }

        map["mac"] = eth.hardwareAddress().toLower();
    }

    if (!ok)
    {
        map["mac"] = "38:60:77:7c:53:18";
        map["ipaddress"] = "127.0.0.1";
        map["netmask"] = "255.0.0.0";
        DBG_Printf(DBG_ERROR, "No valid ethernet interface found\n");
    }

    std::vector<ApiAuth>::const_iterator i = apiAuths.begin();
    std::vector<ApiAuth>::const_iterator end = apiAuths.end();
    for (; i != end; ++i)
    {
        if (i->state == ApiAuth::StateNormal)
        {
            QVariantMap au;
            au["last use date"] = i->lastUseDate.toString("yyyy-MM-ddTHH:mm:ss"); // ISO 8601
            au["create date"] = i->createDate.toString("yyyy-MM-ddTHH:mm:ss"); // ISO 8601
            au["name"] = i->devicetype;
            whitelist[i->apikey] = au;
        }
    }

    if (req.apiVersion() == ApiVersion_1_DDEL)
    {
        map["rfconnected"] = gwRfConnected;
        map["permitjoin"] = (double)gwPermitJoinDuration;
        map["otauactive"] = isOtauActive();
        map["otaustate"] = (isOtauBusy() ? "busy" : (isOtauActive() ? "idle" : "off"));
        map["groupdelay"] = (double)gwGroupSendDelay;
        map["discovery"] = (gwAnnounceInterval > 0);
        map["updatechannel"] = gwUpdateChannel;
        map["fwversion"] = gwFirmwareVersion;
        map["fwneedupdate"] = gwFirmwareNeedUpdate;
        if (gwFirmwareNeedUpdate)
        {
            map["fwversionupdate"] = gwFirmwareVersionUpdate;
        }
        map["announceurl"] = gwAnnounceUrl;
        map["announceinterval"] = (double)gwAnnounceInterval;
        map["swversion"] = GW_SW_VERSION;
        swupdate["version"] = gwUpdateVersion;
        swupdate["updatestate"] = (double)0;
        swupdate["url"] = "";
        swupdate["text"] = "";
        swupdate["notify"] = false;
        map["swupdate"] = swupdate;
        map["port"] = (double)(apsCtrl ? apsCtrl->getParameter(deCONZ::ParamHttpPort) : 80);
        // since api version 1.2.1
        map["apiversion"] = GW_SW_VERSION;
        map["system"] = "other";
#if defined(ARCH_ARMV6) || defined (ARCH_ARMV7)
#ifdef Q_OS_LINUX
        map["system"] = "linux-gw";
#endif
#endif
    }
    else
    {
        map["swversion"] = QString(GW_SW_VERSION).replace(QChar('.'), "");
        swupdate["updatestate"] = (double)0;
        swupdate["url"] = "";
        swupdate["text"] = "";
        swupdate["notify"] = false;
        map["swupdate"] = swupdate;
        // since api version 1.2.1
        map["apiversion"] = "1.0.0";
        // since api version 1.3.0
    }

    map["name"] = gwName;
    map["uuid"] = gwUuid;
    if (apsCtrl)
    {
        map["zigbeechannel"] = apsCtrl->getParameter(deCONZ::ParamCurrentChannel);
        map["panid"] = apsCtrl->getParameter(deCONZ::ParamPANID);
    }
    else
    {
        map["zigbeechannel"] = (double)gwZigbeeChannel;
    }
    map["dhcp"] = true; // dummy
    map["proxyaddress"] = ""; // dummy
    map["proxyport"] = (double)0; // dummy
    map["utc"] = datetime.toString("yyyy-MM-ddTHH:mm:ss"); // ISO 8601
    map["localtime"] = localtime.toString("yyyy-MM-ddTHH:mm:ss"); // ISO 8601
    map["timezone"] = gwTimezone;
    map["networkopenduration"] = gwNetworkOpenDuration;
    map["timeformat"] = gwTimeFormat;
    map["whitelist"] = whitelist;
    map["wifi"] = gwWifi;
    map["wifitype"] = gwWifiType;
    map["wifiname"] = gwWifiName;
    map["wifichannel"] = gwWifiChannel;
    //map["rgbwdisplay"] = gwRgbwDisplay;
    map["linkbutton"] = gwLinkButton;
    map["portalservices"] = false;

    gwIpAddress = map["ipaddress"].toString(); // cache
    gwPort = deCONZ::appArgumentNumeric("--http-port", 80); // cache

    QStringList ipv4 = gwIpAddress.split(".");

    if (ipv4.size() == 4)
    {
        ipv4.removeLast();
        ipv4.append("1");
        map["gateway"] = ipv4.join(".");
    }
    else
    {
        map["gateway"] = "192.168.178.1";
    }
}

/*! GET /api/<apikey>
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::getFullState(const ApiRequest &req, ApiResponse &rsp)
{
    if(!checkApikeyAuthentification(req, rsp))
    {
        return REQ_READY_SEND;
    }

    checkRfConnectState();

    // handle ETag
    if (req.hdr.hasKey("If-None-Match"))
    {
        QString etag = req.hdr.value("If-None-Match");

        if (gwConfigEtag == etag)
        {
            rsp.httpStatus = HttpStatusNotModified;
            rsp.etag = etag;
            return REQ_READY_SEND;
        }
    }

    QVariantMap lightsMap;
    QVariantMap groupsMap;
    QVariantMap configMap;
    QVariantMap schedulesMap;
    QVariantMap sensorsMap;

    // lights
    {
        std::vector<LightNode>::const_iterator i = nodes.begin();
        std::vector<LightNode>::const_iterator end = nodes.end();

        for (; i != end; ++i)
        {
            if (i->state() == LightNode::StateDeleted)
            {
                continue;
            }

            QVariantMap map;
            if (lightToMap(req, &(*i), map))
            {
                lightsMap[i->id()] = map;
            }
        }
    }

    // groups
    {
        std::vector<Group>::const_iterator i = groups.begin();
        std::vector<Group>::const_iterator end = groups.end();

        for (; i != end; ++i)
        {
            // ignore deleted groups
            if (i->state() == Group::StateDeleted || i->state() == Group::StateDeleteFromDB)
            {
                continue;
            }

            if (i->id() != "0")
            {
                QVariantMap map;
                if (groupToMap(&(*i), map))
                {
                    groupsMap[i->id()] = map;
                }
            }
        }
    }

    // schedules
    {
        std::vector<Schedule>::const_iterator i = schedules.begin();
        std::vector<Schedule>::const_iterator end = schedules.end();

        for (; i != end; ++i)
        {
            schedulesMap[i->id] = i->jsonMap;
        }
    }

    // sensors
    {
        std::vector<Sensor>::const_iterator i = sensors.begin();
        std::vector<Sensor>::const_iterator end = sensors.end();

        for (; i != end; ++i)
        {
            QVariantMap map;
            if (sensorToMap(&(*i), map))
            {
                sensorsMap[i->id()] = map;
            }
        }
    }
    configToMap(req, configMap);

    rsp.map["lights"] = lightsMap;
    rsp.map["groups"] = groupsMap;
    rsp.map["config"] = configMap;
    rsp.map["schedules"] = schedulesMap;
    rsp.map["sensors"] = sensorsMap;
    rsp.etag = gwConfigEtag;
    rsp.httpStatus = HttpStatusOk;
    return REQ_READY_SEND;
}

/*! GET /api/<apikey>/config
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::getConfig(const ApiRequest &req, ApiResponse &rsp)
{
    if(!checkApikeyAuthentification(req, rsp))
    {
        return REQ_READY_SEND;
    }

    checkRfConnectState();

    // handle ETag
    if (req.hdr.hasKey("If-None-Match"))
    {
        QString etag = req.hdr.value("If-None-Match");

        if (gwConfigEtag == etag)
        {
            rsp.httpStatus = HttpStatusNotModified;
            rsp.etag = etag;
            return REQ_READY_SEND;
        }
    }

    configToMap(req, rsp.map);
    rsp.httpStatus = HttpStatusOk;
    rsp.etag = gwConfigEtag;
    return REQ_READY_SEND;
}

/*! PUT /api/<apikey>/config
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::modifyConfig(const ApiRequest &req, ApiResponse &rsp)
{
    if(!checkApikeyAuthentification(req, rsp))
    {
        return REQ_READY_SEND;
    }

    bool ok;
    bool changed = false;
    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();

    DBG_Assert(apsCtrl != 0);

    if (!apsCtrl)
    {
        return REQ_NOT_HANDLED;
    }

    rsp.httpStatus = HttpStatusOk;

    if (!ok || map.isEmpty())
    {
        rsp.httpStatus = HttpStatusBadRequest;
        rsp.list.append(errorToMap(ERR_INVALID_JSON, "", "body contains invalid JSON"));
        return REQ_READY_SEND;
    }

    if (map.contains("name")) // optional
    {
        if ((map["name"].type() != QVariant::String) ||
            (map["name"].toString().length() > 16)) // TODO allow longer names
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/config/name"), QString("invalid value, %1, for parameter, name").arg(map["name"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

        QString name = map["name"].toString();

        if (gwName != name)
        {
            gwName = name;

            if (gwName.isEmpty())
            {
                gwName = GW_DEFAULT_NAME;
            }
            changed = true;
        }

        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState["/config/name"] = gwName;
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);

        // sync database
        gwConfig["name"] = gwName;
        queSaveDb(DB_CONFIG, DB_SHORT_SAVE_DELAY);
    }

    if (map.contains("rfconnected")) // optional
    {
        if (map["rfconnected"].type() != QVariant::Bool)
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/config/rfconnected"), QString("invalid value, %1, for parameter, rfconnected").arg(map["rfconnected"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

        // don't change network state if touchlink is busy
        if (touchlinkState != TL_Idle)
        {
            rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/config/rfconnected"), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

        bool rfconnected = map["rfconnected"].toBool();

        if (gwRfConnected != rfconnected)
        {
            gwRfConnected = rfconnected;
            changed = true;
        }

        // also check if persistent settings changed
        if (gwRfConnectedExpected != rfconnected)
        {
            gwRfConnectedExpected = rfconnected;
            queSaveDb(DB_CONFIG, DB_LONG_SAVE_DELAY);
        }

        if (apsCtrl->setNetworkState(gwRfConnected ? deCONZ::InNetwork : deCONZ::NotInNetwork) == deCONZ::Success)
        {
            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState["/config/rfconnected"] = gwRfConnected;
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);
        }
        else
        {
            rsp.list.append(errorToMap(ERR_DEVICE_OFF, QString("/config/rfconnected"), QString("Error, rfconnected, is not modifiable. Device is set to off.")));
        }
    }

    if (map.contains("updatechannel")) // optional
    {
        QString updatechannel = map["updatechannel"].toString();

        if ((map["updatechannel"].type() != QVariant::String) ||
               ! ((updatechannel == "stable") ||
                  (updatechannel == "alpha") ||
                  (updatechannel == "beta")))
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/config/updatechannel"), QString("invalid value, %1, for parameter, updatechannel").arg(map["updatechannel"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

        if (gwUpdateChannel != updatechannel)
        {
            gwUpdateChannel = updatechannel;
            gwUpdateVersion = GW_SW_VERSION; // will be replaced by discovery handler
            changed = true;
            queSaveDb(DB_CONFIG, DB_SHORT_SAVE_DELAY);
        }

        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState["/config/updatechannel"] = updatechannel;
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
    }

    if (map.contains("permitjoin")) // optional
    {
        int seconds = map["permitjoin"].toInt(&ok);
        if (!ok || !((seconds >= 0) && (seconds <= 255)))
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/config/permitjoin"), QString("invalid value, %1, for parameter, permitjoin").arg(map["permitjoin"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

        if (gwPermitJoinDuration != seconds)
        {
            changed = true;
        }

        setPermitJoinDuration(seconds);

        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState["/config/permitjoin"] = (double)seconds;
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
    }

    if (map.contains("groupdelay")) // optional
    {
        int milliseconds = map["groupdelay"].toInt(&ok);
        if (!ok || !((milliseconds >= 0) && (milliseconds <= MAX_GROUP_SEND_DELAY)))
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/config/groupdelay"), QString("invalid value, %1, for parameter, groupdelay").arg(map["groupdelay"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

        if (gwGroupSendDelay != milliseconds)
        {
            gwGroupSendDelay = milliseconds;
            queSaveDb(DB_CONFIG, DB_SHORT_SAVE_DELAY);
            changed = true;
        }

        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState["/config/groupdelay"] = (double)milliseconds;
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
    }

    if (map.contains("rgbwdisplay")) // optional
    {
        QString rgbwDisplay = map["rgbwdisplay"].toString();
        if (rgbwDisplay != "1" && rgbwDisplay != "2")
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/config/rgbwdisplay"), QString("invalid value, %1, for parameter, rgbwdisplay").arg(map["rgbwdisplay"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

        if (gwRgbwDisplay != rgbwDisplay)
        {
            gwRgbwDisplay = rgbwDisplay;
            queSaveDb(DB_CONFIG, DB_SHORT_SAVE_DELAY);
            changed = true;
        }

        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState["/config/rgbwdisplay"] = rgbwDisplay;
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
    }

    if (map.contains("wifi")) // optional
    {
        bool wifi = map["wifi"].toBool();

        if (map["wifi"].type() != QVariant::Bool)
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/config/wifi"), QString("invalid value, %1, for parameter, wifi").arg(map["wifi"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

        if (gwWifi != wifi)
        {
            gwWifi = wifi;
            changed = true;
        }

        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState["/config/wifi"] = wifi;
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
    }

    if (map.contains("wifitype")) // optional
    {
        QString wifiType = map["wifitype"].toString();

        if ((map["wifitype"].type() != QVariant::String) ||
               ! ((wifiType == "accesspoint") ||
                  (wifiType == "ad-hoc") ||
                  (wifiType == "client")))
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/config/wifitype"), QString("invalid value, %1, for parameter, wifitype").arg(map["wifitype"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

        if (gwWifiType != wifiType)
        {
            gwWifiType = wifiType;
            changed = true;
        }

        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState["/config/wifitype"] = wifiType;
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
    }

    if (map.contains("wifiname")) // optional
    {
        QString wifiName = map["wifiname"].toString();

        if ((map["wifiname"].type() != QVariant::String) ||
            (map["wifiname"].toString().length() > 32))
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/config/wifiname"), QString("invalid value, %1, for parameter, wifiname").arg(map["wifiname"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

        if (gwWifiName != wifiName)
        {
            gwWifiName = wifiName;
            changed = true;
        }

        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState["/config/wifiname"] = wifiName;
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
    }

    if (map.contains("wifichannel")) // optional
    {
        int wifiChannel = map["wifichannel"].toInt(&ok);
        if (!ok || !((wifiChannel >= 1) && (wifiChannel <= 11)))
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/config/wifichannel"), QString("invalid value, %1, for parameter, wifichannel").arg(map["wifichannel"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

        if (gwWifiChannel != wifiChannel)
        {
            gwWifiChannel = wifiChannel;
            changed = true;
        }

        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState["/config/wifichannel"] = (double)wifiChannel;
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
    }

    if (map.contains("wifipassword")) // optional
    {
        QString wifiPassword = map["wifipassword"].toString();

        if (map["wifipassword"].type() != QVariant::String ||
            wifiPassword.length() < 5 || wifiPassword.length() > 32)
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/config/wifipassword"), QString("invalid value, %1, for parameter, wifipassword").arg(map["wifipassword"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

#ifdef ARCH_ARM
#ifdef Q_OS_LINUX
        QDateTime now = QDateTime::currentDateTime();

        std::string command = "cp /etc/hostapd/hostapd.conf /etc/hostapd/hostapd_" + now.toString("yyyy-MM-ddThh_mm_ss").toStdString() + ".conf.bak";
        system(command.c_str());

        char const* cmd = "cat /etc/hostapd/hostapd.conf";
        FILE* pipe = popen(cmd, "r");
        if (!pipe)
        {
            rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/config/wifipassword"), QString("error while setting parameter wifipassword")));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
        char buffer[128];
        std::string result = "";
        while(!feof(pipe)) {
            if(fgets(buffer, 128, pipe) != NULL)
                result += buffer;
        }
        pclose(pipe);

        std::string temp = result.substr(result.find("wpa_passphrase="));
        int pos = temp.find('\n');

        result.replace(result.find("wpa_passphrase="), pos, "wpa_passphrase=" + wifiPassword.toStdString() + "\n");

        command = "echo " + result + " > /etc/hostapd/hostapd.conf";
        system(command.c_str());
#endif
#endif

        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState["/config/wifipassword"] = wifiPassword;
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
    }

    if (map.contains("otauactive")) // optional
    {
        bool otauActive = map["otauactive"].toBool();

        if (map["otauactive"].type() != QVariant::Bool)
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/config/otauactive"), QString("invalid value, %1, for parameter, otauactive").arg(map["otauactive"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

        if (isOtauActive() != otauActive)
        {
            changed = true;
            queSaveDb(DB_CONFIG, DB_SHORT_SAVE_DELAY);
        }

        apsCtrl->setParameter(deCONZ::ParamOtauActive, otauActive ? 1 : 0);

        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState["/config/otauactive"] = otauActive;
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
    }

    if (map.contains("discovery")) // optional
    {
        bool discovery = map["discovery"].toBool();

        if (map["discovery"].type() != QVariant::Bool)
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/config/discovery"), QString("invalid value, %1, for parameter, discovery").arg(map["discovery"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

        int minutes = gwAnnounceInterval;

        if (discovery)
        {
            setInternetDiscoveryInterval(ANNOUNCE_INTERVAL);
        }
        else
        {
            setInternetDiscoveryInterval(0);
        }

        if (minutes != gwAnnounceInterval)
        {
            queSaveDb(DB_CONFIG, DB_SHORT_SAVE_DELAY);
            changed = true;
        }

        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState["/config/discovery"] = discovery;
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
    }

    if (map.contains("unlock")) // optional
    {
        uint seconds = map["unlock"].toUInt(&ok);

        if (!ok || (seconds > MAX_UNLOCK_GATEWAY_TIME))
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/config/unlock"), QString("invalid value, %1, for parameter, unlock").arg(map["unlock"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

        lockGatewayTimer->stop();
        changed = true;

        if (seconds > 0)
        {
            gwLinkButton = true;
            lockGatewayTimer->start(seconds * 1000);
            DBG_Printf(DBG_INFO, "gateway unlocked\n");
        }
        else
        {
            gwLinkButton = false;
        }

        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState["/config/unlock"] = (double)seconds;
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
    }

    if (map.contains("zigbeechannel")) // optional
    {
        uint zigbeechannel = map["zigbeechannel"].toUInt(&ok);

        if (!ok || ((zigbeechannel != 0) && (zigbeechannel != 11) && (zigbeechannel != 15) && (zigbeechannel != 20) && (zigbeechannel != 25)))
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/config/zigbeechannel"), QString("invalid value, %1, for parameter, zigbeechannel").arg(map["zigbeechannel"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

            if (startChannelChange(zigbeechannel))
            {
                changed = true;
            }
            else
            {
                // not connected
            }


        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState["/config/zigbeechannel"] = (uint)zigbeechannel;
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
    }

    if (map.contains("networkopenduration")) // optional
    {
        int seconds = map["networkopenduration"].toInt(&ok);

        if (!ok)
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/config/networkopenduration"), QString("invalid value, %1, for parameter, networkopenduration").arg(map["networkopenduration"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

        if (gwNetworkOpenDuration != seconds)
        {
            DBG_Printf(DBG_INFO, "set gwNetworkOpenDuration to: %u\n", seconds);
            gwNetworkOpenDuration = seconds;
            changed = true;
            queSaveDb(DB_CONFIG, DB_SHORT_SAVE_DELAY);
        }

        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState["/config/networkopenduration"] = (double)seconds;
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
    }

    if (map.contains("timezone")) // optional
    {
        QString timezone = map["timezone"].toString();

        if (map["timezone"].type() != QVariant::String)
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/config/timezone"), QString("invalid value, %1, for parameter, timezone").arg(map["timezone"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

        if (gwTimezone != timezone)
        {
            gwTimezone = timezone;
            queSaveDb(DB_CONFIG, DB_SHORT_SAVE_DELAY);
            changed = true;
#ifdef ARCH_ARM
#ifdef Q_OS_LINUX
            //set timezone under gnu linux
            std::string command = "echo '" + timezone.toStdString() + "' | sudo tee /etc/timezone";
            system(command.c_str());

            command = "sudo dpkg-reconfigure -f noninteractive tzdata";
            system(command.c_str());
#endif
#endif
        }

        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState["/config/timezone"] = timezone;
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
    }

    if (map.contains("utc")) // optional
    {
        bool error = false;
        if ((map["utc"].type() != QVariant::String))
        {
            error = true;
        }
        else
        {
            QDateTime utc = QDateTime::fromString(map["utc"].toString(),"yyyy-MM-ddTHH:mm:ss");
            if (!utc.isValid())
            {
                error = true;
            }
        }

        if (error)
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/config/utc"), QString("invalid value, %1, for parameter, utc").arg(map["utc"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
/*      // not implementet yet
        #ifdef WIN32
            QString year = map["utc"].toString().mid(0, 4);
            QString month = map["utc"].toString().mid(5, 2);
            QString day = map["utc"].toString().mid(8, 2);
            QString time = map["utc"].toString().mid(11, 8);

            std::string command = "date " + day.toStdString() + "-" + month.toStdString() + "-" + year.toStdString();
            system(command.c_str());

            DBG_Printf(DBG_INFO, "command set date: %s\n", qPrintable(QString::fromStdString(command)));

            command = "time " + time.toStdString();
            system(command.c_str());

            DBG_Printf(DBG_INFO, "command set time: %s\n", qPrintable(QString::fromStdString(command)));
        #endif
*/
#ifdef ARCH_ARM
#ifdef Q_OS_LINUX
            QString date = map["utc"].toString().mid(0, 10);
            QString time = map["utc"].toString().mid(11, 8);

            std::string command = "sudo date -s " + date.toStdString();
            system(command.c_str());

            DBG_Printf(DBG_INFO, "command set date: %s\n", qPrintable(QString::fromStdString(command)));

            command = "sudo date -s " + time.toStdString();
            system(command.c_str());

            DBG_Printf(DBG_INFO, "command set time: %s\n", qPrintable(QString::fromStdString(command)));
#endif
#endif
        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState["/config/utc"] = map["utc"];
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
    }

    if (map.contains("timeformat")) // optional
    {
        QString timeFormat = map["timeformat"].toString();

        if ((map["timeformat"].type() != QVariant::String) ||
               !((timeFormat == "12h") || (timeFormat == "24h"))
           )
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/config/timeformat"), QString("invalid value, %1, for parameter, timeformat").arg(map["timeformat"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

        if (gwTimeFormat != timeFormat)
        {
            gwTimeFormat = timeFormat;
            changed = true;
            queSaveDb(DB_CONFIG, DB_SHORT_SAVE_DELAY);
        }

        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState["/config/timeformat"] = timeFormat;
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
    }

    if (changed)
    {
        updateEtag(gwConfigEtag);
    }

    rsp.etag = gwConfigEtag;

    return REQ_READY_SEND;
}

/*! DELETE /api/<apikey>/config/whitelist/<username2>
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::deleteUser(const ApiRequest &req, ApiResponse &rsp)
{
    if(!checkApikeyAuthentification(req, rsp))
    {
        return REQ_READY_SEND;
    }

    QString username2 = req.path[4];

    std::vector<ApiAuth>::iterator i = apiAuths.begin();
    std::vector<ApiAuth>::iterator end = apiAuths.end();

    for (; i != end; ++i)
    {
        if (username2 == i->apikey && i->state == ApiAuth::StateNormal)
        {
            i->state = ApiAuth::StateDeleted;
            queSaveDb(DB_AUTH, DB_LONG_SAVE_DELAY);

            QVariantMap rspItem;
            rspItem["success"] = QString("/config/whitelist/%1 deleted.").arg(username2);
            rsp.list.append(rspItem);

            return REQ_READY_SEND;
        }
    }

    rsp.str = "[]"; // empty
    rsp.httpStatus = HttpStatusOk;
    return REQ_READY_SEND;
}

/*! POST /api/<apikey>/config/update
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::updateSoftware(const ApiRequest &req, ApiResponse &rsp)
{
    if(!checkApikeyAuthentification(req, rsp))
    {
        return REQ_READY_SEND;
    }

    rsp.httpStatus = HttpStatusOk;
    QVariantMap rspItem;
    QVariantMap rspItemState;
    rspItemState["/config/update"] = gwUpdateVersion;
    rspItem["success"] = rspItemState;
    rsp.list.append(rspItem);

    // only supported on Raspberry Pi
#ifdef ARCH_ARM
    if (gwUpdateVersion != GW_SW_VERSION)
    {
        openDb();
        saveDb();
        closeDb();
        QTimer::singleShot(5000, this, SLOT(updateSoftwareTimerFired()));
    }
#endif // ARCH_ARM

    return REQ_READY_SEND;
}

/*! POST /api/<apikey>/config/updatefirmware
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::updateFirmware(const ApiRequest &req, ApiResponse &rsp)
{
    if(!checkApikeyAuthentification(req, rsp))
    {
        return REQ_READY_SEND;
    }

    if (startUpdateFirmware())
    {
        rsp.httpStatus = HttpStatusOk;
        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState["/config/updatefirmware"] = gwFirmwareVersionUpdate;
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
    }
    else
    {
        rsp.httpStatus = HttpStatusServiceUnavailable;
    }

    return REQ_READY_SEND;
}

/*! POST /api/<apikey>/config/export
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::exportConfig(const ApiRequest &req, ApiResponse &rsp)
{
    if(!checkApikeyAuthentification(req, rsp))
    {
        return REQ_READY_SEND;
    }

    if (exportConfiguration())
    {
        rsp.httpStatus = HttpStatusOk;
        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState["/config/export"] = "success";
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
    }
    else
    {
        rsp.httpStatus = HttpStatusServiceUnavailable;
    }

    return REQ_READY_SEND;
}

/*! POST /api/<apikey>/config/import
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::importConfig(const ApiRequest &req, ApiResponse &rsp)
{
    if(!checkApikeyAuthentification(req, rsp))
    {
        return REQ_READY_SEND;
    }

    if (importConfiguration())
    {
        rsp.httpStatus = HttpStatusOk;
        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState["/config/import"] = "success";
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);

        QTimer *restartTimer = new QTimer(this);
        restartTimer->setSingleShot(true);
        connect(restartTimer, SIGNAL(timeout()),
                this, SLOT(restartAppTimerFired()));
        restartTimer->start(SET_ENDPOINTCONFIG_DURATION);

    }
    else
    {
        rsp.httpStatus = HttpStatusServiceUnavailable;
    }


    return REQ_READY_SEND;
}

/*! POST /api/<apikey>/config/reset
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::resetConfig(const ApiRequest &req, ApiResponse &rsp)
{
    if(!checkApikeyAuthentification(req, rsp))
    {
        return REQ_READY_SEND;
    }

    bool resetGW = false;
    bool deleteDB = false;
    bool ok;
    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();

    if (!ok || map.isEmpty())
    {
        rsp.httpStatus = HttpStatusBadRequest;
        rsp.list.append(errorToMap(ERR_INVALID_JSON, "", "body contains invalid JSON"));
        return REQ_READY_SEND;
    }

    if ((!map.contains("resetGW")) || (!map.contains("deleteDB")))
    {
        rsp.httpStatus = HttpStatusBadRequest;
        rsp.list.append(errorToMap(ERR_MISSING_PARAMETER, "/config/reset", "missing parameters in body"));
        return REQ_READY_SEND;
    }

    if (map["resetGW"].type() != QVariant::Bool)
    {
        rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/config/reset"), QString("invalid value, %1, for parameter, resetGW").arg(map["resetGW"].toString())));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }
    if (map["deleteDB"].type() != QVariant::Bool)
    {
        rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/config/reset"), QString("invalid value, %1, for parameter, deleteDB").arg(map["deleteDB"].toString())));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }
    resetGW = map["resetGW"].toBool();
    deleteDB = map["deleteDB"].toBool();

    if (resetConfiguration(resetGW, deleteDB))
    {
        //kick all lights out of their groups that they will not recover their groups
        if (deleteDB)
        {
            std::vector<Group>::const_iterator g = groups.begin();
            std::vector<Group>::const_iterator gend = groups.end();

            for (; g != gend; ++g)
            {
                if (g->state() != Group::StateDeleted && g->state() != Group::StateDeleteFromDB)
                {
                    std::vector<LightNode>::iterator i = nodes.begin();
                    std::vector<LightNode>::iterator end = nodes.end();

                    for (; i != end; ++i)
                    {
                        GroupInfo *groupInfo = getGroupInfo(&(*i), g->address());

                        if (groupInfo)
                        {
                            groupInfo->actions &= ~GroupInfo::ActionAddToGroup; // sanity
                            groupInfo->actions |= GroupInfo::ActionRemoveFromGroup;
                            groupInfo->state = GroupInfo::StateNotInGroup;
                        }
                    }
                }
            }
        }
        rsp.httpStatus = HttpStatusOk;
        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState["/config/reset"] = "success";
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
        //wait some seconds that deCONZ can finish Enpoint config,
        //then restart app to apply network config (only on raspbee gw)

        QTimer *restartTimer = new QTimer(this);
        restartTimer->setSingleShot(true);
        connect(restartTimer, SIGNAL(timeout()),
                this, SLOT(restartAppTimerFired()));
        restartTimer->start(SET_ENDPOINTCONFIG_DURATION);

    }
    else
    {
        rsp.httpStatus = HttpStatusServiceUnavailable;
    }

    return REQ_READY_SEND;
}

/*! PUT /api/<apikey>/config/password
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::changePassword(const ApiRequest &req, ApiResponse &rsp)
{
    if(!checkApikeyAuthentification(req, rsp))
    {
        return REQ_READY_SEND;
    }

    bool ok;
    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();

    rsp.httpStatus = HttpStatusOk;

    if (!ok || map.isEmpty())
    {
        rsp.httpStatus = HttpStatusBadRequest;
        rsp.list.append(errorToMap(ERR_INVALID_JSON, "/config/password", "body contains invalid JSON"));
        return REQ_READY_SEND;
    }

    if (map.contains("username") && map.contains("oldhash") && map.contains("newhash"))
    {
        QString username = map["username"].toString();
        QString oldhash = map["oldhash"].toString();
        QString newhash = map["newhash"].toString();

        if ((map["username"].type() != QVariant::String) || (username != gwAdminUserName))
        {
            rsp.httpStatus = HttpStatusUnauthorized;
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, "/config/password", QString("invalid value, %1 for parameter, username").arg(username)));
            return REQ_READY_SEND;
        }

        if ((map["oldhash"].type() != QVariant::String) || oldhash.isEmpty())
        {
            rsp.httpStatus = HttpStatusUnauthorized;
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, "/config/password", QString("invalid value, %1 for parameter, oldhash").arg(oldhash)));
            return REQ_READY_SEND;
        }

        if ((map["newhash"].type() != QVariant::String) || newhash.isEmpty())
        {
            rsp.httpStatus = HttpStatusBadRequest;
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, "/config/password", QString("invalid value, %1 for parameter, newhash").arg(newhash)));
            return REQ_READY_SEND;
        }

        QString enc = encryptString(oldhash);

        if (enc != gwAdminPasswordHash)
        {
            rsp.httpStatus = HttpStatusUnauthorized;
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, "/config/password", QString("invalid value, %1 for parameter, oldhash").arg(oldhash)));
            return REQ_READY_SEND;
        }

        // username and old hash are okay
        // take the new hash and salt it
        enc = encryptString(newhash);
        gwAdminPasswordHash = enc;
        queSaveDb(DB_CONFIG, DB_SHORT_SAVE_DELAY);

        DBG_Printf(DBG_INFO, "Updated password hash: %s\n", qPrintable(enc));

        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState["/config/password"] = "changed";
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
        return REQ_READY_SEND;
    }
    else
    {
        rsp.httpStatus = HttpStatusBadRequest;
        rsp.list.append(errorToMap(ERR_MISSING_PARAMETER, "/config/password", "missing parameters in body"));
        return REQ_READY_SEND;
    }

    return REQ_READY_SEND;
}

/*! DELETE /api/config/password
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::deletePassword(const ApiRequest &req, ApiResponse &rsp)
{
    // reset only allowed within first 10 minutes after startup
    if (getUptime() > 600)
    {
        rsp.httpStatus = HttpStatusForbidden;
        rsp.list.append(errorToMap(ERR_UNAUTHORIZED_USER, req.path.join("/"), "unauthorized user"));
        return REQ_READY_SEND;
    }

    // create default password
    gwConfig.remove("gwusername");
    gwConfig.remove("gwpassword");

    initAuthentification();

    rsp.httpStatus = HttpStatusOk;
    return REQ_READY_SEND;
}

/*! Delayed trigger to update the software.
 */
void DeRestPluginPrivate::updateSoftwareTimerFired()
{
    DBG_Printf(DBG_INFO, "Update software to %s\n", qPrintable(gwUpdateVersion));
    int appRet = APP_RET_RESTART_APP;

    if (gwUpdateChannel == "stable")
    {
        appRet = APP_RET_UPDATE;
    }
    else if (gwUpdateChannel == "alpha")
    {
        appRet = APP_RET_UPDATE_ALPHA;
    }
    else if (gwUpdateChannel == "beta")
    {
        appRet = APP_RET_UPDATE_BETA;
    }
    else
    {
        DBG_Printf(DBG_ERROR, "can't trigger update for unknown updatechannel: %s\n", qPrintable(gwUpdateChannel));
        return;
    }

    qApp->exit(appRet);
}

/*! Locks the gateway.
 */
void DeRestPluginPrivate::lockGatewayTimerFired()
{
    if (gwLinkButton)
    {
        gwLinkButton = false;
        updateEtag(gwConfigEtag);
        DBG_Printf(DBG_INFO, "gateway locked\n");
    }
}

/*! Helper to update the config Etag then rfconnect state changes.
 */
void DeRestPluginPrivate::checkRfConnectState()
{
    if (apsCtrl)
    {
        // while touchlink is active always report connected: true
        if (isTouchlinkActive())
        {
            if (!gwRfConnected)
            {
                gwRfConnected = true;
                updateEtag(gwConfigEtag);
            }
        }
        else
        {
            bool connected = isInNetwork();

            if (connected != gwRfConnected)
            {
                gwRfConnected = connected;
                updateEtag(gwConfigEtag);
            }
        }

        // upgrade setting if needed
        if (!gwRfConnectedExpected && gwRfConnected)
        {
            gwRfConnectedExpected = true;
            queSaveDb(DB_CONFIG, DB_LONG_SAVE_DELAY);
        }
    }
}

/*! get current Timezone from gnu linux as IANA code.
 */
std::string DeRestPluginPrivate::getTimezone()
{

#ifdef ARCH_ARM
#ifdef Q_OS_LINUX
    char const* cmd = "cat /etc/timezone";
    FILE* pipe = popen(cmd, "r");
    if (!pipe) return "error";
    char buffer[128];
    std::string result = "";
    while(!feof(pipe)) {
        if(fgets(buffer, 128, pipe) != NULL)
            result += buffer;
    }
    pclose(pipe);
    return result;
#endif
#endif
    return "none";
}
