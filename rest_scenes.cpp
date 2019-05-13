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
#include "colorspace.h"


/*! Scenes REST API broker.
    \param req - request data
    \param rsp - response data
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::handleScenesApi(const ApiRequest& req, ApiResponse& rsp)
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
        return modifyScene(req, rsp);
    }
    // PUT, PATCH /api/<username>/scenes/<id>/lightstates/<id>
    else if ((req.path.size() == 6) && (req.hdr.method() == QLatin1String("PUT") || req.hdr.method() == QLatin1String("PATCH")) && (req.path[4] == QLatin1String("lightstates")))
    {
        return modifySceneLightState(req, rsp);
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
        rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, resource, QString("Internal error, %1").arg(ERR_NOT_CONNECTED)));
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
        available[QLatin1String("group")] = static_cast<uint>(QVariant::String);
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
        rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, resource + QString("/%1").arg(id), QString("Internal error, %1").arg(ERR_DUPLICATE_EXIST)));
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

    if (gid != 0) // groupscene
    {
        if (!storeScene(group, scene.sid()))
        {
            rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, resource + QString("/%1").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
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

/*! PUT, PATCH /api/<username>/scenes/<id>
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::modifyScene(const ApiRequest& req, ApiResponse& rsp)
{
    bool ok;
    int errors = 0;
    const QString resource = "/" + req.path.mid(2).join("/");

    // status
    userActivity();

    if (!isInNetwork())
    {
        rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, resource, QString("Internal error, %1").arg(ERR_NOT_CONNECTED)));
        rsp.httpStatus = HttpStatusServiceUnavailable;
        return REQ_READY_SEND;
    }

    // parse json
    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();
    if (!ok)
    {
        rsp.list.append(errorToMap(ERR_INVALID_JSON, resource, QLatin1String("body contains invalid JSON")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }
    if (map.isEmpty())
    {
        rsp.str = QLatin1String("[]"); // return empty object
        rsp.httpStatus = HttpStatusOk;
        return REQ_READY_SEND;
    }

    //check available and valid parameters
    {
        QVariantMap available;
        available[QLatin1String("name")] = static_cast<uint>(QVariant::String);
        available[QLatin1String("lights")] = static_cast<uint>(QVariant::List);
        available[QLatin1String("lightstates")] = static_cast<uint>(QVariant::Map);
        available[QLatin1String("storelightstate")] = static_cast<uint>(QVariant::Bool);
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
            else if (available[param].toUInt() != map[param].type())
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

    // get Scene
    QString id = req.path[3];
    Scene* scene = getSceneForId(id);
    if (!scene || (scene->state() == Scene::StateDeleted))
    {
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, "/" + req.path.mid(2,2).join("/"), "resource, /" + req.path.mid(2,2).join("/") + ", not available"));
        rsp.httpStatus = HttpStatusNotFound;
        return REQ_READY_SEND;
    }
    Group* group = getGroupForId(scene->gid());
    if (!group || (group->state() != Group::StateNormal))
    {
        rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, resource, QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
        rsp.httpStatus = HttpStatusServiceUnavailable;
        return REQ_READY_SEND;
    }

    // name
    if (map.contains(QLatin1String("name")))
    {
        QString name = map[QLatin1String("name")].toString();
        scene->name(name);

        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState[resource + "/name"] = name;
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
    }

    // lights
    std::vector<QString> lights;
    QVariantList ls;
    if (map.contains(QLatin1String("lights")))
    {
        if (scene->gid() != 0) {
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, resource + QLatin1String("/lights"), QLatin1String("parameter, lights, is not modifiable")));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

        ls = map[QLatin1String("lights")].toList();
        for (QVariant &l : ls)
        {
            lights.push_back(l.toString());
        }

        // remove lights
        for (const LightState& state : scene->lights())
        {
            if (std::find(lights.begin(), lights.end(), state.lid()) == lights.end())
            {
                LightNode* l = getLightNodeForId(state.lid());
                if (l)
                {
                    GroupInfo *groupInfo = getGroupInfo(l, scene->gid());
                    std::vector<uint8_t> &v = groupInfo->removeScenes;
                    if (std::find(v.begin(), v.end(), scene->sid()) == v.end())
                    {
                        groupInfo->removeScenes.push_back(scene->sid());
                    }
                }
            }
        }
    }
    else if (map.contains(QLatin1String("storelightstate")) && map[QLatin1String("storelightstate")].toBool())
    {
        for (const LightState& state : scene->lights())
        {
            lights.push_back(state.lid());
            ls.append(state.lid());
        }
    }
    // modify/add lights
    for (QString& lid : lights)
    {
        LightNode* light = getLightNodeForId(lid);
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

        GroupInfo *groupInfo = getGroupInfo(light, scene->gid());
        LightState state = light->lightstate();

        if (scene->getLight(lid))
        { // modify
            scene->removeLight(lid);
            scene->addLight(state);
            std::vector<uint8_t> &v = groupInfo->modifyScenes;
            if (std::find(v.begin(), v.end(), scene->sid()) == v.end())
            {
                groupInfo->modifyScenes.push_back(scene->sid());
            }
        }
        else
        { // add
            scene->addLight(state);
            std::vector<uint8_t> &v = groupInfo->addScenes;
            if (std::find(v.begin(), v.end(), scene->sid()) == v.end())
            {
                groupInfo->addScenes.push_back(scene->sid());
            }
        }
        queSaveDb(DB_SCENES, DB_LONG_SAVE_DELAY);
    }
    if (!ls.empty())
    {
        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState[resource + "/lights"] = ls;
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
    }

    // lightstates
    if (map.contains(QLatin1String("lightstates")))
    {
        QVariantMap ls = map[QLatin1String("lightstates")].toMap();
        for (QVariant& l : ls)
        {
            QString lid = l.toString();
            if (!modifyLightState(resource, rsp, lid, ls[lid].toMap(), scene))
            {
                return REQ_READY_SEND;
            }
        }
    }

    // recycle
    if (map.contains(QLatin1String("recycle")))
    {
        bool recycle = map[QLatin1String("recycle")].toBool();
        scene->recycle(recycle);

        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState["/" + req.path.mid(2).join("/") + "/recycle"] = recycle;
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
    }

    // appdata
    if (map.contains(QLatin1String("appdata")))
    {
        QVariantMap appdata = map[QLatin1String("appdata")].toMap();
        scene->appdata(appdata);

        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState["/" + req.path.mid(2).join("/") + "/appdata"] = appdata;
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
    }

    // picture
    if (map.contains(QLatin1String("picture")))
    {
        QString picture = map[QLatin1String("picture")].toString();
        scene->picture(picture);

        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState["/" + req.path.mid(2).join("/") + "/picture"] = picture;
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
    }

    scene->lastupdated();
    updateGroupEtag(group);
    queSaveDb(DB_SCENES, DB_SHORT_SAVE_DELAY);

    if (rsp.list.isEmpty())
    {
        rsp.list.append(errorToMap(ERR_INVALID_JSON, resource, QString("body contains invalid JSON")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    rsp.httpStatus = HttpStatusOk;
    return REQ_READY_SEND;
}

/*! PUT, PATCH /api/<username>/scenes/<id>/lightstates/<id>
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::modifySceneLightState(const ApiRequest& req, ApiResponse& rsp)
{
    const QString resource = "/" + req.path.mid(2).join("/");

    // status
    userActivity();

    if (!isInNetwork())
    {
        rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, resource, QString("Internal error, %1").arg(ERR_NOT_CONNECTED)));
        rsp.httpStatus = HttpStatusServiceUnavailable;
        return REQ_READY_SEND;
    }

    // parse json
    bool ok;
    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();
    if (!ok || map.isEmpty())
    {
        rsp.list.append(errorToMap(ERR_INVALID_JSON, resource, QString("body contains invalid JSON")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    // get Scene
    QString id = req.path[3];
    Scene* scene = getSceneForId(id);

    // get Light
    QString lid = req.path[5];
    if (modifyLightState("/" + req.path.mid(2,2).join("/"), rsp, lid, map, scene))
    {
        rsp.httpStatus = HttpStatusOk;
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
        rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, resource, QString("Internal error, %1").arg(ERR_NOT_CONNECTED)));
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
        rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, resource, QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
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

/*! Modify light state
    \return success
 */
bool DeRestPluginPrivate::modifyLightState(const QString& resource, ApiResponse& rsp, const QString& lid, const QVariantMap& map, Scene* scene)
{
    if (!scene || (scene->state() == Scene::StateDeleted))
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, resource, "resource, /" + resource + ", not available"));
        return false;
    }

    LightState* state = scene->getLight(lid);
    if (!state)
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, resource + "/lightstates/" + lid, "resource , " + resource + "/lightstates/" + lid + ", not available"));
        return false;
    }

    bool ok;
    int errors = 0;
   //check available and valid parameters
    {
        QVariantMap available;
        available[QLatin1String("on")] = static_cast<uint>(QVariant::Bool);
        available[QLatin1String("bri")] = static_cast<uint>(QVariant::Double);
        available[QLatin1String("hue")] = static_cast<uint>(QVariant::Double);
        available[QLatin1String("sat")] = static_cast<uint>(QVariant::Double);
        available[QLatin1String("ct")] = static_cast<uint>(QVariant::Double);
        available[QLatin1String("xy")] = static_cast<uint>(QVariant::List);
        available[QLatin1String("effect")] = static_cast<uint>(QVariant::String);
        available[QLatin1String("transitiontime")] = static_cast<uint>(QVariant::Double);
        QStringList availableKeys = available.keys();

        for (const QString &param : map.keys())
        {
            if (!availableKeys.contains(param))
            {
                rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, resource + "/lightstates/" + lid + "/" + param, QString("parameter, %1, not available").arg(param)));
                rsp.httpStatus = HttpStatusBadRequest;
                errors++;
            }
            else if (available[param].toUInt() != map[param].type())
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

    // on, bri, hue, sat, xy, ct, effect, transitiontime
    bool modify = false;
    // on
    if (map.contains("on"))
    {
        bool on = map["on"].toBool();
        state->setOn(on);
        modify = true;

        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState[resource + "/on"] = on;
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
    }

    // bri
    if (map.contains("bri"))
    {
        uint bri = map["bri"].toUInt(&ok);
        if (!ok || map["bri"].type() != QVariant::Double || (bri >= 256))
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, resource, QString("invalid value, %1, for parameter on").arg(bri)));
            rsp.httpStatus = HttpStatusBadRequest;
            return false;
        }
        state->setBri(bri);
        modify = true;

        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState[resource + "/bri"] = bri;
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
    }

    // determine colormode
    QString colormode = map.contains("xy") ? "xy" : map.contains("ct") ? "ct" : (map.contains("hue") || map.contains("sat")) ? "hs" : "none";
    state->setColorMode(colormode);

    // hue
    if (map.contains("hue"))
    {
        uint hue = map["hue"].toUInt(&ok);
        if (!ok || map["hue"].type() != QVariant::Double || (hue > MAX_ENHANCED_HUE))
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, resource, QString("invalid value, %1, for parameter hue").arg(hue)));
            rsp.httpStatus = HttpStatusBadRequest;
            return false;
        }
        state->setEnhancedHue(hue);
        if (state->colorMode() == "hs" && !map.contains("sat")) 
        {
            double r, g, b;
            double x, y;
            double h = ((360.0 / 65535.0) * hue);
            double s = state->saturation() / 255.0;
            double v = 1.0;

            Hsv2Rgb(&r, &g, &b, h, s, v);
            Rgb2xy(&x, &y, r, g, b);

            if (x < 0) { x = 0; }
            else if (x > 1) { x = 1; }

            if (y < 0) { y = 0; }
            else if (y > 1) { y = 1; }

            DBG_Printf(DBG_INFO, "x: %f, y: %f\n", x, y);
            state->setX(static_cast<quint16>(x * 65535.0));
            state->setY(static_cast<quint16>(y * 65535.0));

            if (state->x() > 65279) { state->setX(65279); }
            else if (state->x() == 0) { state->setX(1); }

            if (state->y() > 65279) { state->setY(65279); }
            else if (state->y() == 0) { state->setY(1); }
        }
        modify = true;

        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState[resource + "/hue"] = hue;
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
    }

    // sat
    if (map.contains("sat"))
    {
        uint sat = map["sat"].toUInt(&ok);
        if (!ok || map["sat"].type() != QVariant::Double || (sat >= 256))
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, resource, QString("invalid value, %1, for parameter sat").arg(sat)));
            rsp.httpStatus = HttpStatusBadRequest;
            return false;
        }
        state->setSaturation(sat);
        if (state->colorMode() == "hs") 
        {
            double r, g, b;
            double x, y;
            double h = ((360.0 / 65535.0) * state->enhancedHue());
            double s = sat / 254.0;
            double v = 1.0;

            Hsv2Rgb(&r, &g, &b, h, s, v);
            Rgb2xy(&x, &y, r, g, b);

            if (x < 0) { x = 0; }
            else if (x > 1) { x = 1; }

            if (y < 0) { y = 0; }
            else if (y > 1) { y = 1; }

            DBG_Printf(DBG_INFO, "x: %f, y: %f\n", x, y);
            state->setX(static_cast<quint16>(x * 65535.0));
            state->setY(static_cast<quint16>(y * 65535.0));

            if (state->x() > 65279) { state->setX(65279); }
            else if (state->x() == 0) { state->setX(1); }

            if (state->y() > 65279) { state->setY(65279); }
            else if (state->y() == 0) { state->setY(1); }
        }
        modify = true;

        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState[resource + "/sat"] = sat;
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
    }

    // ct
    if (map.contains("ct"))
    {
        uint ct = map["ct"].toUInt(&ok);
        if (!ok || map["ct"].type() != QVariant::Double || (ct >= 65536))
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, resource, QString("invalid value, %1, for parameter ct").arg(ct)));
            rsp.httpStatus = HttpStatusBadRequest;
            return false;
        }
        state->setColorTemperature(ct);
        if (state->colorMode() == "hs")
        {
            quint16 x;
            quint16 y;
            MiredColorTemperatureToXY(ct, &x, &y);
            state->setX(x);
            state->setY(y);

            if (state->x() > 65279) { state->setX(65279); }
            else if (state->x() == 0) { state->setX(1); }

            if (state->y() > 65279) { state->setY(65279); }
            else if (state->y() == 0) { state->setY(1); }
        }
        modify = true;

        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState[resource + "/ct"] = ct;
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
    }

    // xy
    if (map.contains("xy"))
    {
        QVariantList xy = map["xy"].toList();
        if ((xy.size() != 2) || (xy[0].type() != QVariant::Double) || (xy[1].type() != QVariant::Double))
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, resource, QString("invalid value, %1, for parameter xy").arg(map["xy"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
        double x = xy[0].toDouble(&ok);
        double y = ok ? xy[1].toDouble() : 0;
        if (!ok || (x < 0.0) || (x > 1.0) || (y < 0.0) || (y > 1.0))
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, resource, QString("invalid value, [%1,%2], for parameter xy").arg(xy[0].toString()).arg(xy[1].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
        uint16_t xy_x = static_cast<uint16_t>(x * 65535.0);
        uint16_t xy_y = static_cast<uint16_t>(y * 65535.0);
        if (xy_x > 65279) { xy_x = 65279; }
        else if (xy_x == 0) { xy_x = 1; }
        if (xy_y > 65279) { xy_y = 65279; }
        else if (xy_y == 0) { xy_y = 1; }
        state->setX(xy_x);
        state->setY(xy_y);
        modify = true;

        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState[resource + "/xy"] = xy;
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
    }

    // effect
    if (map.contains("effect"))
    {
        QString effect = map["effect"].toString();
        if (! (effect == QLatin1String("none") || effect == QLatin1String("colorloop")) )
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, resource, QString("invalid value, %1, for parameter effect").arg(effect)));
            rsp.httpStatus = HttpStatusBadRequest;
            return false;
        }
        state->setColorloopActive(effect == QLatin1String("colorloop"));
        modify = true;

        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState[resource + "/effect"] = effect;
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
    }

    // transitiontime
    if (map.contains("transitiontime"))
    {
        uint transitiontime = map["transitiontime"].toUInt(&ok);
        if (!ok || map["transitiontime"].type() != QVariant::Double || (transitiontime >= 65536))
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, resource, QString("invalid value, %1, for parameter transitiontime").arg(transitiontime)));
            rsp.httpStatus = HttpStatusBadRequest;
            return false;
        }
        state->setTransitionTime(transitiontime);
        modify = true;

        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState[resource + "/transitiontime"] = transitiontime;
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
    }

    // modify
    if (modify)
    {
        scene->lastupdated();
        LightNode *lightNode = getLightNodeForId(lid);
        if (lightNode->isAvailable() && // note: we only modify the scene if node is available
            isLightNodeInGroup(lightNode, scene->gid()))
        {
            GroupInfo *groupInfo = getGroupInfo(lightNode, scene->gid());
            if (groupInfo)
            {
                std::vector<uint8_t> &v = groupInfo->modifyScenes;
                if (std::find(v.begin(), v.end(), scene->sid()) == v.end())
                {
                    DBG_Printf(DBG_INFO, "Start modify scene for 0x%016llX, groupId 0x%04X, scene 0x%02X\n", lightNode->address().ext(), scene->gid(), scene->sid());
                    groupInfo->modifyScenes.push_back(scene->sid());
                }
            }
        } else {
            rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, resource, QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
            rsp.httpStatus = HttpStatusServiceUnavailable;
            return false;
        }
    }

    queSaveDb(DB_SCENES, DB_SHORT_SAVE_DELAY);
    return true;
}

/*! Recall scene.
 */
bool DeRestPluginPrivate::recallScene(Group* group, const Scene* scene)
{
    if (!group || !scene)
    {
        return false;
    }

    bool groupOn = false;
    std::vector<LightState>::const_iterator ls = scene->lights().begin();
    std::vector<LightState>::const_iterator lsend = scene->lights().end();

    for (; ls != lsend; ++ls)
    {
        LightNode *lightNode = getLightNodeForId(ls->lid());

        if (lightNode && lightNode->isAvailable() && lightNode->state() == LightNode::StateNormal)
        {
            if (ls->on())
            {
                groupOn = true;
            }

            if (lightNode->hasColor())
            {
                if (!ls->colorloopActive() && lightNode->isColorLoopActive() != ls->colorloopActive())
                {
                    //stop colorloop if scene was saved without colorloop (Osram don't stop colorloop if another scene is called)
                    TaskItem task2;
                    task2.lightNode = lightNode;
                    task2.req.dstAddress() = task2.lightNode->address();
                    task2.req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
                    task2.req.setDstEndpoint(task2.lightNode->haEndpoint().endpoint());
                    task2.req.setSrcEndpoint(getSrcEndpoint(task2.lightNode, task2.req));
                    task2.req.setDstAddressMode(deCONZ::ApsExtAddress);

                    lightNode->setColorLoopActive(false);
                    addTaskSetColorLoop(task2, false, 15);
                    updateLightEtag(lightNode);
                }
            }

        }
    }

    if (!callScene(group, scene->sid()))
    {
        return false;
    }

    {
        QString scid = QString::number(scene->sid());
        ResourceItem *item = group->item(RActionScene);
        if (item && item->toString() != scid)
        {
            item->setValue(scid);
            updateGroupEtag(group);
            Event e(RGroups, RActionScene, group->id(), item);
            enqueueEvent(e);
        }
    }

    bool groupOnChanged = false;
    bool groupBriChanged = false;
    bool groupHueSatChanged = false;
    bool groupCtChanged = false;
    bool groupColorModeChanged = false;

    //turn on colorloop if scene was saved with colorloop (FLS don't save colorloop at device)
    ls = scene->lights().begin();
    lsend = scene->lights().end();

    for (; ls != lsend; ++ls)
    {
        LightNode *lightNode = getLightNodeForId(ls->lid());

        if (lightNode && lightNode->isAvailable() && lightNode->state() == LightNode::StateNormal)
        {
            bool changed = false;

            if (lightNode->hasColor() && ls->colorloopActive() && lightNode->isColorLoopActive() != ls->colorloopActive())
            {
                TaskItem task2;
                task2.lightNode = lightNode;
                task2.req.dstAddress() = task2.lightNode->address();
                task2.req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
                task2.req.setDstEndpoint(task2.lightNode->haEndpoint().endpoint());
                task2.req.setSrcEndpoint(getSrcEndpoint(task2.lightNode, task2.req));
                task2.req.setDstAddressMode(deCONZ::ApsExtAddress);

                lightNode->setColorLoopActive(true);
                lightNode->setColorLoopSpeed(ls->colorloopTime());
                addTaskSetColorLoop(task2, true, ls->colorloopTime());
                changed = true;
            }

// #if 0 // TODO let pollManger handle updates
            ResourceItem *item = lightNode->item(RStateOn);
            if (item && item->toBool() != ls->on())
            {
                item->setValue(ls->on());
                Event e(RLights, RStateOn, lightNode->id(), item);
                enqueueEvent(e);
                changed = true;
                groupOnChanged = true;
            }

            item = lightNode->item(RStateBri);
            if (item && ls->bri() != item->toNumber())
            {
                item->setValue(ls->bri());
                Event e(RLights, RStateBri, lightNode->id(), item);
                enqueueEvent(e);
                changed = true;
                groupBriChanged = true;
            }

            item = lightNode->item(RStateColorMode);
            if (item)
            {
                if (ls->colorMode() != item->toString())
                {
                    item->setValue(ls->colorMode());
                    Event e(RLights, RStateColorMode, lightNode->id());
                    enqueueEvent(e);
                    changed = true;
                    groupColorModeChanged = true;
                }

                if (ls->colorMode() == QLatin1String("xy"))
                {
                    item = lightNode->item(RStateX);
                    if (item && ls->x() != item->toNumber())
                    {
                        item->setValue(ls->x());
                        Event e(RLights, RStateX, lightNode->id(), item);
                        enqueueEvent(e);
                        changed = true;
                    }
                    item = lightNode->item(RStateY);
                    if (item && ls->y() != item->toNumber())
                    {
                        item->setValue(ls->y());
                        Event e(RLights, RStateY, lightNode->id(), item);
                        enqueueEvent(e);
                        changed = true;
                    }
                }
                else if(ls->colorMode() == QLatin1String("ct"))
                {
                    item = lightNode->item(RStateCt);
                    if (item && ls->colorTemperature() != item->toNumber())
                    {
                        item->setValue(ls->colorTemperature());
                        Event e(RLights, RStateCt, lightNode->id(), item);
                        enqueueEvent(e);
                        changed = true;
                        groupCtChanged = true;
                    }
                }
                else if (ls->colorMode() == QLatin1String("hs"))
                {
                    item = lightNode->item(RStateHue);
                    if (item && ls->enhancedHue() != item->toNumber())
                    {
                        item->setValue(ls->enhancedHue());
                        Event e(RLights, RStateHue, lightNode->id(), item);
                        enqueueEvent(e);
                        changed = true;
                        groupHueSatChanged = true;
                    }

                    item = lightNode->item(RStateSat);
                    if (item && ls->saturation() != item->toNumber())
                    {
                        item->setValue(ls->saturation());
                        Event e(RLights, RStateSat, lightNode->id(), item);
                        enqueueEvent(e);
                        changed = true;
                        groupHueSatChanged = true;
                    }
                }
            }
// #endif
            if (changed)
            {
                updateLightEtag(lightNode);
            }
        }
    }
    if (groupOnChanged || groupBriChanged || groupHueSatChanged || groupCtChanged || groupColorModeChanged)
    {
        if (groupOn && !group->isOn())
        {
            group->setIsOn(true);
            updateGroupEtag(group);
        }
        // recalc other group parameter in webapp
    }

    updateEtag(gwConfigEtag);

    return true;
}
