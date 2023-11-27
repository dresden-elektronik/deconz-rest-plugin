/*
 * Copyright (c) 2022 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSettings>
#include <deconz/dbg_trace.h>
#include "device_ddf_init.h"
#include "device_descriptions.h"
#include "device_js/device_js.h"
#include "event.h"
#include "resource.h"

#define HND_MIN_LOAD_COUNTER 1
#define HND_MAX_LOAD_COUNTER 15
#define HND_MAX_DESCRIPTIONS 16383
#define HND_MAX_ITEMS        1023
#define HND_MAX_SUB_DEVS     15

/*! \union ItemHandlePack

    Packs location to an DDF item into a opaque 32-bit unsigned int handle.
    The DDF item lookup complexity is O(1) via DDF_GetItem() function.
 */
union ItemHandlePack
{
    // 32-bit memory layout
    // llll dddd dddd dddd ddss ssii iiii iiii
    struct {
        //! Max: 15, check for valid handle, for each DDF reload the counter is incremented (wraps to 0).
        unsigned int loadCounter : 4;
        unsigned int description : 14; //! Max: 16383, index into descriptions[].
        unsigned int subDevice: 4;     //! Max: 15, index into description -> subdevices[]
        unsigned int item : 10;        //! Max: 1023, index into subdevice -> items[]
    };
    quint32 handle;
};

static DeviceDescriptions *_instance = nullptr;
static DeviceDescriptionsPrivate *_priv = nullptr;

class DeviceDescriptionsPrivate
{
public:
    uint loadCounter = HND_MIN_LOAD_COUNTER;
    std::map<QString,QString> constants;
    std::vector<DeviceDescription::Item> genericItems;
    std::vector<DeviceDescription> descriptions;

    DeviceDescription invalidDescription;
    DeviceDescription::Item invalidItem;
    DeviceDescription::SubDevice invalidSubDevice;

    QStringList enabledStatusFilter;

    std::vector<DDF_SubDeviceDescriptor> subDevices;

    std::vector<DDF_FunctionDescriptor> readFunctions;
    std::vector<DDF_FunctionDescriptor> writeFunctions;
    std::vector<DDF_FunctionDescriptor> parseFunctions;
};

static bool DDF_ReadConstantsJson(const QString &path, std::map<QString,QString> *constants);
static DeviceDescription::Item DDF_ReadItemFile(const QString &path);
static std::vector<DeviceDescription> DDF_ReadDeviceFile(const QString &path);
static DDF_SubDeviceDescriptor DDF_ReadSubDeviceFile(const QString &path);
static DeviceDescription DDF_MergeGenericItems(const std::vector<DeviceDescription::Item> &genericItems, const DeviceDescription &ddf);
static DeviceDescription::Item *DDF_GetItemMutable(const ResourceItem *item);
static void DDF_UpdateItemHandles(std::vector<DeviceDescription> &descriptions, uint loadCounter);
static void DDF_TryCompileAndFixJavascript(QString *expr, const QString &path);
DeviceDescription DDF_LoadScripts(const DeviceDescription &ddf);

/*! Constructor. */
DeviceDescriptions::DeviceDescriptions(QObject *parent) :
    QObject(parent),
    d_ptr2(new DeviceDescriptionsPrivate)
{
    _instance = this;
    _priv = d_ptr2;

    {  // Parse function as shown in the DDF editor.
        DDF_FunctionDescriptor fn;
        fn.name = "zcl";
        fn.description = "Generic function to parse ZCL attributes and commands.";

        DDF_FunctionDescriptor::Parameter param;

        param.name = "Endpoint";
        param.key = "ep";
        param.description = "255 means any endpoint, 0 means auto selected from subdevice.";
        param.dataType = DataTypeUInt8;
        param.defaultValue = 0;
        param.isOptional = 1;
        param.isHexString = 0;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Cluster ID";
        param.key = "cl";
        param.description = "As string hex value";
        param.dataType = DataTypeUInt16;
        param.defaultValue = 0;
        param.isOptional = 0;
        param.isHexString = 1;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Attribute ID";
        param.key = "at";
        param.description = "As string hex value";
        param.dataType = DataTypeUInt16;
        param.defaultValue = 0;
        param.isOptional = 0;
        param.isHexString = 1;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Manufacturer code";
        param.key = "mf";
        param.description = "As string hex value.";
        param.dataType = DataTypeUInt16;
        param.defaultValue = 0;
        param.isOptional = 1;
        param.isHexString = 1;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Javascript file";
        param.key = "script";
        param.description = "Relative path of a Javascript .js file.";
        param.dataType = DataTypeString;
        param.defaultValue = {};
        param.isOptional = 1;
        param.isHexString = 0;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Expression";
        param.key = "eval";
        param.description = "Javascript expression to transform the raw value.";
        param.dataType = DataTypeString;
        param.defaultValue = QLatin1String("Item.val = Attr.val");
        param.isOptional = 1;
        param.isHexString = 0;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        d_ptr2->parseFunctions.push_back(fn);
    }

    {  // Read function as shown in the DDF editor.
        DDF_FunctionDescriptor fn;
        fn.name = "zcl";
        fn.description = "Generic function to read ZCL attributes.";

        DDF_FunctionDescriptor::Parameter param;

        param.name = "Endpoint";
        param.key = "ep";
        param.description = "255 means any endpoint, 0 means auto selected from subdevice.";
        param.dataType = DataTypeUInt8;
        param.defaultValue = 255;
        param.isOptional = 0;
        param.isHexString = 0;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Cluster ID";
        param.key = "cl";
        param.description = "As string hex value";
        param.dataType = DataTypeUInt16;
        param.defaultValue = 0;
        param.isOptional = 0;
        param.isHexString = 1;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Attribute ID";
        param.key = "at";
        param.description = "As string hex value";
        param.dataType = DataTypeUInt16;
        param.defaultValue = 0;
        param.isOptional = 0;
        param.isHexString = 1;
        param.supportsArray = 1;
        fn.parameters.push_back(param);

        param.name = "Manufacturer code";
        param.key = "mf";
        param.description = "As string hex value.";
        param.dataType = DataTypeUInt16;
        param.defaultValue = 0;
        param.isOptional = 1;
        param.isHexString = 1;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        d_ptr2->readFunctions.push_back(fn);
    }

    {
        DDF_FunctionDescriptor fn;
        fn.name = "ias:zonestatus";
        fn.description = "Generic function to parse IAS ZONE status change notifications or zone status from read/report command.";

        DDF_FunctionDescriptor::Parameter param;

        param.name = "IAS Zone status mask";
        param.key = "mask";
        param.description = "Sets the bitmask for Alert1 and Alert2 item of the IAS Zone status.";
        param.dataType = DataTypeString;
        param.defaultValue = QLatin1String("alarm1,alarm2");
        param.isOptional = 1;
        param.isHexString = 0;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        d_ptr2->parseFunctions.push_back(fn);
    }

    {
        DDF_FunctionDescriptor fn;
        fn.name = "numtostr";
        fn.description = "Generic function to to convert number to string.";

        DDF_FunctionDescriptor::Parameter param;

        param.name = "Source item";
        param.key = "srcitem";
        param.description = "The source item holding the number.";
        param.dataType = DataTypeString;
        param.defaultValue = 0;
        param.isOptional = 0;
        param.isHexString = 0;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Operator";
        param.key = "op";
        param.description = "Comparison operator (lt | le | eq | gt | ge)";
        param.dataType = DataTypeString;
        param.defaultValue = 0;
        param.isOptional = 0;
        param.isHexString = 0;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Mapping";
        param.key = "to";
        param.description = "Array of (num, string) mappings";
        param.dataType = DataTypeString;
        param.defaultValue = 0;
        param.isOptional = 0;
        param.isHexString = 0;
        param.supportsArray = 1;
        fn.parameters.push_back(param);

        d_ptr2->parseFunctions.push_back(fn);
    }

    {
        DDF_FunctionDescriptor fn;
        fn.name = "time";
        fn.description = "Specialized function to parse time, local and last set time from read/report commands of the time cluster and auto-sync time if needed.";

        d_ptr2->parseFunctions.push_back(fn);
    }

    {
        DDF_FunctionDescriptor fn;
        fn.name = "xiaomi:special";
        fn.description = "Generic function to parse custom Xiaomi attributes and commands.";

        DDF_FunctionDescriptor::Parameter param;

        param.name = "Endpoint";
        param.key = "ep";
        param.description = "Source endpoint of the incoming command, default value 255 means any endpoint.";
        param.dataType = DataTypeUInt8;
        param.defaultValue = 255;
        param.isOptional = 1;
        param.isHexString = 0;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Attribute ID";
        param.key = "at";
        param.description = "The attribute to parse, shall be 0xff01, 0xff02 or 0x00f7";
        param.dataType = DataTypeUInt16;
        param.defaultValue = 0;
        param.isOptional = 0;
        param.isHexString = 1;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Index";
        param.key = "idx";
        param.description = "A 8-bit string hex value.";
        param.dataType = DataTypeUInt8;
        param.defaultValue = 0;
        param.isOptional = 0;
        param.isHexString = 1;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Expression";
        param.key = "eval";
        param.description = "Javascript expression to transform the raw value.";
        param.dataType = DataTypeString;
        param.defaultValue = {};
        param.isOptional = 0;
        param.isHexString = 0;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        d_ptr2->parseFunctions.push_back(fn);
    }

    {
        DDF_FunctionDescriptor fn;
        fn.name = "tuya";
        fn.description = "Generic function to read all Tuya datapoints. It has no parameters.";
        d_ptr2->readFunctions.push_back(fn);
    }

    {
        DDF_FunctionDescriptor fn;
        fn.name = "tuya";
        fn.description = "Generic function to parse Tuya data.";

        DDF_FunctionDescriptor::Parameter param;

        param.name = "Datapoint";
        param.key = "dpid";
        param.description = "1-255 the datapoint ID.";
        param.dataType = DataTypeUInt8;
        param.defaultValue = 0;
        param.isOptional = 0;
        param.isHexString = 0;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Javascript file";
        param.key = "script";
        param.description = "Relative path of a Javascript .js file.";
        param.dataType = DataTypeString;
        param.defaultValue = {};
        param.isOptional = 1;
        param.isHexString = 0;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Expression";
        param.key = "eval";
        param.description = "Javascript expression to transform the raw value.";
        param.dataType = DataTypeString;
        param.defaultValue = QLatin1String("Item.val = Attr.val");
        param.isOptional = 1;
        param.isHexString = 0;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        d_ptr2->parseFunctions.push_back(fn);
    }
}

/*! Destructor. */
DeviceDescriptions::~DeviceDescriptions()
{
    Q_ASSERT(_instance == this);
    _instance = nullptr;
    _priv = nullptr;
    Q_ASSERT(d_ptr2);
    delete d_ptr2;
    d_ptr2 = nullptr;
}

void DeviceDescriptions::setEnabledStatusFilter(const QStringList &filter)
{
    if (d_ptr2->enabledStatusFilter != filter)
    {
        d_ptr2->enabledStatusFilter = filter;
        DBG_Printf(DBG_INFO, "DDF enabled for %s status\n", qPrintable(filter.join(QLatin1String(", "))));
    }
}

const QStringList &DeviceDescriptions::enabledStatusFilter() const
{
    return d_ptr2->enabledStatusFilter;
}

/*! Returns the DeviceDescriptions singleton instance.
 */
DeviceDescriptions *DeviceDescriptions::instance()
{
    Q_ASSERT(_instance);
    return _instance;
}

bool DDF_IsStatusEnabled(const QString &status)
{
    if (_priv)
    {
        return _priv->enabledStatusFilter.contains(status, Qt::CaseInsensitive);
    }
    return false;
}

/*! Helper to transform hard C++ coded parse functions to DDF.
 */
void DDF_AnnoteZclParse1(int line, const char *file, const Resource *resource, ResourceItem *item, quint8 ep, quint16 clusterId, quint16 attributeId, const char *eval)
{
    DBG_Assert(resource);
    DBG_Assert(item);
    DBG_Assert(eval);

    if (!_instance || !resource || !item || !eval)
    {
        return;
    }

    if (item->ddfItemHandle() == DeviceDescription::Item::InvalidItemHandle)
    {
        const Device *device = nullptr;
        if (resource->parentResource())
        {
            device = static_cast<const Device*>(resource->parentResource());
        }

        if (!device)
        {
            return;
        }

        const auto *uniqueId = resource->item(RAttrUniqueId);
        if (!uniqueId)
        {
            return;
        }

        auto &ddf = _instance->get(device);
        if (!ddf.isValid())
        {
            return;
        }

        // this is pretty heavy but will be removed later
        const QStringList u = uniqueId->toString().split(QLatin1Char('-'), SKIP_EMPTY_PARTS);

        for (const auto &sub : ddf.subDevices)
        {
            if (u.size() != sub.uniqueId.size())
            {
                continue;
            }

            bool ok = true;
            for (int i = 1; i < qMin(u.size(), sub.uniqueId.size()); i++)
            {
                if (u[i].toUInt(0, 16) != sub.uniqueId[i].toUInt(0, 16))
                {
                    ok = false;
                }
            }

            if (!ok)
            {
                continue;
            }

            for (const auto &ddfItem : sub.items)
            {
                if (ddfItem.name == item->descriptor().suffix)
                {
                    item->setDdfItemHandle(ddfItem.handle);
                    break;
                }
            }

            break;
        }
    }

    if (item->ddfItemHandle() != DeviceDescription::Item::InvalidItemHandle)
    {
        DeviceDescription::Item *ddfItem = DDF_GetItemMutable(item);

        if (ddfItem && ddfItem->isValid())
        {
            if (ddfItem->parseParameters.isNull())
            {
                char buf[255];

                QVariantMap param;
                param[QLatin1String("ep")] = int(ep);
                snprintf(buf, sizeof(buf), "0x%04X", clusterId);
                param[QLatin1String("cl")] = QLatin1String(buf);
                snprintf(buf, sizeof(buf), "0x%04X", attributeId);
                param[QLatin1String("at")] = QLatin1String(buf);
                param[QLatin1String("eval")] = QLatin1String(eval);

                size_t fileLen = strlen(file);
                const char *fileName = file + fileLen;

                for (size_t i = fileLen; i > 0; i--, fileName--)
                {
                    if (*fileName == '/')
                    {
                        fileName++;
                        break;
                    }
                }

                snprintf(buf, sizeof(buf), "%s:%d", fileName, line);
                param[QLatin1String("cppsrc")] = QLatin1String(buf);

                ddfItem->parseParameters = param;

                DBG_Printf(DBG_DDF, "DDF %s:%d: %s updated ZCL function cl: 0x%04X, at: 0x%04X, eval: %s\n", fileName, line, qPrintable(resource->item(RAttrUniqueId)->toString()), clusterId, attributeId, eval);
            }
        }
    }
}

void DeviceDescriptions::handleEvent(const Event &event)
{
    if (event.what() == REventDDFInitRequest)
    {
        handleDDFInitRequest(event);
    }
    else if (event.what() == REventDDFReload)
    {
        readAll(); // todo read only device specific files?
    }
}

/*! Get the DDF object for a \p resource.
    \returns The DDF object, DeviceDescription::isValid() to check for success.
 */
const DeviceDescription &DeviceDescriptions::get(const Resource *resource, DDF_MatchControl match) const
{
    Q_ASSERT(resource);
    Q_ASSERT(resource->item(RAttrModelId));
    Q_ASSERT(resource->item(RAttrManufacturerName));

    Q_D(const DeviceDescriptions);

    const auto modelId = resource->item(RAttrModelId)->toString();
    const auto manufacturer = resource->item(RAttrManufacturerName)->toString();
    const auto manufacturerConstant = stringToConstant(manufacturer);

    auto i = d->descriptions.begin();

    for (;;)
    {
        i = std::find_if(i, d->descriptions.end(), [&modelId, &manufacturer, &manufacturerConstant](const DeviceDescription &ddf)
        {
            // compare manufacturer name case insensitive
            const auto m = std::find_if(ddf.manufacturerNames.cbegin(), ddf.manufacturerNames.cend(),
                                       [&](const auto &x){ return x.compare(manufacturer, Qt::CaseInsensitive) == 0; });

            return (ddf.modelIds.contains(modelId) && (m != ddf.manufacturerNames.cend() || ddf.manufacturerNames.contains(manufacturerConstant)));
        });

        if (i == d->descriptions.end())
        {
            break;
        }

        if (!i->matchExpr.isEmpty() && match == DDF_EvalMatchExpr)
        {
            DeviceJs *djs = DeviceJs::instance();
            djs->reset();
            djs->setResource(resource->parentResource() ? resource->parentResource() : resource);
            if (djs->evaluate(i->matchExpr) == JsEvalResult::Ok)
            {
                const auto res = djs->result();
                DBG_Printf(DBG_DDF, "matchexpr: %s --> %s\n", qPrintable(i->matchExpr), qPrintable(res.toString()));
                if (res.toBool()) // needs to evaluate to true
                {
                    return *i;
                }
            }
            else
            {
                DBG_Printf(DBG_DDF, "failed to evaluate matchexpr for %s: %s, err: %s\n", qPrintable(resource->item(RAttrUniqueId)->toString()), qPrintable(i->matchExpr), qPrintable(djs->errorString()));
            }
            i++; // proceed search
        }
        else
        {
            return *i;
        }
    }

    return d->invalidDescription;
}

void DeviceDescriptions::put(const DeviceDescription &ddf)
{
    if (!ddf.isValid())
    {
        return;
    }

    Q_D(DeviceDescriptions);

    if (ddf.handle >= 0 && ddf.handle <= int(d->descriptions.size()))
    {
        DeviceDescription &ddf0 = d->descriptions[ddf.handle];

        DBG_Assert(ddf0.handle == ddf.handle);
        if (ddf.handle == ddf0.handle)
        {
            DBG_Printf(DBG_DDF, "update ddf %s index %d\n", qPrintable(ddf0.modelIds.front()), ddf.handle);
            ddf0 = ddf;
            DDF_UpdateItemHandles(d->descriptions, d->loadCounter);
            return;
        }
    }
}

const DeviceDescription &DeviceDescriptions::load(const QString &path)
{
    Q_D(DeviceDescriptions);

    auto i = std::find_if(d->descriptions.begin(), d->descriptions.end(), [&path](const auto &ddf){ return ddf.path == path; });
    if (i != d->descriptions.end())
    {
        return *i;
    }

    auto result = DDF_ReadDeviceFile(path);

    if (!result.empty())
    {
        for (auto &ddf : result)
        {
            ddf = DDF_MergeGenericItems(d->genericItems, ddf);
            ddf = DDF_LoadScripts(ddf);

            i = std::find_if(d->descriptions.begin(), d->descriptions.end(), [&ddf](const DeviceDescription &b)
            {
                return ddf.modelIds == b.modelIds && ddf.manufacturerNames == b.manufacturerNames;
            });

            if (i != d->descriptions.end())
            {
                *i = ddf; // update
            }
            else
            {
                d->descriptions.push_back(ddf);
            }
        }

        DDF_UpdateItemHandles(d->descriptions, d->loadCounter);

        i = std::find_if(d->descriptions.begin(), d->descriptions.end(), [&path](const auto &ddf){ return ddf.path == path; });
        if (i != d->descriptions.end())
        {
            return *i;
        }
    }

    return d->invalidDescription;
}

/*! Returns the DDF sub device belonging to a resource. */
const DeviceDescription::SubDevice &DeviceDescriptions::getSubDevice(const Resource *resource) const
{
    Q_D(const DeviceDescriptions);

    if (resource)
    {
        ItemHandlePack h;
        for (int i = 0; i < resource->itemCount(); i++)
        {
            const ResourceItem *item = resource->itemForIndex(size_t(i));
            assert(item);

            h.handle = item->ddfItemHandle();
            if (h.handle == DeviceDescription::Item::InvalidItemHandle)
            {
                continue;
            }

            if (h.loadCounter != d->loadCounter)
            {
                return d->invalidSubDevice;
            }

            DBG_Assert(h.description < d->descriptions.size());
            if (h.description >= d->descriptions.size())
            {
                return d->invalidSubDevice;
            }

            auto &ddf = d->descriptions[h.description];

            DBG_Assert(h.subDevice < ddf.subDevices.size());
            if (h.subDevice >= ddf.subDevices.size())
            {
                return d->invalidSubDevice;
            }

            return ddf.subDevices[h.subDevice];
        }
    }

    return d->invalidSubDevice;
}

/*! Turns a string constant into it's value.
    \returns The constant value on success, or the constant itself on error.
 */
QString DeviceDescriptions::constantToString(const QString &constant) const
{
    Q_D(const DeviceDescriptions);

    if (constant.startsWith('$'))
    {
        const auto i = d->constants.find(constant);

        if (i != d->constants.end())
        {
            return i->second;
        }
    }

    return constant;
}

QString DeviceDescriptions::stringToConstant(const QString &str) const
{
    Q_D(const DeviceDescriptions);

    if (str.startsWith('$'))
    {
        return str;
    }

    const auto end = d->constants.cend();
    for (auto p = d->constants.begin(); p != end; ++p)
    {
        if (p->second == str)
        {
            return p->first;
        }
    }

    return str;
}

QStringList DeviceDescriptions::constants(const QString &prefix) const
{
    Q_D(const DeviceDescriptions);
    QStringList result;

    const auto end = d->constants.cend();
    for (auto p = d->constants.begin(); p != end; ++p)
    {
        if (prefix.isEmpty() || p->first.startsWith(prefix))
        {
            result.push_back(p->first);
        }
    }

    return result;
}

static DeviceDescription::Item *DDF_GetItemMutable(const ResourceItem *item)
{
    if (!_priv || !item)
    {
        return nullptr;
    }

    DeviceDescriptionsPrivate *d = _priv;

    ItemHandlePack h;
    h.handle = item->ddfItemHandle(); // unpack

    if (h.handle == DeviceDescription::Item::InvalidItemHandle)
    {
        return nullptr;
    }

    if (h.loadCounter != d->loadCounter)
    {
        return nullptr;
    }

    DBG_Assert(h.description < d->descriptions.size());
    if (h.description >= d->descriptions.size())
    {
        return nullptr;
    }

    auto &ddf = d->descriptions[h.description];

    DBG_Assert(h.subDevice < ddf.subDevices.size());
    if (h.subDevice >= ddf.subDevices.size())
    {
        return nullptr;
    }

    auto &sub = ddf.subDevices[h.subDevice];

    DBG_Assert(h.item < sub.items.size());

    if (h.item < sub.items.size())
    {
        return &sub.items[h.item];
    }

    return nullptr;
}

/*! Retrieves the DDF item for the given \p item.

    If \p item has a valid DDF item handle the respective entry is returned.
    Otherwise the generic item list is searched based on the item.suffix.

    The returned entry can be check with DeviceDescription::Item::isValid().
 */
const DeviceDescription::Item &DDF_GetItem(const ResourceItem *item)
{
    Q_ASSERT(_instance);
    return _instance->getItem(item);
}

/*! \see DDF_GetItem() description.
 */
const DeviceDescription::Item &DeviceDescriptions::getItem(const ResourceItem *item) const
{
    Q_D(const DeviceDescriptions);

    ItemHandlePack h;
    h.handle = item->ddfItemHandle(); // unpack

    if (h.handle == DeviceDescription::Item::InvalidItemHandle)
    {
        return getGenericItem(item->descriptor().suffix);
    }

    if (h.loadCounter != d->loadCounter)
    {
        return d->invalidItem;
    }

    // Note: There are no further if conditions since at this point it's certain that a handle must be valid.

    Q_ASSERT(h.description < d->descriptions.size());

    const auto &ddf = d->descriptions[h.description];

    Q_ASSERT(h.subDevice < ddf.subDevices.size());

    const auto &sub = ddf.subDevices[h.subDevice];

    Q_ASSERT(h.item < sub.items.size());

    return sub.items[h.item];
}

const DDF_Items &DeviceDescriptions::genericItems() const
{
    return d_ptr2->genericItems;
}

const DeviceDescription::Item &DeviceDescriptions::getGenericItem(const char *suffix) const
{
    Q_D(const DeviceDescriptions);

    for (const auto &item : d->genericItems)
    {
        if (item.name == QLatin1String(suffix))
        {
            return item;
        }
    }

    return d->invalidItem;
}

const std::vector<DDF_FunctionDescriptor> &DeviceDescriptions::getParseFunctions() const
{
    return d_ptr2->parseFunctions;
}

const std::vector<DDF_FunctionDescriptor> &DeviceDescriptions::getReadFunctions() const
{
    return d_ptr2->readFunctions;
}

const std::vector<DDF_SubDeviceDescriptor> &DeviceDescriptions::getSubDevices() const
{
    return d_ptr2->subDevices;
}

/*! Updates all DDF item handles to point to correct location.
    \p loadCounter - the current load counter.
 */
static void DDF_UpdateItemHandles(std::vector<DeviceDescription> &descriptions, uint loadCounter)
{
    int index = 0;
    Q_ASSERT(loadCounter >= HND_MIN_LOAD_COUNTER);
    Q_ASSERT(loadCounter <= HND_MAX_LOAD_COUNTER);

    ItemHandlePack handle;
    handle.description = 0;
    handle.loadCounter = loadCounter;

    for (auto &ddf : descriptions)
    {
        ddf.handle = index++;
        handle.subDevice = 0;
        for (auto &sub : ddf.subDevices)
        {
            handle.item = 0;

            for (auto &item : sub.items)
            {
                item.handle = handle.handle;
                Q_ASSERT(handle.item < HND_MAX_ITEMS);
                handle.item++;
            }

            Q_ASSERT(handle.subDevice < HND_MAX_SUB_DEVS);
            handle.subDevice++;
        }

        Q_ASSERT(handle.description < HND_MAX_DESCRIPTIONS);
        handle.description++;
    }
}

/*! Temporary workaround since DuktapeJS doesn't support 'let', try replace it with 'var'.

    The fix only applies if the JS doesn't compile and after the modified version successfully
    compiles. Can be removed onces all DDFs have been updated.

    Following cases are fixed:

    '^let '   // let at begin of the expression
    ' let '
    '\nlet '
    '\tlet '
    '(let '   // let within a scope like: for (let i=0; i < 3; i++) {...}
*/
static void DDF_TryCompileAndFixJavascript(QString *expr, const QString &path)
{
#ifdef USE_DUKTAPE_JS_ENGINE
    if (DeviceJs::instance()->testCompile(*expr) == JsEvalResult::Ok)
    {
        return;
    }

    int idx = 0;
    int nfixes = 0;
    QString fix = *expr;
    const QString letSearch("let");

    for ( ; idx != -1; )
    {
        idx = fix.indexOf(letSearch, idx);
        if (idx < 0)
        {
            break;
        }

        if (idx == 0 || fix.at(idx - 1).isSpace() || fix.at(idx - 1) == '(')
        {
            fix[idx + 0] = 'v';
            fix[idx + 1] = 'a';
            fix[idx + 2] = 'r';
            idx += 4;
            nfixes++;
        }
    }

    if (nfixes > 0 && DeviceJs::instance()->testCompile(fix) == JsEvalResult::Ok)
    {
        *expr = fix;
        return;
    }

    // if we get here, the expressions has other problems, print compile error and path
    DBG_Printf(DBG_DDF, "DDF failed to compile JS: %s\n%s\n", qPrintable(path), qPrintable(DeviceJs::instance()->errorString()));

#else
    Q_UNUSED(expr)
    Q_UNUSED(path)
#endif
}

/*! Reads all DDF related files.
 */
void DeviceDescriptions::readAll()
{
    Q_D(DeviceDescriptions);

    d->loadCounter = (d->loadCounter + 1) % HND_MAX_LOAD_COUNTER;
    if (d->loadCounter <= HND_MIN_LOAD_COUNTER)
    {
        d->loadCounter = HND_MIN_LOAD_COUNTER;
    }

    DBG_MEASURE_START(DDF_ReadAllFiles);

    std::vector<DeviceDescription> descriptions;
    std::vector<DeviceDescription::Item> genericItems;
    std::vector<DDF_SubDeviceDescriptor> subDevices;

    QStringList dirs;
    dirs.push_back(deCONZ::getStorageLocation(deCONZ::DdfUserLocation));
    dirs.push_back(deCONZ::getStorageLocation(deCONZ::DdfLocation));

    while (!dirs.isEmpty())
    {
        QDirIterator it(dirs.takeFirst(), QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);

        while (it.hasNext())
        {
            it.next();

            if (it.filePath().endsWith(QLatin1String("generic/constants.json")))
            {
                std::map<QString,QString> constants;
                if (DDF_ReadConstantsJson(it.filePath(), &constants))
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
                    auto result = DDF_ReadItemFile(it.filePath());
                    if (result.isValid())
                    {
                        result.isGenericRead = !result.readParameters.isNull() ? 1 : 0;
                        result.isGenericWrite = !result.writeParameters.isNull() ? 1 : 0;
                        result.isGenericParse = !result.parseParameters.isNull() ? 1 : 0;
                        genericItems.push_back(std::move(result));
                    }
                }
                else if (it.filePath().contains(QLatin1String("generic/subdevices/")))
                {
                    auto sub = DDF_ReadSubDeviceFile(it.filePath());
                    if (isValid(sub))
                    {
                        subDevices.push_back(sub);
                    }
                }
                else
                {
                    DBG_Printf(DBG_DDF, "read %s\n", qPrintable(it.fileName()));
                    std::vector<DeviceDescription> result = DDF_ReadDeviceFile(it.filePath());
                    std::move(result.begin(), result.end(), std::back_inserter(descriptions));
                }
            }
        }
    }

    if (!genericItems.empty())
    {
        d->genericItems = std::move(genericItems);
    }

    if (!subDevices.empty())
    {
        std::sort(subDevices.begin(), subDevices.end(), [](const auto &a, const auto &b){
            return a.name < b.name;
        });

        d->subDevices = std::move(subDevices);
    }

    if (!descriptions.empty())
    {
        d->descriptions = std::move(descriptions);
        DDF_UpdateItemHandles(d->descriptions, d->loadCounter);

        for (auto &ddf : d->descriptions)
        {
            ddf = DDF_MergeGenericItems(d->genericItems, ddf);
            ddf = DDF_LoadScripts(ddf);
        }
    }

    DBG_MEASURE_END(DDF_ReadAllFiles);
}

/*! Tries to init a Device from an DDF file.

    Currently this is done syncronously later on it will be async to not block
    the main thread while loading DDF files.
 */
void DeviceDescriptions::handleDDFInitRequest(const Event &event)
{
    Q_D(DeviceDescriptions);

    auto *resource = DEV_GetResource(RDevices, QString::number(event.deviceKey()));

    int result = -1; // error

    if (resource)
    {
        const auto ddf = get(resource);

        if (ddf.isValid())
        {
            result = 0;

            if (!DEV_TestManaged() && !DDF_IsStatusEnabled(ddf.status))
            {
                result = 2;
            }
            else if (DEV_InitDeviceFromDescription(static_cast<Device*>(resource), ddf))
            {
                result = 1; // ok

                if (ddf.status == QLatin1String("Draft"))
                {
                    result = 2;
                }
            }
        }

        if (result >= 0)
        {
            DBG_Printf(DBG_INFO, "DEV found DDF for 0x%016llX, path: %s\n", event.deviceKey(), qPrintable(ddf.path));
        }

        if (result == 0)
        {
            DBG_Printf(DBG_INFO, "DEV init Device from DDF for 0x%016llX failed\n", event.deviceKey());
        }
        else if (result == -1)
        {
            DBG_Printf(DBG_INFO, "DEV no DDF for 0x%016llX, modelId: %s\n", event.deviceKey(), qPrintable(resource->item(RAttrModelId)->toString()));
            DBG_Printf(DBG_INFO, "DEV create on-the-fly DDF for 0x%016llX\n", event.deviceKey());

            DeviceDescription ddf1;

            Device *device = static_cast<Device*>(resource);

            if (DEV_InitBaseDescriptionForDevice(device, ddf1))
            {
                d->descriptions.push_back(ddf1);
                DDF_UpdateItemHandles(d->descriptions, d->loadCounter);
            }
        }
    }

    emit eventNotify(Event(RDevices, REventDDFInitResponse, result, event.deviceKey()));
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
        DBG_Printf(DBG_INFO, "DDF failed to read device constants: %s, err: %s, offset: %d\n", qPrintable(path), qPrintable(error.errorString()), error.offset);
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

    return !constants->empty();
}

ApiDataType API_DataTypeFromString(const QString &str)
{
    if (str == QLatin1String("bool")) return DataTypeBool;
    if (str == QLatin1String("uint8")) return DataTypeUInt8;
    if (str == QLatin1String("uint16")) return DataTypeUInt16;
    if (str == QLatin1String("uint32")) return DataTypeUInt32;
    if (str == QLatin1String("uint64")) return DataTypeUInt64;
    if (str == QLatin1String("int8")) return DataTypeInt8;
    if (str == QLatin1String("int16")) return DataTypeInt16;
    if (str == QLatin1String("int32")) return DataTypeInt32;
    if (str == QLatin1String("int64")) return DataTypeInt64;
    if (str == QLatin1String("string")) return DataTypeString;
    if (str == QLatin1String("double")) return DataTypeReal;
    if (str == QLatin1String("time")) return DataTypeTime;
    if (str == QLatin1String("timepattern")) return DataTypeTimePattern;

    return DataTypeUnknown;
}

/*! Parses an item object.
    \returns A parsed item, use DeviceDescription::Item::isValid() to check for success.
 */
static DeviceDescription::Item DDF_ParseItem(const QJsonObject &obj)
{
    DeviceDescription::Item result{};

    if (obj.contains(QLatin1String("name")))
    {
        result.name = obj.value(QLatin1String("name")).toString().toUtf8().constData();
    }
    else if (obj.contains(QLatin1String("id"))) // generic/item TODO align name/id?
    {
        result.name = obj.value(QLatin1String("id")).toString().toUtf8().constData();
    }

    // Handle deprecated names/ids
    if (result.name == RConfigColorCapabilities) { result.name = RCapColorCapabilities; }
    if (result.name == RConfigCtMax) { result.name = RCapColorCtMax; }
    if (result.name == RConfigCtMin) { result.name = RCapColorCtMin; }

    if (obj.contains(QLatin1String("description")))
    {
        result.description = obj.value(QLatin1String("description")).toString();
    }

    if (result.name.empty())
    {
        return {};
    }

    // try to create a dynamic ResourceItemDescriptor
    if (!getResourceItemDescriptor(result.name, result.descriptor))
    {
        QString schema;
        if (obj.contains(QLatin1String("schema")))
        {
            schema = obj.value(QLatin1String("schema")).toString();
        }

        if (schema == QLatin1String("resourceitem1.schema.json"))
        {
            QString dataType;
            ResourceItemDescriptor rid{};

            if (obj.contains(QLatin1String("access")))
            {
                const auto access = obj.value(QLatin1String("access")).toString();
                if (access == QLatin1String("R"))
                {
                    rid.access = ResourceItemDescriptor::Access::ReadOnly;
                }
                else if (access == QLatin1String("RW"))
                {
                    rid.access = ResourceItemDescriptor::Access::ReadWrite;
                }
            }

            if (obj.contains(QLatin1String("datatype")))
            {
                QString dataType = obj.value(QLatin1String("datatype")).toString().toLower();
                rid.type = API_DataTypeFromString(dataType);
                if (dataType.startsWith("uint") || dataType.startsWith("int"))
                {
                    rid.qVariantType = QVariant::Double;
                }
                else if (rid.type == DataTypeReal)
                {
                    rid.qVariantType = QVariant::Double;
                }
                else if (rid.type == DataTypeBool)
                {
                    rid.qVariantType = QVariant::Bool;
                }
                else
                {
                    DBG_Assert(rid.type == DataTypeString || rid.type == DataTypeTime || rid.type == DataTypeTimePattern);
                    rid.qVariantType = QVariant::String;
                }
            }

            if (obj.contains(QLatin1String("range")))
            {
                const auto range = obj.value(QLatin1String("range")).toArray();
                if (range.count() == 2)
                {
                    bool ok1 = false;
                    bool ok2 = false;
                    double rangeMin = range.at(0).toString().toDouble(&ok1);
                    double rangeMax = range.at(1).toString().toDouble(&ok2);

                    if (ok1 && ok2)
                    {
                        rid.validMin = rangeMin;
                        rid.validMax = rangeMax;
                    }
                    // TODO validate range according to datatype
                }
            }

            if (rid.isValid())
            {
                rid.flags = ResourceItem::FlagDynamicDescriptor;

                // TODO this is fugly, should later on be changed to use the atom table
                size_t len = result.name.size();
                char *dynSuffix  = new char[len + 1];
                memcpy(dynSuffix, result.name.c_str(), len);
                dynSuffix[len] = '\0';
                rid.suffix = dynSuffix;

                // TODO ResourceItemDescriptor::flags (push, etc.)
                if (R_AddResourceItemDescriptor(rid))
                {
                    DBG_Printf(DBG_DDF, "DDF added dynamic ResourceItemDescriptor %s\n", result.name.c_str());
                }
            }
        }
        else
        {
            DBG_Printf(DBG_DDF, "DDF unsupported ResourceItem schema: %s\n", qPrintable(schema));
        }
    }

    if (getResourceItemDescriptor(result.name, result.descriptor))
    {
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

        if (obj.contains(QLatin1String("public")))
        {
            result.isPublic = obj.value(QLatin1String("public")).toBool() ? 1 : 0;
            result.hasIsPublic = 1;
        }

        if (obj.contains(QLatin1String("implicit")))
        {
            result.isImplicit = obj.value(QLatin1String("implicit")).toBool() ? 1 : 0;
        }

        if (obj.contains(QLatin1String("awake")))
        {
            result.awake = obj.value(QLatin1String("awake")).toBool() ? 1 : 0;
        }

        if (obj.contains(QLatin1String("managed")))
        {
            result.isManaged = obj.value(QLatin1String("managed")).toBool() ? 1 : 0;
        }

        if (obj.contains(QLatin1String("static")))
        {
            result.isStatic = 1;
            result.defaultValue = obj.value(QLatin1String("static")).toVariant();
        }
        else
        {
            if (obj.contains(QLatin1String("default")))
            {
                result.defaultValue = obj.value(QLatin1String("default")).toVariant();
            }

            const auto parse = obj.value(QLatin1String("parse"));
            if (parse.isObject())
            {
                result.parseParameters = parse.toVariant();
            }

            const auto read = obj.value(QLatin1String("read"));
            if (read.isObject())
            {
                result.readParameters = read.toVariant();
            }

            if (obj.contains(QLatin1String("refresh.interval")))
            {
                result.refreshInterval = obj.value(QLatin1String("refresh.interval")).toInt(0);
            }

            const auto write = obj.value(QLatin1String("write"));
            if (write.isObject())
            {
                result.writeParameters = write.toVariant();
            }
        }

        DBG_Printf(DBG_DDF, "DDF loaded resource item descriptor: %s, public: %u\n", result.descriptor.suffix, (result.isPublic ? 1 : 0));
    }
    else
    {
        DBG_Printf(DBG_DDF, "DDF failed to load resource item descriptor: %s\n", result.name.c_str());
    }

    return result;
}

/*! Parses a sub device in a DDF object "subdevices" array.
    \returns The sub device object, use DeviceDescription::SubDevice::isValid() to check for success.
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

    if (obj.contains(QLatin1String("meta")))
    {
        auto meta = obj.value(QLatin1String("meta"));
        if (meta.isObject())
        {
            result.meta = meta.toVariant().toMap();
        }
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

// {"at": "0x0021", "dt": "u8", "min": 5, "max": 3600, "change": 1 },

/*! Parses a ZCL report in a DDF_Binding object "report" array.
    \returns The ZCL report, use DDF_ZclReport::isValid() to check for success.
 */
static DDF_ZclReport DDF_ParseZclReport(const QJsonObject &obj)
{
    DDF_ZclReport result{};

    // check required fields
    if (!obj.contains(QLatin1String("at")) ||
        !obj.contains(QLatin1String("dt")) ||
        !obj.contains(QLatin1String("min")) ||
        !obj.contains(QLatin1String("max")))
    {
        return {};
    }

    bool ok = false;
    result.attributeId = obj.value(QLatin1String("at")).toString().toUShort(&ok, 0);

    if (!ok)
    {
        return {};
    }

    {
        auto dataType = obj.value(QLatin1String("dt")).toString().toUShort(&ok, 0);
        if (!ok || dataType > 0xFF)
        {
            return {};
        }
        result.dataType = dataType;
    }

    {
        const auto minInterval = obj.value(QLatin1String("min")).toInt(-1);

        if (minInterval < 0 || minInterval > UINT16_MAX)
        {
            return {};
        }

        result.minInterval = minInterval;
    }

    {
        const auto maxInterval = obj.value(QLatin1String("max")).toInt(-1);

        if (maxInterval < 0 || maxInterval > UINT16_MAX)
        {
            return {};
        }

        result.maxInterval = maxInterval;
    }

    if (obj.contains(QLatin1String("change")))
    {
        result.reportableChange = obj.value(QLatin1String("change")).toString().toUInt(&ok, 0);

        if (!ok)
        {
            return {};
        }
    }

    if (obj.contains(QLatin1String("mf")))
    {
        result.manufacturerCode = obj.value(QLatin1String("mf")).toString().toUShort(&ok, 0);

        if (!ok)
        {
            return {};
        }
    }

    result.valid = true;

    return result;
}

/*! Parses a binding in a DDF object "bindings" array.
    \returns The binding, use DDF_Binding::isValid() to check for success.
 */
static DDF_Binding DDF_ParseBinding(const QJsonObject &obj)
{
    DDF_Binding result{};

    // check required fields
    if (!obj.contains(QLatin1String("bind")) ||
        !obj.contains(QLatin1String("src.ep")) ||
        !obj.contains(QLatin1String("cl")))
    {
        return {};
    }

    const auto type = obj.value(QLatin1String("bind")).toString();

    if (type == QLatin1String("unicast"))
    {
        result.isUnicastBinding = 1;
    }
    else if (type == QLatin1String("groupcast"))
    {
        result.isGroupBinding = 1;
    }
    else
    {
        return {};
    }

    bool ok = false;
    {
        const auto srcEndpoint = obj.value(QLatin1String("src.ep")).toInt(-1);

        if (srcEndpoint < 0 || srcEndpoint > UINT8_MAX)
        {
            return {};
        }
        result.srcEndpoint = srcEndpoint;
    }

    {
        result.clusterId = obj.value(QLatin1String("cl")).toString().toUShort(&ok, 0);

        if (!ok)
        {
            return {};
        }
    }

    if (obj.contains(QLatin1String("dst.ep")))
    {
        const auto dstEndpoint = obj.value(QLatin1String("dst.ep")).toInt(-1);
        if (dstEndpoint < 0 || dstEndpoint >= 255)
        {
            return {};
        }
        result.dstEndpoint = dstEndpoint;
    }
    else
    {
        result.dstEndpoint = 0;
    }

    if (result.isGroupBinding && obj.contains(QLatin1String("config.group")))
    {
        const auto configGroup = obj.value(QLatin1String("config.group")).toInt(-1);
        if (configGroup < 0 || configGroup >= 255)
        {
            return {};
        }
        result.configGroup = configGroup;
    }
    else
    {
        result.configGroup = 0;
    }

    const auto report = obj.value(QLatin1String("report"));
    if (report.isArray())
    {
        const auto reportArr = report.toArray();
        for (const auto &i : reportArr)
        {
            if (i.isObject())
            {
                const auto rep = DDF_ParseZclReport(i.toObject());
                if (isValid(rep))
                {
                    result.reporting.push_back(rep);
                }
            }
        }
    }

    return result;
}

/*! Parses a single string or array of strings in DDF JSON object.
    The obj[key] value can be a string or array of strings.
    \returns List of parsed strings.
 */
static QStringList DDF_ParseStringOrList(const QJsonObject &obj, QLatin1String key)
{
    QStringList result;
    const auto val = obj.value(key);

    if (val.isString()) // "key": "alpha.sensor"
    {
        result.push_back(val.toString());
    }
    else if (val.isArray()) // "key": [ "alpha.sensor", "beta.sensor" ]
    {
        const auto arr = val.toArray();
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
static DeviceDescription DDF_ParseDeviceObject(const QJsonObject &obj, const QString &path)
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

    result.path = path;
    result.manufacturerNames = DDF_ParseStringOrList(obj, QLatin1String("manufacturername"));
    result.modelIds = DDF_ParseStringOrList(obj, QLatin1String("modelid"));
    result.product = obj.value(QLatin1String("product")).toString();

    if (obj.contains(QLatin1String("status")))
    {
        result.status = obj.value(QLatin1String("status")).toString();
    }

    if (obj.contains(QLatin1String("vendor")))
    {
        result.vendor = obj.value(QLatin1String("vendor")).toString();
    }

    if (obj.contains(QLatin1String("sleeper")))
    {
        result.sleeper = obj.value(QLatin1String("sleeper")).toBool() ? 1 : 0;
    }

    if (obj.contains(QLatin1String("matchexpr")))
    {
        result.matchExpr = obj.value(QLatin1String("matchexpr")).toString();
    }

    const auto keys = obj.keys();
    for (const auto &key : keys)
    {
        DBG_Printf(DBG_DDF, "DDF %s: %s\n", qPrintable(key), qPrintable(obj.value(key).toString()));
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

    const auto bindings = obj.value(QLatin1String("bindings"));
    if (bindings.isArray())
    {
        const auto bindingsArr = bindings.toArray();
        for (const auto &i : bindingsArr)
        {
            if (i.isObject())
            {
                const auto bnd = DDF_ParseBinding(i.toObject());
                if (isValid(bnd))
                {
                    result.bindings.push_back(bnd);
                }
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
        DBG_Printf(DBG_DDF, "DDF failed to read %s, err: %s, offset: %d\n", qPrintable(path), qPrintable(error.errorString()), error.offset);
        return { };
    }

    if (doc.isObject())
    {
        return DDF_ParseItem(doc.object());
    }

    return { };
}

/*! Reads an subdevice file under (generic/subdevices/).
    \returns A parsed subdevice, use isValid(DDF_SubDeviceDescriptor) to check for success.
 */
static DDF_SubDeviceDescriptor DDF_ReadSubDeviceFile(const QString &path)
{
    DDF_SubDeviceDescriptor result;

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
        DBG_Printf(DBG_DDF, "DDF failed to read %s, err: %s, offset: %d\n", qPrintable(path), qPrintable(error.errorString()), error.offset);
        return result;
    }

    if (doc.isObject())
    {
        const auto obj = doc.object();
        QString schema;
        if (obj.contains(QLatin1String("schema")))
        {
            schema = obj.value(QLatin1String("schema")).toString();
        }

        if (schema != QLatin1String("subdevice1.schema.json"))
        {
            return result;
        }

        if (obj.contains(QLatin1String("name")))
        {
            result.name = obj.value(QLatin1String("name")).toString();
        }
        if (obj.contains(QLatin1String("type")))
        {
            result.type = obj.value(QLatin1String("type")).toString();
        }
        if (obj.contains(QLatin1String("restapi")))
        {
            result.restApi = obj.value(QLatin1String("restapi")).toString();
        }

        result.order = obj.value(QLatin1String("order")).toInt(SUBDEVICE_DEFAULT_ORDER);

        if (obj.contains(QLatin1String("uuid")))
        {
            const auto uniqueId = obj.value(QLatin1String("uuid"));
            if (uniqueId.isArray())
            {
                const auto arr = uniqueId.toArray();
                for (const auto &i : arr)
                {
                    DBG_Assert(i.isString());
                    result.uniqueId.push_back(i.toString());
                }
            }
        }
        if (obj.contains(QLatin1String("items")))
        {
            const auto items = obj.value(QLatin1String("items"));
            if (items.isArray())
            {
                const auto arr = items.toArray();
                for (const auto &i : arr)
                {
                    DBG_Assert(i.isString());
                    ResourceItemDescriptor rid;
                    if (getResourceItemDescriptor(i.toString(), rid))
                    {
                        result.items.push_back(rid.suffix);
                    }
                }
            }
        }
    }

    return result;
}

QVariant DDF_ResolveParamScript(const QVariant &param, const QString &path)
{
    auto result = param;

    if (param.type() != QVariant::Map)
    {
        return result;
    }

    auto map = param.toMap();

    if (map.contains(QLatin1String("script")))
    {
        const auto script = map["script"].toString();

        const QFileInfo fi(path);
        QFile f(fi.canonicalPath() + "/" + script);

        if (f.exists() && f.open(QFile::ReadOnly))
        {
            QString content = f.readAll();
            if (!content.isEmpty())
            {
                DDF_TryCompileAndFixJavascript(&content, path);
                map["eval"] = content;
                result = std::move(map);
            }
        }
    }
    else if (map.contains(QLatin1String("eval")))
    {
        QString content = map[QLatin1String("eval")].toString();
        if (!content.isEmpty())
        {
            DDF_TryCompileAndFixJavascript(&content, path);
            map[QLatin1String("eval")] = content;
            result = std::move(map);
        }
    }

    return result;
}

DeviceDescription DDF_LoadScripts(const DeviceDescription &ddf)
{
    auto result = ddf;

    for (auto &sub : result.subDevices)
    {
        for (auto &item : sub.items)
        {
            item.parseParameters = DDF_ResolveParamScript(item.parseParameters, ddf.path);
            item.readParameters = DDF_ResolveParamScript(item.readParameters, ddf.path);
            item.writeParameters = DDF_ResolveParamScript(item.writeParameters, ddf.path);
        }
    }

    return result;
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
        DBG_Printf(DBG_DDF, "DDF failed to read %s, err: %s, offset: %d\n", qPrintable(path), qPrintable(error.errorString()), error.offset);
        return result;
    }

    if (doc.isObject())
    {
        const auto ddf = DDF_ParseDeviceObject(doc.object(), path);
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
                const auto ddf = DDF_ParseDeviceObject(i.toObject(), path);
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
                                              [&item](const DeviceDescription::Item &i){ return i.descriptor.suffix == item.descriptor.suffix; });
            if (genItem == genericItems.cend())
            {
                continue;
            }

            item.isImplicit = genItem->isImplicit;
            item.isManaged = genItem->isManaged;
            item.isGenericRead = 0;
            item.isGenericWrite = 0;
            item.isGenericParse = 0;

            if (!item.isStatic)
            {
                if (item.readParameters.isNull()) { item.readParameters = genItem->readParameters; item.isGenericRead = 1; }
                if (item.writeParameters.isNull()) { item.writeParameters = genItem->writeParameters; item.isGenericWrite = 1; }
                if (item.parseParameters.isNull()) { item.parseParameters = genItem->parseParameters; item.isGenericParse = 1; }
                if (item.refreshInterval == DeviceDescription::Item::NoRefreshInterval && genItem->refreshInterval != item.refreshInterval)
                {
                    item.refreshInterval = genItem->refreshInterval;
                }
            }

            if (item.descriptor.access == ResourceItemDescriptor::Access::Unknown)
            {
                item.descriptor.access = genItem->descriptor.access;
            }

            if (!item.hasIsPublic)
            {
                item.isPublic = genItem->isPublic;
            }

            if (!item.defaultValue.isValid() && genItem->defaultValue.isValid())
            {
                item.defaultValue = genItem->defaultValue;
            }
        }
    }

    return result;
}

uint8_t DDF_GetSubDeviceOrder(const QString &type)
{
    if (type.isEmpty() || type.startsWith(QLatin1String("CLIP")))
    {
        return SUBDEVICE_DEFAULT_ORDER;
    }

    if (_priv)
    {
        auto i = std::find_if(_priv->subDevices.cbegin(), _priv->subDevices.cend(), [&](const auto &sub)
        { return sub.name == type; });

        if (i != _priv->subDevices.cend())
        {
            return i->order;
        }
    }

#ifdef QT_DEBUG
    DBG_Printf(DBG_DDF, "DDF No subdevice for type: %s\n", qPrintable(type));
#endif

    return SUBDEVICE_DEFAULT_ORDER;
}

/*! Creates a unique Resource handle.
 */
Resource::Handle R_CreateResourceHandle(const Resource *r, size_t containerIndex)
{
    Q_ASSERT(r->prefix() != nullptr);
    Q_ASSERT(!r->item(RAttrUniqueId)->toString().isEmpty());

    Resource::Handle result;
    result.hash = qHash(r->item(RAttrUniqueId)->toString());
    result.index = containerIndex;
    result.type = r->prefix()[1];
    result.order = 0;

    Q_ASSERT(result.type == 's' || result.type == 'l' || result.type == 'd' || result.type == 'g');
    Q_ASSERT(isValid(result));

    if (result.type == 's' || result.type == 'l')
    {
        const ResourceItem *type = r->item(RAttrType);
        if (type)
        {
            result.order = DDF_GetSubDeviceOrder(type->toString());
        }
    }

    return result;
}
