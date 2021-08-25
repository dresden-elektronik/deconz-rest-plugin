/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <deconz/dbg_trace.h>
#include "device_ddf_init.h"
#include "device_descriptions.h"
#include "event.h"
#include "resource.h"

#define HND_MIN_LOAD_COUNTER 1
#define HND_MAX_LOAD_COUNTER 15
#define HND_MAX_DESCRIPTIONS 16383
#define HND_MAX_ITEMS        1023
#define HND_MAX_SUB_DEVS     15

/*! \union ItemHandlePack

    Packs location to an DDF item into a opaque 32-bit unsigned int handle.
    The DDF item lookup complexity is O(1) via DDF_GetItem() function.
 */
union ItemHandlePack
{
    // 32-bit memory layout
    // llll dddd dddd dddd ddss ssii iiii iiii
    struct {
        //! Max: 15, check for valid handle, for each DDF reload the counter is incremented (wraps to 0).
        unsigned int loadCounter : 4;
        unsigned int description : 14; //! Max: 16383, index into descriptions[].
        unsigned int subDevice: 4;     //! Max: 15, index into description -> subdevices[]
        unsigned int item : 10;        //! Max: 1023, index into subdevice -> items[]
    };
    quint32 handle;
};

static DeviceDescriptions *_instance = nullptr;

class DeviceDescriptionsPrivate
{
public:
    uint loadCounter = HND_MIN_LOAD_COUNTER;
    std::map<QString,QString> constants;
    std::vector<DeviceDescription::Item> genericItems;
    std::vector<DeviceDescription> descriptions;

    DeviceDescription invalidDescription;
    DeviceDescription::Item invalidItem;
};

static bool DDF_ReadConstantsJson(const QString &path, std::map<QString,QString> *constants);
static DeviceDescription::Item DDF_ReadItemFile(const QString &path);
static std::vector<DeviceDescription> DDF_ReadDeviceFile(const QString &path);
static DeviceDescription DDF_MergeGenericItems(const std::vector<DeviceDescription::Item> &genericItems, const DeviceDescription &ddf);
DeviceDescription DDF_LoadScripts(const DeviceDescription &ddf);

/*! Constructor. */
DeviceDescriptions::DeviceDescriptions(QObject *parent) :
    QObject(parent),
    d_ptr2(new DeviceDescriptionsPrivate)
{
    _instance = this;
}

/*! Destructor. */
DeviceDescriptions::~DeviceDescriptions()
{
    Q_ASSERT(_instance == this);
    _instance = nullptr;
    Q_ASSERT(d_ptr2);
    delete d_ptr2;
    d_ptr2 = nullptr;
}

/*! Returns the DeviceDescriptions singleton instance.
 */
DeviceDescriptions *DeviceDescriptions::instance()
{
    Q_ASSERT(_instance);
    return _instance;
}

void DeviceDescriptions::handleEvent(const Event &event)
{
    if (event.what() == REventDDFInitRequest)
    {
        handleDDFInitRequest(event);
    }
    else if (event.what() == REventDDFReload)
    {
        readAll(); // todo read only device specific files?
    }
}

/*! Get the DDF object for a \p resource.
    \returns The DDF object, DeviceDescription::isValid() to check for success.
 */
const DeviceDescription &DeviceDescriptions::get(const Resource *resource) const
{
    Q_ASSERT(resource);
    Q_ASSERT(resource->item(RAttrModelId));

    Q_D(const DeviceDescriptions);

    const auto modelId = resource->item(RAttrModelId)->toString();

    const auto i = std::find_if(d->descriptions.begin(), d->descriptions.end(), [&modelId](const DeviceDescription &ddf)
    {
        return ddf.modelIds.contains(modelId);
    });

    if (i != d->descriptions.end())
    {
        return *i;
    }

    return d->invalidDescription;
}

/*! Turns a string constant into it's value.
    \returns The constant value on success, or the constant itself on error.
 */
QString DeviceDescriptions::constantToString(const QString &constant) const
{
    Q_D(const DeviceDescriptions);

    const auto i = d->constants.find(constant);

    if (i != d->constants.end())
    {
        return i->second;
    }

    return constant;
}

/*! Retrieves the DDF item for the given \p item.

    If \p item has a valid DDF item handle the respective entry is returned.
    Otherwise the generic item list is searched based on the item.suffix.

    The returned entry can be check with DeviceDescription::Item::isValid().
 */
const DeviceDescription::Item &DDF_GetItem(const ResourceItem *item)
{
    Q_ASSERT(_instance);
    return _instance->getItem(item);
}

/*! \see DDF_GetItem() description.
 */
const DeviceDescription::Item &DeviceDescriptions::getItem(const ResourceItem *item) const
{
    Q_D(const DeviceDescriptions);

    ItemHandlePack h;
    h.handle = item->ddfItemHandle(); // unpack

    if (h.handle == DeviceDescription::Item::InvalidItemHandle)
    {
        return getGenericItem(item->descriptor().suffix);
    }

    if (h.loadCounter != d->loadCounter)
    {
        return d->invalidItem;
    }

    // Note: There are no further if conditions since at this point it's certain that a handle must be valid.

    Q_ASSERT(h.description < d->descriptions.size());

    const auto &ddf = d->descriptions[h.description];

    Q_ASSERT(h.subDevice < ddf.subDevices.size());

    const auto &sub = ddf.subDevices[h.subDevice];

    Q_ASSERT(h.item < sub.items.size());

    return sub.items[h.item];
}

const DeviceDescription::Item &DeviceDescriptions::getGenericItem(const char *suffix) const
{
    Q_D(const DeviceDescriptions);

    for (const auto &item : d->genericItems)
    {
        if (item.name == QLatin1String(suffix))
        {
            return item;
        }
    }

    return d->invalidItem;
}

/*! Updates all DDF item handles to point to correct location.
    \p loadCounter - the current load counter.
 */
std::vector<DeviceDescription> DDF_UpdateItemHandles(const std::vector<DeviceDescription> &descriptions, uint loadCounter)
{
    auto result = descriptions;
    Q_ASSERT(loadCounter >= HND_MIN_LOAD_COUNTER);
    Q_ASSERT(loadCounter <= HND_MAX_LOAD_COUNTER);

    ItemHandlePack handle;
    handle.description = 0;
    handle.loadCounter = loadCounter;

    for (auto &ddf : result)
    {
        handle.subDevice = 0;
        for (auto &sub : ddf.subDevices)
        {
            handle.item = 0;

            for (auto &item : sub.items)
            {
                item.handle = handle.handle;
                Q_ASSERT(handle.item < HND_MAX_ITEMS);
                handle.item++;
            }

            Q_ASSERT(handle.subDevice < HND_MAX_SUB_DEVS);
            handle.subDevice++;
        }

        Q_ASSERT(handle.description < HND_MAX_DESCRIPTIONS);
        handle.description++;
    }

    return result;
}

/*! Reads all DDF related files.
 */
void DeviceDescriptions::readAll()
{
    Q_D(DeviceDescriptions);

    d->loadCounter = (d->loadCounter + 1) % HND_MAX_LOAD_COUNTER;
    if (d->loadCounter <= HND_MIN_LOAD_COUNTER)
    {
        d->loadCounter = HND_MIN_LOAD_COUNTER;
    }

    DBG_MEASURE_START(DDF_ReadAllFiles);

    QDirIterator it(deCONZ::getStorageLocation(deCONZ::DdfLocation),
                    QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);

    std::vector<DeviceDescription> descriptions;
    std::vector<DeviceDescription::Item> genericItems;

    while (it.hasNext())
    {
        it.next();

        if (it.filePath().endsWith(QLatin1String("generic/constants.json")))
        {
            std::map<QString,QString> constants;
            if (DDF_ReadConstantsJson(it.filePath(), &constants))
            {
                d->constants = constants;
            }
        }
        else if (it.fileName() == QLatin1String("button_maps.json"))
        {  }
        else if (it.fileName().endsWith(QLatin1String(".json")))
        {
            if (it.filePath().contains(QLatin1String("generic/items/")))
            {
                auto result = DDF_ReadItemFile(it.filePath());
                if (result.isValid())
                {
                    result.isGenericRead = !result.readParameters.isNull() ? 1 : 0;
                    result.isGenericWrite = !result.writeParameters.isNull() ? 1 : 0;
                    result.isGenericParse = !result.parseParameters.isNull() ? 1 : 0;
                    genericItems.push_back(std::move(result));
                }
            }
            else
            {
                DBG_Printf(DBG_INFO, "CHK %s\n", qPrintable(it.fileName()));
                std::vector<DeviceDescription> result = DDF_ReadDeviceFile(it.filePath());
                std::move(result.begin(), result.end(), std::back_inserter(descriptions));
            }
        }
    }

    if (!genericItems.empty())
    {
        d->genericItems = genericItems;
    }

    if (!descriptions.empty())
    {
        d->descriptions = DDF_UpdateItemHandles(descriptions, d->loadCounter);

        for (auto &ddf : d->descriptions)
        {
            ddf = DDF_MergeGenericItems(d->genericItems, ddf);
            ddf = DDF_LoadScripts(ddf);
        }
    }

    DBG_MEASURE_END(DDF_ReadAllFiles);
}

/*! Tries to init a Device from an DDF file.

    Currently this is done syncronously later on it will be async to not block
    the main thread while loading DDF files.
 */
void DeviceDescriptions::handleDDFInitRequest(const Event &event)
{
    auto *resource = DEV_GetResource(RDevices, QString::number(event.deviceKey()));

    int result = -1; // error

    if (resource)
    {
        const auto ddf = get(resource);

        if (ddf.isValid())
        {
            result = 0;

            if (DEV_InitDeviceFromDescription(static_cast<Device*>(resource), ddf))
            {
                result = 1; // ok
            }
        }

        if (result >= 0)
        {
            DBG_Printf(DBG_INFO, "DEV found DDF for 0x%016llX, path: %s\n", event.deviceKey(), qPrintable(ddf.path));
        }

        if (result == 0)
        {
            DBG_Printf(DBG_INFO, "DEV init Device from DDF for 0x%016llX failed\n", event.deviceKey());
        }
        else if (result == -1)
        {
            DBG_Printf(DBG_INFO, "DEV no DDF for 0x%016llX, modelId: %s\n", event.deviceKey(), qPrintable(resource->item(RAttrModelId)->toString()));
        }
    }

    emit eventNotify(Event(RDevices, REventDDFInitResponse, result, event.deviceKey()));
}

/*! Reads constants.json file and places them into \p constants map.
 */
static bool DDF_ReadConstantsJson(const QString &path, std::map<QString,QString> *constants)
{
    Q_ASSERT(constants);

    QFile file(path);

    if (!file.exists())
    {
        return false;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return false;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();

    if (!doc.isObject())
    {
        DBG_Printf(DBG_INFO, "failed to read device constants: %s, err: %s, offset: %d\n", qPrintable(path), qPrintable(error.errorString()), error.offset);
        return false;
    }

    const auto obj = doc.object();
    const QStringList categories {"manufacturers", "device-types"};

    for (const auto &cat : categories)
    {
        if (obj.contains(cat))
        {
            const auto catobj = obj.value(cat).toObject();
            for (auto &key : catobj.keys())
            {
                (*constants)[key] = catobj.value(key).toString();
            }
        }
    }

    return !constants->empty();
}

/*! Parses an item object.
    \returns A parsed item, use DeviceDescription::Item::isValid() to check for success.
 */
static DeviceDescription::Item DDF_ParseItem(const QJsonObject &obj)
{
    DeviceDescription::Item result;

    if (obj.contains(QLatin1String("name")))
    {
        result.name = obj.value(QLatin1String("name")).toString().toUtf8().constData();
    }
    else if (obj.contains(QLatin1String("id"))) // generic/item TODO align name/id?
    {
        result.name = obj.value(QLatin1String("id")).toString().toUtf8().constData();
    }

    if (result.name.empty())
    {

    }
    else if (getResourceItemDescriptor(result.name, result.descriptor))
    {
        DBG_Printf(DBG_INFO, "DDF: loaded resource item descriptor: %s\n", result.descriptor.suffix);

        if (obj.contains(QLatin1String("access")))
        {
            const auto access = obj.value(QLatin1String("access")).toString();
            if (access == "R")
            {
                result.descriptor.access = ResourceItemDescriptor::Access::ReadOnly;
            }
            else if (access == "RW")
            {
                result.descriptor.access = ResourceItemDescriptor::Access::ReadWrite;
            }
        }

        if (obj.contains(QLatin1String("public")))
        {
            result.isPublic = obj.value(QLatin1String("public")).toBool() ? 1 : 0;
        }

        if (obj.contains(QLatin1String("implicit")))
        {
            result.isImplicit = obj.value(QLatin1String("implicit")).toBool() ? 1 : 0;
        }

        if (obj.contains(QLatin1String("awake")))
        {
            result.awake = obj.value(QLatin1String("awake")).toBool() ? 1 : 0;
        }

        if (obj.contains(QLatin1String("refresh.interval")))
        {
            result.refreshInterval = obj.value(QLatin1String("refresh.interval")).toInt(0);
        }

        const auto parse = obj.value(QLatin1String("parse"));
        if (parse.isObject())
        {
            result.parseParameters = parse.toVariant();
        }

        const auto read = obj.value(QLatin1String("read"));
        if (read.isObject())
        {
            result.readParameters = read.toVariant();
        }

        const auto write = obj.value(QLatin1String("write"));
        if (write.isObject())
        {
            result.writeParameters = write.toVariant();
        }

        if (obj.contains(QLatin1String("default")))
        {
            result.defaultValue = obj.value(QLatin1String("default")).toVariant();
        }
    }
    else
    {
        DBG_Printf(DBG_INFO, "DDF: failed to load resource item descriptor: %s\n", qPrintable(result.name));
    }

    return result;
}

/*! Parses a sub device in a DDF object "subdevices" array.
    \returns The sub device object, use DeviceDescription::SubDevice::isValid() to check for success.
 */
static DeviceDescription::SubDevice DDF_ParseSubDevice(const QJsonObject &obj)
{
    DeviceDescription::SubDevice result;

    result.type = obj.value(QLatin1String("type")).toString();
    if (result.type.isEmpty())
    {
        return result;
    }

    result.restApi = obj.value(QLatin1String("restapi")).toString();
    if (result.restApi.isEmpty())
    {
        return result;
    }

    const auto uniqueId = obj.value(QLatin1String("uuid"));
    if (uniqueId.isArray())
    {
        const auto arr = uniqueId.toArray();
        for (const auto &i : arr)
        {
            result.uniqueId.push_back(i.toString());
        }
    }

    const auto fingerPrint = obj.value(QLatin1String("fingerprint"));
    if (fingerPrint.isObject())
    {
        bool ok;
        const auto fp = fingerPrint.toObject();
        result.fingerPrint.endpoint = fp.value(QLatin1String("endpoint")).toString().toUInt(&ok, 0);
        result.fingerPrint.profileId = ok ? fp.value(QLatin1String("profile")).toString().toUInt(&ok, 0) : 0;
        result.fingerPrint.deviceId = ok ? fp.value(QLatin1String("device")).toString().toUInt(&ok, 0) : 0;

        if (fp.value(QLatin1String("in")).isArray())
        {
            const auto arr = fp.value(QLatin1String("in")).toArray();
            for (const auto &cl : arr)
            {
                const auto clusterId = ok ? cl.toString().toUInt(&ok, 0) : 0;
                if (ok)
                {
                    result.fingerPrint.inClusters.push_back(clusterId);
                }
            }
        }

        if (fp.value(QLatin1String("out")).isArray())
        {
            const auto arr = fp.value(QLatin1String("out")).toArray();
            for (const auto &cl : arr)
            {
                const auto clusterId = ok ? cl.toString().toUInt(&ok, 0) : 0;
                if (ok)
                {
                    result.fingerPrint.outClusters.push_back(clusterId);
                }
            }
        }

        if (!ok)
        {
            result.fingerPrint = { };
        }
    }

    const auto items = obj.value(QLatin1String("items"));
    if (!items.isArray())
    {
        return result;
    }

    {
        const auto arr = items.toArray();
        for (const auto &i : arr)
        {
            if (i.isObject())
            {
                const auto item = DDF_ParseItem(i.toObject());

                if (item.isValid())
                {
                    result.items.push_back(item);
                }
                else
                {

                }
            }
        }
    }

    return result;
}

// {"at": "0x0021", "dt": "u8", "min": 5, "max": 3600, "change": 1 },

/*! Parses a ZCL report in a DDF_Binding object "report" array.
    \returns The ZCL report, use DDF_ZclReport::isValid() to check for success.
 */
static DDF_ZclReport DDF_ParseZclReport(const QJsonObject &obj)
{
    DDF_ZclReport result{};

    // check required fields
    if (!obj.contains(QLatin1String("at")) ||
        !obj.contains(QLatin1String("dt")) ||
        !obj.contains(QLatin1String("min")) ||
        !obj.contains(QLatin1String("max")))
    {
        return {};
    }

    bool ok = false;
    result.attributeId = obj.value(QLatin1String("at")).toString().toUShort(&ok, 0);

    if (!ok)
    {
        return {};
    }

    {
        auto dataType = obj.value(QLatin1String("dt")).toString().toUShort(&ok, 0);
        if (!ok || dataType > 0xFF)
        {
            return {};
        }
        result.dataType = dataType;
    }

    {
        const auto minInterval = obj.value(QLatin1String("min")).toInt(-1);

        if (minInterval < 0 || minInterval > UINT16_MAX)
        {
            return {};
        }

        result.minInterval = minInterval;
    }

    {
        const auto maxInterval = obj.value(QLatin1String("max")).toInt(-1);

        if (maxInterval < 0 || maxInterval > UINT16_MAX)
        {
            return {};
        }

        result.maxInterval = maxInterval;
    }

    if (obj.contains(QLatin1String("change")))
    {
        result.reportableChange = obj.value(QLatin1String("change")).toString().toUInt(&ok, 0);

        if (!ok)
        {
            return {};
        }
    }

    result.valid = true;

    return result;
}

/*! Parses a binding in a DDF object "bindings" array.
    \returns The binding, use DDF_Binding::isValid() to check for success.
 */
static DDF_Binding DDF_ParseBinding(const QJsonObject &obj)
{
    DDF_Binding result{};

    // check required fields
    if (!obj.contains(QLatin1String("bind")) ||
        !obj.contains(QLatin1String("src.ep")) ||
        !obj.contains(QLatin1String("cl")))
    {
        return {};
    }

    const auto type = obj.value(QLatin1String("bind")).toString();

    if (type == QLatin1String("unicast"))
    {
        result.isUnicastBinding = 1;
    }
    else
    {
        // TODO group cast
        return {};
    }

    bool ok = false;
    {
        const auto srcEndpoint = obj.value(QLatin1String("src.ep")).toInt(-1);

        if (srcEndpoint < 0 || srcEndpoint > UINT8_MAX)
        {
            return {};
        }
        result.srcEndpoint = srcEndpoint;
    }

    {
        result.clusterId = obj.value(QLatin1String("cl")).toString().toUShort(&ok, 0);

        if (!ok)
        {
            return {};
        }
    }

    if (obj.contains(QLatin1String("dst.ep")))
    {
        const auto dstEndpoint = obj.value(QLatin1String("dst.ep")).toInt(-1);
        if (dstEndpoint < 0 || dstEndpoint >= 255)
        {
            return {};
        }
        result.dstEndpoint = dstEndpoint;
    }

    const auto report = obj.value(QLatin1String("report"));
    if (report.isArray())
    {
        const auto reportArr = report.toArray();
        for (const auto &i : reportArr)
        {
            if (i.isObject())
            {
                const auto rep = DDF_ParseZclReport(i.toObject());
                if (rep.isValid())
                {
                    result.reporting.push_back(rep);
                }
            }
        }
    }

    return result;
}

/*! Parses an model ids in DDF JSON object.
    The model id can be a string or array of strings.
    \returns List of parsed model ids.
 */
static QStringList DDF_ParseModelids(const QJsonObject &obj)
{
    QStringList result;
    const auto modelId = obj.value(QLatin1String("modelid"));

    if (modelId.isString()) // "modelid": "alpha.sensor"
    {
        result.push_back(modelId.toString());
    }
    else if (modelId.isArray()) // "modelid": [ "alpha.sensor", "beta.sensor" ]
    {
        const auto arr = modelId.toArray();
        for (const auto &i : arr)
        {
            if (i.isString())
            {
                result.push_back(i.toString());
            }
        }
    }

    return result;
}

/*! Parses an DDF JSON object.
    \returns DDF object, use DeviceDescription::isValid() to check for success.
 */
static DeviceDescription DDF_ParseDeviceObject(const QJsonObject &obj, const QString &path)
{
    DeviceDescription result;

    const auto schema = obj.value(QLatin1String("schema")).toString();

    if (schema != QLatin1String("devcap1.schema.json"))
    {
        return result;
    }

    const auto subDevices = obj.value(QLatin1String("subdevices"));
    if (!subDevices.isArray())
    {
        return result;
    }

    result.path = path;
    result.manufacturer = obj.value(QLatin1String("manufacturername")).toString();
    result.modelIds = DDF_ParseModelids(obj);
    result.product = obj.value(QLatin1String("product")).toString();

    if (obj.contains(QLatin1String("sleeper")))
    {
        result.sleeper = obj.value(QLatin1String("sleeper")).toBool() ? 1 : 0;
    }

    const auto keys = obj.keys();
    for (const auto &key : keys)
    {
        DBG_Printf(DBG_INFO, "DDF: %s: %s\n", qPrintable(key), qPrintable(obj.value(key).toString()));
    }

    const auto subDevicesArr = subDevices.toArray();
    for (const auto &i : subDevicesArr)
    {
        if (i.isObject())
        {
            const auto sub = DDF_ParseSubDevice(i.toObject());
            if (sub.isValid())
            {
                result.subDevices.push_back(sub);
            }
        }
    }

    const auto bindings = obj.value(QLatin1String("bindings"));
    if (bindings.isArray())
    {
        const auto bindingsArr = bindings.toArray();
        for (const auto &i : bindingsArr)
        {
            if (i.isObject())
            {
                const auto bnd = DDF_ParseBinding(i.toObject());
                if (bnd.isValid())
                {
                    result.bindings.push_back(bnd);
                }
            }
        }
    }


    return result;
}

/*! Reads an item file under (generic/items/).
    \returns A parsed item, use DeviceDescription::Item::isValid() to check for success.
 */
static DeviceDescription::Item DDF_ReadItemFile(const QString &path)
{
    QFile file(path);
    if (!file.exists())
    {
        return { };
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return { };
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();

    if (error.error != QJsonParseError::NoError)
    {
        DBG_Printf(DBG_INFO, "DDF: failed to read %s, err: %s, offset: %d\n", qPrintable(path), qPrintable(error.errorString()), error.offset);
        return { };
    }

    if (doc.isObject())
    {
        return DDF_ParseItem(doc.object());
    }

    return { };
}

QVariant DDF_ResolveParamScript(const QVariant &param, const QString &path)
{
    auto result = param;

    if (param.type() != QVariant::Map)
    {
        return result;
    }

    auto map = param.toMap();

    if (map.contains("script"))
    {
        const auto script = map["script"].toString();

        const QFileInfo fi(path);
        QFile f(fi.canonicalPath() + "/" + script);

        if (f.exists() && f.open(QFile::ReadOnly))
        {
            const auto content = f.readAll();
            if (!content.isEmpty())
            {
                map["eval"] = content;
                result = std::move(map);
            }
        }
    }

    return result;
}

DeviceDescription DDF_LoadScripts(const DeviceDescription &ddf)
{
    auto result = ddf;

    for (auto &sub : result.subDevices)
    {
        for (auto &item : sub.items)
        {
            item.parseParameters = DDF_ResolveParamScript(item.parseParameters, ddf.path);
            item.readParameters = DDF_ResolveParamScript(item.readParameters, ddf.path);
            item.writeParameters = DDF_ResolveParamScript(item.writeParameters, ddf.path);
        }
    }

    return result;
}

/*! Reads a DDF file which may contain one or more device descriptions.
    \returns Vector of parsed DDF objects.
 */
static std::vector<DeviceDescription> DDF_ReadDeviceFile(const QString &path)
{
    std::vector<DeviceDescription> result;

    QFile file(path);
    if (!file.exists())
    {
        return result;
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return result;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();

    if (error.error != QJsonParseError::NoError)
    {
        DBG_Printf(DBG_INFO, "DDF: failed to read %s, err: %s, offset: %d\n", qPrintable(path), qPrintable(error.errorString()), error.offset);
        return result;
    }

    if (doc.isObject())
    {
        const auto ddf = DDF_ParseDeviceObject(doc.object(), path);
        if (ddf.isValid())
        {
            result.push_back(ddf);
        }
    }
    else if (doc.isArray())
    {
        const auto arr = doc.array();
        for (const auto &i : arr)
        {
            if (i.isObject())
            {
                const auto ddf = DDF_ParseDeviceObject(i.toObject(), path);
                if (ddf.isValid())
                {
                    result.push_back(ddf);
                }
            }
        }
    }

    return result;
}

/*! Merge common properties like "read", "parse" and "write" functions from generic items into DDF items.
    Only properties which are already defined in the DDF file won't be overwritten.

    \param genericItems - generic items used as source
    \param ddf - DDF object with unmerged items
    \returns The merged DDF object.
 */
static DeviceDescription DDF_MergeGenericItems(const std::vector<DeviceDescription::Item> &genericItems, const DeviceDescription &ddf)
{
    auto result = ddf;

    for (auto &sub : result.subDevices)
    {
        for (auto &item : sub.items)
        {
            const auto genItem = std::find_if(genericItems.cbegin(), genericItems.cend(),
                                              [&item](const DeviceDescription::Item &i){ return i.descriptor.suffix == item.descriptor.suffix; });
            if (genItem == genericItems.cend())
            {
                continue;
            }

            item.isGenericRead = 0;
            item.isGenericWrite = 0;
            item.isGenericParse = 0;

            if (item.readParameters.isNull()) { item.readParameters = genItem->readParameters; item.isGenericRead = 1; }
            if (item.writeParameters.isNull()) { item.writeParameters = genItem->writeParameters; item.isGenericWrite = 1; }
            if (item.parseParameters.isNull()) { item.parseParameters = genItem->parseParameters; item.isGenericParse = 1; }
            if (item.descriptor.access == ResourceItemDescriptor::Access::Unknown)
            {
                item.descriptor.access = genItem->descriptor.access;
            }
            item.isPublic = genItem->isPublic;
            if (item.refreshInterval == DeviceDescription::Item::NoRefreshInterval && genItem->refreshInterval != item.refreshInterval)
            {
                item.refreshInterval = genItem->refreshInterval;
            }

            if (!item.defaultValue.isValid() && genItem->defaultValue.isValid())
            {
                item.defaultValue = genItem->defaultValue;
            }
        }
    }

    return result;
}
