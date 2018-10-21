/*
 * Copyright (c) 2016-2017 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QString>
#include <QTcpSocket>
#include <QTimer>
#include <QFile>
#include <QUdpSocket>
#include <QVariantMap>
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "gateway_scanner.h"

/*! Inits the UPnP discorvery. */
void DeRestPluginPrivate::initUpnpDiscovery()
{
    DBG_Assert(udpSock == 0);

    initDescriptionXml();

    if (deCONZ::appArgumentNumeric("--upnp", 1) == 0)
    {
        udpSock = 0;
        udpSockOut = 0;
        joinedMulticastGroup = false;
        return;
    }

    udpSock = new QUdpSocket(this);
    udpSockOut = new QUdpSocket(this);
    joinedMulticastGroup = false;

    connect(udpSock, SIGNAL(readyRead()),
            this, SLOT(upnpReadyRead()));

    upnpTimer = new QTimer(this);
    upnpTimer->setSingleShot(false);
    connect(upnpTimer, SIGNAL(timeout()),
            this, SLOT(announceUpnp()));
    upnpTimer->start(1000); // setup phase fast interval
}

/*! Replaces description_in.xml template with dynamic content. */
void DeRestPluginPrivate::initDescriptionXml()
{
    deCONZ::ApsController *apsCtrl = deCONZ::ApsController::instance();
    QString serverRoot = apsCtrl->getParameter(deCONZ::ParamHttpRoot);

    if (!serverRoot.isEmpty())
    {
        descriptionXml.clear();
        QFile f(serverRoot + "/description_in.xml");

        if (f.open(QFile::ReadOnly))
        {
            QByteArray line;
            do {
               line = f.readLine(320);
               if (!line.isEmpty())
               {
                   line.replace(QString("{{IPADDRESS}}"), qPrintable(gwIPAddress));
                   line.replace(QString("{{PORT}}"), qPrintable(QString::number(gwPort)));
                   line.replace(QString("{{GWNAME}}"), qPrintable(gwName));
                   line.replace(QString("{{SERIAL}}"), qPrintable(gwBridgeId.left(6) + gwBridgeId.right(6)));
                   line.replace(QString("{{UUID}}"), qPrintable(gwUuid));
                   descriptionXml.append(line);

                   DBG_Printf(DBG_INFO_L2, "%s", line.constData());
               }
             } while (!line.isEmpty());
        }
    }
}

/*! Sends SSDP broadcast for announcement. */
void DeRestPluginPrivate::announceUpnp()
{
    if (udpSock->state() != QAbstractSocket::BoundState)
    {
        joinedMulticastGroup = false;
        DBG_Printf(DBG_ERROR, "UPNP socket not bound, state: %d\n", udpSock->state());
        // retry
        if (!udpSock->bind(QHostAddress("0.0.0.0"), 1900, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) // SSDP
        {
            DBG_Printf(DBG_ERROR, "UPNP error %s\n", qPrintable(udpSock->errorString()));
        }
        return;
    }

    if (!joinedMulticastGroup)
    {
        QHostAddress groupAddress("239.255.255.250");
        if (!udpSock->joinMulticastGroup(groupAddress))
        {
            DBG_Printf(DBG_ERROR, "UPNP error %s\n", qPrintable(udpSock->errorString()));
            return;
        }
        joinedMulticastGroup = true;
    }

    if (upnpTimer->interval() != (20 * 1000))
        upnpTimer->start(20 * 1000);

    QByteArray datagram = QString(QLatin1String(
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "CACHE-CONTROL: max-age=100\r\n"
        "LOCATION: http://%1:%2/description.xml\r\n"
        "SERVER: Linux/3.14.0 UPnP/1.0 IpBridge/1.26.0\r\n"
        "hue-bridgeid: %3\r\n"
        "GWID.phoscon.de: %3\r\n"
        "NTS: ssdp:alive\r\n"))
            .arg(gwConfig["ipaddress"].toString())
            .arg(gwConfig["port"].toDouble())
            .arg(gwBridgeId.toUpper()).toLocal8Bit();
    QByteArray nt1 = QString(QLatin1String(
        "NT: upnp:rootdevice\r\n"
        "USN: uuid:%1::upnp:rootdevice\r\n")).arg(gwConfig["uuid"].toString()).toLocal8Bit();
    QByteArray nt2 = QString(QLatin1String(
        "NT: uuid:%1\r\n"
        "USN: uuid:%1\r\n")).arg(gwConfig["uuid"].toString()).toLocal8Bit();
    QByteArray nt3 = QString(QLatin1String(
        "NT: urn:schemas-upnp-org:device:basic:1\r\n"
        "USN: uuid:%1::upnp:rootdevice\r\n")).arg(gwConfig["uuid"].toString()).toLocal8Bit();

    quint16 port = 1900;
    QHostAddress host;
    host.setAddress(QLatin1String("239.255.255.250"));

    if (udpSockOut->writeDatagram(datagram + nt1 + "\r\n", host, port) == -1)
    {
        DBG_Printf(DBG_ERROR, "UDP send error %s\n", qPrintable(udpSockOut->errorString()));
    }
    if (udpSockOut->writeDatagram(datagram + nt2 + "\r\n", host, port) == -1)
    {
        DBG_Printf(DBG_ERROR, "UDP send error %s\n", qPrintable(udpSockOut->errorString()));
    }
    if (udpSockOut->writeDatagram(datagram + nt3 + "\r\n", host, port) == -1)
    {
        DBG_Printf(DBG_ERROR, "UDP send error %s\n", qPrintable(udpSockOut->errorString()));
    }
}

/*! Handles SSDP packets. */
void DeRestPluginPrivate::upnpReadyRead()
{
    while (udpSock->hasPendingDatagrams())
    {
        quint16 port;
        QHostAddress host;
        QByteArray datagram;
        datagram.resize(udpSock->pendingDatagramSize());
        udpSock->readDatagram(datagram.data(), datagram.size(), &host, &port);

        QTextStream stream(datagram);
        QString searchTarget;
        QString location;

        if (DBG_IsEnabled(DBG_HTTP))
        {
            DBG_Printf(DBG_HTTP, "%s\n", qPrintable(datagram));
        }

        while (!stream.atEnd())
        {
            QString line = stream.readLine();

            if (!searchTarget.isEmpty())
            {
                break;
            }

            if (line.startsWith(QLatin1String("LOCATION:")))
            {
                location = line;
            }
            else if (line.startsWith(QLatin1String("GWID.phoscon.de")))
            {
                searchTarget = line;
            }
            else if (line.startsWith(QLatin1String("hue-bridgeid")))
            {
                searchTarget = line;
            }
            else if (line.startsWith(QLatin1String("ST:")))
            {
                if (line.contains(QLatin1String("ssdp:all")) ||
                    line.contains(QLatin1String("device:basic")) ||
                    line.contains(QLatin1String("upnp:rootdevice")))
                {
                    searchTarget = line;
                }
            }
        }

        if (searchTarget.isEmpty())
        {}
        else if (datagram.startsWith("M-SEARCH *"))
        {
            QByteArray response = QString(QLatin1String(
                "HTTP/1.1 200 OK\r\n"
                "CACHE-CONTROL: max-age=100\r\n"
                "EXT:\r\n"
                "LOCATION: http://%1:%2/description.xml\r\n"
                "SERVER: Linux/3.14.0 UPnP/1.0 IpBridge/1.26.0\r\n"
                "hue-bridgeid: %3\r\n"
                "GWID.phoscon.de: %3\r\n"))
                    .arg(gwConfig["ipaddress"].toString())
                    .arg(gwConfig["port"].toDouble())
                    .arg(gwBridgeId.toUpper()).toLocal8Bit();
            QByteArray st1 = QString(QLatin1String(
                "ST: upnp:rootdevice\r\n"
                "USN: uuid:%1::upnp:rootdevice\r\n")).arg(gwConfig["uuid"].toString()).toLocal8Bit();
            QByteArray st2 = QString(QLatin1String(
                "ST: uuid:%1\r\n"
                "USN: uuid:%1\r\n")).arg(gwConfig["uuid"].toString()).toLocal8Bit();
            QByteArray st3 = QString(QLatin1String(
                "ST: urn:schemas-upnp-org:device:basic:1\r\n"
                "USN: uuid:%1::upnp:rootdevice\r\n")).arg(gwConfig["uuid"].toString()).toLocal8Bit();

            DBG_Printf(DBG_HTTP, "UPNP %s:%u\n%s\n", qPrintable(host.toString()), port, datagram.data());
            datagram.clear();

            if (searchTarget == "ST: ssdp:all\r\n" || searchTarget == "ST: upnp:rootdevice\r\n")
            {
                datagram.append(response);
                datagram.append(st1);
                datagram.append("\r\n");
                if (udpSockOut->writeDatagram(datagram.data(), datagram.size(), host, port) == -1)
                {
                    DBG_Printf(DBG_ERROR, "UDP send error %s\n", qPrintable(udpSockOut->errorString()));
                }
                datagram.clear();
            }
            if (searchTarget == "ST: ssdp:all\r\n" || searchTarget == QString(QLatin1String("ST: uuid:%1\r\n")).arg(gwConfig["uuid"].toString()))
            {
                datagram.append(response);
                datagram.append(st2);
                datagram.append("\r\n");
                if (udpSockOut->writeDatagram(datagram.data(), datagram.size(), host, port) == -1)
                {
                    DBG_Printf(DBG_ERROR, "UDP send error %s\n", qPrintable(udpSockOut->errorString()));
                }
                datagram.clear();
            }
            if (searchTarget == "ST ssdp:all\r\n" || searchTarget == "ST: urn:schemas-upnp-org:device:basic:1\r\n")
            {
                datagram.append(response);
                datagram.append(st3);
                datagram.append("\r\n");
                if (udpSockOut->writeDatagram(datagram.data(), datagram.size(), host, port) == -1)
                {
                    DBG_Printf(DBG_ERROR, "UDP send error %s\n", qPrintable(udpSockOut->errorString()));
                }
                datagram.clear();
            }
        }
        else if (datagram.startsWith("NOTIFY *") && !location.isEmpty())
        {
            // phoscon gateway or hue bridge
            QStringList ls = searchTarget.split(' ');
            if (ls.size() != 2)
            {
                continue;
            }

            if (ls[1] == gwBridgeId)
            {
                continue; // self
            }

            ls = location.split(' ');
            if (ls.size() != 2 || !ls[1].startsWith(QLatin1String("http://")))
            {
                continue;
            }

            // http://192.168.14.103:80/description.xml

            QUrl url(ls[1]);
            location = QString("http://%1:%2/api/config").arg(url.host()).arg(url.port(80));
            gwScanner->queryGateway(location);
        }
    }
}
