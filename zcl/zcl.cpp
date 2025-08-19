/*
 * Copyright (c) 2021-2025 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QIODevice>
#include <deconz/aps_controller.h>
#include <deconz/dbg_trace.h>
#include <deconz/zcl.h>
#include "zcl.h"


struct ZclDataType
{
    quint8 dataType;
    char type; // 'A' analog 'D' discrete
    quint8 size;
};

static ZclDataType _zclDataTypes[] = {
    { deCONZ::Zcl8BitUint,    'A',  1 },
    { deCONZ::Zcl16BitUint,   'A',  2 },
    { deCONZ::Zcl24BitUint,   'A',  3 },
    { deCONZ::Zcl32BitUint,   'A',  4 },
    { deCONZ::Zcl40BitUint,   'A',  5 },
    { deCONZ::Zcl48BitUint,   'A',  6 },
    { deCONZ::Zcl56BitUint,   'A',  7 },
    { deCONZ::Zcl64BitUint,   'A',  8 },
    { deCONZ::Zcl8BitInt,     'A',  1 },
    { deCONZ::Zcl16BitInt,    'A',  2 },
    { deCONZ::Zcl24BitInt,    'A',  3 },
    { deCONZ::Zcl32BitInt,    'A',  4 },
    { deCONZ::Zcl40BitInt,    'A',  5 },
    { deCONZ::Zcl48BitInt,    'A',  6 },
    { deCONZ::Zcl56BitInt,    'A',  7 },
    { deCONZ::Zcl64BitInt,    'A',  8 },
    { deCONZ::ZclSingleFloat, 'A',  4 },
    { deCONZ::ZclSemiFloat,   'A',  2 },
    { deCONZ::ZclDoubleFloat, 'A',  8 },
    { deCONZ::ZclTimeOfDay,   'A',  4 },
    { deCONZ::ZclDate,        'A',  4 },
    { deCONZ::ZclUtcTime,     'A',  4 },
    { deCONZ::ZclNoData,       0,   0 }
};


ZCL_Result ZCL_ReadAttributes(const ZCL_Param &param, quint64 extAddress, quint16 nwkAddress, deCONZ::ApsController *apsCtrl)
{
    ZCL_Result result{};

//    task.req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
    deCONZ::ApsDataRequest req;
    result.apsReqId = req.id();

    req.setDstEndpoint(param.endpoint);
    req.setDstAddressMode(deCONZ::ApsExtAddress);
    req.dstAddress().setExt(extAddress);
    req.dstAddress().setNwk(nwkAddress);
    req.setClusterId(param.clusterId);
    req.setProfileId(HA_PROFILE_ID);
    req.setSrcEndpoint(0x01); // todo dynamic

    uint fcDirection = deCONZ::ZclFCDirectionClientToServer;
    deCONZ::ZclFrame zclFrame;

    zclFrame.setSequenceNumber(zclNextSequenceNumber());
    zclFrame.setCommandId(deCONZ::ZclReadAttributesId);

    DBG_Printf(DBG_ZCL, "ZCL read attr 0x%016llX, ep: 0x%02X, cl: 0x%04X, attr: 0x%04X, mfcode: 0x%04X, aps.id: %u, zcl.seq: %u\n",
               extAddress, param.endpoint, param.clusterId, param.attributes.front(), param.manufacturerCode, req.id(), zclFrame.sequenceNumber());

    result.sequenceNumber = zclFrame.sequenceNumber();

    if (param.clusterId == 0x0019) // assume device only has client OTA cluster
    {
        fcDirection = deCONZ::ZclFCDirectionServerToClient;
    }

    if (param.manufacturerCode)
    {
        zclFrame.setFrameControl(deCONZ::ZclFCProfileCommand |
                                      deCONZ::ZclFCManufacturerSpecific |
                                      fcDirection |
                                      deCONZ::ZclFCDisableDefaultResponse);
        zclFrame.setManufacturerCode(param.manufacturerCode);
    }
    else
    {
        zclFrame.setFrameControl(deCONZ::ZclFCProfileCommand |
                                      fcDirection |
                                      deCONZ::ZclFCDisableDefaultResponse);
    }

    { // payload
        QDataStream stream(&zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        for (size_t i = 0; i < param.attributeCount; i++)
        {
            stream << param.attributes[i];
        }
    }

    { // ZCL frame
        QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        zclFrame.writeToStream(stream);
    }

    if (apsCtrl->apsdeDataRequest(req) == deCONZ::Success)
    {
        result.isEnqueued = true;
    }

    return result;
}

ZCL_Result ZCL_WriteAttribute(const ZCL_Param &param, quint64 extAddress, quint16 nwkAddress, deCONZ::ApsController *apsCtrl, deCONZ::ZclAttribute *attribute)
{
    ZCL_Result result{};

    DBG_Printf(DBG_INFO, "writeZclAttribute, ep: 0x%02X, cl: 0x%04X, attr: 0x%04X, type: 0x%02X, mfcode: 0x%04X\n", param.endpoint, param.clusterId, param.attributes.front(), attribute->dataType(), param.manufacturerCode);

    deCONZ::ApsDataRequest req;
    deCONZ::ZclFrame zclFrame;

    req.setDstEndpoint(param.endpoint);
    req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
    req.setDstAddressMode(deCONZ::ApsNwkAddress);
    req.dstAddress().setExt(extAddress);
    req.dstAddress().setNwk(nwkAddress);
    req.setClusterId(param.clusterId);
    req.setProfileId(HA_PROFILE_ID);
    req.setSrcEndpoint(1); // TODO

    zclFrame.setSequenceNumber(zclNextSequenceNumber());
    zclFrame.setCommandId(deCONZ::ZclWriteAttributesId);

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

        stream << attribute->id();
        stream << attribute->dataType();

        if (!attribute->writeToStream(stream))
        {
            return result;
        }
    }

    { // ZCL frame
        QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        zclFrame.writeToStream(stream);
    }

    if (apsCtrl->apsdeDataRequest(req) == deCONZ::Success)
    {
        result.isEnqueued = true;
    }

    return result;
}


ZCL_Result ZCL_SendCommand(const ZCL_Param &param, quint64 extAddress, quint16 nwkAddress, deCONZ::ApsController *apsCtrl, std::vector<uint8_t> *payload)
{
    ZCL_Result result{};

    deCONZ::ApsDataRequest req;
    result.apsReqId = req.id();

    req.setDstEndpoint(param.endpoint);
    req.setDstAddressMode(deCONZ::ApsExtAddress);
    req.dstAddress().setExt(extAddress);
    req.dstAddress().setNwk(nwkAddress);
    req.setClusterId(param.clusterId);
    req.setProfileId(HA_PROFILE_ID);
    req.setSrcEndpoint(0x01); // todo dynamic

    uint fcDirection = deCONZ::ZclFCDirectionClientToServer;
    deCONZ::ZclFrame zclFrame;

    zclFrame.setSequenceNumber(zclNextSequenceNumber());
    zclFrame.setCommandId(param.commandId);

    DBG_Printf(DBG_ZCL, "ZCL cmd attr 0x%016llX, ep: 0x%02X, cl: 0x%04X, cmd: 0x%02X, mfcode: 0x%04X, aps.id: %u, zcl.seq: %u\n",
               extAddress, param.endpoint, param.clusterId, param.commandId, param.manufacturerCode, req.id(), zclFrame.sequenceNumber());

    result.sequenceNumber = zclFrame.sequenceNumber();

    if (param.manufacturerCode)
    {
        zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                                      deCONZ::ZclFCManufacturerSpecific |
                                      fcDirection |
                                      deCONZ::ZclFCDisableDefaultResponse);
        zclFrame.setManufacturerCode(param.manufacturerCode);
    }
    else
    {
        zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                                      fcDirection |
                                      deCONZ::ZclFCDisableDefaultResponse);
    }

    if (param.hasFrameControl)
    {
        zclFrame.setFrameControl(param.frameControl);
    }

    { // payload
        QDataStream stream(&zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        for (auto byte : *payload)
        {
            stream << (quint8) byte;
        }
    }

    { // ZCL frame
        QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        zclFrame.writeToStream(stream);
    }

    if (apsCtrl->apsdeDataRequest(req) == deCONZ::Success)
    {
        result.isEnqueued = true;
    }

    return result;
}


ZCL_Result ZCL_ReadReportConfiguration(const ZCL_ReadReportConfigurationParam &param, deCONZ::ApsController *apsCtrl)
{
    ZCL_Result result{};

    deCONZ::ApsDataRequest req;
    result.apsReqId = req.id();

    req.setDstEndpoint(param.endpoint);
    req.setDstAddressMode(deCONZ::ApsExtAddress);
    req.dstAddress().setExt(param.extAddress);
    req.dstAddress().setNwk(param.nwkAddress);
    req.setClusterId(param.clusterId);
    req.setProfileId(HA_PROFILE_ID);
    req.setSrcEndpoint(0x01); // todo dynamic

    deCONZ::ZclFrame zclFrame;

    zclFrame.setSequenceNumber(zclNextSequenceNumber());
    zclFrame.setCommandId(deCONZ::ZclReadReportingConfigId);

    DBG_Printf(DBG_ZCL, "ZCL read report config, ep: 0x%02X, cl: 0x%04X, mfcode: 0x%04X, aps.id: %u, zcl.seq: %u\n",
               param.endpoint, param.clusterId, param.manufacturerCode, req.id(), zclFrame.sequenceNumber());

    result.sequenceNumber = zclFrame.sequenceNumber();

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

        for (const auto &record : param.records)
        {
            stream << record.direction;
            stream << record.attributeId;
        }
    }

    { // ZCL frame
        QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        zclFrame.writeToStream(stream);
    }

    if (apsCtrl->apsdeDataRequest(req) == deCONZ::Success)
    {
        result.isEnqueued = true;
    }

    return result;
}

const ZclDataType *ZCL_GetDataType(deCONZ::ZclDataTypeId type)
{
    const auto *dt = &_zclDataTypes[0];
    while (dt->dataType != deCONZ::ZclNoData)
    {
        if (dt->dataType == type)
        {
            break;
        }
        dt++;
    }

    return dt; // deCONZ::ZclNoData if not found
}

bool ZCL_IsDataTypeAnalog(deCONZ::ZclDataTypeId type)
{
    const auto *dt = ZCL_GetDataType(type);
    return dt->type == 'A';
}

size_t ZCL_DataTypeSize(deCONZ::ZclDataTypeId type)
{
    const auto *dt = ZCL_GetDataType(type);
    return dt->size;
}

ZCL_ReadReportConfigurationRsp ZCL_ParseReadReportConfigurationRsp(const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame)
{
    ZCL_ReadReportConfigurationRsp result{};

    result.sequenceNumber = zclFrame.sequenceNumber();
    result.endpoint = ind.srcEndpoint();
    result.clusterId = ind.clusterId();
    result.manufacturerCode = zclFrame.manufacturerCode();

    QDataStream stream(zclFrame.payload());
    stream.setByteOrder(QDataStream::LittleEndian);

    while (stream.status() == QDataStream::Ok && result.recordCount < ZCL_ReadReportConfigurationRsp::MaxRecords)
    {
        auto &record = result.records[result.recordCount];

        stream >> record.status;
        stream >> record.direction;
        stream >> record.attributeId;

        if (stream.status() != QDataStream::Ok)
        {
            break;
        }

        if (record.status != deCONZ::ZclSuccessStatus)
        {
            // If the status field is not set to SUCCESS, all fields except the direction and attribute identifier fields SHALL be omitted.
            result.recordCount++;
            continue;
        }

        stream >> record.dataType;
        stream >> record.minInterval;
        stream >> record.maxInterval;

        record.reportableChange = 0;

        if (ZCL_IsDataTypeAnalog(deCONZ::ZclDataTypeId(record.dataType)))
        {
            const auto size = ZCL_DataTypeSize(deCONZ::ZclDataTypeId(record.dataType));
            Q_ASSERT(size <= 8);
            if (size > 8)
            {
                break; // unsupported
            }

            for (size_t i = 0; i < size; i++)
            {
                quint8 tmp;
                stream >> tmp;
                record.reportableChange |= quint64(tmp) << (i * 8);
            }
        }

        if (stream.status() == QDataStream::Ok)
        {
            result.recordCount++;
        }
    }

    return result;
}

ZCL_Result ZCL_ConfigureReporting(const ZCL_ConfigureReportingParam &param, deCONZ::ApsController *apsCtrl)
{
    ZCL_Result result{};

    deCONZ::ApsDataRequest req;
    result.apsReqId = req.id();

    req.setDstEndpoint(param.endpoint);
    req.setDstAddressMode(deCONZ::ApsExtAddress);
    req.dstAddress().setExt(param.extAddress);
    req.dstAddress().setNwk(param.nwkAddress);
    req.setClusterId(param.clusterId);
    req.setProfileId(HA_PROFILE_ID);
    req.setSrcEndpoint(0x01); // todo dynamic

    deCONZ::ZclFrame zclFrame;

    zclFrame.setSequenceNumber(zclNextSequenceNumber());
    zclFrame.setCommandId(deCONZ::ZclConfigureReportingId);

    DBG_Printf(DBG_ZCL, "ZCL configure reporting ep: 0x%02X, cl: 0x%04X, mfcode: 0x%04X, aps.id: %u, zcl.seq: %u\n",
               param.endpoint, param.clusterId, param.manufacturerCode, req.id(), zclFrame.sequenceNumber());

    result.sequenceNumber = zclFrame.sequenceNumber();

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

        for (const auto &record : param.records)
        {
            stream << record.direction;
            stream << record.attributeId;
            stream << record.dataType;
            stream << record.minInterval;
            stream << record.maxInterval;

            if (ZCL_IsDataTypeAnalog(deCONZ::ZclDataTypeId(record.dataType)))
            {
                const auto size = ZCL_DataTypeSize(deCONZ::ZclDataTypeId(record.dataType));
                Q_ASSERT(size <= 8);
                if (size > 8)
                {
                    return result; // unsupported
                }

                auto reportableChange = record.reportableChange;
                for (size_t i = 0; i < size; i++)
                {
                    stream << static_cast<quint8>(reportableChange & 0xff);
                    reportableChange >>= 8;
                }
            }

//            stream << record.timeout; // TODO?
        }
    }

    { // ZCL frame
        QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        zclFrame.writeToStream(stream);
    }

    if (apsCtrl->apsdeDataRequest(req) == deCONZ::Success)
    {
        result.isEnqueued = true;
    }

    return result;
}
