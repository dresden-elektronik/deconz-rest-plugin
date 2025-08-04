/*
 * Copyright (c) 2017-2025 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QString>
#include <QTcpSocket>
#include <QVariantMap>
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "json.h"

/*! Resourcelinks REST API broker.
    \param req - request data
    \param rsp - response data
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::handleResourcelinksApi(const ApiRequest &req, ApiResponse &rsp)
{
    if (req.path[2] != QLatin1String("resourcelinks"))
    {
        return REQ_NOT_HANDLED;
    }

    // GET /api/<apikey>/resourcelinks
    if ((req.path.size() == 3) && (req.hdr.method() == "GET"))
    {
        return getAllResourcelinks(req, rsp);
    }
    // GET /api/<apikey>/resourcelinks/<id>
    else if ((req.path.size() == 4) && (req.hdr.method() == "GET"))
    {
        return getResourcelinks(req, rsp);
    }
    // POST /api/<apikey>/resourcelinks
    else if ((req.path.size() == 3) && (req.hdr.method() == "POST"))
    {
        return createResourcelinks(req, rsp);
    }
    // PUT, PATCH /api/<apikey>/resourcelinks/<id>
    else if ((req.path.size() == 4) && (req.hdr.method() == "PUT" || req.hdr.method() == "PATCH"))
    {
        return updateResourcelinks(req, rsp);
    }
    // DELETE /api/<apikey>/resourcelinks/<id>
    else if ((req.path.size() == 4) && (req.hdr.method() == "DELETE"))
    {
        return deleteResourcelinks(req, rsp);
    }

    return REQ_NOT_HANDLED;
}

/*! GET /api/<apikey>/resourcelinks
    \param req - request data
    \param rsp - response data
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::getAllResourcelinks(const ApiRequest &req, ApiResponse &rsp)
{
    Q_UNUSED(req);
    for (const Resourcelinks &rl : resourcelinks)
    {
        if (rl.state == Resourcelinks::StateNormal)
        {
            rsp.map[rl.id] = rl.data;
        }
    }

    if (rsp.map.keys().isEmpty())
    {
        rsp.str = "{}"; // empty
    }

    rsp.httpStatus = HttpStatusOk;

    return REQ_READY_SEND;
}

/*! GET /api/<apikey>/resourcelinks/<id>
    \param req - request data
    \param rsp - response data
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::getResourcelinks(const ApiRequest &req, ApiResponse &rsp)
{
    DBG_Assert(req.path.size() == 4);
    const QString &id = req.path[3];
    rsp.httpStatus = HttpStatusOk;

    for (const Resourcelinks &rl : resourcelinks)
    {
        if (id == rl.id && rl.state == Resourcelinks::StateNormal)
        {
            rsp.map = rl.data;
            return REQ_READY_SEND;
        }
    }

    rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/resourcelinks/%1").arg(id), QString("resource, /resourcelinks/%1, not available").arg(id)));
    rsp.httpStatus = HttpStatusNotFound;
    return REQ_READY_SEND;
}

/*! POST /api/<apikey>/resourcelinks
    \param req - request data
    \param rsp - response data
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::createResourcelinks(const ApiRequest &req, ApiResponse &rsp)
{
    rsp.httpStatus = HttpStatusOk;

    bool ok;
    int errors = 0;
    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();

    Resourcelinks rl;

    if (!ok)
    {
        rsp.list.append(errorToMap(ERR_INVALID_JSON, QString("/resourcelinks"), QString("body contains invalid JSON")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    userActivity();

    //check required parameters
    {
        QStringList required({"name", "classid", "links"});

        for (const QString &param : required)
        {
            if (!map.contains(param))
            {
                rsp.list.append(errorToMap(ERR_MISSING_PARAMETER, QString("/resourcelinks/%1").arg(param), QString("invalid/missing parameters in body")));
                rsp.httpStatus = HttpStatusBadRequest;
                errors++;
            }
        }

        if (errors > 0)
        {
            return REQ_READY_SEND;
        }
    }

    //check available and valid parameters
    {
        QVariantMap available;
        available["name"] = static_cast<uint>(QVariant::String);
        available["description"] = static_cast<uint>(QVariant::String);
        available["type"] = static_cast<uint>(QVariant::String);
        available["classid"] = static_cast<uint>(QVariant::Double);
        available["links"] = static_cast<uint>(QVariant::List);
        available["recycle"] = static_cast<uint>(QVariant::Bool);
        QStringList availableKeys = available.keys();

        for (const QString &param : map.keys())
        {
            if (!availableKeys.contains(param))
            {
                rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/resourcelinks/%1").arg(param), QString("parameter, %1, not available").arg(param)));
                rsp.httpStatus = HttpStatusBadRequest;
                errors++;
            }
            else if (available[param].toUInt() != map[param].type())
            {
                DBG_Printf(DBG_INFO, "%d -- %d\n", available[param].toUInt(), map[param].type());
                rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/resourcelinks/%1").arg(param), QString("invalid value, %1, for parameter, %2").arg(map[param].toString()).arg(param)));
                rsp.httpStatus = HttpStatusBadRequest;
                errors++;
            }
        }

        if (errors > 0)
        {
            return REQ_READY_SEND;
        }
    }

    // generate id
    int id = 1;
    rl.id.setNum(id);
    while (std::find_if(resourcelinks.begin(), resourcelinks.end(),
                        [&rl](Resourcelinks &rl2) { return rl2.id == rl.id; }) != resourcelinks.end())
    {
        id++;
        rl.id.setNum(id);
    }

    rl.setNeedSaveDatabase(true);
    rl.data = map;
    rl.data["type"]  = QLatin1String("Link");
    rl.data["owner"] = req.path[1];

    if (!rl.data.contains(QLatin1String("description")) || rl.data["description"].toString().isNull())
    {
        rl.data["description"] = QLatin1String("");
    }

    if (!rl.data.contains(QLatin1String("recycle")))
    {
        rl.data["recycle"] = false;
    }

    resourcelinks.push_back(rl);
    queSaveDb(DB_RESOURCELINKS, DB_SHORT_SAVE_DELAY);

    QVariantMap rspItemState;
    QVariantMap rspItem;

    rspItemState["id"] = rl.id;
    rspItem["success"] = rspItemState;
    rsp.list.append(rspItem);

    return REQ_READY_SEND;
}

/*! PUT,PATCH /api/<apikey>/resourcelinks/<id>
    \param req - request data
    \param rsp - response data
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::updateResourcelinks(const ApiRequest &req, ApiResponse &rsp)
{
    bool ok;
    int errors = 0;
    DBG_Assert(req.path.size() == 4);
    const QString &id = req.path[3];
    rsp.httpStatus = HttpStatusOk;
    Resourcelinks *rl = nullptr;

    for (Resourcelinks &r : resourcelinks)
    {
        if (id == r.id && r.state == Resourcelinks::StateNormal)
        {
            rl = &r;
            break;
        }
    }

    if (!rl)
    {
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/resourcelinks/%1").arg(id), QString("resource, /resourcelinks/%1, not available").arg(id)));
        rsp.httpStatus = HttpStatusNotFound;
        return REQ_READY_SEND;
    }

    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();

    if (!ok)
    {
        rsp.list.append(errorToMap(ERR_INVALID_JSON, QString("/resourcelinks"), QString("body contains invalid JSON")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    userActivity();

    //check available and valid parameters
    {
        QVariantMap available;
        available["name"] = static_cast<uint>(QVariant::String);
        available["description"] = static_cast<uint>(QVariant::String);
        available["classid"] = static_cast<uint>(QVariant::Double);
        available["links"] = static_cast<uint>(QVariant::List);
        available["recycle"] = static_cast<uint>(QVariant::Bool);
        QStringList availableKeys = available.keys();

        for (const QString &param : map.keys())
        {
            if (!availableKeys.contains(param))
            {
                rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/resourcelinks/%1").arg(param), QString("parameter, %1, not available").arg(param)));
                rsp.httpStatus = HttpStatusBadRequest;
                errors++;
            }
            else if (available[param].toUInt() != map[param].type())
            {
                rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/resourcelinks/%1").arg(param), QString("invalid value, %1, for parameter, %2").arg(map[param].toString()).arg(param)));
                rsp.httpStatus = HttpStatusBadRequest;
                errors++;
            }
        }

        if (errors > 0)
        {
            return REQ_READY_SEND;
        }
    }

    for (const QString &param : map.keys())
    {
        rl->data[param] = map[param];
        QVariantMap rspItemState;
        QVariantMap rspItem;
        rspItemState[QString("/resourcelinks/%1/%2").arg(id).arg(param)] = map[param];
        rspItem["success"] = rspItemState;
        rsp.list.push_back(rspItem);
    }

    if (!rl->data.contains(QLatin1String("description")) || rl->data["description"].toString().isNull())
    {
        rl->data["description"] = QLatin1String("");
    }

    rl->setNeedSaveDatabase(true);
    queSaveDb(DB_RESOURCELINKS, DB_SHORT_SAVE_DELAY);

    if (rsp.list.empty())
    {
        rsp.str = QLatin1String("[]"); // empty
    }

    return REQ_READY_SEND;
}

/*! DELETE /api/<apikey>/resourcelinks/<id>
    \param req - request data
    \param rsp - response data
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::deleteResourcelinks(const ApiRequest &req, ApiResponse &rsp)
{
    DBG_Assert(req.path.size() == 4);
    const QString &id = req.path[3];
    rsp.httpStatus = HttpStatusOk;

    for (Resourcelinks &rl : resourcelinks)
    {
        if (id == rl.id && rl.state == Resourcelinks::StateNormal)
        {
            rl.state = Resourcelinks::StateDeleted;
            rl.setNeedSaveDatabase(true);
            queSaveDb(DB_RESOURCELINKS, DB_SHORT_SAVE_DELAY);

            QVariantMap rspItem;
            rspItem["success"] = QString("/resourcelinks/%1 deleted.").arg(id);
            rsp.list.append(rspItem);
            return REQ_READY_SEND;
        }
    }

    rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/resourcelinks/%1").arg(id), QString("resource, /resourcelinks/%1, not available").arg(id)));
    rsp.httpStatus = HttpStatusNotFound;
    return REQ_READY_SEND;
}
