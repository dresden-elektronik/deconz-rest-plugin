/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "device.h"
#include "device_access_fn.h"
#include "device_js.h"

enum DA_Constants
{
    BroadcastEndpoint = 255, //! Accept incoming commands from any endpoint.
    AutoEndpoint = 0 //! Use src/dst endpoint of the related Resource (uniqueid).
};

struct ZclParam
{
    bool valid = false;
    quint8 endpoint;
    quint16 clusterId;
    quint16 manufacturerCode;
    std::vector<quint16> attributes;
};

quint8 zclNextSequenceNumber(); // todo defined in de_web_plugin_private.h

/*! Helper to get an unsigned int from \p var which might be a number or string value.

   \param var - Holds the string or number.
   \param max – Upper bound of the allowed value.
   \param ok – true if var holds and uint which is <= \p max.
 */
uint variantToUint(const QVariant &var, size_t max, bool *ok)
{
    Q_ASSERT(ok);
    *ok = false;
    const auto val = var.toString().toUInt(ok, 0);
    *ok = *ok && val <= max;

    return *ok ? val : 0;
}

/*! Extracts common ZCL parameters from an object.
 */
static ZclParam getZclParam(const QVariantMap &param)
{
    ZclParam result;

    if (param.contains("ep") && param.contains("cl") && param.contains("at"))
    {

    }
    else
    {
        return result;
    }

    bool ok = false;
    result.endpoint = variantToUint(param["ep"], UINT8_MAX, &ok);
    result.clusterId = ok ? variantToUint(param["cl"], UINT16_MAX, &ok) : 0;
    result.manufacturerCode = ok && param.contains("mf") ? variantToUint(param["mf"], UINT16_MAX, &ok) : 0;

    QVariantList attrArr;
    if (param["at"].type() == QVariant::List)
    {
        attrArr = param["at"].toList();
    }
    else if (param["at"].type() == QVariant::String)
    {
        attrArr.push_back(param["at"]);
    }

    const QVariantList &arr = attrArr; // get rid of detached Qt container warning
    for (const auto &at : arr)
    {
        ok = ok && at.type() == QVariant::String;
        if (ok)
        {
            result.attributes.push_back(variantToUint(at, UINT16_MAX, &ok));
        }
    }

    result.valid = ok && !result.attributes.empty();
    return result;
}

/*! A generic function to parse ZCL values from read/report commands.
    The item->parseParameters() is expected to contain 5 elements (given in the device description file).

    ["parseGenericAttribute/4", endpoint, clusterId, attributeId, expression]

    - endpoint, 0xff means any endpoint
    - clusterId: string hex value
    - attributeId: string hex value
    - expression: Javascript expression to transform the raw value

    Example: { "parse": ["parseGenericAttribute/4", 1, "0x0402", "0x0000", "$raw + $config/offset"] }
 */
bool parseGenericAttribute4(Resource *r, ResourceItem *item, const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame)
{
    bool result = false;

    if (zclFrame.commandId() != deCONZ::ZclReadAttributesResponseId && zclFrame.commandId() != deCONZ::ZclReportAttributesId)
    {
        return result;
    }

    if (!item->parseFunction()) // init on first call
    {
        Q_ASSERT(item->parseParameters().size() == 5);
        if (item->parseParameters().size() != 5)
        {
            return result;
        }
        bool ok;
        auto endpoint = item->parseParameters().at(1).toString().toUInt(&ok, 0);

        if (endpoint == AutoEndpoint)
        {
            // hack to get endpoint. todo find better solution
            auto ls = r->item(RAttrUniqueId)->toString().split('-', QString::SkipEmptyParts);
            if (ls.size() >= 2)
            {
                bool ok = false;
                uint ep = ls[1].toUInt(&ok, 10);
                if (ok && ep < BroadcastEndpoint)
                {
                    endpoint = ep;
                }
            }
        }

        const auto clusterId = ok ? item->parseParameters().at(2).toString().toUInt(&ok, 0) : 0;
        const auto attributeId = ok ? item->parseParameters().at(3).toString().toUInt(&ok, 0) : 0;

        if (!ok)
        {
            return result;
        }

        item->setParseFunction(parseGenericAttribute4);
        item->setZclProperties(clusterId, {quint16(attributeId)}, endpoint);
    }

    if (ind.clusterId() != item->clusterId() || zclFrame.payload().isEmpty())
    {
        return result;
    }

    if (item->endpoint() < BroadcastEndpoint && item->endpoint() != ind.srcEndpoint())
    {
        return result;
    }

    QDataStream stream(zclFrame.payload());
    stream.setByteOrder(QDataStream::LittleEndian);

    while (!stream.atEnd())
    {
        quint16 attrId;
        quint8 status;
        quint8 dataType;

        stream >> attrId;

        if (zclFrame.commandId() == deCONZ::ZclReadAttributesResponseId)
        {
            stream >> status;
            if (status != deCONZ::ZclSuccessStatus)
            {
                continue;
            }
        }

        stream >> dataType;
        deCONZ::ZclAttribute attr(attrId, dataType, QLatin1String(""), deCONZ::ZclReadWrite, true);

        if (!attr.readFromStream(stream))
        {
            break;
        }

        if (std::find(item->attributes().begin(), item->attributes().end(), attrId) != item->attributes().end())
        {
            const auto expr = item->parseParameters().back().toString();

            if (!expr.isEmpty())
            {
                DeviceJs engine;
                engine.setResource(r);
                engine.setItem(item);
                engine.setZclAttribute(attr);
                engine.setZclFrame(zclFrame);
                engine.setApsIndication(ind);

                if (engine.evaluate(expr) == JsEvalResult::Ok)
                {
                    const auto res = engine.result();
                    DBG_Printf(DBG_INFO, "expression: %s --> %s\n", qPrintable(expr), qPrintable(res.toString()));
                    // item->setValue(res, ResourceItem::SourceDevice);
                    result = true;
                }
                else
                {
                    DBG_Printf(DBG_INFO, "failed to evaluate expression for %s/%s: %s, err: %s\n", qPrintable(r->item(RAttrUniqueId)->toString()), item->descriptor().suffix, qPrintable(expr), qPrintable(engine.errorString()));
                }
            }
            break;
        }
    }

    return result;
}

/*! A generic function to parse ZCL values from read/report commands.
    The item->parseParameters() is expected to be an object (given in the device description file).

    {"fn": "zcl", "ep": endpoint, "cl": clusterId, "at": attributeId, "mf": manufacturerCode, "eval": expression}

    - endpoint, 0xff means any endpoint
    - clusterId: string hex value
    - attributeId: string hex value or array of string hex values
    - manufacturerCode: (optional) string hex value, defaults to "0x0000" for non manufacturer specific commands
    - expression: Javascript expression to transform the raw value

    Example: { "parse": {"fn": "zcl", "ep:" 1, "cl": "0x0402", "at": "0x0000", "eval": "Attr.val + R.item('config/offset').val" } }
 */
bool parseZclAttribute(Resource *r, ResourceItem *item, const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame)
{
    bool result = false;

    if (zclFrame.commandId() != deCONZ::ZclReadAttributesResponseId && zclFrame.commandId() != deCONZ::ZclReportAttributesId)
    {
        return result;
    }

    if (!item->parseFunction()) // init on first call
    {
        Q_ASSERT(item->parseParameters().size() == 1);
        if (item->parseParameters().size() != 1)
        {
            return result;
        }

        ZclParam param = getZclParam(item->parseParameters().front().toMap());

        if (!param.valid)
        {
            return result;
        }

        if (param.endpoint == AutoEndpoint)
        {
            // hack to get endpoint. todo find better solution
            auto ls = r->item(RAttrUniqueId)->toString().split('-', QString::SkipEmptyParts);
            if (ls.size() >= 2)
            {
                bool ok = false;
                uint ep = ls[1].toUInt(&ok, 10);
                if (ok && ep < BroadcastEndpoint)
                {
                    param.endpoint = ep;
                }
            }
        }

        item->setParseFunction(parseZclAttribute);
        item->setZclProperties(param.clusterId, param.attributes, param.endpoint);
    }

    if (ind.clusterId() != item->clusterId() || zclFrame.payload().isEmpty())
    {
        return result;
    }

    if (item->endpoint() < BroadcastEndpoint && item->endpoint() != ind.srcEndpoint())
    {
        return result;
    }

    QDataStream stream(zclFrame.payload());
    stream.setByteOrder(QDataStream::LittleEndian);

    while (!stream.atEnd())
    {
        quint16 attrId;
        quint8 status;
        quint8 dataType;

        stream >> attrId;

        if (zclFrame.commandId() == deCONZ::ZclReadAttributesResponseId)
        {
            stream >> status;
            if (status != deCONZ::ZclSuccessStatus)
            {
                continue;
            }
        }

        stream >> dataType;
        deCONZ::ZclAttribute attr(attrId, dataType, QLatin1String(""), deCONZ::ZclReadWrite, true);

        if (!attr.readFromStream(stream))
        {
            break;
        }

        if (std::find(item->attributes().begin(), item->attributes().end(), attrId) != item->attributes().end())
        {
            const auto expr = item->parseParameters().front().toMap()["eval"].toString();

            if (!expr.isEmpty())
            {
                DeviceJs engine;
                engine.setResource(r);
                engine.setItem(item);
                engine.setZclAttribute(attr);
                engine.setZclFrame(zclFrame);
                engine.setApsIndication(ind);

                if (engine.evaluate(expr) == JsEvalResult::Ok)
                {
                    const auto res = engine.result();
                    DBG_Printf(DBG_INFO, "expression: %s --> %s\n", qPrintable(expr), qPrintable(res.toString()));
                    // item->setValue(res, ResourceItem::SourceDevice);
                    result = true;
                }
                else
                {
                    DBG_Printf(DBG_INFO, "failed to evaluate expression for %s/%s: %s, err: %s\n", qPrintable(r->item(RAttrUniqueId)->toString()), item->descriptor().suffix, qPrintable(expr), qPrintable(engine.errorString()));
                }
            }
        }
    }

    return result;
}

/*! Handle manufacturer specific Xiaomi ZCL attribute report commands to basic cluster.
 */
deCONZ::ZclAttribute parseXiaomiZclTag(const quint8 rtag, const deCONZ::ZclFrame &zclFrame)
{
    deCONZ::ZclAttribute result;

    quint16 attrId = 0;
    quint8 dataType = 0;
    quint8 length = 0;

    QDataStream stream(zclFrame.payload());
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.setFloatingPointPrecision(QDataStream::SinglePrecision);

    while (attrId == 0 && !stream.atEnd())
    {
        quint16 a;
        stream >> a;
        stream >> dataType;

        if (dataType == deCONZ::ZclCharacterString || dataType == deCONZ::ZclOctedString)
        {
            stream >> length;
        }

        if (a == 0xff01 && dataType == deCONZ::ZclCharacterString)
        {
            attrId = a;
        }
        else if (a == 0xff02 && dataType == 0x4c /*deCONZ::ZclStruct*/)
        {
//            attrId = a;
        }
        else if (a == 0x00f7 && dataType == deCONZ::ZclOctedString)
        {
            attrId = a;
        }

        if (dataType == deCONZ::ZclCharacterString && attrId != 0xff01)
        {
            for (; length > 0; length--) // skip string attribute
            {
                quint8 dummy;
                stream >> dummy;
            }
        }
    }

    if (stream.atEnd() || attrId == 0)
    {
        return result;
    }

    while (!stream.atEnd())
    {
        quint8 tag = 0;

        if (attrId == 0xff01 || attrId == 0x00f7)
        {
            stream >> tag;
        }

        stream >> dataType;

        deCONZ::ZclAttribute atmp(tag, dataType, QLatin1String(""), deCONZ::ZclRead, true);

        if (!atmp.readFromStream(stream))
        {
            return result;
        }

        if (tag == rtag)
        {
            result = atmp;
            break;
        }
    }

    return result;
}

/*! A generic function to read ZCL attributes.
    The item->readParameters() is expected to contain 5 elements (given in the device description file).

    ["readGenericAttribute/4", endpoint, clusterId, attributeId, manufacturerCode]

    - endpoint, 0xff means any endpoint
    - clusterId: string hex value
    - attributeId: string hex value
    - manufacturerCode: must be set to 0x0000 for non manufacturer specific commands

    Example: { "read": ["readGenericAttribute/4", 1, "0x0402", "0x0000", "0x110b"] }
 */
bool readGenericAttribute4(const Resource *r, const ResourceItem *item, deCONZ::ApsController *apsCtrl, DA_ReadResult *result)
{
    Q_ASSERT(result);
    *result = {};

    Q_ASSERT(item->readParameters().size() == 5);
    if (item->readParameters().size() != 5)
    {
        return false;
    }

    const auto *extAddr = r->item(RAttrExtAddress);
    const auto *nwkAddr = r->item(RAttrNwkAddress);

    if (!extAddr || !nwkAddr)
    {
        return false;
    }

    bool ok;
    const auto endpoint = item->readParameters().at(1).toString().toUInt(&ok, 0);
    const auto clusterId = ok ? item->readParameters().at(2).toString().toUInt(&ok, 0) : 0;
    const auto attributeId = ok ? item->readParameters().at(3).toString().toUInt(&ok, 0) : 0;
    const auto manufacturerCode = ok ? item->readParameters().at(4).toString().toUInt(&ok, 0) : 0;

    if (!ok)
    {
        return false;
    }

    DBG_Printf(DBG_INFO, "readGenericAttribute/4, ep: 0x%02X, cl: 0x%04X, attr: 0x%04X, mfcode: 0x%04X\n", endpoint, clusterId, attributeId, manufacturerCode);

    const std::vector<quint16> attributes = { static_cast<quint16>(attributeId) };


//    task.req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
    deCONZ::ApsDataRequest req;
    result->apsReqId = req.id();

    req.setDstEndpoint(endpoint);
    req.setDstAddressMode(deCONZ::ApsExtAddress);
    req.dstAddress().setExt(extAddr->toNumber());
    req.dstAddress().setNwk(nwkAddr->toNumber());
    req.setClusterId(clusterId);
    req.setProfileId(HA_PROFILE_ID);
    req.setSrcEndpoint(0x01); // todo dynamic

    deCONZ::ZclFrame zclFrame;

    zclFrame.setSequenceNumber(zclNextSequenceNumber());
    zclFrame.setCommandId(deCONZ::ZclReadAttributesId);

    result->sequenceNumber = zclFrame.sequenceNumber();

    if (manufacturerCode)
    {
        zclFrame.setFrameControl(deCONZ::ZclFCProfileCommand |
                                      deCONZ::ZclFCManufacturerSpecific |
                                      deCONZ::ZclFCDirectionClientToServer |
                                      deCONZ::ZclFCDisableDefaultResponse);
        zclFrame.setManufacturerCode(manufacturerCode);
    }
    else
    {
        zclFrame.setFrameControl(deCONZ::ZclFCProfileCommand |
                                      deCONZ::ZclFCDirectionClientToServer |
                                      deCONZ::ZclFCDisableDefaultResponse);
    }

    { // payload
        QDataStream stream(&zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        for (uint i = 0; i < attributes.size(); i++)
        {
            stream << attributes[i];
        }
    }

    { // ZCL frame
        QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        zclFrame.writeToStream(stream);
    }

    if (apsCtrl->apsdeDataRequest(req) == deCONZ::Success)
    {
        result->isEnqueued = true;
    }
    else
    {

    }

    return result->isEnqueued;
}

/*! A generic function to read ZCL attributes.
    The item->readParameters() is expected to be an object (given in the device description file).

    { "fn": "zcl", "ep": endpoint, "cl" : clusterId, "at": attributeId, "mf": manufacturerCode }

    - endpoint, 0xff means any endpoint
    - clusterId: string hex value
    - attributeId: string hex value
    - manufacturerCode: (optional) string hex value, defaults to "0x0000" for non manufacturer specific commands

    Example: { "read": {"fn": "zcl", "ep": 1, "cl": "0x0402", "at": "0x0000", "mf": "0x110b"} }
 */
static bool readZclAttribute(const Resource *r, const ResourceItem *item, deCONZ::ApsController *apsCtrl, DA_ReadResult *result)
{
    Q_ASSERT(result);
    *result = { };

    Q_ASSERT(item->readParameters().size() == 1);
    if (item->readParameters().size() != 1)
    {
        return false;
    }

    auto *rTop = r->parentResource() ? r->parentResource() : r;

    const auto *extAddr = rTop->item(RAttrExtAddress);
    const auto *nwkAddr = rTop->item(RAttrNwkAddress);

    if (!extAddr || !nwkAddr)
    {
        return false;
    }

    auto param = getZclParam(item->readParameters().front().toMap());

    if (!param.valid)
    {
        return false;
    }

    if (param.endpoint == AutoEndpoint)
    {
        // hack to get endpoint. todo find better solution
        auto ls = r->item(RAttrUniqueId)->toString().split('-', QString::SkipEmptyParts);
        if (ls.size() >= 2)
        {
            bool ok = false;
            uint ep = ls[1].toUInt(&ok, 10);
            if (ok && ep < BroadcastEndpoint)
            {
                param.endpoint = ep;
            }
        }
    }

    DBG_Printf(DBG_INFO, "readZclAttribute, ep: 0x%02X, cl: 0x%04X, attr: 0x%04X, mfcode: 0x%04X\n",
               param.endpoint, param.clusterId, param.attributes.front(), param.manufacturerCode);


//    task.req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
    deCONZ::ApsDataRequest req;
    result->apsReqId = req.id();

    req.setDstEndpoint(param.endpoint);
    req.setDstAddressMode(deCONZ::ApsExtAddress);
    req.dstAddress().setExt(extAddr->toNumber());
    req.dstAddress().setNwk(nwkAddr->toNumber());
    req.setClusterId(param.clusterId);
    req.setProfileId(HA_PROFILE_ID);
    req.setSrcEndpoint(0x01); // todo dynamic

    deCONZ::ZclFrame zclFrame;

    zclFrame.setSequenceNumber(zclNextSequenceNumber());
    zclFrame.setCommandId(deCONZ::ZclReadAttributesId);

    result->sequenceNumber = zclFrame.sequenceNumber();

    if (param.manufacturerCode)
    {
        zclFrame.setFrameControl(deCONZ::ZclFCProfileCommand |
                                      deCONZ::ZclFCManufacturerSpecific |
                                      deCONZ::ZclFCDirectionClientToServer |
                                      deCONZ::ZclFCDisableDefaultResponse);
        zclFrame.setManufacturerCode(param.manufacturerCode);
    }
    else
    {
        zclFrame.setFrameControl(deCONZ::ZclFCProfileCommand |
                                      deCONZ::ZclFCDirectionClientToServer |
                                      deCONZ::ZclFCDisableDefaultResponse);
    }

    { // payload
        QDataStream stream(&zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        for (const quint16 attrId : param.attributes)
        {
            stream << attrId;
        }
    }

    { // ZCL frame
        QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        zclFrame.writeToStream(stream);
    }

    if (apsCtrl->apsdeDataRequest(req) == deCONZ::Success)
    {
        result->isEnqueued = true;
    }

    return result->isEnqueued;
}

/*! A generic function to write ZCL attributes.
    The item->writeParameters() is expected to contain 7 elements (given in the device description file).

    ["writeGenericAttribute/6", endpoint, clusterId, attributeId, zclDataType, manufacturerCode, expression]

    - endpoint: the destination endpoint
    - clusterId: string hex value
    - attributeId: string hex value
    - zclDataType: string hex value
    - manufacturerCode: must be set to 0x0000 for non manufacturer specific commands
    - expression: to transform the item value

    Example: { "write": ["writeGenericAttribute/6", 1, "0x0020", "0x0000", "0x23", "0x0000", "$raw"] }
 */
bool writeGenericAttribute6(const Resource *r, const ResourceItem *item, deCONZ::ApsController *apsCtrl)
{
    Q_ASSERT(r);
    Q_ASSERT(item);
    Q_ASSERT(apsCtrl);

    bool result = false;
    Q_ASSERT(item->writeParameters().size() == 7);
    if (item->writeParameters().size() != 7)
    {
        return result;
    }

    const auto rParent = r->parentResource() ? r->parentResource() : r;
    const auto *extAddr = rParent->item(RAttrExtAddress);
    const auto *nwkAddr = rParent->item(RAttrNwkAddress);

    if (!extAddr || !nwkAddr)
    {
        return result;
    }

    bool ok;
    const auto endpoint = item->writeParameters().at(1).toString().toUInt(&ok, 0);
    const auto clusterId = ok ? item->writeParameters().at(2).toString().toUInt(&ok, 0) : 0;
    const auto attributeId = ok ? item->writeParameters().at(3).toString().toUInt(&ok, 0) : 0;
    const auto dataType = ok ? item->writeParameters().at(4).toString().toUInt(&ok, 0) : 0;
    const auto manufacturerCode = ok ? item->writeParameters().at(5).toString().toUInt(&ok, 0) : 0;
    auto expr = item->writeParameters().back().toString();

    if (!ok)
    {
        return result;
    }

    DBG_Printf(DBG_INFO, "writeGenericAttribute/6, ep: 0x%02X, cl: 0x%04X, attr: 0x%04X, type: 0x%02X, mfcode: 0x%04X, expr: %s\n", endpoint, clusterId, attributeId, dataType, manufacturerCode, qPrintable(expr));

    const std::vector<quint16> attributes = { static_cast<quint16>(attributeId) };


    deCONZ::ApsDataRequest req;
    deCONZ::ZclFrame zclFrame;

    req.setDstEndpoint(endpoint);
    req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
    req.setDstAddressMode(deCONZ::ApsNwkAddress);
    req.dstAddress().setNwk(nwkAddr->toNumber());
    req.dstAddress().setExt(extAddr->toNumber());
    req.setClusterId(clusterId);
    req.setProfileId(HA_PROFILE_ID);
    req.setSrcEndpoint(1); // TODO

    zclFrame.setSequenceNumber(zclNextSequenceNumber());
    zclFrame.setCommandId(deCONZ::ZclWriteAttributesId);

    if (manufacturerCode)
    {
        zclFrame.setFrameControl(deCONZ::ZclFCProfileCommand |
                                 deCONZ::ZclFCManufacturerSpecific |
                                 deCONZ::ZclFCDirectionClientToServer |
                                 deCONZ::ZclFCDisableDefaultResponse);
        zclFrame.setManufacturerCode(manufacturerCode);
    }
    else
    {
        zclFrame.setFrameControl(deCONZ::ZclFCProfileCommand |
                                 deCONZ::ZclFCDirectionClientToServer |
                                 deCONZ::ZclFCDisableDefaultResponse);
    }

    { // payload
        deCONZ::ZclAttribute attribute(attributeId, dataType, QLatin1String(""), deCONZ::ZclReadWrite, true);

        if (expr == QLatin1String("$raw"))
        {
            attribute.setValue(item->toVariant());
        }
        else if (expr.contains(QLatin1String("$raw")) && dataType < deCONZ::ZclOctedString) // numeric data type
        {
            if ((dataType >= deCONZ::Zcl8BitData && dataType <= deCONZ::Zcl64BitUint)
                    || dataType == deCONZ::Zcl8BitEnum || dataType == deCONZ::Zcl16BitEnum)
            {
                expr.replace("$raw", QString::number(item->toNumber()));
            }
            else if (dataType >= deCONZ::Zcl8BitInt && dataType <= deCONZ::Zcl64BitInt)
            {
                expr.replace("$raw", QString::number(item->toNumber()));
            }
            else if (dataType >= deCONZ::ZclSemiFloat && dataType <= deCONZ::ZclDoubleFloat)
            {
                Q_ASSERT(0); // TODO implement
            }
            else
            {
                return result;
            }

// TODO rewrite for DeviceJs
//            QJSEngine engine;

//            const auto res = engine.evaluate(expr);

//            if (!res.isError())
//            {
//                DBG_Printf(DBG_INFO, "expression: %s = %.0f\n", qPrintable(expr), res.toNumber());
//                attribute.setValue(res.toVariant());
//            }
//            else
//            {
//                DBG_Printf(DBG_INFO, "failed to evaluate expression: %s, err: %d\n", qPrintable(expr), res.errorType());
//                return result;
//            }

            return result;
        }

        QDataStream stream(&zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        stream << attribute.id();
        stream << attribute.dataType();

        if (!attribute.writeToStream(stream))
        {
            return result;
        }
    }

    { // ZCL frame
        QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        zclFrame.writeToStream(stream);
    }

    result = apsCtrl->apsdeDataRequest(req) == deCONZ::Success;

    return result;
}

ParseFunction_t DA_GetParseFunction(const std::vector<QVariant> &params)
{
    ParseFunction_t result = nullptr;

    const std::array<ParseFunction, 2> functions =
    {
        ParseFunction("parseGenericAttribute/4", 4, parseGenericAttribute4),
        ParseFunction("zcl", 1, parseZclAttribute)
    };

    QString fnName;

    if (params.size() == 1 && params.front().type() == QVariant::Map)
    {
        const auto params1 = params.front().toMap();
        if (params1.contains("fn"))
        {
            fnName = params1["fn"].toString();
        }
    }
    else if (params.size() >= 1)
    {
        fnName = params.at(0).toString();
    }

    for (const auto &pf : functions)
    {
        if (pf.name == fnName)
        {
            result = pf.fn;
            break;
        }
    }

    return result;
}

ReadFunction_t DA_GetReadFunction(const std::vector<QVariant> &params)
{
    ReadFunction_t result = nullptr;

    const std::array<ReadFunction, 2> functions =
    {
        ReadFunction("readGenericAttribute/4", 4, readGenericAttribute4),
        ReadFunction("zcl", 1, readZclAttribute)
    };

    QString fnName;

    if (params.size() == 1 && params.front().type() == QVariant::Map)
    {
        const auto params1 = params.front().toMap();
        if (params1.contains("fn"))
        {
            fnName = params1["fn"].toString();
        }
    }
    else if (params.size() >= 1)
    {
        fnName = params.at(0).toString();
    }

    for (const auto &pf : functions)
    {
        if (pf.name == fnName)
        {
            result = pf.fn;
            break;
        }
    }

    return result;
}

WriteFunction_t DA_GetWriteFunction(const std::vector<QVariant> &params)
{
    WriteFunction_t result = nullptr;

    const std::array<WriteFunction, 1> functions =
    {
        WriteFunction("writeGenericAttribute/6", 6, writeGenericAttribute6)
    };

    if (params.size() >= 1)
    {
        const auto fnName = params.at(0).toString();
        for (const auto &pf : functions)
        {
            if (pf.name == fnName)
            {
                result = pf.fn;
                break;
            }
        }
    }

    return result;
}
