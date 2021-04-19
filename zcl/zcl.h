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

quint8 zclNextSequenceNumber();
ZCL_Result ZCL_ReadAttributes(const ZCL_Param &param, quint64 extAddress, quint16 nwkAddress, deCONZ::ApsController *apsCtrl);

#endif // ZCL_H
