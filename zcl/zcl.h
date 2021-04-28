/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef ZCL_H
#define ZCL_H

namespace deCONZ
{
    class ApsController;
    class ApsDataIndication;
    class ZclFrame;
}

struct ZCL_Param
{
    bool valid = false;
    quint8 endpoint = 0;
    quint16 clusterId = 0;
    quint16 manufacturerCode = 0;
    std::vector<quint16> attributes;
};

struct ZCL_Result
{
    bool isEnqueued = false;    //! true when request was accepted for the APS request queue.
    uint8_t apsReqId = 0;       //! Underlying deCONZ::ApsDataRequest::id() to match in confirm.
    uint8_t sequenceNumber = 0; //! ZCL sequence number.

    /*! To check ZCL_Result in an if statement: e.g. `if(ZCL_SomeReq()) ...` */
    operator bool() const
    {
        return isEnqueued;
    }
};

struct ZCL_ReadReportConfigurationParam
{
    quint64 extAddress = 0;
    quint16 nwkAddress = 0;
    quint16 manufacturerCode = 0;
    quint16 clusterId = 0;

    struct Record
    {
        quint16 attributeId;
        quint8 direction;
        quint8 _pad;
    };
    quint8 endpoint = 0;
    std::vector<Record> records;
};

struct ZCL_ConfigureReportingParam
{
    quint64 extAddress = 0;
    quint16 nwkAddress = 0;
    quint16 manufacturerCode = 0;
    quint16 clusterId = 0;

    struct Record
    {
        quint64 reportableChange;
        quint16 attributeId;
        quint16 minInterval;
        quint16 maxInterval;
        quint16 timeout;
        quint8 direction;
        quint8 dataType;
        quint8 _pad;
    };
    quint8 endpoint = 0;
    std::vector<Record> records;
};

struct ZCL_ReadReportConfigurationRsp
{
    enum { MaxRecords = 6 };
    quint16 manufacturerCode = 0;
    quint16 clusterId = 0;
    quint8 sequenceNumber = 0;
    quint8 endpoint = 0;
    quint8 recordCount = 0;

    struct Record
    {
        quint64 reportableChange;
        quint16 attributeId;
        quint16 minInterval;
        quint16 maxInterval;
        quint8 status;
        quint8 direction;
        quint8 dataType;
        struct
        {
            unsigned char hasReportableChange : 1;
            unsigned char hasMinMaxInterval : 1;
            unsigned char _pad : 6;
        };
    };
    Record records[MaxRecords];
};

quint8 zclNextSequenceNumber();
ZCL_Result ZCL_ReadAttributes(const ZCL_Param &param, quint64 extAddress, quint16 nwkAddress, deCONZ::ApsController *apsCtrl);
ZCL_Result ZCL_ReadReportConfiguration(const ZCL_ReadReportConfigurationParam &param, deCONZ::ApsController *apsCtrl);
ZCL_Result ZCL_ConfigureReporting(const ZCL_ConfigureReportingParam &param, deCONZ::ApsController *apsCtrl);
ZCL_ReadReportConfigurationRsp ZCL_ParseReadReportConfigurationRsp(const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame);

#endif // ZCL_H
