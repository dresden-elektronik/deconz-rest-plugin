/*
 * Copyright (c) 2013-2024 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QBuffer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QTime>
#include <deconz.h>
#include "gateway.h"
#include "group.h"
#include "json.h"


#define PHILIPS_MAC_PREFIX QLatin1String("001788")

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
    quint8 mode;
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
    void checkAuthResponse(const QByteArray &data);
    bool hasAuthorizedError(const QVariant &var);

    DeRestPluginPrivate *parent;
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

Gateway::Gateway(DeRestPluginPrivate *parent) :
    QObject(parent),
    d_ptr(new GatewayPrivate)
{
    Q_D(Gateway);
    d->parent = parent;
    d->pings = 0;
    d->port = 0;
    d->state = Gateway::StateOffline;
    d->pairingEnabled = false;
    d->needSaveDatabase = false;
    d->reply = nullptr;
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
    Q_ASSERT(d_ptr);

    if (d_ptr)
    {
        delete d_ptr;
        d_ptr = nullptr;
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

            cmd.transitionTime = 0;

            // filter
            if (ind.clusterId() == SCENE_CLUSTER_ID)
            {
                switch(zclFrame.commandId())
                {
                    case SCENE_COMMAND_RECALL_SCENE:
                        if (zclFrame.payload().size() < 3) // sanity
                        {
                            continue;
                        }
                        // payload U16 group, U8 scene
                        cmd.param.sceneId = zclFrame.payload().at(2);
                        break;
                    case SCENE_COMMAND_IKEA_MOVE_CT:
                        cmd.mode = zclFrame.payload().at(0);
                        cmd.transitionTime = 2540.0 / 83; // value for DimUp/Down
                        break;
                    case SCENE_COMMAND_IKEA_STEP_CT:
                        // payload U8 mode
                        cmd.mode = zclFrame.payload().at(0);
                        cmd.param.level = 43;    // value for DimUp/Down
                        cmd.transitionTime = 5;  // value for DimUp/Down
                        break;
                    case SCENE_COMMAND_IKEA_STOP_CT:
                        break;
                    default:
                        continue;
                }
            }
            else if (ind.clusterId() == ONOFF_CLUSTER_ID) // onoff
            {
                switch(zclFrame.commandId())
                {
                    case ONOFF_COMMAND_OFF:
                        // Set on: false trough REST API
                        break;
                    case ONOFF_COMMAND_ON:
                        // Set on: true trough REST API
                        // Hue dimmer switch On
                        break;
                    case ONOFF_COMMAND_TOGGLE:
                        // IKEA Trådfri Remote On/Off
                        {
                            ::Group *group = d->parent->getGroupForId(cg.local);
                            const ResourceItem *item = group ? group->item(RStateAllOn) : nullptr;
                            if (group && item)
                            {
                                cmd.param.level = item->toBool() ? 0x00 : 0x01;
                                break;
                            }
                        }
                        continue;
                    case ONOFF_COMMAND_OFF_WITH_EFFECT:
                        // Hue dimmer switch Off
                        cmd.transitionTime = 4;
                        break;
                    case ONOFF_COMMAND_ON_WITH_TIMED_OFF:
                        // IKEA Trådfri motion sensor
                        break;
                    default:
                        continue;
                }
            }
            else if (ind.clusterId() == LEVEL_CLUSTER_ID)
            {
                switch (zclFrame.commandId())
                {
                    case LEVEL_COMMAND_MOVE_TO_LEVEL:
                        // Set bri through REST API
                        // payload U8 level, U16 transition time
                        cmd.param.level = zclFrame.payload().at(0);
                        cmd.transitionTime = zclFrame.payload().at(1);
                        break;
                    case LEVEL_COMMAND_MOVE_WITH_ON_OFF:
                        // IKEA Trådfri remote DimUp Hold
                        // fall through
                    case LEVEL_COMMAND_MOVE:
                        // IKEA Trådfri remote DimDown Hold
                        // payload U8 mode, U8 rate
                        cmd.mode = zclFrame.payload().at(0);
                        cmd.param.level = zclFrame.payload().at(1);
                        // DBG_Printf(DBG_INFO_L2, "GW level %u\n", cmd.param.level);
                        cmd.transitionTime = 2540.0 / cmd.param.level;
                        break;
                    case LEVEL_COMMAND_STEP_WITH_ON_OFF:
                        // IKEA Trådfri remote DimUp Short Release
                        // fall through
                    case LEVEL_COMMAND_STEP:
                        // Hue dimmer switch DimUp, DimDown Press, Hold
                        // IKEA Trådfri remote DimDown Short Release
                        // payload U8 mode, U8 step size, U16 transitionTime
                        cmd.mode = zclFrame.payload().at(0);
                        cmd.param.level = zclFrame.payload().at(1);
                        cmd.transitionTime = zclFrame.payload().at(2);
                        break;
                    case LEVEL_COMMAND_STOP_WITH_ON_OFF:
                        // IKEA Trådfri remote DimUp Long Release
                        // fall through
                    case LEVEL_COMMAND_STOP:
                        // Hue dimmer switch DimUp, DimDown Long Release
                        // IKEA Trådfri remote DimDown Long Release
                        break;
                    default:
                        continue;
                }
            }
            else
            {
                continue;
            }

            cmd.clusterId = ind.clusterId();
            cmd.groupId = cg.remote;
            cmd.commandId = zclFrame.commandId();
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
            url = QString("http://%1:%2/api/%3/config").arg(address.toString()).arg(port).arg(apikey);

        }
        else
        {
            url = QString("http://%1:%2/api/config").arg(address.toString()).arg(port);
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
        const QString url = QString("http://%1:%2/api/").arg(address.toString()).arg(port);

        QVariantMap map;
        map[QLatin1String("devicetype")] = QLatin1String("x-gw");
        //map[QLatin1String("username")] = apikey;

        QString json = Json::serialize(map);

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

            r->deleteLater();

            if (code == 403)
            {
                // gateway must be unlocked ...
            }
            else if (code == 200)
            {
                checkAuthResponse(r->readAll());
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

        if (apikey.isEmpty())
        {
            state = Gateway::StateNotAuthorized;
            startTimer(5000, ActionProcess);
            return;
        }

        if (commands.empty())
        {
            const QString url = QString("http://%1:%2/api/%3/groups").arg(address.toString()).arg(port).arg(apikey);

            pings++;
            reply = manager->get(QNetworkRequest(url));
            QObject::connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                    manager->parent(), SLOT(error(QNetworkReply::NetworkError)));
        }
        else
        {
            bool ok = false;
            double level;
            QString url;
            QVariantMap map;
            const Command &cmd = commands.back();

            if (cmd.clusterId == SCENE_CLUSTER_ID)
            {
                switch (cmd.commandId) {
                    case SCENE_COMMAND_RECALL_SCENE:
                        ok = true;
                        if (uuid.startsWith(PHILIPS_MAC_PREFIX))  // cascade gateway is Hue bridge
                        {
                            const QString scene = QString("g%1s%2").arg(cmd.groupId).arg(cmd.param.sceneId);
                            map[QLatin1String("scene")] = scene;
                        }
                        else
                        {
                            url = QString("http://%1:%2/api/%3/groups/%4/scenes/%5/recall")
                                        .arg(address.toString()).arg(port).arg(apikey).arg(cmd.groupId).arg(cmd.param.sceneId);
                        }
                        break;
                    case SCENE_COMMAND_IKEA_STEP_CT:
                        ok = true;
                        level = cmd.param.level * (cmd.mode == 0x00 ? 1 : -1);
                        map[QLatin1String("ct_inc")] = level;
                        break;
                    case SCENE_COMMAND_IKEA_MOVE_CT:
                        ok = true;
                        level = cmd.mode == 0x00 ? 254 : -254;
                        map[QLatin1String("ct_inc")] = level;
                        break;
                    case SCENE_COMMAND_IKEA_STOP_CT:
                        ok = true;
                        level = 0;
                        map[QLatin1String("ct_inc")] = level;
                        break;
                    default:
                        break;
                }
            }
            else if (cmd.clusterId == ONOFF_CLUSTER_ID) // onoff
            {
                switch (cmd.commandId) {
                    case ONOFF_COMMAND_OFF_WITH_EFFECT:
                        // Hue dimmer switch Off
                        // fall through
                    case ONOFF_COMMAND_OFF:
                        // Set on: false through REST API
                        ok = true;
                        map[QLatin1String("on")] = false;
                        break;
                    case ONOFF_COMMAND_ON:
                        // Set on: true through REST API
                        // Hue dimmer switch On
                        ok = true;
                        map[QLatin1String("on")] = true;
                        break;
                    case ONOFF_COMMAND_TOGGLE:
                        ok = true;
                        map[QLatin1String("on")] = cmd.param.level == 0x01;
                        break;
                    default:
                        break;
                }
            }
            else if (cmd.clusterId == LEVEL_CLUSTER_ID)
            {
                switch (cmd.commandId)
                {
                      case LEVEL_COMMAND_MOVE_TO_LEVEL:
                          // Set bri through REST API
                          ok = true;
                          level = cmd.param.level;
                          map[QLatin1String("bri")] = level;
                          break;
                      case LEVEL_COMMAND_MOVE_WITH_ON_OFF:
                          if (cmd.mode == 0x00)
                          {
                              map[QLatin1String("on")] = true;
                          }
                          // fall through
                      case LEVEL_COMMAND_MOVE:
                          // IKEA Trådfri remote DimDown Hold
                          ok = true;
                          level = cmd.mode == 0x00 ? 254 : -254;
                          map[QLatin1String("bri_inc")] = level;
                          break;
                      case LEVEL_COMMAND_STEP_WITH_ON_OFF:
                          // IKEA Trådfri remote DimUp Short Release
                          if (cmd.mode == 0x00)
                          {
                              map[QLatin1String("on")] = true;
                          }
                          // fall through
                      case LEVEL_COMMAND_STEP:
                          // Hue dimmer switch DimUp, DimDown Short Release, Hold
                          // IKEA Trådfri remote DimDown Short Release
                          ok = true;
                          level = cmd.param.level * (cmd.mode == 0x00 ? 1 : -1);
                          map[QLatin1String("bri_inc")] = level;
                          break;
                      case LEVEL_COMMAND_STOP_WITH_ON_OFF:
                          // IKEA Trådfri remote DimUp Long Release
                          // fall through
                      case LEVEL_COMMAND_STOP:
                          // Philips Hue dimmer DimUp, DimDown Long Release
                          // IKEA Trådfri remote DimDown Long Release
                          ok = true;
                          level = 0;
                          map[QLatin1String("bri_inc")] = level;
                          break;
                      default:
                          break;
                }
            }

            commands.pop_back();

            if (!ok)
            {
                startTimer(50, EventTimeout);
                return;
            }

            if (url.isEmpty())
            {
                url = QString("http://%1:%2/api/%3/groups/%4/action")
                            .arg(address.toString()).arg(port).arg(apikey).arg(cmd.groupId);
            }

            QString json;
            if (!map.isEmpty())
            {
                if (cmd.transitionTime != 0)
                {
                    map[QLatin1String("transitiontime")] = (double) cmd.transitionTime;
                }
                json = Json::serialize(map);
                DBG_Printf(DBG_INFO_L2, "GW body %s\n", qPrintable(json));
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
            else if (code == 403)
            {
                state = Gateway::StateNotAuthorized;
                startTimer(5000, ActionProcess);
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

    if (hasAuthorizedError(var))
    {
        return;
    }

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

    if (hasAuthorizedError(var))
    {
        return;
    }

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

void GatewayPrivate::checkAuthResponse(const QByteArray &data)
{
    bool ok;
    QVariant var = Json::parse(data, ok);

    if (hasAuthorizedError(var))
    {
        return;
    }

    if (var.type() != QVariant::List)
    {
        return; // unexpected
    }

    QVariantMap map = var.toList().first().toMap();
    if (!map.contains("success"))
    {
        return;
    }

    map = map["success"].toMap();

    if (map.contains("username"))
    {
        apikey = map["username"].toString();
        needSaveDatabase = true;
        state = Gateway::StateConnected;
    }
}

bool GatewayPrivate::hasAuthorizedError(const QVariant &var)
{
    if (var.type() != QVariant::List)
    {
        return false;
    }

    QVariantList ls = var.toList();
    for (const QVariant &item : ls)
    {
        QVariantMap map = item.toMap();
        if (!map.contains(QLatin1String("error")))
        {
            continue;
        }

        map = map["error"].toMap();

        if (map.contains(QLatin1String("type")) && map["type"].toInt() == ERR_UNAUTHORIZED_USER)
        {
            if (state == Gateway::StateConnected)
            {
                state = Gateway::StateNotAuthorized;
                apikey.clear();
                return true;
            }
        }
    }

    return false;
}
