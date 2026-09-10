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
#include <QDataStream>
#include <deconz/aps.h>
#include <deconz/aps_controller.h>
#include <deconz/dbg_trace.h>
#include <deconz/zdp_profile.h>
#include "zdp.h"

static uint8_t zdpSeq;

ZDP_Result ZDP_NodeDescriptorReq(const deCONZ::Address &addr, deCONZ::ApsController *apsCtrl)
{
    Q_ASSERT(apsCtrl);

    DBG_Printf(DBG_INFO, "ZDP get node descriptor for 0x%04X\n", addr.nwk());
    ZDP_Result result;

    if (!addr.hasExt() || !addr.hasNwk())
    {
        return result;
    }

    deCONZ::ApsDataRequest apsReq;

    result.apsReqId = apsReq.id();
    result.zdpSeq = zdpSeq++;

    // ZDP Header
    apsReq.dstAddress() = addr;
    apsReq.setDstAddressMode(deCONZ::ApsExtAddress);
    apsReq.setDstEndpoint(ZDO_ENDPOINT);
    apsReq.setSrcEndpoint(ZDO_ENDPOINT);
    apsReq.setProfileId(ZDP_PROFILE_ID);
    apsReq.setRadius(0);
    apsReq.setClusterId(ZDP_NODE_DESCRIPTOR_CLID);
//    apsReq.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);

    QDataStream stream(&apsReq.asdu(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream << result.zdpSeq;
    stream << addr.nwk();

    if (apsCtrl->apsdeDataRequest(apsReq) == deCONZ::Success)
    {
        result.isEnqueued = true;
        return result;
    }

    return result;
}

ZDP_Result ZDP_ActiveEndpointsReq(const deCONZ::Address &addr, deCONZ::ApsController *apsCtrl)
{
    Q_ASSERT(apsCtrl);

    DBG_Printf(DBG_INFO, "ZDP get active endpoints for 0x%04X\n", addr.nwk());
    ZDP_Result result;

    if (!addr.hasExt() || !addr.hasNwk())
    {
        return result;
    }

    deCONZ::ApsDataRequest apsReq;

    result.apsReqId = apsReq.id();
    result.zdpSeq = zdpSeq++;

    // ZDP Header
    apsReq.dstAddress() = addr;
    apsReq.setDstAddressMode(deCONZ::ApsExtAddress);
    apsReq.setDstEndpoint(ZDO_ENDPOINT);
    apsReq.setSrcEndpoint(ZDO_ENDPOINT);
    apsReq.setProfileId(ZDP_PROFILE_ID);
    apsReq.setRadius(0);
    apsReq.setClusterId(ZDP_ACTIVE_ENDPOINTS_CLID);
//    apsReq.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);

    QDataStream stream(&apsReq.asdu(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream << result.zdpSeq;
    stream << addr.nwk();

    if (apsCtrl->apsdeDataRequest(apsReq) == deCONZ::Success)
    {
        result.isEnqueued = true;
        return result;
    }

    return result;
}

ZDP_Result ZDP_SimpleDescriptorReq(const deCONZ::Address &addr, quint8 endpoint, deCONZ::ApsController *apsCtrl)
{
    Q_ASSERT(apsCtrl);

    DBG_Printf(DBG_INFO, "ZDP get simple descriptor 0x%02X for 0x%04X\n", endpoint, addr.nwk());
    ZDP_Result result;

    if (!addr.hasExt() || !addr.hasNwk())
    {
        return result;
    }

    deCONZ::ApsDataRequest apsReq;

    result.apsReqId = apsReq.id();
    result.zdpSeq = zdpSeq++;

    // ZDP Header
    apsReq.dstAddress() = addr;
    apsReq.setDstAddressMode(deCONZ::ApsExtAddress);
    apsReq.setDstEndpoint(ZDO_ENDPOINT);
    apsReq.setSrcEndpoint(ZDO_ENDPOINT);
    apsReq.setProfileId(ZDP_PROFILE_ID);
    apsReq.setRadius(0);
    apsReq.setClusterId(ZDP_SIMPLE_DESCRIPTOR_CLID);
//    apsReq.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);

    QDataStream stream(&apsReq.asdu(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream << result.zdpSeq;
    stream << addr.nwk();
    stream << endpoint;

    if (apsCtrl->apsdeDataRequest(apsReq) == deCONZ::Success)
    {
        result.isEnqueued = true;
        return result;
    }

    return result;
}

ZDP_Result ZDP_BindReq(const deCONZ::Binding &bnd, deCONZ::ApsController *apsCtrl)
{
    ZDP_Result result;
    deCONZ::ApsDataRequest apsReq;

    // set destination addressing
    apsReq.setDstAddressMode(deCONZ::ApsExtAddress);
    apsReq.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
    apsReq.dstAddress().setExt(bnd.srcAddress());
    apsReq.setDstEndpoint(ZDO_ENDPOINT);
    apsReq.setSrcEndpoint(ZDO_ENDPOINT);
    apsReq.setProfileId(ZDP_PROFILE_ID);
    apsReq.setClusterId(ZDP_BIND_REQ_CLID);

    result.apsReqId = apsReq.id();
    result.zdpSeq = zdpSeq++;

    // prepare payload
    QDataStream stream(&apsReq.asdu(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream << result.zdpSeq;
    stream << bnd.srcAddress();
    stream << bnd.srcEndpoint();
    stream << bnd.clusterId();
    stream << static_cast<uint8_t>(bnd.dstAddressMode());

    if (bnd.dstAddressMode() == deCONZ::ApsGroupAddress)
    {
        stream << bnd.dstAddress().group();
    }
    else if (bnd.dstAddressMode() == deCONZ::ApsExtAddress && bnd.dstAddress().ext() != 0 && bnd.dstEndpoint() != 0)
    {
        stream << quint64(bnd.dstAddress().ext());
        stream << bnd.dstEndpoint();
    }
    else
    {
        return { };
    }

    if (apsCtrl && (apsCtrl->apsdeDataRequest(apsReq) == deCONZ::Success))
    {
        result.isEnqueued = true;
        apsCtrl->addBinding(bnd);
    }

    return result;
}

ZDP_Result ZDP_UnbindReq(const deCONZ::Binding &bnd, deCONZ::ApsController *apsCtrl)
{
    ZDP_Result result;
    deCONZ::ApsDataRequest apsReq;

    // set destination addressing
    apsReq.setDstAddressMode(deCONZ::ApsExtAddress);
    apsReq.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
    apsReq.dstAddress().setExt(bnd.srcAddress());
    apsReq.setDstEndpoint(ZDO_ENDPOINT);
    apsReq.setSrcEndpoint(ZDO_ENDPOINT);
    apsReq.setProfileId(ZDP_PROFILE_ID);
    apsReq.setClusterId(ZDP_UNBIND_REQ_CLID);

    result.apsReqId = apsReq.id();
    result.zdpSeq = zdpSeq++;

    // prepare payload
    QDataStream stream(&apsReq.asdu(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream << result.zdpSeq;
    stream << bnd.srcAddress();
    stream << bnd.srcEndpoint();
    stream << bnd.clusterId();
    stream << static_cast<uint8_t>(bnd.dstAddressMode());

    if (bnd.dstAddressMode() == deCONZ::ApsGroupAddress)
    {
        stream << bnd.dstAddress().group();
    }
    else if (bnd.dstAddressMode() == deCONZ::ApsExtAddress && bnd.dstAddress().ext() != 0 && bnd.dstEndpoint() != 0)
    {
        stream << quint64(bnd.dstAddress().ext());
        stream << bnd.dstEndpoint();
    }
    else
    {
        return { };
    }

    if (apsCtrl && (apsCtrl->apsdeDataRequest(apsReq) == deCONZ::Success))
    {
        result.isEnqueued = true;
        apsCtrl->removeBinding(bnd);
    }

    return result;
}

ZDP_Result ZDP_MgmtBindReq(uint8_t startIndex, const deCONZ::Address &addr, deCONZ::ApsController *apsCtrl)
{
    ZDP_Result result;
    deCONZ::ApsDataRequest apsReq;

    // set destination addressing
    apsReq.setDstAddressMode(deCONZ::ApsExtAddress);
    apsReq.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
    apsReq.dstAddress() = addr;
    apsReq.setDstEndpoint(ZDO_ENDPOINT);
    apsReq.setSrcEndpoint(ZDO_ENDPOINT);
    apsReq.setProfileId(ZDP_PROFILE_ID);
    apsReq.setClusterId(ZDP_MGMT_BIND_REQ_CLID);

    result.apsReqId = apsReq.id();
    result.zdpSeq = ZDP_NextSequenceNumber();

    // prepare payload
    QDataStream stream(&apsReq.asdu(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream << result.zdpSeq;
    stream << startIndex;

    if (apsCtrl && (apsCtrl->apsdeDataRequest(apsReq) == deCONZ::Success))
    {
        result.isEnqueued = true;
    }

    return result;
}

uint8_t ZDP_NextSequenceNumber()
{
    uint8_t result = zdpSeq++;
    return result;
}
