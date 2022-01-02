/*
 * Copyright (c) 2020 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "de_web_plugin_private.h"

#define IDENTIFY_COMMAND_IDENTIFY_QUERY          0x01
#define IDENTIFY_COMMAND_IDENTIFY_QUERY_RESPONSE 0x00

void DeRestPluginPrivate::handleIdentifyClusterIndication(const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame)
{

    if (zclFrame.commandId() == IDENTIFY_COMMAND_IDENTIFY_QUERY &&
        zclFrame.isClusterCommand() &&
        (zclFrame.frameControl() & deCONZ::ZclFCDirectionServerToClient) == 0)
    {
        {   // Aqara Opple switches send identify query command when not configured for using Multistate Input Cluster
            // Note they behave differently when paired to coordinator vs. router
            auto *sensor = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), 0x01);

            if (sensor && sensor->modelId().endsWith(QLatin1String("86opcn01")))
            {
                auto *item = sensor->item(RConfigPending);
                if (item /*&& (item->toNumber() & R_PENDING_MODE)*/)
                {
                    // Aqara Opple switches need to be configured to send proper button events
                    // send the magic word
                    DBG_Printf(DBG_INFO, "Write Aqara Opple switch 0x%016llX mode attribute 0x0009 = 1\n", ind.srcAddress().ext());
                    deCONZ::ZclAttribute attr(0x0009, deCONZ::Zcl8BitUint, QLatin1String("mode"), deCONZ::ZclReadWrite, false);
                    attr.setValue(static_cast<quint64>(1));
                    writeAttribute(sensor, 0x01, 0xFCC0, attr, VENDOR_XIAOMI);
                    item->setValue(item->toNumber() & ~R_PENDING_MODE);
                }
                return;
            }

            if (permitJoinFlag)
            {
                // send Idendify Query Response if requested during pairing
                deCONZ::ApsDataRequest req;
                deCONZ::ZclFrame outZclFrame;

                req.dstAddress() = ind.srcAddress();
                req.setDstAddressMode(deCONZ::ApsExtAddress);
                req.setClusterId(ind.clusterId());
                req.setProfileId(ind.profileId());
                req.setDstEndpoint(ind.srcEndpoint());
                req.setSrcEndpoint(endpoint());

                outZclFrame.setSequenceNumber(zclFrame.sequenceNumber());
                outZclFrame.setCommandId(IDENTIFY_COMMAND_IDENTIFY_QUERY_RESPONSE);


                outZclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                                            deCONZ::ZclFCDirectionServerToClient |
                                            deCONZ::ZclFCDisableDefaultResponse);

                { // payload
                    QDataStream stream(&outZclFrame.payload(), QIODevice::WriteOnly);
                    stream.setByteOrder(QDataStream::LittleEndian);

                    stream << quint16(60); // our identify time
                }

                { // ZCL frame
                    QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
                    stream.setByteOrder(QDataStream::LittleEndian);
                    outZclFrame.writeToStream(stream);
                }

                if (apsCtrlWrapper.apsdeDataRequest(req) == deCONZ::Success)
                {
                    DBG_Printf(DBG_INFO, "send identify query response to 0x%016llX\n", ind.srcAddress().ext());
                }
            }
        }
    }
}
