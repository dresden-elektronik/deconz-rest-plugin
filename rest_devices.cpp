/*
 * Copyright (c) 2013-2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QString>
#include <QVariantMap>
#include <QProcess>
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "product_match.h"
#include "rest_devices.h"
#include "utils/utils.h"

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
    // GET /api/<apikey>/devices
    if (req.hdr.pathComponentsCount() == 3 && req.hdr.httpMethod() == HttpGet)
    {
        return getAllDevices(req, rsp);
    }
    // GET /api/<apikey>/devices/<uniqueid>
    else if (req.hdr.pathComponentsCount() == 4 && req.hdr.httpMethod() == HttpGet)
    {
        return getDevice(req, rsp);
    }
    // GET /api/<apikey>/devices/<uniuqueid>/introspect
    else if (req.hdr.pathComponentsCount() == 5 && req.hdr.httpMethod() == HttpGet && req.hdr.pathAt(4) == QLatin1String("introspect"))
    {
        return RIS_GetDeviceIntrospect(req, rsp);
    }
    // GET /api/<apikey>/devices/<uniqueid>/[<prefix>/]<item>/introspect
    else if (req.hdr.pathComponentsCount() > 5 && req.hdr.httpMethod() == HttpGet &&
             req.hdr.pathAt(req.hdr.pathComponentsCount() - 1) == QLatin1String("introspect"))
    {
        return RIS_GetDeviceItemIntrospect(req, rsp);
    }
    // PUT /api/<apikey>/devices/<uniqueid>/installcode
    else if (req.hdr.pathComponentsCount() == 5 && req.hdr.httpMethod() == HttpPut && req.hdr.pathAt(4) == QLatin1String("installcode"))
    {
        return putDeviceInstallCode(req, rsp);
    }

    return REQ_NOT_HANDLED;
}

/*! Deletes a Sensor as a side effect it will be removed from the REST API
    and a ZDP reset will be send if possible.
 */
bool deleteSensor(Sensor *sensor, DeRestPluginPrivate *plugin)
{
    if (sensor && plugin && sensor->deletedState() == Sensor::StateNormal)
    {
        sensor->setDeletedState(Sensor::StateDeleted);
        sensor->setNeedSaveDatabase(true);
        sensor->setResetRetryCount(10);

        plugin->enqueueEvent(Event(sensor->prefix(), REventDeleted, sensor->id()));
        return true;
    }

    return false;
}

/*! Deletes a LightNode as a side effect it will be removed from the REST API
    and a ZDP reset will be send if possible.
 */
bool deleteLight(LightNode *lightNode, DeRestPluginPrivate *plugin)
{
    if (lightNode && plugin && lightNode->state() == LightNode::StateNormal)
    {
        lightNode->setState(LightNode::StateDeleted);
        lightNode->setResetRetryCount(10);
        lightNode->setNeedSaveDatabase(true);

        // delete all group membership from light (todo this is messy)
        for (auto &group : lightNode->groups())
        {
            //delete Light from all scenes.
            plugin->deleteLightFromScenes(lightNode->id(), group.id);

            //delete Light from all groups
            group.actions &= ~GroupInfo::ActionAddToGroup;
            group.actions |= GroupInfo::ActionRemoveFromGroup;
            if (group.state != GroupInfo::StateNotInGroup)
            {
                group.state = GroupInfo::StateNotInGroup;
            }
        }

        plugin->enqueueEvent(Event(lightNode->prefix(), REventDeleted, lightNode->id()));
        return true;
    }

    return false;
}

/*! Deletes all resources related to a device from the REST API.
 */
bool RestDevices::deleteDevice(quint64 extAddr)
{
    int count = 0;

    for (auto &sensor : plugin->sensors)
    {
        if (sensor.address().ext() == extAddr && deleteSensor(&sensor, plugin))
        {
            count++;
        }
    }

    for (auto &lightNode : plugin->nodes)
    {
        if (lightNode.address().ext() == extAddr && deleteLight(&lightNode, plugin))
        {
            count++;
        }
    }

    if (count > 0)
    {
        plugin->queSaveDb(DB_SENSORS | DB_LIGHTS | DB_GROUPS | DB_SCENES, DB_SHORT_SAVE_DELAY);
    }

    // delete device entry, regardless if REST resources exists
    plugin->deleteDeviceDb(generateUniqueId(extAddr, 0, 0));

    return count > 0;
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
        if (s.uniqueId().indexOf(uniqueid) != 0) { continue; }
        if (s.type().startsWith(QLatin1String("CLIP"))) { continue; }

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

/*! GET /api/<apikey>/devices/<uniqueid>/introspect
    \return REQ_READY_SEND
            REQ_NOT_HANDLED

    Unstable API to experiment: don't use in production!
 */
int RIS_GetDeviceIntrospect(const ApiRequest &req, ApiResponse &rsp)
{
    Q_UNUSED(req)
    rsp.str = QLatin1String("[\"introspect\": false]");
    return REQ_READY_SEND;
}

/*! Returns string form of a ApiDataType.
 */
QLatin1String RIS_DataTypeToString(ApiDataType type)
{
    static const std::array<QLatin1String, 14> map = {
         QLatin1String("unknown"),
         QLatin1String("bool"),
         QLatin1String("uint8"),
         QLatin1String("uint16"),
         QLatin1String("uint32"),
         QLatin1String("uint64"),
         QLatin1String("int8"),
         QLatin1String("int16"),
         QLatin1String("int32"),
         QLatin1String("int64"),
         QLatin1String("double"),
         QLatin1String("string"),
         QLatin1String("time"),
         QLatin1String("timepattern")
    };

    if (type < map.size())
    {

        return map[type];
    }

    return map[0];
}

/*! Returns string form of \c state/buttonevent action part.
 */
QLatin1String RIS_ButtonEventActionToString(int buttonevent)
{
    const uint action = buttonevent % 1000;

    static std::array<QLatin1String, 11> map = {
         QLatin1String("INITIAL_PRESS"),
         QLatin1String("HOLD"),
         QLatin1String("SHORT_RELEASE"),
         QLatin1String("LONG_RELEASE"),
         QLatin1String("DOUBLE_PRESS"),
         QLatin1String("TREBLE_PRESS"),
         QLatin1String("QUADRUPLE_PRESS"),
         QLatin1String("SHAKE"),
         QLatin1String("DROP"),
         QLatin1String("TILT"),
         QLatin1String("MANY_PRESS")
    };

    if (action < map.size())
    {

        return map[action];
    }

    return QLatin1String("UNKNOWN");
}

/*! Returns generic introspection for a \c ResourceItem.
 */
QVariantMap RIS_IntrospectGenericItem(const ResourceItemDescriptor &rid)
{
    QVariantMap result;

    result[QLatin1String("type")] = RIS_DataTypeToString(rid.type);

    if (rid.validMin != 0 || rid.validMax != 0)
    {
        result[QLatin1String("minval")] = rid.validMin;
        result[QLatin1String("maxval")] = rid.validMax;
    }

    return result;
}

/*! Returns introspection for \c state/buttonevent.
 */
QVariantMap RIS_IntrospectButtonEventItem(const ResourceItemDescriptor &rid, const Resource *r)
{
    QVariantMap result = RIS_IntrospectGenericItem(rid);

    Q_ASSERT(r->prefix() == RSensors);
    const auto *sensor = static_cast<const Sensor*>(r);

    if (!sensor)
    {
        return result;
    }

    // TODO dependency on plugin needs to be removed to make this testable
    const auto &buttonMapButtons = plugin->buttonMeta;
    const auto &buttonMapData = plugin->buttonMaps;
    const auto &buttonMapForModelId = plugin->buttonProductMap;

    const auto *buttonData = BM_ButtonMapForProduct(productHash(r), buttonMapData, buttonMapForModelId);

    if (!buttonData)
    {
        return result;
    }

    int buttonBits = 0; // button 1 = 1 << 1, button 2 = 1 << 2 ...

    {
        QVariantMap values;

        for (const auto &btn : buttonData->buttons)
        {
            buttonBits |= 1 << int(btn.button / 1000);

            QVariantMap m;
            m[QLatin1String("button")] = int(btn.button / 1000);
            m[QLatin1String("action")] = RIS_ButtonEventActionToString(btn.button);
            values[QString::number(btn.button)] = m;
        }
        result[QLatin1String("values")] = values;
    }

    const auto buttonsMeta = std::find_if(buttonMapButtons.cbegin(), buttonMapButtons.cend(),
                                          [buttonData](const auto &meta){ return meta.buttonMapRef.hash == buttonData->buttonMapRef.hash; });

    QVariantMap buttons;

    if (buttonsMeta != buttonMapButtons.cend())
    {
        for (const auto &button : buttonsMeta->buttons)
        {
            QVariantMap m;
            m[QLatin1String("name")] = button.name;
            buttons[QString::number(button.button)] = m;
        }
    }
    else // fallback if no "buttons" is defined in the button map, generate a generic one
    {
        for (int i = 1 ; i < 32; i++)
        {
            if (buttonBits & (1 << i))
            {
                QVariantMap m;
                m[QLatin1String("name")] = QString("Button %1").arg(i);
                buttons[QString::number(i)] = m;
            }
        }
    }

    result[QLatin1String("buttons")] = buttons;

    return result;
}

/*! /api/<apikey>/devices/<uniqueid>/[<prefix>/]<item>/introspect

    Fills ResourceItemDescriptor \p rid for the '[<prefix>/]<item>' part of the URL.

    \note The verification that the URL has enough segments must be done by the caller.
*/
bool RIS_ResourceItemDescriptorFromHeader(const QHttpRequestHeader &hdr, ResourceItemDescriptor *rid)
{
    const auto last = hdr.pathAt(hdr.pathComponentsCount() - 2);
    const char *beg = hdr.pathAt(4).data();
    const char *end = last.data() + last.size();

    if (beg && end && beg < end)
    {
        const QLatin1String suffix(beg, end - beg);

        if (getResourceItemDescriptor(suffix, *rid))
        {
            return true;
        }
    }

    return false;
}

/*! Returns the Resource for a given \p uniqueid.
 */
static Resource *resourceForUniqueId(const QLatin1String &uniqueid)
{
    Resource *r = plugin->getResource(RSensors, uniqueid);

    if (!r)
    {
        plugin->getResource(RLights, uniqueid);
    }

    return r;
}

/*! GET /api/<apikey>/devices/<uniqueid>/[<prefix>/]<item>/introspect
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int RIS_GetDeviceItemIntrospect(const ApiRequest &req, ApiResponse &rsp)
{
    rsp.httpStatus = HttpStatusOk;
    const Resource *r = resourceForUniqueId(req.hdr.pathAt(3));

    if (!r)
    {
        rsp.httpStatus = HttpStatusNotFound;
        return REQ_READY_SEND;
    }

    ResourceItemDescriptor rid;

    if (!RIS_ResourceItemDescriptorFromHeader(req.hdr, &rid))
    {
        rsp.httpStatus = HttpStatusNotFound;
        return REQ_READY_SEND;
    }

    if (rid.suffix == RStateButtonEvent)
    {
        rsp.map = RIS_IntrospectButtonEventItem(rid, r);
    }
    else
    {
        rsp.map = RIS_IntrospectGenericItem(rid);
    }

    return REQ_READY_SEND;
}

/*! PUT /api/<apikey>/devices/<uniqueid>/installcode
    \return REQ_READY_SEND
            REQ_NOT_HANDLED

    Adds an Zigbee 3.0 Install Code for a device to let it securely join.
    Unstable API to experiment: don't use in production!
 */
int RestDevices::putDeviceInstallCode(const ApiRequest &req, ApiResponse &rsp)
{
    DBG_Assert(req.path.size() == 5);

    bool ok;
    const QString &uniqueid = req.path[3];

    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();

    if (!ok || map.isEmpty())
    {
        rsp.list.append(plugin->errorToMap(ERR_INVALID_JSON, QString("/devices/%1/installcode").arg(uniqueid), QString("body contains invalid JSON")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    // installcode
    if (map.contains("installcode"))
    {
        QString installCode = map["installcode"].toString().trimmed();

        if (map["installcode"].type() == QVariant::String && !installCode.isEmpty())
        {
            // TODO process install code

            // MAC: f8f005fffff2b37a
            // IC: 07E2EE0C820EFE0C0C21742E0A037C07
            // CRC-16 7AC5

            QProcess cli;
            cli.start("hashing-cli", QStringList() << "-i" << installCode);
            if (!cli.waitForStarted(2000))
            {
                rsp.list.append(plugin->errorToMap(ERR_INTERNAL_ERROR, QString("/devices"), QString("internal error, %1, occured").arg(cli.error())));
                rsp.httpStatus = HttpStatusServiceUnavailable;
                return REQ_READY_SEND;
            }

            if (!cli.waitForFinished(2000))
            {
                rsp.list.append(plugin->errorToMap(ERR_INTERNAL_ERROR, QString("/devices"), QString("internal error, %1, occured").arg(cli.error())));
                rsp.httpStatus = HttpStatusServiceUnavailable;
                return REQ_READY_SEND;
            }

            QByteArray mmoHash;
            while (!cli.atEnd())
            {
                const QByteArray result = cli.readLine();
                if (result.contains("Hash Result:"))
                {
                    const auto ls = result.split(':');
                    DBG_Assert(ls.size() == 2);
                    if (ls.size() == 2)
                    {
                        mmoHash = ls[1].trimmed();
                        break;
                    }
                }
            }

            if (mmoHash.isEmpty())
            {
                rsp.list.append(plugin->errorToMap(ERR_INTERNAL_ERROR, QLatin1String("/devices"), QLatin1String("internal error, failed to calc mmo hash, occured")));
                rsp.httpStatus = HttpStatusServiceUnavailable;
                return REQ_READY_SEND;
            }

#if DECONZ_LIB_VERSION >= 0x010B00
            QVariantMap m;
            m["mac"] = uniqueid.toULongLong(&ok, 16);
            m["key"] = mmoHash;
            if (ok && mmoHash.size() == 32)
            {
                ok = deCONZ::ApsController::instance()->setParameter(deCONZ::ParamLinkKey, m);
            }
#endif
            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState["installcode"] = installCode;
            rspItemState["mmohash"] = mmoHash;
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);
            rsp.httpStatus = HttpStatusOk;
            return REQ_READY_SEND;
        }
        else
        {
            rsp.list.append(plugin->errorToMap(ERR_INVALID_VALUE, QString("/devices"), QString("invalid value, %1, for parameter, installcode").arg(installCode)));
            rsp.httpStatus = HttpStatusBadRequest;
        }
    }
    else
    {
        rsp.list.append(plugin->errorToMap(ERR_MISSING_PARAMETER, QString("/devices/%1/installcode").arg(uniqueid), QString("missing parameters in body")));
        rsp.httpStatus = HttpStatusBadRequest;
    }

    return REQ_READY_SEND;
}
