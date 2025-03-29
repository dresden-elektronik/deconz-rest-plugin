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

#define SUBDEVICE_DEFAULT_ORDER  200

class Event;

class DDF_ZclReport
{
public:
    quint32 reportableChange = 0;
    quint16 attributeId = 0;
    quint16 minInterval = 0;
    quint16 maxInterval = 0;
    quint16 manufacturerCode = 0;
    quint8 direction = 0;
    quint8 dataType = 0;
    bool valid = false;
};

inline bool operator==(const DDF_ZclReport &a, const DDF_ZclReport &b)
{
    return memcmp(&a, &b, sizeof(a)) == 0;
}

inline bool operator!=(const DDF_ZclReport &a, const DDF_ZclReport &b) { return !(a == b); }

inline bool isValid(const DDF_ZclReport &rep) { return rep.valid; }

class DDF_Binding
{
public:
    union
    {
        quint16 dstGroup;
        quint64 dstExtAddress;
    };

    quint16 clusterId;
    quint8 srcEndpoint;
    quint8 dstEndpoint;
    quint8 configGroup;
    struct
    {
        unsigned int isGroupBinding : 1;
        unsigned int isUnicastBinding : 1;
        unsigned int pad : 6 + 24;
    };
    std::vector<DDF_ZclReport> reporting;
};

inline bool isValid(const DDF_Binding &bnd) { return (bnd.isUnicastBinding || bnd.isGroupBinding) && bnd.srcEndpoint != 0; }

inline bool operator==(const DDF_Binding &a, const DDF_Binding &b)
{
    if (a.clusterId == b.clusterId &&
        a.srcEndpoint == b.srcEndpoint &&
        a.reporting == b.reporting)
    {
        if (a.isUnicastBinding && b.isUnicastBinding &&
            a.dstExtAddress == b.dstExtAddress &&
            a.dstEndpoint == b.dstEndpoint)
        {
            return true;
        }
        if (a.isGroupBinding && b.isGroupBinding && a.dstGroup == b.dstGroup)
        {
            return true;
        }
    }

    return false;
}

inline bool operator!=(const DDF_Binding &a, const DDF_Binding &b)  { return !(a == b); }

class DeviceDescription
{
public:
    bool isValid() const { return !manufacturerNames.isEmpty() && !modelIds.empty() && !subDevices.empty(); }

    uint32_t sha256Hash[8] = {}; // either raw .json hash or bundle hash
    int64_t lastModified = 0;
    uint64_t signedBy = 0; // each bit is an index into d->publicKeys[] (max. 64)

    // TODO get rid of QStringLists use the atom lists instead
    QStringList modelIds;
    QStringList manufacturerNames; // as reported in Basic cluster

    std::vector<unsigned> modelidAtomIndices;
    std::vector<unsigned> mfnameAtomIndices;

    int storageLocation = -1; // deCONZ::StorageLocation

    QString path;
    QString vendor; // optional: friendly name of manufacturer
    QString product;
    QString status;
    QString matchExpr;

    int handle = -1; // index in container
    int sleeper = -1;
    int supportsMgmtBind = -1;

    class Item
    {
    public:
        using Handle = uint32_t;
        enum Constants {
            NoRefreshInterval = -1,
            InvalidItemHandle = 0
        };

        Item()
        {
            flags = 0;
        }

        bool isValid() const { return !name.empty() && descriptor.isValid(); }
        bool operator==(const Item &other) const
        {
            return flags == other.flags &&
                   handle == other.handle &&
                   refreshInterval == other.refreshInterval &&
                   name == other.name &&
                   descriptor.suffix == other.descriptor.suffix &&
                   parseParameters == other.parseParameters &&
                   readParameters == other.readParameters &&
                   writeParameters == other.writeParameters &&
                   defaultValue == other.defaultValue &&
                   description == other.description;
        }

        bool operator!=(const Item &other) const { return !(*this == other); }


        Handle handle = InvalidItemHandle;

        union
        {
            struct // 16 bits flags
            {
                unsigned short isGenericRead : 1;
                unsigned short isGenericWrite : 1;
                unsigned short isGenericParse : 1;
                unsigned short isPublic : 1;
                unsigned short isStatic : 1;
                unsigned short isImplicit : 1;
                unsigned short isManaged : 1; // managed internally
                unsigned short awake : 1;
                unsigned short hasIsPublic : 1; // to overwrite specific values
                unsigned short pad : 7;
            };

            unsigned short flags;
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
        QVariantMap meta;
        std::vector<Item> items;
        SensorFingerprint fingerPrint;
        std::vector<unsigned> buttonEvents;
    };

    std::vector<SubDevice> subDevices;
    std::vector<DDF_Binding> bindings;
};

class DDF_SubDeviceDescriptor
{
public:
    QString type;
    QString name;
    QString restApi;
    QStringList uniqueId;
    std::vector<const char*> items; // RAttrName, etc.
    int order;
};

inline bool isValid(const DDF_SubDeviceDescriptor &sub)
{
    return !sub.type.isEmpty() && !sub.name.isEmpty() && !sub.restApi.isEmpty() && !sub.uniqueId.isEmpty();
}

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
        QVariant defaultValue = 0;
    };

    QString name;
    QString description;
    std::vector<Parameter> parameters;
};

class DeviceDescriptionsPrivate;

using DDF_Items = std::vector<DeviceDescription::Item>;

enum DDF_MatchControl
{
    DDF_EvalMatchExpr,
    DDF_IgnoreMatchExpr
};

class DeviceDescriptions : public QObject
{
    Q_OBJECT

public:
    explicit DeviceDescriptions(QObject *parent = nullptr);
    ~DeviceDescriptions();
    void setEnabledStatusFilter(const QStringList &filter);
    const QStringList &enabledStatusFilter() const;
    const DeviceDescription &get(const Resource *resource, DDF_MatchControl match = DDF_EvalMatchExpr);
    const DeviceDescription &getFromHandle(DeviceDescription::Item::Handle hnd) const;
    void put(const DeviceDescription &ddf);
    const DeviceDescription &load(const QString &path);

    const DeviceDescription::SubDevice &getSubDevice(const Resource *resource) const;

    void prepare();

    QString constantToString(const QString &constant) const;
    QString stringToConstant(const QString &str) const;

    const DDF_Items &genericItems() const;
    const DeviceDescription::Item &getItem(const ResourceItem *item) const;
    const DeviceDescription::Item &getGenericItem(const char *suffix) const;
    const std::vector<DDF_FunctionDescriptor> &getParseFunctions() const;
    const std::vector<DDF_FunctionDescriptor> &getReadFunctions() const;
    const std::vector<DDF_FunctionDescriptor> &getWriteFunctions() const;
    const std::vector<DDF_SubDeviceDescriptor> &getSubDevices() const;

    static DeviceDescriptions *instance();

public Q_SLOTS:
    void handleEvent(const Event &event);
    void readAll();
    void readAllRawJson();
    void readAllBundles();

    void ddfReloadTimerFired();
    void reloadAllRawJsonAndBundles(const Resource *resource);

Q_SIGNALS:
    void eventNotify(const Event&); //! Emitted \p Event needs to be enqueued in a higher layer.
    void loaded();

private:
    void handleDDFInitRequest(const Event &event);
    bool loadDDFAndBundlesFromDisc(const Resource *resource);

    Q_DECLARE_PRIVATE_D(d_ptr2, DeviceDescriptions)
    DeviceDescriptionsPrivate *d_ptr2 = nullptr;
};

#define DDF_AnnoteZclParse(resource, item, ep, cl, at, eval) \
    DDF_AnnoteZclParse1(__LINE__, __FILE__, resource, item, ep, cl, at, eval)

bool DDF_IsStatusEnabled(const QString &status);
void DDF_AnnoteZclParse1(int line, const char* file, const Resource *resource, ResourceItem *item, quint8 ep, quint16 clusterId, quint16 attributeId, const char *eval);
const DeviceDescription::Item &DDF_GetItem(const ResourceItem *item);
Resource::Handle R_CreateResourceHandle(const Resource *r, size_t containerIndex);

void DEV_ReloadDeviceIdendifier(unsigned atomIndexMfname, unsigned atomIndexModelid);

#endif // DEVICEDESCRIPTIONS_H
