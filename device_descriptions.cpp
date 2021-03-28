#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <deconz.h>
#include "device_descriptions.h"
#include "resource.h"

DeviceDescriptions *_instance = nullptr;

class DeviceDescriptionsPrivate
{
public:
    std::map<QString,QString> constants;
    std::vector<DeviceDescription> descriptions;
};

static bool readDeviceConstantsJson(const QString &path, std::map<QString,QString> *constants);
static std::vector<DeviceDescription> readDeviceDescriptionFile(const QString &path);

DeviceDescriptions::DeviceDescriptions(QObject *parent) :
    QObject(parent),
    d_ptr2(new DeviceDescriptionsPrivate)
{
    _instance = this;
}

DeviceDescriptions::~DeviceDescriptions()
{
    Q_ASSERT(_instance == this);
    _instance = nullptr;
    Q_ASSERT(d_ptr2);
    delete d_ptr2;
    d_ptr2 = nullptr;
}

DeviceDescriptions *DeviceDescriptions::instance()
{
    Q_ASSERT(_instance);
    return _instance;
}

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

void DeviceDescriptions::readAll()
{
    Q_D(DeviceDescriptions);

    QDirIterator it(deCONZ::getStorageLocation(deCONZ::ApplicationsDataLocation) + QLatin1String("/devices"),
                    QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);

    std::vector<DeviceDescription> descriptions;

    while (it.hasNext())
    {
        it.next();

        if (it.fileName() == QLatin1String("constants.json"))
        {
            std::map<QString,QString> constants;
            if (readDeviceConstantsJson(deCONZ::getStorageLocation(deCONZ::ApplicationsDataLocation) + QLatin1String("/devices/generic/constants.json"), &d->constants))
            {
                d->constants = constants;
            }
        }
        else if (it.fileName() == QLatin1String("button_maps.json"))
        {  }
        else if (it.fileName().endsWith(QLatin1String(".json")))
        {
            DBG_Printf(DBG_INFO, "CHK %s\n", qPrintable(it.fileName()));
            std::vector<DeviceDescription> result = readDeviceDescriptionFile(it.filePath());
            std::move(result.begin(), result.end(), std::back_inserter(descriptions));
        }
    }

    if (!descriptions.empty())
    {
        d->descriptions = descriptions;
    }
}

static bool readDeviceConstantsJson(const QString &path, std::map<QString,QString> *constants)
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

static DeviceDescription::Item parseDeviceDescriptionItem(const QJsonObject &obj)
{
    DeviceDescription::Item result;

    result.name = obj.value(QLatin1String("name")).toString();

    if (result.name.isEmpty())
    {

    }
    else if (getResourceItemDescriptor(result.name, result.descriptor))
    {
        DBG_Printf(DBG_INFO, "DDF: loaded resource item descriptor: %s\n", result.descriptor.suffix);

        const auto parse = obj.value(QLatin1String("parse"));
        if (parse.isArray())
        {
            for (const auto i : parse.toArray())
            {
                result.parseParameters.push_back(i.toVariant());
            }
        }

        const auto read = obj.value(QLatin1String("read"));
        if (read.isArray())
        {
            for (const auto i : read.toArray())
            {
                result.readParameters.push_back(i.toVariant());
            }
        }

        const auto write = obj.value(QLatin1String("write"));
        if (write.isArray())
        {
            for (const auto i : write.toArray())
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

static DeviceDescription::SubDevice parseDeviceDescriptionSubDevice(const QJsonObject &obj)
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
        for (const auto i : uniqueId.toArray())
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
            for (const auto &cl : fp.value(QLatin1String("in")).toArray())
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
            for (const auto &cl : fp.value(QLatin1String("out")).toArray())
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

    for (const auto &i : items.toArray())
    {
        if (i.isObject())
        {
            const auto item = parseDeviceDescriptionItem(i.toObject());

            if (item.isValid())
            {
                result.items.push_back(item);
            }
            else
            {

            }
        }
    }

    return result;
}

static QStringList parseDeviceDescriptionModelids(const QJsonObject &obj)
{
    QStringList result;
    const auto modelId = obj.value(QLatin1String("modelid"));

    if (modelId.isString()) // "modelid": "alpha.sensor"
    {
        result.push_back(modelId.toString());
    }
    else if (modelId.isArray()) // "modelid": [ "alpha.sensor", "beta.sensor" ]
    {
        for (const auto &i : modelId.toArray())
        {
            if (i.isString())
            {
                result.push_back(i.toString());
            }
        }
    }

    return result;
}

static DeviceDescription parseDeviceDescriptionObject(const QJsonObject &obj)
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
    result.modelIds = parseDeviceDescriptionModelids(obj);
    result.product = obj.value(QLatin1String("product")).toString();

    for (const auto &key : obj.keys())
    {
        DBG_Printf(DBG_INFO, "DDF: %s: %s\n", qPrintable(key), qPrintable(obj.value(key).toString()));
    }

    for (const auto &i : subDevices.toArray())
    {
        if (i.isObject())
        {
            const auto sub = parseDeviceDescriptionSubDevice(i.toObject());
            if (sub.isValid())
            {
                result.subDevices.push_back(sub);
            }
        }
    }

    return result;
}

static std::vector<DeviceDescription> readDeviceDescriptionFile(const QString &path)
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
        const auto ddf = parseDeviceDescriptionObject(doc.object());
        if (ddf.isValid())
        {
            result.push_back(ddf);
        }
    }
    else if (doc.isArray())
    {
        for (const auto &i : doc.array())
        {
            if (i.isObject())
            {
                const auto ddf = parseDeviceDescriptionObject(i.toObject());
                if (ddf.isValid())
                {
                    result.push_back(ddf);
                }
            }
        }
    }

    return result;
}
