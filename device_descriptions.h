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
    quint32 reportableChange = 0;
    quint16 attributeId = 0;
    quint16 minInterval = 0;
    quint16 maxInterval = 0;
    quint16 manufacturerCode = 0;
    quint8 direction = 0;
    quint8 dataType = 0;
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
    bool isValid() const { return !manufacturerNames.isEmpty() && !modelIds.empty() && !subDevices.empty(); }

    QStringList modelIds;
    QStringList manufacturerNames; // as reported in Basic cluster
    QString vendor; // optional: friendly name of manufacturer
    QString product;
    QString status;

    int handle = -1; // index in container
    int sleeper = -1;

    class Item
    {
    public:
        using Handle = quint32;
        enum Constants {
            NoRefreshInterval = -1,
            InvalidItemHandle = 0
        };

        Item()
        {
            isGenericRead = 0;
            isGenericWrite = 0;
            isGenericParse = 0;
            isPublic = 0;
            isStatic = 0;
            isImplicit = 0;
            isManaged = 0;
            awake = 0;
            pad = 0;
        }

        bool isValid() const { return !name.empty() && descriptor.isValid(); }
        Handle handle = InvalidItemHandle;

        struct // 16 bits flags
        {
            unsigned int isGenericRead : 1;
            unsigned int isGenericWrite : 1;
            unsigned int isGenericParse : 1;
            unsigned int isPublic : 1;
            unsigned int isStatic : 1;
            unsigned int isImplicit : 1;
            unsigned int isManaged : 1; // managed internally
            unsigned int awake : 1;
            unsigned int pad : 8;
        };

        int refreshInterval = NoRefreshInterval;
        BufString<64> name;  // todo global cache
        ResourceItemDescriptor descriptor;
        QVariant parseParameters;
        QVariant readParameters;
        QVariant writeParameters;
        QVariant defaultValue;
        QString description;
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

struct DDF_FunctionDescriptor
{
    struct Parameter
    {
        struct // 16 bits flags
        {
            unsigned int isOptional : 1;
            unsigned int supportsArray : 1;
            unsigned int isHexString : 1;
            unsigned int pad : 5;
        };

        QString name;
        QString key;
        QString description;
        ApiDataType dataType = DataTypeUnknown;
        qint64 defaultValue = 0;
    };

    QString name;
    QString description;
    std::vector<Parameter> parameters;
};

class DeviceDescriptionsPrivate;

using DDF_Items = std::vector<DeviceDescription::Item>;

class DeviceDescriptions : public QObject
{
    Q_OBJECT

public:
    explicit DeviceDescriptions(QObject *parent = nullptr);
    ~DeviceDescriptions();
    const DeviceDescription &get(const Resource *resource) const;
    void put(const DeviceDescription &ddf);

    QString constantToString(const QString &constant) const;
    QString stringToConstant(const QString &str) const;

    const DDF_Items &genericItems() const;
    const DeviceDescription::Item &getItem(const ResourceItem *item) const;
    const DeviceDescription::Item &getGenericItem(const char *suffix) const;
    std::vector<DDF_FunctionDescriptor> &getParseFunctions() const;
    std::vector<DDF_FunctionDescriptor> &getReadFunctions() const;

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

#define DDF_AnnoteZclParse(resource, item, ep, cl, at, eval) \
    DDF_AnnoteZclParse1(__LINE__, __FILE__, resource, item, ep, cl, at, eval)

void DDF_AnnoteZclParse1(int line, const char* file, const Resource *resource, const ResourceItem *item, quint8 ep, quint16 clusterId, quint16 attributeId, const char *eval);
const DeviceDescription::Item &DDF_GetItem(const ResourceItem *item);

#endif // DEVICEDESCRIPTIONS_H
