#ifndef DEVICE_DESCRIPTIONS_H
#define DEVICE_DESCRIPTIONS_H

#include <QObject>
#include <QVariantMap>

class DeviceDescription
{
public:
    QStringList modelIds;
    QString manufacturer;
    QString product;

    class SubDevice
    {
    public:
        QString type;
        QStringList uuid; // [ "$address.ext", "01", "0405"],
        std::vector<QVariantMap> items;
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

public Q_SLOTS:
    void readAll();

Q_SIGNALS:
    void loaded();

private:
    Q_DECLARE_PRIVATE_D(d_ptr2, DeviceDescriptions)
    DeviceDescriptionsPrivate *d_ptr2 = nullptr;

};

#endif // DEVICEDESCRIPTIONS_H
