/*
 * Copyright (C) 2013 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QApplication>
#include <QString>
#include <QTcpSocket>
#include <QHttpRequestHeader>
#include <QVariantMap>
#include <QNetworkInterface>
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "json.h"


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
        // TODO deleteUser()
    }
    // POST /api/<apikey>/config/update
    else if ((req.path.size() == 4) && (req.hdr.method() == "POST") && (req.path[2] == "config") && (req.path[3] == "update"))
    {
        return updateSoftware(req, rsp);
    }
    // PUT /api/<apikey>/config/password
    else if ((req.path.size() == 4) && (req.hdr.method() == "PUT") && (req.path[2] == "config") && (req.path[3] == "password"))
    {
        return changePassword(req, rsp);
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
        apiAuths.push_back(auth);
        needSaveDatabase = true;
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
void DeRestPluginPrivate::configToMap(QVariantMap &map)
{
    bool ok;
    QVariantMap whitelist;
    QVariantMap swupdate;
    QDateTime datetime = QDateTime::currentDateTime();

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

        map["mac"] = eth.hardwareAddress();
    }

    if (!ok)
    {
        map["mac"] = "38:60:77:7c:53:18";
        map["ipaddress"] = "127.0.0.1";
        map["netmask"] = "255.0.0.0";
        DBG_Printf(DBG_ERROR, "No valid ethernet interface found\n");
    }

    map["name"] = gwName;
    map["uuid"] = gwUuid;
    map["port"] = (double)deCONZ::appArgumentNumeric("--http-port", 80);
    map["dhcp"] = true; // dummy
    map["gateway"] = "192.168.178.1"; // TODO
    map["proxyaddress"] = ""; // dummy
    map["proxyport"] = (double)0; // dummy
    map["utc"] = datetime.toString("yyyy-MM-ddThh:mm:ss"); // ISO 8601
    map["whitelist"] = whitelist;
    map["swversion"] = GW_SW_VERSION;
    map["announceurl"] = gwAnnounceUrl;
    map["announceinterval"] = (double)gwAnnounceInterval;
    map["rfconnected"] = isInNetwork();
    map["permitjoin"] = (double)gwPermitJoinDuration;
    map["otauactive"] = gwOtauActive;
    map["otaustate"] = (isOtauBusy() ? "busy" : (gwOtauActive ? "idle" : "off"));
    map["groupdelay"] = (double)gwGroupSendDelay;
    map["discovery"] = (gwAnnounceInterval > 0);

    swupdate["version"] = gwUpdateVersion;
    swupdate["updatestate"] = (double)0;
    swupdate["url"] = "";
    swupdate["text"] = "";
    swupdate["notify"] = false;
    map["swupdate"] = swupdate;

    map["linkbutton"] = gwLinkButton;
    map["portalservices"] = false;

    gwIpAddress = map["ipaddress"].toString(); // cache
    gwPort = map["port"].toDouble(); // cache
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

    QVariantMap lights;
    QVariantMap groups;
    QVariantMap config;
    QVariantMap schedules;

    // lights
    {
        std::vector<LightNode>::const_iterator i = this->nodes.begin();
        std::vector<LightNode>::const_iterator end = this->nodes.end();

        for (; i != end; ++i)
        {
            QVariantMap map;
            if (lightToMap(req, &(*i), map))
            {
                lights[i->id()] = map;
            }
        }
    }

    // groups
    {
        std::vector<Group>::const_iterator i = this->groups.begin();
        std::vector<Group>::const_iterator end = this->groups.end();

        for (; i != end; ++i)
        {
            // ignore deleted groups
            if (i->state() == Group::StateDeleted)
            {
                continue;
            }

            if (i->id() != "0")
            {
                QVariantMap map;
                if (groupToMap(&(*i), map))
                {
                    groups[i->id()] = map;
                }
            }
        }
    }

    configToMap(config);

    rsp.map["lights"] = lights;
    rsp.map["groups"] = groups;
    rsp.map["config"] = config;
    rsp.map["schedules"] = schedules;
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

    configToMap(rsp.map);
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
    deCONZ::ApsController *apsCtrl = deCONZ::ApsController::instance();

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
        needSaveDatabase = true;
    }

    if (map.contains("rfconnected")) // optional
    {
        if (map["rfconnected"].type() != QVariant::Bool)
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/config/rfconnected"), QString("invalid value, %1, for parameter, rfconnected").arg(map["rfconnected"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

        bool rfconnected = map["rfconnected"].toBool();

        if (gwRfConnected != rfconnected)
        {
            gwRfConnected = rfconnected;
            changed = true;
        }

        if (apsCtrl->setNetworkState(gwRfConnected ? deCONZ::InNetwork : deCONZ::NotInNetwork) == 0)
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
            needSaveDatabase = true;
            changed = true;
        }

        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState["/config/groupdelay"] = (double)milliseconds;
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

        if (gwOtauActive != otauActive)
        {
            gwOtauActive = otauActive;
            changed = true;
        }

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
            needSaveDatabase = true;
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

    if (changed)
    {
        updateEtag(gwConfigEtag);
    }

    rsp.etag = gwConfigEtag;

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
        saveDb();
        QTimer::singleShot(5000, this, SLOT(updateSoftwareTimerFired()));
    }
#endif // ARCH_ARM

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
        needSaveDatabase = true;

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

/*! Delayed trigger to update the software
 */
void DeRestPluginPrivate::updateSoftwareTimerFired()
{
    DBG_Printf(DBG_INFO, "Update software to %s\n", qPrintable(gwUpdateVersion));
    qApp->exit(APP_RET_UPDATE);
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
