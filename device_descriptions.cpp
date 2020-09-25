#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <deconz.h>
#include "device_descriptions.h"

class DeviceDescriptionsPrivate
{
public:
    std::map<QString,QString> manufacturers;
    std::vector<DeviceDescription> descriptions;
};

static bool readDeviceConstantsJson(const QString &path, std::map<QString,QString> *manufacturers);

DeviceDescriptions::DeviceDescriptions(QObject *parent) :
    QObject(parent),
    d_ptr2(new DeviceDescriptionsPrivate)
{
    readAll();
}

DeviceDescriptions::~DeviceDescriptions()
{
    Q_ASSERT(d_ptr2);
    delete d_ptr2;
    d_ptr2 = nullptr;
}

void DeviceDescriptions::readAll()
{
    Q_D(DeviceDescriptions);

    {
        std::map<QString,QString> manufacturers;
        if (readDeviceConstantsJson(deCONZ::getStorageLocation(deCONZ::ApplicationsDataLocation) + QLatin1String("/devices/generic/constants.json"), &d->manufacturers))
        {
            d->manufacturers = manufacturers;
        }
    }
}

static bool readDeviceConstantsJson(const QString &path, std::map<QString,QString> *manufacturers)
{
    Q_ASSERT(manufacturers);

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

    const auto mfobj = doc.object().value("manufacturers").toObject();
    for (auto &mf : mfobj.keys())
    {
        (*manufacturers)[mf] = mfobj.value(mf).toString();
    }

    return false;
}
