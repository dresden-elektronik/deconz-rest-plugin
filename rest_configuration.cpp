/*
 * Copyright (c) 2013-2018 dresden elektronik ingenieurtechnik gmbh.
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
#include <QProcessEnvironment>
#include "daylight.h"
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "json.h"
#include <stdlib.h>
#include <time.h>
#include <QProcess>
#include "gateway.h"
#ifdef Q_OS_LINUX
  #include <unistd.h>
#ifdef ARCH_ARM
  #include <sys/reboot.h>
  #include <sys/time.h>
#endif
#endif // Q_OS_LINUX

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
    gwWifiClientName = "Not set";
    gwWifiChannel = "1";
    gwWifiIp = QLatin1String("192.168.8.1");
    gwWifiPw = "";
    gwWifiClientPw = "";
    gwRgbwDisplay = "1";
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
    fwUpdateState = FW_Idle;

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    if (env.contains(QLatin1String("INVOCATION_ID")))
    {
        // deCONZ is startet from a systemd unit
        //   -- deconz.service or deconz-gui.service
        if (env.contains(QLatin1String("DISPLAY")))
        {
            gwRunMode = QLatin1String("systemd/gui");
        }
        else
        {
            gwRunMode = QLatin1String("systemd/headless");
        }
    }
    else
    {
        gwRunMode = QLatin1String("normal");

#ifdef Q_OS_LINUX
        // check if we run from shell script
        QFile pproc(QString("/proc/%1/cmdline").arg(getppid()));

        if (pproc.exists() && pproc.open(QIODevice::ReadOnly))
        {
            QByteArray name = pproc.readAll();
            if (name.endsWith(".sh"))
            {
                DBG_Printf(DBG_INFO, "runs in shell script %s\n", qPrintable(name));
                gwRunFromShellScript = true;
                gwRunMode = QLatin1String("shellscript");
            }
            else
            {
                gwRunFromShellScript = false;
                DBG_Printf(DBG_INFO, "parent process %s\n", qPrintable(name));
            }
        }
#else
        gwRunFromShellScript = false;
#endif
    }

    DBG_Printf(DBG_INFO, "gw run mode: %s\n", qPrintable(gwRunMode));

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

    timeManagerState = TM_Init;
    ntpqProcess = 0;
    QTimer::singleShot(2000, this, SLOT(timeManagerTimerFired()));

    pollSwUpdateStateTimer = new QTimer(this);
    pollSwUpdateStateTimer->setSingleShot(false);
    connect(pollSwUpdateStateTimer, SIGNAL(timeout()),
            this, SLOT(pollSwUpdateStateTimerFired()));
}

/*! Init timezone. */
void DeRestPluginPrivate::initTimezone()
{
#ifdef Q_OS_LINUX
#ifdef ARCH_ARM
    if (gwTimezone.isEmpty())
    {
        // set timezone in system and save it in db
        gwTimezone = QLatin1String("Etc/GMT");

        if (getenv("TZ") == NULL)
        {
            setenv("TZ", qPrintable(gwTimezone), 1);
        }
        else
        {
            gwTimezone = getenv("TZ");
        }
        queSaveDb(DB_CONFIG, DB_SHORT_SAVE_DELAY);
    }
    else
    {
        // set system timezone from db
        if (getenv("TZ") != gwTimezone)
        {
            setenv("TZ", qPrintable(gwTimezone), 1);
        }
    }
    tzset();
#endif
#endif

    if (daylightSensorId.isEmpty())
    {
        Sensor dl;
        ResourceItem *item;
        openDb();
        daylightSensorId = QString::number(getFreeSensorId());
        closeDb();
        dl.setId(daylightSensorId);
        dl.setType(QLatin1String("Daylight"));
        dl.setName(QLatin1String("Daylight"));
        item = dl.addItem(DataTypeBool, RConfigConfigured);
        item->setValue(false);
        item = dl.addItem(DataTypeInt8, RConfigSunriseOffset);
        item->setValue(30);
        item = dl.addItem(DataTypeInt8, RConfigSunsetOffset);
        item->setValue(-30);
        item = dl.addItem(DataTypeBool, RStateDaylight);
        item->setValue(QVariant());
        item = dl.addItem(DataTypeInt32, RStateStatus);
        item->setValue(QVariant());

        dl.removeItem(RConfigReachable);

        dl.setModelId(QLatin1String("PHDL00"));
        dl.setManufacturer(QLatin1String("Philips"));
        dl.setSwVersion(QLatin1String("1.0"));
        dl.item(RConfigOn)->setValue(true);
        dl.setNeedSaveDatabase(true);
        queSaveDb(DB_SENSORS, DB_SHORT_SAVE_DELAY);
        sensors.push_back(dl);
    }

    QTimer *daylighTimer = new QTimer(this);
    connect(daylighTimer, SIGNAL(timeout()), this, SLOT(daylightTimerFired()));
    daylighTimer->setSingleShot(false);
    daylighTimer->start(10000);

    daylightTimerFired();
}

/*! Init WiFi parameters if necessary. */
void DeRestPluginPrivate::initWiFi()
{
#if !defined(ARCH_ARMV6) && !defined (ARCH_ARMV7)
    gwWifi = QLatin1String("not-available");
    return;
#endif

    // only configure for official image
    if (gwSdImageVersion.isEmpty())
    {
        return;
    }

    if (gwBridgeId.isEmpty() || gwBridgeId.endsWith(QLatin1String("0000")))
    {
        QTimer::singleShot(5000, this, SLOT(initWiFi()));
        return;
    }

    pollDatabaseWifiTimer = new QTimer(this);
    pollDatabaseWifiTimer->setSingleShot(false);
    connect(pollDatabaseWifiTimer, SIGNAL(timeout()),
            this, SLOT(pollDatabaseWifiTimerFired()));
    pollDatabaseWifiTimer->start(10000);

    if (gwWifiName == QLatin1String("Phoscon-Gateway-0000"))
    {
        // proceed to correct these
        gwWifiName.clear();
    }
    else if (gwWifi == QLatin1String("configured"))
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
    // PUT /api/<apikey>/config/wifi
    else if ((req.path.size() == 4) && (req.hdr.method() == "PUT") && (req.path[2] == "config") && (req.path[3] == "wifi"))
    {
        return configureWifi(req, rsp);
    }
    // PUT /api/<apikey>/config/wifi/restore
    else if ((req.path.size() == 5) && (req.hdr.method() == "PUT") && (req.path[2] == "config") && (req.path[3] == "wifi") && (req.path[4] == "restore"))
    {
        return restoreWifiConfig(req, rsp);
    }
    // PUT /api/<apikey>/config/wifi/scanresult
    else if ((req.path.size() == 5) && (req.hdr.method() == "PUT") && (req.path[2] == "config") && (req.path[3] == "wifi") && (req.path[4] == "scanresult"))
    {
        return putWifiScanResult(req, rsp);
    }
    // PUT /api/<apikey>/config/wifi/updated
    else if ((req.path.size() == 5) && (req.hdr.method() == "PUT") && (req.path[2] == "config") && (req.path[3] == "wifi") && (req.path[4] == "updated"))
    {
        return putWifiUpdated(req, rsp);
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

    auth.setDeviceType(map["devicetype"].toString());

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
        map["permitjoin"] = (double)gwPermitJoinDuration;
        map["permitjoinfull"] = (double)gwPermitJoinResend;
        map["otauactive"] = isOtauActive();
        map["otaustate"] = (isOtauBusy() ? "busy" : (isOtauActive() ? "idle" : "off"));
        map["groupdelay"] = (double)gwGroupSendDelay;
        map["discovery"] = (gwAnnounceInterval > 0);
        map["updatechannel"] = gwUpdateChannel;
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
        map["wifi"] = gwWifi;
#else
        map["wifi"] = QLatin1String("not-available");
#endif
        map["wifiavailable"] = gwWifiAvailable;
        map["wifitype"] = gwWifiType;
        map["wifiname"] = gwWifiName;
        map["wificlientname"] = gwWifiClientName;
        map["wifichannel"] = gwWifiChannel;
        map["wifiip"] = gwWifiIp;
//        map["wifiappw"] = gwWifiPw;
        map["wifiappw"] = QString(); // TODO add secured transfer via PKI
        map["wificlientpw"] = QString(); // TODO add secured transfer via PKI
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

    map["fwversion"] = gwFirmwareVersion;
    map["rfconnected"] = gwRfConnected;
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

    if (gwDeviceName.isEmpty())
    {
        gwDeviceName = apsCtrl->getParameter(deCONZ::ParamDeviceName);
    }

    if (!gwDeviceName.isEmpty())
    {
        map["devicename"] = gwDeviceName;
    }

    if (gwConfig.contains(QLatin1String("ntp")))
    {
        map["ntp"] = gwConfig["ntp"];
    }

    map["modelid"] = QLatin1String("deCONZ");
    map["dhcp"] = true; // dummy
    map["proxyaddress"] = gwProxyAddress;
    map["proxyport"] = (double)gwProxyPort;
    map["UTC"] = datetime.toString(QLatin1String("yyyy-MM-ddTHH:mm:ss")); // ISO 8601
    map["localtime"] = localtime.toString(QLatin1String("yyyy-MM-ddTHH:mm:ss")); // ISO 8601
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

    if (gwDeviceName.isEmpty())
    {
        gwDeviceName = apsCtrl->getParameter(deCONZ::ParamDeviceName);
    }

    if (!gwDeviceName.isEmpty())
    {
        map["devicename"] = gwDeviceName;
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
    QVariantMap resourcelinksMap;
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

    // resourcelinks
    {
        std::vector<Resourcelinks>::const_iterator i = resourcelinks.begin();
        std::vector<Resourcelinks>::const_iterator end = resourcelinks.end();

        for (; i != end; ++i)
        {
            if (i->state != Resourcelinks::StateNormal)
            {
                continue;
            }
            resourcelinksMap[i->id] = i->data;
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
    rsp.map["resourcelinks"] = resourcelinksMap;
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

    // add more details if this was requested from discover page
    // this should speedup multi-gateway discovery
    if (!gateways.empty())
    {
        // restrict info TODO more limited "Origin"
        QString referer = req.hdr.value(QLatin1String("Referer"));
        if (referer.contains(QLatin1String("js/scanner-worker.js")))
        {
            QVariantList ls;
            for (const Gateway *gw : gateways)
            {
                DBG_Assert(gw != 0);
                if (gw)
                {
                    QVariantMap g;
                    g["host"] = gw->address().toString();
                    g["port"] = gw->port();
                    ls.push_back(g);
                }
            }

            if (!ls.empty())
            {
                rsp.map["gateways"] = ls;
            }
        }
    }

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
            int rc = setenv("TZ", qPrintable(timezone), 1);
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
            if (!utc.isValid() || map["utc"].toString().length() != 19)
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

#ifdef ARCH_ARM
        int ret = 0;
        std::string date = map["utc"].toString().toStdString();

        time_t mytime = time(0);
        struct tm* tm_ptr = localtime(&mytime);

        if (tm_ptr)
        {
            tm_ptr->tm_year = atoi(date.substr(0,4).c_str()) - 1900;
            tm_ptr->tm_mon  = atoi(date.substr(5,2).c_str()) - 1;
            tm_ptr->tm_mday = atoi(date.substr(8,2).c_str());
            tm_ptr->tm_hour  = atoi(date.substr(11,2).c_str());
            tm_ptr->tm_min  = atoi(date.substr(14,2).c_str());
            tm_ptr->tm_sec  = atoi(date.substr(17,2).c_str());

            const struct timeval tv = {mktime(tm_ptr), 0};
            ret = settimeofday(&tv, NULL);
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
        queSaveDb(DB_CONFIG, DB_SHORT_SAVE_DELAY);
        pollSwUpdateStateTimer->start(5000);
    }

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
    // reset only allowed for certain referers
    bool ok = true;
    QString referer = req.hdr.value(QLatin1String("Referer"));
    if (referer.isEmpty() || !referer.contains(QLatin1String("login.html")))
    {
        ok = false;
    }

    // reset only allowed within first 10 minutes after startup
    if (ok && getUptime() > 600)
    {
        ok = false;
    }

    if (!ok)
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

/*! GET /api/config/wifi
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::getWifiState(const ApiRequest &req, ApiResponse &rsp)
{
    Q_UNUSED(req);

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

/*! PUT /api/config/wifi
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::configureWifi(const ApiRequest &req, ApiResponse &rsp)
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
        rsp.list.append(errorToMap(ERR_INVALID_JSON, "/config/wifi", "body contains invalid JSON"));
        return REQ_READY_SEND;
    }

    if (map.contains("type"))
    {
        QString type = map["type"].toString();

        if ((map["type"].type() != QVariant::String) || ((type != "accesspoint") && (type != "client")))
        {
            rsp.httpStatus = HttpStatusBadRequest;
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, "/config/wifi", QString("invalid value, %1 for parameter, type").arg(type)));
            return REQ_READY_SEND;
        }

        gwWifiType = type;
        gwWifi = "configured";
    }
    if (map.contains("name"))
    {
        QString name = map["name"].toString();

        if ((map["name"].type() != QVariant::String) || name.isEmpty())
        {
            rsp.httpStatus = HttpStatusBadRequest;
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, "/config/wifi", QString("invalid value, %1 for parameter, name").arg(name)));
            return REQ_READY_SEND;
        }

        if (gwWifiType == "accesspoint")
        {
            gwWifiName = name;
        }
        else
        {
            gwWifiClientName = name;
        }
    }
    if (map.contains("password"))
    {
        QString password = map["password"].toString();

        if ((map["password"].type() != QVariant::String) || password.isEmpty())
        {
            rsp.httpStatus = HttpStatusBadRequest;
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, "/config/wifi", QString("invalid value, %1 for parameter, password").arg(password)));
            return REQ_READY_SEND;
        }

        if (gwWifiType == "accesspoint")
        {
            gwWifiPw = password;
        }
        else
        {
            gwWifiClientPw = password;
        }
    }
    if (map.contains("wifi"))
    {
        QString wifi = map["wifi"].toString();

        if ((map["wifi"].type() != QVariant::String) || ((wifi != "configured") && (wifi != "not-configured") && (wifi != "new-configured") && (wifi != "deactivated")))
        {
            rsp.httpStatus = HttpStatusBadRequest;
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, "/config/wifi", QString("invalid value, %1 for parameter, wifi").arg(wifi)));
            return REQ_READY_SEND;
        }
        gwWifi = wifi;
    }

    /*
            if (map.contains("channel"))
            {
                bool ok;
                int channel = map["channel"].toInt(&ok);
                if (ok && channel >= 1 && channel <= 11)
                {
                    gwWifiChannel = channel;
                }
                else
                {
                    rsp.httpStatus = HttpStatusBadRequest;
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, "/config/wifi", QString("invalid value, %1 for parameter, channel").arg(channel)));
                    return REQ_READY_SEND;
                }
            }
    */

    updateEtag(gwConfigEtag);
    queSaveDb(DB_CONFIG,DB_SHORT_SAVE_DELAY);

    QVariantMap rspItem;
    QVariantMap rspItemState;
    rspItemState["/config/wifi/"] = gwWifi;
    rspItem["success"] = rspItemState;
    rsp.list.append(rspItem);

    return REQ_READY_SEND;
}

/*! PUT /api/config/wifi/restore
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::restoreWifiConfig(const ApiRequest &req, ApiResponse &rsp)
{
    Q_UNUSED(req);

    rsp.httpStatus = HttpStatusOk;
    QVariantMap rspItem;
    QVariantMap rspItemState;
    rspItemState["/config/wifi/restore"] = "original configuration restored";
    rspItem["success"] = rspItemState;
    rsp.list.append(rspItem);
    return REQ_READY_SEND;
}

/*! PUT /api/config/wifi/scanresult
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::putWifiScanResult(const ApiRequest &req, ApiResponse &rsp)
{
    QHostAddress localHost(QHostAddress::LocalHost);
    rsp.httpStatus = HttpStatusForbidden;

    if (req.sock->peerAddress() != localHost)
    {
        rsp.list.append(errorToMap(ERR_UNAUTHORIZED_USER, req.path.join("/"), "unauthorized user"));
        return REQ_READY_SEND;
    }

    rsp.httpStatus = HttpStatusOk;

    bool ok;
    QVariant var = Json::parse(req.content, ok);
    if (ok)
    {
        gwWifiAvailable = var.toList();
    }

    return REQ_READY_SEND;
}

/*! PUT /api/config/wifi/updated (wifi service notifications)
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::putWifiUpdated(const ApiRequest &req, ApiResponse &rsp)
{
    QHostAddress localHost(QHostAddress::LocalHost);
    rsp.httpStatus = HttpStatusForbidden;

    if (req.sock->peerAddress() != localHost)
    {
        rsp.list.append(errorToMap(ERR_UNAUTHORIZED_USER, req.path.join("/"), "unauthorized user"));
        return REQ_READY_SEND;
    }

    rsp.httpStatus = HttpStatusOk;

    if (!req.content.isEmpty())
    {
        DBG_Printf(DBG_HTTP, "wifi: %s\n", qPrintable(req.content));
        // TODO forward events
    }
    return REQ_READY_SEND;
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

/* Check daylight state */
void DeRestPluginPrivate::daylightTimerFired()
{
    Sensor *sensor = getSensorNodeForId(daylightSensorId);
    DBG_Assert(sensor != 0);
    if (!sensor)
    {
        return;
    }

    double lat = NAN;
    double lng = NAN;
    ResourceItem *configured = sensor->item(RConfigConfigured);
    if (!configured || !configured->toBool())
    {
        return;
    }

    {
        ResourceItem *ilat = sensor->item(RConfigLat);
        ResourceItem *ilng = sensor->item(RConfigLong);
        if (!ilat || !ilng)
        {
            return;
        }

        bool ok1;
        bool ok2;
        lat = ilat->toString().toDouble(&ok1);
        lng = ilng->toString().toDouble(&ok2);
        if (!ok1 || !ok2)
        {
            return;
        }
    }

    ResourceItem *daylight = sensor->item(RStateDaylight);
    ResourceItem *status = sensor->item(RStateStatus);
    ResourceItem *sunriseOffset = sensor->item(RConfigSunriseOffset);
    ResourceItem *sunsetOffset = sensor->item(RConfigSunsetOffset);
    DBG_Assert(daylight && status && sunriseOffset && sunsetOffset);
    if (!daylight || !status || !sunriseOffset || !sunsetOffset)
    {
        return;
    }

    std::vector<DL_Result> daylightTimes;

    quint64 nowMs = QDateTime::currentDateTime().toMSecsSinceEpoch();
    getDaylightTimes(nowMs, lat, lng, daylightTimes);

    const char *curName = 0;
    int cur = 0;
    quint64 sunrise = 0;
    quint64 sunset = 0;

    for (const DL_Result &r : daylightTimes)
    {
        //qDebug() << r.name << QDateTime::fromMSecsSinceEpoch(r.msecsSinceEpoch).toString();

        if (r.msecsSinceEpoch <= nowMs)
        {
            curName = r.name;
            cur = r.weight;
        }

        if      (r.weight == DL_SUNRISE_START)  { sunrise = r.msecsSinceEpoch; }
        else if (r.weight == DL_SUNSET_END)     { sunset = r.msecsSinceEpoch; }
    }

    bool dl = false;
    if (sunrise > 0 && sunset > 0)
    {
        sunrise += (sunriseOffset->toNumber() * 60 * 1000);
        sunset += (sunsetOffset->toNumber() * 60 * 1000);

        if (nowMs > sunrise && nowMs < sunset)
        {
            dl = true;
        }
    }

    if (!daylight->lastSet().isValid() || daylight->toBool() != dl)
    {
        daylight->setValue(dl);
        Event e(RSensors, RStateStatus, sensor->id(), status);
        enqueueEvent(e);
        sensor->updateStateTimestamp();
        sensor->setNeedSaveDatabase(true);
        saveDatabaseItems |= DB_SENSORS;
    }

    if (cur && cur != status->toNumber())
    {
        status->setValue(cur);
        Event e(RSensors, RStateStatus, sensor->id(), status);
        enqueueEvent(e);
        sensor->updateStateTimestamp();
        sensor->setNeedSaveDatabase(true);
        saveDatabaseItems |= DB_SENSORS;
    }

    if (curName)
    {
        DBG_Printf(DBG_INFO, "Daylight now: %s, status: %d\n", curName, cur);
    }
}

/*! Manager to asure gateway has proper time.
 */
void DeRestPluginPrivate::timeManagerTimerFired()
{
    if (timeManagerState == TM_Init)
    {
        DBG_Assert(ntpqProcess == 0);
        timeManagerState = TM_WaitNtpq;
        ntpqProcess = new QProcess(this);
        connect(ntpqProcess, SIGNAL(finished(int)), this, SLOT(ntpqFinished()));
        QStringList args;
        args << "-c" << "rv";
        ntpqProcess->start(QLatin1String("ntpq"), args);
    }
}

/*! Manager to asure gateway has proper time.
 */
void DeRestPluginPrivate::ntpqFinished()
{
    DBG_Assert(ntpqProcess != 0);
    DBG_Assert(timeManagerState == TM_WaitNtpq);
    if (timeManagerState == TM_WaitNtpq && ntpqProcess)
    {
        QByteArray data = ntpqProcess->readAll();
        //DBG_Printf(DBG_INFO, "NTP exit %d, %s\n", ntpqProcess->exitCode(), qPrintable(data));

        QString ntpState;

        if (ntpqProcess->exitCode() != 0 ||
            data.contains("sync_unspec")) // ntp not yet synchronized
        {
            ntpState = QLatin1String("unsynced");
            timeManagerState = TM_Init;
            QTimer::singleShot(60000, this, SLOT(timeManagerTimerFired()));
        }
        else // synced somehow sync_*
        {
            timeManagerState = TM_NtpRunning;
            ntpState = QLatin1String("synced");
            QTimer::singleShot(30 * 60 * 1000, this, SLOT(timeManagerTimerFired()));
        }

        if (gwConfig["ntp"] != ntpState)
        {
            gwConfig["ntp"] = ntpState;
            updateEtag(gwConfigEtag);
        }

        ntpqProcess->deleteLater();
        ntpqProcess = 0;
    }
}
