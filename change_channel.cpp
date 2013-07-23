/*
 * Copyright (C) 2013 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "de_web_plugin_private.h"

void DeRestPluginPrivate::changeChannel(int channel)
{
    DBG_Assert(channel >= 11 && channel <= 26);

    if ((channel >= 11) && (channel <= 26))
    {


        deCONZ::ApsDataRequest req;

        req.setTxOptions(0);
        req.setDstEndpoint(0);
//        req.setDstAddressMode(deCONZ::ApsGroupAddress);
//        req.dstAddress().setGroup(groupId);
        req.setDstAddressMode(deCONZ::ApsNwkAddress);
        req.dstAddress().setNwk(0xFFFF);
        req.setProfileId(0x0000); // ZDP profile
        req.setClusterId(0x0038); //  Mgmt_NWK_Update_req
        req.setSrcEndpoint(0);
        req.setRadius(0);

        uint8_t zdpSeq = 0x77;
        uint32_t scanChannels = (1 << (uint)channel);
        uint8_t scanDuration = 0xfe; //special value = channel change
        uint8_t nwkUpdateId = 0x00;

        QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream << zdpSeq;
        stream << scanChannels;
        stream << scanDuration;
        stream << nwkUpdateId;

        deCONZ::ApsController *apsCtrl = deCONZ::ApsController::instance();

        if (apsCtrl->apsdeDataRequest(req) == 0)
        {
            DBG_Printf(DBG_INFO, "change channel to %d, channel mask = 0x%08lX\n", channel, scanChannels);
        }
        else
        {
            DBG_Printf(DBG_ERROR, "cant send change channel\n");
        }
#if 0
        req.setTxOptions(0);
        req.setDstEndpoint(0);
//        req.setDstAddressMode(deCONZ::ApsGroupAddress);
//        req.dstAddress().setGroup(groupId);
        req.setDstAddressMode(deCONZ::ApsExtAddress);
        req.dstAddress().setExt(0x17880100b829e1); // TODO: use selected node
        req.setProfileId(0x0000); // ZDP profile
        req.setClusterId(0x0034); //  Mgmt_Leave_req
        req.setSrcEndpoint(0);
        req.setRadius(0);

        uint8_t zdpSeq = 0x77;
        uint64_t deviceAddress = 0;
        uint8_t flags = 0x00;

        QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream << zdpSeq;
        stream << deviceAddress;
        stream << flags;

        if (apsCtrl->apsdeDataRequest(req) == 0)
        {
            DBG_Printf(DBG_INFO, "send mgmt leave\n");
        }
        else
        {
            DBG_Printf(DBG_ERROR, "cant send mgmt leave\n");
        }
#endif
    }
}
