/*
 * Copyright (c) 2016-2017 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QApplication>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkInterface>
#include <QNetworkReply>
#include <vector>
#include "gateway_scanner.h"
#include "deconz.h"
#include "json.h"

enum ScanState
{
    StateInit,
    StateIdle,
    StateRunning,
};

enum ScanEvent
{
    ActionProcess,
    EventTimeout,
    EventGotReply
};

class GatewayScannerPrivate
{
public:
    void initScanner();
    void handleEvent(ScanEvent event);
    void startScanTimer(int msec, ScanEvent action);
    void stopTimer();
    void queryNextIp();
    void processReply();

    GatewayScanner *q;
    ScanState state;
    QNetworkAccessManager *manager;
    QNetworkReply *reply;
    QTimer *timer;
    ScanEvent timerAction;
    std::vector<quint32> interfaces;
    quint32 scanIp;
    quint16 scanPort;
    int scanIteration;
    quint32 host;
    size_t interfaceIter;
};

GatewayScanner::GatewayScanner(QObject *parent) :
    QObject(parent),
    d_ptr(new GatewayScannerPrivate)
{
    Q_D(GatewayScanner);
    d->q = this;
    d->scanIteration = 0;
    d->state = StateInit;
    d->manager = new QNetworkAccessManager(this);
    connect(d->manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(requestFinished(QNetworkReply*)));
    d->timer = new QTimer(this);
    d->timer->setSingleShot(true);
    connect(d->timer, SIGNAL(timeout()), this, SLOT(scanTimerFired()));
}

GatewayScanner::~GatewayScanner()
{
    Q_ASSERT(d_ptr != nullptr);
    if (d_ptr)
    {
        delete d_ptr;
        d_ptr = nullptr;
    }
}

bool GatewayScanner::isRunning() const
{
    Q_D(const GatewayScanner);

    return (d->state != StateInit);
}

void GatewayScanner::queryGateway(const QString &url)
{
    Q_D(GatewayScanner);

    if (!isRunning() && d->reply == nullptr)
    {
        d->reply = d->manager->get(QNetworkRequest(url));
        QObject::connect(d->reply, SIGNAL(error(QNetworkReply::NetworkError)),
                d->manager->parent(), SLOT(onError(QNetworkReply::NetworkError)));
    }
}

void GatewayScanner::startScan()
{
    Q_D(GatewayScanner);

    // TODO fix: GLib-ERROR **: Creating pipes for GWakeup: Too many open files
    if (d->state == StateInit)
    {
        d->startScanTimer(1, ActionProcess);
    }
}

void GatewayScanner::scanTimerFired()
{
    Q_D(GatewayScanner);
    d->handleEvent(d->timerAction);
}

void GatewayScanner::requestFinished(QNetworkReply *reply)
{
    Q_D(GatewayScanner);

    if (reply == d->reply)
    {
        d->processReply();
    }

    if (isRunning())
    {
        d->handleEvent(EventGotReply);
    }
    reply->deleteLater();
}

void GatewayScannerPrivate::processReply()
{
    if (!reply)
    {
        return;
    }

    QNetworkReply *r = reply;
    reply = nullptr;

    int code = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (code != 200) // not authorized or ok
    {
        return;
    }

    bool ok;
    QVariant var = Json::parse(r->readAll(), ok);
    if (!ok)
    {
        return;
    }

    QVariantMap map = var.toMap();
    if (map.isEmpty())
    {
        return;
    }

    if (!map.contains(QLatin1String("bridgeid")) ||
        !map.contains(QLatin1String("modelid")) ||
        !map.contains(QLatin1String("name")))
    {
        return;
    }

    QString name = map["name"].toString();
    //QString modelid = map["modelid"].toString();
    QString bridgeid = map["bridgeid"].toString();

    QUrl url = r->url();

    QHostAddress host(url.host());
    if (host.isNull() || name.isEmpty() || bridgeid.isEmpty())
    {
        return;
    }

    //DBG_Printf(DBG_INFO, "GW: %s %s, %s, %s\n", qPrintable(url.host()), qPrintable(name), qPrintable(modelid), qPrintable(bridgeid));
    q->foundGateway(host, static_cast<quint16>(url.port(80)), bridgeid, name);
}

void GatewayScanner::onError(QNetworkReply::NetworkError code)
{
    Q_D(GatewayScanner);
    Q_UNUSED(code)

    sender()->deleteLater();

    if (!d->timer->isActive())
    {
        // must be in timeout window
        return;
    }

    if (d->reply && sender() == d->reply)
    {
        //DBG_Printf(DBG_INFO, "reply err: %d\n", code);
        d->timer->stop();
        d->handleEvent(EventGotReply);
    }
}

void GatewayScannerPrivate::initScanner()
{
    QList<QNetworkInterface> ifaces = QNetworkInterface::allInterfaces();

    QList<QNetworkInterface>::Iterator ifi = ifaces.begin();
    QList<QNetworkInterface>::Iterator ifend = ifaces.end();

    for (; ifi != ifend; ++ifi)
    {
        QString name = ifi->humanReadableName();

        // filter
        if (name.contains("vm", Qt::CaseInsensitive) ||
            name.contains("virtual", Qt::CaseInsensitive) ||
            name.contains("loop", Qt::CaseInsensitive))
        {
            continue;
        }

        QList<QNetworkAddressEntry> addr = ifi->addressEntries();

        QList<QNetworkAddressEntry>::Iterator i = addr.begin();
        QList<QNetworkAddressEntry>::Iterator end = addr.end();

        for (; i != end; ++i)
        {
            QHostAddress a = i->ip();

            if (a.protocol() == QAbstractSocket::IPv4Protocol)
            {
                quint32 ipv4 = a.toIPv4Address();
                if ((ipv4 & 0xff000000UL) == 0x7f000000UL)
                {
                    // 127.x.x.x
                    continue;
                }

                if (std::find(interfaces.begin(), interfaces.end(), ipv4) == interfaces.end())
                {
                    interfaces.push_back(ipv4);
                }
            }
        }
    }

    scanIteration++;
    interfaceIter = 0;
    host = 0;
}

void GatewayScannerPrivate::handleEvent(ScanEvent event)
{
    if (state == StateInit)
    {
        if (event == ActionProcess)
        {
            initScanner();
            state = StateIdle;
            startScanTimer(10, ActionProcess);
        }
        else
        {
            Q_ASSERT(0);
        }
    }
    else if (state == StateIdle)
    {
        if (event == ActionProcess)
        {
            queryNextIp();
        }
        else if (event == EventTimeout)
        {
            QNetworkReply *r = reply;
            if (reply)
            {
                reply = nullptr;
                if (r->isRunning())
                {
                    r->abort();
                }
                r->deleteLater();
            }
            host++;
            startScanTimer(1000, ActionProcess);
        }
        else if (event == EventGotReply)
        {
            host++;
            startScanTimer(1, ActionProcess);
        }
        else
        {
            Q_ASSERT(0);
        }
    }
    else
    {
        Q_ASSERT(0);
    }
}

void GatewayScannerPrivate::startScanTimer(int msec, ScanEvent action)
{
    timerAction = action;
    timer->stop();
    timer->start(msec);
}

void GatewayScannerPrivate::stopTimer()
{
    timer->stop();
}

void GatewayScannerPrivate::queryNextIp()
{
    if (!interfaces.empty() && host > 255)
    {
        interfaces.pop_back();
        host = 0;
    }

    if (interfaces.empty())
    {
        state = StateInit;
        DBG_Printf(DBG_INFO, "scan finished\n");
        return;
    }

    scanIp = interfaces.back();
    scanPort = 80;

    if (host == (scanIp & 0x000000fful))
    {
        DBG_Printf(DBG_INFO, "scan skip host .%u\n", host);
        host++; // don't scan own ip
    }

    const QString url = QString("http://%1.%2.%3.%4:%5/api/config")
                .arg(((scanIp >> 24) & 0xff))
                .arg(((scanIp >> 16) & 0xff))
                .arg(((scanIp >> 8) & 0xff))
                .arg(host & 0xff).arg(scanPort);

    scanIp &= 0xffffff00ull;
    scanIp |= host & 0xff;

    //DBG_Printf(DBG_INFO, "scan %s\n", qPrintable(url));
    reply = manager->get(QNetworkRequest(url));
    QObject::connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
            manager->parent(), SLOT(onError(QNetworkReply::NetworkError)));

    startScanTimer(1000, EventTimeout);
}
