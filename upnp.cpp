/*
 * Copyright (C) 2013 dresden elektronik ingenieurtechnik gmbh.
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
#include <QHttpRequestHeader>
#include <QVariantMap>
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"

void DeRestPluginPrivate::initUpnpDiscovery()
{
    DBG_Assert(udpSock == 0);

    QHostAddress groupAddress("239.255.255.250");
    udpSock = new QUdpSocket(this);
    udpSockOut = new QUdpSocket(this);
    if (!udpSock->bind(1900, QUdpSocket::ShareAddress)) // SSDP
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
    QString serverRoot = deCONZ::appArgumentString("--http-root", "");

    if (serverRoot != "")
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
                   line.replace(QString("{{PORT}}"), QString::number(gwPort).toAscii().constData());
                   line.replace(QString("{{IPADDRESS}}"), gwIpAddress.toAscii().constData());
                   line.replace(QString("{{UUID}}"), gwUuid.toAscii().constData());
                   descriptionXml.append(line);

                   DBG_Printf(DBG_INFO, "%s", line.constData());
               }
             } while (!line.isEmpty());
        }
    }
}

void DeRestPluginPrivate::announceUpnp()
{
    quint16 port = 1900;
    QHostAddress host;
    QByteArray datagram = QString(
    "NOTIFY * HTTP/1.1\r\n"
    "HOST: 239.255.255.250:1900"
    "CACHE-CONTROL: max-age=100\r\n"
    "LOCATION: http://%1:%2/description.xml\r\n"
    "SERVER: FreeRTOS/6.0.5, UPnP/1.0, IpBridge/0.1\r\n"
    "NTS: ssdp:alive\r\n"
    "NT: upnp:rootdevice\r\n"
    "USN: uuid:2f402f80-da50-11e1-9b23-nydalenlys::upnp:rootdevice\r\n")
            .arg(gwConfig["ipaddress"].toString())
            .arg(gwConfig["port"].toDouble()).toLocal8Bit();

    host.setAddress("239.255.255.250");

    if (udpSockOut->writeDatagram(datagram, host, port) == -1)
    {
        DBG_Printf(DBG_ERROR, "UDP send error %s\n", qPrintable(udpSockOut->errorString()));
    }
}

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
            datagram.clear();

            datagram.append("HTTP/1.1 200 OK\n");
            datagram.append("CACHE-CONTROL: max-age=100\n");
            datagram.append("EXT:\n");
            datagram.append(QString("LOCATION: http://%1:%2/description.xml\n")
                            .arg(gwConfig["ipaddress"].toString())
                            .arg(gwConfig["port"].toDouble()).toLocal8Bit());
            datagram.append("SERVER: FreeRTOS/6.0.5, UPnP/1.0, IpBridge/0.1\n");
            datagram.append("ST: upnp:rootdevice\n");
            datagram.append("USN: uuid:2fa00080-d000-11e1-9b23-001f80007bbe::upnp:rootdevice\n");

            if (udpSockOut->writeDatagram(datagram.data(), datagram.size(), host, port) == -1)
            {
                DBG_Printf(DBG_ERROR, "UDP send error %s\n", qPrintable(udpSockOut->errorString()));
            }

            //DBG_Printf(DBG_INFO, "UPNP %s:%u\n", qPrintable(host.toString()), port);
            //qDebug() << datagram;
        }
    }
}
