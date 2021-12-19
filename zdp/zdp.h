/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef ZDP_H
#define ZDP_H

#include <cstdint>

namespace deCONZ
{
    class Address;
    class ApsController;
    class Binding;
}

class ZDP_Binding
{
public:
    uint64_t srcExtAddress;

    bool isValid() const { return (isUnicastBinding || isGroupBinding) && srcEndpoint != 0; }
    union
    {
        uint16_t dstGroup;
        uint64_t dstExtAddress;
    };

    uint16_t clusterId;
    uint8_t srcEndpoint;
    uint8_t dstEndpoint;
    struct
    {
        unsigned int isGroupBinding : 1;
        unsigned int isUnicastBinding : 1;
        unsigned int pad : 6 + 24;
    };
};

struct ZDP_Result
{
    bool isEnqueued = false; //! true when request was accepted for the APS request queue.
    uint8_t apsReqId = 0;    //! Underlying deCONZ::ApsDataRequest::id() to match in confirm.
    uint8_t zdpSeq = 0;      //! ZDP sequence number.

    /*! To check ZDP_Result in an if statement: e.g. `if(ZDP_SomeReq()) ...` */
    operator bool() const
    {
        return isEnqueued;
    }
};

ZDP_Result ZDP_NodeDescriptorReq(const deCONZ::Address &addr, deCONZ::ApsController *apsCtrl);
ZDP_Result ZDP_ActiveEndpointsReq(const deCONZ::Address &addr, deCONZ::ApsController *apsCtrl);
ZDP_Result ZDP_SimpleDescriptorReq(const deCONZ::Address &addr, uint8_t endpoint, deCONZ::ApsController *apsCtrl);
ZDP_Result ZDP_BindReq(const deCONZ::Binding &bnd, deCONZ::ApsController *apsCtrl);
ZDP_Result ZDP_UnbindReq(const deCONZ::Binding &bnd, deCONZ::ApsController *apsCtrl);
ZDP_Result ZDP_MgmtBindReq(uint8_t startIndex, const deCONZ::Address &addr, deCONZ::ApsController *apsCtrl);
uint8_t ZDP_NextSequenceNumber();

#endif // ZDP_H
