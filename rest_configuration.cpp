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
  #include <signal.h>
#endif
#endif // Q_OS_LINUX

/*! Constructor. */
ApiConfig::ApiConfig() :
    Resource(RConfig)
{
}

/*! Init the configuration. */
void DeRestPluginPrivate::initConfig()
{
    QString dataPath = deCONZ::getStorageLocation(deCONZ::ApplicationsDataLocation);

    pollDatabaseWifiTimer = 0;

    // default configuration
    gwRunFromShellScript = false;
    gwDeleteUnknownRules = (deCONZ::appArgumentNumeric("--delete-unknown-rules", 1) == 1) ? true : false;
    gwRfConnected = false; // will be detected later
    gwRfConnectedExpected = (deCONZ::appArgumentNumeric("--auto-connect", 1) == 1) ? true : false;
    gwPermitJoinDuration = 0;
    gwPermitJoinResend = 0;
    gwNetworkOpenDuration = 60;
    gwWifiState = WifiStateInitMgmt;
    gwWifiMgmt = 0;
    gwWifi = QLatin1String("not-configured");
    gwWifiStateString = QLatin1String("not-configured");
    gwWifiType = QLatin1String("accesspoint");
    gwWifiName = QString();
    gwWifiEth0 = QString();
    gwWifiWlan0 = QString();
    gwWifiClientName = QString();
    gwWifiChannel = "1";
    gwWifiIp = QLatin1String("192.168.8.1");
    gwWifiPw = "";
    gwWifiClientPw = "";
    gwHomebridge = QLatin1String("not-managed");
    gwHomebridgePin = QString();
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
    gwMAC = "38:60:77:7c:53:18";
    gwIPAddress = "127.0.0.1";
    gwPort = (apsCtrl ? apsCtrl->getParameter(deCONZ::ParamHttpPort) : deCONZ::appArgumentNumeric("--http-port", 80));
    gwNetMask = "255.0.0.0";
    gwLANBridgeId = (deCONZ::appArgumentNumeric("--lan-bridgeid", 0) == 1);
    gwBridgeId = "0000000000000000";
    gwAllowLocal = (deCONZ::appArgumentNumeric("--allow-local", 1) == 1);
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
        else
        {
            DBG_Printf(DBG_INFO, "GW sd-card image version file does not exist: %s\n", qPrintable(f.fileName()));
        }
    }

    if (!gwSdImageVersion.isEmpty())
    {
        DBG_Printf(DBG_INFO, "GW sd-card image version: %s\n", qPrintable(gwSdImageVersion));
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

    connect(deCONZ::ApsController::instance(), &deCONZ::ApsController::configurationChanged,
            this, &DeRestPluginPrivate::configurationChanged);
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
        item = dl.addItem(DataTypeBool, RStateDark);
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

/*! Init the network info. */
void DeRestPluginPrivate::initNetworkInfo()
{
    bool ok = false;
    bool retry = false;

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

        retry = true;
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

                QString mac = i->hardwareAddress().toLower();
                gwMAC = mac;
                if (gwLANBridgeId) {
                    gwBridgeId = mac.mid(0,2) + mac.mid(3,2) + mac.mid(6,2) + "fffe" + mac.mid(9,2) + mac.mid(12,2) + mac.mid(15,2);
                    if (!gwConfig.contains("bridgeid") || gwConfig["bridgeid"] != gwBridgeId)
                    {
                        DBG_Printf(DBG_INFO, "Set bridgeid to %s\n", qPrintable(gwBridgeId));
                        gwConfig["bridgeid"] = gwBridgeId;
                        queSaveDb(DB_CONFIG, DB_SHORT_SAVE_DELAY);
                    }
                }
                gwIPAddress = a->ip().toString();
                gwConfig["ipaddress"] = gwIPAddress;
                gwNetMask = a->netmask().toString();
                initDescriptionXml();
                ok = true;
                retry = false;
                break;
            }
        }
    }

    if (!ok)
    {
        DBG_Printf(DBG_ERROR, "No valid ethernet interface found\n");
    }

    if (retry)
    {
        QTimer::singleShot(10000, this, SLOT(initNetworkInfo()));
    }
}

/*! Init WiFi parameters if necessary. */
void DeRestPluginPrivate::initWiFi()
{
    bool retry = false;
#if !defined(ARCH_ARMV6) && !defined (ARCH_ARMV7)
    gwWifi = QLatin1String("not-available");
    return;
#endif

    // only configure for official image
    if (gwSdImageVersion.isEmpty())
    {
        return;
    }

    if (gwWifiState == WifiStateInitMgmt)
    {
        retry = true;
    }

    if (gwBridgeId.isEmpty() || gwBridgeId.endsWith(QLatin1String("0000")))
    {
        retry = true;
    }

    quint32 fwVersion = apsCtrl->getParameter(deCONZ::ParamFirmwareVersion);
    if (fwVersion < 0x261e0500) // first version to support security material set
    {
        retry = true;
    }

    if (gwWifi != QLatin1String("not-configured"))
    {
        retry = true;
    }

    QByteArray sec0 = apsCtrl->getParameter(deCONZ::ParamSecurityMaterial0);
    if (sec0.isEmpty())
    {
        retry = true;
    }

    if (retry)
    {
        QTimer::singleShot(10000, this, SLOT(initWiFi()));
        return;
    }

    if (!pollDatabaseWifiTimer)
    {
        pollDatabaseWifiTimer = new QTimer(this);
        pollDatabaseWifiTimer->setSingleShot(false);
        connect(pollDatabaseWifiTimer, SIGNAL(timeout()),
                this, SLOT(pollDatabaseWifiTimerFired()));
        pollDatabaseWifiTimer->start(10000);
    }

    if (gwWifiMgmt & WIFI_MGMT_ACTIVE)
    {
        return;
    }

    if (gwWifiName == QLatin1String("Phoscon-Gateway-0000"))
    {
        // proceed to correct these
        gwWifiName.clear();
    }

    gwWifi = QLatin1String("configured");
    gwWifiType = QLatin1String("accesspoint");
    gwWifiStateString = QLatin1String("configured-ap");

    if (gwWifiName.isEmpty() || gwWifiName == QLatin1String("Not set"))
    {
        gwWifiName = QString("Phoscon-Gateway-%1").arg(gwBridgeId.right(4));
        gwWifiBackupName = gwWifiName;
    }

    if (gwWifiPw.isEmpty() || gwWifiPw.length() < 8)
    {
        gwWifiPw = sec0.mid(16, 16).toUpper();
        gwWifiBackupPw = gwWifiPw;
    }

    queSaveDb(DB_CONFIG, DB_SHORT_SAVE_DELAY);
}

/*! Handle deCONZ::ApsController::configurationChanged() event.
    This will be called when the configuration was changed via deCONZ network settings.
 */
void DeRestPluginPrivate::configurationChanged()
{
    if (!apsCtrl)
    {
        return;
    }

    DBG_Printf(DBG_INFO, "deCONZ configuration changed");

    bool update = false;

    const quint64 macAddress = apsCtrl->getParameter(deCONZ::ParamMacAddress);
    //const quint16 nwkAddress = apsCtrl->getParameter(deCONZ::ParamNwkAddress);
    if (macAddress != 0 && gwDeviceAddress.ext() != macAddress)
    {
        gwDeviceAddress = {}; // reset let idle timer update bridgeid
        update = true;
    }

    const quint8 channel = apsCtrl->getParameter(deCONZ::ParamCurrentChannel);
    if (channel >= 11 && channel <= 26 && gwZigbeeChannel != channel)
    {
        gwZigbeeChannel = channel;
        update = true;
    }

    if (update)
    {
        updateZigBeeConfigDb();
        queSaveDb(DB_CONFIG, DB_SHORT_SAVE_DELAY);
    }
}

/*! Configuration REST API broker.
    \param req - request data
    \param rsp - response data
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::handleConfigurationApi(const ApiRequest &req, ApiResponse &rsp)
{
    // GET /api/<apikey>/config
    if ((req.path.size() == 3) && (req.hdr.method() == "GET") && (req.path[2] == "config"))
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
    // PUT /api/<apikey>/config/homebridge/reset
    else if ((req.path.size() == 5) && (req.hdr.method() == "PUT") && (req.path[2] == "config") && (req.path[3] == "homebridge") && (req.path[4] == "reset"))
    {
        return resetHomebridge(req, rsp);
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
    QHostAddress localHost(QHostAddress::LocalHost);

    if (!gwLinkButton)
    {
        if (gwAllowLocal && req.sock->peerAddress() == localHost)
        {
            // proceed
        }
        else if (!allowedToCreateApikey(req, rsp, map))
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

    basicConfigToMap(map);
    map["ipaddress"] = gwIPAddress;
    map["netmask"] = gwNetMask;
    if (gwDeviceName.isEmpty())
    {
        gwDeviceName = apsCtrl->getParameter(deCONZ::ParamDeviceName);
    }

    if (!gwDeviceName.isEmpty())
    {
        map["devicename"] = gwDeviceName;
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
        map["port"] = gwPort;
        // since api version 1.2.1
        map["apiversion"] = QLatin1String(GW_SW_VERSION);
        map["system"] = "other";
#if defined(ARCH_ARMV6) || defined (ARCH_ARMV7)
#ifdef Q_OS_LINUX
        map["system"] = "linux-gw";
#endif
        map["wifi"] = gwWifi;
        map["homebridge"] = gwHomebridge;
        map["homebridgepin"] = gwHomebridgePin;
#else
        map["wifi"] = QLatin1String("not-available");
        map["homebridge"] = QLatin1String("not-available");
#endif
        map["wifiavailable"] = gwWifiAvailable;
        map["wifitype"] = gwWifiType;
        map["wifiname"] = gwWifiName;
        map["wificlientname"] = gwWifiClientName;
        map["wifichannel"] = gwWifiChannel;
        map["wifimgmt"] = (double)gwWifiMgmt;
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
            map["modelid"] = QLatin1String("BSB002");
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
        map["swupdate"] = swupdate;
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

    if (gwConfig.contains(QLatin1String("ntp")))
    {
        map["ntp"] = gwConfig["ntp"];
    }

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

    QStringList ipv4 = gwIPAddress.split(".");

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
    map["name"] = gwName;
    map["datastoreversion"] = QLatin1String("60");
    QStringList versions = QString(GW_SW_VERSION).split('.');
    QString swversion;
    swversion.sprintf("%d.%d.%d", versions[0].toInt(), versions[1].toInt(), versions[2].toInt());
    map["swversion"] = swversion;
    map["apiversion"] = QString(GW_API_VERSION);
    map["mac"] = gwMAC;
    map["bridgeid"] = gwBridgeId;
    map["factorynew"] = false;
    map["replacesbridgeid"] = QVariant();
    map["modelid"] = QLatin1String("deCONZ");
    map["starterkitid"] = QLatin1String("");
}

/*! GET /api/<apikey>
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::getFullState(const ApiRequest &req, ApiResponse &rsp)
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

    QVariantMap lightsMap;
    QVariantMap groupsMap;
    QVariantMap schedulesMap;
    QVariantMap scenesMap;
    QVariantMap sensorsMap;
    QVariantMap rulesMap;
    QVariantMap configMap;
    QVariantMap resourcelinksMap;

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
                if (groupToMap(req, &(*i), map))
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

    // scenes
    {
    }

    configToMap(req, configMap);

    rsp.map["lights"] = lightsMap;
    rsp.map["groups"] = groupsMap;
    rsp.map["schedules"] = schedulesMap;
    rsp.map["scenes"] = scenesMap;
    rsp.map["sensors"] = sensorsMap;
    rsp.map["rules"] = rulesMap;
    rsp.map["config"] = configMap;
    rsp.map["resourcelinks"] = resourcelinksMap;
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
            startSearchLights();
            startSearchSensors();
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
        rsp.list.append(errorToMap(ERR_UNAUTHORIZED_USER, "/" + req.path.join("/"), "unauthorized user"));
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

        const quint32 fwVersion = apsCtrl->getParameter(deCONZ::ParamFirmwareVersion);
        QString str;
        str.sprintf("0x%08x", fwVersion);

        if (gwFirmwareVersion != str)
        {
            gwFirmwareVersion = str;
            gwConfig["fwversion"] = str;
            updateEtag(gwConfigEtag);
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
    rsp.map["state"] = gwWifiStateString;
    rsp.map["type"] = gwWifiType;
    rsp.map["ip"] = gwWifiIp;
    rsp.map["name"] = gwWifiName;
    rsp.map["pw"] = QString();
    rsp.map["workingtype"] = gwWifiWorkingType;
    rsp.map["workingname"] = gwWifiWorkingName;
    rsp.map["workingpw"] = QString();
    // rsp.map["wifiappw"] = gwWifiPw;
    rsp.map["wifiappw"] = QString();
    rsp.map["wifiavailable"] = gwWifiAvailable;
    rsp.map["lastupdated"] = gwWifiLastUpdated;
    rsp.map["eth0"] = gwWifiEth0;
    rsp.map["wlan0"] = gwWifiWlan0;
    rsp.map["active"] = gwWifiActive;

    rsp.httpStatus = HttpStatusOk;

    return REQ_READY_SEND;
}

/*! PUT /api/config/wifi
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::configureWifi(const ApiRequest &req, ApiResponse &rsp)
{
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
            gwWifiName = name;
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
            gwWifiPw = password;
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

    QDateTime currentDateTime = QDateTime::currentDateTimeUtc();
    gwWifiLastUpdated = currentDateTime.toTime_t();

    updateEtag(gwConfigEtag);
    queSaveDb(DB_CONFIG | DB_SYNC, DB_SHORT_SAVE_DELAY);

#ifdef ARCH_ARM
    kill(gwWifiPID, SIGUSR1);
#endif

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
        rsp.list.append(errorToMap(ERR_UNAUTHORIZED_USER, "/" + req.path.join("/"), "unauthorized user"));
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
        rsp.list.append(errorToMap(ERR_UNAUTHORIZED_USER, "/" + req.path.join("/"), "unauthorized user"));
        return REQ_READY_SEND;
    }

    int pid = req.path[1].toInt();
    if (gwWifiPID != pid)
    {
        gwWifiPID = pid;
    }

    rsp.httpStatus = HttpStatusOk;

    if (req.content.isEmpty())
    {
        return REQ_READY_SEND;
    }

    // TODO forward events
    bool ok;
    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();

    if (!ok || map.isEmpty())
    {
        return REQ_READY_SEND;
    }

    QString status;

    if (map.contains("status"))
    {
        status = map["status"].toString();
    }

    if (status == QLatin1String("current-config") && map.contains("mgmt"))
    {
        quint32 mgmt = map["mgmt"].toUInt();

        if (gwWifiMgmt != mgmt)
        {
            gwWifiMgmt = mgmt;

            if (gwWifiMgmt & WIFI_MGMT_ACTIVE)
            {
                gwWifiActive = QLatin1String("active");
            }
            else
            {
                gwWifiActive = QLatin1String("inactive");
                gwWifiWorkingName = QString();
                gwWifiWorkingType = QString();
            }

            updateEtag(gwConfigEtag);
        }

        QString type;
        QString ssid;

        if (map.contains("type")) { type = map["type"].toString(); }
        if (map.contains("ssid")) { ssid = map["ssid"].toString(); }

        if (gwWifiState == WifiStateInitMgmt)
        {
            gwWifiState = WifiStateIdle;

            if (type == QLatin1String("accesspoint") && !ssid.isEmpty())
            {
                if (gwWifi != QLatin1String("not-configured") && (gwWifiMgmt & WIFI_MGTM_HOSTAPD) == 0)
                {
                    gwWifi = QLatin1String("not-configured"); // not configured by deCONZ
                }

                if (gwWifiMgmt & WIFI_MGMT_ACTIVE)
                {
                    gwWifiActive = QLatin1String("active");
                    gwWifiWorkingName = ssid;
                    gwWifiWorkingType = type;
                }
                else
                {
                    gwWifiActive = QLatin1String("inactive");
                    gwWifiWorkingName = QString();
                    gwWifiWorkingType = QString();
                }
            }

            if (type == QLatin1String("client") && !ssid.isEmpty())
            {
                if (gwWifi != QLatin1String("not-configured") && (gwWifiMgmt & WIFI_MGTM_WPA_SUPPLICANT) == 0)
                {
                    gwWifi = QLatin1String("not-configured"); // not configured by deCONZ
                }

                if (gwWifiMgmt & WIFI_MGMT_ACTIVE)
                {
                    gwWifiActive = QLatin1String("active");
                    gwWifiWorkingName = ssid;
                    gwWifiWorkingType = type;
                }
                else
                {
                    gwWifiActive = QLatin1String("inactive");
                    gwWifiWorkingName = QString();
                    gwWifiWorkingType = QString();
                }
                gwWifiClientName = ssid;
            }

            updateEtag(gwConfigEtag);
        }
    }
    else if (status == QLatin1String("current-config"))
    {
        QString workingtype;
        QString workingname;
        QString workingpw;

        if (map.contains("workingtype")) { workingtype = map["workingtype"].toString(); }
        if (map.contains("workingname")) { workingname = map["workingname"].toString(); }
        if (map.contains("workingpw")) { workingpw = map["workingpw"].toString(); }

        bool changed = false;
        if (gwWifiWorkingType != workingtype)
        {
            gwWifiWorkingType = workingtype;
            changed = true;
        }

        if (gwWifiWorkingName != workingname)
        {
            gwWifiWorkingName = workingname;
            changed = true;
        }

        if (gwWifiWorkingPw != workingpw)
        {
            gwWifiWorkingPw = workingpw;
            changed = true;
        }

        if (changed)
        {
            updateEtag(gwConfigEtag);
            queSaveDb(DB_CONFIG | DB_SYNC, DB_SHORT_SAVE_DELAY);
        }
    }

    else if (status == QLatin1String("got-ip"))
    {
        QString ip = map["ipv4"].toString();

        if (!ip.isEmpty() && gwWifiIp != ip)
        {
            if (gwWifiActive != QLatin1String("active"))
            {
                gwWifiActive = QLatin1String("active");
            }

            gwWifiIp = ip;
            updateEtag(gwConfigEtag);
        }

        if (gwWifiWlan0 != ip)
        {
            if (ip.isEmpty())
            {
                gwWifiWlan0 = QString();
            }
            else
            {
                gwWifiWlan0 = ip;
            }
            updateEtag(gwConfigEtag);
        }
    }
    else if (status == QLatin1String("got-ip-eth0"))
    {
        QString ip = map["ipv4"].toString();

        if (gwWifiEth0 != ip)
        {
            if (ip.isEmpty())
            {
                gwWifiEth0 = QString();
            }
            else
            {
                gwWifiEth0 = ip;
            }
            updateEtag(gwConfigEtag);
        }
    }
    else if (status == QLatin1String("ap-connecting") && gwWifiStateString != QLatin1String("ap-connecting"))
    {
        gwWifiStateString = QLatin1String("ap-connecting");

        updateEtag(gwConfigEtag);
    }
    else if (status == QLatin1String("client-connecting") && gwWifiStateString != QLatin1String("client-connecting"))
    {
        gwWifiStateString = QLatin1String("client-connecting");

        updateEtag(gwConfigEtag);
    }
    else if (status == QLatin1String("ap-configured") && gwWifiStateString != QLatin1String("ap-configured"))
    {
        bool changed = false;
        gwWifiStateString = QLatin1String("ap-configured");

        if (gwWifiWorkingType != QLatin1String("accesspoint"))
        {
            gwWifiWorkingType = QLatin1String("accesspoint");
            changed = true;
        }

        if (gwWifiActive != QLatin1String("active"))
        {
            gwWifiActive = QLatin1String("active");
            changed = true;
        }

        updateEtag(gwConfigEtag);

        if (changed)
        {
            queSaveDb(DB_CONFIG | DB_SYNC, DB_SHORT_SAVE_DELAY);
        }
    }
    else if (status == QLatin1String("client-configured") && gwWifiStateString != QLatin1String("client-configured"))
    {
        bool changed = false;
        gwWifiStateString = QLatin1String("client-configured");

        if (gwWifiWorkingType != QLatin1String("client"))
        {
            gwWifiWorkingType = QLatin1String("client");
            changed = true;
        }

        if (gwWifiActive != QLatin1String("active"))
        {
            gwWifiActive = QLatin1String("active");
            changed = true;
        }

        updateEtag(gwConfigEtag);

        if (changed)
        {
            queSaveDb(DB_CONFIG | DB_SYNC, DB_SHORT_SAVE_DELAY);
        }
    }
    else if (status == QLatin1String("ap-connect-fail") && gwWifiStateString != QLatin1String("ap-connect-fail"))
    {
        gwWifiStateString = QLatin1String("ap-connect-fail");

        if (gwWifiWorkingType != QLatin1String("accesspoint"))
        {
            gwWifiWorkingType = QLatin1String("accesspoint");
        }

        if (gwWifiActive != QLatin1String("inactive"))
        {
            gwWifiActive = QLatin1String("inactive");
        }

        gwWifiWorkingName = QString();
        gwWifiWorkingPw = QString();
        gwWifiWorkingType = QString();

        updateEtag(gwConfigEtag);
    }
    else if (status == QLatin1String("client-connect-fail") && gwWifiStateString != QLatin1String("client-connect-fail"))
    {
        gwWifiStateString = QLatin1String("client-connect-fail");

        if (gwWifiWorkingType != QLatin1String("client"))
        {
            gwWifiWorkingType = QLatin1String("client");
        }

        if (gwWifiActive != QLatin1String("inactive"))
        {
            gwWifiActive = QLatin1String("inactive");
        }

        gwWifiWorkingName = QString();
        gwWifiWorkingPw = QString();
        gwWifiWorkingType = QString();

        updateEtag(gwConfigEtag);
    }
    else if (status == QLatin1String("check-ap") && gwWifiStateString != QLatin1String("check-ap"))
    {
        gwWifiStateString = QLatin1String("check-ap");

        if (gwWifiWorkingType != QLatin1String("accesspoint"))
        {
            gwWifiWorkingType = QLatin1String("accesspoint");
        }

        updateEtag(gwConfigEtag);
    }
    else if (status == QLatin1String("check-client") && gwWifiStateString != QLatin1String("check-client"))
    {
        gwWifiStateString = QLatin1String("check-client");

        if (gwWifiWorkingType != QLatin1String("client"))
        {
            gwWifiWorkingType = QLatin1String("client");
        }

        updateEtag(gwConfigEtag);
    }
    else if (status == QLatin1String("not-configured") && gwWifiStateString != QLatin1String("not-configured"))
    {
        gwWifiStateString = QLatin1String("not-configured");

        if (gwWifi != QLatin1String("not-configured"))
        {
            gwWifi = QLatin1String("not-configured");
        }

        updateEtag(gwConfigEtag);
    }
    else if (status == QLatin1String("deactivated") && gwWifiStateString != QLatin1String("deactivated"))
    {
        gwWifiStateString = QLatin1String("deactivated");

        if (gwWifi != QLatin1String("deactivated"))
        {
            gwWifi = QLatin1String("deactivated");
        }

        if (gwWifiActive != QLatin1String("inactive"))
        {
            gwWifiActive = QLatin1String("inactive");
        }

        gwWifiWorkingName = QString();
        gwWifiWorkingPw = QString();
        gwWifiWorkingType = QString();

        updateEtag(gwConfigEtag);
    }
    else if (status == QLatin1String("check-config") && gwWifiStateString != QLatin1String("check-config"))
    {
        gwWifiStateString = QLatin1String("check-config");
        updateEtag(gwConfigEtag);
    }
    else if (status == QLatin1String("last-working-config") && gwWifiStateString != QLatin1String("last-working-config"))
    {
        gwWifiStateString = QLatin1String("last-working-config");
        updateEtag(gwConfigEtag);
    }
    else if (status == QLatin1String("ap-backup") && gwWifiStateString != QLatin1String("ap-backup"))
    {
        gwWifiStateString = QLatin1String("ap-backup");
        updateEtag(gwConfigEtag);
    }

    DBG_Printf(DBG_HTTP, "wifi: %s\n", qPrintable(req.content));
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

/*! PUT /api/config/homebridge/reset
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::resetHomebridge(const ApiRequest &req, ApiResponse &rsp)
{
    Q_UNUSED(req);

    rsp.httpStatus = HttpStatusOk;
#ifdef ARCH_ARM
    gwHomebridge = QLatin1String("reset");
    queSaveDb(DB_CONFIG | DB_SYNC, DB_SHORT_SAVE_DELAY);
#endif
    QVariantMap rspItem;
    QVariantMap rspItemState;
    rspItemState["/config/homebridge/reset"] = "success";
    rspItem["success"] = rspItemState;
    rsp.list.append(rspItem);
    return REQ_READY_SEND;
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

    {
        // check uniqueid
        // note: might change if device is changed
        ResourceItem *item = sensor->item(RAttrUniqueId);
        QString uniqueid = gwBridgeId.toLower() + QLatin1String("-01");
        // 00:21:2e:ff:ff:00:aa:bb-01
        for (int i = 0; i < (7 * 3); i += 3)
        {
            uniqueid.insert(i + 2, ':');
        }

        if (!item || (item->toString() != uniqueid))
        {
            item = sensor->addItem(DataTypeString, RAttrUniqueId);
            item->setValue(uniqueid);
        }
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
    ResourceItem *dark = sensor->item(RStateDark);
    ResourceItem *status = sensor->item(RStateStatus);
    ResourceItem *sunriseOffset = sensor->item(RConfigSunriseOffset);
    ResourceItem *sunsetOffset = sensor->item(RConfigSunsetOffset);
    DBG_Assert(daylight && status && sunriseOffset && sunsetOffset);
    if (!daylight || !dark || !status || !sunriseOffset || !sunsetOffset)
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
    quint64 dawn = 0;
    quint64 dusk = 0;

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
        else if (r.weight == DL_DAWN)           { dawn = r.msecsSinceEpoch; }
        else if (r.weight == DL_DUSK)           { dusk = r.msecsSinceEpoch; }
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

    bool dk = true;
    if (dawn > 0 && dusk > 0)
    {
        dawn += (sunriseOffset->toNumber() * 60 * 1000);
        dusk += (sunsetOffset->toNumber() * 60 * 1000);

        if (nowMs > dawn && nowMs < dusk)
        {
            dk = false;
        }
    }

    bool updated = false;

    if (!daylight->lastSet().isValid() || daylight->toBool() != dl)
    {
        daylight->setValue(dl);
        Event e(RSensors, RStateDaylight, sensor->id(), daylight);
        enqueueEvent(e);
        updated = true;
    }

    if (!dark->lastSet().isValid() || dark->toBool() != dk)
    {
        dark->setValue(dk);
        Event e(RSensors, RStateDark, sensor->id(), dark);
        enqueueEvent(e);
        updated = true;
    }

    if (cur && cur != status->toNumber())
    {
        status->setValue(cur);
        Event e(RSensors, RStateStatus, sensor->id(), status);
        enqueueEvent(e);
        updated = true;
    }

    if (updated)
    {
        sensor->updateStateTimestamp();
        enqueueEvent(Event(RSensors, RStateLastUpdated, sensor->id()));
        sensor->setNeedSaveDatabase(true);
        saveDatabaseItems |= DB_SENSORS;
    }

    if (curName)
    {
        DBG_Printf(DBG_INFO_L2, "Daylight now: %s, status: %d, daylight: %d, dark: %d\n", curName, cur, dl, dk);
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
