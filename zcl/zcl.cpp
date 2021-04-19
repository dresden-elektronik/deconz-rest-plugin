/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <deconz/aps_controller.h>
#include <deconz/dbg_trace.h>
#include <deconz/zcl.h>
#include "zcl.h"

ZCL_Result ZCL_ReadAttributes(const ZCL_Param &param, quint64 extAddress, quint16 nwkAddress, deCONZ::ApsController *apsCtrl)
{
    ZCL_Result result;


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

    deCONZ::ZclFrame zclFrame;

    zclFrame.setSequenceNumber(zclNextSequenceNumber());
    zclFrame.setCommandId(deCONZ::ZclReadAttributesId);

    DBG_Printf(DBG_INFO, "ZCL read attr, ep: 0x%02X, cl: 0x%04X, attr: 0x%04X, mfcode: 0x%04X, aps.id: %u, zcl.seq: %u\n",
               param.endpoint, param.clusterId, param.attributes.front(), param.manufacturerCode, req.id(), zclFrame.sequenceNumber());

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

        for (const quint16 attrId : param.attributes)
        {
            stream << attrId;
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
