/*
 * Copyright (c) 2013-2017 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QApplication>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
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
#include <time.h>
#include <QProcess>
#ifdef ARCH_ARM
  #include <env.h>
  #include <unistd.h>
  #include <sys/reboot.h>
  #include <sys/time.h>
#endif

/*! Constructor. */
ApiConfig::ApiConfig() :
    Resource(RConfig)
{
}

/*! Intit the configuration. */
void DeRestPluginPrivate::initConfig()
{
    QString dataPath = deCONZ::getStorageLocation(deCONZ::ApplicationsDataLocation);

    // default configuration
    gwRunFromShellScript = false;
    gwDeleteUnknownRules = (deCONZ::appArgumentNumeric("--delete-unknown-rules", 1) == 1) ? true : false;
    gwRfConnected = false; // will be detected later
    gwRfConnectedExpected = (deCONZ::appArgumentNumeric("--auto-connect", 1) == 1) ? true : false;
    gwPermitJoinDuration = 0;
    gwPermitJoinResend = 0;
    gwNetworkOpenDuration = 60;
    gwWifi = "not-configured";
    gwWifiType = "accesspoint";
    gwWifiName = "Not set";
    gwWifiChannel = "1";
    gwWifiIp = QLatin1String("192.168.8.1");
    gwWifiPw = "";
    gwRgbwDisplay = "1";
    gwTimezone = QString::fromStdString(getTimezone());
    gwTimeFormat = "12h";
    gwZigbeeChannel = 0;
    gwName = GW_DEFAULT_NAME;
    gwUpdateVersion = GW_SW_VERSION; // will be replaced by discovery handler
    gwSwUpdateState = swUpdateState.noUpdate;
    gwUpdateChannel = "stable";
    gwReportingEnabled = (deCONZ::appArgumentNumeric("--reporting", 1) == 1) ? true : false;
    gwFirmwareNeedUpdate = false;
    gwFirmwareVersion = "0x00000000"; // query later
    gwFirmwareVersionUpdate = "";
    gwBridgeId = "0000000000000000";
    gwConfig["websocketport"] = 443;

    // offical dresden elektronik sd-card image?
    {
        QFile f(dataPath + QLatin1String("/gw-version"));
        if (f.exists() && f.open(QFile::ReadOnly))
        {
            gwSdImageVersion = f.readAll().trimmed();
        }
    }

#ifdef Q_OS_LINUX
#ifdef ARCH_ARM
    {
        QFile f("/sys/block/mmcblk0/device/cid");
        if (f.exists() && f.open(QFile::ReadOnly))
        {
            QByteArray cid = f.readAll().left(32);
            // wipe serial number
            for (int i = 18; i < (18 + 8); i++)
            { cid[i] = 'f'; }
            DBG_Printf(DBG_INFO, "sd-card cid: %s\n", qPrintable(cid));
        }
    }
#endif
#endif

    config.addItem(DataTypeTime, RConfigLocalTime);

    {
        QHttpRequestHeader hdr;
        QStringList path;
        QString content;
        ApiRequest dummyReq(hdr, path, 0, content);
        dummyReq.version = ApiVersion_1_DDEL;
        configToMap(dummyReq, gwConfig);
    }

    gwProxyPort = 0;
    gwProxyAddress = "none";
}

/*! Init WiFi parameters if necessary. */
void DeRestPluginPrivate::initWiFi()
{
    // only configure for official image
    if (gwSdImageVersion.isEmpty())
    {
        return;
    }

    if (gwBridgeId.isEmpty())
    {
        QTimer::singleShot(5000, this, SLOT(initWiFi()));
        return;
    }

    if (gwWifi == QLatin1String("configured"))
    {
        return;
    }

    QByteArray sec0 = apsCtrl->getParameter(deCONZ::ParamSecurityMaterial0);
    if (sec0.isEmpty())
    {
        QTimer::singleShot(10000, this, SLOT(initWiFi()));
        return;
    }

    gwWifi = QLatin1String("configured");

    if (gwWifiName.isEmpty() || gwWifiName == QLatin1String("Not set"))
    {
        gwWifiName = QString("Phoscon-Gateway-%1").arg(gwBridgeId.right(4));
    }

    if (gwWifiPw.isEmpty() || gwWifiPw.length() < 8)
    {
        gwWifiPw = sec0.mid(16, 16).toUpper();
    }

    queSaveDb(DB_CONFIG, DB_SHORT_SAVE_DELAY);
}

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
    else if ((req.path.size() == 2) && (req.hdr.method() == "GET"))
    {
        // GET /api/config
        if (req.path[1] == "config")
        {
          return getBasicConfig(req, rsp);
        }
        // GET /api/challenge
        else if (req.path[1] == "challenge")
        {
          return getChallenge(req, rsp);
        }
        // GET /api/<apikey>
        else
        {
          return getFullState(req, rsp);
        }
    }
    // GET /api/<apikey>/config
    else if ((req.path.size() == 3) && (req.hdr.method() == "GET") && (req.path[2] == "config"))
    {
        return getConfig(req, rsp);
    }
    // GET /api/<apikey>/config/wifi
    else if ((req.path.size() == 4) && (req.hdr.method() == "GET") && (req.path[2] == "config") && (req.path[3] == "wifi"))
    {
        return getWifiState(req, rsp);
    }
    // PUT /api/<apikey>/config/wifi/restore
    else if ((req.path.size() == 5) && (req.hdr.method() == "PUT") && (req.path[2] == "config") && (req.path[3] == "wifi") && (req.path[4] == "restore"))
    {
        return restoreWifiConfig(req, rsp);
    }
    // PUT, PATCH /api/<apikey>/config
    else if ((req.path.size() == 3) && (req.hdr.method() == "PUT" || req.hdr.method() == "PATCH") && (req.path[2] == "config"))
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
    // POST /api/<apikey>/config/restart
    else if ((req.path.size() == 4) && (req.hdr.method() == "POST") && (req.path[2] == "config") && (req.path[3] == "restart"))
    {
        return restartGateway(req, rsp);
    }
    // POST /api/<apikey>/config/restartapp
    else if ((req.path.size() == 4) && (req.hdr.method() == "POST") && (req.path[2] == "config") && (req.path[3] == "restartapp"))
    {
        return restartApp(req, rsp);
    }
    // POST /api/<apikey>/config/shutdown
    else if ((req.path.size() == 4) && (req.hdr.method() == "POST") && (req.path[2] == "config") && (req.path[3] == "shutdown"))
    {
        return shutDownGateway(req, rsp);
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
    // POST /api/<apikey>/config/wifiscan
    else if ((req.path.size() == 4) && (req.hdr.method() == "POST") && (req.path[2] == "config") && (req.path[3] == "wifiscan"))
    {
        return scanWifiNetworks(req, rsp);
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
        if (!allowedToCreateApikey(req, rsp, map))
        {
            return REQ_READY_SEND;
        }
    }

    if (!ok || map.isEmpty())
    {
        rsp.httpStatus = HttpStatusBadRequest;
        rsp.list.append(errorToMap(ERR_INVALID_JSON, QLatin1String("/"), QLatin1String("body contains invalid JSON")));
        return REQ_READY_SEND;
    }

    if (!map.contains("devicetype")) // required
    {
        rsp.list.append(errorToMap(ERR_MISSING_PARAMETER, QLatin1String("/"), QLatin1String("missing parameters in body")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    auth.devicetype = map["devicetype"].toString();

    if (map.contains("username")) // optional (note username = apikey)
    {
        if ((map["username"].type() != QVariant::String) ||
            (map["username"].toString().length() < 10))
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QLatin1String("/"), QString("invalid value, %1, for parameter, username").arg(map["username"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

        auth.apikey = map["username"].toString();

        // check if this apikey is already known
        std::vector<ApiAuth>::const_iterator i = apiAuths.begin();
        std::vector<ApiAuth>::const_iterator end = apiAuths.end();

        for (; i != end; ++i)
        {
            if (auth.apikey == i->apikey && i->state == ApiAuth::StateNormal)
            {
                found = true;
                break;
            }
        }
    }
    else
    {
        // check for glitches from some devices registering too fast (Amazon Echo)
        std::vector<ApiAuth>::const_iterator i = apiAuths.begin();
        std::vector<ApiAuth>::const_iterator end = apiAuths.end();

        for (; i != end && !found ; ++i)
        {
            if (auth.devicetype == i->devicetype && i->state == ApiAuth::StateNormal)
            {
                if (i->createDate.secsTo(QDateTime::currentDateTimeUtc()) < 30)
                {
                    auth = *i;
                    found = true;
                    DBG_Printf(DBG_INFO, "reuse recently created auth username: %s, devicetype: %s\n", qPrintable(auth.apikey), qPrintable(auth.devicetype));
                }
            }
        }

        if (!found)
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
        auth.needSaveDatabase = true;
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
    bool ok = false;
    QVariantMap whitelist;
    QVariantMap swupdate;
    QVariantMap swupdate2;
    QVariantMap autoinstall;
    QVariantMap bridge;
    QVariantMap devicetypes;
    QVariantMap portalstate;
    QVariantMap internetservices;
    QVariantMap backup;
    QDateTime datetime = QDateTime::currentDateTimeUtc();
    QDateTime localtime = QDateTime::currentDateTime();

    {
        QList<QNetworkInterface> ifaces = QNetworkInterface::allInterfaces();
        QList<QNetworkInterface>::Iterator i = ifaces.begin();
        QList<QNetworkInterface>::Iterator end = ifaces.end();

        // optimistic approach chose the first available ethernet interface
        for (; !ok && i != end; ++i)
        {
            if (i->name() == QLatin1String("tun0"))
            {
                continue;
            }

            if ((i->flags() & QNetworkInterface::IsUp) &&
                (i->flags() & QNetworkInterface::IsRunning) &&
                !(i->flags() & QNetworkInterface::IsLoopBack))
            {
                //DBG_Printf(DBG_INFO, "%s (%s)\n", qPrintable(i->name()), qPrintable(i->humanReadableName()));

                QList<QNetworkAddressEntry> addresses = i->addressEntries();

                if (ok || addresses.isEmpty())
                {
                    continue;
                }

                QList<QNetworkAddressEntry>::Iterator a = addresses.begin();
                QList<QNetworkAddressEntry>::Iterator aend = addresses.end();

                for (; a != aend; ++a)
                {
                    if (a->ip().protocol() != QAbstractSocket::IPv4Protocol)
                    {
                        continue;
                    }

                    quint32 ipv4 = a->ip().toIPv4Address();
                    if ((ipv4 & 0xff000000UL) == 0x7f000000UL)
                    {
                        // 127.x.x.x
                        continue;
                    }

                    if ((ipv4 & 0x80000000UL) != 0x00000000UL && // class A 0xxx xxxx
                        (ipv4 & 0xc0000000UL) != 0x80000000UL && // class B 10xx xxxx
                        (ipv4 & 0xe0000000UL) != 0xc0000000UL)   // class C 110x xxxx
                    {
                        // unsupported network
                        continue;
                    }

                    map["ipaddress"] = a->ip().toString();
                    map["netmask"] = a->netmask().toString();
                    map["mac"] = i->hardwareAddress().toLower();
                    ok = true;
                    break;
                }
            }
        }
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
        map["permitjoinfull"] = (double)gwPermitJoinResend;
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

        switch (fwUpdateState)
        {
        case FW_DisconnectDevice:
        case FW_Update:
        case FW_UpdateWaitFinished:
        { map[QLatin1String("fwupdatestate")] = QLatin1String("running"); }
            break;

        default:
        { map[QLatin1String("fwupdatestate")] = QLatin1String("idle"); }
            break;
        }

        map["announceurl"] = gwAnnounceUrl;
        map["announceinterval"] = (double)gwAnnounceInterval;
        map["swversion"] = QLatin1String(GW_SW_VERSION);
        map["swcommit"] = QLatin1String(GIT_COMMMIT);
        swupdate["version"] = gwUpdateVersion;
        swupdate["updatestate"] = (double)0;
        swupdate["url"] = "";
        swupdate["text"] = "";
        swupdate["notify"] = false;
        map["swupdate"] = swupdate;
        map["port"] = (double)(apsCtrl ? apsCtrl->getParameter(deCONZ::ParamHttpPort) : 80);
        // since api version 1.2.1
        map["apiversion"] = QLatin1String(GW_SW_VERSION);
        map["system"] = "other";
#if defined(ARCH_ARMV6) || defined (ARCH_ARMV7)
#ifdef Q_OS_LINUX
        map["system"] = "linux-gw";
#endif
#endif
        map["wifi"] = gwWifi;
        map["wifitype"] = gwWifiType;
        map["wifiname"] = gwWifiName;
        map["wifichannel"] = gwWifiChannel;
        map["wifiip"] = gwWifiIp;
//        map["wifiappw"] = gwWifiPw;
        map["wifiappw"] = QLatin1String(""); // TODO add secured transfer via PKI
    }
    else
    {
        if (req.strict)
        {
            map["swversion"] = QLatin1String("01038802");
            map["apiversion"] = QLatin1String("1.20.0");
            map["bridgeid"] = QLatin1String("BSB002");
        }
        else
        {
            QStringList versions = QString(GW_SW_VERSION).split('.');
            QString swversion;
            swversion.sprintf("%d.%d.%d", versions[0].toInt(), versions[1].toInt(), versions[2].toInt());
            map["swversion"] = swversion;
            map["apiversion"] = QString(GW_API_VERSION);
            map["bridgeid"] = gwBridgeId;
        }
        devicetypes["bridge"] = false;
        devicetypes["lights"] = QVariantList();
        devicetypes["sensors"] = QVariantList();
        swupdate["devicetypes"] = devicetypes;
        swupdate["updatestate"] = (double)0;
        swupdate["checkforupdate"] = false;
        swupdate["url"] = "";
        swupdate["text"] = "";
        swupdate["notify"] = false;
        map["portalconnection"] = QLatin1String("disconnected");
        portalstate["signedon"] = false;
        portalstate["incoming"] = false;
        portalstate["outgoing"] = false;
        portalstate["communication"] = QLatin1String("disconnected");
        map["portalstate"] = portalstate;
        internetservices["remoteaccess"] = QLatin1String("disconnected");
        map["internetservices"] = internetservices;
        backup["status"] = QLatin1String("idle");
        backup["errorcode"] = 0;
        map["backup"] = backup;
        map["factorynew"] = false;
        map["replacesbridgeid"] = QVariant();
        map["datastoreversion"] = QLatin1String("60");
        map["swupdate"] = swupdate;
        map["starterkitid"] = QLatin1String("");
    }

    bridge["state"] = gwSwUpdateState;
    bridge["lastinstall"] = "";
    swupdate2["bridge"] = bridge;
    swupdate2["checkforupdate"] = false;
    swupdate2["state"] = gwSwUpdateState;
    swupdate2["install"] = false;
    autoinstall["updatetime"] = "";
    autoinstall["on"] = false;
    swupdate2["autoinstall"] = autoinstall;
    swupdate2["lastchange"] = "";
    swupdate2["lastinstall"] = "";
    map["swupdate2"] = swupdate2;

    map["name"] = gwName;
    map["uuid"] = gwUuid;
    if (apsCtrl)
    {
        map["zigbeechannel"] = apsCtrl->getParameter(deCONZ::ParamCurrentChannel);
        map["panid"] = apsCtrl->getParameter(deCONZ::ParamPANID);
        gwPort = apsCtrl->getParameter(deCONZ::ParamHttpPort); // cache
    }
    else
    {
        map["zigbeechannel"] = (double)gwZigbeeChannel;
        gwPort = deCONZ::appArgumentNumeric("--http-port", 80); // cache
    }

    if (!gwDeviceName.isEmpty())
    {
        map["devicename"] = gwDeviceName;
    }

    map["modelid"] = QLatin1String("deCONZ");
    map["dhcp"] = true; // dummy
    map["proxyaddress"] = gwProxyAddress;
    map["proxyport"] = (double)gwProxyPort;
    map["UTC"] = datetime.toString("yyyy-MM-ddTHH:mm:ss"); // ISO 8601
    map["localtime"] = localtime.toString("yyyy-MM-ddTHH:mm:ss"); // ISO 8601
    map["timezone"] = gwTimezone;
    map["networkopenduration"] = gwNetworkOpenDuration;
    map["timeformat"] = gwTimeFormat;
    map["whitelist"] = whitelist;
    map["linkbutton"] = gwLinkButton;
    map["portalservices"] = false;
    map["websocketport"] = (double)gwConfig["websocketport"].toUInt();
    map["websocketnotifyall"] = gwWebSocketNotifyAll;

    gwIpAddress = map["ipaddress"].toString(); // cache


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

/*! Puts all parameters in a map for later JSON serialization.
 */
void DeRestPluginPrivate::basicConfigToMap(QVariantMap &map)
{
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

    if (eth.isValid() && !eth.addressEntries().isEmpty())
    {
        map["mac"] = eth.hardwareAddress().toLower();
    }
    else
    {
        DBG_Printf(DBG_ERROR, "No valid ethernet interface found\n");
    }

    map["bridgeid"] = gwBridgeId;
    QStringList versions = QString(GW_SW_VERSION).split('.');
    QString swversion;
    swversion.sprintf("%d.%d.%d", versions[0].toInt(), versions[1].toInt(), versions[2].toInt());
    map["swversion"] = swversion;
    map["modelid"] = QLatin1String("deCONZ");
    map["factorynew"] = false;
    map["replacesbridgeid"] = QVariant();
    map["datastoreversion"] = QLatin1String("60");
    map["apiversion"] = QString(GW_API_VERSION);
    map["name"] = gwName;
    map["starterkitid"] = QLatin1String("");
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
    QVariantMap rulesMap;

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
            if (i->state == Schedule::StateDeleted)
            {
                continue;
            }
            schedulesMap[i->id] = i->jsonMap;
        }
    }

    // sensors
    {
        std::vector<Sensor>::const_iterator i = sensors.begin();
        std::vector<Sensor>::const_iterator end = sensors.end();

        for (; i != end; ++i)
        {
            if (i->deletedState() == Sensor::StateDeleted)
            {
                continue;
            }
            QVariantMap map;
            if (sensorToMap(&(*i), map, req.strict))
            {
                sensorsMap[i->id()] = map;
            }
        }
    }

    // rules
    {
        std::vector<Rule>::const_iterator i = rules.begin();
        std::vector<Rule>::const_iterator end = rules.end();

        for (; i != end; ++i)
        {
            if (i->state() == Rule::StateDeleted)
            {
                continue;
            }
            QVariantMap map;
            if (ruleToMap(&(*i), map))
            {
                rulesMap[i->id()] = map;
            }
        }
    }

    configToMap(req, configMap);

    rsp.map["lights"] = lightsMap;
    rsp.map["groups"] = groupsMap;
    rsp.map["config"] = configMap;
    rsp.map["schedules"] = schedulesMap;
    rsp.map["sensors"] = sensorsMap;
    rsp.map["rules"] = rulesMap;
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
        return getBasicConfig(req, rsp);
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

/*! GET /api/config
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::getBasicConfig(const ApiRequest &req, ApiResponse &rsp)
{
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
    basicConfigToMap(rsp.map);
    rsp.httpStatus = HttpStatusOk;
    rsp.etag = gwConfigEtag;
    return REQ_READY_SEND;
}

/*! GET /api/challenge
    Creates a new authentification challenge which should be used as HMAC-Sha256(challenge, install code).
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::getChallenge(const ApiRequest &req, ApiResponse &rsp)
{
    Q_UNUSED(req);
    QDateTime now = QDateTime::currentDateTime();

    if (!apsCtrl || (gwLastChallenge.isValid() && gwLastChallenge.secsTo(now) < 5))
    {
        rsp.httpStatus = HttpStatusServiceUnavailable;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/api/challenge"), QString("too many requests, try again later")));
        return REQ_READY_SEND;
    }

    qsrand(time(0));
    QByteArray challange;

    for (int i = 0; i < 64; i++)
    {
        challange.append(QString::number(qrand()));
    }

    gwLastChallenge = now;
    gwChallenge = QCryptographicHash::hash(challange, QCryptographicHash::Sha256).toHex();
    rsp.map["challenge"] = gwChallenge;
    rsp.httpStatus = HttpStatusOk;

    return REQ_READY_SEND;
}

/*! PUT, PATCH /api/<apikey>/config
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
    bool restartNetwork = false;
    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();
#ifdef ARCH_ARM
#ifdef Q_OS_LINUX
    std::string command = "";
#endif
#endif

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
        if (!ok || !(seconds >= 0))
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/config/permitjoin"), QString("invalid value, %1, for parameter, permitjoin").arg(map["permitjoin"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

        if (gwPermitJoinResend != seconds)
        {
            gwPermitJoinResend = seconds;
            changed = true;
        }

        if (seconds > 0)
        {
            startFindSensors();
        }

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
        QString wifi = map["wifi"].toString();

        if (map["wifi"].type() != QVariant::String ||
                ! ((wifi == "not-running") ||
                   (wifi == "running")))
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/config/wifi"), QString("invalid value, %1, for parameter, wifi").arg(map["wifi"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

        QString wifiType = "accesspoint";
        QString wifiName = "RaspBee-AP";
        QString wifiChannel = "1";
        QString wifiPassword = "raspbeegw";
        bool ret = true;

        if (map.contains("wifitype"))
        {
            wifiType = map["wifitype"].toString();

            if ((map["wifitype"].type() != QVariant::String) ||
                   ! ((wifiType == "accesspoint") ||
                      (wifiType == "ad-hoc") ||
                      (wifiType == "client")))
            {
                rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/config/wifitype"), QString("invalid value, %1, for parameter, wifitype").arg(map["wifitype"].toString())));
                rsp.httpStatus = HttpStatusBadRequest;
                return REQ_READY_SEND;
            }
        }

        if (map.contains("wifiname"))
        {
            wifiName = map["wifiname"].toString();

            if ((map["wifiname"].type() != QVariant::String) ||
                (map["wifiname"].toString().length() > 32))
            {
                rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/config/wifiname"), QString("invalid value, %1, for parameter, wifiname").arg(map["wifiname"].toString())));
                rsp.httpStatus = HttpStatusBadRequest;
                return REQ_READY_SEND;
            }
        }

        if (map.contains("wifichannel"))
        {
            wifiChannel = map["wifichannel"].toString();
            if (!((wifiChannel.toInt(&ok) >= 1) && (wifiChannel.toInt(&ok) <= 11)))
            {
                rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/config/wifichannel"), QString("invalid value, %1, for parameter, wifichannel").arg(map["wifichannel"].toString())));
                rsp.httpStatus = HttpStatusBadRequest;
                return REQ_READY_SEND;
            }
        }

        if (map.contains("wifipassword"))
        {
            wifiPassword = map["wifipassword"].toString();

            if (map["wifipassword"].type() != QVariant::String ||
                wifiPassword.length() < 8 || wifiPassword.length() > 63)
            {
                rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/config/wifipassword"), QString("invalid value, %1, for parameter, wifipassword").arg(map["wifipassword"].toString())));
                rsp.httpStatus = HttpStatusBadRequest;
                return REQ_READY_SEND;
            }
        }

        if ((gwWifi == "not-configured" && wifi == "running") ||
            (wifiType == "client" && map.contains("wifipassword") && map.contains("wifiname")))
        {
#ifdef ARCH_ARM
#ifdef Q_OS_LINUX
            command = "sudo bash /usr/bin/deCONZ-configure-wifi.sh " + wifiType.toStdString() + " \"" + wifiName.toStdString() + "\" \"" + wifiPassword.toStdString() + "\" " + wifiChannel.toStdString();
            system(command.c_str());
/*
            char const* cmd = "ifconfig wlan0 | grep 'inet addr:' | cut -d: -f2 | cut -d' ' -f1";
            FILE* pipe = popen(cmd, "r");
            if (!pipe)
            {
                rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/config/wifi"), QString("Error setting wifi")));
                rsp.httpStatus = HttpStatusServiceUnavailable;
                return REQ_READY_SEND;
            }
            char buffer[128];
            std::string result = "";
            while(!feof(pipe)) {
                if(fgets(buffer, 128, pipe) != NULL)
                    result += buffer;
            }
            pclose(pipe);

            QString ip = QString::fromStdString(result);

            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState["ip"] = ip;
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);
*/
#endif
#endif
        }
        else if ((gwWifi == "not-running" && wifi == "running") ||
                 (gwWifi == "running" && wifi == "not-running"))
        {
#ifdef ARCH_ARM
#ifdef Q_OS_LINUX

            char const* cmd = "";

            if (gwWifiType == "client")
            {
                if (wifi == "running")
                {
                    cmd = "sudo bash /usr/bin/deCONZ-startstop-wifi.sh client start";
                }
                else
                {
                    cmd = "sudo bash /usr/bin/deCONZ-startstop-wifi.sh client stop";
                }
            }
            else
            {
                if (wifi == "running")
                {
                    cmd = "sudo bash /usr/bin/deCONZ-startstop-wifi.sh accesspoint start";
                }
                else
                {
                    cmd = "sudo bash /usr/bin/deCONZ-startstop-wifi.sh accesspoint stop";
                }
            }

            FILE* pipe = popen(cmd, "r");
            if (!pipe)
            {
                rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/config/wifi"), QString("Error setting wifi")));
                rsp.httpStatus = HttpStatusServiceUnavailable;
                return REQ_READY_SEND;
            }
            char buffer[128];
            std::string result = "";
            while(!feof(pipe)) {
                if(fgets(buffer, 128, pipe) != NULL)
                    result += buffer;
            }
            pclose(pipe);

            QString ip = QString::fromStdString(result);

            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState["ip"] = ip;
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);
#endif
#endif
        }
        else if ((gwWifi == "running" && wifi == "running") || (gwWifi == "not-running" && wifi == "not-running"))
        {
            ret = false;
        }

        if (gwWifi != wifi)
        {
            gwWifi = wifi;
            queSaveDb(DB_CONFIG,DB_SHORT_SAVE_DELAY);
            changed = true;
        }

        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState["/config/wifi"] = wifi;
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
        if (ret == true)
        {
            // skip return here because user wants to set wifitype, ssid, pw or channel
            return REQ_READY_SEND;
        }
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
            QString wifiName = "RaspBee-AP";
            QString wifiChannel = "1";
            QString wifiPassword = "raspbeegw";

            if (map.contains("wifiname"))
            {
                wifiName = map["wifiname"].toString();

                if ((map["wifiname"].type() != QVariant::String) ||
                    (map["wifiname"].toString().length() > 32))
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/config/wifiname"), QString("invalid value, %1, for parameter, wifiname").arg(map["wifiname"].toString())));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }
            }

            if (map.contains("wifichannel"))
            {
                wifiChannel = map["wifichannel"].toString();
                if (!((wifiChannel.toInt(&ok) >= 1) && (wifiChannel.toInt(&ok) <= 11)))
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/config/wifichannel"), QString("invalid value, %1, for parameter, wifichannel").arg(map["wifichannel"].toString())));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }
            }

            if (map.contains("wifipassword"))
            {
                wifiPassword = map["wifipassword"].toString();

                if (map["wifipassword"].type() != QVariant::String ||
                    wifiPassword.length() < 8 || wifiPassword.length() > 63)
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/config/wifipassword"), QString("invalid value, %1, for parameter, wifipassword").arg(map["wifipassword"].toString())));
                    rsp.httpStatus = HttpStatusBadRequest;
                    return REQ_READY_SEND;
                }
            }

#ifdef ARCH_ARM
#ifdef Q_OS_LINUX
            command = "sudo bash /usr/bin/deCONZ-configure-wifi.sh " + wifiType.toStdString() + " \"" + wifiName.toStdString() + "\" \"" + wifiPassword.toStdString() + "\" " + wifiChannel.toStdString();
            system(command.c_str());
/*
            char const* cmd = "ifconfig wlan0 | grep 'inet addr:' | cut -d: -f2 | cut -d' ' -f1";
            FILE* pipe = popen(cmd, "r");
            if (!pipe)
            {
                rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/config/wifi"), QString("Error setting wifi")));
                rsp.httpStatus = HttpStatusServiceUnavailable;
                return REQ_READY_SEND;
            }
            char buffer[128];
            std::string result = "";
            while(!feof(pipe)) {
                if(fgets(buffer, 128, pipe) != NULL)
                    result += buffer;
            }
            pclose(pipe);

            QString ip = QString::fromStdString(result);

            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState["ip"] = ip;
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);
*/
#endif
#endif
            gwWifiType = wifiType;
            queSaveDb(DB_CONFIG,DB_SHORT_SAVE_DELAY);
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
#ifdef ARCH_ARM
#ifdef Q_OS_LINUX
            if (gwWifiType != "client")
            {
                command = "sudo sed -i 's/^ssid=.*/ssid=" + wifiName.toStdString() + "/g' /etc/hostapd/hostapd.conf";
                system(command.c_str());
            }
#endif
#endif
            if (gwWifi == "running")
            {
                if (gwWifiType != "client")
                {
                    restartNetwork = true;
                }

            }
            gwWifiName = wifiName;
            queSaveDb(DB_CONFIG,DB_SHORT_SAVE_DELAY);
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
        QString wifiChannel = map["wifichannel"].toString();
        if (!((wifiChannel.toInt(&ok) >= 1) && (wifiChannel.toInt(&ok) <= 11)))
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/config/wifichannel"), QString("invalid value, %1, for parameter, wifichannel").arg(map["wifichannel"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

        if (gwWifiChannel != wifiChannel)
        {
#ifdef ARCH_ARM
#ifdef Q_OS_LINUX
            if (gwWifiType != "client")
            {
                command = "sudo sed -i 's/^channel=.*/channel=" + wifiChannel.toStdString() + "/g' /etc/hostapd/hostapd.conf";
                system(command.c_str());
            }
#endif
#endif
            if (gwWifi == "running")
            {
                if (gwWifiType != "client")
                {
                    restartNetwork = true;
                }
            }
            gwWifiChannel = wifiChannel;
            queSaveDb(DB_CONFIG,DB_SHORT_SAVE_DELAY);
            changed = true;
        }

        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState["/config/wifichannel"] = wifiChannel;
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
    }

    if (map.contains("wifipassword")) // optional
    {
        QString wifiPassword = map["wifipassword"].toString();

        if (map["wifipassword"].type() != QVariant::String ||
            wifiPassword.length() < 8 || wifiPassword.length() > 63)
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/config/wifipassword"), QString("invalid value, %1, for parameter, wifipassword").arg(map["wifipassword"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

#ifdef ARCH_ARM
#ifdef Q_OS_LINUX
        if (gwWifiType != "client")
        {
            command = "sudo sed -i 's/wpa_passphrase=.*/wpa_passphrase=" + wifiPassword.toStdString() + "/g' /etc/hostapd/hostapd.conf";
            system(command.c_str());
        }
#endif
#endif
        if (gwWifi == "running")
        {
            if (gwWifiType != "client")
            {
                restartNetwork = true;
            }

        }
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
            int rc = 0;

            rc = setenv("TZ", ":" + timezone.toStdString(), 1);
            tzset();

            if (rc != 0)
            {
                rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/config/timezone"), QString("Error setting timezone")));
                rsp.httpStatus = HttpStatusServiceUnavailable;
                return REQ_READY_SEND;
            }
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
        int ret = 0;
        std::string date = map["utc"].toString().toStdString();

        time_t mytime = time(0);
        struct tm* tm_ptr = localtime(&mytime);

        if (tm_ptr)
        {
            tm_ptr->tm_year = atoi(date.substr(0,4).c_str());
            tm_ptr->tm_mon  = atoi(date.substr(5,2).c_str()) - 1;
            tm_ptr->tm_mday = atoi(date.substr(8,2).c_str());
            tm_ptr->tm_hour  = atoi(date.substr(11,2).c_str());
            tm_ptr->tm_min  = atoi(date.substr(14,2).c_str());
            tm_ptr->tm_sec  = atoi(date.substr(17,2).c_str());

            DBG_Printf(DBG_INFO, "%d-%d-%dT%d:%d:%d\n", tm_ptr->tm_year,tm_ptr->tm_mon,tm_ptr->tm_mday,tm_ptr->tm_hour,tm_ptr->tm_min,tm_ptr->tm_sec);
            const struct timeval tv = {mktime(tm_ptr), 0};
            ret = settimeofday(&tv, 0);
        }

        if (ret != 0)
        {
            rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/config/utc"), QString("Error setting date and time")));
            rsp.httpStatus = HttpStatusServiceUnavailable;
            return REQ_READY_SEND;
        }
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

    if (map.contains("websocketnotifyall")) // optional
    {
        bool notifyAll = map["websocketnotifyall"].toBool();

        if (gwWebSocketNotifyAll != notifyAll)
        {
            gwWebSocketNotifyAll = notifyAll;
            changed = true;
            queSaveDb(DB_CONFIG, DB_SHORT_SAVE_DELAY);
        }
        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState["/config/websocketnotifyall"] = notifyAll;
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
    }

    if (changed)
    {
        updateEtag(gwConfigEtag);
    }

    if (restartNetwork)
    {
#ifdef ARCH_ARM
#ifdef Q_OS_LINUX

        char const* cmd = "";
        cmd = "sudo bash /usr/bin/deCONZ-startstop-wifi.sh accesspoint start";
        FILE* pipe = popen(cmd, "r");
        if (!pipe)
        {
            rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/config/wifi"), QString("Error setting wifi")));
            rsp.httpStatus = HttpStatusServiceUnavailable;
            return REQ_READY_SEND;
        }
        char buffer[128];
        std::string result = "";
        while(!feof(pipe)) {
            if(fgets(buffer, 128, pipe) != NULL)
                result += buffer;
        }
        pclose(pipe);
#endif
#endif
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
            i->needSaveDatabase = true;
            i->state = ApiAuth::StateDeleted;
            queSaveDb(DB_AUTH, DB_LONG_SAVE_DELAY);

            QVariantMap rspItem;
            rspItem["success"] = QString("/config/whitelist/%1 deleted.").arg(username2);
            rsp.list.append(rspItem);
            rsp.httpStatus = HttpStatusOk;

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
    if (gwSwUpdateState != swUpdateState.transferring)
    {
        gwSwUpdateState = swUpdateState.transferring;
    }
    queSaveDb(DB_CONFIG, DB_SHORT_SAVE_DELAY);
    rspItemState["/config/update"] = gwUpdateVersion;
#ifdef ARCH_ARM
    rspItemState["/config/swupdate2/state"] = gwSwUpdateState;
#endif
    rspItem["success"] = rspItemState;
    rsp.list.append(rspItem);

    return REQ_READY_SEND;
}

/*! POST /api/<apikey>/config/restart
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::restartGateway(const ApiRequest &req, ApiResponse &rsp)
{
    if(!checkApikeyAuthentification(req, rsp))
    {
        return REQ_READY_SEND;
    }

    rsp.httpStatus = HttpStatusOk;
    QVariantMap rspItem;
    QVariantMap rspItemState;
    rspItemState["/config/restart"] = true;
    rspItem["success"] = rspItemState;
    rsp.list.append(rspItem);

#ifdef ARCH_ARM
        openDb();
        saveDb();
        closeDb();

        QTimer *restartTimer = new QTimer(this);
        restartTimer->setSingleShot(true);
        connect(restartTimer, SIGNAL(timeout()),
                this, SLOT(restartGatewayTimerFired()));
        restartTimer->start(500);
#endif // ARCH_ARM

    return REQ_READY_SEND;
}

/*! POST /api/<apikey>/config/restartapp
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::restartApp(const ApiRequest &req, ApiResponse &rsp)
{
    if(!checkApikeyAuthentification(req, rsp))
    {
        return REQ_READY_SEND;
    }

    rsp.httpStatus = HttpStatusOk;
    QVariantMap rspItem;
    QVariantMap rspItemState;
    rspItemState["/config/restartapp"] = true;
    rspItem["success"] = rspItemState;
    rsp.list.append(rspItem);

    openDb();
    saveDb();
    closeDb();

    QTimer *restartTimer = new QTimer(this);
    restartTimer->setSingleShot(true);
    connect(restartTimer, SIGNAL(timeout()),
            this, SLOT(simpleRestartAppTimerFired()));
    restartTimer->start(500);

    return REQ_READY_SEND;
}

/*! POST /api/<apikey>/config/shutdown
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::shutDownGateway(const ApiRequest &req, ApiResponse &rsp)
{
    if(!checkApikeyAuthentification(req, rsp))
    {
        return REQ_READY_SEND;
    }

    rsp.httpStatus = HttpStatusOk;
    QVariantMap rspItem;
    QVariantMap rspItemState;
    rspItemState["/config/shutdown"] = true;
    rspItem["success"] = rspItemState;
    rsp.list.append(rspItem);

#ifdef ARCH_ARM
        openDb();
        saveDb();
        closeDb();

        QTimer *shutdownTimer = new QTimer(this);
        shutdownTimer->setSingleShot(true);
        connect(shutdownTimer, SIGNAL(timeout()),
                this, SLOT(shutDownGatewayTimerFired()));
        shutdownTimer->start(500);
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

/*! GET /api/config/wifi
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::getWifiState(const ApiRequest &req, ApiResponse &rsp)
{
    Q_UNUSED(req);

    checkWifiState();

    rsp.map["wifi"] = gwWifi;
    rsp.map["wifitype"] = gwWifiType;
    rsp.map["wifiname"] = gwWifiName;
    rsp.map["wifichannel"] = gwWifiChannel;
    rsp.map["wifiip"] = gwWifiIp;
    // rsp.map["wifiappw"] = gwWifiPw;
    rsp.map["wifiappw"] = QLatin1String("");

    rsp.httpStatus = HttpStatusOk;

    return REQ_READY_SEND;
}

/*! PUT /api/config/wifi/restore
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::restoreWifiConfig(const ApiRequest &req, ApiResponse &rsp)
{
    Q_UNUSED(req);

#if 0
#ifdef ARCH_ARM
#ifdef Q_OS_LINUX
    std::string command = "sudo bash /usr/bin/deCONZ-startstop-wifi.sh accesspoint restore" ;
    system(command.c_str());
#endif
#endif
#endif

    rsp.httpStatus = HttpStatusOk;
    QVariantMap rspItem;
    QVariantMap rspItemState;
    rspItemState["/config/wifi/restore"] = "original configuration restored";
    rspItem["success"] = rspItemState;
    rsp.list.append(rspItem);
    return REQ_READY_SEND;
}

/*! check wifi state on raspberry pi.
 */
void DeRestPluginPrivate::checkWifiState()
{
#if 0
#ifdef ARCH_ARM
#ifdef Q_OS_LINUX
    char const* cmd = "sudo bash /usr/bin/deCONZ-check-wifi.sh";
    FILE* pipe = popen(cmd, "r");
    if (!pipe) return;
    char buffer[128];
    std::string result = "";
    while(!feof(pipe)) {
        if(fgets(buffer, 128, pipe) != NULL)
            result += buffer;
    }
    pclose(pipe);

    if (QString::fromStdString(result).indexOf("not_installed") != -1)
    {
        if (gwWifi != "not-installed")
        {
            // changed
            updateEtag(gwConfigEtag);
            gwWifi = "not-installed";
        }
        return;
    }
    if (QString::fromStdString(result).indexOf("no_file") != -1 || QString::fromStdString(result).indexOf("not_configured") != -1)
    {
        if (gwWifi != "not-configured")
        {
            // changed
            updateEtag(gwConfigEtag);
            gwWifi = "not-configured";
        }
        return;
    }
    if (QString::fromStdString(result).indexOf("wlan0_down") != -1)
    {
        if (QString::fromStdString(result).indexOf("mode_") != -1)
        {
            int begin = QString::fromStdString(result).indexOf("mode_") + 5;
            int end = QString::fromStdString(result).indexOf("/mode");
            gwWifiType = QString::fromStdString(result.substr(begin, end-begin));
        }

        if (QString::fromStdString(result).indexOf("ssid_") != -1)
        {
            int begin = QString::fromStdString(result).indexOf("ssid_") + 5;
            int end = QString::fromStdString(result).indexOf("/ssid");
            gwWifiName = QString::fromStdString(result.substr(begin, end-begin));
        }

        if (QString::fromStdString(result).indexOf("channel_") != -1)
        {
            int begin = QString::fromStdString(result).indexOf("channel_") + 8;
            int end = QString::fromStdString(result).indexOf("/channel");
            QString channel = gwWifiChannel = QString::fromStdString(result.substr(begin, end-begin));
            gwWifiChannel = channel;
        }
        if (QString::fromStdString(result).indexOf("pw_") != -1)
        {
            int begin = QString::fromStdString(result).indexOf("pw_") + 3;
            int end = QString::fromStdString(result).indexOf("/pw");
            QString pw = QString::fromStdString(result.substr(begin, end-begin));
            gwWifiPw = pw;
        }
        if (QString::fromStdString(result).indexOf("ip_") != -1)
        {
            int begin = QString::fromStdString(result).indexOf("ip_") + 3;
            int end = QString::fromStdString(result).indexOf("/ip");
            QString ip = QString::fromStdString(result.substr(begin, end-begin));
            gwWifiIp = ip;
        }
        if (gwWifi != "not-running")
        {
            // changed
            updateEtag(gwConfigEtag);
            gwWifi = "not-running";
            queSaveDb(DB_CONFIG,DB_SHORT_SAVE_DELAY);
        }
        return;
    }
    if (QString::fromStdString(result).indexOf("wifi_running") != -1)
    {
        if (QString::fromStdString(result).indexOf("mode_") != -1)
        {
            int begin = QString::fromStdString(result).indexOf("mode_") + 5;
            int end = QString::fromStdString(result).indexOf("/mode");
            gwWifiType = QString::fromStdString(result.substr(begin, end-begin));
        }

        if (QString::fromStdString(result).indexOf("ssid_") != -1)
        {
            int begin = QString::fromStdString(result).indexOf("ssid_") + 5;
            int end = QString::fromStdString(result).indexOf("/ssid");
            gwWifiName = QString::fromStdString(result.substr(begin, end-begin));
        }

        if (QString::fromStdString(result).indexOf("channel_") != -1)
        {
            int begin = QString::fromStdString(result).indexOf("channel_") + 8;
            int end = QString::fromStdString(result).indexOf("/channel");
            QString channel = QString::fromStdString(result.substr(begin, end-begin));
            gwWifiChannel = channel;
        }
        if (QString::fromStdString(result).indexOf("pw_") != -1)
        {
            int begin = QString::fromStdString(result).indexOf("pw_") + 3;
            int end = QString::fromStdString(result).indexOf("/pw");
            QString pw = QString::fromStdString(result.substr(begin, end-begin));
            gwWifiPw = pw;
        }
        if (QString::fromStdString(result).indexOf("ip_") != -1)
        {
            int begin = QString::fromStdString(result).indexOf("ip_") + 3;
            int end = QString::fromStdString(result).indexOf("/ip");
            QString ip = QString::fromStdString(result.substr(begin, end-begin));
            gwWifiIp = ip;
        }
        if (gwWifi != "running")
        {
            // changed
            updateEtag(gwConfigEtag);
            gwWifi = "running";
            queSaveDb(DB_CONFIG,DB_SHORT_SAVE_DELAY);
        }

        return;
    }
#endif
#endif
#endif
    return;
}

/*! POST /api/<apikey>/config/wifiscan
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::scanWifiNetworks(const ApiRequest &req, ApiResponse &rsp)
{
    Q_UNUSED(req);
    Q_UNUSED(rsp);

    QVariantMap cells;
#if 0
    QVariantMap cell;

#ifdef ARCH_ARM
#ifdef Q_OS_LINUX
    char const* cmd1 = "ifconfig";
    FILE* pipe1 = popen(cmd1, "r");
    if (!pipe1) return -1;
    char buffer1[128];
    std::string ifconfig = "";
    while(!feof(pipe1)) {
        if(fgets(buffer1, 128, pipe1) != NULL)
            ifconfig += buffer1;
    }
    pclose(pipe1);

    if (QString::fromStdString(ifconfig).indexOf("wlan0") == -1)
    {
        //wlan0 down, activate it
        std::string command = "sudo ifup wlan0" ;
        system(command.c_str());
    }

    char const* cmd = "sudo iwlist wlan0 scan";
    FILE* pipe = popen(cmd, "r");
    if (!pipe) return -1;
    char buffer[128];
    std::string result = "";
    while(!feof(pipe)) {
        if(fgets(buffer, 128, pipe) != NULL)
            result += buffer;
    }
    pclose(pipe);

    QStringList wifiCells = QString::fromStdString(result).split("Cell");

    QString channel;
    QString security;
    QString cipher;
    QString key;
    QString name;
    QString signalStrength;

    QList<QString>::const_iterator i = wifiCells.begin();
    QList<QString>::const_iterator end = wifiCells.end();
    int n = 1;
    for (; i != end; ++i)
    {
        if (QString(*i).indexOf("ESSID:") != -1 && QString(*i).indexOf("Mode:Ad-Hoc") == -1)
        {
            channel = "";
            security = "";
            cipher = "";
            key = "";
            name = "";
            signalStrength = "";

            int begin = QString(*i).indexOf("ESSID:\"") + 7;
            int end = QString(*i).indexOf("\"\n");
            name = QString(*i).mid(begin, end-begin);
            cell["name"] = name;

            if (QString(*i).indexOf("Channel:") != -1)
            {
                int begin = QString(*i).indexOf("Channel:") + 8;
                channel = QString(*i).mid(begin, 2);
                channel = channel.simplified();
                cell["channel"] = channel;
            }

            if (QString(*i).indexOf("Signal level=") != -1)
            {
                int begin = QString(*i).indexOf("Signal level=") + 13;
                int end = QString(*i).indexOf(" dBm");
                signalStrength = QString(*i).mid(begin, end-begin);
                cell["signalstrength"] = signalStrength;
            }
            // security
            if (QString(*i).indexOf("WPA") != -1)
            {
                security = "WPA";
            }
            if (QString(*i).indexOf("WPA2") != -1)
            {
                security = "WPA2";
            }
            if (QString(*i).indexOf("WEP") != -1)
            {
                security = "WEP";
            }
            //group cipher TODO
            if (QString(*i).indexOf("CCMP") != -1)
            {
                cipher = "(CCMP)";
            }
            else if (QString(*i).indexOf("AES") != -1)
            {
                cipher = "(AES)";
            }
            else if (QString(*i).indexOf("TKIP") != -1)
            {
                cipher = "(TKIP)";
            }
            // authentication suites
            if (QString(*i).indexOf("PSK") != -1)
            {
                key = "-PSK";
            }
            cell["security"] = QString("%1%2").arg(security).arg(key);

            cells[QString::number(n)] = cell;
            n++;
        }
    }
#endif
#endif
#endif
    rsp.map["cells"] = cells;
    rsp.httpStatus = HttpStatusOk;
    return REQ_READY_SEND;
}

/*! Check if permitJoin is > 60 seconds then resend permitjoin with 60 seconds
 */
void DeRestPluginPrivate::resendPermitJoinTimerFired()
{
    resendPermitJoinTimer->stop();
    if (gwPermitJoinDuration <= 1)
    {
        if (gwPermitJoinResend > 0)
        {

            if (gwPermitJoinResend >= 60)
            {
                setPermitJoinDuration(60);
            }
            else
            {
                setPermitJoinDuration(gwPermitJoinResend);
            }
            gwPermitJoinResend -= 60;
            updateEtag(gwConfigEtag);
            if (gwPermitJoinResend <= 0)
            {
                gwPermitJoinResend = 0;
                return;
            }

        }
        else if (gwPermitJoinResend == 0)
        {
            setPermitJoinDuration(0);
            return;
        }
    }
    else if (gwPermitJoinResend == 0)
    {
        setPermitJoinDuration(0);
        return;
    }
    resendPermitJoinTimer->start(1000);
}
