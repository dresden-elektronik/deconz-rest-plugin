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

    if (type == DataTypeUInt8 || type == DataTypeUInt16 || type == DataTypeUInt32)
    {
        return static_cast<quint32>(item->toNumber());
    }

    if (type == DataTypeInt8 || type == DataTypeInt16 || type == DataTypeInt32)
    {
        return static_cast<qint32>(item->toNumber());
    }

    if (type == DataTypeInt64 || type == DataTypeUInt64)
    {
        return QString::number(item->toNumber());
    }

    return {};
}

void JsResourceItem::setValue(const QVariant &val)
{
    if (item)
    {
        DBG_Printf(DBG_INFO, "JsResourceItem.setValue(%s) = %s\n", item->descriptor().suffix, qPrintable(val.toString()));
        item->setValue(val, ResourceItem::SourceDevice);
    }
}

JsZclAttribute::JsZclAttribute(QObject *parent) :
    QObject(parent)
{

}

QVariant JsZclAttribute::value() const
{
    if (!attr)
    {
        return {};
    }

    const auto type = attr->dataType();

    if (type == deCONZ::ZclBoolean)
    {
        return attr->numericValue().u8 > 0;
    }

    switch (type)
    {
    case deCONZ::Zcl8BitBitMap:
    case deCONZ::Zcl8BitData:
    case deCONZ::Zcl8BitUint:
    case deCONZ::Zcl8BitEnum:
    case deCONZ::Zcl16BitBitMap:
    case deCONZ::Zcl16BitData:
    case deCONZ::Zcl16BitUint:
    case deCONZ::Zcl16BitEnum:
    case deCONZ::Zcl24BitBitMap:
    case deCONZ::Zcl24BitData:
    case deCONZ::Zcl24BitUint:
    case deCONZ::Zcl32BitBitMap:
    case deCONZ::Zcl32BitData:
    case deCONZ::Zcl32BitUint:
    case deCONZ::Zcl40BitBitMap:
    case deCONZ::Zcl40BitData:
    case deCONZ::Zcl40BitUint:
    case deCONZ::Zcl48BitBitMap:
    case deCONZ::Zcl48BitData:
    case deCONZ::Zcl48BitUint:
    case deCONZ::Zcl56BitBitMap:
    case deCONZ::Zcl56BitData:
    case deCONZ::Zcl56BitUint:
    case deCONZ::Zcl64BitBitMap:
    case deCONZ::Zcl64BitUint:
    case deCONZ::Zcl64BitData:
    case deCONZ::ZclIeeeAddress:
        return QString::number(attr->numericValue().u64);

    case deCONZ::Zcl48BitInt:
    case deCONZ::Zcl56BitInt:
    case deCONZ::Zcl64BitInt:
        return QString::number(attr->numericValue().s64);
    default:
        break;
    }

    if (attr->toVariant().isValid())
    {
        return attr->toVariant();
    }

    return {};
}
