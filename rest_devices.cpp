/*
 * Copyright (c) 2013-2025 dresden elektronik ingenieurtechnik gmbh.
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
#include "database.h"
#include "device_descriptions.h"
#include "device_ddf_bundle.h"
#include "deconz/atom_table.h"
#include "deconz/u_assert.h"
#include "deconz/u_sstream_ex.h"
#include "deconz/u_memory.h"
#include "rest_devices.h"
#include "utils/scratchmem.h"
#include "json.h"
#include "crypto/mmohash.h"
#include "utils/ArduinoJson.h"
#include "utils/utils.h"

using JsonDoc = StaticJsonDocument<1024 * 1024 * 2>; // 2 megabytes


static void putJsonQVariantValue(JsonObject &obj, std::string key, const QVariant &value);
static void putJsonArrayQVariantValue(JsonArray &arr, const QVariant &value);


static RestDevicesPrivate *priv_;

class RestDevicesPrivate
{
public:
    JsonDoc json;

    char jsonBuffer[1024 * 1024];
};

RestDevices::RestDevices(QObject *parent) :
    QObject(parent)
{
    d = new RestDevicesPrivate;
    priv_ = d;
    plugin = qobject_cast<DeRestPluginPrivate*>(parent);
    Q_ASSERT(plugin);
}

RestDevices::~RestDevices()
{
    priv_ = nullptr;
    delete d;
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
    // PUT /api/<apikey>/devices/<uniqueid>/ddf/reload
    else if (req.path.size() == 6 && req.hdr.method() == QLatin1String("PUT") && req.path[4] == QLatin1String("ddf") && req.path[5] == QLatin1String("reload"))
    {
        return putDeviceReloadDDF(req, rsp);
    }
    // PUT /api/<apikey>/devices/<uniqueid>/ddf/policy
    else if (req.path.size() == 6 && req.hdr.method() == QLatin1String("PUT") && req.path[4] == QLatin1String("ddf") && req.path[5] == QLatin1String("policy"))
    {
        return putDeviceSetDDFPolicy(req, rsp);
    }
    // GET /api/<apikey>/devices/<uniqueid>/ddf
    else if (req.hdr.pathComponentsCount() == 5 && req.hdr.httpMethod() == HttpGet && req.hdr.pathAt(4) == QLatin1String("ddf"))
    {
        return getDeviceDDF(req, rsp);
    }
    // GET /api/<apikey>/devices/<uniqueid>/ddffull
    else if (req.hdr.pathComponentsCount() == 5 && req.hdr.httpMethod() == HttpGet && req.hdr.pathAt(4) == QLatin1String("ddffull"))
    {
        return getDeviceDDF(req, rsp);
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

static DeviceKey getDeviceKey(QLatin1String uniqueid)
{
    DeviceKey result = 0;
    const char *str = uniqueid.data();

    if (uniqueid.size() < 23)
        return result;

    // 00:11:22:33:44:55:66:77
    for (int pos = 0; pos < 23; pos++)
    {
        uint64_t ch = (unsigned)str[pos];
        if (ch == ':' && (pos % 3) == 2) // ensure color only every 3rd pos
            continue;

        result <<= 4;

        if      (ch >= '0' && ch <= '9') ch = ch - '0';
        else if (ch >= 'a' && ch <= 'f') ch = (ch - 'a') + 10;
        else if (ch >= 'A' && ch <= 'F') ch = (ch - 'A') + 10;
        else
        {
            result = 0;
            break;
        }

        result |= (ch & 0x0F);
    }

    return result;
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

        enqueueEvent(Event(sensor->prefix(), REventDeleted, sensor->id()));
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

        enqueueEvent(Event(lightNode->prefix(), REventDeleted, lightNode->id()));
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

    enqueueEvent(Event(RDevices, REventDeleted, 0, extAddr));

    return count > 0;
}

void RestDevices::handleEvent(const Event &event)
{
    if (event.resource() == RDevices && event.what() == REventDeleted)
    {
        DEV_RemoveDevice(plugin->m_devices, event.deviceKey());
    }
}

/*! GET /api/<apikey>/devices
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int RestDevices::getAllDevices(const ApiRequest &req, ApiResponse &rsp)
{
    Q_UNUSED(req)

    rsp.httpStatus = HttpStatusOk;

    for (const auto &d : plugin->m_devices)
    {
        Q_ASSERT(d);
        rsp.list.push_back(d->item(RAttrUniqueId)->toString());
    }

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

    const auto deviceKey = extAddressFromUniqueId(req.hdr.pathAt(3));

    Device *device = DEV_GetDevice(plugin->m_devices, deviceKey);

    rsp.httpStatus = device ? HttpStatusOk : HttpStatusNotFound;

    if (!device)
    {
        return REQ_READY_SEND;
    }

    const DeviceDescription &ddf = plugin->deviceDescriptions->get(device);

    if (ddf.isValid())
    {
        rsp.map["productid"] = ddf.product;
    }

    {
        const ResourceItem *ddfPolicyItem = device->item(RAttrDdfPolicy);
        if (ddfPolicyItem)
        {
            rsp.map["ddf_policy"] = ddfPolicyItem->toString();
        }
    }

    if (ddf.storageLocation == deCONZ::DdfBundleLocation || ddf.storageLocation == deCONZ::DdfBundleUserLocation)
    {
        const ResourceItem *ddfHashItem = device->item(RAttrDdfHash);
        if (ddfHashItem && ddfHashItem->toCString()[0] != '\0')
        {
            rsp.map["ddf_hash"] = ddfHashItem->toString();
        }
    }

    QVariantList subDevices;

    for (const auto &sub : device->subDevices())
    {
        QVariantMap map;

        for (int i = 0; i < sub->itemCount(); i++)
        {
            auto *item = sub->itemForIndex(i);
            Q_ASSERT(item);

            if (item->descriptor().suffix == RStateLastUpdated ||
                item->descriptor().suffix == RAttrId)
            {
                continue;
            }

            if (!item->isPublic())
            {
                continue;
            }

            const auto ls = QString(QLatin1String(item->descriptor().suffix)).split(QLatin1Char('/'));

            if (ls.size() == 2)
            {
                if (item->descriptor().suffix == RAttrLastSeen || item->descriptor().suffix == RAttrLastAnnounced ||
                    item->descriptor().suffix == RAttrManufacturerName || item->descriptor().suffix == RAttrModelId ||
                    item->descriptor().suffix == RAttrSwVersion || item->descriptor().suffix == RAttrName)
                {
                    if (!rsp.map.contains(ls.at(1)))
                    {
                        rsp.map[ls.at(1)] = item->toString(); // top level attribute
                    }
                }
                else if (ls.at(0) == QLatin1String("attr"))
                {
                    map[ls.at(1)] = item->toVariant(); // sub device top level attribute
                }
                else
                {
                    QVariantMap m2;
                    if (map.contains(ls.at(0)))
                    {
                        m2 = map[ls.at(0)].toMap();
                    }

                    QVariantMap itemMap;

                    itemMap[QLatin1String("value")] = item->toVariant();

                    QDateTime dt = item->lastChanged().isValid() ? item->lastChanged().toUTC() : item->lastSet().toUTC();
                    // UTC in msec resolution
                    dt.setOffsetFromUtc(0);
                    itemMap[QLatin1String("lastupdated")] = dt.toString(QLatin1String("yyyy-MM-ddTHH:mm:ssZ"));


                    m2[ls.at(1)] = itemMap;
                    map[ls.at(0)] = m2;
                }
            }
        }

        subDevices.push_back(map);
    }

    rsp.map["uniqueid"] = device->item(RAttrUniqueId)->toString();
    rsp.map["subdevices"] = subDevices;

    return REQ_READY_SEND;
}

static void putJsonArrayQVariantValue(JsonArray &arr, const QVariant &value)
{
    if (value.type() == QVariant::String)
    {
        arr.add(value.toString().toStdString());
    }
    else if (value.type() == QVariant::Bool)
    {
        arr.add(value.toBool());
    }
    else if (value.type() == QVariant::Double)
    {
        arr.add(value.toDouble());
    }
    else if (value.type() == QVariant::Int)
    {
        arr.add(value.toInt());
    }
    else if (value.type() == QVariant::UInt)
    {
        arr.add(value.toUInt());
    }
    else if (value.type() == QVariant::ULongLong)
    {
        arr.add(uint64_t(value.toULongLong()));
    }
    else if (value.type() == QVariant::LongLong)
    {
        arr.add(int64_t(value.toLongLong()));
    }
    else if (value.type() == QVariant::List)
    {
        JsonArray arr1 = arr.createNestedArray();
        const QVariantList ls = value.toList();

        for (const auto &v : ls)
        {
            putJsonArrayQVariantValue(arr1, v);
        }
    }
    else if (value.type() == QVariant::Map)
    {
        JsonObject obj1 = arr.createNestedObject();
        const QVariantMap map = value.toMap();

        auto i = map.constBegin();
        const auto end = map.constEnd();
        for (; i != end; ++i)
        {
            putJsonQVariantValue(obj1, i.key().toStdString(), i.value());
        }
    }
    else
    {
        DBG_Printf(DBG_DDF, "DDF TODO %s:%d arr add type: %s\n", __FILE__, __LINE__, QVariant::typeToName(value.type()));
    }
}

static void putJsonQVariantValue(JsonObject &obj, std::string key, const QVariant &value)
{
    if (value.type() == QVariant::String)
    {
        obj[key] = value.toString().toStdString();
    }
    else if (value.type() == QVariant::Bool)
    {
        obj[key] = value.toBool();
    }
    else if (value.type() == QVariant::Double)
    {
        obj[key] = value.toDouble();
    }
    else if (value.type() == QVariant::Int)
    {
        obj[key] = value.toInt();
    }
    else if (value.type() == QVariant::UInt)
    {
        obj[key] = value.toUInt();
    }
    else if (value.type() == QVariant::ULongLong)
    {
        obj[key] = uint64_t(value.toULongLong());
    }
    else if (value.type() == QVariant::LongLong)
    {
        obj[key] = int64_t(value.toLongLong());
    }
    else if (value.type() == QVariant::List)
    {
        JsonArray arr = obj.createNestedArray(key);
        const QVariantList ls = value.toList();

        for (const auto &v : ls)
        {
            putJsonArrayQVariantValue(arr, v);
        }
    }
    else if (value.type() == QVariant::Map)
    {
        JsonObject obj1 = obj.createNestedObject(key);
        const QVariantMap map = value.toMap();

        auto i = map.constBegin();
        const auto end = map.constEnd();
        for (; i != end; ++i)
        {
            putJsonQVariantValue(obj1, i.key().toStdString(), i.value());
        }
    }
    else
    {
        DBG_Printf(DBG_DDF, "DDF TODO %s:%d obj.%s type: %s\n", __FILE__, __LINE__, key.c_str(), QVariant::typeToName(value.type()));
    }
}

static void putItemParameter(JsonObject &item, const char *name, const QVariantMap &param)
{
    JsonObject parse = item.createNestedObject(name);

    const auto end = param.constEnd();
    for (auto cur = param.constBegin(); cur != end; cur++)
    {
        if (cur.key() == QLatin1String("eval"))
        {
            // no script cached 'eval' value
            if (!param.contains(QLatin1String("script")))
            {
                putJsonQVariantValue(parse, "eval", cur.value());
            }
        }
        else
        {
            putJsonQVariantValue(parse, cur.key().toStdString(), cur.value());
        }
    }
}

bool ddfSerializeV1(JsonDoc &doc, const DeviceDescription &ddf, char *buf, size_t bufsize, bool ddfFull, bool prettyPrint)
{
    doc.clear();

    doc["schema"] = "devcap1.schema.json";

    if (ddf.manufacturerNames.size() == 1)
    {
        doc["manufacturername"] = ddf.manufacturerNames.front().toStdString();
    }
    else
    {
        JsonArray arr = doc.createNestedArray("manufacturername");
        for (const QString &i : ddf.manufacturerNames)
        {
            arr.add(i.toStdString());
        }
    }

    if (ddf.modelIds.size() == 1)
    {
        doc["modelid"] = ddf.modelIds.front().toStdString();
    }
    else
    {
        JsonArray arr = doc.createNestedArray("modelid");
        for (const QString &i : ddf.modelIds)
        {
            arr.add(i.toStdString());
        }
    }

    if (!ddf.vendor.isEmpty())
    {
        doc["vendor"] = ddf.vendor.toStdString();
    }

    if (!ddf.product.isEmpty())
    {
        doc["product"] = ddf.product.toStdString();
    }

    if (ddf.sleeper >= 0)
    {
        doc["sleeper"] = ddf.sleeper > 0;
    }

    doc["status"] = ddf.status.toStdString();

    if (!ddf.matchExpr.isEmpty())
    {
        doc["matchexpr"] = ddf.matchExpr.toStdString();
    }

    {
        JsonArray subDevices = doc.createNestedArray("subdevices");

        for (const DeviceDescription::SubDevice &sub : ddf.subDevices)
        {
            JsonObject subDevice = subDevices.createNestedObject();

            subDevice["type"] = sub.type.toStdString();
            subDevice["restapi"] = sub.restApi.toStdString();

            JsonArray uuid = subDevice.createNestedArray("uuid");
            for (const QString &i : sub.uniqueId)
            {
                uuid.add(i.toStdString());
            }

            if (!sub.meta.isEmpty())
            {
                putJsonQVariantValue(subDevice, "meta", sub.meta);
            }

            if (isValid(sub.fingerPrint))
            {
                // "fingerprint": { "profile": "0x0104", "device": "0x0107", "endpoint": "0x02", "in": ["0x0000", "0x0001", "0x0402"] },

                char buf[16];

                JsonObject fp = subDevice.createNestedObject("fingerprint");

                snprintf(buf, sizeof(buf), "0x%04X", sub.fingerPrint.profileId);
                fp["profile"] = std::string(buf);

                snprintf(buf, sizeof(buf), "0x%04X", sub.fingerPrint.deviceId);
                fp["device"] = std::string(buf);

                snprintf(buf, sizeof(buf), "0x%02X", sub.fingerPrint.endpoint);
                fp["endpoint"] = std::string(buf);

                if (!sub.fingerPrint.inClusters.empty())
                {
                    JsonArray inClusters = fp.createNestedArray("in");

                    for (const auto clusterId : sub.fingerPrint.inClusters)
                    {
                        snprintf(buf, sizeof(buf), "0x%04X", clusterId);
                        inClusters.add(std::string(buf));
                    }
                }

                if (!sub.fingerPrint.outClusters.empty())
                {
                    JsonArray outClusters = fp.createNestedArray("out");

                    for (const auto clusterId : sub.fingerPrint.outClusters)
                    {
                        snprintf(buf, sizeof(buf), "0x%04X", clusterId);
                        outClusters.add(std::string(buf));
                    }
                }
            }

            JsonArray items = subDevice.createNestedArray("items");

            for (const DeviceDescription::Item &i : sub.items)
            {
                JsonObject item = items.createNestedObject();

                if (i.isImplicit && !ddfFull)
                {
                    item["name"] = i.name.c_str();
                    continue;
                }

                item["name"] = i.name.c_str();
                if (!i.isPublic) { item["public"] = false; }
                if (i.awake)     { item["awake"] = true; }

                if (!i.description.isEmpty())
                {
                    item["description"] = i.description.toStdString();
                }

                if (i.refreshInterval > 0)
                {
                    item["refresh.interval"] = i.refreshInterval;
                }

                if (!i.isStatic)
                {
                    if (!i.readParameters.isNull()  && (ddfFull || !i.isGenericRead))  { putItemParameter(item, "read", i.readParameters.toMap()); }
                    if (!i.writeParameters.isNull() && (ddfFull || !i.isGenericWrite)) { putItemParameter(item, "write", i.writeParameters.toMap()); }
                    if (!i.parseParameters.isNull() && (ddfFull || !i.isGenericParse)) { putItemParameter(item, "parse", i.parseParameters.toMap()); }
                }
                if (!i.defaultValue.isNull())
                {
                    if (i.isStatic)
                    {
                        putJsonQVariantValue(item, "static", i.defaultValue);
                    }
                    else
                    {
                        putJsonQVariantValue(item, "default", i.defaultValue);
                    }
                }
            }
        }
    }

    if (!ddf.bindings.empty())
    {
        JsonArray bindings = doc.createNestedArray("bindings");

        for (const DDF_Binding &bnd : ddf.bindings)
        {
            JsonObject binding = bindings.createNestedObject();

            if      (bnd.isUnicastBinding) { binding["bind"] = "unicast"; }
            else if (bnd.isGroupBinding)
            {
                binding["bind"] = "groupcast";
                binding["config.group"] = bnd.configGroup;
            }

            binding["src.ep"] = bnd.srcEndpoint;

            if (bnd.dstEndpoint > 0) { binding["dst.ep"] = bnd.dstEndpoint; }

            char buf[16];

            snprintf(buf, sizeof(buf), "0x%04X", bnd.clusterId);

            binding["cl"] = std::string(buf);

            if (!bnd.reporting.empty())
            {
                JsonArray reportings = binding.createNestedArray("report");

                for (const DDF_ZclReport &rep: bnd.reporting)
                {
                    JsonObject report = reportings.createNestedObject();

                    snprintf(buf, sizeof(buf), "0x%04X", rep.attributeId);
                    report["at"] = std::string(buf);

                    // TODO ZCLDB names
                    snprintf(buf, sizeof(buf), "0x%02X", rep.dataType);
                    report["dt"] = std::string(buf);

                    if (rep.manufacturerCode > 0)
                    {
                        snprintf(buf, sizeof(buf), "0x%04X", rep.manufacturerCode);
                        report["mf"] = std::string(buf);
                    }

                    report["min"] = rep.minInterval;
                    report["max"] = rep.maxInterval;

                    if (rep.reportableChange > 0)
                    {
                        snprintf(buf, sizeof(buf), "0x%08X", rep.reportableChange); // TODO proper length
                        report["change"] = std::string(buf);
                    }
                }
            }
        }
    }

    size_t sz = 0;

    if (prettyPrint)
    {
        sz = serializeJsonPretty(doc, buf, bufsize);
    }
    else
    {
        sz = serializeJson(doc, buf, bufsize);
    }
    U_ASSERT(sz < bufsize);

    DBG_Printf(DBG_INFO, "JSON serialized size %d\n", int(sz));

    return sz > 0 && sz < bufsize;
}

QString DDF_ToJsonPretty(const DeviceDescription &ddf)
{
    QString result;

    if (priv_ && ddfSerializeV1(priv_->json, ddf, priv_->jsonBuffer, sizeof(priv_->jsonBuffer), false, true))
    {
        result = priv_->jsonBuffer;
    }

    return result;
}

int RestDevices::getDeviceDDF(const ApiRequest &req, ApiResponse &rsp)
{
    const auto deviceKey = extAddressFromUniqueId(req.hdr.pathAt(3));

    bool ddfFull = req.hdr.pathAt(4) == QLatin1String("ddffull");

    Device *device = DEV_GetDevice(plugin->m_devices, deviceKey);

    rsp.httpStatus = device ? HttpStatusOk : HttpStatusNotFound;

    if (!device)
    {
        return REQ_READY_SEND;
    }

    DeviceDescription ddf = DeviceDescriptions::instance()->get(device);

    if (ddf.isValid())
    {
        if (ddf.bindings.empty())
        {
            ddf.bindings = device->bindings();
        }

        if (ddfSerializeV1(d->json, ddf, d->jsonBuffer, sizeof(d->jsonBuffer), ddfFull, false))
        {
            rsp.str = d->jsonBuffer;
        }
        else
        {
            // error
        }
    }
    else
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.str = QLatin1String("{}");
    }

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
    rsp.str = QLatin1String("{\"introspect\": false}");
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

    // TODO(mpi): this list is incomplete

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

    {  // 1) if the DDF provides buttons and button event descriptions take it from there
        const DeviceDescription &ddf = DeviceDescriptions::instance()->get(r);
        if (ddf.isValid())
        {
            for (const DeviceDescription::SubDevice &subd : ddf.subDevices)
            {
                if (!subd.buttonEvents.empty())
                {
                    QVariantMap buttons;
                    QVariantMap values;

                    for (unsigned btn: subd.buttonEvents)
                    {
                        {
                            QVariantMap m;
                            m[QLatin1String("button")] = int(btn / 1000);
                            m[QLatin1String("action")] = RIS_ButtonEventActionToString(btn);
                            values[QString::number(btn)] = m;
                        }

                        QString btnNum = QString::number(btn/1000);

                        if (!buttons.contains(btnNum))
                        {
                            QVariantMap m;
                            m[QLatin1String("name")] = QString("Button %1").arg(btn/1000);
                            buttons[btnNum] = m;
                        }

                    }

                    result[QLatin1String("buttons")] = buttons;
                    result[QLatin1String("values")] = values;

                    return result;
                }
            }
        }
    }

    // 2) try getting button and button event description from button maps

    const deCONZ::Node *node = getCoreNode(sensor->address().ext(), deCONZ::ApsController::instance());

    if (!node)
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

            const auto sd = std::find_if(node->simpleDescriptors().cbegin(), node->simpleDescriptors().cend(),
                                         [&btn](const deCONZ::SimpleDescriptor &x){ return x.endpoint() == btn.endpoint; });

            if (sd == node->simpleDescriptors().cend())
            {
                continue;
            }

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
            if (buttonBits & (1 << button.button))
            {
                QVariantMap m;
                AT_Atom nameAtom = AT_GetAtomByIndex({button.nameAtomeIndex});

                if (nameAtom.data)
                {
                    m[QLatin1String("name")] = QString::fromUtf8((const char*)nameAtom.data, nameAtom.len);
                    buttons[QString::number(button.button)] = m;
                }
            }
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
        rsp.list.append(errorToMap(ERR_INVALID_JSON, QString("/devices/%1/installcode").arg(uniqueid), QString("body contains invalid JSON")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    // installcode
    if (map.contains("installcode"))
    {
        std::string installCode = map["installcode"].toString().toStdString();

        if (map["installcode"].type() == QVariant::String && !installCode.empty())
        {
            char mmoHashHex[128] = {0};
            std::vector<unsigned char> mmoHash;

            if (!CRYPTO_GetMmoHashFromInstallCode(installCode, mmoHash))
            {
                rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QLatin1String("/devices"), QLatin1String("internal error, failed to calc mmo hash, occured")));
                rsp.httpStatus = HttpStatusServiceUnavailable;
                return REQ_READY_SEND;
            }

#if DECONZ_LIB_VERSION >= 0x010B00
            QVariantMap m;
            m["mac"] = uniqueid.toULongLong(&ok, 16);

            if (mmoHash.size() == 16)
            {
                DBG_HexToAscii(&mmoHash[0], mmoHash.size(), reinterpret_cast<unsigned char*>(&mmoHashHex[0]));
            }
            m["key"] = QString::fromLatin1(&mmoHashHex[0]);
            if (ok && strlen(mmoHashHex) == 32)
            {
                ok = deCONZ::ApsController::instance()->setParameter(deCONZ::ParamLinkKey, m);
            }
#endif
            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState["installcode"] = QString::fromLatin1(installCode.data());
            rspItemState["mmohash"] = QString::fromLatin1(&mmoHashHex[0]);
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);
            rsp.httpStatus = HttpStatusOk;
            return REQ_READY_SEND;
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/devices"), QString("invalid value, %1, for parameter, installcode").arg(installCode.data())));
            rsp.httpStatus = HttpStatusBadRequest;
        }
    }
    else
    {
        rsp.list.append(errorToMap(ERR_MISSING_PARAMETER, QString("/devices/%1/installcode").arg(uniqueid), QString("missing parameters in body")));
        rsp.httpStatus = HttpStatusBadRequest;
    }

    return REQ_READY_SEND;
}

int RestDevices::putDeviceReloadDDF(const ApiRequest &req, ApiResponse &rsp)
{
    DBG_Assert(req.path.size() == 6);

    rsp.httpStatus = HttpStatusOk;

    QLatin1String uniqueId = req.hdr.pathAt(3);
    DeviceKey deviceKey = getDeviceKey(uniqueId);

    if (deviceKey)
    {
        Device *device = DEV_GetDevice(plugin->m_devices, deviceKey);
        if (device)
        {
            DeviceDescriptions::instance()->reloadAllRawJsonAndBundles(device);
        }

        QVariantMap rspItem;
        QVariantMap rspItemState;
        rspItemState["reload"] = req.path.at(3);
        rspItem["success"] = rspItemState;
        rsp.list.append(rspItem);
        rsp.httpStatus = HttpStatusOk;
    }
    else
    {
        // TODO
    }

    return REQ_READY_SEND;
}

/*

    curl -X PUT -H "Content-Type: application/json" -d '{"policy": "nope", "hash":"value"}' 127.0.0.1:8090/api/12345/devices/00.99/ddf/policy


*/
int RestDevices::putDeviceSetDDFPolicy(const ApiRequest &req, ApiResponse &rsp)
{
    DBG_Assert(req.path.size() == 6);

    Device *device = nullptr;
    QLatin1String uniqueId = req.hdr.pathAt(3);
    DeviceKey deviceKey = getDeviceKey(uniqueId);

    const QByteArray content = req.content.toUtf8();
    const QString errAddr = QString("/devices/%1/ddf/policy").arg(uniqueId);

    U_SStream ss;

    cj_ctx cj;
    std::array<cj_token, 16> tokens;
    cj_token_ref refParent = 0;
    cj_token_ref refPolicy;

    char policyBuf[32];
    char bundleHashBuf[96];
    unsigned bundleHashLen = 0;
    unsigned policyLen = 0;

    if (deviceKey != 0)
    {
        device = DEV_GetDevice(plugin->m_devices, deviceKey);
    }

    if (!device)
    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE, errAddr, QString("resource, /devices/%1, not available").arg(uniqueId)));

        return REQ_READY_SEND;
    }

    cj_parse_init(&cj, content.data(), (cj_size)content.size(), tokens.data(), tokens.size());
    cj_parse(&cj);

    if (cj.status != CJ_OK)
    {

        rsp.list.append(errorToMap(ERR_INVALID_JSON, errAddr, "body contains invalid JSON"));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }


    if (cj_copy_value(&cj, policyBuf, sizeof(policyBuf), refParent, "policy") == 0)
    {
        rsp.list.append(errorToMap(ERR_MISSING_PARAMETER, errAddr, "missing parameters in body"));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    /*
     * Verify it's a valid policy value.
     */

    policyLen = U_strlen(policyBuf);
    const char *validValues[5] = { "latest_prefer_stable", "latest", "pin", "raw_json", nullptr };

    U_sstream_init(&ss, policyBuf, policyLen);

    int v = 0;
    for (; validValues[v]; v++)
    {
        unsigned len = U_strlen(validValues[v]);
        if (policyLen == len && U_sstream_starts_with(&ss, validValues[v]))
            break;
    }

    if (validValues[v] == nullptr)
    {
        rsp.list.append(errorToMap(ERR_INVALID_VALUE, errAddr, QString("invalid value, %1, for parameter, policy").arg(policyBuf)));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    /*
     * The 'pin' policy requires a 'hash' value to be specified.
     */

    if (U_sstream_starts_with(&ss, "pin"))
    {
        if (cj_copy_value(&cj, bundleHashBuf, sizeof(bundleHashBuf), refParent, "hash") == 0)
        {
            rsp.list.append(errorToMap(ERR_MISSING_PARAMETER, errAddr, "missing parameters in body"));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

        bundleHashLen = U_strlen(bundleHashBuf);
        if (!DDFB_SanitizeBundleHashString(bundleHashBuf, bundleHashLen))
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, errAddr, QString("invalid value, %1, for parameter, hash").arg(bundleHashBuf)));
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }

    bool needReload = false;
    ResourceItem *ddfPolicyItem = device->item(RAttrDdfPolicy);
    ResourceItem *ddfHashItem = device->item(RAttrDdfHash);
    U_ASSERT(ddfPolicyItem);
    U_ASSERT(ddfHashItem);

    if (!ddfPolicyItem->equalsString(policyBuf, policyLen))
    {
        ddfPolicyItem->setValue(policyBuf, policyLen, ResourceItem::SourceApi);
        needReload = true;

        DB_ResourceItem2 dbItem;
        dbItem.name = RAttrDdfPolicy;
        U_memcpy(dbItem.value, policyBuf, policyLen);
        dbItem.value[policyLen] = '\0';
        dbItem.valueSize = policyLen;
        dbItem.timestampMs = ddfPolicyItem->lastSet().toMSecsSinceEpoch();
        DB_StoreDeviceItem(device->deviceId(), dbItem);
    }

    if (bundleHashLen != 0 && !ddfHashItem->equalsString(bundleHashBuf, bundleHashLen))
    {
        ddfHashItem->setValue(bundleHashBuf, bundleHashLen, ResourceItem::SourceApi);
        needReload = true;

        DB_ResourceItem2 dbItem;
        dbItem.name = RAttrDdfHash;
        U_memcpy(dbItem.value, bundleHashBuf, bundleHashLen);
        dbItem.value[bundleHashLen] = '\0';
        dbItem.valueSize = bundleHashLen;
        dbItem.timestampMs = ddfHashItem->lastSet().toMSecsSinceEpoch();
        DB_StoreDeviceItem(device->deviceId(), dbItem);
    }

    rsp.httpStatus = HttpStatusOk;

    {
        QVariantMap result;
        QVariantMap item;

        item[QString("/devices/%1/ddf/policy").arg(uniqueId)] = QString::fromLatin1(policyBuf);
        result["success"] = item;
        rsp.list.append(result);
    }

    if (bundleHashLen != 0)
    {
        QVariantMap result;
        QVariantMap item;
        item[QString("/devices/%1/ddf/hash").arg(uniqueId)] = QString::fromLatin1(bundleHashBuf);
        result["success"] = item;
        rsp.list.append(result);
    }

    if (needReload)
    {
        emit eventNotify(Event(RDevices, REventDDFReload, 0, deviceKey));
    }

    return REQ_READY_SEND;
}
