#include <QBuffer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QTime>
#include <deconz.h>
#include "gateway.h"
#include "json.h"

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
    void checkConfigResponse(const QByteArray &data);
    void checkGroupsResponse(const QByteArray &data);

    Gateway::State state;
    bool pairingEnabled;
    QString apikey;
    QString name;
    QString uuid;
    QHostAddress address;
    quint16 port;
    QTimer *timer;
    GW_Event timerAction;
    QNetworkAccessManager *manager;
    QBuffer *reqBuffer;
    QNetworkReply *reply;
    std::vector<Gateway::Group> groups;
};

Gateway::Gateway(QObject *parent) :
    QObject(parent),
    d_ptr(new GatewayPrivate)
{
    Q_D(Gateway);
    d->state = Gateway::StateOffline;
    d->pairingEnabled = false;
    d->reply = 0;
    d->manager = new QNetworkAccessManager(this);
    connect(d->manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(finished(QNetworkReply*)));
    d->timer = new QTimer(this);
    d->timer->setSingleShot(true);
    d->reqBuffer = new QBuffer(this);
    connect(d->timer, SIGNAL(timeout()), this, SLOT(timerFired()));

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

void Gateway::setApiKey(const QString &apiKey)
{
    Q_D(Gateway);
    if (d->apikey != apiKey)
    {
        d->apikey = apiKey;
    }
}

bool Gateway::pairingEnabled() const
{
    Q_D(const Gateway);
    return d->pairingEnabled;
}

void Gateway::setPairingEnabled(bool pairingEnabled)
{
    Q_D(Gateway);
    if (d->pairingEnabled != pairingEnabled)
    {
        d->pairingEnabled = pairingEnabled;
    }
}

Gateway::State Gateway::state() const
{
    Q_D(const Gateway);
    return d->state;
}

const std::vector<Gateway::Group> &Gateway::groups() const
{
    Q_D(const Gateway);
    return d->groups;
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
    if      (state == Gateway::StateOffline)       { handleEventStateOffline(event); }
    else if (state == Gateway::StateNotAuthorized) { handleEventStateNotAuthorized(event); }
    else if (state == Gateway::StateConnected)     { handleEventStateConnected(event); }
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

        startTimer(2000, EventTimeout);
    }
    else if (event == EventResponse)
    {
        QNetworkReply *r = reply;
        if (reply)
        {
            timer->stop();
            reply = 0;
            int code = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

            r->deleteLater();

            if (code == 403)
            {
                state = Gateway::StateNotAuthorized;
                startTimer(1000, ActionProcess);
            }
            else if (code == 200)
            {
                checkConfigResponse(r->readAll());
                state = Gateway::StateConnected;
                startTimer(500, ActionProcess);
            }
            else
            {
                DBG_Printf(DBG_INFO, "unhandled http status code in offline state %d\n", code);
                startTimer(10000, EventTimeout);
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
    if (event == ActionProcess)
    {
        if (!pairingEnabled)
        {
            startTimer(5000, ActionProcess);
            return;
        }

        // try to create user account
        QString url;
        url.sprintf("http://%s:%u/api/", qPrintable(address.toString()), port);

        QVariantMap map;
        map[QLatin1String("devicetype")] = QLatin1String("x-gw");
        map[QLatin1String("username")] = apikey;

        QString json = deCONZ::jsonStringFromMap(map);

        reqBuffer->close();
        reqBuffer->setData(json.toUtf8());
        reqBuffer->open(QBuffer::ReadOnly);

        QNetworkRequest req(url);
        reply = manager->sendCustomRequest(req, "POST", reqBuffer);

        QObject::connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                manager->parent(), SLOT(error(QNetworkReply::NetworkError)));

        startTimer(5000, EventTimeout);
    }
    else if (event == EventResponse)
    {

        QNetworkReply *r = reply;
        if (reply)
        {
            timer->stop();
            reply = 0;
            int code = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

            {
                QByteArray data = r->readAll();
                DBG_Printf(DBG_INFO, "GW create user reply %d: %s\n", code, qPrintable(data));
            }

            r->deleteLater();

            if (code == 403)
            {
                // retry
                startTimer(10000, ActionProcess);
            }
            else if (code == 200)
            {
                // go to state offline first to query config
                state = Gateway::StateOffline;
                startTimer(100, ActionProcess);
            }
        }
    }
    else if (event == EventTimeout)
    {
        state = Gateway::StateOffline;
        startTimer(5000, ActionProcess);
    }
}

void GatewayPrivate::handleEventStateConnected(GW_Event event)
{
    if (event == ActionProcess)
    {
        QString url;
        url.sprintf("http://%s:%u/api/%s/groups",
                    qPrintable(address.toString()), port, qPrintable(apikey));

        reply = manager->get(QNetworkRequest(url));
        QObject::connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                manager->parent(), SLOT(error(QNetworkReply::NetworkError)));


        startTimer(1000, EventTimeout);
    }
    else if (event == EventResponse)
    {
        QNetworkReply *r = reply;
        if (reply)
        {
            timer->stop();
            reply = 0;
            int code = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();


            if (code == 200)
            {
                //state = Gateway::StateConnected;
                // ok check again later
                checkGroupsResponse(r->readAll());
                startTimer(15000, ActionProcess);
            }
            else
            {
                DBG_Printf(DBG_INFO, "unhandled http status code in connected state %d switch to offline state\n", code);
                state = Gateway::StateOffline;
                startTimer(5000, ActionProcess);
            }

            r->deleteLater();
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
        DBG_Printf(DBG_INFO, "request timeout in connected state switch to offline state\n");
        state = Gateway::StateOffline;
        startTimer(5000, ActionProcess);
    }
}

void GatewayPrivate::checkConfigResponse(const QByteArray &data)
{
    bool ok;
    QVariant var = Json::parse(data, ok);

    if (var.type() != QVariant::Map)
        return;

    QVariantMap map = var.toMap();

    if (!ok || map.isEmpty())
    {
        return;
    }

    if (map.contains(QLatin1String("name")))
    {
        name = map[QLatin1String("name")].toString();
    }
}

void GatewayPrivate::checkGroupsResponse(const QByteArray &data)
{
    bool ok;
    QVariant var = Json::parse(data, ok);

    if (var.type() != QVariant::Map)
        return;

    QVariantMap map = var.toMap();

    if (!ok || map.isEmpty())
    {
        return;
    }

    QStringList groupIds = map.keys();

    QStringList::iterator i = groupIds.begin();
    QStringList::iterator end = groupIds.end();

    if (groups.size() != (size_t)groupIds.size())
    {
        groups.clear();
    }

    for (size_t j = 0; i != end; ++i, j++)
    {

        QVariantMap g = map[*i].toMap();
        QString name = g["name"].toString();

        if (j == groups.size())
        {
            Gateway::Group group;
            group.name = name;
            group.id = *i;
            groups.push_back(group);
            DBG_Printf(DBG_INFO, "\tgroup %s: %s\n", qPrintable(group.id), qPrintable(group.name));
        }
        else if (j < groups.size())
        {
            Gateway::Group &group = groups[j];
            if (group.name != name || group.id != *i)
            {
                // update
                group.name = name;
                group.id = *i;
                DBG_Printf(DBG_INFO, "\tgroup %s: %s\n", qPrintable(group.id), qPrintable(group.name));
            }
        }
    }
}
