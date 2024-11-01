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

    // PUT /api/<apikey>/hue-scenes/groups/<group_id>/scenes/<scene_id>/recall
    else if ((req.path.size() == 8) && (req.hdr.method() == "PUT")  && (req.path[5] == "scenes") && (req.path[7] == "play"))
    {
        return playHueDynamicScene(req, rsp);
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

    // FIXME: Collapse into 'recallHueDynamicScene'
    if (!callScene(group, sceneId))
    {
        rsp.httpStatus = HttpStatusServiceUnavailable;
        rsp.list.append(errorToMap(ERR_BRIDGE_BUSY, QString("/hue-scenes/groups/%1/scenes/%2").arg(gid).arg(sid), QString("gateway busy")));
        return REQ_READY_SEND;
    }

    // FIXME: Rename into 'recallHueDynamicScene'
    // FIXME: Validate contents of 'map'
    if (!addTaskPlayHueDynamicScene(taskRef, group->address(), scene->id, map))
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

    // FIXME: Remove unnecessary check
    //        This call is meant to check the state of the lights have changed to match the
    //        recalled scene. Apparently, IKEA lights tend to misbehave. This might not be
    //        needed with Philips Hue lights that support effects.
    // TODO: Verify that the group and light's states update after the recall
    //recallSceneCheckGroupChanges(this, group, scene);

    updateEtag(gwConfigEtag);

    rspItemState["id"] = sid;
    rspItem["success"] = rspItemState;
    rsp.list.append(rspItem);
    rsp.httpStatus = HttpStatusOk;

    processTasks();

    return REQ_READY_SEND;
}
