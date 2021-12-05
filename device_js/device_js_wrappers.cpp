/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "resource.h"
#include "device_js_wrappers.h"

JsResource::JsResource(QJSEngine *parent) :
    QObject(parent)
{

}

QJSValue JsResource::item(const QString &suffix)
{
    ResourceItemDescriptor rid;

    if (!getResourceItemDescriptor(suffix, rid))
    {
        return {};
    }

    ResourceItem *item = r ? r->item(rid.suffix) : nullptr;
    const ResourceItem *citem = cr ? cr->item(rid.suffix) : nullptr;

    if (item || citem)
    {
        auto *ritem = new JsResourceItem(this);
        ritem->item = item;
        ritem->citem = citem;
        return static_cast<QJSEngine*>(parent())->newQObject(ritem);
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
        item = nullptr;
    }
}

QVariant JsResourceItem::value() const
{
    if (!citem)
    {
        return {};
    }

    const auto type = citem->descriptor().type;
    if (type == DataTypeBool)
    {
        return citem->toBool();
    }

    if (type == DataTypeString || type == DataTypeTime ||  type == DataTypeTimePattern)
    {
        return citem->toString();
    }

    if (type == DataTypeUInt8 || type == DataTypeUInt16 || type == DataTypeUInt32)
    {
        return static_cast<quint32>(citem->toNumber());
    }

    if (type == DataTypeInt8 || type == DataTypeInt16 || type == DataTypeInt32)
    {
        return static_cast<qint32>(citem->toNumber());
    }

    if (type == DataTypeInt64 || type == DataTypeUInt64)
    {
        return QString::number(citem->toNumber());
    }

    return {};
}

void JsResourceItem::setValue(const QVariant &val)
{
    if (item)
    {
//        DBG_Printf(DBG_INFO, "JsResourceItem.setValue(%s) = %s\n", item->descriptor().suffix, qPrintable(val.toString()));
        item->setValue(val, ResourceItem::SourceDevice);
    }
}

QString JsResourceItem::name() const
{
    if (citem)
    {
        return QLatin1String(citem->descriptor().suffix);
    }

    return {};
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

    case deCONZ::ZclSingleFloat:
        return attr->numericValue().real;

    default:
        break;
    }

    if (attr->toVariant().isValid())
    {
        return attr->toVariant();
    }

    return {};
}

int JsZclAttribute::id() const
{
    if (attr)
    {
        return attr->id();
    }

    return -1;
}

int JsZclAttribute::dataType() const
{
    if (attr)
    {
        return attr->dataType();
    }

    return -1;
}

JsZclFrame::JsZclFrame(QObject *parent) :
    QObject(parent)
{

}

int JsZclFrame::at(int i) const
{
    if (zclFrame && i >= 0 && i < zclFrame->payload().size())
    {
        return zclFrame->payload().at(i);
    }

    return 0;
}

int JsZclFrame::cmd() const
{
    if (zclFrame)
    {
        return zclFrame->commandId();
    }

    return -1;
}

int JsZclFrame::payloadSize() const
{
    if (zclFrame)
    {
        return zclFrame->payload().size();
    }
    return 0;
}

bool JsZclFrame::isClCmd() const
{
    if (zclFrame)
    {
        return zclFrame->isClusterCommand();
    }

    return false;
}
