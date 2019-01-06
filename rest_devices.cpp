/*
 * Copyright (c) 2013-2019 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QString>
#include <QTcpSocket>
#include <QUrlQuery>
#include <QVariantMap>
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "json.h"
#include "rest_devices.h"

RestDevices::RestDevices(QObject *parent) :
    QObject(parent)
{
    plugin = qobject_cast<DeRestPluginPrivate*>(parent);
    Q_ASSERT(plugin);
}

/*! Devices REST API broker.
    \param req - request data
    \param rsp - response data
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int RestDevices::handleApi(const ApiRequest &req, ApiResponse &rsp)
{
    if (req.path[2] != QLatin1String("devices"))
    {
        return REQ_NOT_HANDLED;
    }

    // GET /api/<apikey>/devices
    if ((req.path.size() == 3) && (req.hdr.method() == QLatin1String("GET")))
    {
        return getAllDevices(req, rsp);
    }
    // GET /api/<apikey>/devices/<uniqueid>
    else if ((req.path.size() == 4) && (req.hdr.method() == QLatin1String("GET")))
    {
        return getDevice(req, rsp);
    }

    return REQ_NOT_HANDLED;
}

/*! GET /api/<apikey>/devices
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int RestDevices::getAllDevices(const ApiRequest &req, ApiResponse &rsp)
{
    Q_UNUSED(req)

    if (rsp.list.isEmpty())
    {
        rsp.str = QLatin1String("[]"); // return empty list
    }
    return REQ_READY_SEND;
}

/*! GET /api/<apikey>/devices/<uniqueid>
    \return REQ_READY_SEND
            REQ_NOT_HANDLED

    Unstable API to experiment: don't use in production!
 */
int RestDevices::getDevice(const ApiRequest &req, ApiResponse &rsp)
{
    DBG_Assert(req.path.size() == 4);

    if (req.path.size() != 4)
    {
        return REQ_NOT_HANDLED;
    }

    const QString &uniqueid = req.path[3];

    QVariantList subDevices;
    QString modelid;
    QString swversion;
    QString manufacturer;

    // humble attemp to merge resources, these might be merged in one resource container later

    for (const LightNode &l : plugin->nodes)
    {
        if (l.uniqueId().indexOf(uniqueid) != 0)
        {
            continue;
        }

        if (manufacturer.isEmpty() && !l.manufacturer().isEmpty()) { manufacturer = l.manufacturer(); }
        if (modelid.isEmpty() && !l.modelId().isEmpty()) { modelid = l.modelId(); }
        if (swversion.isEmpty() && !l.swBuildId().isEmpty()) { swversion = l.swBuildId(); }

        QVariantMap m;
        if (plugin->lightToMap(req, &l, m))
        {
            subDevices.push_back(m);
        }
    }

    for (const Sensor &s : plugin->sensors)
    {
        if (s.uniqueId().indexOf(uniqueid) != 0)
        {
            continue;
        }

        if (manufacturer.isEmpty() && !s.manufacturer().isEmpty()) { manufacturer = s.manufacturer(); }
        if (modelid.isEmpty() && !s.modelId().isEmpty()) { modelid = s.modelId(); }
        if (swversion.isEmpty() && !s.swVersion().isEmpty()) { swversion = s.swVersion(); }

        QVariantMap m;
        if (plugin->sensorToMap(&s, m, req))
        {
            subDevices.push_back(m);
        }
    }

    rsp.map["uniqueid"] = uniqueid;
    rsp.map["sub"] = subDevices;
    if (!manufacturer.isEmpty()) { rsp.map["manufacturername"] = manufacturer; }
    if (!modelid.isEmpty()) { rsp.map["modelid"] = modelid; }
    if (!swversion.isEmpty()) { rsp.map["swversion"] = swversion; }

    return REQ_READY_SEND;
}
