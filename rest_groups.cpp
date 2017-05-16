/*
 * Copyright (c) 2016 dresden elektronik ingenieurtechnik gmbh.
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
#include <QVariantMap>
#include "colorspace.h"
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
    // PUT, PATCH /api/<apikey>/groups/<id>
    else if ((req.path.size() == 4) && (req.hdr.method() == "PUT" || req.hdr.method() == "PATCH"))
    {
        return setGroupAttributes(req, rsp);
    }
    // PUT, PATCH /api/<apikey>/groups/<id>/action
    else if ((req.path.size() == 5) && (req.hdr.method() == "PUT" || req.hdr.method() == "PATCH") && (req.path[4] == "action"))
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
    else if ((req.path.size() == 6) && (req.hdr.method() == "PUT" || req.hdr.method() == "PATCH")  && (req.path[4] == "scenes"))
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
    // PUT, PATCH /api/<apikey>/groups/<group_id>/scenes/<scene_id>/lights/<light_id>/state
    else if ((req.path.size() == 9) && (req.hdr.method() == "PUT" || req.hdr.method() == "PATCH")  && (req.path[4] == "scenes") && (req.path[6] == "lights"))
    {
        return modifyScene(req, rsp);
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
        if (i->state() == Group::StateDeleted || i->state() == Group::StateDeleteFromDB)
        {
            continue;
        }

        if (i->address() != 0) // don't return special group 0
        {
            QVariantMap mnode;
            groupToMap(&(*i), mnode);
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
                if (group1->state() != Group::StateDeleted && group1->state() != Group::StateDeleteFromDB)
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
            quint16 gidLastTry = 0;
            group.setAddress(1);

            do {
                ok = true;
                std::vector<Group>::iterator i = groups.begin();
                std::vector<Group>::iterator end = groups.end();

                for (; i != end; ++i)
                {
                    if (i->address() == group.address())
                    {
                        if (i->state() == Group::StateDeleted)
                        {
                            if (gidLastTry == 0) // mark gid so it could be reused
                            {
                                gidLastTry = group.address();
                            }

                        }

                        group.setAddress(i->address() + 1);
                        ok = false;
                        break;
                    }
                }

                if (group.address() == 0) // overflow
                {
                    break;
                }
            } while (!ok);

            if (ok && group.address() > 0)
            {
                // ok, nothing todo here
            }
            // reuse deleted groupId?
            else if (!ok && gidLastTry > 0)
            {
                // remove from list
                std::vector<Group>::iterator i = groups.begin();
                std::vector<Group>::iterator end = groups.end();

                for (; i != end; ++i)
                {
                    if (i->address() == gidLastTry)
                    {
                        groups.erase(i); // ok, replace
                        break;
                    }
                }

                group.setAddress(gidLastTry);
            }
            else
            {
                rsp.list.append(errorToMap(ERR_BRIDGE_GROUP_TABLE_FULL, QString("/groups"), QString("group could not be created. Group table is full.")));
                rsp.httpStatus = HttpStatusBadRequest;
                return REQ_READY_SEND;
            }

            group.setName(name);
            group.colorX = 0;
            group.colorY = 0;
            group.setIsOn(false);
            group.level = 128;
            group.hue = 0;
            group.hueReal = 0.0f;
            group.sat = 128;
            groups.push_back(group);
            updateGroupEtag(&groups.back());
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

    if (!group || group->state() == Group::StateDeleted || group->state() == Group::StateDeleteFromDB)
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

    groupToMap(group,rsp.map);

    return REQ_READY_SEND;
}

/*! PUT, PATCH /api/<apikey>/groups/<id>
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

    // hidden
    if (map.contains("hidden"))
    {
        bool hidden = map["hidden"].toBool();

        if (map["hidden"].type() == QVariant::Bool)
        {
            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/groups/%1/hidden").arg(id)] = (hidden == true) ? "true" : "false";
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);

            if (group->hidden != hidden)
            {
                group->hidden = hidden;
                changed = true;
                queSaveDb(DB_GROUPS, DB_SHORT_SAVE_DELAY);
            }
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/groups/%1").arg(id), QString("invalid value for parameter, /groups/%2/hidden").arg(id)));
            rsp.httpStatus = HttpStatusBadRequest;
        }
    }

    // check optional parameter lights
    if (map.contains("lights"))
    {
        QVariantList lights = map["lights"].toList();
        uint8_t groupCount;
        uint8_t groupCapacity;

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
                    groupCount = lightNode->groupCount();
                    groupCapacity = lightNode->groupCapacity();

                    if (groupCapacity > 0 || (groupCapacity == 0 && groupCount == 0)) // xxx workaround
                    {
                        GroupInfo *groupInfo = getGroupInfo(lightNode, group->address());

                        if (!groupInfo)
                        {
                            groupInfo = createGroupInfo(lightNode, group->address());
                            lightNode->setNeedSaveDatabase(true);
                        }

                        DBG_Assert(groupInfo != 0);
                        if (groupInfo)
                        {
                            groupInfo->actions &= ~GroupInfo::ActionRemoveFromGroup; // sanity
                            groupInfo->actions |= GroupInfo::ActionAddToGroup;

                            if (groupInfo->state != GroupInfo::StateInGroup)
                            {
                                lightNode->setNeedSaveDatabase(true);
                                groupInfo->state = GroupInfo::StateInGroup;
                                ResourceItem *item = lightNode->item(RStateOn);
                                if (item && item->toBool())
                                {
                                    group->setIsOn(true);
                                    item = lightNode->item(RStateBri);
                                    group->level = item ? item->toNumber() : 254;
                                }
                            }
                        }

                        changed = true; // necessary for adding last available light to group from main view.
                    }
                    else
                    {
                        rsp.list.append(errorToMap(ERR_DEVICE_GROUP_TABLE_FULL, QString("/groups/%1/lights/%2").arg(id).arg(lid), QString(" Could not add %1 to group. Group capacity of the device is reached.").arg(qPrintable(lightNode->name()))));
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
                        j->setNeedSaveDatabase(true);

                        //delete Light from all scenes
                        deleteLightFromScenes(j->id(), k->id);

                        changed = true;
                    }
                }
            }
        }

        // check optional parameter multideviceids
        if (map.contains("multideviceids"))
        {
            group->m_multiDeviceIds.clear();

            QStringList multiIds = map["multideviceids"].toStringList();

            QStringList::const_iterator m = multiIds.begin();
            QStringList::const_iterator m_end = multiIds.end();

            for (;m != m_end; ++m)
            {
                group->m_multiDeviceIds.push_back(*m);
            }
        }
        queSaveDb(DB_LIGHTS | DB_GROUPS, DB_SHORT_SAVE_DELAY);
    }

    // check optional lightsequence
    if (map.contains("lightsequence"))
    {
        changed = true;
        group->m_lightsequence.clear();

        QStringList lightsequence = map["lightsequence"].toStringList();

        QStringList::const_iterator l = lightsequence.begin();
        QStringList::const_iterator l_end = lightsequence.end();

        for (;l != l_end; ++l)
        {
            group->m_lightsequence.push_back(*l);
        }
        queSaveDb(DB_GROUPS, DB_SHORT_SAVE_DELAY);

        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState[QString("/groups/%1/lightsequence").arg(id)] = map["lightsequence"];
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
    }

    if (changed)
    {
        updateGroupEtag(group);
    }

    rsp.etag = group->etag;

    return REQ_READY_SEND;
}

/*! PUT, PATCH /api/<apikey>/groups/<id>/action
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::setGroupState(const ApiRequest &req, ApiResponse &rsp)
{
    TaskItem task;
    QString id = req.path[3];
    Group *group = getGroupForId(id);

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
        task.req.dstAddress().setNwk(deCONZ::BroadcastRouters);
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

    bool hasOn = map.contains("on");
    bool hasOnTime = map.contains("ontime");
    bool hasBri = map.contains("bri");
    bool hasHue = map.contains("hue");
    bool hasSat = map.contains("sat");
    bool hasXy = map.contains("xy");
    bool hasCt = map.contains("ct");
    bool hasEffect = map.contains("effect");
    bool hasEffectColorLoop = false;
    bool hasAlert = map.contains("alert");

    bool on = false;
    uint bri = 0;
    uint hue = UINT_MAX;
    uint sat = UINT_MAX;
    double x = 0;
    double y = 0;
    uint ct = 0;

    // transition time
    if (map.contains("transitiontime"))
    {
        uint tt = map["transitiontime"].toUInt(&ok);

        if (ok && tt < 0xFFFFUL)
        {
            task.transitionTime = tt;
        }
    }

    // on/off
    if (hasOn)
    {
        hasOn = false;
        if (map["on"].type() == QVariant::Bool)
        {
            hasOn = true;
            on = map["on"].toBool();
            group->setIsOn(on);
            quint16 ontime = 0;
            quint8 command = on ? ONOFF_COMMAND_ON : ONOFF_COMMAND_OFF;         
            if (on)
            {
                if (hasOnTime && map["ontime"].type() == QVariant::Double)
                {
                    double ot = map["ontime"].toDouble(&ok);
                    if (ok && (ot >= 0 && ot <= 65535))
                    {
                        ontime = ot;
                        command = ONOFF_COMMAND_ON_WITH_TIMED_OFF;
                    }
                }
            }

            if (group->isColorLoopActive())
            {
                addTaskSetColorLoop(task, false, 15);
                group->setColorLoopActive(false); // deactivate colorloop if active
            }
            std::vector<LightNode>::iterator i = nodes.begin();
            std::vector<LightNode>::iterator end = nodes.end();

            for (; i != end; ++i)
            {
                if (isLightNodeInGroup(&(*i), group->address()))
                {
                    if (i->isColorLoopActive() && i->isAvailable() && i->state() != LightNode::StateDeleted)
                    {
                        TaskItem task2;
                        task2.lightNode = &(*i);
                        task2.req.dstAddress() = task2.lightNode->address();
                        task2.req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
                        task2.req.setDstEndpoint(task2.lightNode->haEndpoint().endpoint());
                        task2.req.setSrcEndpoint(getSrcEndpoint(task2.lightNode, task2.req));
                        task2.req.setDstAddressMode(deCONZ::ApsExtAddress);

                        addTaskSetColorLoop(task2, false, 15);
                        i->setColorLoopActive(false);
                    }
                }
            }

            if (hasBri ||
                addTaskSetOnOff(task, command, ontime)) // onOff task only if no bri is given
            {
                QVariantMap rspItem;
                QVariantMap rspItemState;
                rspItemState[QString("/groups/%1/action/on").arg(id)] = on;
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
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/groups/%1/action/on").arg(id), QString("invalid value, %1, for parameter, on").arg(map["on"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }

    // brightness
    if (hasBri)
    { 
        hasBri = false;
        bri = map["bri"].toUInt(&ok);

        if ((map["bri"].type() == QVariant::String) && map["bri"].toString() == "stop")
        {
            if (addTaskStopBrightness(task))
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
        else if (ok && (map["bri"].type() == QVariant::Double) && (bri < 256))
        {
            hasBri = true;
            group->level = bri;
            if (addTaskSetBrightness(task, bri, hasOn))
            {
                QVariantMap rspItem;
                QVariantMap rspItemState;
                rspItemState[QString("/groups/%1/action/bri").arg(id)] = map["bri"];
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
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/groups/%1/action/bri").arg(id), QString("invalid value, %1, for parameter, bri").arg(map["bri"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }

    // hue
    if (hasHue)
    {
        hasHue = false;
        uint hue2 = map["hue"].toUInt(&ok);

        if (ok && (map["hue"].type() == QVariant::Double) && (hue2 < (MAX_ENHANCED_HUE + 1)))
        {
            hasHue = true;
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

                group->hue = hue;
                group->hueReal = task.hueReal;
                group->colormode = QLatin1String("hs");
            }

            if (!hasXy && !hasSat)
            {
                double r, g, b;
                double x, y;
                double h = ((360.0f / 65535.0f) * hue);
                double s = group->sat / 255.0f;
                double v = 1.0f;

                Hsv2Rgb(&r, &g, &b, h, s, v);
                Rgb2xy(&x, &y, r, g, b);

                DBG_Printf(DBG_INFO, "x: %f, y: %f\n", x, y);
                group->colorX = x * 65279.0f;
                group->colorY = y * 65279.0f;
            }

            if (hasSat || // merge later to set hue and saturation
                hasXy || hasCt || hasEffectColorLoop ||
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
    if (hasSat)
    {
        hasSat = false;
        uint sat2 = map["sat"].toUInt(&ok);

        if (ok && (map["sat"].type() == QVariant::Double) && (sat2 < 256))
        {
            hasSat = true;
            if (sat2 == 255)
            {
                sat2 = 254; // max valid value for level attribute
            }

            sat = sat2;
            task.sat = sat;
            task.taskType = TaskSetSat;
            group->sat = sat;
            group->colormode = QLatin1String("hs");

            if (!hasXy && !hasHue)
            {
                double r, g, b;
                double x, y;
                double h = ((360.0f / 65535.0f) * group->hue);
                double s = sat / 254.0f;
                double v = 1.0f;

                Hsv2Rgb(&r, &g, &b, h, s, v);
                Rgb2xy(&x, &y, r, g, b);

                DBG_Printf(DBG_INFO, "x: %f, y: %f\n", x, y);
                group->colorX = x * 65279.0f;
                group->colorY = y * 65279.0f;
            }

            if (hasXy || hasCt
               || (!hasEffectColorLoop && hasHue && (hue != UINT_MAX)) // merge later to set hue and saturation
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
    if (hasHue && hasSat && (!supportColorModeXyForGroups || (!hasXy && !hasCt)))
    {
        if (!hasEffectColorLoop && (hue != UINT_MAX) && (sat != UINT_MAX))
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
    if (hasXy)
    {
        hasXy = false;
        QVariantList ls = map["xy"].toList();

        if ((ls.size() == 2) && (ls[0].type() == QVariant::Double) && (ls[1].type() == QVariant::Double))
        {
            x = ls[0].toDouble();
            y = ls[1].toDouble();

            if ((x < 0.0f) || (x > 1.0f) || (y < 0.0f) || (y > 1.0f))
            {
                rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/groups/%1").arg(id), QString("invalid value, [%1,%2], for parameter, /groups/%3/xy").arg(x).arg(y).arg(id)));
            }
            else if (hasEffectColorLoop ||
                     addTaskSetXyColor(task, x, y))
            {
                QVariantMap rspItem;
                QVariantMap rspItemState;
                rspItemState[QString("/groups/%1/action/xy").arg(id)] = map["xy"];
                rspItem["success"] = rspItemState;
                rsp.list.append(rspItem);
                hasXy = true;
                group->colorX = x * 65279.0f; // current X in range 0 .. 65279
                group->colorY = y * 65279.0f; // current Y in range 0 .. 65279
                group->colormode = QLatin1String("xy");
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

    // color temperature
    if (hasCt)
    {
        hasCt = false;
        ct = map["ct"].toUInt(&ok);

        if (ok && (map["ct"].type() == QVariant::Double))
        {
            group->colorTemperature = ct;
            group->colormode = QLatin1String("ct");
            if (addTaskSetColorTemperature(task, ct))
            {
                hasCt = true;
                QVariantMap rspItem;
                QVariantMap rspItemState;
                rspItemState[QString("/groups/%1/action/ct").arg(id)] = map["ct"];
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
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/groups/%1/action/ct").arg(id), QString("invalid value, %1, for parameter, ct").arg(map["ct"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }

    // alert
    if (hasAlert)
    {
        QString alert = map["alert"].toString();

        if (alert == "none")
        {
            task.identifyTime = 0;
        }
        else if (alert == "select")
        {
            task.identifyTime = 1;
        }
        else if (alert == "lselect")
        {
            task.identifyTime = 30;
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/groups/%1/action/alert").arg(id), QString("invalid value, %1, for parameter, alert").arg(map["alert"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

        task.taskType = TaskIdentify;
        taskToLocalData(task);

        if (addTaskIdentify(task, task.identifyTime))
        {
            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/groups/%1/action/alert").arg(id)] = map["alert"];
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/groups/%1").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
        }
    }

    // colorloop
    if (hasEffect)
    {
        QString effect = map["effect"].toString();

        if ((effect == "none") || (effect == "colorloop"))
        {
            hasEffectColorLoop = effect == "colorloop";
            uint speed = 15;

            if (hasEffectColorLoop)
            {
                if (map.contains("colorloopspeed"))
                {
                    speed = map["colorloopspeed"].toUInt(&ok);
                    if (ok && (map["colorloopspeed"].type() == QVariant::Double) && (speed < 256) && (speed > 0))
                    {
                        // ok
                        std::vector<LightNode>::iterator i = nodes.begin();
                        std::vector<LightNode>::iterator end = nodes.end();

                        for (; i != end; ++i)
                        {
                            if (isLightNodeInGroup(&(*i), group->address()))
                            {
                                i->setColorLoopSpeed(speed);
                            }
                        }
                    }
                    else
                    {
                        rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1/state/colorloopspeed").arg(id), QString("invalid value, %1, for parameter, colorloopspeed").arg(map["colorloopspeed"].toString())));
                    }
                }
            }

            if (addTaskSetColorLoop(task, hasEffectColorLoop, speed))
            {
                QVariantMap rspItem;
                QVariantMap rspItemState;
                rspItemState[QString("/groups/%1/action/effect").arg(id)] = map["effect"];
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
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/groups/%1/action/effect").arg(id), QString("invalid value, %1, for parameter, effect").arg(map["effect"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }

    { // update lights state
        std::vector<LightNode>::iterator i = nodes.begin();
        std::vector<LightNode>::iterator end = nodes.end();

        for (; i != end; ++i)
        {
            if (/*i->isAvailable() &&*/ i->state() != LightNode::StateDeleted && isLightNodeInGroup(&*i, group->address()))
            {
                ResourceItem *item = i->item(RStateOn);
                bool modified = false;
                if (hasOn && item && on != item->toBool())
                {
                    item->setValue(on);
                    Event e(RLights, RStateOn, i->id());
                    enqueueEvent(e);
                    modified = true;
                }

                item = i->item(RStateBri);
                if (hasBri && item && bri != item->toNumber())
                {
                    item->setValue(bri);
                    Event e(RLights, RStateBri, i->id());
                    enqueueEvent(e);
                    modified = true;
                }

                item = i->item(RStateColorMode);
                if (item)
                {
                    if (hasXy && i->modelId() != QLatin1String("FLS-PP")) // don't use xy for old black FLS-PP
                    {
                        if (item->toString() != QLatin1String("xy"))
                        {
                            item->setValue(QVariant(QLatin1String("xy")));
                            Event e(RLights, RStateColorMode, i->id());
                            enqueueEvent(e);
                            modified = true;
                        }

                        quint16 colorX = x * 65279.0f; // current X in range 0 .. 65279
                        quint16 colorY = y * 65279.0f; // current Y in range 0 .. 65279

                        item = i->item(RStateX);
                        if (item && item->toNumber() != colorX)
                        {
                            item->setValue(colorX);
                            Event e(RLights, RStateX, i->id());
                            enqueueEvent(e);
                            modified = true;
                        }

                        item = i->item(RStateY);
                        if (item && item->toNumber() != colorY)
                        {
                            item->setValue(colorY);
                            Event e(RLights, RStateY, i->id());
                            enqueueEvent(e);
                            modified = true;
                        }
                    }
                    else if (hasCt && i->item(RStateCt))
                    {
                        if (item->toString() != QLatin1String("ct"))
                        {
                            item->setValue(QVariant(QLatin1String("ct")));
                            Event e(RLights, RStateColorMode, i->id());
                            enqueueEvent(e);
                            modified = true;
                        }

                        item = i->item(RStateCt);
                        DBG_Assert(item != 0);

                        if (item && item->toNumber() != ct)
                        {
                            item->setValue(ct);
                            Event e(RLights, RStateCt, i->id());
                            enqueueEvent(e);
                            modified = true;
                        }
                    }
                    else if (hasHue)
                    {
                        if (item->toString() != QLatin1String("hs"))
                        {
                            item->setValue(QVariant(QLatin1String("hs")));
                            Event e(RLights, RStateColorMode, i->id());
                            enqueueEvent(e);
                            modified = true;
                        }

                        item = i->item(RStateHue);

                        if (item->toNumber() != hue)
                        {
                            i->setEnhancedHue(hue);
                            item->setValue(hue);
                            Event e(RLights, RStateHue, i->id());
                            enqueueEvent(e);

                            if (!hasXy && !hasSat)
                            {
                                double r, g, b;
                                double x, y;
                                double h = ((360.0f / 65535.0f) * hue);
                                double s = i->saturation() / 255.0f;
                                double v = 1.0f;

                                Hsv2Rgb(&r, &g, &b, h, s, v);
                                Rgb2xy(&x, &y, r, g, b);

                                DBG_Printf(DBG_INFO, "x: %f, y: %f\n", x, y);
                                item = i->item(RStateX);
                                if (item)
                                {
                                    item->setValue(x * 65279.0f);
                                }
                                item = i->item(RStateY);
                                if (item)
                                {
                                    item->setValue(y * 65279.0f);
                                }
                            }
                        }
                    }
                    else if (hasSat)
                    {
                        if (item->toString() != QLatin1String("hs"))
                        {
                            item->setValue(QVariant(QLatin1String("hs")));
                            Event e(RLights, RStateColorMode, i->id());
                            enqueueEvent(e);
                            modified = true;
                        }

                        item = i->item(RStateSat);

                        if (item && item->toNumber() != sat)
                        {
                            item->setValue(sat);
                            Event e(RLights, RStateSat, i->id());
                            enqueueEvent(e);

                            if (!hasXy)
                            {
                                double r, g, b;
                                double x, y;
                                double h = (!hasHue) ? ((360.0f / 65535.0f) * i->enhancedHue()) : ((360.0f / 65535.0f) * hue);
                                double s = sat / 254.0f;
                                double v = 1.0f;

                                Hsv2Rgb(&r, &g, &b, h, s, v);
                                Rgb2xy(&x, &y, r, g, b);

                                DBG_Printf(DBG_INFO, "x: %f, y: %f\n", x, y);
                                item = i->item(RStateX);
                                if (item)
                                {
                                    item->setValue(x * 65279.0f);
                                }
                                item = i->item(RStateY);
                                if (item)
                                {
                                    item->setValue(y * 65279.0f);
                                }
                            }

                            modified = true;
                        }
                    }
                }

                if (modified)
                {
                    updateLightEtag(&*i);
                }
            }
        }
    }

    updateGroupEtag(group);
    rsp.etag = group->etag;

    processTasks();

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

    queSaveDb(DB_GROUPS | DB_LIGHTS, DB_SHORT_SAVE_DELAY);

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

/*! Adds a new group with unique id. */
Group *DeRestPluginPrivate::addGroup()
{
    for (quint16 id = 1 ; id < 5000; id++)
    {
        if (!getGroupForId(id))
        {
            Group group;
            group.setAddress(id);
            groups.push_back(group);
            updateGroupEtag(&groups.back());
            queSaveDb(DB_GROUPS, DB_SHORT_SAVE_DELAY);
            return &groups.back();
        }
    }

    return 0;
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
    QVariantList scenes;

    action["on"] = group->isOn();
    action["hue"] = (double)((uint16_t)(group->hueReal * 65535));
    action["effect"] = group->isColorLoopActive() ? QLatin1String("colorloop") : QLatin1String("none");
    action["bri"] = (double)group->level;
    action["sat"] = (double)group->sat;
    action["ct"] = (double)group->colorTemperature;
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
    action["colormode"] = group->colormode; // TODO

    map["id"] = group->id();
    map["name"] = group->name();
    map["hidden"] = group->hidden;
    QString etag = group->etag;
    etag.remove('"'); // no quotes allowed in string
    map["etag"] = etag;
    map["action"] = action;
    map["type"] = "LightGroup"; // TODO

    QStringList multis;
    std::vector<QString>::const_iterator m = group->m_multiDeviceIds.begin();
    std::vector<QString>::const_iterator mend = group->m_multiDeviceIds.end();

    for ( ;m != mend; ++m)
    {
        multis.append(*m);
    }

    map["multideviceids"] = multis;

    QStringList lightsequence;
    std::vector<QString>::const_iterator l = group->m_lightsequence.begin();
    std::vector<QString>::const_iterator lend = group->m_lightsequence.end();

    for ( ;l != lend; ++l)
    {
        lightsequence.append(*l);
    }

    map["lightsequence"] = lightsequence;

    QStringList deviceIds;
    std::vector<QString>::const_iterator d = group->m_deviceMemberships.begin();
    std::vector<QString>::const_iterator dend = group->m_deviceMemberships.end();

    for ( ;d != dend; ++d)
    {
        deviceIds.append(*d);
    }
    map["devicemembership"] = deviceIds;

    // append lights which are known members in this group
    QVariantList lights;
    std::vector<LightNode>::const_iterator i = nodes.begin();
    std::vector<LightNode>::const_iterator end = nodes.end();

    for (; i != end; ++i)
    {
        if (i->state() == LightNode::StateDeleted)
        {
            continue;
        }

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
            scene["transitiontime"] = si->transitiontime();

            scenes.append(scene);
        }
    }

    map["scenes"] = scenes;
    map["state"] = group->state();

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

    if (!group || (group->state() != Group::StateNormal))
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/groups/%1").arg(id), QString("resource, /groups/%1, not available").arg(id)));
        return REQ_READY_SEND;
    }

    scene.setTransitiontime(10);

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
    bool ommit = false;

    if (group->m_deviceMemberships.size() >= 1)
    {
        QString deviceId = group->m_deviceMemberships[0];
        Sensor *s = getSensorNodeForId(deviceId);
        if (s && s->modelId() == "Lighting Switch")
        {
            ommit = true; // ommit scene 2 and 3 for Lighting Switch
        }
    }
    scene.id = 1;
    do {
        ok = true; // will be false if a scene.id is already used
        std::vector<Scene>::iterator i = group->scenes.begin();
        std::vector<Scene>::iterator end = group->scenes.end();

        for (; i != end; ++i)
        {
            if (ommit && (scene.id == 2 || scene.id == 3))
            {
                scene.id++;
                ok = false;
            }
            else if (i->id == scene.id)
            {
                if (i->state == Scene::StateDeleted)
                {
                    group->scenes.erase(i); // ok, replace
                }
                else
                {
                    scene.id++;
                    ok = false;
                }
                break;
            }
        }
    } while (!ok);

    scene.groupAddress = group->address();

    if (scene.name.isEmpty())
    {
        scene.name.sprintf("Scene %u", scene.id);
    }

    std::vector<LightNode>::iterator ni = nodes.begin();
    std::vector<LightNode>::iterator nend = nodes.end();
    for (; ni != nend; ++ni)
    {
        LightNode *lightNode = &(*ni);

        if (lightNode->isAvailable() &&
            isLightNodeInGroup(lightNode, group->address()))
        {
            if (lightNode->sceneCapacity() <= 0)
            {
                rsp.list.append(errorToMap(ERR_DEVICE_SCENES_TABLE_FULL, QString("/groups/%1/scenes/lights/%2").arg(id).arg(lightNode->id()), QString("Could not set scene for %1. Scene capacity of the device is reached.").arg(qPrintable(lightNode->name()))));
                continue;
            }

            LightState state;
            state.setLightId(lightNode->id());
            state.setTransitionTime(10);
            ResourceItem *item = lightNode->item(RStateOn);
            DBG_Assert(item != 0);
            if (item)
            {
                state.setOn(item->toBool());
            }
            item = lightNode->item(RStateBri);
            if (item)
            {
                state.setBri(qMin((quint16)item->toNumber(), (quint16)254));
            }

            item = lightNode->item(RStateColorMode);
            if (item)
            {
                if (item->toString() == QLatin1String("xy") || item->toString() == QLatin1String("hs"))
                {
                    item = lightNode->item(RStateX);
                    if (item)
                    {
                        state.setX(item->toNumber());
                    }
                    item = lightNode->item(RStateY);
                    if (item)
                    {
                        state.setY(item->toNumber());
                    }
                    item = lightNode->item(RStateHue);
                    if (item)
                    {
                        state.setEnhancedHue(item->toNumber());
                    }
                    item = lightNode->item(RStateSat);
                    if (item)
                    {
                        state.setSaturation(item->toNumber());
                    }
                }
                else if (item->toString() == QLatin1String("ct"))
                {
                    item = lightNode->item(RStateCt);
                    DBG_Assert(item != 0);
                    if (item)
                    {
                        state.setColorTemperature(item->toNumber());
                    }
                }

                state.setColorloopActive(lightNode->isColorLoopActive());
                state.setColorloopTime(lightNode->colorLoopSpeed());
                state.setColorMode(lightNode->colorMode());
            }
            else
            {
                state.setColorMode(QLatin1String("none"));
            }

            scene.addLightState(state);
            queSaveDb(DB_SCENES, DB_LONG_SAVE_DELAY);
        }
    }

    group->scenes.push_back(scene);
    updateGroupEtag(group);
    queSaveDb(DB_SCENES, DB_SHORT_SAVE_DELAY);

    if (!storeScene(group, scene.id))
    {
        rsp.list.append(errorToMap(ERR_BRIDGE_BUSY, QString("/groups/%1/scenes/%2").arg(id).arg(scene.id), QString("gateway busy")));
        rsp.httpStatus = HttpStatusServiceUnavailable;
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
        if (i->state != Scene::StateDeleted)
        {
            QString sceneId = QString::number(i->id);
            QVariantMap scene;
            scene["name"] = i->name;

            QVariantList lights;
            std::vector<LightState>::const_iterator l = i->lights().begin();
            std::vector<LightState>::const_iterator lend = i->lights().end();
            for (; l != lend; ++l)
            {
                lights.append(l->lid());
            }
            scene["lights"] = lights;
            scene["transitiontime"] = i->transitiontime();

            rsp.map[sceneId] = scene;
        }
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
            if ((i->id == sceneId) && (i->state == Scene::StateNormal))
            {
                QVariantList lights;
                std::vector<LightState>::const_iterator l = i->lights().begin();
                std::vector<LightState>::const_iterator lend = i->lights().end();
                for (; l != lend; ++l)
                {
                    QVariantMap lstate;
                    lstate["id"] = l->lid();
                    lstate["on"] = l->on();
                    lstate["bri"] = l->bri();
                    LightNode *lightNode = getLightNodeForId(l->lid());
                    if (lightNode && lightNode->hasColor()) // TODO store hasColor in LightState
                    {
                        if (l->colorMode() == QLatin1String("xy"))
                        {
                            double x = l->x() / 65279.0f;
                            double y = l->y() / 65279.0f;
                            lstate["x"] = x;
                            lstate["y"] = y;
                        }
                        else if (l->colorMode() == QLatin1String("ct"))
                        {
                            lstate["ct"] = (double)l->colorTemperature();
                        }
                        else if (l->colorMode() == QLatin1String("hs"))
                        {
                            lstate["hue"] = (double)l->enhancedHue();
                            lstate["sat"] = (double)l->saturation();
                        }

                        lstate["colormode"] = l->colorMode();
                    }
                    lstate["transitiontime"] = l->transitionTime();

                    lights.append(lstate);
                }
                rsp.map["name"] = i->name;
                rsp.map["lights"] = lights;
                rsp.map["state"] = i->state;
                return REQ_READY_SEND;
            }
        }
    }

    rsp.httpStatus = HttpStatusNotFound;
    rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/groups/%1/scenes/%2").arg(gid).arg(sid), QString("resource, /groups/%1/scenes/%2, not available").arg(gid).arg(sid)));
    return REQ_READY_SEND;

}

/*! PUT, PATCH /api/<apikey>/groups/<group_id>/scenes/<scene_id>
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
                        updateGroupEtag(group);
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
    const QString &gid = req.path[3];
    const QString &sid = req.path[5];
    Group *group = getGroupForId(gid);
    rsp.httpStatus = HttpStatusOk;

    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();

    uint tt = 0;
    bool hasTt = false;

    userActivity();

    if (!isInNetwork())
    {
        rsp.list.append(errorToMap(ERR_NOT_CONNECTED, QString("/groups/%1/scenes/%2").arg(gid).arg(sid), "not connected"));
        rsp.httpStatus = HttpStatusServiceUnavailable;
        return REQ_READY_SEND;
    }

    if (!group || (group->state() != Group::StateNormal))
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/groups/%1/scenes/%2").arg(gid).arg(sid), QString("resource, /groups/%1/scenes/%2, not available").arg(gid).arg(sid)));
        return REQ_READY_SEND;
    }

    if (!ok)
    {
        rsp.list.append(errorToMap(ERR_INVALID_JSON, QString("/groups/%1/scenes/%2").arg(gid).arg(sid), QString("body contains invalid JSON")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    // check if scene exists
    uint8_t sceneId = sid.toUInt(&ok);
    Scene *scene = ok ? group->getScene(sceneId) : 0;

    if (!scene || (scene->state != Scene::StateNormal))
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/groups/%1/scenes/%2").arg(gid).arg(sid), QString("resource, /groups/%1/scenes/%2, not available").arg(gid).arg(sid)));
        return REQ_READY_SEND;
    }

    if (map.contains("transitiontime"))
    {
        tt = map["transitiontime"].toUInt(&ok);

        if (ok && tt < 0xFFFFUL)
        {
            hasTt = true;
            scene->setTransitiontime(tt);
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/groups/%1/scenes/%2/transitiontime").arg(gid).arg(sid), QString("invalid value, %1, for parameter transitiontime").arg(tt)));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }
    else
    {
        scene->setTransitiontime(10);
    }

    if (scene->externalMaster)
    {
        // we take control over scene
        scene->externalMaster = false;
    }

    // search for lights that have their scenes capacity reached or need to be updated
    std::vector<LightNode>::iterator ni = nodes.begin();
    std::vector<LightNode>::iterator nend = nodes.end();
    for (; ni != nend; ++ni)
    {
        LightNode *lightNode = &(*ni);
        if (lightNode->isAvailable() &&
            isLightNodeInGroup(lightNode, group->address()))
        {
            bool foundLight = false;
            std::vector<LightState>::iterator ls = scene->lights().begin();
            std::vector<LightState>::iterator lsend = scene->lights().end();
            for (; ls != lsend; ++ls)
            {
                if (ls->lid() != lightNode->id())
                {
                    continue;
                }

                bool needModify = false;
                ResourceItem *item = lightNode->item(RStateOn);
                DBG_Assert(item != 0);

                if (item && ls->on() != item->toBool())
                {
                    ls->setOn(item->toBool());
                    needModify = true;
                }

                item = lightNode->item(RStateBri);
                DBG_Assert(item != 0);

                if (item && ls->bri() != item->toNumber())
                {
                    ls->setBri(qMin((quint16)item->toNumber(), (quint16)254));
                    needModify = true;
                }

                item = lightNode->item(RStateColorMode);

                if (item)
                {
                    if (ls->colorMode() != item->toString())
                    {
                        ls->setColorMode(item->toString());
                        needModify = true;
                    }

                    if (item->toString() == QLatin1String("xy") || item->toString() == QLatin1String("hs"))
                    {
                        item = lightNode->item(RStateHue);
                        DBG_Assert(item != 0);
                        if (item && item->toNumber() != ls->enhancedHue())
                        {
                            ls->setEnhancedHue(item->toNumber());
                            needModify = true;
                        }

                        item = lightNode->item(RStateSat);
                        DBG_Assert(item != 0);
                        if (item && item->toNumber() != ls->saturation())
                        {
                            ls->setSaturation(item->toNumber());
                            needModify = true;
                        }

                        item = lightNode->item(RStateX);
                        DBG_Assert(item != 0);
                        if (item && item->toNumber() != ls->x())
                        {
                            ls->setX(item->toNumber());
                            needModify = true;
                        }

                        item = lightNode->item(RStateY);
                        DBG_Assert(item != 0);
                        if (item && item->toNumber() != ls->y())
                        {
                            ls->setY(item->toNumber());
                            needModify = true;
                        }
                    }

                    if (item->toString() == QLatin1String("ct"))
                    {
                        item = lightNode->item(RStateCt);
                        DBG_Assert(item != 0);
                        if (item && item->toNumber() != ls->colorTemperature())
                        {
                            ls->setColorTemperature(item->toNumber());
                            needModify = true;
                        }
                    }
                }
                else if (ls->colorMode() != QLatin1String("none"))
                {
                    ls->setColorMode(QLatin1String("none"));
                    needModify = true;
                }

                if (hasTt)
                {
                    if (ls->transitionTime() != tt)
                    {
                        ls->setTransitionTime(tt);
                        needModify = true;
                    }
                }
                else if (ls->transitionTime() != 10)
                {
                    ls->setTransitionTime(10);
                    needModify = true;
                }

                if (needModify)
                {
                    queSaveDb(DB_SCENES, DB_LONG_SAVE_DELAY);
                }

                ls->tVerified = QTime(); // invalidate, trigger verify or add
                foundLight = true;
                break;
            }

            if (!foundLight)
            {
                if (lightNode->sceneCapacity() <= 0)
                {
                    rsp.list.append(errorToMap(ERR_DEVICE_SCENES_TABLE_FULL, QString("/groups/%1/scenes/lights/%2").arg(gid).arg(lightNode->id()), QString("Could not set scene for %1. Scene capacity of the device is reached.").arg(qPrintable(lightNode->name()))));
                }

                LightState state;
                state.setLightId(lightNode->id());
                state.setTransitionTime(10);
                ResourceItem *item = lightNode->item(RStateOn);
                DBG_Assert(item != 0);
                if (item)
                {
                    state.setOn(item->toBool());
                }
                item = lightNode->item(RStateBri);
                if (item)
                {
                    state.setBri(qMin((quint16)item->toNumber(), (quint16)254));
                }

                item = lightNode->item(RStateColorMode);
                if (item)
                {
                    if (item->toString() == QLatin1String("xy") || item->toString() == QLatin1String("hs"))
                    {
                        item = lightNode->item(RStateX);
                        DBG_Assert(item != 0);
                        if (item)
                        {
                            state.setX(item->toNumber());
                        }
                        item = lightNode->item(RStateY);
                        DBG_Assert(item != 0);
                        if (item)
                        {
                            state.setY(item->toNumber());
                        }
                        item = lightNode->item(RStateHue);
                        DBG_Assert(item != 0);
                        if (item)
                        {
                            state.setEnhancedHue(item->toNumber());
                        }
                        item = lightNode->item(RStateSat);
                        DBG_Assert(item != 0);
                        if (item)
                        {
                            state.setSaturation(item->toNumber());
                        }
                    }
                    else if (item->toString() == QLatin1String("ct"))
                    {
                        item = lightNode->item(RStateCt);
                        DBG_Assert(item != 0);
                        if (item)
                        {
                            state.setColorTemperature(item->toNumber());
                        }
                    }

                    state.setColorloopActive(lightNode->isColorLoopActive());
                    state.setColorloopTime(lightNode->colorLoopSpeed());
                    state.setColorMode(lightNode->colorMode());
                }

                scene->addLightState(state);
                queSaveDb(DB_SCENES, DB_LONG_SAVE_DELAY);
            }
        }
    }

    if (!storeScene(group, scene->id))
    {
        rsp.httpStatus = HttpStatusServiceUnavailable;
        rsp.list.append(errorToMap(ERR_BRIDGE_BUSY, QString("/groups/%1/scenes/%2").arg(gid).arg(sid), QString("gateway busy")));
        return REQ_READY_SEND;
    }

    updateGroupEtag(group);

    rspItemState["id"] = sid;
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
    const QString &gid = req.path[3];
    const QString &sid = req.path[5];
    Group *group = getGroupForId(gid);
    rsp.httpStatus = HttpStatusOk;

    userActivity();

    if (!isInNetwork())
    {
        rsp.list.append(errorToMap(ERR_NOT_CONNECTED, QString("/groups/%1/scenes/%2").arg(gid).arg(sid), "not connected"));
        rsp.httpStatus = HttpStatusServiceUnavailable;
        return REQ_READY_SEND;
    }

    if (!group || (group->state() != Group::StateNormal))
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/groups/%1/scenes/%2").arg(gid).arg(sid), QString("resource, /groups/%1/scenes/%2, not available").arg(gid).arg(sid)));
        return REQ_READY_SEND;
    }

    // check if scene exists

    uint8_t sceneId = sid.toUInt(&ok);
    Scene *scene = ok ? group->getScene(sceneId) : 0;

    if (!scene || (scene->state != Scene::StateNormal))
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/groups/%1/scenes/%2").arg(gid).arg(sid), QString("resource, /groups/%1/scenes/%2, not available").arg(gid).arg(sid)));
        return REQ_READY_SEND;
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

    if (!callScene(group, sceneId))
    {
        rsp.httpStatus = HttpStatusServiceUnavailable;
        rsp.list.append(errorToMap(ERR_BRIDGE_BUSY, QString("/groups/%1/scenes/%2").arg(gid).arg(sid), QString("gateway busy")));
        return REQ_READY_SEND;
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

            ResourceItem *item = lightNode->item(RStateOn);
            if (item && item->toBool() != ls->on())
            {
                item->setValue(ls->on());
                Event e(RLights, RStateOn, lightNode->id());
                enqueueEvent(e);
                changed = true;
                groupOnChanged = true;
            }

            item = lightNode->item(RStateBri);
            if (ls->bri() != item->toNumber())
            {
                item->setValue(ls->bri());
                Event e(RLights, RStateBri, lightNode->id());
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
                        Event e(RLights, RStateX, lightNode->id());
                        enqueueEvent(e);
                        changed = true;
                    }
                    item = lightNode->item(RStateY);
                    if (item && ls->y() != item->toNumber())
                    {
                        item->setValue(ls->y());
                        Event e(RLights, RStateY, lightNode->id());
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
                        Event e(RLights, RStateCt, lightNode->id());
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
                        Event e(RLights, RStateHue, lightNode->id());
                        enqueueEvent(e);
                        changed = true;
                        groupHueSatChanged = true;
                    }

                    item = lightNode->item(RStateSat);
                    if (item && ls->saturation() != item->toNumber())
                    {
                        item->setValue(ls->saturation());
                        Event e(RLights, RStateSat, lightNode->id());
                        enqueueEvent(e);
                        changed = true;
                        groupHueSatChanged = true;
                    }
                }
            }

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

    rspItemState["id"] = sid;
    rspItem["success"] = rspItemState;
    rsp.list.append(rspItem);
    rsp.httpStatus = HttpStatusOk;

    processTasks();

    return REQ_READY_SEND;
}

/*! PUT, PATCH /api/<apikey>/groups/<group_id>/scenes/<scene_id>/lights/<light_id>/state
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::modifyScene(const ApiRequest &req, ApiResponse &rsp)
{
    bool ok;
    Scene scene;
    QVariantMap rspItem;
    QVariantMap rspItemState;
    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();
    QString gid = req.path[3];
    QString sid = req.path[5];
    QString lid = req.path[7];
    Group *group = getGroupForId(gid);
    LightNode *light = getLightNodeForId(lid);
    rsp.httpStatus = HttpStatusOk;

    userActivity();

    if (!isInNetwork())
    {
        rsp.list.append(errorToMap(ERR_NOT_CONNECTED, QString("/groups/%1/scenes/%2/lights/%3/state").arg(gid).arg(sid).arg(lid), "Not connected"));
        rsp.httpStatus = HttpStatusServiceUnavailable;
        return REQ_READY_SEND;
    }

    if (!ok || map.isEmpty())
    {
        rsp.list.append(errorToMap(ERR_INVALID_JSON, QString("/groups/%1/scenes/%2/lights/%3/state").arg(gid).arg(sid).arg(lid), QString("body contains invalid JSON")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    if (!group || (group->state() == Group::StateDeleted))
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/groups/%1/scenes/%2/lights/%3/state").arg(gid).arg(sid).arg(lid), QString("resource, /groups/%1, not available").arg(gid)));
        return REQ_READY_SEND;
    }

    if (!light || (light->state() == LightNode::StateDeleted) || !light->isAvailable())
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/groups/%1/scenes/%2/lights/%3/state").arg(gid).arg(sid).arg(lid), QString("resource, /lights/%1, not available").arg(lid)));
        return REQ_READY_SEND;
    }

    bool on;
    uint bri = 0;
    uint tt = 0;
    uint16_t xy_x;
    uint16_t xy_y;

    bool hasOn = false;
    bool hasBri = false;
    bool hasTt = false;
    bool hasXy = false;

    // on
    if (map.contains("on"))
    {
        on = map["on"].toBool();

        if (map["on"].type() == QVariant::Bool)
        {
            hasOn = true;
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/groups/%1/scenes/%2/lights/%3/state/on").arg(gid).arg(sid).arg(lid), QString("invalid value, %1, for parameter on").arg(on)));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }

    // bri
    if (map.contains("bri"))
    {
        bool ok;
        bri = map["bri"].toUInt(&ok);

        if (ok && map["bri"].type() == QVariant::Double && (bri < 256))
        {
            hasBri = true;
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/groups/%1/scenes/%2/lights/%3/state/bri").arg(gid).arg(sid).arg(lid), QString("invalid value, %1, for parameter bri").arg(bri)));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }

    // transitiontime
    if (map.contains("transitiontime"))
    {
        bool ok;
        tt = map["transitiontime"].toUInt(&ok);

        if (ok && tt < 0xFFFFUL)
        {
            hasTt = true;
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/groups/%1/scenes/%2/lights/%3/state/bri").arg(gid).arg(sid).arg(lid), QString("invalid value, %1, for parameter bri").arg(tt)));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }

    // xy
    if (map.contains("xy"))
    {

        QVariantList xy = map["xy"].toList();

        if ((xy.size() == 2) && (xy[0].type() == QVariant::Double) && (xy[1].type() == QVariant::Double))
        {
            double x = xy[0].toDouble();
            double y = xy[1].toDouble();

            if ((x < 0.0f) || (x > 1.0f) || (y < 0.0f) || (y > 1.0f))
            {
                rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1").arg(lid), QString("invalid value, [%1,%2], for parameter, /lights/%3/xy").arg(x).arg(y).arg(lid)));
                rsp.httpStatus = HttpStatusBadRequest;
                return REQ_READY_SEND;
            }
            else
            {
                hasXy = true;
                xy_x = floor(x * 65279.0f);
                xy_y = floor(y * 65279.0f);
            }
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/groups/%1/scenes/%2/lights/%3/state/xy").arg(gid).arg(sid).arg(lid), QString("invalid value, %1, for parameter xy").arg(xy[0].toString()).arg(xy[1].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
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

                    if (hasOn)
                    {
                        l->setOn(on);
                    }
                    if (hasBri)
                    {
                        l->setBri(bri);
                    }
                    if (hasTt)
                    {
                        l->setTransitionTime(tt);
                    }
                    if (hasXy)
                    {
                        l->setX(xy_x);
                        l->setY(xy_y);
                    }

                    if (!modifyScene(group, i->id))
                    {
                        rsp.httpStatus = HttpStatusServiceUnavailable;
                        rsp.list.append(errorToMap(ERR_BRIDGE_BUSY, QString("/groups/%1/scenes/%2/lights/%3/state").arg(gid).arg(sid).arg(lid), QString("gateway busy")));
                        return REQ_READY_SEND;
                    }

                    break;
                }
            }

            if (!foundLightState)
            {
                rsp.httpStatus = HttpStatusBadRequest;
                rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/groups/%1/scenes/%2/lights/%3/state").arg(gid).arg(sid).arg(lid), QString("Light %1 is not available in scene.").arg(lid)));
                return REQ_READY_SEND;

                /* //TODO or not TODO: add light to scene, when light is not a member of the scene. Error Message when ScenesTable of device is full.
                if (hasOn && hasBri && hastt && hasXy)
                {
                    LightState state;
                    state.setOn(on);
                    state.setBri(bri);
                    state.setTransitiontime(tt);
                    state.setX(xy_x);
                    state.setY(xy_y);
                    state.setLid(lid);

                    if (!modifyScene(group, i->id))
                    {
                        rsp.httpStatus = HttpStatusServiceUnavailable;
                        rsp.list.append(errorToMap(ERR_BRIDGE_BUSY, QString("/groups/%1/scenes/%2/lights/%3/state").arg(gid).arg(sid).arg(lid), QString("gateway busy")));
                        return REQ_READY_SEND;
                    }

                    i->m_lights.push_back(state);
                }
                else
                {
                    rsp.httpStatus = HttpStatusBadRequest;
                    rsp.list.append(errorToMap(ERR_MISSING_PARAMETER, QString("/groups/%1/scenes/%2/lights/%3/state").arg(gid).arg(sid).arg(lid), QString("Light %1 not available in scene. Missing parameters to add light to scene.").arg(lid)));
                    return REQ_READY_SEND;
                }
                */
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

    updateGroupEtag(group);

    queSaveDb(DB_SCENES, DB_SHORT_SAVE_DELAY);

    rspItemState["id"] = sid;
    rspItem["success"] = rspItemState;
    rsp.list.append(rspItem);
    rsp.httpStatus = HttpStatusOk;

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

    updateGroupEtag(group);
    queSaveDb(DB_SCENES, DB_SHORT_SAVE_DELAY);

    rspItemState["id"] = QString::number(scene.id);
    rspItem["success"] = rspItemState;
    rsp.list.append(rspItem);
    rsp.httpStatus = HttpStatusOk;

    return REQ_READY_SEND;
}
