/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QJSEngine>
#include "device.h"
#include "device_access_fn.h"

quint8 zclNextSequenceNumber(); // todo defined in de_web_plugin_private.h

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
    Q_UNUSED(r)

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
        const auto endpoint = item->parseParameters().at(1).toString().toUInt(&ok, 0);
        const auto clusterId = ok ? item->parseParameters().at(2).toString().toUInt(&ok, 0) : 0;
        const auto attributeId = ok ? item->parseParameters().at(3).toString().toUInt(&ok, 0) : 0;

        if (!ok)
        {
            return result;
        }

        item->setParseFunction(parseGenericAttribute4);
        item->setZclProperties(clusterId, attributeId, endpoint);
    }

    if (ind.clusterId() != item->clusterId() || zclFrame.payload().isEmpty())
    {
        return result;
    }

    if (item->endpoint() < 0xff && item->endpoint() != ind.srcEndpoint())
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

        if (attrId == item->attributeId())
        {
            auto expr = item->parseParameters().back().toString();

            if (expr.contains(QLatin1String("$old")) && dataType < deCONZ::ZclOctedString)
            {
                expr.replace("$old", QString::number(item->toNumber()));
            }

            if (expr == QLatin1String("$raw"))
            {
                if (item->setValue(attr.toVariant(), ResourceItem::SourceDevice))
                {
                    result = true;
                }

                DBG_Printf(DBG_INFO, "RD cluster: 0x%04X / %04X --> %s\n", ind.clusterId(), attrId, qPrintable(attr.toString()));
            }
            else if (expr.contains(QLatin1String("$raw")) && dataType < deCONZ::ZclOctedString) // numeric data type
            {
                if ((dataType >= deCONZ::Zcl8BitData && dataType <= deCONZ::Zcl64BitUint)
                     || dataType == deCONZ::Zcl8BitEnum || dataType == deCONZ::Zcl16BitEnum)
                {
                    expr.replace("$raw", QString::number(attr.numericValue().u64));
                }
                else if (dataType >= deCONZ::Zcl8BitInt && dataType <= deCONZ::Zcl64BitInt)
                {
                    expr.replace("$raw", QString::number(attr.numericValue().s64));
                }
                else if (dataType >= deCONZ::ZclSemiFloat && dataType <= deCONZ::ZclDoubleFloat)
                {
                    expr.replace("$raw", QString::number(attr.numericValue().real));
                }
                else
                {
                    return result;
                }

                QJSEngine engine;

                const auto res = engine.evaluate(expr);

                if (!res.isError())
                {
                    DBG_Printf(DBG_INFO, "expression: %s = %.0f\n", qPrintable(expr), res.toNumber());
                    item->setValue(res.toVariant(), ResourceItem::SourceDevice);
                    result = true;
                }
                else
                {
                    DBG_Printf(DBG_INFO, "failed to evaluate expression: %s, err: %d\n", qPrintable(expr), res.errorType());
                }
            }
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
bool readGenericAttribute4(const Resource *r, const ResourceItem *item, deCONZ::ApsController *apsCtrl)
{
    bool result = false;
    Q_ASSERT(item->readParameters().size() == 5);
    if (item->readParameters().size() != 5)
    {
        return result;
    }

    const auto *extAddr = r->item(RAttrExtAddress);
    const auto *nwkAddr = r->item(RAttrNwkAddress);

    if (!extAddr || !nwkAddr)
    {
        return result;
    }

    bool ok;
    const auto endpoint = item->readParameters().at(1).toString().toUInt(&ok, 0);
    const auto clusterId = ok ? item->readParameters().at(2).toString().toUInt(&ok, 0) : 0;
    const auto attributeId = ok ? item->readParameters().at(3).toString().toUInt(&ok, 0) : 0;
    const auto manufacturerCode = ok ? item->readParameters().at(4).toString().toUInt(&ok, 0) : 0;

    if (!ok)
    {
        return result;
    }

    DBG_Printf(DBG_INFO, "readGenericAttribute/4, ep: 0x%02X, cl: 0x%04X, attr: 0x%04X, mfcode: 0x%04X\n", endpoint, clusterId, attributeId, manufacturerCode);

    const std::vector<quint16> attributes = { static_cast<quint16>(attributeId) };


//    task.req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
    deCONZ::ApsDataRequest req;
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
        result = true;
    }
    else
    {

    }

    return result;
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

            QJSEngine engine;

            const auto res = engine.evaluate(expr);

            if (!res.isError())
            {
                DBG_Printf(DBG_INFO, "expression: %s = %.0f\n", qPrintable(expr), res.toNumber());
                attribute.setValue(res.toVariant());
            }
            else
            {
                DBG_Printf(DBG_INFO, "failed to evaluate expression: %s, err: %d\n", qPrintable(expr), res.errorType());
                return result;
            }
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

const std::vector<ParseFunction> parseFunctions = {
    ParseFunction("parseGenericAttribute/4", 4, parseGenericAttribute4)
};

const std::vector<ReadFunction> readFunctions = {
    ReadFunction("readGenericAttribute/4", 4, readGenericAttribute4)
};

const std::vector<WriteFunction> writeFunctions = {
    WriteFunction("writeGenericAttribute/6", 6, writeGenericAttribute6)
};

ParseFunction_t getParseFunction(const std::vector<ParseFunction> &functions, const std::vector<QVariant> &params)
{
    ParseFunction_t result = nullptr;

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

ReadFunction_t getReadFunction(const std::vector<ReadFunction> &functions, const std::vector<QVariant> &params)
{
    ReadFunction_t result = nullptr;

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

WriteFunction_t getWriteFunction(const std::vector<WriteFunction> &functions, const std::vector<QVariant> &params)
{
    WriteFunction_t result = nullptr;

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
