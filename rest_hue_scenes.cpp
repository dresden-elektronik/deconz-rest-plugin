/*
 * Handle Hue-specific Dynamic Scenes.
 */

#include "de_web_plugin.h"
#include "de_web_plugin_private.h"

/*! Scenes REST API broker.
    \param req - request data
    \param rsp - response data
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::handleHueScenesApi(const ApiRequest &req, ApiResponse &rsp)
{
    if (req.path[2] != QLatin1String("hue-scenes"))
    {
        return REQ_NOT_HANDLED;
    }

    // PUT /api/<apikey>/hue-scenes/groups/<group_id>/scenes/<scene_id>/play
    else if ((req.path.size() == 8) && (req.hdr.method() == "PUT")  && (req.path[5] == "scenes") && (req.path[7] == "play"))
    {
        return playHueDynamicScene(req, rsp);
    }
    // PUT, PATCH /api/<apikey>/hue-scenes/groups/<group_id>/scenes/<scene_id>/lights/<light_id>/state
    else if ((req.path.size() == 10) && (req.hdr.method() == "PUT" || req.hdr.method() == "PATCH")  && (req.path[5] == "scenes") && (req.path[7] == "lights") && (req.path[9] == "state"))
    {
        return modifyHueScene(req, rsp);
    }

    return REQ_NOT_HANDLED;
}

/*! PUT /api/<apikey>/groups/<group_id>/scenes/<scene_id>/play
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::playHueDynamicScene(const ApiRequest &req, ApiResponse &rsp)
{
    bool ok;
    QVariantMap rspItem;
    QVariantMap rspItemState;
    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();
    const QString &gid = req.path[4];
    const QString &sid = req.path[6];
    Scene *scene = nullptr;
    Group *group = getGroupForId(gid);
    rsp.httpStatus = HttpStatusOk;

    if (req.sock)
    {
        userActivity();
    }

    if (!isInNetwork())
    {
        rsp.list.append(errorToMap(ERR_NOT_CONNECTED, QString("/hue-scenes/groups/%1/scenes/%2").arg(gid).arg(sid), "not connected"));
        rsp.httpStatus = HttpStatusServiceUnavailable;
        return REQ_READY_SEND;
    }

    if (!group || (group->state() != Group::StateNormal))
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/hue-scenes/groups/%1/scenes/%2").arg(gid).arg(sid), QString("resource, /groups/%1/scenes/%2, not available").arg(gid).arg(sid)));
        return REQ_READY_SEND;
    }

    // check if scene exists
    uint8_t sceneId = 0;
    ok = false;
    if (sid == QLatin1String("next") || sid == QLatin1String("prev"))
    {
        ResourceItem *item = group->item(RActionScene);
        DBG_Assert(item != 0);
        uint lastSceneId = 0;
        if (item && !item->toString().isEmpty())
        {
            lastSceneId = item->toString().toUInt(&ok);
        }

        int idx = -1;
        std::vector<quint8> scenes; // available scenes

        for (const Scene &s : group->scenes)
        {
            if (s.state != Scene::StateNormal)
            {
                continue;
            }

            if (lastSceneId == s.id)
            {
                idx = scenes.size(); // remember current index
            }
            scenes.emplace_back(s.id);
        }

        if (scenes.size() == 1)
        {
            ok = true;
            sceneId = scenes[0];
        }
        else if (scenes.size() > 1)
        {
            ok = true;
            if (idx == -1) // not found
            {
                idx = 0; // use first
            }
            else if (sid[0] == 'p') // prev
            {
                if (idx > 0)  { idx--; }
                else          { idx = scenes.size() - 1; } // jump to last scene
            }
            else // next
            {
                if (idx < int(scenes.size() - 1)) { idx++; }
                else  { idx = 0; } // jump to first scene
            }
            DBG_Assert(idx >= 0 && idx < int(scenes.size()));
            sceneId = scenes[idx];
        }
        // else ok == false
    }
    else
    {
        sceneId = sid.toUInt(&ok);
    }

    scene = ok ? group->getScene(sceneId) : nullptr;

    if (!scene || (scene->state != Scene::StateNormal))
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/hue-scenes/groups/%1/scenes/%2").arg(gid).arg(sid), QString("resource, /groups/%1/scenes/%2, not available").arg(gid).arg(sid)));
        return REQ_READY_SEND;
    }

    TaskItem taskRef;
    taskRef.req.setDstEndpoint(0xFF);
    taskRef.req.setDstAddressMode(deCONZ::ApsGroupAddress);
    taskRef.req.dstAddress().setGroup(group->address());
    taskRef.req.setSrcEndpoint(getSrcEndpoint(0, taskRef.req));

    if (!callScene(group, sceneId))
    {
        rsp.httpStatus = HttpStatusServiceUnavailable;
        rsp.list.append(errorToMap(ERR_BRIDGE_BUSY, QString("/hue-scenes/groups/%1/scenes/%2").arg(gid).arg(sid), QString("gateway busy")));
        return REQ_READY_SEND;
    }

    QList<QString> validatedParameters;
    if (!validateHueDynamicScenePalette(rsp, scene, map, validatedParameters))
    {
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    if (!addTaskHueDynamicSceneRecall(taskRef, group->address(), scene->id, map))
    {
        rsp.httpStatus = HttpStatusServiceUnavailable;
        rsp.list.append(errorToMap(ERR_BRIDGE_BUSY, QString("/hue-scenes/groups/%1/scenes/%2").arg(gid).arg(sid), QString("gateway busy")));
        return REQ_READY_SEND;
    }

    {
        const QString scid = QString::number(sceneId);
        ResourceItem *item = group->item(RActionScene);
        if (item && item->toString() != scid)
        {
            item->setValue(scid);
            updateGroupEtag(group);
            Event e(RGroups, RActionScene, group->id(), item);
            enqueueEvent(e);
        }
    }

    // TODO: Verify that the group's and lights' states update after the recall
    //       This call is meant to check the state of the lights have changed to match the
    //       recalled scene. Apparently, IKEA lights tend to misbehave. This might not be
    //       needed with Philips Hue lights that support effects.
    //recallSceneCheckGroupChanges(this, group, scene);

    updateEtag(gwConfigEtag);

    rspItemState["id"] = sid;
    rspItem["success"] = rspItemState;
    rsp.list.append(rspItem);
    rsp.httpStatus = HttpStatusOk;

    processTasks();

    return REQ_READY_SEND;
}

/*! PUT, PATCH /api/<apikey>/hue-scenes/groups/<group_id>/scenes/<scene_id>/lights/<light_id>/state
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::modifyHueScene(const ApiRequest &req, ApiResponse &rsp)
{
    bool ok;
    QVariantMap rspItem;
    QVariantMap rspItemState;
    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();
    QString gid = req.path[4];
    QString sid = req.path[6];
    QString lid = req.path[8];
    Group *group = getGroupForId(gid);
    Scene scene;
    LightNode *light = getLightNodeForId(lid);
    rsp.httpStatus = HttpStatusOk;

    userActivity();

    if (!isInNetwork())
    {
        rsp.list.append(errorToMap(ERR_NOT_CONNECTED, QString("/hue-scenes/groups/%1/scenes/%2/lights/%3/state").arg(gid).arg(sid).arg(lid), "Not connected"));
        rsp.httpStatus = HttpStatusServiceUnavailable;
        return REQ_READY_SEND;
    }

    if (!ok || map.isEmpty())
    {
        rsp.list.append(errorToMap(ERR_INVALID_JSON, QString("/hue-scenes/groups/%1/scenes/%2/lights/%3/state").arg(gid).arg(sid).arg(lid), QString("body contains invalid JSON")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    if (!group || (group->state() == Group::StateDeleted))
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/hue-scenes/groups/%1/scenes/%2/lights/%3/state").arg(gid).arg(sid).arg(lid), QString("resource, /groups/%1, not available").arg(gid)));
        return REQ_READY_SEND;
    }

    if (!light || (light->state() == LightNode::StateDeleted) || !light->isAvailable())
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/hue-scenes/groups/%1/scenes/%2/lights/%3/state").arg(gid).arg(sid).arg(lid), QString("resource, /lights/%1, not available").arg(lid)));
        return REQ_READY_SEND;
    }

    std::vector<Scene>::iterator i = group->scenes.begin();
    std::vector<Scene>::iterator end = group->scenes.end();

    bool foundScene = false;
    bool foundLightState = false;

    for ( ;i != end; ++i)
    {
        if (QString::number(i->id) == sid && i->state != Scene::StateDeleted)
        {
            foundScene = true;
            scene = *i;

            std::vector<LightState>::iterator l = i->lights().begin();
            std::vector<LightState>::iterator lend = i->lights().end();

            for ( ;l != lend; ++l)
            {
                if (l->lid() == lid)
                {
                    foundLightState = true;
                    break;
                }
            }

            break;
        }
    }

    if (!foundScene)
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/groups/%1/scenes/%2/lights/%3/state").arg(gid).arg(sid).arg(lid), QString("resource, /scenes/%1, not available").arg(sid)));
        return REQ_READY_SEND;
    }

    if (!foundLightState)
    {
        rsp.httpStatus = HttpStatusBadRequest;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/groups/%1/scenes/%2/lights/%3/state").arg(gid).arg(sid).arg(lid), QString("Light %1 is not available in scene.").arg(lid)));
        return REQ_READY_SEND;
    }

    QList<QString> validatedParameters;
    if (!validateHueLightState(rsp, light, map, validatedParameters))
    {
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    TaskItem taskRef;
    taskRef.lightNode = getLightNodeForId(lid);
    taskRef.req.dstAddress() = taskRef.lightNode->address();
    taskRef.req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
    taskRef.req.setDstEndpoint(taskRef.lightNode->haEndpoint().endpoint());
    taskRef.req.setSrcEndpoint(getSrcEndpoint(taskRef.lightNode, taskRef.req));
    taskRef.req.setDstAddressMode(deCONZ::ApsExtAddress);

    if (!addTaskHueManufacturerSpecificAddScene(taskRef, group->address(), scene.id, map))
    {
        rsp.httpStatus = HttpStatusServiceUnavailable;
        rsp.list.append(errorToMap(ERR_BRIDGE_BUSY, QString("/hue-scenes/groups/%1/scenes/%2/lights/%3/state").arg(gid).arg(sid).arg(lid), QString("gateway busy")));
        return REQ_READY_SEND;
    }

    updateGroupEtag(group);

    queSaveDb(DB_SCENES, DB_SHORT_SAVE_DELAY);

    rspItemState["id"] = sid;
    rspItem["success"] = rspItemState;
    rsp.list.append(rspItem);
    rsp.httpStatus = HttpStatusOk;

    return REQ_READY_SEND;
}
