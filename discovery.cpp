/*
 * Copyright (C) 2013 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "de_web_plugin_private.h"

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
        internetDiscoveryTimerFired();
    }

    inetDiscoveryTimer->start();
}

/*! Sets the announce interval for internet discovery.

    \param minutes a value between 0..180 min
    \return true on success
 */
bool DeRestPluginPrivate::setInternetDiscoveryInterval(int minutes)
{
    if ((minutes < 0) || (minutes > 180))
    {
        DBG_Printf(DBG_INFO, "ignored invalid announce interval (%d minutes)\n", minutes);
        return false;
    }

    inetDiscoveryTimer->stop();
    gwAnnounceInterval = minutes;

    if (gwAnnounceInterval > 0)
    {
        int msec = 1000 * 60 * gwAnnounceInterval;
        inetDiscoveryTimer->start(msec);
        DBG_Printf(DBG_INFO, "updated announce interval to %d minutes\n", minutes);
    }
    return true;
}

/*! Handle announce trigger timer.
 */
void DeRestPluginPrivate::internetDiscoveryTimerFired()
{
    if (gwAnnounceInterval > 0)
    {
        QString str = QString("{ \"name\": \"%1\", \"mac\": \"%2\", \"internal_ip\":\"%3\", \"internal_port\":%4, \"interval\":%5, \"swversion\":\"%6\" }")
                .arg(gwName)
                .arg(gwConfig["mac"].toString())
                .arg(gwConfig["ipaddress"].toString())
                .arg(gwConfig["port"].toDouble())
                .arg(gwAnnounceInterval)
                .arg(gwConfig["swversion"].toString());
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
        //QNetworkRequest req = reply->request();
        QByteArray version = reply->rawHeader("deCONZ-Latest-RaspBee");

        if (!version.isEmpty())
        {
            gwUpdateVersion = version;
            DBG_Printf(DBG_INFO, "Latest RaspBee version %s\n", qPrintable(version));
        }
#endif // ARCH_ARM
    }
    else
    {
        DBG_Printf(DBG_INFO, "Network reply error: %s\n", qPrintable(reply->errorString()));
    }

    reply->deleteLater();
}
