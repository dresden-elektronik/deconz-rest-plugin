#ifndef DEVICE_DESCRIPTIONS_H
#define DEVICE_DESCRIPTIONS_H

#include <QObject>
#include <QVariantMap>
#include "resource.h"
#include "sensor.h"

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
        bool isValid() const { return !name.isEmpty() && descriptor.isValid(); }
        QString name;
        ResourceItemDescriptor descriptor;
        std::vector<QVariant> parseParameters;
        std::vector<QVariant> readParameters;
        std::vector<QVariant> writeParameters;
        QVariant defaultValue;
    };

    class SubDevice
    {
    public:
        bool isValid() const { return !type.isEmpty() && !restApi.isEmpty() && !uniqueId.isEmpty() && !items.empty(); }
        QString type;
        QString restApi;
        QStringList uniqueId; // [ "$address.ext", "01", "0405"],
        std::vector<Item> items;
        SensorFingerprint fingerPrint;
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
    DeviceDescription get(const Resource *resource);
    QString constantToString(const QString &constant) const;

    static DeviceDescriptions *instance();

public Q_SLOTS:
    void readAll();

Q_SIGNALS:
    void loaded();

private:
    Q_DECLARE_PRIVATE_D(d_ptr2, DeviceDescriptions)
    DeviceDescriptionsPrivate *d_ptr2 = nullptr;
};

#endif // DEVICEDESCRIPTIONS_H
