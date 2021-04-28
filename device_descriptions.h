/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef DEVICE_DESCRIPTIONS_H
#define DEVICE_DESCRIPTIONS_H

#include <QObject>
#include <QVariantMap>
#include "resource.h"
#include "sensor.h"

class Event;

class DDF_ZclReport
{
public:
    bool isValid() const { return valid; }
    quint32 reportableChange;
    quint16 attributeId;
    quint16 minInterval;
    quint16 maxInterval;
    quint16 manufacturerCode;
    quint8 direction;
    quint8 dataType;
    bool valid = false;
};

class DDF_Binding
{
public:
    bool isValid() const { return (isUnicastBinding || isGroupBinding) && srcEndpoint != 0; }
    union
    {
        quint16 dstGroup;
        quint64 dstExtAddress;
    };

    quint16 clusterId;
    quint8 srcEndpoint;
    quint8 dstEndpoint;
    struct
    {
        unsigned int isGroupBinding : 1;
        unsigned int isUnicastBinding : 1;
        unsigned int pad : 6 + 24;
    };
    std::vector<DDF_ZclReport> reporting;
};

class DeviceDescription
{
public:
    bool isValid() const { return !modelIds.empty() && !subDevices.empty(); }

    QStringList modelIds;
    QString manufacturer;
    QString product;
    int sleeper = -1;

    class Item
    {
    public:
        using Handle = quint32;
        enum Constants {
            NoRefreshInterval = -1,
            InvalidItemHandle = 0
        };

        bool isValid() const { return !name.isEmpty() && descriptor.isValid(); }
        Handle handle = InvalidItemHandle;

        bool isPublic = true;
        bool isStatic = false;
        bool awake = false;
        int refreshInterval = NoRefreshInterval;
        QString name;
        ResourceItemDescriptor descriptor;
        QVariant parseParameters;
        QVariant readParameters;
        QVariant writeParameters;
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

    QString path;
    std::vector<SubDevice> subDevices;
    std::vector<DDF_Binding> bindings;
};

class DeviceDescriptionsPrivate;

class DeviceDescriptions : public QObject
{
    Q_OBJECT

public:
    explicit DeviceDescriptions(QObject *parent = nullptr);
    ~DeviceDescriptions();
    const DeviceDescription &get(const Resource *resource) const;
    QString constantToString(const QString &constant) const;

    const DeviceDescription::Item &getItem(const ResourceItem *item) const;

    static DeviceDescriptions *instance();

public Q_SLOTS:
    void handleEvent(const Event &event);
    void readAll();

Q_SIGNALS:
    void eventNotify(const Event&); //! Emitted \p Event needs to be enqueued in a higher layer.
    void loaded();

private:
    const DeviceDescription::Item &getGenericItem(const char *suffix) const;
    void handleDDFInitRequest(const Event &event);

    Q_DECLARE_PRIVATE_D(d_ptr2, DeviceDescriptions)
    DeviceDescriptionsPrivate *d_ptr2 = nullptr;
};

const DeviceDescription::Item &DDF_GetItem(const ResourceItem *item);

#endif // DEVICEDESCRIPTIONS_H
