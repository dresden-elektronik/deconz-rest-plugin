#include <QBuffer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QTime>
#include <deconz.h>
#include "gateway.h"
#include "json.h"


#define ONOFF_COMMAND_OFF     0x00
#define ONOFF_COMMAND_ON      0x01
#define ONOFF_COMMAND_TOGGLE  0x02
#define ONOFF_COMMAND_OFF_WITH_EFFECT  0x040
#define ONOFF_COMMAND_ON_WITH_TIMED_OFF  0x42

enum GW_Event
{
    ActionProcess,
    EventTimeout,
    EventResponse,
    EventCommandAdded
};

class Command
{
public:
    quint16 groupId;
    quint16 clusterId;
    quint8 commandId;

    union {
        quint8 sceneId;
        quint8 level;
    } param;
    quint16 transitionTime;
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
    bool needSaveDatabase;
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
    int pings;
    std::vector<Gateway::Group> groups;
    std::vector<Gateway::CascadeGroup> cascadeGroups;
    std::vector<Command> commands;
};

Gateway::Gateway(QObject *parent) :
    QObject(parent),
    d_ptr(new GatewayPrivate)
{
    Q_D(Gateway);
    d->pings = 0;
    d->state = Gateway::StateOffline;
    d->pairingEnabled = false;
    d->needSaveDatabase = false;
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
        d->needSaveDatabase = true;
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
        d->needSaveDatabase = true;
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
        d->needSaveDatabase = true;
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
        d->needSaveDatabase = true;
    }
}

void Gateway::setApiKey(const QString &apiKey)
{
    Q_D(Gateway);
    if (d->apikey != apiKey)
    {
        d->apikey = apiKey;
        d->needSaveDatabase = true;
    }
}

const QString &Gateway::apiKey() const
{
    Q_D(const Gateway);
    return d->apikey;
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
        d->needSaveDatabase = true;
    }
}

Gateway::State Gateway::state() const
{
    Q_D(const Gateway);
    return d->state;
}

bool Gateway::needSaveDatabase() const
{
    Q_D(const Gateway);
    return d->needSaveDatabase;
}

void Gateway::setNeedSaveDatabase(bool save)
{
    Q_D(Gateway);
    d->needSaveDatabase = save;
}

void Gateway::addCascadeGroup(quint16 local, quint16 remote)
{
    Q_D(Gateway);
    for (size_t i = 0; i < d->cascadeGroups.size(); i++)
    {
        if (d->cascadeGroups[i].local == local && d->cascadeGroups[i].remote == remote)
        {
            // already known
            return;
        }
    }

    CascadeGroup cg;
    cg.local = local;
    cg.remote = remote;
    d->cascadeGroups.push_back(cg);
    d->needSaveDatabase = true;
}

void Gateway::removeCascadeGroup(quint16 local, quint16 remote)
{
    Q_D(Gateway);
    for (size_t i = 0; i < d->cascadeGroups.size(); i++)
    {
        if (d->cascadeGroups[i].local == local && d->cascadeGroups[i].remote == remote)
        {
            d->cascadeGroups[i].local = d->cascadeGroups.back().local;
            d->cascadeGroups[i].remote = d->cascadeGroups.back().remote;
            d->cascadeGroups.pop_back();
            d->needSaveDatabase = true;
            return;
        }
    }
}

void Gateway::handleGroupCommand(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    Q_D(Gateway);
    if (d->state != StateConnected)
    {
        return;
    }

    if (ind.dstAddressMode() != deCONZ::ApsGroupAddress)
    {
        return;
    }

    for (size_t j = 0; j < d->cascadeGroups.size(); j++)
    {
        const CascadeGroup &cg = d->cascadeGroups[j];
        if (cg.local == ind.dstAddress().group())
        {
            Command cmd;

            // filter
            if (ind.clusterId() == 0x0005 && zclFrame.commandId() == 0x05) // recall scene
            {
                if (zclFrame.payload().size() < 3) // sanity
                    continue;

                // payload U16 group, U8 scene
                cmd.param.sceneId = zclFrame.payload().at(2);
            }
            else if (ind.clusterId() == 0x0006) // onoff
            {
            }
//            else if (ind.clusterId() == 0x0008) // level
//            {
//            }
            else
            {
                continue;
            }

            cmd.clusterId = ind.clusterId();
            cmd.groupId = cg.remote;
            cmd.commandId = zclFrame.commandId();
            cmd.transitionTime = 0;
            d->commands.push_back(cmd);
            d->handleEvent(EventCommandAdded);

            DBG_Printf(DBG_INFO, "GW %s forward command 0x%02X on cluster 0x%04X on group 0x%04X to remote group 0x%04X\n", qPrintable(d->name), zclFrame.commandId(), ind.clusterId(), cg.local, cg.remote);
        }
    }
}

const std::vector<Gateway::Group> &Gateway::groups() const
{
    Q_D(const Gateway);
    return d->groups;
}

const std::vector<Gateway::CascadeGroup> &Gateway::cascadeGroups() const
{
    Q_D(const Gateway);
    return d->cascadeGroups;
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

        pings = 0;

        QString url;
        if (!apikey.isEmpty())
        {
            url.sprintf("http://%s:%u/api/%s/config",
                        qPrintable(address.toString()), port, qPrintable(apikey));

        }
        else
        {
            url.sprintf("http://%s:%u/api/config", qPrintable(address.toString()), port);
        }

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
                if (!apikey.isEmpty())
                {
                    apikey.clear();
                    needSaveDatabase = true;
                }
                startTimer(5000, ActionProcess);
            }
            else if (code == 200)
            {
                checkConfigResponse(r->readAll());
                state = Gateway::StateConnected;
                startTimer(5000, ActionProcess);
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

        pings = 0;

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

//            {
//                QByteArray data = r->readAll();
//                DBG_Printf(DBG_INFO, "GW create user reply %d: %s\n", code, qPrintable(data));
//            }

            r->deleteLater();

            if (code == 403)
            {
                // gateway must be unlocked ...
            }
            else if (code == 200)
            {
                // go to state offline first to query config
                state = Gateway::StateOffline;
                startTimer(100, ActionProcess);
            }

            // retry
            if (!timer->isActive())
            {
                startTimer(10000, ActionProcess);
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
        Q_ASSERT(reply == 0);

        if (commands.empty())
        {
            QString url;
            url.sprintf("http://%s:%u/api/%s/groups",
                        qPrintable(address.toString()), port, qPrintable(apikey));

            pings++;
            reply = manager->get(QNetworkRequest(url));
            QObject::connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                    manager->parent(), SLOT(error(QNetworkReply::NetworkError)));
        }
        else
        {
            QString url;
            QVariantMap map;
            const Command &cmd = commands.back();

            if (cmd.clusterId == 0x0005 && cmd.commandId == 0x05) // recall scene
            {
                url.sprintf("http://%s:%u/api/%s/groups/%u/scenes/%u/recall",
                            qPrintable(address.toString()), port, qPrintable(apikey), cmd.groupId, cmd.param.sceneId);
            }
            else if (cmd.clusterId == 0x0006)
            {
                if (cmd.commandId == ONOFF_COMMAND_ON)
                {
                    url.sprintf("http://%s:%u/api/%s/groups/%u/action",
                                qPrintable(address.toString()), port, qPrintable(apikey), cmd.groupId);
                    map[QLatin1String("on")] = true;
                }
                else if (cmd.commandId == ONOFF_COMMAND_OFF || cmd.commandId == ONOFF_COMMAND_OFF_WITH_EFFECT)
                {
                    url.sprintf("http://%s:%u/api/%s/groups/%u/action",
                                qPrintable(address.toString()), port, qPrintable(apikey), cmd.groupId);
                    map[QLatin1String("on")] = false;
                }
            }

            commands.pop_back();

            if (url.isEmpty())
            {
                startTimer(50, EventTimeout);
                return;
            }

            QString json;
            if (!map.isEmpty())
            {
                json = deCONZ::jsonStringFromMap(map);
            }
            else
            {
                json = QLatin1String("{}");
            }
            reqBuffer->close();
            reqBuffer->setData(json.toUtf8());
            reqBuffer->open(QBuffer::ReadOnly);

            QNetworkRequest req(url);
            reply = manager->put(req, reqBuffer);

            QObject::connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                    manager->parent(), SLOT(error(QNetworkReply::NetworkError)));
        }

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
                // ok check again later
                if (r->url().toString().endsWith(QLatin1String("/groups")))
                {
                    pings = 0;
                    checkGroupsResponse(r->readAll());
                }
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
        if (pings > 5)
        {
            DBG_Printf(DBG_INFO, "max request timeout in connected state switch to offline state\n");
            state = Gateway::StateOffline;
        }
        startTimer(5000, ActionProcess);
    }
    else if (event == EventCommandAdded)
    {
        if (!reply) // not busy
        {
            startTimer(50, ActionProcess);
        }
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
