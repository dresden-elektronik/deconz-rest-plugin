/*
 * Copyright (c) 2018-2019 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "de_web_plugin.h"
#include "de_web_plugin_private.h"


/*! Scenes REST API broker.
    \param req - request data
    \param rsp - response data
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::handleScenesApi(const ApiRequest &req, ApiResponse &rsp)
{
    // PUT /api/<username>/scenes
    if ((req.path.size() == 3) && (req.hdr.method() == QLatin1String("PUT")))
    {
        return createScene(req, rsp);
    }
    // POST /api/<username>/scenes
    else if ((req.path.size() == 3) && (req.hdr.method() == QLatin1String("POST")))
    {
        return createScene(req, rsp);
    }
    // GET /api/<username>/scenes
    else if ((req.path.size() == 3) && (req.hdr.method() == QLatin1String("GET")))
    {
        return getAllScenes(req, rsp);
    }
    // GET /api/<username>/scenes/<id>
    else if ((req.path.size() == 4) && (req.hdr.method() == QLatin1String("GET")))
    {
        return getScene(req, rsp);
    }
    // PUT, PATCH /api/<username>/scenes/<id>
    else if ((req.path.size() == 4) && (req.hdr.method() == QLatin1String("PUT") || req.hdr.method() == QLatin1String("PATCH")))
    {
        // return modifyScene(req, rsp);
    }
    // PUT, PATCH /api/<username>/scenes/<id>/lightstates/<id>
    else if ((req.path.size() == 6) && (req.hdr.method() == QLatin1String("PUT") || req.hdr.method() == QLatin1String("PATCH")) && (req.path[4] == QLatin1String("lightstates")))
    {
        // return modifySceneLightState(req, rsp);
    }
    // DELETE /api/<username>/scenes/<id>
    else if ((req.path.size() == 4) && (req.hdr.method() == QLatin1String("DELETE")))
    {
        return deleteScene(req, rsp);
    }
    if (rsp.map.isEmpty())
    {
        rsp.str = QLatin1String("{}"); // return empty object
    }
    rsp.httpStatus = HttpStatusOk;
    return REQ_READY_SEND;
}

/*! POST /api/<apikey>/(groups/<group_id>/)scenes
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::createScene(const ApiRequest& req, ApiResponse& rsp)
{
    bool ok;
    int errors = 0;
    const QString resource = "/" + req.path.mid(2).join("/");

    // status
    userActivity();

    if (!isInNetwork())
    {
        rsp.list.append(errorToMap(ERR_NOT_CONNECTED, resource, QLatin1String("Not connected")));
        rsp.httpStatus = HttpStatusServiceUnavailable;
        return REQ_READY_SEND;
    }

    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();

    if (!ok || map.isEmpty())
    {
        rsp.list.append(errorToMap(ERR_INVALID_JSON, resource, QLatin1String("body contains invalid JSON")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    //check available and valid parameters
    {
        QVariantMap available;
        available[QLatin1String("id")] = static_cast<uint>(QVariant::String);
        available[QLatin1String("name")] = static_cast<uint>(QVariant::String);
        available[QLatin1String("type")] = static_cast<uint>(QVariant::String);
        available[QLatin1String("group")] = static_cast<uint>(QVariant::Double);
        available[QLatin1String("lights")] = static_cast<uint>(QVariant::List);
        available[QLatin1String("recycle")] = static_cast<uint>(QVariant::Bool);
        available[QLatin1String("appdata")] = static_cast<uint>(QVariant::Map);
        available[QLatin1String("picture")] = static_cast<uint>(QVariant::String);
        QStringList availableKeys = available.keys();

        for (const QString &param : map.keys())
        {
            if (!availableKeys.contains(param))
            {
                rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, resource + QString("/%1").arg(param), QString("parameter, %1, not available").arg(param)));
                rsp.httpStatus = HttpStatusBadRequest;
                errors++;
            }
            else if ((available[param].toUInt() != map[param].type()) || ((param == QLatin1String("type")) && (map[param] != QLatin1String("LightScene")) && (map[param] != QLatin1String("GroupScene"))))
            {
                DBG_Printf(DBG_INFO, "%d -- %d\n", available[param].toUInt(), map[param].type());
                rsp.list.append(errorToMap(ERR_INVALID_VALUE, resource + QString("/%1").arg(param), QString("invalid value, %1, for parameter, %2").arg(map[param].toString()).arg(param)));
                rsp.httpStatus = HttpStatusBadRequest;
                errors++;
            }
        }

        if (errors > 0)
        {
            return REQ_READY_SEND;
        }
    }

    // gid
    uint16_t gid = 0;
    if ((req.path.size() == 5) && req.path[2] == QLatin1String("groups"))
    {
        gid = req.path[3].toUInt(&ok);
    }
    else if (map.contains(QLatin1String("type")) && (map[QLatin1String("type")] == QLatin1String("GroupScene")) && map.contains(QLatin1String("group")))
    {
        gid = map[QLatin1String("group")].toUInt(&ok);
    }
    Group *group = getGroupForId(gid);
    if (!group || (group->state() != Group::StateNormal))
    {
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/groups/%1").arg(gid), QString("resource, /groups/%1, not available").arg(gid)));
        rsp.httpStatus = HttpStatusNotFound;
        return REQ_READY_SEND;
    }

    // sid
    uint8_t sid = 1;
    if (group->m_deviceMemberships.size() >= 1)
    {
        QString deviceId = group->m_deviceMemberships[0];
        Sensor *s = getSensorNodeForId(deviceId);
        if (s && s->modelId() == QLatin1String("Lighting Switch"))
        {
            sid = 4; // ommit scene 2 and 3 for Lighting Switch
        }
    }
    do {
        ok = true;
        for (Scene& s : group->scenes)
        {
            if (s.sid() == sid) {
                sid++;
                ok = false;
            }
        }
    } while (!ok);

    // id
    QString id;
    id.sprintf("0x%04X%02X", gid, sid);
    if (map.contains(QLatin1String("id")))
    {
        QString m_id = map[QLatin1String("id")].toString();
        if (m_id.size() == 8 && m_id.startsWith(QLatin1String("0x")) && m_id != id)
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, resource + QLatin1String("/id"), QString("invalid value, %1, for parameter, id").arg(m_id)));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }
    Scene *s = getSceneForId(id);
    if (s && s->state() == Scene::StateNormal)
    {
        rsp.list.append(errorToMap(ERR_DUPLICATE_EXIST, resource + QString("/%1").arg(id), QString("resource, /scenes/%1, already exists").arg(id)));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    // owner
    QString owner = req.apikey();

    // version
    uint16_t version = 1;
    if (req.hdr.method() == QLatin1String("POST"))
    {
        version = 2;
    }

    // create scene
    Scene scene(group->address(), sid, group->address() == gwGroup0 ? Scene::LightScene : Scene::GroupScene);
    scene.init(id, owner, QDateTime::currentDateTimeUtc(), version);

    // name
    if (map.contains(QLatin1String("name")))
    {
        scene.name(map[QLatin1String("name")].toString());
    }

    // lights
    std::vector<LightNode*> lights;
    if (gid != 0) // groupscene
    {
        std::vector<LightNode>::iterator l = nodes.begin();
        std::vector<LightNode>::iterator l_end = nodes.end();
        for (; l != l_end; ++l)
        {
            LightNode* light = &(*l);
            if (isLightNodeInGroup(light, scene.gid()))
            {
                lights.push_back(light);
            }
        }
    }
    else if (map.contains(QLatin1String("lights")))
    {
        QVariantList ls = map[QLatin1String("lights")].toList();
        for (QVariant &l : ls)
        {
            QString lid = l.toString();
            LightNode* light = getLightNodeForId(lid);
            lights.push_back(light);
        }
        
    }
    for (LightNode* light : lights)
    {
        if (!light || (light->state() == LightNode::StateDeleted) || !light->isAvailable())
        {
            rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, resource, QString("resource, /lights/%1, not available").arg(light->id())));
            rsp.httpStatus = HttpStatusNotFound;
            return REQ_READY_SEND;
        }
        if (light->sceneCapacity() <= 0)
        {
            rsp.list.append(errorToMap(ERR_DEVICE_SCENES_TABLE_FULL, resource, QString("Could not set scene for %1. Scene capacity of the device is reached.").arg(qPrintable(light->name()))));
            continue;
        }
        LightState state = light->lightstate();
        scene.addLight(state);
        queSaveDb(DB_SCENES, DB_LONG_SAVE_DELAY);

    }
    if (gid != 0) // groupscene
    {
        if (!storeScene(group, scene.sid()))
        {
            rsp.list.append(errorToMap(ERR_BRIDGE_BUSY, resource + QString("/%1").arg(id), QLatin1String("gateway busy")));
            rsp.httpStatus = HttpStatusServiceUnavailable;
            return REQ_READY_SEND;
        }
    }
    else
    {
        for (LightNode* light : lights)
        {
            GroupInfo *groupInfo = getGroupInfo(light, scene.gid());
            std::vector<uint8_t> &v = groupInfo->addScenes;
            if (std::find(v.begin(), v.end(), scene.sid()) == v.end())
            {
                groupInfo->addScenes.push_back(scene.sid());
            }
        }
    }

    // recycle
    if (req.hdr.method() == QLatin1String("PUT"))
    {
        scene.recycle(true);
    }
    else if (map.contains(QLatin1String("recycle")))
    {
        scene.recycle(map[QLatin1String("recycle")].toBool());
    }

    // appdata
    if (map.contains(QLatin1String("appdata")))
    {
        scene.appdata(map[QLatin1String("appdata")].toMap());
    }

    // picture
    if (map.contains(QLatin1String("picture")))
    {
        scene.picture(map[QLatin1String("picture")].toString());
    }

    group->scenes.push_back(scene);
    updateGroupEtag(group);
    queSaveDb(DB_SCENES, DB_SHORT_SAVE_DELAY);

    QVariantMap rspItem;
    QVariantMap rspItemState;

    rspItemState["id"] = scene.id();
    rspItem["success"] = rspItemState;
    rsp.list.append(rspItem);
    rsp.httpStatus = HttpStatusOk;

    return REQ_READY_SEND;
}

/*! GET /api/<apikey>/scenes
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::getAllScenes(const ApiRequest& req, ApiResponse& rsp)
{
    Q_UNUSED(req);
    std::vector<Group>::const_iterator g = groups.begin();
    std::vector<Group>::const_iterator g_end = groups.end();

    for (; g != g_end; ++g)
    {
        std::vector<Scene>::const_iterator s = g->scenes.begin();
        std::vector<Scene>::const_iterator s_end = g->scenes.end();

        for (; s != s_end; ++s)
        {
            // ignore deleted groups
            if (s->state() == Scene::StateDeleted)
            {
                continue;
            }
            rsp.map[s->id()] = s->map();
        }
    }
    if (rsp.map.isEmpty())
    {
        rsp.str = "{}"; // return empty object
    }

    rsp.httpStatus = HttpStatusOk;
    return REQ_READY_SEND;
}

/*! GET /api/<apikey>/scenes/<scene_id>
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::getScene(const ApiRequest& req, ApiResponse& rsp)
{
    const QString resource = "/" + req.path.mid(2).join("/");

    QString id = req.path[3];
    Scene* s = getSceneForId(id);
    if (s) {
        rsp.map = s->map();

        QVariantMap lightstates;
        for (const LightState& l : s->lights())
        {
            lightstates[l.lid()] = l.map();
        }
        rsp.map["lightstates"] = lightstates;
    }
    else
    {
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, resource, "resource, " + resource + ", not available"));
        rsp.httpStatus = HttpStatusNotFound;
        return REQ_READY_SEND;
    }

    return REQ_READY_SEND;
}

/*! DELETE /api/<username>/scenes/<id>
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::deleteScene(const ApiRequest& req, ApiResponse& rsp)
{
    const QString resource = "/" + req.path.mid(2).join("/");
    QString id = req.path[3];

    // status
    userActivity();

    if (!isInNetwork())
    {
        rsp.list.append(errorToMap(ERR_NOT_CONNECTED, resource, QLatin1String("Not connected")));
        rsp.httpStatus = HttpStatusServiceUnavailable;
        return REQ_READY_SEND;
    }

    Scene* scene = getSceneForId(id);
    if (!scene || scene->state() == Scene::StateDeleted)
    {
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, resource, "resource, " + resource + ", not available"));
        rsp.httpStatus = HttpStatusNotFound;
        return REQ_READY_SEND;
    }

    Group* group = getGroupForId(scene->gid());
    if (!group || group->state() == Group::StateDeleted)
    {
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/groups/%1").arg(scene->gid()), QString("resource, /groups/%1, not available").arg(scene->gid())));
        rsp.httpStatus = HttpStatusNotFound;
        return REQ_READY_SEND;
    }

    if (!removeScene(group, scene))
    {
        rsp.list.append(errorToMap(ERR_NOT_CONNECTED, resource, "gateway busy"));
        rsp.httpStatus = HttpStatusServiceUnavailable;
        return REQ_READY_SEND;
    }

    updateGroupEtag(group);
    queSaveDb(DB_SCENES, DB_SHORT_SAVE_DELAY);

    QVariantMap rspItem;
    QVariantMap rspItemState;
    rsp.httpStatus = HttpStatusOk;
    rspItemState["id"] = id;
    rspItem["success"] = rspItemState;
    rsp.list.append(rspItem);

    return REQ_READY_SEND;
}
