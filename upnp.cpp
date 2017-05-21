/*
 * Copyright (c) 2016 dresden elektronik ingenieurtechnik gmbh.
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

/*! Inits the UPnP discorvery. */
void DeRestPluginPrivate::initUpnpDiscovery()
{
    DBG_Assert(udpSock == 0);

    QHostAddress groupAddress("239.255.255.250");
    udpSock = new QUdpSocket(this);
    udpSockOut = new QUdpSocket(this);
    if (!udpSock->bind(QHostAddress("0.0.0.0"), 1900, QUdpSocket::ShareAddress)) // SSDP
    {
        DBG_Printf(DBG_ERROR, "UPNP error %s\n", qPrintable(udpSock->errorString()));
    }

    if (!udpSock->joinMulticastGroup(groupAddress))
    {
        DBG_Printf(DBG_ERROR, "UPNP error %s\n", qPrintable(udpSock->errorString()));
    }

    connect(udpSock, SIGNAL(readyRead()),
            this, SLOT(upnpReadyRead()));

    QTimer *timer = new QTimer(this);
    timer->setSingleShot(false);
    connect(timer, SIGNAL(timeout()),
            this, SLOT(announceUpnp()));
    timer->start(20 * 1000);

    // replace description_in.xml template with dynamic content

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
                   line.replace(QString("{{PORT}}"), qPrintable(QString::number(gwPort)));
                   line.replace(QString("{{IPADDRESS}}"), qPrintable(gwIpAddress));
                   line.replace(QString("{{UUID}}"), qPrintable(gwUuid));
                   line.replace(QString("{{GWNAME}}"), qPrintable(gwName));
                   line.replace(QString("{{BRIDGEID}}"), qPrintable(gwBridgeId));
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
    if (gwBridgeId.isEmpty())
    {
      // Don't announce before bridgeid has been set.
      return;
    }
    quint16 port = 1900;
    QHostAddress host;
    QByteArray datagram = QString(QLatin1String(
    "NOTIFY * HTTP/1.1\r\n"
    "HOST: 239.255.255.250:1900\r\n"
    "CACHE-CONTROL: max-age=100\r\n"
    "LOCATION: http://%1:%2/description.xml\r\n"
    "SERVER: FreeRTOS/7.4.2, UPnP/1.0, IpBridge/1.8.0\r\n"
    "NTS: ssdp:alive\r\n"
    "NT: upnp:rootdevice\r\n"
    "USN: uuid:%3::upnp:rootdevice\r\n"
    "GWID.phoscon.de: %4\r\n"
    "\r\n"))
            .arg(gwConfig["ipaddress"].toString())
            .arg(gwConfig["port"].toDouble())
            .arg(gwConfig["uuid"].toString())
            .arg(gwBridgeId).toLocal8Bit();

    host.setAddress(QLatin1String("239.255.255.250"));

    if (udpSockOut->writeDatagram(datagram, host, port) == -1)
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

        if (datagram.startsWith("M-SEARCH *"))
        {
            if (gwBridgeId.isEmpty())
            {
              // Don't respond before bridgeid has been set.
              return;
            }
            DBG_Printf(DBG_HTTP, "UPNP %s:%u\n%s\n", qPrintable(host.toString()), port, datagram.data());
            datagram.clear();

            datagram.append("HTTP/1.1 200 OK\r\n");
            datagram.append("CACHE-CONTROL: max-age=100\r\n");
            datagram.append("EXT:\r\n");
            datagram.append(QString("LOCATION: http://%1:%2/description.xml\r\n")
                            .arg(gwConfig["ipaddress"].toString())
                            .arg(gwConfig["port"].toDouble()).toLocal8Bit());
            datagram.append("SERVER: FreeRTOS/7.4.2, UPnP/1.0, IpBridge/1.8.0\r\n");
            datagram.append("ST: upnp:rootdevice\r\n");
            datagram.append(QString("USN: uuid:%1::upnp:rootdevice\r\n").arg(gwUuid));
            datagram.append(QString("GWID.phoscon.de: %1\r\n").arg(gwBridgeId));
            datagram.append("\r\n");

            if (udpSockOut->writeDatagram(datagram.data(), datagram.size(), host, port) == -1)
            {
                DBG_Printf(DBG_ERROR, "UDP send error %s\n", qPrintable(udpSockOut->errorString()));
            }
        }
    }
}
