/*
 * Copyright (c) 2016-2020 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QFile>
#include <QHostInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QSysInfo>
#include "de_web_plugin_private.h"
#include "json.h"
#ifdef Q_OS_LINUX
#include <unistd.h>
#include <sys/time.h>
#endif

/*! Inits the internet discovery manager.
 */
void DeRestPluginPrivate::initInternetDicovery()
{
    Q_ASSERT(inetDiscoveryManager == 0);
    inetDiscoveryManager = new QNetworkAccessManager;
    connect(inetDiscoveryManager, SIGNAL(finished(QNetworkReply*)),
            this, SLOT(internetDiscoveryFinishedRequest(QNetworkReply*)));


    DBG_Assert(gwAnnounceInterval >= 0);
    if (gwAnnounceInterval < 0)
    {
        gwAnnounceInterval = ANNOUNCE_INTERVAL;
    }

    gwAnnounceVital = 0;
    inetDiscoveryTimer = new QTimer(this);
    inetDiscoveryTimer->setSingleShot(false);

    {
        QList<QNetworkProxy> proxies = QNetworkProxyFactory::systemProxyForQuery(QNetworkProxyQuery(gwAnnounceUrl));

        if (!proxies.isEmpty())
        {
            const QNetworkProxy &proxy = proxies.first();
            if (proxy.type() == QNetworkProxy::HttpProxy || proxy.type() == QNetworkProxy::HttpCachingProxy)
            {
                gwProxyPort = proxy.port();
                gwProxyAddress = proxy.hostName();
                inetDiscoveryManager->setProxy(proxy);
                QHostInfo::lookupHost(proxy.hostName(),
                                      this, SLOT(inetProxyHostLookupDone(QHostInfo)));
            }
        }
    }

    connect(inetDiscoveryTimer, SIGNAL(timeout()),
            this, SLOT(internetDiscoveryTimerFired()));

    setInternetDiscoveryInterval(gwAnnounceInterval);

    // force first run
    if (gwAnnounceInterval > 0)
    {
        QTimer::singleShot(5000, this, SLOT(internetDiscoveryTimerFired()));
    }

    {
        QFile f("/etc/os-release");
        if (f.exists() && f.open(QFile::ReadOnly))
        {
            QTextStream stream(&f);

            while (!stream.atEnd())
            {
                QString line = stream.readLine(200);
                QStringList lineLs = line.split(QChar('='));

                if (lineLs.size() != 2)
                {
                    continue;
                }

                if (lineLs[0] == QLatin1String("PRETTY_NAME"))
                {
                    osPrettyName = lineLs[1];
                    osPrettyName.remove(QChar('"'));
                }
            }
        }
    }
#ifdef ARCH_ARM
    { // get raspberry pi revision
        QFile f("/proc/cpuinfo");
        if (f.exists() && f.open(QFile::ReadOnly))
        {
            QByteArray arr = f.readAll();
            QTextStream stream(arr);

            while (!stream.atEnd())
            {
                QString line = stream.readLine(200);
                QStringList lineLs = line.split(QChar(':'));

                if (lineLs.size() != 2)
                {
                    continue;
                }

                if (lineLs[0].startsWith(QLatin1String("Revision")))
                {
                    piRevision = lineLs[1].trimmed();
                    break;
                }
            }
        }
    }
#endif // ARCH_ARM

    if (osPrettyName.isEmpty())
    {
#ifdef Q_OS_WIN
        switch (QSysInfo::WindowsVersion)
        {
        case QSysInfo::WV_XP:         osPrettyName = QLatin1String("WinXP"); break;
        case QSysInfo::WV_WINDOWS7:   osPrettyName = QLatin1String("Win7"); break;
        case QSysInfo::WV_WINDOWS8:   osPrettyName = QLatin1String("Win8"); break;
        case QSysInfo::WV_WINDOWS8_1: osPrettyName = QLatin1String("Win8.1"); break;
        case QSysInfo::WV_WINDOWS10:  osPrettyName = QLatin1String("Win10"); break;
        default:
            osPrettyName = QLatin1String("Win");
            break;
        }
#endif
#ifdef Q_OS_OSX
        osPrettyName = "Mac";
#endif
#ifdef Q_OS_LINUX
        osPrettyName = "Linux";
#endif
    }
}

/*! Sets the announce interval for internet discovery.

    \param minutes a value between 0..180 min
    \return true on success
 */
bool DeRestPluginPrivate::setInternetDiscoveryInterval(int minutes)
{
    if ((minutes < 0) || (minutes > 180))
    {
        DBG_Printf(DBG_INFO, "discovery ignored invalid announce interval (%d minutes)\n", minutes);
        return false;
    }

    inetDiscoveryTimer->stop();
    if (gwAnnounceInterval != minutes)
    {
        DBG_Printf(DBG_INFO, "discovery updated announce interval to %d minutes\n", minutes);
    }

    gwAnnounceInterval = minutes;

    if (gwAnnounceInterval > 0)
    {
        int msec = 1000 * 60 * gwAnnounceInterval;
        inetDiscoveryTimer->start(msec);
    }
    return true;
}

/*! Handle announce trigger timer.
 */
void DeRestPluginPrivate::internetDiscoveryTimerFired()
{
    if (gwAnnounceInterval <= 0)
    {
        return;
    }

    if (gwSwUpdateState == swUpdateState.transferring || gwSwUpdateState == swUpdateState.installing)
    {
        return; // don't interfere with running operations
    }

    int i = 0;
    const deCONZ::Node *node;
    deCONZ::ApsController *ctrl = deCONZ::ApsController::instance();

    while (ctrl && ctrl->getNode(i, &node) == 0)
    {
      i++;
    }

    QVariantMap map;
    map["name"] = gwName;
    map["mac"] = gwBridgeId;
    map["internal_ip"] = gwConfig["ipaddress"].toString();
    map["internal_port"] = gwConfig["port"].toDouble();
    map["interval"] = gwAnnounceInterval;
    map["swversion"] = gwConfig["swversion"].toString();
    map["fwversion"] = gwConfig["fwversion"].toString();
    map["nodecount"] = (double)i;
    map["uptime"] = (double)getUptime();
    map["updatechannel"] = gwUpdateChannel;
    map["os"] = osPrettyName;
    map["runmode"] = gwRunMode;
    if (!piRevision.isEmpty())
    {
        map["pirev"] = piRevision;
    }

    QByteArray data = Json::serialize(map);
    inetDiscoveryManager->put(QNetworkRequest(QUrl(gwAnnounceUrl)), data);
}

/*! Callback for finished HTTP requests.

    \param reply the reply to a HTTP request
 */
void DeRestPluginPrivate::internetDiscoveryFinishedRequest(QNetworkReply *reply)
{
    DBG_Assert(reply != 0);

    if (!reply)
    {
        return;
    }

    if (reply->error() == QNetworkReply::NoError)
    {
        if (gwAnnounceVital < 0)
        {
            gwAnnounceVital = 0;
        }
        gwAnnounceVital++;
        DBG_Printf(DBG_INFO, "Announced to internet %s\n", qPrintable(gwAnnounceUrl));
        internetDiscoveryExtractGeo(reply);
        internetDiscoveryExtractVersionInfo(reply);
    }
    else
    {
        DBG_Printf(DBG_INFO, "discovery network reply error: %s\n", qPrintable(reply->errorString()));
        if (gwAnnounceVital > 0)
        {
            gwAnnounceVital = 0;
        }
        gwAnnounceVital--;

        if (gwProxyAddress != QLatin1String("none") && gwProxyPort > 0)
        {
            if (inetDiscoveryManager->proxy().type() != QNetworkProxy::HttpProxy)
            {
                //first fail, speed up retry
                QTimer::singleShot(5000, this, SLOT(internetDiscoveryTimerFired()));
            }

            QNetworkProxy proxy(QNetworkProxy::HttpProxy, gwProxyAddress, gwProxyPort);
            inetDiscoveryManager->setProxy(proxy);
        }

        if (gwAnnounceVital < -10) // try alternative path
        {
            gwAnnounceUrl = QLatin1String("https://phoscon.de/discover");
        }
    }

    reply->deleteLater();
}

/*! Fills major.minor.patch versions as int in the array \p ls.
    \returns true if \p version is a valid version string and \p ls could be filled.
 */
bool versionToIntList(const QString &version, std::array<int, 3> &ls)
{
    bool result = false;
    const auto versionList = version.split('.');

    if (versionList.size() >= 3)
    {
        for (size_t i = 0; i < ls.size(); i++)
        {
            ls[i] = versionList[i].toInt(&result);
            if (!result)
            {
                break;
            }
        }
    }

    return result;
}

/*! Returns true if the \p remote version is newer than \p current version.
 */
bool remoteVersionIsNewer(const std::array<int, 3> &current, const std::array<int, 3> &remote)
{
    return current[0] <  remote[0] ||
           (current[0] == remote[0] && current[1] <  remote[1]) ||
           (current[0] == remote[0] && current[1] == remote[1] && current[2] < remote[2]);
}

/*! Extracts the update channels version info about the deCONZ/WebApp.

    \param reply which holds the version info in JSON format
 */
void DeRestPluginPrivate::internetDiscoveryExtractVersionInfo(QNetworkReply *reply)
{
    bool ok;
    QByteArray content = reply->readAll();
    QVariant var = Json::parse(content, ok);
    QVariantMap map = var.toMap();

    if (!ok || map.isEmpty())
    {
        DBG_Printf(DBG_ERROR, "discovery couldn't extract version info from reply\n");
    }

#ifdef ARCH_ARM
    if (reply->hasRawHeader("date") || reply->hasRawHeader("Date"))
    {
        // if NTP is not working (UDP blocked, proxies, etc)
        // try to extract time from discover server http header if local time is too off
        QString date = reply->rawHeader("date");
        if (date.isEmpty())
        {
            date = reply->rawHeader("Date");
        }
        DBG_Printf(DBG_INFO, "discovery server date: %s\n", qPrintable(date));

        QStringList ls = date.split(' ');
        if (ls.size() == 6) // RFC 1123: ddd, dd MMM yyyy HH:mm:ss GTM
        {
            int day = ls[1].toInt();
            int month = 0;
            int year = ls[3].toInt();

            const char *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
            for (int i = 0; i < 12; i++)
            {
                if (ls[2].startsWith(months[i]))
                {
                    month = i + 1;
                    break;
                }
            }
            QDateTime dt;
            dt.setTimeSpec(Qt::UTC);
            dt.setDate(QDate(year, month, day));
            dt.setTime(QTime::fromString(ls[4]));

            QDateTime now = QDateTime::currentDateTimeUtc();
            int diff = qAbs(now.secsTo(dt));

            if (diff < 60)
            {
                DBG_Printf(DBG_INFO, "\t local time seems to be ok\n");
            }
            else if (getUptime() > 600 &&
                     gwConfig["ntp"].toString() != QLatin1String("synced"))
            {
                DBG_Printf(DBG_INFO, "\t = %s, diff %d\n", qPrintable(dt.toString()), diff);
                // lazy adjustment of process time
                //time_t t = dt.toSecsSinceEpoch(); // Qt 5.8
                time_t t = dt.toTime_t();
                struct tm tbrokenDown;
                if (localtime_r(&t, &tbrokenDown))
                {
                    struct timeval tv;
                    tv.tv_sec = mktime(&tbrokenDown);
                    tv.tv_usec = 0;
                    if (settimeofday(&tv, NULL) < 0)
                    {
                        DBG_Printf(DBG_ERROR, "settimeofday(): errno %d\n", errno);
                    }
                }
            }
        }
    }
#endif // ARCH_ARM

#ifdef ARCH_ARM
    if (map.contains("versions") && (map["versions"].type() == QVariant::Map))
    {
        const auto versions = map["versions"].toMap();

        if (versions.contains(gwUpdateChannel) && (versions[gwUpdateChannel].type() == QVariant::String))
        {
            const auto version = versions[gwUpdateChannel].toString();

            if (!version.isEmpty())
            {
                std::array<int, 3> current = { };
                std::array<int, 3> remote = { };

                if (versionToIntList(gwUpdateVersion, current) &&
                    versionToIntList(version, remote) &&
                    remoteVersionIsNewer(current, remote))
                {
                    DBG_Printf(DBG_INFO, "discovery found version %s for update channel %s\n", qPrintable(version), qPrintable(gwUpdateChannel));
                    gwUpdateVersion = version;
                    gwSwUpdateState = swUpdateState.readyToInstall;
                }
                else
                {
                    gwSwUpdateState = swUpdateState.noUpdate;
                }
                updateEtag(gwConfigEtag);
            }
            else
            {
                DBG_Printf(DBG_ERROR, "discovery reply doesn't contain valid version info for update channel %s\n", qPrintable(gwUpdateChannel));
            }
        }
        else
        {
            DBG_Printf(DBG_ERROR, "discovery reply doesn't contain version info for update channel %s\n", qPrintable(gwUpdateChannel));
        }
    }
    else
    {
        DBG_Printf(DBG_ERROR, "discovery reply doesn't contain valid version info\n");
    }
#endif // ARCH_ARM

    if (map.contains("interval") && (map["interval"].type() == QVariant::Double))
    {
        const int interval = map["interval"].toInt(&ok);
        if (ok && interval >= 0 && interval != gwAnnounceInterval)
        {
            setInternetDiscoveryInterval(interval);
        }
    }
}

/*! Extracts the geo information.

    \param reply from discovery server
 */
void DeRestPluginPrivate::internetDiscoveryExtractGeo(QNetworkReply *reply)
{
//    for (const QByteArray &hdr : reply->rawHeaderList())
//    {
//        DBG_Printf(DBG_INFO, "hdr: %s: %s\n", qPrintable(hdr), qPrintable(reply->rawHeader(hdr)));
//    }

    if (reply->hasRawHeader("X-AppEngine-CityLatLong"))
    {
        QList<QByteArray> ll = reply->rawHeader("X-AppEngine-CityLatLong").split(',');
        if (ll.size() != 2)
        {
            // no geo information available
            return;
        }

        Sensor *sensor = getSensorNodeForId(daylightSensorId);
        DBG_Assert(sensor != 0);
        if (!sensor)
        {
            return;
        }

        ResourceItem *configured = sensor->item(RConfigConfigured);
        ResourceItem *lat = sensor->item(RConfigLat);
        ResourceItem *lon = sensor->item(RConfigLong);

        DBG_Assert(configured && lat && lon);
        if (!configured || !lat || !lon)
        {
            return;
        }

        if (!configured->toBool() || !configured->lastSet().isValid())
        {
            configured->setValue(true);
            lat->setValue(QString(ll[0]));
            lon->setValue(QString(ll[1]));
            sensor->setNeedSaveDatabase(true);
            queSaveDb(DB_SENSORS, DB_SHORT_SAVE_DELAY);
        }
    }
}

/*! Finished Lookup of http proxy IP address.

    \param host holds the proxy host info
 */
void DeRestPluginPrivate::inetProxyHostLookupDone(const QHostInfo &host)
{
    if (host.error() != QHostInfo::NoError)
    {
        DBG_Printf(DBG_ERROR, "Proxy host lookup failed: %s\n", qPrintable(host.errorString()));
        return;
    }

    foreach (const QHostAddress &address, host.addresses())
    {
        QString addr = address.toString();
        if (address.protocol() == QAbstractSocket::IPv4Protocol &&
            !addr.isEmpty() && gwProxyAddress != address.toString())
        {
            DBG_Printf(DBG_INFO, "Found proxy IP address: %s\n", qPrintable(address.toString()));
            gwProxyAddress = address.toString();
            DBG_Assert(gwProxyPort != 0);
            queSaveDb(DB_CONFIG, DB_SHORT_SAVE_DELAY);
            updateEtag(gwConfigEtag);
            return;
        }
    }
}

/*! Check if a received via field contains a usable proxy.

    \param via holds the proxy host info received by http request header
 */
void DeRestPluginPrivate::inetProxyCheckHttpVia(const QString &via)
{
    if (via.isEmpty())
    {
        return;
    }

    if (gwProxyPort != 0 && !gwProxyAddress.isEmpty() && gwProxyAddress != QLatin1String("none"))
    {
        return; // already configured?
    }

    // https://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html#sec14.45
    // 1.1 proxy.some-domain.com:3128 (squid/2.7.STABLE9)
    DBG_Printf(DBG_INFO, "Test proxy: \t%s\n", qPrintable(via));

    for (QString &entry : via.split(',')) // there might be multiple proxied in the chain
    {
        QStringList ls = entry.split(' ');
        if (ls.size() < 2)
        {
            continue;
        }

        if (!ls[0].contains(QLatin1String("1.1")))
        {
            continue;
        }

        QStringList recvBy = ls[1].split(':');
        if (ls.size() < 1) // at least host must be specified
        {
            continue;
        }

        quint16 port = 8080;
        if (recvBy.size() == 2) // optional
        {
            port = recvBy[1].toUInt();
        }

        DBG_Printf(DBG_INFO, "\t --> %s:%u\n", qPrintable(recvBy[0]), port);

        if (gwProxyPort != 0)
        {
            continue;
        }

        if (gwAnnounceVital >= 0)
        {
            continue;
        }

        gwProxyAddress = recvBy[0];
        gwProxyPort = port;

        if (gwProxyAddress.contains('.'))
        {
            recvBy = gwProxyAddress.split('.');
            gwProxyAddress = recvBy[0]; // strip domain // todo might be too restrictive
        }

        QNetworkProxy proxy(QNetworkProxy::HttpProxy, gwProxyAddress, gwProxyPort);
        inetDiscoveryManager->setProxy(proxy);
        QHostInfo::lookupHost(proxy.hostName(),
                              this, SLOT(inetProxyHostLookupDone(QHostInfo)));
        updateEtag(gwConfigEtag);

        if (gwAnnounceInterval > 0)
        {
            QTimer::singleShot(5000, this, SLOT(internetDiscoveryTimerFired()));
        }
    }
}
