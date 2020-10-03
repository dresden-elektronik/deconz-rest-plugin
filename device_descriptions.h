#ifndef DEVICE_DESCRIPTIONS_H
#define DEVICE_DESCRIPTIONS_H

#include <QObject>
#include <QVariantMap>
#include "resource.h"

class DeviceDescription
{
public:
    bool isValid() const { return !modelIds.empty() && !subDevices.empty(); }

    QStringList modelIds;
    QString manufacturer;
    QString product;

    class Item
    {
    public:
        bool isValid() const { return descriptor.isValid(); }
        QString name;
        ResourceItemDescriptor descriptor;
        std::vector<QVariant> parseParameters;
        std::vector<QVariant> readParameters;
        std::vector<QVariant> writeParameters;
    };

    class SubDevice
    {
    public:
        bool isValid() const { return !type.isEmpty() && !endpoint.isEmpty() && !uniqueId.isEmpty() && !items.empty(); }
        QString type;
        QString endpoint;
        QStringList uniqueId; // [ "$address.ext", "01", "0405"],
        std::vector<Item> items;
    };

    std::vector<SubDevice> subDevices;
};

class DeviceDescriptionsPrivate;

class DeviceDescriptions : public QObject
{
    Q_OBJECT

public:
    explicit DeviceDescriptions(QObject *parent = nullptr);
    ~DeviceDescriptions();

    static DeviceDescriptions *instance();

    DeviceDescription get(const Resource *resource);

public Q_SLOTS:
    void readAll();

Q_SIGNALS:
    void loaded();

private:
    Q_DECLARE_PRIVATE_D(d_ptr2, DeviceDescriptions)
    DeviceDescriptionsPrivate *d_ptr2 = nullptr;

};

#endif // DEVICEDESCRIPTIONS_H
