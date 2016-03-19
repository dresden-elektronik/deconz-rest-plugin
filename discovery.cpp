/*
 * Copyright (c) 2016 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "de_web_plugin_private.h"
#include "json.h"
#ifdef Q_OS_LINUX
#include <unistd.h>
#endif

/*! Inits the internet discovery manager.
 */
void DeRestPluginPrivate::initInternetDicovery()
{
    inetDiscoveryManager = new QNetworkAccessManager(this);
    connect(inetDiscoveryManager, SIGNAL(finished(QNetworkReply*)),
            this, SLOT(internetDiscoveryFinishedRequest(QNetworkReply*)));


    DBG_Assert(gwAnnounceInterval >= 0);
    if (gwAnnounceInterval < 0)
    {
        gwAnnounceInterval = 15;
    }

    inetDiscoveryTimer = new QTimer(this);

    connect(inetDiscoveryTimer, SIGNAL(timeout()),
            this, SLOT(internetDiscoveryTimerFired()));

    setInternetDiscoveryInterval(gwAnnounceInterval);

    // force first run
    if (gwAnnounceInterval > 0)
    {
        QTimer::singleShot(5000, this, SLOT(internetDiscoveryTimerFired()));
    }

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
    gwAnnounceInterval = minutes;

    if (gwAnnounceInterval > 0)
    {
        int msec = 1000 * 60 * gwAnnounceInterval;
        inetDiscoveryTimer->start(msec);
        DBG_Printf(DBG_INFO, "discovery updated announce interval to %d minutes\n", minutes);
    }
    return true;
}

/*! Handle announce trigger timer.
 */
void DeRestPluginPrivate::internetDiscoveryTimerFired()
{
    if (gwAnnounceInterval > 0)
    {
        QString str = QString("{ \"name\": \"%1\", \"mac\": \"%2\", \"internal_ip\":\"%3\", \"internal_port\":%4, \"interval\":%5, \"swversion\":\"%6\", \"fwversion\":\"%7\", \"nodecount\":%8, \"uptime\":%9, \"updatechannel\":\"%10\" }")
                .arg(gwName)
                .arg(gwConfig["mac"].toString())
                .arg(gwConfig["ipaddress"].toString())
                .arg(gwConfig["port"].toDouble())
                .arg(gwAnnounceInterval)
                .arg(gwConfig["swversion"].toString())
                .arg(gwConfig["fwversion"].toString())
                .arg(nodes.size())
                .arg(getUptime())
                .arg(gwUpdateChannel);
        QByteArray data(qPrintable(str));
        inetDiscoveryManager->put(QNetworkRequest(QUrl(gwAnnounceUrl)), data);
    }
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
        DBG_Printf(DBG_INFO, "Announced to internet\n");
#ifdef ARCH_ARM
        // currently this is only supported for the RaspBee Gateway
        internetDiscoveryExtractVersionInfo(reply);
#endif // ARCH_ARM
    }
    else
    {
        DBG_Printf(DBG_INFO, "discovery network reply error: %s\n", qPrintable(reply->errorString()));
    }

    reply->deleteLater();
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

    if (map.contains("versions") && (map["versions"].type() == QVariant::Map))
    {
        QString version;
        QVariantMap versions = map["versions"].toMap();

        if (versions.contains(gwUpdateChannel) && (versions[gwUpdateChannel].type() == QVariant::String))
        {
            version = versions[gwUpdateChannel].toString();

            if (!version.isEmpty())
            {
                if (gwUpdateVersion != version)
                {
                    DBG_Printf(DBG_INFO, "discovery found version %s for update channel %s\n", qPrintable(version), qPrintable(gwUpdateChannel));
                    gwUpdateVersion = version;
                    updateEtag(gwConfigEtag);
                }
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
}
