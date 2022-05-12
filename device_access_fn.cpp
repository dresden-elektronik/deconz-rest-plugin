/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QTimeZone>
#include "air_quality.h"
#include "device_access_fn.h"
#include "device_descriptions.h"
#include "device_js/device_js.h"
#include "ias_zone.h"
#include "resource.h"
#include "zcl/zcl.h"
#include "de_web_plugin_private.h"

#define TIME_CLUSTER_ID     0x000A

#define TIME_ATTRID_TIME                    0x0000
#define TIME_ATTRID_LOCAL_TIME              0x0007
#define TIME_ATTRID_LAST_SET_TIME           0x0008

/*
    Documentation for manufacturer specific Tuya cluster (0xEF00)

        https://developer.tuya.com/en/docs/iot-device-dev/tuya-zigbee-universal-docking-access-standard?id=K9ik6zvofpzql

    Tuya ZCL insights

        https://github.com/TuyaInc/tuya_zigbee_sdk/blob/master/silicon_labs_zigbee/include/zigbee_attr.h

    Basic cluster (0x0000)
    -------------

    0x0001 Application version:: 0b 01 00 0001 = 1.0.1 ie 0x41 for 1.0.1

    0x0004 Manufacturer name:  XXX…XXX (16 bytes in length, consisting of an 8-byte prefix and an 8-byte PID)
                               0-7 bytes: _ TZE600_
                               8-16 bytes: PID (created and provided by the product manager in the platform or self-service)

    Tuya cluster (0xEF00)
    ---------------------

    https://developer.tuya.com/en/docs/iot-device-dev/tuya-zigbee-universal-docking-access-standard?id=K9ik6zvofpzql

    Zigbee generic docking is suitable for scenarios where the Zigbee standard protocol is not supported or not very suitable.

    ZDP Simple Descriptor Device Id (0x0051)

    Frame control for outgoing commands:

        deCONZ::ZclFCClusterCommand
        deCONZ::ZclFCDirectionClientToServer
        deCONZ::ZclFCDisableDefaultResponse

    DP data format
    --------------

    DPID  U8        Datapoint serial number
    Type  U8        Datatype in value

          Name  Id   Length
          ------------------------------
          raw     0x00
          bool    0x01
          value   0x02
          string  0x03
          enum    0x04
          bitmap  0x05

    Length U16      Length of Value
    Value  1/2/4/N  The value as big endian


    ZCL Payload of comamnds
    -----------------------

    Example MoesGo switch TY_DATA_REPORT

    00 4c        sequence number
    02           DPID
    02           Type: Value
    00 04        Length: 4
    00 00 00 15

*/
#define TUYA_CLUSTER_ID 0xEF00

enum TuyaCommandId : unsigned char
{
    TY_DATA_REQUEST              = 0x00,
    TY_DATA_RESPONSE             = 0x01,
    TY_DATA_REPORT               = 0x02,
    TY_DATA_QUERY                = 0x03,
    TUYA_MCU_VERSION_REQ         = 0x10,
    TUYA_MCU_VERSION_RSP         = 0x11,
    TUYA_MCU_OTA_NOTIFY          = 0x12,
    TUYA_MCU_OTA_BLOCK_DATA_REQ  = 0x13,
    TUYA_MCU_OTA_BLOCK_DATA_RSP  = 0x14,
    TUYA_MCU_OTA_RESULT          = 0x15,
    TUYA_MCU_SYNC_TIME           = 0x24
};

enum TuyaDataType : unsigned char
{
    TuyaDataTypeRaw              = 0x00,
    TuyaDataTypeBool             = 0x01,
    TuyaDataTypeValue            = 0x02,
    TuyaDataTypeString           = 0x03,
    TuyaDataTypeEnum             = 0x04,
    TuyaDataTypeBitmap           = 0x05
};

enum DA_Constants
{
    BroadcastEndpoint = 255, //! Accept incoming commands from any endpoint.
    AutoEndpoint = 0 //! Use src/dst endpoint of the related Resource (uniqueid).
};

struct ParseFunction
{
    ParseFunction(const QString &_name, const int _arity, ParseFunction_t _fn) :
        name(_name),
        arity(_arity),
        fn(_fn)
    { }
    QString name;
    int arity = 0; // number of parameters given by the device description file
    ParseFunction_t fn = nullptr;
};

struct ReadFunction
{
    ReadFunction(const QString &_name, const int _arity, ReadFunction_t _fn) :
        name(_name),
        arity(_arity),
        fn(_fn)
    { }
    QString name;
    int arity = 0; // number of parameters given by the device description file
    ReadFunction_t fn = nullptr;
};

struct WriteFunction
{
    WriteFunction(const QString &_name, const int _arity, WriteFunction_t _fn) :
        name(_name),
        arity(_arity),
        fn(_fn)
    { }
    QString name;
    int arity = 0; // number of parameters given by the device description file
    WriteFunction_t fn = nullptr;
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

    if (var.isNull())
    {
        return 0;
    }

    const auto val = var.toString().toUInt(ok, 0);
    *ok = *ok && val <= max;

    return *ok ? val : 0;
}

/*! Extracts common ZCL parameters from an object.
 */
static ZCL_Param getZclParam(const QVariantMap &param)
{
    ZCL_Param result{};

    if (!param.contains(QLatin1String("cl")))
    {
        return result;
    }

    bool ok = true;

    result.endpoint = param.contains("ep") ? variantToUint(param["ep"], UINT8_MAX, &ok) : quint8(AutoEndpoint);
    result.clusterId = ok ? variantToUint(param["cl"], UINT16_MAX, &ok) : 0;
    result.manufacturerCode = ok && param.contains("mf") ? variantToUint(param["mf"], UINT16_MAX, &ok) : 0;

    if (param.contains(QLatin1String("cmd"))) // optional
    {
        result.commandId = variantToUint(param["cmd"], UINT8_MAX, &ok);
        result.hasCommandId = ok ? 1 : 0;
    }
    else
    {
        result.hasCommandId = 0;
    }

    result.attributeCount = 0;
    const auto attr = param[QLatin1String("at")]; // optional

    if (!ok)
    { }
    else if (attr.type() == QVariant::String)
    {
        result.attributes[result.attributeCount] = variantToUint(attr, UINT16_MAX, &ok);
        result.attributeCount = 1;
    }
    else if (attr.type() == QVariant::List)
    {
        const auto arr = attr.toList();
        for (const auto &at : arr)
        {
            if (result.attributeCount == ZCL_Param::MaxAttributes)
            {
                break;
            }

            if (ok && at.type() == QVariant::String)
            {
                result.attributes[result.attributeCount] = variantToUint(at, UINT16_MAX, &ok);
                result.attributeCount++;
            }
        }

        ok = result.attributeCount == size_t(arr.size());
    }
    else if (param["eval"].toString().contains("Attr")) // guard against missing "at"
    {
        ok = false;
    }

    result.valid = ok;

    return result;
}

quint8 resolveAutoEndpoint(const Resource *r)
{
    quint8 result = AutoEndpoint;

    // hack to get endpoint. todo find better solution
    const auto ls = r->item(RAttrUniqueId)->toString().split('-', SKIP_EMPTY_PARTS);
    if (ls.size() >= 2)
    {
        bool ok = false;
        uint ep = ls[1].toUInt(&ok, 16);
        if (ok && ep < BroadcastEndpoint)
        {
            result = ep;
        }
    }

    return result;
}

/*! Evaluates an items Javascript expression for a received attribute.
 */
bool evalZclAttribute(Resource *r, ResourceItem *item, const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame, const deCONZ::ZclAttribute &attr, const QVariant &parseParameters)
{
    bool ok = false;
    const auto &zclParam = item->zclParam();

    for (size_t i = 0; i < zclParam.attributeCount; i++)
    {
        if (zclParam.attributes[i] == attr.id())
        {
            ok = true;
            break;
        }
    }

    if (!ok)
    {
        return false;
    }

    const auto expr = parseParameters.toMap()["eval"].toString();

    if (!expr.isEmpty())
    {
        DeviceJs &engine = *DeviceJs::instance();
        engine.reset();
        engine.setResource(r);
        engine.setItem(item);
        engine.setZclAttribute(attr);
        engine.setZclFrame(zclFrame);
        engine.setApsIndication(ind);

        if (engine.evaluate(expr) == JsEvalResult::Ok)
        {
            const auto res = engine.result();
            if (res.isValid())
            {
                DBG_Printf(DBG_DDF, "%s/%s expression: %s --> %s\n", r->item(RAttrUniqueId)->toCString(), item->descriptor().suffix, qPrintable(expr), qPrintable(res.toString()));

                // item->setValue(res, ResourceItem::SourceDevice);
                return true;
            }
        }
        else
        {
            DBG_Printf(DBG_DDF, "failed to evaluate expression for %s/%s: %s, err: %s\n", r->item(RAttrUniqueId)->toCString(), item->descriptor().suffix, qPrintable(expr), qPrintable(engine.errorString()));
        }
    }
    return false;
}

/*! Evaluates an items Javascript expression for a received ZCL frame.
 */
bool evalZclFrame(Resource *r, ResourceItem *item, const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame, const QVariant &parseParameters)
{
    const auto expr = parseParameters.toMap()["eval"].toString();

    if (!expr.isEmpty())
    {
        DeviceJs &engine = *DeviceJs::instance();
        engine.reset();
        engine.setResource(r);
        engine.setItem(item);
        engine.setZclFrame(zclFrame);
        engine.setApsIndication(ind);

        if (engine.evaluate(expr) == JsEvalResult::Ok)
        {
            const auto res = engine.result();
            if (res.isValid())
            {
                DBG_Printf(DBG_INFO, "expression: %s --> %s\n", qPrintable(expr), qPrintable(res.toString()));
                return true;
            }
        }
        else
        {
            DBG_Printf(DBG_INFO, "failed to evaluate expression for %s/%s: %s, err: %s\n", qPrintable(r->item(RAttrUniqueId)->toString()), item->descriptor().suffix, qPrintable(expr), qPrintable(engine.errorString()));
        }
    }
    return false;
}

/*! A general purpose function to map number values of a source item to a string which is stored in \p item .

    The item->parseParameters() is expected to be an object (given in the device description file).
    {"fn": "numtostring", "srcitem": suffix, "op": operator, "to": array}
    - srcitem: the suffix of the source item which holds the numeric value
    - op: (lt | le | eq | gt | ge) the operator used to match the 'to' array
    - to: [number, string, [number, string], ...] an sorted array to map 'number -> string' with the given operator

    Example: { "parse": {"fn": "numtostr", "srcitem": "state/airqualityppb", "op": "le", "to": [65, "good", 65535, "bad"] }
 */
bool parseNumericToString(Resource *r, ResourceItem *item, const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame, const QVariant &parseParameters)
{
    Q_UNUSED(ind)
    Q_UNUSED(zclFrame)
    bool result = false;

    ResourceItem *srcItem = nullptr;
    const auto map = parseParameters.toMap();

    enum Op { OpNone, OpLessThan, OpLessEqual, OpEqual, OpGreaterThan, OpGreaterEqual };
    Op op = OpNone;

    if (!item->parseFunction()) // init on first call
    {
        if (item->descriptor().type != DataTypeString)
        {
            return result;
        }

        if (!map.contains(QLatin1String("to")) || !map.contains(QLatin1String("op")) || !map.contains(QLatin1String("srcitem")))
        {
            return result;
        }

        item->setParseFunction(parseNumericToString);
    }

    ResourceItemDescriptor rid;
    if (!getResourceItemDescriptor(map["srcitem"].toString(), rid))
    {
        return result;
    }

    srcItem = r->item(rid.suffix);
    if (!srcItem)
    {
        return result;
    }

    if (!(srcItem->needPushChange() || srcItem->needPushSet()))
    {
        return result; // only update if needed
    }

    {
        const auto opString = map[QLatin1String("op")].toString();

        if      (opString == QLatin1String("le")) { op = OpLessEqual; }
        else if (opString == QLatin1String("lt")) { op = OpLessThan; }
        else if (opString == QLatin1String("eq")) { op = OpEqual; }
        else if (opString == QLatin1String("ge")) { op = OpGreaterEqual; }
        else if (opString == QLatin1String("gt")) { op = OpGreaterThan; }
        else
        {
            return result;
        }
    }

    const qint64 num = srcItem->toNumber();
    const auto to = map["to"].toList();

    if (to.size() & 1)
    {
        return result; // array size must be even
    }

    auto i = std::find_if(to.cbegin(), to.cend(), [num, op](const QVariant &var)
    {
        if (var.type() == QVariant::Double)
        {
            if (op == OpLessEqual)    { return num <= var.toInt(); }
            if (op == OpLessThan)     { return num < var.toInt();  }
            if (op == OpEqual)        { return num == var.toInt(); }
            if (op == OpGreaterEqual) { return num >= var.toInt(); }
            if (op == OpGreaterThan)  { return num > var.toInt();  }
        }
        return false;
    });

    if (i != to.cend())
    {
        i++; // point next element (string)

        if (i != to.cend() && i->type() == QVariant::String)
        {
            const QString str = i->toString();
            if (!str.isEmpty())
            {
                item->setValue(str);
                item->setLastZclReport(srcItem->lastZclReport()); // Treat as report
                result = true;
            }
        }
    }

    return result;
}

/*! A generic function to parse ZCL values from read/report commands.
    The item->parseParameters() is expected to be an object (given in the device description file).

    {"fn": "zcl", "ep": endpoint, "cl": clusterId, "at": attributeId, "mf": manufacturerCode, "eval": expression}

    - endpoint: (optional) 255 means any endpoint, 0 means auto selected from the related resource, defaults to 0
    - clusterId: string hex value
    - attributeId: string hex value or array of string hex values
    - manufacturerCode: (optional) string hex value, defaults to "0x0000" for non manufacturer specific commands
    - expression: Javascript expression to transform the raw value

    Example: { "parse": {"fn": "zcl", "ep:" 1, "cl": "0x0402", "at": "0x0000", "eval": "Attr.val + R.item('config/offset').val" } }
 */
bool parseZclAttribute(Resource *r, ResourceItem *item, const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame, const QVariant &parseParameters)
{
    bool result = false;

    if (!item->parseFunction()) // init on first call
    {
        Q_ASSERT(!parseParameters.isNull());
        if (parseParameters.isNull())
        {
            return result;
        }

        ZCL_Param param = getZclParam(parseParameters.toMap());

        Q_ASSERT(param.valid);
        if (!param.valid)
        {
            return result;
        }

        if (param.hasCommandId && param.commandId != zclFrame.commandId())
        {
            return result;
        }
        else if (!param.hasCommandId && zclFrame.commandId() != deCONZ::ZclReadAttributesResponseId && zclFrame.commandId() != deCONZ::ZclReportAttributesId)
        {
            return result;
        }

        if (param.manufacturerCode != zclFrame.manufacturerCode())
        {
            return result;
        }

        if (param.endpoint == AutoEndpoint)
        {
            param.endpoint = resolveAutoEndpoint(r);

            if (param.endpoint == AutoEndpoint)
            {
                return result;
            }
        }

        item->setParseFunction(parseZclAttribute);
        item->setZclProperties(param);
    }

    const auto &zclParam = item->zclParam();

    if (ind.clusterId() != zclParam.clusterId)
    {
        return result;
    }

    if (zclParam.endpoint < BroadcastEndpoint && zclParam.endpoint != ind.srcEndpoint())
    {
        return result;
    }

    if (zclParam.attributeCount == 0) // attributes are optional
    {
        if (evalZclFrame(r, item, ind, zclFrame, parseParameters))
        {
            result = true;
        }
        return result;
    }

    if (zclFrame.payload().isEmpty() && zclParam.attributeCount > 0)
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

        if (evalZclAttribute(r, item, ind, zclFrame, attr, parseParameters))
        {
            if (zclFrame.commandId() == deCONZ::ZclReportAttributesId)
            {
                item->setLastZclReport(deCONZ::steadyTimeRef().ref);
            }
            result = true;
        }
    }

    return result;
}

/*! A generic function to parse Tuya private cluster values from response/report commands.
    The item->parseParameters() is expected to be an object (given in the device description file).

    {"fn": "tuya", "dpid": datapointId, "eval": expression}

    - datapointId: 1-255 the datapoint identifier (DPID) to extract
    - expression: Javascript expression to transform the raw value

    Example: { "parse": {"fn": "tuya", "dpid:" 1, "eval": "Attr.val + R.item('config/offset').val" } }
 */
bool parseTuyaData(Resource *r, ResourceItem *item, const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame, const QVariant &parseParameters)
{
    bool result = false;

    if (ind.clusterId() != TUYA_CLUSTER_ID || !(zclFrame.commandId() == TY_DATA_REPORT || zclFrame.commandId() ==  TY_DATA_RESPONSE))
    {
        return result;
    }

    if (!item->parseFunction()) // init on first call
    {
        const auto map = parseParameters.toMap();
        if (map.isEmpty())
        {
            return result;
        }

        if (!map.contains(QLatin1String("dpid")) || !map.contains(QLatin1String("eval")))
        {
            return result;
        }

        bool ok = false;
        ZCL_Param param{};
        param.attributes[0] = variantToUint(map.value(QLatin1String("dpid")), 255, &ok);
        if (!ok)
        {
            return result;
        }
        param.valid = 1;
        param.endpoint = ind.srcEndpoint();
        param.clusterId = ind.clusterId();
        param.attributeCount = 1;

        item->setParseFunction(parseTuyaData);
        item->setZclProperties(param);
    }

    quint16 seq;
    quint8 dpid;
    quint8 dataType;
    quint16 dataLength;
    quint8 zclDataType = 0;
    const auto &zclParam = item->zclParam();

    QDataStream stream(zclFrame.payload());
    stream.setByteOrder(QDataStream::BigEndian); // tuya is big endian!

    stream >> seq;

    while (!stream.atEnd()) // a message can contain multiple datapoints
    {
        stream >> dpid;
        stream >> dataType;
        stream >> dataLength;

        if (stream.status() != QDataStream::Ok)
        {
            return result;
        }

        deCONZ::NumericUnion num;
        num.u64 = 0;

        switch (dataType)
        {
        case TuyaDataTypeRaw:
        case TuyaDataTypeString:
            return result; // TODO implement?

        case TuyaDataTypeBool:
        { stream >> num.u8; zclDataType = deCONZ::ZclBoolean; }
            break;

        case TuyaDataTypeEnum:
        { stream >> num.u8; zclDataType = deCONZ::Zcl8BitUint; }
            break;

        case TuyaDataTypeValue: // docs aren't clear, assume signed
        {  stream >> num.s32; zclDataType = deCONZ::Zcl32BitInt; }
            break;

        case TuyaDataTypeBitmap:
        {
            switch (dataLength)
            {
            case 1: { stream >> num.u8;  zclDataType = deCONZ::Zcl8BitUint; } break;
            case 2: { stream >> num.u16; zclDataType = deCONZ::Zcl16BitUint; } break;
            case 4: { stream >> num.u32; zclDataType = deCONZ::Zcl32BitUint; } break;
            }
        }
            break;

        default:
            return result; // unkown datatype
        }

        if (dpid == zclParam.attributes[0])
        {
            // map datapoint into ZCL attribute
            deCONZ::ZclAttribute attr(dpid, zclDataType, QLatin1String(""), deCONZ::ZclReadWrite, true);

            if (zclDataType == deCONZ::Zcl32BitInt)
            {
                attr.setValue(qint64(num.s32));
            }
            else
            {
                attr.setValue(quint64(num.u32));
            }

            if (evalZclAttribute(r, item, ind, zclFrame, attr, parseParameters))
            {
                item->setLastZclReport(deCONZ::steadyTimeRef().ref);
                result = true;
            }
        }

        const char *rt = zclFrame.commandId() == TY_DATA_REPORT ? "REPORT" : "RESPONSE";

        DBG_Printf(DBG_INFO, "TY_DATA_%s: seq %u, dpid: 0x%02X, type: 0x%02X, length: %u, val: %d\n",
                   rt, seq, dpid, dataType, dataLength, num.s32);
    }

    return result;
}

/*! A generic function to trigger Tuya device reporting all datapoints.
    Important: This function should be attached to only one item!
    The item->readParameters() is expected to be an object (given in the device description file).

    { "fn": "tuya"}

    Example: { "read": {"fn": "tuya"} }
 */
static DA_ReadResult readTuyaAllData(const Resource *r, const ResourceItem *item, deCONZ::ApsController *apsCtrl, const QVariant &readParameters)
{
    Q_UNUSED(item)
    Q_UNUSED(readParameters);

    DA_ReadResult result{};

    // Workaround: dont't query too quickly, reports will only be send a few seconds after receiving the query command.
    // The device report timer resets on each received query.
    // Not the ideal solution since this is global across all devices but should do the trick for now.
    static deCONZ::SteadyTimeRef lastReadGlobal{};

    auto now = deCONZ::steadyTimeRef();
    if (now - lastReadGlobal < deCONZ::TimeSeconds{15})
    {
        return result;
    }

    lastReadGlobal = now;

    auto *rTop = r->parentResource() ? r->parentResource() : r;

    const auto *extAddr = rTop->item(RAttrExtAddress);
    const auto *nwkAddr = rTop->item(RAttrNwkAddress);

    if (!extAddr || !nwkAddr)
    {
        return result;
    }

    deCONZ::ApsDataRequest req;
    deCONZ::ZclFrame zclFrame;

    req.setDstEndpoint(1); // TODO is this always 1? if not search simple descriptor for Tuya cluster
    req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
    req.setDstAddressMode(deCONZ::ApsNwkAddress);
    req.dstAddress().setNwk(nwkAddr->toNumber());
    req.dstAddress().setExt(extAddr->toNumber());
    req.setClusterId(TUYA_CLUSTER_ID);
    req.setProfileId(HA_PROFILE_ID);
    req.setSrcEndpoint(1); // TODO

    zclFrame.setSequenceNumber(zclNextSequenceNumber());
    zclFrame.setCommandId(TY_DATA_QUERY);

    zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCDisableDefaultResponse);

    // no payload

    { // ZCL frame
        QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        zclFrame.writeToStream(stream);
    }

    result.isEnqueued = apsCtrl->apsdeDataRequest(req) == deCONZ::Success;
    result.apsReqId = req.id();
    result.sequenceNumber = zclFrame.sequenceNumber();

    return result;
}

/*! A generic function to write Tuya data.
    The \p writeParameters is expected to contain one object (given in the device description file).

    { "fn": "tuya", "dpid": datapointId, "dt": dataType, "eval": expression }

    - datapointId: number
    - dataType: string hex value

          bool           0x10
          s32 value      0x2b
          enum           0x30
          8-bit bitmap   0x18
          16-bit bitmap  0x19
          32-bit bitmap  0x1b

    - expression: to transform the item value

    Example: "write": {"fn":"tuya", "dpid": 1,  "dt": "0x10", "eval": "Item.val == 1"}
 */
bool writeTuyaData(const Resource *r, const ResourceItem *item, deCONZ::ApsController *apsCtrl, const QVariant &writeParameters)
{
    Q_ASSERT(r);
    Q_ASSERT(item);
    Q_ASSERT(apsCtrl);

    bool result = false;
    const auto rParent = r->parentResource() ? r->parentResource() : r;
    const auto *extAddr = rParent->item(RAttrExtAddress);
    const auto *nwkAddr = rParent->item(RAttrNwkAddress);

    if (!extAddr || !nwkAddr)
    {
        return result;
    }

    const auto map = writeParameters.toMap();

    if (!map.contains(QLatin1String("dpid")) || !map.contains(QLatin1String("dt")) || !map.contains(QLatin1String("eval")))
    {
        return result;
    }

    bool ok = false;

    const auto dpid = variantToUint(map.value(QLatin1String("dpid")), 255, &ok);
    if (!ok)
    {
        return result;
    }
    const auto dataType = variantToUint(map.value("dt"), UINT8_MAX, &ok);
    switch (dataType)
    {
    case deCONZ::ZclBoolean:
    case deCONZ::Zcl32BitInt:
    case deCONZ::Zcl8BitEnum:
    case deCONZ::Zcl8BitBitMap:
    case deCONZ::Zcl16BitBitMap:
    case deCONZ::Zcl32BitBitMap:
        break;

    default:
        return result; // unsupported datatype
    }

    const auto expr = map.value("eval").toString();

    if (!ok || expr.isEmpty())
    {
        return result;
    }

    DBG_Printf(DBG_INFO, "writeTuyaData, dpid: 0x%02X, type: 0x%02X, expr: %s\n",
               dpid & 0xFF, dataType & 0xFF, qPrintable(expr));

    deCONZ::ApsDataRequest req;
    deCONZ::ZclFrame zclFrame;

    req.setDstEndpoint(1); // TODO is this always 1? if not search simple descriptor for Tuya cluster
    req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
    req.setDstAddressMode(deCONZ::ApsNwkAddress);
    req.dstAddress().setNwk(nwkAddr->toNumber());
    req.dstAddress().setExt(extAddr->toNumber());
    req.setClusterId(TUYA_CLUSTER_ID);
    req.setProfileId(HA_PROFILE_ID);
    req.setSrcEndpoint(1); // TODO

    zclFrame.setSequenceNumber(zclNextSequenceNumber());
    zclFrame.setCommandId(TY_DATA_REQUEST);

    zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCDisableDefaultResponse);


    { // payload
        QVariant value;
        DeviceJs &engine = *DeviceJs::instance();
        engine.reset();
        engine.setResource(r);
        engine.setItem(item);

        if (engine.evaluate(expr) == JsEvalResult::Ok)
        {
            value = engine.result();
            DBG_Printf(DBG_INFO, "Tuya write expression: %s --> %s\n", qPrintable(expr), qPrintable(value.toString()));
        }
        else
        {
            DBG_Printf(DBG_INFO, "failed to evaluate Tuya write expression for %s/%s: %s, err: %s\n", qPrintable(r->item(RAttrUniqueId)->toString()), item->descriptor().suffix, qPrintable(expr), qPrintable(engine.errorString()));
            return result;
        }

        if (!value.isValid())
        {
            return result;
        }

        QDataStream stream(&zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::BigEndian); // Tuya is big endian

        stream << quint16(req.id()); // use as sequence number
        stream << quint8(dpid);

        switch (dataType)
        {
        case deCONZ::ZclBoolean:
        {
            stream << quint8(TuyaDataTypeBool);
            stream << quint16(1); // length
            stream << quint8(value.toUInt());
        }
            break;

        case deCONZ::Zcl32BitInt:
        {
            stream << quint8(TuyaDataTypeValue);
            stream << quint16(4); // length
            stream << qint32(value.toInt());
        }
            break;

        case deCONZ::Zcl8BitEnum:
        {
            stream << quint8(TuyaDataTypeEnum);
            stream << quint16(1); // length
            stream << quint8(value.toUInt());
        }
            break;

        case deCONZ::Zcl8BitBitMap:
        {
            stream << quint8(TuyaDataTypeBitmap);
            stream << quint16(1); // length
            stream << quint8(value.toUInt());
        }
            break;

        case deCONZ::Zcl16BitBitMap:
        {
            stream << quint8(TuyaDataTypeBitmap);
            stream << quint16(2); // length
            stream << quint16(value.toUInt());
        }
            break;

        case deCONZ::Zcl32BitBitMap:
        {
            stream << quint8(TuyaDataTypeBitmap);
            stream << quint16(4); // length
            stream << quint32(value.toUInt());
        }
            break;

        default: // TODO unsupported datatype
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

/*! Extracts manufacturer specific Xiaomi ZCL attribute from report commands to basic cluster.

    \param zclFrame - Contains the special report with attribute 0xff01, 0xff02 or 0x00f7.
    \param rtag - The tag or struct index of the attribute to return.
    \returns Parsed attribute, use attr.id() != 0xffff to check for valid result.
 */
deCONZ::ZclAttribute parseXiaomiZclTag(const quint8 rtag, const deCONZ::ZclFrame &zclFrame)
{
    deCONZ::ZclAttribute result;

    quint16 attrId = 0;
    quint8 dataType = 0;
    quint8 length = 0;
    quint16 structElements = 0;

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
            attrId = a;
            stream >> structElements;
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

    quint8 tag = 0;

    while (!stream.atEnd())
    {
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

        if (structElements > 0)
        {
            tag++; // running struct index
        }
    }

    return result;
}

/*! A generic function to parse ZCL values from Xiaomi special report commands.
    The item->parseParameters() is expected to be an object (given in the device description file).

    {"fn": "xiaomi:special", "ep": endpoint, "at": attributeId, "idx": index, "eval": expression}

    - endpoint: (optional), 0xff means any endpoint (default: 0xff)
    - attributeId: string hex value of 0xff01, 0xff02 or 0x00f7.
    - index: string hex value representing the tag or index in the structure
    - expression: Javascript expression to transform the raw value (as alternative "script" can be used to reference a external JS script file)

    Example: { "parse": {"fn": "xiaomi:special", "at": "0xff01", "idx": "0x01", "eval": "Item.val = Attr.val" } }
 */
bool parseXiaomiSpecial(Resource *r, ResourceItem *item, const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame, const QVariant &parseParameters)
{
    bool result = false;

    if (zclFrame.commandId() != deCONZ::ZclReportAttributesId)
    {
        return result;
    }

    if (ind.clusterId() != 0x0000 && ind.clusterId() != 0xfcc0) // must be basic or lumi specific cluster
    {
        return result;
    }

    if (!item->parseFunction()) // init on first call
    {
        Q_ASSERT(!parseParameters.isNull());
        if (parseParameters.isNull())
        {
            return result;
        }

        const auto map = parseParameters.toMap();

        bool ok = true;
        ZCL_Param param;

        param.endpoint = BroadcastEndpoint; // default
        param.clusterId = 0x0000;
        
        if (ind.clusterId() == 0xfcc0)
        {
            param.clusterId = 0xfcc0;
            param.manufacturerCode = 0x115f;
        }

        if (map.contains(QLatin1String("ep")))
        {
            param.endpoint = variantToUint(map["ep"], UINT8_MAX, &ok);
        }
        const auto at = ok ? variantToUint(map["at"], UINT16_MAX, &ok) : 0;
        const auto idx = ok ? variantToUint(map["idx"], UINT16_MAX, &ok) : 0;

        DBG_Assert(at == 0xff01 || at == 0xff02 || at == 0x00f7);
        if (!ok)
        {
            return result;
        }

        param.attributeCount = 2;
        param.attributes[0] = at;
        // keep tag/idx as second "attribute id"
        param.attributes[1] = idx;

        if (param.endpoint == AutoEndpoint)
        {
            param.endpoint = resolveAutoEndpoint(r);

            if (param.endpoint == AutoEndpoint)
            {
                return result;
            }
        }

        item->setParseFunction(parseXiaomiSpecial);
        item->setZclProperties(param);
    }

    const auto &zclParam = item->zclParam();

    if (!(ind.clusterId() == 0x0000 || ind.clusterId() == 0xfcc0) || zclFrame.payload().isEmpty())
    {
        return result;
    }

    if (zclParam.endpoint < BroadcastEndpoint && zclParam.endpoint != ind.srcEndpoint())
    {
        return result;
    }

    Q_ASSERT(zclParam.attributeCount == 2); // attribute id + tag/idx
    const auto attr = parseXiaomiZclTag(zclParam.attributes[1], zclFrame);

    if (evalZclAttribute(r, item, ind, zclFrame, attr, parseParameters))
    {
        result = true;
    }

    return result;
}

/*! A function to parse IAS Zone status change notifications or read/report commands for IAS Zone status of the IAS Zone cluster.
    The item->parseParameters() is expected to be an object (given in the device description file).

    {"fn": "ias:zonestatus", "mask": expression}

    - mask (optional): The bitmask to be applied for Alarm1 and Alarm2 of the IAS zone status value as list of strings

    Example: { "parse": {"fn": "ias:zonestatus", "mask": "alarm1,alarm2" } }
 */
bool parseIasZoneNotificationAndStatus(Resource *r, ResourceItem *item, const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame, const QVariant &parseParameters)
{
    bool result = false;

    if (ind.clusterId() != IAS_ZONE_CLUSTER_ID)
    {
        return result;
    }

    if (ind.srcEndpoint() != resolveAutoEndpoint(r))
    {
        return result;
    }

    if (zclFrame.isClusterCommand())  // is IAS Zone status notification?
    {
        if (zclFrame.commandId() != CMD_STATUS_CHANGE_NOTIFICATION)
        {
            return result;
        }

    }
    else if (zclFrame.commandId() != deCONZ::ZclReadAttributesResponseId && zclFrame.commandId() != deCONZ::ZclReportAttributesId) // is read or report?
    {
        return result;
    }

    if (!item->parseFunction()) // init on first call
    {
        item->setParseFunction(parseIasZoneNotificationAndStatus);
    }

    QDataStream stream(zclFrame.payload());
    stream.setByteOrder(QDataStream::LittleEndian);

    quint16 zoneStatus = UINT16_MAX;

    while (!stream.atEnd())
    {
        if (zclFrame.isClusterCommand())
        {
            quint8 extendedStatus;
            quint8 zoneId;
            quint16 delay;

            stream >> zoneStatus;
            stream >> extendedStatus; // reserved, set to 0
            stream >> zoneId;
            stream >> delay;

            DBG_Assert(stream.status() == QDataStream::Ok);
        }
        else
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

            if (attr.id() == 0x0002)
            {
                zoneStatus = attr.numericValue().u16;
                break;
            }
        }
    }

    if (zoneStatus != UINT16_MAX)
    {
        int mask = 0;
        const char *suffix = item->descriptor().suffix;

        if (suffix == RStateAlarm || suffix == RStateCarbonMonoxide || suffix == RStateFire || suffix == RStateOpen ||
            suffix == RStatePresence || suffix == RStateVibration || suffix == RStateWater)
        {
            const auto map = parseParameters.toMap();

            if (map.contains(QLatin1String("mask")))
            {
                QStringList alarmMask = map["mask"].toString().split(',', QString::SkipEmptyParts);

                if (alarmMask.contains(QLatin1String("alarm1"))) { mask |= STATUS_ALARM1; }
                if (alarmMask.contains(QLatin1String("alarm2"))) { mask |= STATUS_ALARM2; }
            }
        }
        else if (suffix == RStateTampered)
        {
            mask |= STATUS_TAMPER;
        }
        else if (suffix == RStateLowBattery)
        {
            mask |= STATUS_BATTERY;
        }
        else if (suffix == RStateTest)
        {
            mask |= STATUS_TEST;
        }

        item->setValue((zoneStatus & mask) != 0);
        item->setLastZclReport(deCONZ::steadyTimeRef().ref);    // Treat as report
        result = true;
    }

    return result;
}

/*! A function to write current time data to the time server cluster, syncing on-device RTC. Time calculations are borrowed from time.cpp.

    {"fn": "time"}

    - The function does not require any further parameters

    Example: { "write": {"fn": "time"} }
 */
bool writeTimeData(const Resource *r, const ResourceItem *item, deCONZ::ApsController *apsCtrl, const QVariant &writeParameters)
{
    Q_UNUSED(writeParameters);
    Q_UNUSED(item);

    Q_ASSERT(r);
    Q_ASSERT(apsCtrl);

    bool result = false;
    const auto rParent = r->parentResource() ? r->parentResource() : r;
    const auto *extAddr = rParent->item(RAttrExtAddress);
    const auto *nwkAddr = rParent->item(RAttrNwkAddress);

    if (!extAddr || !nwkAddr)
    {
        return result;
    }

    quint8 endpoint = resolveAutoEndpoint(r);

    if (endpoint == AutoEndpoint)
    {
        return result;
    }
    
    DBG_Printf(DBG_DDF, "%s correcting time drift...\n", r->item(RAttrUniqueId)->toCString());

    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QDateTime yearStart(QDate(QDate::currentDate().year(), 1, 1), QTime(0, 0), Qt::UTC);
    const QTimeZone timeZone(QTimeZone::systemTimeZoneId());

    QDateTime epoch;

    quint32 time_now = 0xFFFFFFFF;              // id 0x0000 Time
    qint8 time_status = 0x0D;                   // id 0x0001 TimeStatus Master|MasterZoneDst|Superseding
    qint32 time_zone = 0xFFFFFFFF;              // id 0x0002 TimeZone
    quint32 time_dst_start = 0xFFFFFFFF;        // id 0x0003 DstStart
    quint32 time_dst_end = 0xFFFFFFFF;          // id 0x0004 DstEnd
    qint32 time_dst_shift = 0xFFFFFFFF;         // id 0x0005 DstShift
    quint32 time_valid_until_time = 0xFFFFFFFF; // id 0x0009 ValidUntilTime

    epoch = QDateTime(QDate(2000, 1, 1), QTime(0, 0), Qt::UTC);;
    time_now = epoch.secsTo(now);
    time_zone = timeZone.offsetFromUtc(yearStart);

    if (timeZone.hasTransitions())
    {
        const QTimeZone::OffsetData dstStartOffsetData = timeZone.nextTransition(yearStart);
        const QTimeZone::OffsetData dstEndOffsetData = timeZone.nextTransition(dstStartOffsetData.atUtc);
        time_dst_start = epoch.secsTo(dstStartOffsetData.atUtc);
        time_dst_end = epoch.secsTo(dstEndOffsetData.atUtc);
        time_dst_shift = dstStartOffsetData.daylightTimeOffset;
    }

    time_valid_until_time = time_now + (3600 * 24);

    deCONZ::ApsDataRequest req;
    deCONZ::ZclFrame zclFrame;

    req.setDstEndpoint(endpoint);
    req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
    req.setDstAddressMode(deCONZ::ApsExtAddress);
    req.dstAddress().setNwk(nwkAddr->toNumber());
    req.dstAddress().setExt(extAddr->toNumber());
    req.setClusterId(TIME_CLUSTER_ID);
    req.setProfileId(HA_PROFILE_ID);
    req.setSrcEndpoint(1); // TODO

    zclFrame.setSequenceNumber(zclNextSequenceNumber());
    zclFrame.setCommandId(deCONZ::ZclWriteAttributesId);

    zclFrame.setFrameControl(deCONZ::ZclFCProfileCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCDisableDefaultResponse);

    { // payload
        QDataStream stream(&zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        stream << (quint16) 0x0000; // Time
        stream << (quint8) deCONZ::ZclUtcTime;
        stream << time_now;

        stream << (quint16) 0x0001; // Time Status
        stream << (quint8) deCONZ::Zcl8BitBitMap;
        stream << time_status;

        stream << (quint16) 0x0002; // Time Zone
        stream << (quint8) deCONZ::Zcl32BitInt;
        stream << time_zone;

        stream << (quint16) 0x0003; // Dst Start
        stream << (quint8) deCONZ::Zcl32BitUint;
        stream << time_dst_start;

        stream << (quint16) 0x0004; // Dst End
        stream << (quint8) deCONZ::Zcl32BitUint;
        stream << time_dst_end;

        stream << (quint16) 0x0005; // Dst Shift
        stream << (quint8) deCONZ::Zcl32BitInt;
        stream << time_dst_shift;

        stream << (quint16) 0x0009; // Valid Until Time
        stream << (quint8) deCONZ::ZclUtcTime;
        stream << time_valid_until_time;
    }

    { // ZCL frame
        QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        zclFrame.writeToStream(stream);
    }

    result = apsCtrl->apsdeDataRequest(req) == deCONZ::Success;

    return result;
}

/*! A specialized function to parse TUYA_MCU_SYNC_TIME and sync the time on the device.

    {"fn": "tuyatime"}

    - The function should only be contained once in the DDF file for the device
    - The function does not require any further parameters

    Example: { "parse": {"fn": "tuyatime"} }
 */
bool parseTuyaTime(Resource *r, ResourceItem *item, const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame, const QVariant &parseParameters)
{
    Q_UNUSED(parseParameters);
    Q_UNUSED(r);
    Q_UNUSED(item);

    if (zclFrame.isDefaultResponse() || zclFrame.commandId() != TUYA_MCU_SYNC_TIME)
    {
        return false;
    }

    DBG_Printf(DBG_INFO, "Tuya Time sync request received\n");
    
    QDataStream stream(zclFrame.payload());
    stream.setByteOrder(QDataStream::BigEndian);

    quint16 sequenceNumber;
    stream >> sequenceNumber;

    // "Timestamps" contained in the ZCL Payload will be always zero.
    // The Tuya protocol does not send the local time of the device
    // quint32 standardTimeStamp;
    // stream >> standardTimeStamp;
    // quint32 localTimeStamp;
    // stream >> localTimeStamp;

    QByteArray data;
    QDataStream stream2(&data, QIODevice::WriteOnly);
    stream2.setByteOrder(QDataStream::BigEndian);

    stream2 << sequenceNumber;
    // Add UTC time
    const quint32 timeNow = QDateTime::currentSecsSinceEpoch();
    stream2 << timeNow;
    
    // Add local time
    const quint32 timeLocalTime = QDateTime::currentDateTime().toSecsSinceEpoch();
    stream2 << timeLocalTime;

    DeRestPluginPrivate *app = DeRestPluginPrivate::instance();
    app->sendTuyaCommand(ind, TUYA_MCU_SYNC_TIME, data);

    return true;

}

/*! A specialized function to parse time (utc), local and last set time from read/report commands of the time cluster and auto-sync time if needed.
    The item->parseParameters() is expected to be an object (given in the device description file).

    {"fn": "time"}

    - The function does not require any further parameters

    Example: { "parse": {"fn": "time"} }
 */
bool parseAndSyncTime(Resource *r, ResourceItem *item, const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame, const QVariant &parseParameters)
{
    Q_UNUSED(parseParameters);
    bool result = false;

    if (ind.clusterId() != TIME_CLUSTER_ID)
    {
        return result;
    }

    if (ind.srcEndpoint() != resolveAutoEndpoint(r))
    {
        return result;
    }

    if (zclFrame.commandId() != deCONZ::ZclReadAttributesResponseId && zclFrame.commandId() != deCONZ::ZclReportAttributesId) // is read or report?
    {
        return result;
    }

    if (!item->parseFunction()) // init on first call
    {
        item->setParseFunction(parseAndSyncTime);
    }

    QDataStream stream(zclFrame.payload());
    stream.setByteOrder(QDataStream::LittleEndian);

    const QDateTime epoch = QDateTime(QDate(2000, 1, 1), QTime(0, 0), Qt::UTC);
    const char *suffix = item->descriptor().suffix;

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

        switch (attrId) {
        case TIME_ATTRID_TIME:
        {
            if (suffix == RStateUtc)
            {
                QDateTime time = epoch.addSecs(attr.numericValue().u32);
                const qint32 drift = QDateTime::currentDateTimeUtc().secsTo(time);

                if (item->toVariant().toDateTime().toMSecsSinceEpoch() != time.toMSecsSinceEpoch())
                {
                    item->setValue(time, ResourceItem::SourceDevice);
                }

                if (drift < -10 || drift > 10)
                {
                    DBG_Printf(DBG_DDF, "%s/%s : time drift detected, %d seconds to now\n", r->item(RAttrUniqueId)->toCString(), suffix, drift);

                    auto *apsCtrl = deCONZ::ApsController::instance();

                    if (writeTimeData(r, item, apsCtrl, item->toVariant())) // last parameter's content is irrelevant
                    {
                        // Check if drift got eliminated
                        const auto &ddfItem = DDF_GetItem(item);
                        const auto readFunction = DA_GetReadFunction(ddfItem.readParameters);
                        auto res = readFunction(r, item, apsCtrl, ddfItem.readParameters);

                        if (res.isEnqueued)
                        {
                            DBG_Printf(DBG_DDF, "%s time verification queued...\n", r->item(RAttrUniqueId)->toCString());
                        }
                    }
                }
                else
                {
                    DBG_Printf(DBG_DDF, "%s/%s : NO considerable time drift detected, %d seconds to now\n", r->item(RAttrUniqueId)->toCString(), suffix, drift);
                }

                item->setLastZclReport(deCONZ::steadyTimeRef().ref);    // Treat as report

                result = true;
            }
        }
            break;

        case TIME_ATTRID_LOCAL_TIME:
        {
            if (suffix == RStateLocaltime)
            {
                QDateTime time = epoch.addSecs(attr.numericValue().u32 - QDateTime::currentDateTime().offsetFromUtc());

                if (item->toVariant().toDateTime().toMSecsSinceEpoch() != time.toMSecsSinceEpoch())
                {
                    item->setValue(time, ResourceItem::SourceDevice);
                }

                item->setLastZclReport(deCONZ::steadyTimeRef().ref);     // Treat as report

                result = true;
            }
        }
            break;

        case TIME_ATTRID_LAST_SET_TIME:
        {
            if (suffix == RStateLastSet)
            {
                QDateTime time = epoch.addSecs(attr.numericValue().u32);

                if (item->toVariant().toDateTime().toMSecsSinceEpoch() != time.toMSecsSinceEpoch())
                {
                    item->setValue(time, ResourceItem::SourceDevice);
                }

                item->setLastZclReport(deCONZ::steadyTimeRef().ref);     // Treat as report

                result = true;
            }
        }
            break;
        }
    }

    return result;
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
static DA_ReadResult readZclAttribute(const Resource *r, const ResourceItem *item, deCONZ::ApsController *apsCtrl, const QVariant &readParameters)
{
    Q_UNUSED(item)

    DA_ReadResult result;

    Q_ASSERT(!readParameters.isNull());
    if (readParameters.isNull())
    {
        return result;
    }

    auto *rTop = r->parentResource() ? r->parentResource() : r;

    const auto *extAddr = rTop->item(RAttrExtAddress);
    const auto *nwkAddr = rTop->item(RAttrNwkAddress);

    if (!extAddr || !nwkAddr)
    {
        return result;
    }

    auto param = getZclParam(readParameters.toMap());

    if (!param.valid)
    {
        return result;
    }

    if (param.endpoint == AutoEndpoint)
    {
        param.endpoint = resolveAutoEndpoint(r);

        if (param.endpoint == AutoEndpoint)
        {
            return result;
        }
    }

    const auto zclResult = ZCL_ReadAttributes(param, extAddr->toNumber(), nwkAddr->toNumber(), apsCtrl);

    result.isEnqueued = zclResult.isEnqueued;
    result.apsReqId = zclResult.apsReqId;
    result.sequenceNumber = zclResult.sequenceNumber;

    return result;
}

/*! A generic function to write ZCL attributes.
    The \p writeParameters is expected to contain one object (given in the device description file).

    { "fn": "zcl", "ep": endpoint, "cl": clusterId, "at": attributeId, "dt": zclDataType, "mf": manufacturerCode, "eval": expression }

    - endpoint: (optional) the destination endpoint
    - clusterId: string hex value
    - attributeId: string hex value
    - zclDataType: string hex value
    - manufacturerCode: must be set to 0x0000 for non manufacturer specific commands
    - expression: to transform the item value

    Example: "write": {"cl": "0x0000", "at": "0xff0d",  "dt": "0x20", "mf": "0x11F5", "eval": "Item.val"}
 */
bool writeZclAttribute(const Resource *r, const ResourceItem *item, deCONZ::ApsController *apsCtrl, const QVariant &writeParameters)
{
    Q_ASSERT(r);
    Q_ASSERT(item);
    Q_ASSERT(apsCtrl);

    bool result = false;
    const auto rParent = r->parentResource() ? r->parentResource() : r;
    const auto *extAddr = rParent->item(RAttrExtAddress);
    const auto *nwkAddr = rParent->item(RAttrNwkAddress);

    if (!extAddr || !nwkAddr)
    {
        return result;
    }

    const auto map = writeParameters.toMap();
    ZCL_Param param = getZclParam(map);

    if (!param.valid)
    {
        return result;
    }

    if (param.attributeCount != 1)
    {
        return result;
    }

    if (param.endpoint == AutoEndpoint)
    {
        param.endpoint = resolveAutoEndpoint(r);

        if (param.endpoint == AutoEndpoint)
        {
            return result;
        }
    }

    if (!map.contains("dt") || !map.contains("eval"))
    {
        return result;
    }

    bool ok;
    const auto dataType = variantToUint(map.value("dt"), UINT8_MAX, &ok);
    const auto expr = map.value("eval").toString();

    if (!ok || expr.isEmpty())
    {
        return result;
    }

    DBG_Printf(DBG_INFO, "writeZclAttribute, ep: 0x%02X, cl: 0x%04X, attr: 0x%04X, type: 0x%02X, mfcode: 0x%04X, expr: %s\n", param.endpoint, param.clusterId, param.attributes.front(), dataType, param.manufacturerCode, qPrintable(expr));

    deCONZ::ApsDataRequest req;
    deCONZ::ZclFrame zclFrame;

    req.setDstEndpoint(param.endpoint);
    req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
    req.setDstAddressMode(deCONZ::ApsNwkAddress);
    req.dstAddress().setNwk(nwkAddr->toNumber());
    req.dstAddress().setExt(extAddr->toNumber());
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
        deCONZ::ZclAttribute attribute(param.attributes[0], dataType, QLatin1String(""), deCONZ::ZclReadWrite, true);

        if (!expr.isEmpty())
        {
            DeviceJs &engine = *DeviceJs::instance();
            engine.reset();
            engine.setResource(r);
            engine.setItem(item);

            if (engine.evaluate(expr) == JsEvalResult::Ok)
            {
                const auto res = engine.result();
                DBG_Printf(DBG_INFO, "expression: %s --> %s\n", qPrintable(expr), qPrintable(res.toString()));
                attribute.setValue(res);
            }
            else
            {
                DBG_Printf(DBG_INFO, "failed to evaluate expression for %s/%s: %s, err: %s\n", qPrintable(r->item(RAttrUniqueId)->toString()), item->descriptor().suffix, qPrintable(expr), qPrintable(engine.errorString()));
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

ParseFunction_t DA_GetParseFunction(const QVariant &params)
{
    ParseFunction_t result = nullptr;

    const std::array<ParseFunction, 7> functions =
    {
        ParseFunction(QLatin1String("zcl"), 1, parseZclAttribute),
        ParseFunction(QLatin1String("xiaomi:special"), 1, parseXiaomiSpecial),
        ParseFunction(QLatin1String("ias:zonestatus"), 1, parseIasZoneNotificationAndStatus),
        ParseFunction(QLatin1String("tuya"), 1, parseTuyaData),
        ParseFunction(QLatin1String("numtostr"), 1, parseNumericToString),
        ParseFunction(QLatin1String("time"), 1, parseAndSyncTime),
        ParseFunction(QLatin1String("tuyatime"), 1, parseTuyaTime)
    };

    QString fnName;

    if (params.type() == QVariant::Map)
    {
        const auto params1 = params.toMap();
        if (params1.isEmpty())
        {  }
        else if (params1.contains(QLatin1String("fn")))
        {
            fnName = params1["fn"].toString();
        }
        else
        {
            fnName = QLatin1String("zcl"); // default
        }
    }

    for (const auto &f : functions)
    {
        if (f.name == fnName)
        {
            result = f.fn;
            break;
        }
    }

    return result;
}

ReadFunction_t DA_GetReadFunction(const QVariant &params)
{
    ReadFunction_t result = nullptr;

    const std::array<ReadFunction, 2> functions =
    {
        ReadFunction(QLatin1String("zcl"), 1, readZclAttribute),
        ReadFunction(QLatin1String("tuya"), 1, readTuyaAllData)
    };

    QString fnName;

    if (params.type() == QVariant::Map)
    {
        const auto params1 = params.toMap();
        if (params1.isEmpty())
        {  }
        else if (params1.contains(QLatin1String("fn")))
        {
            fnName = params1["fn"].toString();
        }
        else
        {
            fnName = QLatin1String("zcl"); // default
        }
    }

    for (const auto &f : functions)
    {
        if (f.name == fnName)
        {
            result = f.fn;
            break;
        }
    }

    return result;
}

WriteFunction_t DA_GetWriteFunction(const QVariant &params)
{
    WriteFunction_t result = nullptr;

    const std::array<WriteFunction, 2> functions =
    {
        WriteFunction(QLatin1String("zcl"), 1, writeZclAttribute),
        WriteFunction(QLatin1String("tuya"), 1, writeTuyaData)
    };

    QString fnName;

    if (params.type() == QVariant::Map)
    {
        const auto params1 = params.toMap();
        if (params1.isEmpty())
        {  }
        else if (params1.contains(QLatin1String("fn")))
        {
            fnName = params1["fn"].toString();
        }
        else
        {
            fnName = QLatin1String("zcl"); // default
        }
    }

    for (const auto &f : functions)
    {
        if (f.name == fnName)
        {
            result = f.fn;
            break;
        }
    }

    return result;
}
