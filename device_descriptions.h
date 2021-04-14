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
        enum Constants {
            NoRefreshInterval = -1
        };

        bool isValid() const { return !name.isEmpty() && descriptor.isValid(); }
        bool isPublic = true;
        bool awake = false;
        int refreshInterval = NoRefreshInterval;
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

    QString path;
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
    void handleEvent(const Event &event);
    void readAll();

Q_SIGNALS:
    void eventNotify(const Event&); //! Emitted \p Event needs to be enqueued in a higher layer.
    void loaded();

private:
    void handleDDFInitRequest(const Event &event);

    Q_DECLARE_PRIVATE_D(d_ptr2, DeviceDescriptions)
    DeviceDescriptionsPrivate *d_ptr2 = nullptr;
};

#endif // DEVICEDESCRIPTIONS_H
