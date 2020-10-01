#include "zdp.h"

static uint8_t zdpSeq;

int zdpSendNodeDescriptorReq(quint16 nwkAddress, deCONZ::ApsController *apsCtrl)
{
    Q_ASSERT(apsCtrl);

    DBG_Printf(DBG_ZDP, "get node descriptor for 0x%04X\n", nwkAddress);
    deCONZ::ApsDataRequest apsReq;

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

    stream << zdpSeq++;
    stream << nwkAddress;

    return apsCtrl->apsdeDataRequest(apsReq);
}

int zdpSendActiveEndpointsReq(uint16_t nwkAddress, deCONZ::ApsController *apsCtrl)
{
    Q_ASSERT(apsCtrl);

    DBG_Printf(DBG_ZDP, "get active endpoints for 0x%04X\n", nwkAddress);
    deCONZ::ApsDataRequest apsReq;

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

    stream << zdpSeq++;
    stream << nwkAddress;

    return apsCtrl->apsdeDataRequest(apsReq);
}

int zdpSendSimpleDescriptorReq(uint16_t nwkAddress, quint8 endpoint, deCONZ::ApsController *apsCtrl)
{
    Q_ASSERT(apsCtrl);

    DBG_Printf(DBG_ZDP, "[3] get simple descriptor 0x%02X for 0x%04X\n", endpoint, nwkAddress);
    deCONZ::ApsDataRequest apsReq;

    // ZDP Header
    apsReq.dstAddress().setNwk(nwkAddress);
    apsReq.setDstAddressMode(deCONZ::ApsNwkAddress);
    apsReq.setDstEndpoint(ZDO_ENDPOINT);
    apsReq.setSrcEndpoint(ZDO_ENDPOINT);
    apsReq.setProfileId(ZDP_PROFILE_ID);
    apsReq.setRadius(0);
    apsReq.setClusterId(ZDP_SIMPLE_DESCRIPTOR_CLID);
    //apsReq.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);

    QDataStream stream(&apsReq.asdu(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream << zdpSeq++;
    stream << nwkAddress;
    stream << endpoint;

    return apsCtrl->apsdeDataRequest(apsReq);
}
