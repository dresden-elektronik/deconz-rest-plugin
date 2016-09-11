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

    // GET /api/<apikey>/gateways
    if ((req.path.size() == 3) && (req.hdr.method() == QLatin1String("GET")))
    {
        return getGateways(req, rsp);
    }

    return REQ_NOT_HANDLED;
}

int DeRestPluginPrivate::getGateways(const ApiRequest &req, ApiResponse &rsp)
{
    Q_UNUSED(req);
    rsp.httpStatus = HttpStatusOk;
    rsp.str = "{}";

    for (size_t i = 0; i < gateways.size(); i++)
    {
        Gateway *gw = gateways[i];
        Q_ASSERT(gw);
        QVariantMap g;
        if (!gw->uuid().isEmpty())
        {
            g[QLatin1String("uuid")] = gw->uuid();
        }
        if (!gw->name().isEmpty())
        {
            g[QLatin1String("name")] = gw->name();
        }
        g[QLatin1String("ip")] = gw->address().toString();
        g[QLatin1String("port")] = (double)gw->port();
        rsp.list.push_back(g);
    }

    return REQ_READY_SEND;
}

void DeRestPluginPrivate::foundGateway(quint32 ip, quint16 port, const QString &uuid, const QString &name)
{

    for (size_t i = 0; i < gateways.size(); i++)
    {
        Gateway *gw = gateways[i];
        Q_ASSERT(gw);

        if (gw->address().toIPv4Address() == ip && gw->port() == port)
            return; // already known

        if (!uuid.isEmpty() && gw->uuid() == uuid)
            return; // already known
    }

    Gateway *gw = new Gateway(this);
    gw->setAddress(QHostAddress(ip));
    gw->setPort(port);
    gw->setUuid(uuid);
    gw->setName(name);
    DBG_Printf(DBG_INFO, "found gateway %s:%u\n", qPrintable(gw->address().toString()), port);
    gateways.push_back(gw);
}
