/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QDataStream>
#include <deconz/aps.h>
#include <deconz/aps_controller.h>
#include <deconz/dbg_trace.h>
#include <deconz/zdp_profile.h>
#include "zdp.h"

static uint8_t zdpSeq;

ZDP_Result ZDP_NodeDescriptorReq(quint16 nwkAddress, deCONZ::ApsController *apsCtrl)
{
    Q_ASSERT(apsCtrl);

    DBG_Printf(DBG_INFO, "ZDP get node descriptor for 0x%04X\n", nwkAddress);
    deCONZ::ApsDataRequest apsReq;
    ZDP_Result result;

    result.apsReqId = apsReq.id();
    result.zdpSeq = zdpSeq++;

    // ZDP Header
    apsReq.dstAddress().setNwk(nwkAddress);
    apsReq.setDstAddressMode(deCONZ::ApsNwkAddress);
    apsReq.setDstEndpoint(ZDO_ENDPOINT);
    apsReq.setSrcEndpoint(ZDO_ENDPOINT);
    apsReq.setProfileId(ZDP_PROFILE_ID);
    apsReq.setRadius(0);
    apsReq.setClusterId(ZDP_NODE_DESCRIPTOR_CLID);
    apsReq.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);

    QDataStream stream(&apsReq.asdu(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream << result.zdpSeq;
    stream << nwkAddress;

    if (apsCtrl->apsdeDataRequest(apsReq) == deCONZ::Success)
    {
        result.isEnqueued = true;
        return result;
    }

    return result;
}

ZDP_Result ZDP_ActiveEndpointsReq(uint16_t nwkAddress, deCONZ::ApsController *apsCtrl)
{
    Q_ASSERT(apsCtrl);

    DBG_Printf(DBG_INFO, "ZDP get active endpoints for 0x%04X\n", nwkAddress);
    deCONZ::ApsDataRequest apsReq;
    ZDP_Result result;

    result.apsReqId = apsReq.id();
    result.zdpSeq = zdpSeq++;

    // ZDP Header
    apsReq.dstAddress().setNwk(nwkAddress);
    apsReq.setDstAddressMode(deCONZ::ApsNwkAddress);
    apsReq.setDstEndpoint(ZDO_ENDPOINT);
    apsReq.setSrcEndpoint(ZDO_ENDPOINT);
    apsReq.setProfileId(ZDP_PROFILE_ID);
    apsReq.setRadius(0);
    apsReq.setClusterId(ZDP_ACTIVE_ENDPOINTS_CLID);
    apsReq.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);

    QDataStream stream(&apsReq.asdu(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream << result.zdpSeq;
    stream << nwkAddress;

    if (apsCtrl->apsdeDataRequest(apsReq) == deCONZ::Success)
    {
        result.isEnqueued = true;
        return result;
    }

    return result;
}

ZDP_Result ZDP_SimpleDescriptorReq(uint16_t nwkAddress, quint8 endpoint, deCONZ::ApsController *apsCtrl)
{
    Q_ASSERT(apsCtrl);

    DBG_Printf(DBG_INFO, "ZDP get simple descriptor 0x%02X for 0x%04X\n", endpoint, nwkAddress);
    deCONZ::ApsDataRequest apsReq;
    ZDP_Result result;

    result.apsReqId = apsReq.id();
    result.zdpSeq = zdpSeq++;

    // ZDP Header
    apsReq.dstAddress().setNwk(nwkAddress);
    apsReq.setDstAddressMode(deCONZ::ApsNwkAddress);
    apsReq.setDstEndpoint(ZDO_ENDPOINT);
    apsReq.setSrcEndpoint(ZDO_ENDPOINT);
    apsReq.setProfileId(ZDP_PROFILE_ID);
    apsReq.setRadius(0);
    apsReq.setClusterId(ZDP_SIMPLE_DESCRIPTOR_CLID);
    apsReq.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);

    QDataStream stream(&apsReq.asdu(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream << result.zdpSeq;
    stream << nwkAddress;
    stream << endpoint;

    if (apsCtrl->apsdeDataRequest(apsReq) == deCONZ::Success)
    {
        result.isEnqueued = true;
        return result;
    }

    return result;
}
