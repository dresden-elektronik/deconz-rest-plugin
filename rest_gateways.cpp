/*
 * Copyright (c) 2016 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QApplication>
#include <QDesktopServices>
#include <QFile>
#include <QString>
#include <QVariantMap>
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "gateway.h"
#include "gateway_scanner.h"
#include "json.h"
#include <stdlib.h>

/*! Gateways REST API broker.
    \param req - request data
    \param rsp - response data
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::handleGatewaysApi(const ApiRequest &req, ApiResponse &rsp)
{
    if (req.path[2] != QLatin1String("gateways"))
    {
        return REQ_NOT_HANDLED;
    }

    if(!checkApikeyAuthentification(req, rsp))
    {
        return REQ_READY_SEND;
    }

    // GET /api/<apikey>/gateways
    if ((req.path.size() == 3) && (req.hdr.method() == QLatin1String("GET")))
    {
        return getAllGateways(req, rsp);
    }
    // GET /api/<apikey>/gateways/<id>
    else if ((req.path.size() == 4) && (req.hdr.method() == QLatin1String("GET")))
    {
        return getGatewayState(req, rsp);
    }
    // PUT /api/<apikey>/gateways/<id>/state
    else if ((req.path.size() == 5) && (req.hdr.method() == QLatin1String("PUT")) && (req.path[4] == QLatin1String("state")))
    {
        return setGatewayState(req, rsp);
    }

    return REQ_NOT_HANDLED;
}

int DeRestPluginPrivate::getAllGateways(const ApiRequest &req, ApiResponse &rsp)
{
    Q_UNUSED(req);
    rsp.httpStatus = HttpStatusOk;

    for (size_t i = 0; i < gateways.size(); i++)
    {
        QVariantMap map;
        gatewayToMap(req, gateways[i], map);
        if (!map.isEmpty())
        {
            rsp.map[QString::number(i + 1)] = map;
        }
    }

    if (rsp.map.isEmpty())
    {
        rsp.str = "{}";
        return REQ_READY_SEND;
    }

    return REQ_READY_SEND;
}

/*! GET /api/<apikey>/gateways/<id>
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::getGatewayState(const ApiRequest &req, ApiResponse &rsp)
{
    DBG_Assert(req.path.size() == 4);

    if (req.path.size() != 4)
    {
        return REQ_NOT_HANDLED;
    }

    rsp.httpStatus = HttpStatusOk;

    bool ok;
    size_t idx = req.path[3].toUInt(&ok);

    if (!ok || idx == 0 || (idx - 1) >= gateways.size())
    {
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/gateways/%1").arg(req.path[3]), QString("resource, /gateways/%1, not available").arg(req.path[3])));
        rsp.httpStatus = HttpStatusNotFound;
        return REQ_READY_SEND;
    }

    idx -= 1;

    gatewayToMap(req, gateways[idx], rsp.map);

    if (rsp.map.isEmpty())
    {
        rsp.str = "{}";
    }

    return REQ_READY_SEND;
}

/*! PUT /api/<apikey>/gateways/<id>
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::setGatewayState(const ApiRequest &req, ApiResponse &rsp)
{
    DBG_Assert(req.path.size() == 5);

    if (req.path.size() != 5)
    {
        return REQ_NOT_HANDLED;
    }

    rsp.httpStatus = HttpStatusOk;

    bool ok;
    const QString &id = req.path[3];
    size_t idx = id.toUInt(&ok);

    if (!ok || idx == 0 || (idx - 1) >= gateways.size())
    {
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/gateways/%1").arg(id), QString("resource, /gateways/%1, not available").arg(id)));
        rsp.httpStatus = HttpStatusNotFound;
        return REQ_READY_SEND;
    }

    Gateway *gw = gateways[idx - 1];

    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();

    if (!ok || map.isEmpty())
    {
        rsp.httpStatus = HttpStatusBadRequest;
        rsp.list.append(errorToMap(ERR_INVALID_JSON, QString("/gateways/%1/state").arg(id), QLatin1String("body contains invalid JSON")));
        return REQ_READY_SEND;
    }

    if (map.contains(QLatin1String("pairing")))
    {
        if (map[QLatin1String("pairing")].type() == QVariant::Bool)
        {
            bool pairing = map[QLatin1String("pairing")].toBool();
            if (gw->pairingEnabled() != pairing)
            {
                gw->setPairingEnabled(pairing);
            }
            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/gateways/%1/state/pairing").arg(id)] = map[QLatin1String("pairing")];
            rspItem[QLatin1String("success")] = rspItemState;
            rsp.list.append(rspItem);
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/gateways/%1/state/pairing").arg(id), QString("invalid value, %1, for parameter, pairing").arg(map[QLatin1String("pairing")].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }

    if (!rsp.list.empty())
    {
        return REQ_READY_SEND;
    }

    return REQ_NOT_HANDLED;
}

/*! Puts all parameters in a map for later JSON serialization.
 */
void DeRestPluginPrivate::gatewayToMap(const ApiRequest &req, const Gateway *gw, QVariantMap &map)
{
    Q_UNUSED(req);

    if (!gw)
    {
        return;
    }

    if (!gw->uuid().isEmpty())
    {
        map[QLatin1String("uuid")] = gw->uuid();
    }
    if (!gw->name().isEmpty())
    {
        map[QLatin1String("name")] = gw->name();
    }
    map[QLatin1String("ip")] = gw->address().toString();
    map[QLatin1String("port")] = (double)gw->port();
    map[QLatin1String("pairing")] = gw->pairingEnabled();

    if (!gw->groups().empty())
    {
        QVariantMap groups;

        for (size_t i = 0; i < gw->groups().size(); i++)
        {
            const Gateway::Group &g = gw->groups()[i];
            groups[g.id] = g.name;
        }

        map[QLatin1String("groups")] = groups;
    }

    switch (gw->state())
    {
    case Gateway::StateConnected:     { map[QLatin1String("state")] = QLatin1String("connected"); } break;
    case Gateway::StateNotAuthorized: { map[QLatin1String("state")] = QLatin1String("not authorized"); } break;
    case Gateway::StateOffline:       { map[QLatin1String("state")] = QLatin1String("offline"); } break;
    default:                          { map[QLatin1String("state")] = QLatin1String("unknown"); }
        break;
    }
}

void DeRestPluginPrivate::foundGateway(quint32 ip, quint16 port, const QString &uuid, const QString &name)
{
    if (uuid.isEmpty())
    {
        return;
    }

    for (size_t i = 0; i < gateways.size(); i++)
    {
        Gateway *gw = gateways[i];
        Q_ASSERT(gw);

        if (gw && gw->uuid() == uuid)
        {
            if (gw->address().toIPv4Address() != ip || gw->port() != port)
            {
                gw->setAddress(QHostAddress(ip));
                gw->setPort(port);
            }

            return; // already known
        }
    }

    Q_ASSERT(gwUuid.length() >= 10);
    QString gwApikey = gwUuid.left(10);

    Gateway *gw = new Gateway(this);
    gw->setAddress(QHostAddress(ip));
    gw->setPort(port);
    gw->setUuid(uuid);
    gw->setName(name);
    gw->setApiKey(gwApikey);
    DBG_Printf(DBG_INFO, "found gateway %s:%u\n", qPrintable(gw->address().toString()), port);
    gateways.push_back(gw);
}
