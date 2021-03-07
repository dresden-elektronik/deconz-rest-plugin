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
int DeRestPluginPrivate::handleGroupsApi(const ApiRequest &req, ApiResponse &rsp)
{
    if (req.path[2] != QLatin1String("groups"))
    {
        return REQ_NOT_HANDLED;
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

    // handle ETag
    if (req.hdr.hasKey("If-None-Match"))
    {
        QString etag = req.hdr.value("If-None-Match");

        if (gwGroupsEtag == etag)
        {
            rsp.httpStatus = HttpStatusNotModified;
            rsp.etag = etag;
            return REQ_READY_SEND;
        }
    }

    std::vector<Group>::const_iterator i = groups.begin();
    std::vector<Group>::const_iterator end = groups.end();

    for (; i != end; ++i)
    {
        // ignore deleted groups
        if (i->state() == Group::StateDeleted || i->state() == Group::StateDeleteFromDB)
        {
            continue;
        }

        if (i->address() != gwGroup0) // don't return special group 0
        {
            QVariantMap mnode;
            groupToMap(req, &(*i), mnode);
            rsp.map[i->id()] = mnode;
        }
    }

    if (rsp.map.isEmpty())
    {
        rsp.str = "{}"; // return empty object
    }

    rsp.etag = gwGroupsEtag;

    return REQ_READY_SEND;
}

/*! POST /api/<apikey>/groups
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::createGroup(const ApiRequest &req, ApiResponse &rsp)
{
    rsp.httpStatus = HttpStatusOk;

    bool ok;
    Group group;
    QString type;
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

    // type
    if (map.contains("type"))
    {
        ok = false;
        type = map["type"].toString();
        if (map["type"].type() == QVariant::String)
        {
            for (const char *t : { "LightGroup", "Luminaire", "Lightsource", "Room" })
            {
                if (type == QLatin1String(t))
                {
                    ok = true;
                    break;
                }
            }
        }

        if (!ok)
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/groups"), QString("invalid value, %1, for parameter, type").arg(type)));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

        ResourceItem *item = group.item(RAttrType);
        DBG_Assert(item != 0);
        item->setValue(type);
    }

    // class
    if (type == "Room" && map.contains("class"))
    {
        ok = false;
        QString gclass = map["class"].toString();
        if (map["class"].type() == QVariant::String && type == QLatin1String("Room"))
        {
            for (const char *c : { "Living room", "Kitchen", "Dining", "Bedroom", "Kids bedroom",
                                   "Bathroom", "Nursery", "Recreation", "Office", "Gym", "Hallway",
                                   "Toilet", "Front door", "Garage", "Terrace", "Garden", "Driveway",
                                   "Carport", "Other",
                                   "Home", "Downstairs", "Upstairs", "Top floor", "Attic", "Guest room",
                                   "Staircase", "Lounge", "Man cave", "Computer", "Studio", "Music", 
                                   "TV", "Reading", "Closet", "Storage", "Laundry room", "Balcony", 
                                   "Porch", "Barbecue", "Pool" })
            {
                if (gclass == QLatin1String(c))
                {
                    ok = true;
                    break;
                }
            }
        }

        if (!ok)
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/groups"), QString("invalid value, %1, for parameter, class").arg(gclass)));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

        ResourceItem *item = group.item(RAttrClass);
        DBG_Assert(item != 0);
        item->setValue(gclass);
    }

    // uniqueid
    if (map.contains("uniqueid"))
    {
        QString uniqueid = map["uniqueid"].toString();
        // AA:BB:CC:DD or AA:BB:CC:DD-XX
        if (uniqueid.size() == 11 || uniqueid.size() == 14)
        {
            ResourceItem *item = group.addItem(DataTypeString, RAttrUniqueId);
            DBG_Assert(item != 0);
            item->setValue(uniqueid);
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/groups"), QString("invalid value, %1, for parameter, uniqeid").arg(uniqueid)));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }

    // name
    if (map.contains("name"))
    {
        QString name = map["name"].toString().trimmed();

        if (map["name"].type() == QVariant::String && !name.isEmpty())
        {
            QVariantMap rspItem;
            QVariantMap rspItemState;

#if 0 // this is check under application control
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
#endif

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
                        break;
                    }
                }

                if (group.address() == 0) // overflow
                {
                    break;
                }
            } while (!ok);

            if (!ok)
            {
                rsp.list.append(errorToMap(ERR_BRIDGE_GROUP_TABLE_FULL, QString("/groups"), QString("group could not be created. Group table is full.")));
                rsp.httpStatus = HttpStatusBadRequest;
                return REQ_READY_SEND;
            }

            ResourceItem *item = group.item(RAttrName);
            DBG_Assert(item != 0);
            item->setValue(name);

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

    groupToMap(req, group, rsp.map);

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

    // class
    if (map.contains("class"))
    {
        ok = false;
        QString gclass = map["class"].toString();
        if (map["class"].type() == QVariant::String &&
            group->item(RAttrType)->toString() == QLatin1String("Room"))
        {
            for (const char *c : { "Living room", "Kitchen", "Dining", "Bedroom", "Kids bedroom",
                                   "Bathroom", "Nursery", "Recreation", "Office", "Gym", "Hallway",
                                   "Toilet", "Front door", "Garage", "Terrace", "Garden", "Driveway",
                                   "Carport", "Other",
                                   "Home", "Downstairs", "Upstairs", "Top floor", "Attic", "Guest room",
                                   "Staircase", "Lounge", "Man cave", "Computer", "Studio", "Music", 
                                   "TV", "Reading", "Closet", "Storage", "Laundry room", "Balcony", 
                                   "Porch", "Barbecue", "Pool" })
            {
                if (gclass == QLatin1String(c))
                {
                    ok = true;
                    break;
                }
            }
        }

        if (!ok)
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/groups"), QString("invalid value, %1, for parameter, class").arg(gclass)));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

        ResourceItem *item = group->item(RAttrClass);
        DBG_Assert(item != 0);
        if (item && item->toString() != gclass)
        {
            item->setValue(gclass);
            Event e(RGroups, RAttrClass, group->address());
            enqueueEvent(e);
        }
    }

    // name
    if (map.contains("name"))
    {
        QString name = map["name"].toString().trimmed();

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
                    Event e(RGroups, RAttrName, group->address());
                    enqueueEvent(e);
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

            Event e(RGroups, REventCheckGroupAnyOn, int(group->address()));
            enqueueEvent(e);

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

/*! Helper to generate a new task with new task and req id based on a reference */
static void copyTaskReq(TaskItem &a, TaskItem &b)
{
    b.req.dstAddress() = a.req.dstAddress();
    b.req.setDstAddressMode(a.req.dstAddressMode());
    b.req.setSrcEndpoint(a.req.srcEndpoint());
    b.req.setDstEndpoint(a.req.dstEndpoint());
    b.req.setRadius(a.req.radius());
    b.transitionTime = a.transitionTime;
}

/*! PUT, PATCH /api/<apikey>/groups/<id>/action
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::setGroupState(const ApiRequest &req, ApiResponse &rsp)
{
    TaskItem taskRef;
    QString id = req.path[3];
    Group *group = getGroupForId(id);

    if (req.sock)
    {
        userActivity();
    }

    if (!isInNetwork())
    {
        rsp.list.append(errorToMap(ERR_NOT_CONNECTED, QString("/groups/%1/action").arg(id), "Not connected"));
        rsp.httpStatus = HttpStatusServiceUnavailable;
        return REQ_READY_SEND;
    }

    if (!group || (group->state() != Group::StateNormal))
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/groups/%1").arg(id), QString("resource, /groups/%1, not available").arg(id)));
        return REQ_READY_SEND;
    }

    rsp.httpStatus = HttpStatusOk;

    // set destination parameters
    taskRef.req.dstAddress().setGroup(group->address());
    taskRef.req.setDstAddressMode(deCONZ::ApsGroupAddress);
    taskRef.req.setDstEndpoint(0xFF); // broadcast endpoint
    taskRef.req.setSrcEndpoint(getSrcEndpoint(0, taskRef.req));

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
    bool hasOpen = map.contains("open");
    bool hasBri = map.contains("bri");
    bool hasHue = map.contains("hue");
    bool hasSat = map.contains("sat");
    bool hasXy = map.contains("xy");
    bool hasCt = map.contains("ct");
    bool hasCtInc = map.contains("ct_inc");
    bool hasBriInc = map.contains("bri_inc");
    bool hasEffect = map.contains("effect");
    bool hasEffectColorLoop = false;
    bool hasAlert = map.contains("alert");
    bool hasToggle = map.contains("toggle");
    bool hasWrap = map.contains("wrap");

    bool on = false;
    bool targetOpen = false;
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
            taskRef.transitionTime = tt;
        }
    }

    // toggle
    if (hasToggle)
    {
        if (map["toggle"].type() == QVariant::Bool)
        {
            if (map["toggle"] == true)
            {
                map["on"] = group->item(RStateAnyOn)->toBool() ? false : true;
                hasOn = true;
            }
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/groups/%1/action/toggle").arg(id), QString("invalid value, %1, for parameter, toggle").arg(map["toggle"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
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
            quint8 flags = 0;
            if (on)
            {
                if (hasOnTime && map["ontime"].type() == QVariant::Double)
                {
                    uint ot = map["ontime"].toUInt(&ok);
                    if (ok && ot <= 65535)
                    {
                        ontime = static_cast<quint16>(ot);
                        command = ONOFF_COMMAND_ON_WITH_TIMED_OFF;
                    }

                    if (ok && map.contains("onoffcontrol"))
                    {
                        uint ooc = map["onoffcontrol"].toUInt(&ok);
                        if (ok && ooc & 1) // accept only when on
                        {
                            flags |= 1;
                        }
                    }
                }
            }

            if (group->isColorLoopActive())
            {
                TaskItem task;
                copyTaskReq(taskRef, task);
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

            TaskItem task;
            copyTaskReq(taskRef, task);
            if (hasBri ||
                addTaskSetOnOff(task, command, ontime, flags)) // onOff task only if no bri is given
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

    if (hasOpen)
    {
        hasOpen = false;
        if (map["open"].type() == QVariant::Bool)
        {
            hasOpen = true;
            targetOpen = map["open"].toBool();

            TaskItem task;
            copyTaskReq(taskRef, task);
            if (addTaskWindowCovering(task, targetOpen ? WINDOW_COVERING_COMMAND_OPEN : WINDOW_COVERING_COMMAND_CLOSE, 0, 0))
            {
                QVariantMap rspItem;
                QVariantMap rspItemState;
                rspItemState[QString("/groups/%1/action/open").arg(id)] = targetOpen;
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
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/groups/%1/action/open").arg(id), QString("invalid value, %1, for parameter, on").arg(map["on"].toString())));
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
            TaskItem task;
            copyTaskReq(taskRef, task);
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
            TaskItem task;
            copyTaskReq(taskRef, task);
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

        if (ok && (map["hue"].type() == QVariant::Double) && (hue2 <= MAX_ENHANCED_HUE))
        {
            TaskItem task;
            copyTaskReq(taskRef, task);
            hasHue = true;
            hue = hue2;
            { // TODO: this is needed if saturation is set and addTaskSetEnhancedHue() will not be called
                task.hueReal = (double)hue / (360.0f * 182.04444f);

                if (task.hueReal < 0.0)
                {
                    task.hueReal = 0.0;
                }
                else if (task.hueReal > 1.0)
                {
                    task.hueReal = 1.0;
                }
                task.hue = task.hueReal * 254.0;
                if (hue > MAX_ENHANCED_HUE_Z)
                {
                    hue = MAX_ENHANCED_HUE_Z;
                }
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
                double h = ((360.0 / 65535.0) * hue);
                double s = group->sat / 255.0;
                double v = 1.0;

                Hsv2Rgb(&r, &g, &b, h, s, v);
                Rgb2xy(&x, &y, r, g, b);

                if (x < 0) { x = 0; }
                else if (x > 1) { x = 1; }

                if (y < 0) { y = 0; }
                else if (y > 1) { y = 1; }

                DBG_Printf(DBG_INFO, "x: %f, y: %f\n", x, y);
                group->colorX = static_cast<quint16>(x * 65535.0);
                group->colorY = static_cast<quint16>(y * 65535.0);

                if (group->colorX > 65279) { group->colorX = 65279; }
                else if (group->colorX == 0) { group->colorX = 1; }

                if (group->colorY > 65279) { group->colorY = 65279; }
                else if (group->colorY == 0) { group->colorY = 1; }
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
            if (sat2 >= 255)
            {
                sat2 = 254; // max valid value for level attribute
            }

            TaskItem task;
            copyTaskReq(taskRef, task);
            sat = sat2;
            task.sat = sat;
            task.taskType = TaskSetSat;
            group->sat = sat;
            group->colormode = QLatin1String("hs");

            if (!hasXy && !hasHue)
            {
                double r, g, b;
                double x, y;
                double h = ((360.0 / 65535.0) * group->hue);
                double s = sat / 254.0;
                double v = 1.0;

                Hsv2Rgb(&r, &g, &b, h, s, v);
                Rgb2xy(&x, &y, r, g, b);

                if (x < 0) { x = 0; }
                else if (x > 1) { x = 1; }

                if (y < 0) { y = 0; }
                else if (y > 1) { y = 1; }

                DBG_Printf(DBG_INFO, "x: %f, y: %f\n", x, y);
                group->colorX = static_cast<quint16>(x * 65535.0);
                group->colorY = static_cast<quint16>(y * 65535.0);

                if (group->colorX > 65279) { group->colorX = 65279; }
                else if (group->colorX == 0) { group->colorX = 1; }

                if (group->colorY > 65279) { group->colorY = 65279; }
                else if (group->colorY == 0) { group->colorY = 1; }
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

            TaskItem task;
            copyTaskReq(taskRef, task);
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
    if (hasXy && supportColorModeXyForGroups)
    {
        hasXy = false;
        QVariantList ls = map["xy"].toList();

        if ((ls.size() == 2) && (ls[0].type() == QVariant::Double) && (ls[1].type() == QVariant::Double))
        {
            x = ls[0].toDouble(&ok);
            y = ok ? ls[1].toDouble(&ok) : 0;
            TaskItem task;
            copyTaskReq(taskRef, task);

            if (!ok || (x < 0.0) || (x > 1.0) || (y < 0.0) || (y > 1.0))
            {
                rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/groups/%1").arg(id), QString("invalid value, [%1,%2], for parameter, /groups/%3/xy").arg(x).arg(y).arg(id)));
                hasXy = false;
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
                group->colormode = QLatin1String("xy");
                group->colorX = static_cast<quint16>(x * 65535.0); // current X in range 0 .. 65279
                group->colorY = static_cast<quint16>(y * 65535.0); // current Y in range 0 .. 65279

                if (group->colorX > 65279) { group->colorX = 65279; }
                else if (group->colorX == 0) { group->colorX = 1; }

                if (group->colorY > 65279) { group->colorY = 65279; }
                else if (group->colorY == 0) { group->colorY = 1; }
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

    // ct_inc
    if (hasCtInc)
    {
        int ct_inc = map["ct_inc"].toInt(&ok);

        if (hasCt)
        {
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_MODIFIEABLE, QString("/groups/%1").arg(id), QString("parameter, /lights/%1/ct_inc, is not modifiable. ct was specified.").arg(id)));
        }
        else if (ok && (map["ct_inc"].type() == QVariant::Double) && (ct_inc >= -65534 && ct_inc <= 65534))
        {
            TaskItem task;
            copyTaskReq(taskRef, task);
            task.inc = ct_inc;
            task.taskType = TaskIncColorTemperature;

            group->colormode = QLatin1String("ct");

            if (addTaskIncColorTemperature(task, ct_inc)) // will only be evaluated if no ct is set
            {
                taskToLocalData(task);
                QVariantMap rspItem;
                QVariantMap rspItemState;
                rspItemState[QString("/groups/%1/action/ct").arg(id)] = group->colorTemperature;
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
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/groups/%1/action/ct_inc").arg(id), QString("invalid value, %1, for parameter, ct_inc").arg(map["ct_inc"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }

    // bri_inc
    if (hasBriInc && !hasBri)
    {
        int briInc = map["bri_inc"].toInt(&ok);
        if (hasWrap && map["wrap"].type() == QVariant::Bool && map["wrap"].toBool() == true)
        {
            std::vector<LightNode>::iterator i = nodes.begin();
            std::vector<LightNode>::iterator end = nodes.end();

            // Find the highest and lowest brightness lights
            int hiBri = -1, loBri = 255;
            for (; i != end; ++i)
            {
                if (isLightNodeInGroup(&(*i), group->address()))
                {
                    ResourceItem *item = i->item(RStateBri);
                    if (item && i->isAvailable() && i->state() != LightNode::StateDeleted)
                    {
                        const int bri = static_cast<int>(item->toNumber());
                        hiBri = (bri > hiBri) ? bri : hiBri;
                        loBri = (bri < loBri) ? bri : loBri;
                    }
                }
            }

            // Check if we need to wrap around
            if (hiBri >= 0 && loBri < 255)
            {
                if (briInc < 0 && loBri + briInc <= -briInc)
                {
                    briInc = 254;
                }
                else if (briInc > 0 && hiBri + briInc >= 254)
                {
                    briInc = -254;
                }
            }
        }

        if (ok && (map["bri_inc"].type() == QVariant::Double) && (briInc >= -254 && briInc <= 254))
        {
            TaskItem task;
            copyTaskReq(taskRef, task);
            task.inc = briInc;
            task.taskType = TaskIncBrightness;

            if (addTaskIncBrightness(task, briInc))
            {
                taskToLocalData(task);
                QVariantMap rspItem;
                QVariantMap rspItemState;
                rspItemState[QString("/groups/%1/action/bri_inc").arg(id)] = briInc;
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
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/groups/%1/action/bri_inc").arg(id), QString("invalid value, %1, for parameter, bri_inc").arg(map["bri_inc"].toString())));
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
            TaskItem task;
            copyTaskReq(taskRef, task);
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
        TaskItem task;
        copyTaskReq(taskRef, task);
        QString alert = map["alert"].toString();

        if (alert == "none")
        {
            task.taskType = TaskIdentify;
            task.identifyTime = 0;
        }
        else if (alert == "select")
        {
            task.taskType = TaskIdentify;
            task.identifyTime = 2;    // Hue lights don't react to 1.
        }
        else if (alert == "lselect")
        {
            task.taskType = TaskIdentify;
            task.identifyTime = 15;   // Default for Philips Hue bridge
        }
        else if (alert == "blink")
        {
            task.taskType = TaskTriggerEffect;
            task.effectIdentifier = 0x00;
        }
        else if (alert == "breathe")
        {
            task.taskType = TaskTriggerEffect;
            task.effectIdentifier = 0x01;
        }
        else if (alert == "okay")
        {
            task.taskType = TaskTriggerEffect;
            task.effectIdentifier = 0x02;
        }
        else if (alert == "channelchange")
        {
            task.taskType = TaskTriggerEffect;
            task.effectIdentifier = 0x0b;
        }
        else if (alert == "finish")
        {
            task.taskType = TaskTriggerEffect;
            task.effectIdentifier = 0xfe;
        }
        else if (alert == "stop")
        {
            task.taskType = TaskTriggerEffect;
            task.effectIdentifier = 0xff;
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/groups/%1/action/alert").arg(id), QString("invalid value, %1, for parameter, alert").arg(map["alert"].toString())));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

        group->alert = QLatin1String("alert");
        taskToLocalData(task);

        if ((task.taskType == TaskIdentify && addTaskIdentify(task, task.identifyTime)) ||
            (task.taskType == TaskTriggerEffect && addTaskTriggerEffect(task, task.effectIdentifier)))
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

            TaskItem task;
            copyTaskReq(taskRef, task);
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
                if (hasOn && item && group->isOn() != item->toBool())
                {
                    item->setValue(group->isOn());
                    Event e(RLights, RStateOn, i->id(), item);
                    enqueueEvent(e);
                    modified = true;
                }

                item = i->item(RStateBri);
                if (hasBri && item && group->level != item->toNumber())
                {
                    item->setValue(group->level);
                    Event e(RLights, RStateBri, i->id(), item);
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

                        quint16 colorX = static_cast<quint16>(x * 65535.0); // current X in range 0 .. 65279
                        quint16 colorY = static_cast<quint16>(y * 65535.0); // current Y in range 0 .. 65279

                        if (colorX > 65279) { colorX = 65279; }
                        else if (colorX == 0) { colorX = 1; }

                        if (colorY > 65279) { colorY = 65279; }
                        else if (colorY == 0) { colorY = 1; }

                        item = i->item(RStateX);
                        if (item && item->toNumber() != colorX)
                        {
                            item->setValue(colorX);
                            Event e(RLights, RStateX, i->id(), item);
                            enqueueEvent(e);
                            modified = true;
                        }

                        item = i->item(RStateY);
                        if (item && item->toNumber() != colorY)
                        {
                            item->setValue(colorY);
                            Event e(RLights, RStateY, i->id(), item);
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
                        DBG_Assert(item);

                        if (item && item->toNumber() != ct)
                        {
                            item->setValue(ct);
                            Event e(RLights, RStateCt, i->id(), item);
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

                        if (item && item->toNumber() != group->hue)
                        {
                            item->setValue(group->hue);
                            Event e(RLights, RStateHue, i->id(), item);
                            enqueueEvent(e);

                            item = i->item(RStateSat);

                            if (item && !hasXy && !hasSat)
                            {
                                double r, g, b;
                                double x, y;
                                double h = ((360.0 / 65535.0) * hue);
                                double s = item->toNumber() / 255.0;
                                double v = 1.0;

                                Hsv2Rgb(&r, &g, &b, h, s, v);
                                Rgb2xy(&x, &y, r, g, b);

                                if (x < 0) { x = 0; }
                                else if (x > 1) { x = 1; }

                                if (y < 0) { y = 0; }
                                else if (y > 1) { y = 1; }

                                DBG_Printf(DBG_INFO, "x: %f, y: %f\n", x, y);
                                item = i->item(RStateX);
                                if (item)
                                {
                                    x = x * 65535.0;
                                    if (x > 65279) { x = 65279; }
                                    else if (x < 1) { x = 1; }
                                    item->setValue(static_cast<quint16>(x));
                                }
                                item = i->item(RStateY);
                                if (item)
                                {
                                    y = y * 65535.0;
                                    if (y > 65279) { y = 65279; }
                                    else if (y < 1) { y = 1; }
                                    item->setValue(static_cast<quint16>(y));
                                }
                            }
                        }
                    }
                    // TODO case hasHue && hasSat not handled
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

                        if (item && item->toNumber() != group->sat)
                        {
                            item->setValue(group->sat);
                            Event e(RLights, RStateSat, i->id(), item);
                            enqueueEvent(e);

                            if (!hasXy)
                            {
                                quint16 enhancedHue = 0;
                                {
                                    ResourceItem *item2 = i->item(RStateHue);
                                    if (item2)
                                    {
                                        enhancedHue = static_cast<quint16>(item2->toNumber());
                                    }
                                }

                                double r, g, b;
                                double x, y;
                                double h = (!hasHue) ? ((360.0 / 65535.0) * enhancedHue) : ((360.0 / 65535.0) * hue);
                                double s = sat / 254.0;
                                double v = 1.0;

                                Hsv2Rgb(&r, &g, &b, h, s, v);
                                Rgb2xy(&x, &y, r, g, b);

                                if (x < 0) { x = 0; }
                                else if (x > 1) { x = 1; }

                                if (y < 0) { y = 0; }
                                else if (y > 1) { y = 1; }

                                DBG_Printf(DBG_INFO, "x: %f, y: %f\n", x, y);
                                item = i->item(RStateX);
                                if (item)
                                {
                                    x = x * 65535.0;
                                    if (x > 65279) { x = 65279; }
                                    else if (x < 1) { x = 1; }
                                    item->setValue(static_cast<quint16>(x));
                                }
                                item = i->item(RStateY);
                                if (item)
                                {
                                    y = y * 65535.0;
                                    if (y > 65279) { y = 65279; }
                                    else if (y < 1) { y = 1; }
                                    item->setValue(static_cast<quint16>(y));
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

    if (!group || (group->state() == Group::StateDeleted) || (group->address() == gwGroup0))
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString("/groups/%1").arg(id), QString("resource, /groups/%1, not available").arg(id)));
        return REQ_READY_SEND;
    }

    group->setState(Group::StateDeleted);
    group->m_deviceMemberships.clear();

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
            i->setNeedSaveDatabase(true);
            groupInfo->actions &= ~GroupInfo::ActionAddToGroup; // sanity
            groupInfo->actions |= GroupInfo::ActionRemoveFromGroup;
            groupInfo->state = GroupInfo::StateNotInGroup;
        }
    }

    updateGroupEtag(group);
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

    return nullptr;
}

/*! Put all parameters in a map for later json serialization.
    \return true - on success
            false - on error
 */
bool DeRestPluginPrivate::groupToMap(const ApiRequest &req, const Group *group, QVariantMap &map)
{
    if (!group)
    {
        return false;
    }

    QVariantMap action;
    QVariantMap state;
    QVariantList scenes;

    action["on"] = group->isOn();
    action["hue"] = (double)((uint16_t)(group->hueReal * 65535));
    action["effect"] = group->isColorLoopActive() ? QLatin1String("colorloop") : QLatin1String("none");
    action["bri"] = (double)group->level;
    action["sat"] = (double)group->sat;
    action["ct"] = (double)group->colorTemperature;
    action["alert"] = group->alert;
    QVariantList xy;

    double colorX = group->colorX;
    double colorY = group->colorY;
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
    // x = CurrentX / 65536 (CurrentX in the range 0 to 65279 inclusive)
    const double x = colorX / 65535.0; // normalize to 0 .. 1
    const double y = colorY / 65535.0; // normalize to 0 .. 1
    xy.append(x);
    xy.append(y);
    action["xy"] = xy;
    action["colormode"] = group->colormode; // TODO

    for (int i = 0; i < group->itemCount(); i++)
    {
        const ResourceItem *item = group->itemForIndex(i);
        DBG_Assert(item != nullptr);
        if (item->descriptor().suffix == RStateAllOn) { state["all_on"] = item->toBool(); }
        else if (item->descriptor().suffix == RStateAnyOn) { state["any_on"] = item->toBool(); }
        else if (item->descriptor().suffix == RActionScene) { action["scene"] = item->toVariant(); }
        else if (item->descriptor().suffix == RAttrName) { map["name"] = item->toString(); }
        else if (item->descriptor().suffix == RAttrType) { map["type"] = item->toString(); }
        else if (item->descriptor().suffix == RAttrClass) { map["class"] = item->toString(); }
        else if (item->descriptor().suffix == RAttrUniqueId) { map["uniqueid"] = item->toString(); }
    }
    if (map["type"] != QLatin1String("Room"))
    {
        map.remove("class");
    }

    map["id"] = group->id();
    QString etag = group->etag;
    etag.remove('"'); // no quotes allowed in string
    map["etag"] = etag;
    map["action"] = action;
    map["state"] = state;

    // following attributes are only shown for Phoscon App
    if (req.apiVersion() >= ApiVersion_1_DDEL)
    {
        QStringList multis;
        auto m = group->m_multiDeviceIds.begin();
        auto mend = group->m_multiDeviceIds.end();

        for ( ;m != mend; ++m)
        {
            multis.append(*m);
        }

        map["hidden"] = group->hidden;
        map["multideviceids"] = multis;

        QStringList lightsequence;
        auto l = group->m_lightsequence.begin();
        auto lend = group->m_lightsequence.end();

        for ( ;l != lend; ++l)
        {
            lightsequence.append(*l);
        }

        map["lightsequence"] = lightsequence;
    }

    QStringList deviceIds;
    auto d = group->m_deviceMemberships.begin();
    auto dend = group->m_deviceMemberships.end();

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
            scene["lightcount"] = (double)si->lights().size();

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
        QString name = map["name"].toString().trimmed();

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

    scene.id = 1;

    // id
    if (map.contains("id")) // optional
    {
        uint sid = map["id"].toUInt(&ok);
        if (ok && sid < 256)
        {
            scene.id = sid;
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/groups/%1/scenes/id").arg(id), QString("invalid value, %1, for parameter, /groups/%2/scenes/id").arg(map["id"].toString()).arg(id)));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

        Scene *s = getSceneForId(group->address(), sid);

        if (s && s->state == Scene::StateNormal)
        {
            rsp.list.append(errorToMap(ERR_DUPLICATE_EXIST, QString("/groups/%1/scenes").arg(id), QString("resource, /groups/%1/scenes/%2, already exists").arg(id).arg(sid)));
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
        if (s && s->modelId() == QLatin1String("Lighting Switch"))
        {
            ommit = true; // ommit scene 2 and 3 for Lighting Switch
        }
    }

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
        scene.name = tr("Scene %1").arg(scene.id);
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
                state.setColorMode(lightNode->toString(RStateColorMode));
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

    if (rsp.map.isEmpty())
    {
        rsp.str = "{}"; // return empty object
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
                            double x = l->x() / 65535.0;
                            double y = l->y() / 65535.0;
                            if (x > 0.9961) { x = 0.9961; }
                            else if (x < 0) { x = 0; }
                            if (y > 0.9961) { y = 0.9961; }
                            else if (y < 0) { y = 0; }
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
        name = map["name"].toString().trimmed();

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
        uint tt = map["transitiontime"].toUInt(&ok);

        if (ok && tt < 0xFFFFUL)
        {
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

    if (!storeScene(group, scene->id))
    {
        rsp.httpStatus = HttpStatusServiceUnavailable;
        rsp.list.append(errorToMap(ERR_BRIDGE_BUSY, QString("/groups/%1/scenes/%2").arg(gid).arg(sid), QString("gateway busy")));
        return REQ_READY_SEND;
    }

    // search for lights that have their scenes capacity reached or need to be updated
    std::vector<LightNode>::iterator ni = nodes.begin();
    std::vector<LightNode>::iterator nend = nodes.end();
    for (; ni != nend; ++ni)
    {
        LightNode *lightNode = &*ni;
        if (!lightNode->isAvailable())
        {
            continue;
        }

        if (!isLightNodeInGroup(lightNode, group->address()))
        {
            continue;
        }

        bool needModify = false;
        LightState *ls = scene->getLightState(lightNode->id());

        if (!ls)
        {
            LightState lsnew;
            lsnew.setLightId(lightNode->id());

            /*if (lightNode->sceneCapacity() <= 0)
            {
                rsp.list.append(errorToMap(ERR_DEVICE_SCENES_TABLE_FULL, QString("/groups/%1/scenes/lights/%2").arg(gid).arg(lightNode->id()), QString("Could not set scene for %1. Scene capacity of the device is reached.").arg(qPrintable(lightNode->name()))));
            }*/

            scene->addLightState(lsnew);
            ls = scene->getLightState(lightNode->id());
            needModify = true;
        }

        if (!ls)
        {
            continue;
        }

        if (req.sock != nullptr) // this isn't done by a rule (sensor pir control)
        {
            ls->setNeedRead(true);
        }

        lightNode->clearRead(READ_SCENE_DETAILS | READ_SCENES); // prevent reading before writing

        ResourceItem *item = lightNode->item(RStateOn);
        DBG_Assert(item != 0);

        if (item && ls->on() != item->toBool())
        {
            ls->setOn(item->toBool());
            needModify = true;
        }

        item = lightNode->item(RStateBri);

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

            if (item->toString() == QLatin1String("xy") ||
                item->toString() == QLatin1String("hs"))
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
            else if (item->toString() == QLatin1String("ct"))
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

        if (ls->transitionTime() != scene->transitiontime())
        {
            ls->setTransitionTime(scene->transitiontime());
            needModify = true;
        }

        if (needModify)
        {
            queSaveDb(DB_SCENES, DB_LONG_SAVE_DELAY);
        }

        if (req.sock != nullptr)
        {
            ls->tVerified.invalidate(); // invalidate, trigger verify or add
        }
    }

    updateGroupEtag(group);

    rspItemState["id"] = sid;
    rspItem["success"] = rspItemState;
    rsp.list.append(rspItem);
    rsp.httpStatus = HttpStatusOk;

    return REQ_READY_SEND;
}

/*! PUT /api/<apikey>/groups/<group_id>/scenes/<scene_id>/recall
    PUT /api/<apikey>/groups/<group_id>/scenes/next/recall
    PUT /api/<apikey>/groups/<group_id>/scenes/prev/recall
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

    if (req.sock)
    {
        userActivity();
    }

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
    Scene *scene = 0;
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

    scene = ok ? group->getScene(sceneId) : 0;

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

    {
        QString scid = QString::number(sceneId);
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
    uint16_t ct = 0;

    bool hasOn = false;
    bool hasBri = false;
    bool hasTt = false;
    bool hasXy = false;
    bool hasCt = false;

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

    if (map.contains("ct"))
    {
        bool ok;
        ct = map["ct"].toUInt(&ok);

        if (ok && map["ct"].type() == QVariant::Double && (ct < 1000))
        {
            hasCt = true;
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/groups/%1/scenes/%2/lights/%3/state/ct").arg(gid).arg(sid).arg(lid), QString("invalid value, %1, for parameter ct").arg(ct)));
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
            double x = xy[0].toDouble(&ok);
            double y = ok ? xy[1].toDouble() : 0;

            if (!ok || (x < 0.0) || (x > 1.0) || (y < 0.0) || (y > 1.0))
            {
                rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1").arg(lid), QString("invalid value, [%1,%2], for parameter, /lights/%3/xy").arg(x).arg(y).arg(lid)));
                rsp.httpStatus = HttpStatusBadRequest;
                return REQ_READY_SEND;
            }
            else
            {
                hasXy = true;
                xy_x = static_cast<quint16>(x * 65535.0);
                xy_y = static_cast<quint16>(y * 65535.0);

                if (xy_x > 65279) { xy_x = 65279; }
                else if (xy_x == 0) { xy_x = 1; }

                if (xy_y > 65279) { xy_y = 65279; }
                else if (xy_y == 0) { xy_y = 1; }
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
                        l->setColorMode(QLatin1String("xy"));
                        l->setX(xy_x);
                        l->setY(xy_y);
                    }
                    else if (hasCt)
                    {
                        l->setColorMode(QLatin1String("ct"));
                        l->setColorTemperature(ct);
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

void DeRestPluginPrivate::handleGroupEvent(const Event &e)
{
    DBG_Assert(e.resource() == RGroups);
    DBG_Assert(e.what() != nullptr);
    DBG_Assert(e.num() >= 0);
    DBG_Assert(e.num() <= UINT16_MAX);

    if (e.num() < 0 || e.num() > UINT16_MAX)
    {
        return;
    }

    const quint16 groupId = static_cast<quint16>(e.num());
    Group *group = getGroupForId(groupId);

    if (!group)
    {
        return;
    }
    const QDateTime now = QDateTime::currentDateTime();

    if (e.what() == REventCheckGroupAnyOn)
    {
        int on = 0;
        int count = 0;

        std::vector<LightNode>::const_iterator i = nodes.begin();
        std::vector<LightNode>::const_iterator end = nodes.end();

        for (; i != end; ++i)
        {
            if (!isLightNodeInGroup(&*i, groupId))
            {
                continue;
            }

            const ResourceItem *item = i->item(RStateOn);

            if (i->isAvailable() && item)
            {
                count++;
                if (item->toBool()) { on++; }
            }
        }

        ResourceItem *item = group->item(RStateAllOn);
        DBG_Assert(item != nullptr);
        if (item && (item->toBool() != (on > 0 && on == count) || !item->lastSet().isValid()))
        {
            item->setValue(on > 0 && on == count);
            updateGroupEtag(group);
            Event e(RGroups, RStateAllOn, group->address());
            enqueueEvent(e);
        }
        item = group->item(RStateAnyOn);
        DBG_Assert(item != nullptr);
        if (item && (item->toBool() != (on > 0) || !item->lastSet().isValid()))
        {
            item->setValue(on > 0);
            updateGroupEtag(group);
            Event e(RGroups, RStateAnyOn, group->address());
            enqueueEvent(e);
        }
        return;
    }

    // push state updates through websocket
    if (strncmp(e.what(), "state/", 6) == 0)
    {
        ResourceItem *item = group->item(e.what());
        if (item)
        {
            if (group->lastStatePush.isValid() && item->lastSet() < group->lastStatePush)
            {
                DBG_Printf(DBG_INFO_L2, "discard group state push for %s: %s (already pushed)\n", qPrintable(e.id()), e.what());
                webSocketServer->flush(); // force transmit send buffer
                return; // already pushed
            }

            QVariantMap map;
            map["t"] = QLatin1String("event");
            map["e"] = QLatin1String("changed");
            map["r"] = QLatin1String("groups");
            map["id"] = group->id();
            QVariantMap state;

            for (int i = 0; i < group->itemCount(); i++)
            {
                item = group->itemForIndex(i);
                const ResourceItemDescriptor &rid = item->descriptor();

                if (strncmp(rid.suffix, "state/", 6) == 0)
                {
                    const char *key = item->descriptor().suffix + 6;

                    if (item->lastSet().isValid() && (gwWebSocketNotifyAll || (item->lastChanged().isValid() && item->lastChanged() >= group->lastStatePush)))
                    {
                        state[key] = item->toVariant();
                    }
                }

            }

            if (!state.isEmpty())
            {
                map["state"] = state;
                webSocketServer->broadcastTextMessage(Json::serialize(map));
                group->lastStatePush = now;
            }
        }
    }
    else if (strncmp(e.what(), "attr/", 5) == 0)
    {
        ResourceItem *item = group->item(e.what());
        if (item)
        {
            QVariantMap map;
            map["t"] = QLatin1String("event");
            map["e"] = QLatin1String("changed");
            map["r"] = QLatin1String("groups");
            map["id"] = group->id();
            map[e.what() + 5] = item->toVariant();

            webSocketServer->broadcastTextMessage(Json::serialize(map));
        }
    }
    else if (e.what() == REventAdded)
    {
        QVariantMap map;
        map["t"] = QLatin1String("event");
        map["e"] = QLatin1String("added");
        map["r"] = QLatin1String("groups");
        map["id"] = e.id();

        webSocketServer->broadcastTextMessage(Json::serialize(map));
    }
    else if (e.what() == REventDeleted)
    {
        QVariantMap map;
        map["t"] = QLatin1String("event");
        map["e"] = QLatin1String("deleted");
        map["r"] = QLatin1String("groups");
        map["id"] = e.id();

        webSocketServer->broadcastTextMessage(Json::serialize(map));
    }
}
