/*
 * Copyright (C) 2013 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QString>
#include <QTextCodec>
#include <QTcpSocket>
#include <QHttpRequestHeader>
#include <QVariantMap>
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "json.h"

/*! Groups and scenes REST API broker.
    \param req - request data
    \param rsp - response data
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::handleGroupsApi(ApiRequest &req, ApiResponse &rsp)
{
    if (req.path[2] != "groups")
    {
        return REQ_NOT_HANDLED;
    }

    if(!checkApikeyAuthentification(req, rsp))
    {
        return REQ_READY_SEND;
    }

    // GET /api/<apikey>/groups
    if ((req.path.size() == 3) && (req.hdr.method() == "GET"))
    {
        return getAllGroups(req, rsp);
    }
    // POST /api/<apikey>/groups
    else if ((req.path.size() == 3) && (req.hdr.method() == "POST"))
    {
        return createGroup(req, rsp);
    }
    // GET /api/<apikey>/groups/<id>
    else if ((req.path.size() == 4) && (req.hdr.method() == "GET"))
    {
        return getGroupAttributes(req, rsp);
    }
    // PUT /api/<apikey>/groups/<id>
    else if ((req.path.size() == 4) && (req.hdr.method() == "PUT"))
    {
        return setGroupAttributes(req, rsp);
    }
    // PUT /api/<apikey>/groups/<id>/action
    else if ((req.path.size() == 5) && (req.hdr.method() == "PUT") && (req.path[4] == "action"))
    {
        return setGroupState(req, rsp);
    }
    // DELETE /api/<apikey>/groups/<id>
    else if ((req.path.size() == 4) && (req.hdr.method() == "DELETE"))
    {
        return deleteGroup(req, rsp);
    }
    // POST /api/<apikey>/groups/<group_id>/scenes
    else if ((req.path.size() == 5) && (req.hdr.method() == "POST")  && (req.path[4] == "scenes"))
    {
        return createScene(req, rsp);
    }
    // GET /api/<apikey>/groups/<group_id>/scenes
    else if ((req.path.size() == 5) && (req.hdr.method() == "GET")  && (req.path[4] == "scenes"))
    {
        return getAllScenes(req, rsp);
    }
    // GET /api/<apikey>/groups/<group_id>/scenes/<scene_id>
    else if ((req.path.size() == 6) && (req.hdr.method() == "GET")  && (req.path[4] == "scenes"))
    {
        return getSceneAttributes(req, rsp);
    }
    // PUT /api/<apikey>/groups/<group_id>/scenes/<scene_id>
    else if ((req.path.size() == 6) && (req.hdr.method() == "PUT")  && (req.path[4] == "scenes"))
    {
        return setSceneAttributes(req, rsp);
    }
    // PUT /api/<apikey>/groups/<group_id>/scenes/<scene_id>/store
    else if ((req.path.size() == 7) && (req.hdr.method() == "PUT")  && (req.path[4] == "scenes") && (req.path[6] == "store"))
    {
        return storeScene(req, rsp);
    }
    // PUT /api/<apikey>/groups/<group_id>/scenes/<scene_id>/recall
    else if ((req.path.size() == 7) && (req.hdr.method() == "PUT")  && (req.path[4] == "scenes") && (req.path[6] == "recall"))
    {
        return recallScene(req, rsp);
    }
    // DELETE /api/<apikey>/groups/<group_id>/scenes/<scene_id>
    else if ((req.path.size() == 6) && (req.hdr.method() == "DELETE")  && (req.path[4] == "scenes"))
    {
        return deleteScene(req, rsp);
    }

    return REQ_NOT_HANDLED;
}

/*! GET /api/<apikey>/groups
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::getAllGroups(const ApiRequest &req, ApiResponse &rsp)
{
    Q_UNUSED(req);
    rsp.httpStatus = HttpStatusOk;

    std::vector<Group>::const_iterator i = groups.begin();
    std::vector<Group>::const_iterator end = groups.end();

    for (; i != end; ++i)
    {
        // ignore deleted groups
        if (i->state() == Group::StateDeleted)
        {
            continue;
        }

        if (i->address() != 0) // don't return special group 0
        {
            QVariantMap mnode;

            mnode["name"] = i->name();
            QString etag = i->etag;
            etag.remove('"'); // no quotes allowed in string
            mnode["etag"] = etag;
            rsp.map[i->id()] = mnode;
        }
    }

    if (rsp.map.isEmpty())
    {
        rsp.str = "{}"; // return empty object
    }

    return REQ_READY_SEND;
}

/*! POST /api/<apikey>/groups
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
    \note currently not in Philips API 1.0
 */
int DeRestPluginPrivate::createGroup(const ApiRequest &req, ApiResponse &rsp)
{
    rsp.httpStatus = HttpStatusOk;

    bool ok;
    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();

    rsp.httpStatus = HttpStatusOk;

    userActivity();

    if (!ok || map.isEmpty())
    {
        rsp.list.append(errorToMap(ERR_INVALID_JSON, QString("/groups"), QString("body contains invalid JSON")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    // name
    if (map.contains("name"))
    {
        QString name = map["name"].toString();

        if (map["name"].type() == QVariant::String)
        {
            QVariantMap rspItem;
            QVariantMap rspItemState;
            Group *group1 = getGroupForName(name);

            // already exist? .. do nothing
            if (group1)
            {
                // If a group with the same name was deleted before
                // a new group with a different id will be created
                // TODO: same behavoir as for creating duplicated scenes
                if (group1->state() != Group::StateDeleted)
                {
                    rspItemState["id"] = group1->id();
                    rspItem["success"] = rspItemState;
                    rsp.list.append(rspItem);
                    rsp.httpStatus = HttpStatusOk;
                    return REQ_READY_SEND;
                }
                else
                {
                    DBG_Printf(DBG_INFO, "create group with same name as prior deleted group. but use different id\n");
                }
            }

            // does not exist, create group
            Group group;

            // create a new group id
            group.setAddress(1);

            do {
                ok = true;
                std::vector<Group>::iterator i = groups.begin();
                std::vector<Group>::iterator end = groups.end();

                for (; i != end; ++i)
                {
                    if (i->address() == group.address())
                    {
                        group.setAddress(i->address() + 1);
                        ok = false;
                    }
                }
            } while (!ok);

            group.setName(name);
            group.colorX = 0;
            group.colorY = 0;
            group.setIsOn(false);
            group.level = 128;
            group.hue = 0;
            group.hueReal = 0.0f;
            group.sat = 128;
            updateEtag(group.etag);
            updateEtag(gwConfigEtag);
            groups.push_back(group);
            queSaveDb(DB_GROUPS, DB_SHORT_SAVE_DELAY);

            rspItemState["id"] = group.id();
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);
            rsp.httpStatus = HttpStatusOk;
            return REQ_READY_SEND;
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/groups"), QString("invalid value, %1, for parameter, name").arg(name)));
            rsp.httpStatus = HttpStatusBadRequest;
        }
    }
    else
    {
        rsp.list.append(errorToMap(ERR_MISSING_PARAMETER, QString("/groups"), QString("missing parameters in body")));
        rsp.httpStatus = HttpStatusBadRequest;
    }

    return REQ_READY_SEND;
}

/*! GET /api/<apikey>/groups/<id>
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::getGroupAttributes(const ApiRequest &req, ApiResponse &rsp)
{
    QString id = req.path[3];
    Group *group = getGroupForId(id);
    rsp.httpStatus = HttpStatusOk;

    if (!group)
    {
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/groups/%1").arg(id), QString("resource, /groups/%1, not available").arg(id)));
        rsp.httpStatus = HttpStatusNotFound;
        return REQ_READY_SEND;
    }

    // handle ETag
    if (req.hdr.hasKey("If-None-Match"))
    {
        QString etag = req.hdr.value("If-None-Match");

        if (group->etag == etag)
        {
            rsp.httpStatus = HttpStatusNotModified;
            rsp.etag = etag;
            return REQ_READY_SEND;
        }
    }

    QVariantMap action;
    QVariantList scenes;

    action["on"] = group->isOn();
    action["hue"] = (double)((uint16_t)(group->hueReal * 65535));
    action["effect"] = "none"; // TODO
    action["bri"] = (double)group->level;
    action["sat"] = (double)group->sat;
    action["ct"] = (double)500; // TODO
    QVariantList xy;

    // sanity for colorX
    if (group->colorX > 65279)
    {
        group->colorX = 65279;
    }
    // sanity for colorY
    if (group->colorY > 65279)
    {
        group->colorY = 65279;
    }
    double x = (double)group->colorX / 65279.0f; // normalize 0 .. 65279 to 0 .. 1
    double y = (double)group->colorY / 65279.0f; // normalize 0 .. 65279 to 0 .. 1
    xy.append(x);
    xy.append(y);
    action["xy"] = xy;

    rsp.map["id"] = group->id();
    rsp.map["name"] = group->name();
    QString etag = group->etag;
    etag.remove('"'); // no quotes allowed in string
    rsp.map["etag"] = etag;
    rsp.map["action"] = action;

    // append lights which are known members in this group
    QVariantList lights;
    std::vector<LightNode>::const_iterator i = nodes.begin();
    std::vector<LightNode>::const_iterator end = nodes.end();

    for (; i != end; ++i)
    {
        std::vector<GroupInfo>::const_iterator ii = i->groups().begin();
        std::vector<GroupInfo>::const_iterator eend = i->groups().end();

        for (; ii != eend; ++ii)
        {
            if (ii->id == group->address())
            {
                if (ii->state == GroupInfo::StateInGroup)
                {
                    lights.append(i->id());
                }
                break;
            }
        }
    }

    rsp.map["lights"] = lights;

    std::vector<Scene>::const_iterator si = group->scenes.begin();
    std::vector<Scene>::const_iterator send = group->scenes.end();

    for ( ;si != send; ++si)
    {
        if (si->state != Scene::StateDeleted)
        {
            QVariantMap scene;
            QString sid = QString::number(si->id);
            scene["id"] = sid;
            scene["name"] = si->name;

            scenes.append(scene);
        }
    }

    rsp.map["scenes"] = scenes;

    return REQ_READY_SEND;
}

/*! PUT /api/<apikey>/groups/<id>
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::setGroupAttributes(const ApiRequest &req, ApiResponse &rsp)
{
    rsp.httpStatus = HttpStatusOk;

    bool ok;
    bool changed = false;
    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();
    QString id = req.path[3];
    Group *group = getGroupForId(id);
    rsp.httpStatus = HttpStatusOk;

    userActivity();

    if (!ok || map.isEmpty())
    {
        rsp.list.append(errorToMap(ERR_INVALID_JSON, QString("/groups/%1").arg(id), QString("body contains invalid JSON")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    if (!group)
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/groups/%1").arg(id), QString("resource, /groups/%1, not available").arg(id)));
        return REQ_READY_SEND;
    }

    // name
    if (map.contains("name"))
    {
        QString name = map["name"].toString();

        if (map["name"].type() == QVariant::String)
        {
            if (name.size() <= 32)
            {
                QVariantMap rspItem;
                QVariantMap rspItemState;
                rspItemState[QString("/groups/%1/name").arg(id)] = name;
                rspItem["success"] = rspItemState;
                rsp.list.append(rspItem);

                if (group->name() != name)
                {
                    group->setName(name);
                    changed = true;
                    queSaveDb(DB_GROUPS, DB_SHORT_SAVE_DELAY);
                }
            }
            else
            {
                rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/groups/%1").arg(id), QString("invalid value, %1, for parameter, /groups/%2/name").arg(name).arg(id)));
                rsp.httpStatus = HttpStatusBadRequest;
            }
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/groups/%1").arg(id), QString("invalid value, %1, for parameter, /groups/%2/name").arg(name).arg(id)));
            rsp.httpStatus = HttpStatusBadRequest;
        }
    }

    // check optional parameter lights
    if (map.contains("lights"))
    {
        QVariantList lights = map["lights"].toList();

        // for each node in the list send a add to group request (unicast)
        // note: nodes which are currently switched off will not be added to the group
        QVariantList::iterator i = lights.begin();
        QVariantList::iterator end = lights.end();

        QStringList lids;

        ok = true;

        for (;i != end; ++i)
        {
            if (i->type() == QVariant::String)
            {
                QString lid = i->toString();
                lids.append(lid);
                DBG_Printf(DBG_INFO, "group %u member %u\n", group->address(), lid.toUInt());

                LightNode *lightNode = getLightNodeForId(lid);

                if (lightNode)
                {
                    GroupInfo *groupInfo = getGroupInfo(lightNode, group->address());

                    if (!groupInfo)
                    {
                        groupInfo = createGroupInfo(lightNode, group->address());
                        changed = true;
                    }

                    DBG_Assert(groupInfo != 0);
                    if (groupInfo)
                    {
                        groupInfo->actions &= ~GroupInfo::ActionRemoveFromGroup; // sanity
                        groupInfo->actions |= GroupInfo::ActionAddToGroup;
                        groupInfo->state = GroupInfo::StateInGroup;
                    }
                }
                else
                {
                    ok = false;
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/groups/%1/lights").arg(id), QString(" device, %1, could not be added to group. Device does not exist.").arg(lid)));
                }
            }
            else
            {
                ok = false;
                rsp.httpStatus = HttpStatusBadRequest;
                // TODO: return error
            }
        }

        if (ok)
        {
            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/groups/%1/lights").arg(id)] = map["lights"];
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);

            // for each node which are currently in the group but not in the list send a remove group command (unicast)
            // note: nodes which are currently switched off will not be removed from the group
            std::vector<LightNode>::iterator j = nodes.begin();
            std::vector<LightNode>::iterator jend = nodes.end();
            for (; j != jend; ++j)
            {
                if (lids.contains(j->id()))
                {
                    continue;
                }

                std::vector<GroupInfo>::iterator k = j->groups().begin();
                std::vector<GroupInfo>::iterator kend = j->groups().end();

                for (; k != kend; ++k)
                {
                    if (k->id == group->address())
                    {
                        k->actions &= ~GroupInfo::ActionAddToGroup; // sanity
                        k->actions |= GroupInfo::ActionRemoveFromGroup;
                        k->state = GroupInfo::StateNotInGroup;
                        changed = true;
                    }
                }
            }
        }
    }

    if (changed)
    {
        updateEtag(group->etag);
        updateEtag(gwConfigEtag);
    }

    rsp.etag = group->etag;

    return REQ_READY_SEND;
}

/*! PUT /api/<apikey>/groups/<id>/action
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::setGroupState(const ApiRequest &req, ApiResponse &rsp)
{
    TaskItem task;
    QString id = req.path[3];
    Group *group = getGroupForId(id);
    uint hue = UINT_MAX;
    uint sat = UINT_MAX;

    userActivity();

    if (!isInNetwork())
    {
        rsp.list.append(errorToMap(ERR_NOT_CONNECTED, QString("/groups/%1/action").arg(id), "Not connected"));
        rsp.httpStatus = HttpStatusServiceUnavailable;
        return REQ_READY_SEND;
    }

    if (!group || (group->state() == Group::StateDeleted))
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/groups/%1").arg(id), QString("resource, /groups/%1, not available").arg(id)));
        return REQ_READY_SEND;
    }

    rsp.httpStatus = HttpStatusOk;

    // set destination parameters
    if (id == "0")
    {
        // use a broadcast
        task.req.dstAddress().setNwk(0xFFFF);
        task.req.dstAddress().setGroup(0); // taskToLocal() needs this
        task.req.setDstAddressMode(deCONZ::ApsNwkAddress);
    }
    else
    {
        task.req.dstAddress().setGroup(group->address());
        task.req.setDstAddressMode(deCONZ::ApsGroupAddress);
    }
    task.req.setDstEndpoint(0xFF); // broadcast endpoint
    task.req.setSrcEndpoint(getSrcEndpoint(0, task.req));

    bool ok;
    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();

    if (!ok || map.isEmpty())
    {
        rsp.list.append(errorToMap(ERR_INVALID_JSON, QString("/groups/%1/action").arg(id), QString("body contains invalid JSON")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    // transition time
    if (map.contains("transitionTime"))
    {
        uint tt = map["transitionTime"].toUInt(&ok);

        if (ok && tt < 0xFFFFUL)
        {
            task.transitionTime = tt;
        }
    }

    // on/off
    if (map.contains("on")) // TODO ignore if (on == true) && (bri)
    {
        if (map["on"].type() == QVariant::Bool)
        {
            bool on = map["on"].toBool();
            if (map.contains("bri") ||
                addTaskSetOnOff(task, on)) // onOff task only if no bri is given
            {
                QVariantMap rspItem;
                QVariantMap rspItemState;
                rspItemState[QString("/groups/%1/action/on").arg(id)] = on;
                rspItem["success"] = rspItemState;
                rsp.list.append(rspItem);
                taskToLocalData(task);
            }
            else
            {
                rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/groups/%1").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
            }
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/groups/%1/action/on").arg(id), QString("invalid value, %1, for parameter, on").arg(map["on"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }

    // brightness
    if (map.contains("bri"))
    {
        uint bri = map["bri"].toUInt(&ok);

        if (ok && (map["bri"].type() == QVariant::Double) && (bri < 256))
        {
            if (addTaskSetBrightness(task, bri, map.contains("on")))
            {
                QVariantMap rspItem;
                QVariantMap rspItemState;
                rspItemState[QString("/groups/%1/action/bri").arg(id)] = map["bri"];
                rspItem["success"] = rspItemState;
                rsp.list.append(rspItem);
                taskToLocalData(task);
            }
            else
            {
                rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/groups/%1").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
            }
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/groups/%1/action/bri").arg(id), QString("invalid value, %1, for parameter, bri").arg(map["bri"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }

    // hue
    if (map.contains("hue")) // TODO check if map has no xy, ct ...
    {
        uint hue2 = map["hue"].toUInt(&ok);

        if (ok && (map["hue"].type() == QVariant::Double) && (hue2 < (MAX_ENHANCED_HUE + 1)))
        {
            hue = hue2;
            { // TODO: this is needed if saturation is set and addTaskSetEnhancedHue() will not be called
                task.hueReal = (double)hue / (360.0f * 182.04444f);

                if (task.hueReal < 0.0f)
                {
                    task.hueReal = 0.0f;
                }
                else if (task.hueReal > 1.0f)
                {
                    task.hueReal = 1.0f;
                }
                task.hue = task.hueReal * 254.0f;
                task.enhancedHue = hue;
                task.taskType = TaskSetEnhancedHue;
                taskToLocalData(task);
            }

            if (map.contains("sat") || // merge later to set hue and saturation
                addTaskSetEnhancedHue(task, hue)) // will only be evaluated if no sat is set
            {
                QVariantMap rspItem;
                QVariantMap rspItemState;
                rspItemState[QString("/groups/%1/action/hue").arg(id)] = map["hue"];
                rspItem["success"] = rspItemState;
                rsp.list.append(rspItem);
            }
            else
            {
                rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/groups/%1").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
            }
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/groups/%1/action/hue").arg(id), QString("invalid value, %1, for parameter, hue").arg(map["hue"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }

    // saturation
    if (map.contains("sat")) // TODO check if map has no xy, ct ...
    {
        uint sat2 = map["sat"].toUInt(&ok);

        if (ok && (map["sat"].type() == QVariant::Double) && (sat2 < 256))
        {
            if (sat2 == 255)
            {
                sat2 = 254; // max valid value for level attribute
            }

            sat = sat2;
            task.sat = sat;
            task.taskType = TaskSetSat;
            taskToLocalData(task);

            if ((map.contains("hue") && (hue != UINT_MAX)) // merge later to set hue and saturation
               || addTaskSetSaturation(task, sat)) // will only be evaluated if no hue is set
            {
                QVariantMap rspItem;
                QVariantMap rspItemState;
                rspItemState[QString("/groups/%1/action/sat").arg(id)] = map["sat"];
                rspItem["success"] = rspItemState;
                rsp.list.append(rspItem);
            }
            else
            {
                rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/groups/%1").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
            }
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/groups/%1/action/sat").arg(id), QString("invalid value, %1, for parameter, sat").arg(map["sat"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }

    // hue and saturation
    if (map.contains("hue") && map.contains("sat"))
    {
        if ((hue != UINT_MAX) && (sat != UINT_MAX))
        {
            // need 8 bit hue
            qreal f = (qreal)hue / 182.04444f;

            f /= 360.0f;

            if (f > 1.0f)
            {
                f = 1.0f;
            }

            hue = f * 254.0f;

            DBG_Printf(DBG_INFO, "hue: %u, sat: %u\n", hue, sat);
            if (!addTaskSetHueAndSaturation(task, hue, sat))
            {
                DBG_Printf(DBG_INFO, "cant send task set hue and saturation\n");
            }
        }
        else
        {
            DBG_Printf(DBG_INFO, "cant merge hue and saturation: invalid value(s) hue: %u, sat: %u\n", hue, sat);
        }
    }

    // xy
    if (map.contains("xy"))
    {
        QVariantList ls = map["xy"].toList();

        if ((ls.size() == 2) && (ls[0].type() == QVariant::Double) && (ls[1].type() == QVariant::Double))
        {
            double x = ls[0].toDouble();
            double y = ls[1].toDouble();

            if ((x < 0.0f) || (x > 1.0f) || (y < 0.0f) || (y > 1.0f))
            {
                rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/groups/%1").arg(id), QString("invalid value, [%1,%2], for parameter, /groups/%3/xy").arg(x).arg(y).arg(id)));
            }
            else if (addTaskSetXyColor(task, x, y))
            {
                QVariantMap rspItem;
                QVariantMap rspItemState;
                rspItemState[QString("/groups/%1/action/xy").arg(id)] = map["xy"];
                rspItem["success"] = rspItemState;
                rsp.list.append(rspItem);
                taskToLocalData(task);
            }
            else
            {
                rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/groups/%1").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
            }
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/groups/%1/action/xy").arg(id), QString("invalid value, %1, for parameter, xy").arg(map["xy"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }

    updateEtag(group->etag);
    updateEtag(gwConfigEtag);
    rsp.etag = group->etag;

    processTasks();
    // TODO: ct, alert, effect

    return REQ_READY_SEND;
}

/*! DELETE /api/<apikey>/groups/<id>
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
    \note currently not in Philips API 1.0
 */
int DeRestPluginPrivate::deleteGroup(const ApiRequest &req, ApiResponse &rsp)
{
    QString id = req.path[3];
    Group *group = getGroupForId(id);

    userActivity();

    if (!group || (group->state() == Group::StateDeleted))
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/groups/%1").arg(id), QString("resource, /groups/%1, not available").arg(id)));
        return REQ_READY_SEND;
    }

    group->setState(Group::StateDeleted);

    // remove any known scene
    group->scenes.clear();

    QVariantMap rspItem;
    QVariantMap rspItemState;
    rspItemState["id"] = id;
    rspItem["success"] = rspItemState;
    rsp.list.append(rspItem);
    rsp.httpStatus = HttpStatusOk;

    queSaveDb(DB_GROUPS, DB_SHORT_SAVE_DELAY);

    // for each node which is part of this group send a remove group request (will be unicast)
    // note: nodes which are curently switched off will not be removed!
    std::vector<LightNode>::iterator i = nodes.begin();
    std::vector<LightNode>::iterator end = nodes.end();

    for (; i != end; ++i)
    {
        GroupInfo *groupInfo = getGroupInfo(&(*i), group->address());

        if (groupInfo)
        {
            groupInfo->actions &= ~GroupInfo::ActionAddToGroup; // sanity
            groupInfo->actions |= GroupInfo::ActionRemoveFromGroup;
            groupInfo->state = GroupInfo::StateNotInGroup;
        }
    }

    updateEtag(gwConfigEtag);
    rsp.httpStatus = HttpStatusOk;

    return REQ_READY_SEND;
}

/*! Put all parameters in a map for later json serialization.
    \return true - on success
            false - on error
 */
bool DeRestPluginPrivate::groupToMap(const Group *group, QVariantMap &map)
{
    if (!group)
    {
        return false;
    }

    QVariantMap action;
    action["hue"] = (double)((uint16_t)(group->hueReal * 65535));
    action["on"] = group->isOn();
    action["effect"] = "none"; // TODO
    action["bri"] = (double)group->level;
    action["sat"] = (double)group->sat;
    action["ct"] = (double)500; // TODO
    QVariantList xy;
    uint16_t colorX = group->colorX;
    uint16_t colorY = group->colorY;
    // sanity for colorX
    if (colorX > 65279)
    {
        colorX = 65279;
    }
    // sanity for colorY
    if (colorY > 65279)
    {
        colorY = 65279;
    }
    double x = (double)colorX / 65279.0f; // normalize 0 .. 65279 to 0 .. 1
    double y = (double)colorY / 65279.0f; // normalize 0 .. 65279 to 0 .. 1
    xy.append(x);
    xy.append(y);
    action["xy"] = xy;
    action["colormode"] = "hs"; // TODO
    map["action"] = action;
    map["name"] = group->name();
    QString etag = group->etag;
    etag.remove('"'); // no quotes allowed in string
    map["etag"] = etag;

    // append lights which are known members in this group
    QVariantList lights;
    std::vector<LightNode>::const_iterator i = nodes.begin();
    std::vector<LightNode>::const_iterator end = nodes.end();

    for (; i != end; ++i)
    {
        std::vector<GroupInfo>::const_iterator ii = i->groups().begin();
        std::vector<GroupInfo>::const_iterator eend = i->groups().end();

        for (; ii != eend; ++ii)
        {
            if (ii->id == group->address())
            {
                if (ii->state == GroupInfo::StateInGroup)
                {
                    lights.append(i->id());
                }
                break;
            }
        }
    }

    map["lights"] = lights;

    QVariantList scenes;
    std::vector<Scene>::const_iterator si = group->scenes.begin();
    std::vector<Scene>::const_iterator send = group->scenes.end();

    for ( ;si != send; ++si)
    {
        if (si->state != Scene::StateDeleted)
        {
            QVariantMap scene;
            QString sid = QString::number(si->id);
            scene["id"] = sid;
            scene["name"] = si->name;

            scenes.append(scene);
        }
    }

    map["scenes"] = scenes;

    return true;
}

/*! POST /api/<apikey>/groups/<group_id>/scenes
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::createScene(const ApiRequest &req, ApiResponse &rsp)
{
    bool ok;
    Scene scene;
    QVariantMap rspItem;
    QVariantMap rspItemState;
    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();
    QString id = req.path[3];
    Group *group = getGroupForId(id);
    rsp.httpStatus = HttpStatusOk;

    userActivity();

    if (!isInNetwork())
    {
        rsp.list.append(errorToMap(ERR_NOT_CONNECTED, QString("/groups/%1/scenes").arg(id), "Not connected"));
        rsp.httpStatus = HttpStatusServiceUnavailable;
        return REQ_READY_SEND;
    }

    if (!ok || map.isEmpty())
    {
        rsp.list.append(errorToMap(ERR_INVALID_JSON, QString("/groups/%1").arg(id), QString("body contains invalid JSON")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    if (!group || (group->state() == Group::StateDeleted))
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/groups/%1").arg(id), QString("resource, /groups/%1, not available").arg(id)));
        return REQ_READY_SEND;
    }

    // name
    if (map.contains("name")) // required
    {
        QString name = map["name"].toString();

        if (map["name"].type() == QVariant::String)
        {
            if (name.size() <= 32)
            {
                scene.name = name;

                std::vector<Scene>::const_iterator i = group->scenes.begin();
                std::vector<Scene>::const_iterator end = group->scenes.end();

                for (; i != end; ++i)
                {
                    if ((i->name == name) && (i->state != Scene::StateDeleted))
                    {
                        DBG_Printf(DBG_INFO, "Scene with name %s already exist\n", qPrintable(name));

                        rsp.list.append(errorToMap(ERR_DUPLICATE_EXIST, QString("/groups/%1/scenes").arg(id), QString("resource, /groups/%1/scenes/%2, already exists").arg(id).arg(name)));
                        rsp.httpStatus = HttpStatusBadRequest;
                        return REQ_READY_SEND;
                    }
                }
            }
            else
            {
                rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/groups/%1/scenes/name").arg(id), QString("invalid value, %1, for parameter, /groups/%2/scenes/name").arg(name).arg(id)));
                rsp.httpStatus = HttpStatusBadRequest;
                return REQ_READY_SEND;
            }
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/groups/%1/scenes/name").arg(id), QString("invalid value, %1, for parameter, /groups/%2/scenes/name").arg(name).arg(id)));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }

    // search a unused id
    scene.id = 1;
    do {
        ok = true; // will be false if a scene.id is already used
        std::vector<Scene>::iterator i = group->scenes.begin();
        std::vector<Scene>::iterator end = group->scenes.end();

        for (; i != end; ++i)
        {
            if (i->id == scene.id)
            {
                scene.id++;
                ok = false;
                break;
            }
        }
    } while (!ok);

    scene.groupAddress = group->address();

    if (scene.name.isEmpty())
    {
        scene.name.sprintf("Scene %u", scene.id);
    }
    group->scenes.push_back(scene);
    updateEtag(group->etag);
    updateEtag(gwConfigEtag);
    queSaveDb(DB_SCENES, DB_SHORT_SAVE_DELAY);

    if (!storeScene(group, scene.id))
    {
        rsp.httpStatus = HttpStatusServiceUnavailable;
        rsp.list.append(errorToMap(ERR_BRIDGE_BUSY, QString("/groups/%1/scenes/%2").arg(id).arg(scene.id), QString("gateway busy")));
        return REQ_READY_SEND;
    }


    rspItemState["id"] = QString::number(scene.id);
    rspItem["success"] = rspItemState;
    rsp.list.append(rspItem);
    rsp.httpStatus = HttpStatusOk;

    return REQ_READY_SEND;
}

/*! GET /api/<apikey>/groups/<group_id>/scenes
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::getAllScenes(const ApiRequest &req, ApiResponse &rsp)
{
    QString id = req.path[3];
    Group *group = getGroupForId(id);
    rsp.httpStatus = HttpStatusOk;

    if (!group || (group->state() == Group::StateDeleted))
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/groups/%1").arg(id), QString("resource, /groups/%1, not available").arg(id)));
        return REQ_READY_SEND;
    }

    std::vector<Scene>::const_iterator i = group->scenes.begin();
    std::vector<Scene>::const_iterator end = group->scenes.end();

    for (; i != end; ++i)
    {
        QString sceneId = QString::number(i->id);
        QVariantMap scene;
        scene["name"] = i->name;
        rsp.map[sceneId] = scene;
    }

    return REQ_READY_SEND;
}

/*! GET /api/<apikey>/groups/<group_id>/scenes/<scene_id>
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::getSceneAttributes(const ApiRequest &req, ApiResponse &rsp)
{
    bool ok;
    QString gid = req.path[3];
    QString sid = req.path[5];
    Group *group = getGroupForId(gid);
    rsp.httpStatus = HttpStatusOk;

    if (!group || (group->state() == Group::StateDeleted))
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/groups/%1/scenes/%2").arg(gid).arg(sid), QString("resource, /groups/%1/scenes/%2, not available").arg(gid).arg(sid)));
        return REQ_READY_SEND;
    }

    std::vector<Scene>::const_iterator i = group->scenes.begin();
    std::vector<Scene>::const_iterator end = group->scenes.end();

    uint sceneId = sid.toUInt(&ok);

    if (ok)
    {
        for (; i != end; ++i)
        {
            if ((i->id == sceneId) && (i->state != Scene::StateDeleted))
            {
                rsp.map["name"] = i->name;
                return REQ_READY_SEND;
            }
        }
    }

    rsp.httpStatus = HttpStatusNotFound;
    rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/groups/%1/scenes/%2").arg(gid).arg(sid), QString("resource, /groups/%1/scenes/%2, not available").arg(gid).arg(sid)));
    return REQ_READY_SEND;

}

/*! PUT /api/<apikey>/groups/<group_id>/scenes/<scene_id>
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::setSceneAttributes(const ApiRequest &req, ApiResponse &rsp)
{
    bool ok;
    QString gid = req.path[3];
    QString sid = req.path[5];
    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();
    QVariantMap rspItem;
    QVariantMap rspItemState;
    Group *group = getGroupForId(gid);
    QString name;
    rsp.httpStatus = HttpStatusOk;

    if (!ok || map.isEmpty())
    {
        rsp.list.append(errorToMap(ERR_INVALID_JSON, QString("/groups/%1/scenes/%2").arg(gid).arg(sid), QString("body contains invalid JSON")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    if (!group || (group->state() == Group::StateDeleted))
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/groups/%1/scenes/%2").arg(gid).arg(sid), QString("resource, /groups/%1/scenes/%2, not available").arg(gid).arg(sid)));
        return REQ_READY_SEND;
    }

    // name
    if (map.contains("name")) // optional
    {
        name = map["name"].toString();

        if (map["name"].type() == QVariant::String)
        {
            if (name.size() > MAX_SCENE_NAME_LENGTH)
            {
                rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/groups/%1/scenes/%2/name").arg(gid).arg(sid), QString("invalid value, %1, for parameter, /groups/%2/scenes/%3/name").arg(name).arg(gid).arg(sid)));
                rsp.httpStatus = HttpStatusBadRequest;
                return REQ_READY_SEND;
            }
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/groups/%1/scenes/%2/name").arg(gid).arg(sid), QString("invalid value, %1, for parameter, /groups/%2/scenes/%3/name").arg(name).arg(gid).arg(sid)));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }

    std::vector<Scene>::iterator i = group->scenes.begin();
    std::vector<Scene>::iterator end = group->scenes.end();

    uint sceneId = sid.toUInt(&ok);

    if (ok)
    {
        for (; i != end; ++i)
        {
            if ((i->id == sceneId) && (i->state != Scene::StateDeleted))
            {
                if (!name.isEmpty())
                {
                    if (i->name != name)
                    {
                        i->name = name;
                        updateEtag(group->etag);
                        updateEtag(gwConfigEtag);
                        queSaveDb(DB_SCENES, DB_SHORT_SAVE_DELAY);
                    }

                    rspItemState[QString("/groups/%1/scenes/%2/name").arg(gid).arg(sid)] = name;
                    rspItem["success"] = rspItemState;
                    rsp.list.append(rspItem);
                }

                break;
            }

        }

        return REQ_READY_SEND;
    }

    rsp.httpStatus = HttpStatusNotFound;
    rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/groups/%1/scenes/%2").arg(gid).arg(sid), QString("resource, /groups/%1/scenes/%2, not available").arg(gid).arg(sid)));
    return REQ_READY_SEND;
}

/*! PUT /api/<apikey>/groups/<group_id>/scenes/<scene_id>/store
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::storeScene(const ApiRequest &req, ApiResponse &rsp)
{
    bool ok;
    QVariantMap rspItem;
    QVariantMap rspItemState;
    QString gid = req.path[3];
    QString sid = req.path[5];
    Group *group = getGroupForId(gid);
    rsp.httpStatus = HttpStatusOk;

    userActivity();

    if (!isInNetwork())
    {
        rsp.list.append(errorToMap(ERR_NOT_CONNECTED, QString("/groups/%1/scenes/%2").arg(gid).arg(sid), "not connected"));
        rsp.httpStatus = HttpStatusServiceUnavailable;
        return REQ_READY_SEND;
    }

    if (!group || (group->state() == Group::StateDeleted))
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/groups/%1/scenes/%2").arg(gid).arg(sid), QString("resource, /groups/%1/scenes/%2, not available").arg(gid).arg(sid)));
        return REQ_READY_SEND;
    }

    // check if scene exists
    Scene scene;
    std::vector<Scene>::const_iterator i = group->scenes.begin();
    std::vector<Scene>::const_iterator end = group->scenes.end();

    uint8_t sceneId = sid.toUInt(&ok);

    if (ok)
    {
        ok = false;
        for (; i != end; ++i)
        {
            if ((i->id == sceneId) && (i->state != Scene::StateDeleted))
            {
                scene = *i;
                ok = true;
                break;
            }
        }
    }

    if (!ok)
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/groups/%1/scenes/%2").arg(gid).arg(sid), QString("resource, /groups/%1/scenes/%2, not available").arg(gid).arg(sid)));
        return REQ_READY_SEND;
    }

    if (!storeScene(group, scene.id))
    {
        rsp.httpStatus = HttpStatusServiceUnavailable;
        rsp.list.append(errorToMap(ERR_BRIDGE_BUSY, QString("/groups/%1/scenes/%2").arg(gid).arg(sid), QString("gateway busy")));
        return REQ_READY_SEND;
    }

    rspItemState["id"] = QString::number(scene.id);
    rspItem["success"] = rspItemState;
    rsp.list.append(rspItem);
    rsp.httpStatus = HttpStatusOk;

    return REQ_READY_SEND;
}

/*! PUT /api/<apikey>/groups/<group_id>/scenes/<scene_id>/recall
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::recallScene(const ApiRequest &req, ApiResponse &rsp)
{
    bool ok;
    QVariantMap rspItem;
    QVariantMap rspItemState;
    QString gid = req.path[3];
    QString sid = req.path[5];
    Group *group = getGroupForId(gid);
    rsp.httpStatus = HttpStatusOk;

    userActivity();

    if (!isInNetwork())
    {
        rsp.list.append(errorToMap(ERR_NOT_CONNECTED, QString("/groups/%1/scenes/%2").arg(gid).arg(sid), "not connected"));
        rsp.httpStatus = HttpStatusServiceUnavailable;
        return REQ_READY_SEND;
    }

    if (!group || (group->state() == Group::StateDeleted))
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/groups/%1/scenes/%2").arg(gid).arg(sid), QString("resource, /groups/%1/scenes/%2, not available").arg(gid).arg(sid)));
        return REQ_READY_SEND;
    }

    // check if scene exists
    Scene scene;
    std::vector<Scene>::const_iterator i = group->scenes.begin();
    std::vector<Scene>::const_iterator end = group->scenes.end();

    uint8_t sceneId = sid.toUInt(&ok);

    if (ok)
    {
        ok = false;
        for (; i != end; ++i)
        {
            if ((i->id == sceneId) && (i->state != Scene::StateDeleted))
            {
                scene = *i;
                ok = true;
                break;
            }
        }
    }

    if (!ok)
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/groups/%1/scenes/%2").arg(gid).arg(sid), QString("resource, /groups/%1/scenes/%2, not available").arg(gid).arg(sid)));
        return REQ_READY_SEND;
    }

    if (!callScene(group, scene.id))
    {
        rsp.httpStatus = HttpStatusServiceUnavailable;
        rsp.list.append(errorToMap(ERR_BRIDGE_BUSY, QString("/groups/%1/scenes/%2").arg(gid).arg(sid), QString("gateway busy")));
        return REQ_READY_SEND;
    }


    { // FIXME: Turn on all lights of the group based on the assumption
      // that the light state in the scene is also 'on' which might not be the case.
      // This shall be removed if the scenes will be queried from the lights.
        std::vector<LightNode>::iterator i = nodes.begin();
        std::vector<LightNode>::iterator end = nodes.end();

        uint16_t groupId = group->id().toUInt();

        for (; i != end; ++i)
        {
            if (isLightNodeInGroup(&(*i), groupId))
            {
                if (!i->isOn())
                {
                    i->setIsOn(true);
                    updateEtag(i->etag);
                }
            }
        }
    }

    // turning 'on' the group is also a assumtion but a very likely one
    if (!group->isOn())
    {
        group->setIsOn(true);
        updateEtag(group->etag);
    }

    updateEtag(gwConfigEtag);

    rspItemState["id"] = QString::number(scene.id);
    rspItem["success"] = rspItemState;
    rsp.list.append(rspItem);
    rsp.httpStatus = HttpStatusOk;

    processTasks();

    return REQ_READY_SEND;
}

/*! DELETE /api/<apikey>/groups/<group_id>/scenes/<scene_id>
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::deleteScene(const ApiRequest &req, ApiResponse &rsp)
{
    bool ok;
    QVariantMap rspItem;
    QVariantMap rspItemState;
    QString gid = req.path[3];
    QString sid = req.path[5];
    Group *group = getGroupForId(gid);
    rsp.httpStatus = HttpStatusOk;

    userActivity();

    if (!isInNetwork())
    {
        rsp.list.append(errorToMap(ERR_NOT_CONNECTED, QString("/groups/%1/scenes/%2").arg(gid).arg(sid), "Not connected"));
        rsp.httpStatus = HttpStatusServiceUnavailable;
        return REQ_READY_SEND;
    }

    if (!group || (group->state() == Group::StateDeleted))
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/groups/%1/scenes/%2").arg(gid).arg(sid), QString("resource, /groups/%1/scenes/%2, not available").arg(gid).arg(sid)));
        return REQ_READY_SEND;
    }

    // check if scene exists
    Scene scene;
    std::vector<Scene>::iterator i = group->scenes.begin();
    std::vector<Scene>::iterator end = group->scenes.end();

    uint8_t sceneId = sid.toUInt(&ok);

    if (ok)
    {
        ok = false;
        for (; i != end; ++i)
        {
            if (i->id == sceneId)
            {
                scene = *i;

                if (!removeScene(group, scene.id))
                {
                    rsp.list.append(errorToMap(ERR_NOT_CONNECTED, QString("/groups/%1/scenes/%2").arg(gid).arg(sid), "gateway busy"));
                    rsp.httpStatus = HttpStatusServiceUnavailable;
                    return REQ_READY_SEND;
                }

                ok = true;
                break;
            }
        }
    }

    if (!ok)
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/groups/%1/scenes/%2").arg(gid).arg(sid), QString("resource, /groups/%1/scenes/%2, not available").arg(gid).arg(sid)));
        return REQ_READY_SEND;
    }

    updateEtag(group->etag);
    updateEtag(gwConfigEtag);
    queSaveDb(DB_SCENES, DB_SHORT_SAVE_DELAY);

    rspItemState["id"] = QString::number(scene.id);
    rspItem["success"] = rspItemState;
    rsp.list.append(rspItem);
    rsp.httpStatus = HttpStatusOk;

    return REQ_READY_SEND;
}
