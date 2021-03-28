#include "resource.h"
#include "device_js_wrappers.h"

JsResource::JsResource(QJSEngine *parent) :
    QObject(parent)
{

}

QJSValue JsResource::item(const QString &suffix)
{
    ResourceItemDescriptor rid;
    if (r && getResourceItemDescriptor(suffix, rid))
    {
        auto it = r->item(rid.suffix);
        if (it)
        {
            auto *ritem = new JsResourceItem(this);
            ritem->item = it;
            return static_cast<QJSEngine*>(parent())->newQObject(ritem);
        }
    }

    return {};
}

JsResourceItem::JsResourceItem(QObject *parent) :
    QObject(parent)
{

}

JsResourceItem::~JsResourceItem()
{
    if (item)
    {
        DBG_Printf(DBG_INFO, "dtor %s\n", item->descriptor().suffix);
        item = nullptr;
    }
}

QJSValue JsResourceItem::str() const
{
    if (item)
    {
        return item->toString();
    }

    return {};
}

QVariant JsResourceItem::value() const
{
    if (!item)
    {
        return {};
    }

    const auto type = item->descriptor().type;
    if (type == DataTypeBool)
    {
        return item->toBool();
    }

    if (type == DataTypeString || type == DataTypeTime ||  type == DataTypeTimePattern)
    {
        return item->toString();
    }

    if (item->descriptor().type == DataTypeUInt8 || type == DataTypeUInt16 || type == DataTypeUInt32)
    {
        return static_cast<quint32>(item->toNumber());
    }

    if (item->descriptor().type == DataTypeInt8 || type == DataTypeInt16 || type == DataTypeInt32)
    {
        return static_cast<qint32>(item->toNumber());
    }

    if (item->descriptor().type == DataTypeInt64 || type == DataTypeUInt64)
    {
        return QString::number(item->toNumber());
    }

    return {};
}

void JsResourceItem::setValue(const QVariant &val)
{
    if (item)
    {
        DBG_Printf(DBG_INFO, "set %s = %s\n", item->descriptor().suffix, qPrintable(val.toString()));
    }
}
