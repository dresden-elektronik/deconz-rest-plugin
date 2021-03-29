#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <deconz.h>
#include "device_descriptions.h"
#include "resource.h"

static DeviceDescriptions *_instance = nullptr;

class DeviceDescriptionsPrivate
{
public:
    std::map<QString,QString> constants;
    std::vector<DeviceDescription::Item> genericItems;
    std::vector<DeviceDescription> descriptions;
};

static bool DDF_ReadConstantsJson(const QString &path, std::map<QString,QString> *constants);
static DeviceDescription::Item DDF_ReadItemFile(const QString &path);
static std::vector<DeviceDescription> DDF_ReadDeviceFile(const QString &path);
static DeviceDescription DDF_MergeGenericItems(const std::vector<DeviceDescription::Item> &genericItems, const DeviceDescription &ddf);

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

/*! Get the DDF object for a \p resource.
    \returns The DDF object, DeviceDescription::isValid() to check for success.
 */
DeviceDescription DeviceDescriptions::get(const Resource *resource)
{
    Q_ASSERT(resource);
    Q_ASSERT(resource->item(RAttrModelId));

    Q_D(DeviceDescriptions);

    const auto modelId = resource->item(RAttrModelId)->toString();

    const auto i = std::find_if(d->descriptions.begin(), d->descriptions.end(), [&modelId](const DeviceDescription &ddf)
    {
        return ddf.modelIds.contains(modelId);
    });

    if (i != d->descriptions.end())
    {
        return *i;
    }

    return {};
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

/*! Reads all DDF related files.
 */
void DeviceDescriptions::readAll()
{
    Q_D(DeviceDescriptions);

    QDirIterator it(deCONZ::getStorageLocation(deCONZ::ApplicationsDataLocation) + QLatin1String("/devices"),
                    QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);

    std::vector<DeviceDescription> descriptions;
    std::vector<DeviceDescription::Item> genericItems;

    while (it.hasNext())
    {
        it.next();

        if (it.fileName() == QLatin1String("constants.json"))
        {
            std::map<QString,QString> constants;
            if (DDF_ReadConstantsJson(deCONZ::getStorageLocation(deCONZ::ApplicationsDataLocation) + QLatin1String("/devices/generic/constants.json"), &d->constants))
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
                const auto result = DDF_ReadItemFile(it.filePath());
                if (result.isValid())
                {
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
        d->descriptions = descriptions;

        for (auto &ddf : d->descriptions)
        {
            ddf = DDF_MergeGenericItems(d->genericItems, ddf);
        }
    }
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

    return false;
}

/*! Parses an item object.
    \returns A parsed item, use DeviceDescription::Item::isValid() to check for success.
 */
static DeviceDescription::Item DDF_ParseItem(const QJsonObject &obj)
{
    DeviceDescription::Item result;

    if (obj.contains(QLatin1String("name")))
    {
        result.name = obj.value(QLatin1String("name")).toString();
    }
    else if (obj.contains(QLatin1String("id"))) // generic/item TODO align name/id?
    {
        result.name = obj.value(QLatin1String("id")).toString();
    }

    if (result.name.isEmpty())
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

        const auto parse = obj.value(QLatin1String("parse"));
        if (parse.isArray())
        {
            const auto arr = parse.toArray();
            for (const auto &i : arr)
            {
                result.parseParameters.push_back(i.toVariant());
            }
        }

        const auto read = obj.value(QLatin1String("read"));
        if (read.isArray())
        {
            const auto arr = read.toArray();
            for (const auto &i : arr)
            {
                result.readParameters.push_back(i.toVariant());
            }
        }

        const auto write = obj.value(QLatin1String("write"));
        if (write.isArray())
        {
            const auto arr = write.toArray();
            for (const auto &i : arr)
            {
                result.writeParameters.push_back(i.toVariant());
            }
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
    \returns The sub device object, use DeviceDescription::SubDevice::isValid() to check for sucess.
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
static DeviceDescription DDF_ParseDeviceObject(const QJsonObject &obj)
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

    result.manufacturer = obj.value(QLatin1String("manufacturer")).toString();
    result.modelIds = DDF_ParseModelids(obj);
    result.product = obj.value(QLatin1String("product")).toString();

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
        const auto ddf = DDF_ParseDeviceObject(doc.object());
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
                const auto ddf = DDF_ParseDeviceObject(i.toObject());
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
                                              [&item](const DeviceDescription::Item &i){ return i.name == item.name; });
            if (genItem == genericItems.cend())
            {
                continue;
            }

            if (item.parseParameters.empty()) { item.parseParameters = genItem->parseParameters; }
            if (item.readParameters.empty()) { item.readParameters = genItem->readParameters; }
            if (item.writeParameters.empty()) { item.writeParameters = genItem->writeParameters; }
            if (item.descriptor.access == ResourceItemDescriptor::Access::Unknown)
            {
                item.descriptor.access = genItem->descriptor.access;
            }
        }
    }

    return result;
}
