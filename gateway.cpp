#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QTime>
#include <deconz.h>
#include "gateway.h"

enum GW_State
{
    GW_StateOffline,
    GW_StateNotAuthorized,
    GW_StateConnected
};

enum GW_Event
{
    ActionProcess,
    EventTimeout,
    EventResponse
};

class GatewayPrivate
{
public:
    void startTimer(int msec, GW_Event event);
    void handleEvent(GW_Event event);
    void handleEventStateOffline(GW_Event event);
    void handleEventStateNotAuthorized(GW_Event event);
    void handleEventStateConnected(GW_Event event);

    GW_State state;
    QString apikey;
    QString name;
    QString uuid;
    QHostAddress address;
    quint16 port;
    QTimer *timer;
    GW_Event timerAction;
    QNetworkAccessManager *manager;
    QNetworkReply *reply;
    QTime tLastReply;
};

Gateway::Gateway(QObject *parent) :
    QObject(parent),
    d_ptr(new GatewayPrivate)
{
    Q_D(Gateway);
    d->state = GW_StateOffline;
    d->reply = 0;
    d->manager = new QNetworkAccessManager(this);
    d->timer = new QTimer(this);
    d->timer->setSingleShot(true);
    connect(d->timer, SIGNAL(timeout()), this, SLOT(timerFired()));

    d->apikey = "adasd90892msd";

    d->startTimer(5000, ActionProcess);
}

Gateway::~Gateway()
{
    Q_ASSERT(d_ptr != 0);

    if (d_ptr)
    {
        delete d_ptr;
        d_ptr = 0;
    }
}

void Gateway::setAddress(const QHostAddress &address)
{
    Q_D(Gateway);
    if (d->address != address)
    {
        d->address = address;
    }
}

const QString &Gateway::name() const
{
    Q_D(const Gateway);
    return d->name;
}

void Gateway::setName(const QString &name)
{
    Q_D(Gateway);
    if (d->name != name)
    {
        d->name = name;
    }
}

const QString &Gateway::uuid() const
{
    Q_D(const Gateway);
    return d->uuid;
}

void Gateway::setUuid(const QString &uuid)
{
    Q_D(Gateway);
    if (d->uuid != uuid)
    {
        d->uuid = uuid;
    }
}

const QHostAddress &Gateway::address() const
{
    Q_D(const Gateway);
    return d->address;
}

quint16 Gateway::port() const
{
    Q_D(const Gateway);
    return d->port;
}

void Gateway::setPort(quint16 port)
{
    Q_D(Gateway);
    if (d->port != port)
    {
        d->port = port;
    }
}

void Gateway::timerFired()
{
    Q_D(Gateway);
    d->handleEvent(d->timerAction);
}

void Gateway::finished(QNetworkReply *reply)
{
    Q_D(Gateway);
    if (d->reply == reply)
    {
        d->handleEvent(EventResponse);
    }
}

void Gateway::error(QNetworkReply::NetworkError)
{
    Q_D(Gateway);
    if (d->reply && sender() == d->reply)
    {
        d->handleEvent(EventResponse);
    }
}


void GatewayPrivate::startTimer(int msec, GW_Event event)
{
    timerAction = event;
    timer->start(msec);
}

void GatewayPrivate::handleEvent(GW_Event event)
{
    if      (state == GW_StateOffline)       { handleEventStateOffline(event); }
    else if (state == GW_StateNotAuthorized) { handleEventStateNotAuthorized(event); }
    else if (state == GW_StateConnected)     { handleEventStateConnected(event); }
    else
    {
        Q_ASSERT(0);
    }
}

void GatewayPrivate::handleEventStateOffline(GW_Event event)
{
    if (event == ActionProcess)
    {
        if (port == 0 || address.isNull())
        {
            // need parameters
            startTimer(1000, ActionProcess);
            return;
        }

        QString url;
        url.sprintf("http://%s:%u/api/%s/config",
                    qPrintable(address.toString()), port, qPrintable(apikey));

        reply = manager->get(QNetworkRequest(url));
        QObject::connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                manager->parent(), SLOT(error(QNetworkReply::NetworkError)));


        startTimer(100, EventTimeout);
    }
    else if (event == EventResponse)
    {
        timer->stop();
        tLastReply.start();

        QNetworkReply *r = reply;
        if (reply)
        {
            reply = 0;
            int code = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

            if (code == 403 || code == 200) // not authorized or ok
            {
                QNetworkRequest req = r->request();
                QByteArray data = r->readAll();
                DBG_Printf(DBG_INFO, "GW reply code %d from %s\n%s\n", code, qPrintable(req.url().toString()), qPrintable(data));
            }
            r->deleteLater();

            if (code == 403)
            {
                state = GW_StateNotAuthorized;
                startTimer(10, ActionProcess);
            }
            else if (code == 200)
            {
                state = GW_StateConnected;
                startTimer(10, ActionProcess);
            }
        }
    }
    else if (event == EventTimeout)
    {
        if (reply)
        {
            QNetworkReply *r = reply;
            reply = 0;
            if (r->isRunning())
            {
                r->abort();
            }
            r->deleteLater();
        }
        startTimer(10000, ActionProcess);
    }
}

void GatewayPrivate::handleEventStateNotAuthorized(GW_Event event)
{

}

void GatewayPrivate::handleEventStateConnected(GW_Event event)
{

}
