#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "debug.h"

void DeRestPluginPrivate::handleDebugging(const deCONZ::ApsDataRequest &req)
{
    //bool found;

    //DBG_Printf(DBG_INFO_L2, "[REQUEST] - Sending data to node: 0x%016llX (%04X)\n", req.dstAddress().ext(), req.dstAddress().nwk());
//    DBG_Printf(DBG_INFO_L2, "[REQUEST] - 0x%016llX (0x%04X) - Request for Profile: 0x%04X, Cluster: 0x%04X, Endpoint: %d\n", req.dstAddress().ext(), req.dstAddress().nwk(),
//                        req.profileId(), req.clusterId(), req.dstEndpoint());
    //DBG_Printf(DBG_INFO_L2, "[REQUEST] - 0x%016llX (0x%04X) - Request ASDU: %s\n", req.dstAddress().ext(), req.dstAddress().nwk(), qPrintable(req.asdu().toHex()));

    QString apsData = QLatin1String("[REQUEST] - 0x") + QString("%1").arg(req.dstAddress().ext(), 16, 16, QLatin1Char('0')).toUpper() + QLatin1String(" (0x") +
                      QString("%1").arg(req.dstAddress().nwk(), 4, 16, QLatin1Char('0')).toUpper() +  QLatin1String(") - Profile: 0x") +
                      QString("%1").arg(req.profileId(), 4, 16, QLatin1Char('0')).toUpper() + QLatin1String(", Cluster: 0x") +
                      QString("%1").arg(req.clusterId(), 4, 16, QLatin1Char('0')).toUpper() + QLatin1String(", EP: 0x") +
                      QString("%1").arg(req.dstEndpoint(), 2, 16, QLatin1Char('0')).toUpper() + QLatin1String(", DstAddrMode: ") + QString("%1").arg(req.dstAddressMode(), 2, 16, QLatin1Char('0')).toUpper();

    if (req.profileId() == ZDP_PROFILE_ID)
    {
        const auto matchCl = matchKeyFromMap1(req.clusterId(), zdpCluster);

        quint8 seq;
        quint16 nwk;

        switch (matchCl.key)
        {
            case ZDP_NODE_DESCRIPTOR_CLID:
            {
                QDataStream stream(req.asdu());
                stream.setByteOrder(QDataStream::LittleEndian);

                stream >> seq;
                stream >> nwk;

                DBG_Printf(DBG_INFO_L2, "%s, %s (0x%04X) - Sequence no.: %d, NWK: 0x%04X\n",
                            qPrintable(apsData), qUtf8Printable(matchCl.value), matchCl.key, seq, nwk);
            }
                break;

            case ZDP_SIMPLE_DESCRIPTOR_CLID:
            {
                quint8 ep;

                QDataStream stream(req.asdu());
                stream.setByteOrder(QDataStream::LittleEndian);

                stream >> seq;
                stream >> nwk;
                stream >> ep;

                DBG_Printf(DBG_INFO_L2, "%s, %s (0x%04X) - Sequence no.: %d, NWK: 0x%04X, Endpoint: %02X\n",
                            qPrintable(apsData), qUtf8Printable(matchCl.value), matchCl.key, seq, nwk, ep);
            }
                break;

            case ZDP_ACTIVE_ENDPOINTS_CLID:
            {
                QDataStream stream(req.asdu());
                stream.setByteOrder(QDataStream::LittleEndian);

                stream >> seq;
                stream >> nwk;

                DBG_Printf(DBG_INFO_L2, "%s, %s (0x%04X) - Sequence no.: %d, NWK: 0x%04X\n",
                            qPrintable(apsData), qUtf8Printable(matchCl.value), matchCl.key, seq, nwk);
            }
                break;

            case ZDP_BIND_REQ_CLID:
            {
                quint64 srcIeee;
                quint8 srcEp;
                quint16 srcCluster;
                quint8 adrMode;
                quint64 dstIeee;
                quint8 dstEp;


                QDataStream stream(req.asdu());
                stream.setByteOrder(QDataStream::LittleEndian);

                stream >> seq;
                stream >> srcIeee;
                stream >> srcEp;
                stream >> srcCluster;
                stream >> adrMode;
//                DBG_Printf(DBG_INFO_L2, "[REQUEST] - 0x%016llX (0x%04X) - Send bind request - ASDU size: %d\n",
//                            req.dstAddress().ext(), req.dstAddress().nwk(), req.asdu().size());

                if (req.asdu().size() == 15)
                {
                    quint16 dstGroup;
                    stream >> dstGroup;

                    DBG_Printf(DBG_INFO_L2, "%s, %s (0x%04X) - Sequence no.: %d, SrcExt: 0x%016llX, srcEp: %02X, srcCluster: 0x%04X, adrMode: %02X, dstGroup: 0x%04X\n",
                            qPrintable(apsData), qUtf8Printable(matchCl.value), matchCl.key, seq, srcIeee, srcEp, srcCluster, adrMode, dstGroup);
                }
                else if (req.asdu().size() >= 22 && req.asdu().size() <= 24)
                {
                    quint16 dstCluster = 0xFFFF;

                    stream >> dstIeee;
                    stream >> dstEp;

                    if (!stream.atEnd())
                    {
                        stream >> dstCluster;
                    }

                    DBG_Printf(DBG_INFO_L2, "%s, %s (0x%04X) - Sequence no.: %d, srcExt: 0x%016llX, srcEp: %02X, srcCluster: 0x%04X, adrMode: %02X, dstExt: 0x%016llX, dstEp: %02X, dstCluster: 0x%04X\n",
                            qPrintable(apsData), qUtf8Printable(matchCl.value), matchCl.key, seq, srcIeee, srcEp, srcCluster, adrMode, dstIeee, dstEp, dstCluster);
                }
            }
                break;

            case ZDP_MGMT_BIND_REQ_CLID:
            {
                /*QDataStream stream(req.asdu());
                stream.setByteOrder(QDataStream::LittleEndian);

                stream >> seq;
                stream >> status;
                // Potentially following data is not yet picked up here

                DBG_Printf(DBG_INFO_L2, "%s, %s (0x%02X) - Sequence no.: %d, Status: 0x%02X\n",
                            qPrintable(apsData), seq, status);*/
            }
                break;

            default:
                break;
        }
    }
    else
    {
        deCONZ::ZclFrame zclFrame; // dummy

        {
            QDataStream stream(req.asdu());
            stream.setByteOrder(QDataStream::LittleEndian);
            zclFrame.readFromStream(stream);
        }

        //DBG_Printf(DBG_INFO_L2, "[REQUEST] - 0x%016llX (0x%04X) - ZCL payload (size %d): %s\n", req.dstAddress().ext(), req.dstAddress().nwk(),
        //            zclFrame.payload().size(), qPrintable(zclFrame.payload().toHex()));

        if (zclFrame.isProfileWideCommand())
        {
            const auto matchCmd = matchValueFromMap1(zclFrame.commandId(), zclGeneralCommandIds);

            switch (matchCmd.value)
            {
                case deCONZ::ZclGeneralCommandId::ZclReadAttributesId:
                {
                    dbgReadAttributes(zclFrame, matchCmd, apsData);
                }
                    break;

                case deCONZ::ZclGeneralCommandId::ZclWriteAttributesId:
                {
                    dbgWriteAttributes(zclFrame, matchCmd, apsData);
                }
                    break;

                case deCONZ::ZclGeneralCommandId::ZclReadReportingConfigId:
                {
                    dbgReadReportingConfig(zclFrame, matchCmd, apsData);
                }
                    break;

                case deCONZ::ZclGeneralCommandId::ZclConfigureReportingId:
                {
                    dbgConfigureReporting(zclFrame, matchCmd, apsData);
                }
                    break;

                case deCONZ::ZclGeneralCommandId::ZclReadAttributesResponseId:
                case deCONZ::ZclGeneralCommandId::ZclReportAttributesId:
                {
                    dbgReportAttributesAndReadAttributesRsp(zclFrame, matchCmd, apsData);
                    //dbgReportAttributesAndReadAttributesRsp2(req.dstAddressMod, zclFrame, matchCmd);
                }
                    break;

                case deCONZ::ZclGeneralCommandId::ZclWriteAttributesResponseId:
                {
                    dbgWriteAttributes(zclFrame, matchCmd, apsData);
                }
                    break;

                case deCONZ::ZclGeneralCommandId::ZclReadReportingConfigResponseId:
                {
                    dbgReadReportingConfigRsp(zclFrame, matchCmd, apsData);
                }
                    break;

                case deCONZ::ZclGeneralCommandId::ZclConfigureReportingResponseId:
                {
                    dbgConfigureReportingRsp(zclFrame, matchCmd, apsData);
                }
                    break;

                case deCONZ::ZclGeneralCommandId::ZclWriteAttributesUndividedId:
                case deCONZ::ZclGeneralCommandId::ZclWriteAttributesNoResponseId:
                case deCONZ::ZclGeneralCommandId::ZclDefaultResponseId:
                case deCONZ::ZclGeneralCommandId::ZclDiscoverAttributesId:
                case deCONZ::ZclGeneralCommandId::ZclDiscoverAttributesResponseId:
                case deCONZ::ZclGeneralCommandId::ZclWriteAttributesStructuredId:
                case deCONZ::ZclGeneralCommandId::ZclWriteAttributesStructuredResponseId:
                {
//                    DBG_Printf(DBG_INFO_L2, "[REQUEST] - 0x%016llX (0x%04X) - %s (0x%02X) - Sequence no.: %d, Mfc: 0x%04X, PL-Size: %u, ZCL-Payload: 0x%s\n",
//                                        req.dstAddress().ext(), req.dstAddress().nwk(), qUtf8Printable(matchCmd.key), matchCmd.value, zclFrame.sequenceNumber(), zclFrame.manufacturerCode(),
//                                        zclFrame.payload().size(), qPrintable(zclFrame.payload().toHex().toUpper()));

                    dbgRareGeneralCommand(zclFrame, matchCmd, apsData);

//                    DBG_Printf(DBG_INFO_L2, "%s %s (0x%02X) - Sequence no.: %d, Mfc: 0x%04X, PL-Size: %u, ZCL-Payload: 0x%s\n",
//                                    qPrintable(apsData), qUtf8Printable(matchCmd.key), matchCmd.value, zclFrame.sequenceNumber(), zclFrame.manufacturerCode(),
//                                    zclFrame.payload().size(), qPrintable(zclFrame.payload().toHex().toUpper()));
                }
                    break;

                default:
                    break;
            }
        }
        else if (zclFrame.isClusterCommand())
        {
//            DBG_Printf(DBG_INFO_L2, "[REQUEST] - 0x%016llX (0x%04X) - Cluster command: %02X - Sequence no.: %d, Mfc: 0x%04X, PL-Size: %u, ZCL-Payload: 0x%s\n",
//                                req.dstAddress().ext(), req.dstAddress().nwk(), zclFrame.commandId(), zclFrame.sequenceNumber(), zclFrame.manufacturerCode(), zclFrame.payload().size(),
//                                qPrintable(zclFrame.payload().toHex().toUpper()));

            dbgClusterCommand(zclFrame, apsData);

//            DBG_Printf(DBG_INFO_L2, "%s, Cluster command: %02X, SeqNo.: %d, Mfc: 0x%04X, PL-Size: %u, ZCL-Payload: 0x%s\n",
//                                qPrintable(apsData), zclFrame.commandId(), zclFrame.sequenceNumber(), zclFrame.manufacturerCode(), zclFrame.payload().size(),
//                                qPrintable(zclFrame.payload().toHex().toUpper()));
        }
    }
}

/*! New debugging callback callback.
    \param ind - the indication primitive
    \note Will be called from the main application for each incoming indication.
    Any filtering for nodes, profiles, clusters must be handled by this plugin.
 */
void DeRestPluginPrivate::handleDebugging(const deCONZ::ApsDataIndication &ind)
{
    //DBG_Printf(DBG_INFO_L2, "[INDICATION] - Incoming data from node: 0x%016llX (%04X)\n", ind.srcAddress().ext(), ind.srcAddress().nwk());
//    DBG_Printf(DBG_INFO_L2, "[INDICATION] - 0x%016llX (0x%04X) - Indication for Profile: 0x%04X, Cluster: 0x%04X, Endpoint: %d, Status: 0x%02X\n", ind.srcAddress().ext(), ind.srcAddress().nwk(),
//                        ind.profileId(), ind.clusterId(), ind.srcEndpoint(), ind.status());

    QString apsData = QLatin1String("[INDICATION] - 0x") + QString("%1").arg(ind.srcAddress().ext(), 16, 16, QLatin1Char('0')).toUpper() + QLatin1String(" (0x") +
                   QString("%1").arg(ind.srcAddress().nwk(), 4, 16, QLatin1Char('0')).toUpper() +  QLatin1String(") - Profile: 0x") +
                   QString("%1").arg(ind.profileId(), 4, 16, QLatin1Char('0')).toUpper() + QLatin1String(", Cluster: 0x") +
                   QString("%1").arg(ind.clusterId(), 4, 16, QLatin1Char('0')).toUpper() + QLatin1String(", EP: 0x") +
                   QString("%1").arg(ind.srcEndpoint(), 2, 16, QLatin1Char('0')).toUpper();

    if (ind.dstAddress().isNwkUnicast())
    {
        apsData += ", Unicast to: 0x" + QString("%1").arg(ind.dstAddress().nwk(), 4, 16, QLatin1Char('0')).toUpper();
        apsData += ", DstAddrMode: " + QString("%1").arg(ind.dstAddressMode(), 2, 16, QLatin1Char('0')).toUpper();
    }
    else if (ind.dstAddressMode() == deCONZ::ApsGroupAddress)
    {
        apsData += ", Broadcast to: 0x" + QString("%1").arg(ind.dstAddress().group(), 4, 16, QLatin1Char('0')).toUpper();
    }
    else
    {
        apsData += ", DstAddrMode: " + QString("%1").arg(ind.dstAddressMode(), 2, 16, QLatin1Char('0')).toUpper();
    }


    //DBG_Printf(DBG_INFO_L2, "[INDICATION] - 0x%016llX (0x%04X) - Indication ASDU: %s\n", ind.srcAddress().ext(), ind.srcAddress().nwk(), qPrintable(ind.asdu().toHex()));


    //DBG_Printf(DBG_INFO_L2, "[FOR_ERIK] - %04X: resp: %d: 0x%016llX-%02X-%04X, payload (ASDU): %s\n", ind.srcAddress().nwk(), zclFrame.sequenceNumber(), ind.srcAddress().ext(),
                //ind.dstEndpoint(), ind.clusterId(), qPrintable(ind.asdu().toHex()));

    if (ind.profileId() == ZDP_PROFILE_ID)
    {
        const auto matchCl = matchKeyFromMap1(ind.clusterId(), zdpCluster);

        quint8 seq;
        quint8 status;
        quint16 nwk;

        switch (matchCl.key)
        {
            case ZDP_NODE_DESCRIPTOR_RSP_CLID:
            {
                deCONZ::NodeDescriptor nd;

                QDataStream stream(ind.asdu());
                stream.setByteOrder(QDataStream::LittleEndian);

                stream >> seq;
                stream >> status;
                stream >> nwk;

                nd.readFromStream(stream);

                DBG_Printf(DBG_INFO_L2, "%s, %s (0x%04X) - Sequence no.: %d, Node descriptor: 0x%s\n",
                            qUtf8Printable(apsData), qUtf8Printable(matchCl.value), matchCl.key, seq, qPrintable(nd.toByteArray().toHex()));
            }
                break;

            case ZDP_SIMPLE_DESCRIPTOR_RSP_CLID:
            {
                quint8 sdLength;
                quint8 ep;
                quint16 profile;
                quint16 appDevice;
                quint8 appVersion;
                quint8 inClusterCount;
                quint8 outClusterCount;
                quint16 cluster;
                QString inClusters;
                QString outClusters;

                QDataStream stream(ind.asdu());
                stream.setByteOrder(QDataStream::LittleEndian);

                stream >> seq;
                stream >> status;
                stream >> nwk;
                stream >> sdLength;
                stream >> ep;
                stream >> profile;
                stream >> appDevice;
                stream >> appVersion;
                stream >> inClusterCount;

                for (int i=0; i<inClusterCount; i++)
                {
                    if (!stream.atEnd())
                    {
                        stream >> cluster;
                        inClusters += QString("%1").arg(cluster, 4, 16, QLatin1Char('0')).toUpper() + ", ";
                    }
                }

                inClusters = inClusters.left(inClusters.lastIndexOf(QChar(',')));   // Strip everything after the last comma, for beauty
                stream >> outClusterCount;

                for (int i=0; i<outClusterCount; i++)
                {
                    if (!stream.atEnd())
                    {
                        stream >> cluster;
                        outClusters += QString("%1").arg(cluster, 4, 16, QLatin1Char('0')).toUpper() + ", ";
                    }
                }

                outClusters = outClusters.left(outClusters.lastIndexOf(QChar(',')));    // Strip everything after the last comma, for beauty

//                DBG_Printf(DBG_INFO_L2, "[INDICATION] - 0x%016llX (0x%04X) - Received simple descriptor response - Sequence no.: %d\n",
//                            ind.srcAddress().ext(), ind.srcAddress().nwk(), seq);
                DBG_Printf(DBG_INFO_L2, "%s, %s (0x%04X) - Sequence no.: %d, Ep: %02X, Profile: 0x%04X, DeviceID: 0x%04X, Input clusters: %s, Output clusters: %s\n",
                            qUtf8Printable(apsData), qUtf8Printable(matchCl.value), matchCl.key, seq, ep, profile, appDevice, qUtf8Printable(inClusters), qUtf8Printable(outClusters));
            }
                break;

            case ZDP_ACTIVE_ENDPOINTS_RSP_CLID:
            {
                quint8 epCount;
                quint8 ep;
                QString endpoints;

                QDataStream stream(ind.asdu());
                stream.setByteOrder(QDataStream::LittleEndian);

                stream >> seq;
                stream >> status;
                stream >> nwk;
                stream >> epCount;

                for (int i=0; i<epCount; i++)
                {
                    if (!stream.atEnd())
                    {
                        stream >> ep;
                        endpoints += QString("%1").arg(ep, 2, 16, QLatin1Char('0')).toUpper() + ", ";
                    }
                }

                endpoints = endpoints.left(endpoints.lastIndexOf(QChar(',')));   // Strip everything after the last comma, for beauty

                DBG_Printf(DBG_INFO_L2, "%s, %s (0x%04X) - Sequence no.: %d, Active endpoints: %s\n",
                            qUtf8Printable(apsData), qUtf8Printable(matchCl.value), matchCl.key, seq, qUtf8Printable(endpoints));
            }
                break;

            case ZDP_MATCH_DESCRIPTOR_CLID:
            {
                quint16 profile;
                quint8 inClusterCount;
                quint8 outClusterCount;
                quint16 cluster;
                QString inClusters;
                QString outClusters;

                QDataStream stream(ind.asdu());
                stream.setByteOrder(QDataStream::LittleEndian);

                stream >> seq;
                stream >> nwk;
                stream >> profile;
                stream >> inClusterCount;
                stream >> outClusterCount;

                for (int i=0; i<inClusterCount; i++)
                {
                    if (!stream.atEnd())
                    {
                        stream >> cluster;
                        inClusters += QString("%1").arg(cluster, 4, 16, QLatin1Char('0')).toUpper() + ", ";
                    }
                }

                inClusters = inClusters.left(inClusters.lastIndexOf(QChar(',')));   // Strip everything after the last comma, for beauty

                for (int i=0; i<outClusterCount; i++)
                {
                    if (!stream.atEnd())
                    {
                        stream >> cluster;
                        outClusters += QString("%1").arg(cluster, 4, 16, QLatin1Char('0')).toUpper() + ", ";
                    }
                }

                outClusters = outClusters.left(outClusters.lastIndexOf(QChar(',')));    // Strip everything after the last comma, for beauty

                DBG_Printf(DBG_INFO_L2, "%s, %s (0x%04X) - Sequence no.: %d, Profile: 0x%04X, Input clusters: %s, Output clusters: %s\n\n",
                        qUtf8Printable(apsData), qUtf8Printable(matchCl.value), matchCl.key, seq, profile, qUtf8Printable(inClusters), qUtf8Printable(outClusters));

            }
                break;

            case ZDP_DEVICE_ANNCE_CLID:
            {
                quint64 ieee;
                quint8 macCapabilities;

                QDataStream stream(ind.asdu());
                stream.setByteOrder(QDataStream::LittleEndian);

                stream >> seq;
                stream >> nwk;
                stream >> ieee;
                stream >> macCapabilities;

                DBG_Printf(DBG_INFO_L2, "%s, %s (0x%04X) - Sequence no.: %d, MAC capabilities: 0x%02X\n",
                            qUtf8Printable(apsData), qUtf8Printable(matchCl.value), matchCl.key, seq, macCapabilities);
            }
                break;

            case ZDP_IEEE_ADDR_CLID:
            {
                quint8 reqType;
                quint8 idx;

                QDataStream stream(ind.asdu());
                stream.setByteOrder(QDataStream::LittleEndian);

                stream >> seq;
                stream >> nwk;
                stream >> reqType;
                stream >> idx;

                DBG_Printf(DBG_INFO_L2, "%s, %s (0x%04X) - Sequence no.: %d\n",
                            qUtf8Printable(apsData), qUtf8Printable(matchCl.value), matchCl.key, seq);
            }
                break;

            case ZDP_NWK_ADDR_CLID:
            {
                quint64 ieee;
                quint8 reqType;
                quint8 idx;

                QDataStream stream(ind.asdu());
                stream.setByteOrder(QDataStream::LittleEndian);

                stream >> seq;
                stream >> ieee;
                stream >> reqType;
                stream >> idx;


                DBG_Printf(DBG_INFO_L2, "%s, %s (0x%04X) - Sequence no.: %d\n",
                            qUtf8Printable(apsData), qUtf8Printable(matchCl.value), matchCl.key, seq);
            }
                break;

            case ZDP_MGMT_LQI_RSP_CLID:
            {
                quint8 neighEntries;
                quint8 startIndex;
                quint8 listCount;
                QDataStream stream(ind.asdu());
                stream.setByteOrder(QDataStream::LittleEndian);

                stream >> seq;
                stream >> status;
                stream >> neighEntries;
                stream >> startIndex;
                stream >> listCount;

                DBG_Printf(DBG_INFO_L2, "%s, %s (0x%04X) - Sequence no.: %d\n",
                            qUtf8Printable(apsData), qUtf8Printable(matchCl.value), matchCl.key, seq);
            }
                break;

            case ZDP_MGMT_BIND_RSP_CLID:
            {
                QDataStream stream(ind.asdu());
                stream.setByteOrder(QDataStream::LittleEndian);

                stream >> seq;
                stream >> status;
                // Potentially following data is not yet picked up here

                const auto match = matchKeyFromMap1(status, zclStatusCodes);

                DBG_Printf(DBG_INFO_L2, "%s, %s (0x%04X) - Sequence no.: %d, Status: %s\n",
                            qUtf8Printable(apsData), qUtf8Printable(matchCl.value), matchCl.key, seq, qUtf8Printable(match.value));
            }
                break;

            case ZDP_BIND_RSP_CLID:
            {
                QDataStream stream(ind.asdu());
                stream.setByteOrder(QDataStream::LittleEndian);

                stream >> seq;
                stream >> status;

                const auto match = matchKeyFromMap1(status, zclStatusCodes);

                DBG_Printf(DBG_INFO_L2, "%s, %s (0x%04X) - Sequence no.: %d, Status: %s\n",
                            qUtf8Printable(apsData), qUtf8Printable(matchCl.value), matchCl.key, seq, qUtf8Printable(match.value));
            }
                break;

            case ZDP_UNBIND_RSP_CLID:
            {
                QDataStream stream(ind.asdu());
                stream.setByteOrder(QDataStream::LittleEndian);

                stream >> seq;
                stream >> status;

                const auto match = matchKeyFromMap1(status, zclStatusCodes);

                DBG_Printf(DBG_INFO_L2, "%s, %s (0x%04X) - Sequence no.: %d, Status: %s\n",
                            qUtf8Printable(apsData), qUtf8Printable(matchCl.value), matchCl.key, seq, qUtf8Printable(match.value));
            }
                break;

            case ZDP_MGMT_LEAVE_RSP_CLID:
            {
                QDataStream stream(ind.asdu());
                stream.setByteOrder(QDataStream::LittleEndian);

                stream >> seq;
                stream >> status;

                DBG_Printf(DBG_INFO_L2, "%s, %s (0x%04X) - Sequence no.: %d, Status: 0x%02X\n",
                            qUtf8Printable(apsData), qUtf8Printable(matchCl.value), matchCl.key, seq);
            }
                break;

            default:
                break;
        }
    }
    else
    {
        deCONZ::ZclFrame zclFrame; // dummy

        {
            QDataStream stream(ind.asdu());
            stream.setByteOrder(QDataStream::LittleEndian);
            zclFrame.readFromStream(stream);
        }

        //DBG_Printf(DBG_INFO_L2, "[INDICATION] - 0x%016llX (0x%04X) - ZCL payload (size %d): %s\n", ind.srcAddress().ext(), ind.srcAddress().nwk(), zclFrame.payload().size(),
        //        qPrintable(zclFrame.payload().toHex()));

        if (zclFrame.isProfileWideCommand())
        {
            //DBG_Printf(DBG_INFO_L2, "[INDICATION] - 0x%016llX (0x%04X) - Response command: %02X - Sequence no.: %d, Mfc: 0x%04X, Status: 0x%02X\n",
            //        ind.srcAddress().ext(), ind.srcAddress().nwk(), zclFrame.commandId(), zclFrame.sequenceNumber(), zclFrame.manufacturerCode(), zclFrame.defaultResponseStatus());

            const auto matchCmd = matchValueFromMap1(zclFrame.commandId(), zclGeneralCommandIds);

            switch (matchCmd.value)
            {
                case deCONZ::ZclGeneralCommandId::ZclReadAttributesResponseId:
                case deCONZ::ZclGeneralCommandId::ZclReportAttributesId:
                {
                    dbgReportAttributesAndReadAttributesRsp(zclFrame, matchCmd, apsData);
                }
                    break;

                case deCONZ::ZclGeneralCommandId::ZclConfigureReportingResponseId:
                {
                    dbgConfigureReportingRsp(zclFrame, matchCmd, apsData);
                }
                    break;

                case deCONZ::ZclGeneralCommandId::ZclReadReportingConfigResponseId:
                {
                    dbgReadReportingConfigRsp(zclFrame, matchCmd, apsData);
                }
                    break;

                case deCONZ::ZclGeneralCommandId::ZclWriteAttributesResponseId:
                {
                    dbgWriteAttributesRsp(zclFrame, matchCmd, apsData);
                }
                    break;

                case deCONZ::ZclGeneralCommandId::ZclReadAttributesId:
                {
                    dbgReadAttributes(zclFrame, matchCmd, apsData);
                }
                    break;

                case deCONZ::ZclGeneralCommandId::ZclConfigureReportingId:
                {
                    dbgConfigureReporting(zclFrame, matchCmd, apsData);
                }
                break;

                case deCONZ::ZclGeneralCommandId::ZclReadReportingConfigId:
                {
                    dbgReadReportingConfig(zclFrame, matchCmd, apsData);
                }
                    break;

                case deCONZ::ZclGeneralCommandId::ZclWriteAttributesId:
                {
                    dbgWriteAttributes(zclFrame, matchCmd, apsData);
                }
                    break;

                case deCONZ::ZclGeneralCommandId::ZclWriteAttributesUndividedId:
                case deCONZ::ZclGeneralCommandId::ZclWriteAttributesNoResponseId:
                case deCONZ::ZclGeneralCommandId::ZclDefaultResponseId:
                case deCONZ::ZclGeneralCommandId::ZclDiscoverAttributesId:
                case deCONZ::ZclGeneralCommandId::ZclDiscoverAttributesResponseId:
                case deCONZ::ZclGeneralCommandId::ZclWriteAttributesStructuredId:
                case deCONZ::ZclGeneralCommandId::ZclWriteAttributesStructuredResponseId:
                {
//                    DBG_Printf(DBG_INFO_L2, "[INDICATION] - 0x%016llX (0x%04X) - %s (0x%02X) - Sequence no.: %d, Mfc: 0x%04X, Status: 0x%02X, PL-Size: %u, ZCL-Payload: 0x%s\n",
//                            ind.srcAddress().ext(), ind.srcAddress().nwk(), qUtf8Printable(matchCmd.key), matchCmd.value, zclFrame.sequenceNumber(), zclFrame.manufacturerCode(), zclFrame.defaultResponseStatus(),
//                            zclFrame.payload().size(), qPrintable(zclFrame.payload().toHex().toUpper()));

                    dbgRareGeneralCommand(zclFrame, matchCmd, apsData);

//                    DBG_Printf(DBG_INFO_L2, "%s, %s (0x%02X), SeqNo.: %d, Mfc: 0x%04X, Status: 0x%02X, PL-Size: %u, ZCL-Payload: 0x%s\n",
//                            qUtf8Printable(apsData), qUtf8Printable(matchCmd.key), matchCmd.value, zclFrame.sequenceNumber(), zclFrame.manufacturerCode(), zclFrame.defaultResponseStatus(),
//                            zclFrame.payload().size(), qPrintable(zclFrame.payload().toHex().toUpper()));
                }
                    break;

                default:
                    break;
            }
        }
        else if (zclFrame.isClusterCommand())
        {
//            DBG_Printf(DBG_INFO_L2, "[INDICATION] - 0x%016llX (0x%04X) - Cluster command: 0x%02X - Sequence no.: %d, Mfc: 0x%04X, Status: 0x%02X, PL-Size: %u, ZCL-Payload: 0x%s\n",
//                    ind.srcAddress().ext(), ind.srcAddress().nwk(), zclFrame.commandId(), zclFrame.sequenceNumber(), zclFrame.manufacturerCode(), zclFrame.defaultResponseStatus(), zclFrame.payload().size(),
//                    qPrintable(zclFrame.payload().toHex().toUpper()));

            dbgClusterCommand(zclFrame, apsData);

//            DBG_Printf(DBG_INFO_L2, "%s, Cluster command: 0x%02X, SeqNo.: %d, Mfc: 0x%04X, Status: 0x%02X, PL-Size: %u, ZCL-Payload: 0x%s\n",
//                    qUtf8Printable(apsData), zclFrame.commandId(), zclFrame.sequenceNumber(), zclFrame.manufacturerCode(), zclFrame.defaultResponseStatus(), zclFrame.payload().size(),
//                    qPrintable(zclFrame.payload().toHex().toUpper()));
        }
    }
}

void dbgRareGeneralCommand(const deCONZ::ZclFrame &zclFrame, const KeyValMap &matchCmd, const QString &apsData)
{
    QString data;

    data += apsData + QLatin1String(", ") + matchCmd.key + QLatin1String(" (0x") + QString("%1").arg(matchCmd.value, 2, 16, QLatin1Char('0')).toUpper() +
            QLatin1String("), SeqNo.: ") + QString::number(zclFrame.sequenceNumber()) + QLatin1String(", Mfc: 0x") +
            QString("%1").arg(zclFrame.manufacturerCode(), 4, 16, QLatin1Char('0')).toUpper() + QLatin1String(", PL-Size: ") +
            QString::number(zclFrame.payload().size()) + QLatin1String(", ZCL-Payload: 0x") + QString(zclFrame.payload().toHex().toUpper());

    printDebugMessage(data);
}

void dbgClusterCommand(const deCONZ::ZclFrame &zclFrame, const QString &apsData)
{
    QString data;

    data += apsData + QLatin1String(", Cluster command: 0x") + QString("%1").arg(zclFrame.commandId(), 2, 16, QLatin1Char('0')).toUpper() +
            QLatin1String(", SeqNo.: ") + QString::number(zclFrame.sequenceNumber()) + QLatin1String(", Mfc: 0x") +
            QString("%1").arg(zclFrame.manufacturerCode(), 4, 16, QLatin1Char('0')).toUpper() + QLatin1String(", PL-Size: ") +
            QString::number(zclFrame.payload().size()) + QLatin1String(", ZCL-Payload: 0x") + QString(zclFrame.payload().toHex().toUpper());

    printDebugMessage(data);
}

void dbgWriteAttributes(const deCONZ::ZclFrame &zclFrame, const KeyValMap &matchCmd, const QString &apsData)
{
    QString data;

    QDataStream stream(zclFrame.payload());
    stream.setByteOrder(QDataStream::LittleEndian);

    data += apsData + QLatin1String(", ") + matchCmd.key + QLatin1String(" (0x") + QString("%1").arg(matchCmd.value, 2, 16, QLatin1Char('0')).toUpper() +
            QLatin1String("), SeqNo.: ") + QString::number(zclFrame.sequenceNumber()) + QLatin1String(", Mfc: 0x") +
            QString("%1").arg(zclFrame.manufacturerCode(), 4, 16, QLatin1Char('0')).toUpper();

    while (!stream.atEnd())
    {
        quint16 attributeId;
        quint8 attributeType;

        stream >> attributeId;
        stream >> attributeType;

        deCONZ::ZclAttribute attr(attributeId, attributeType, QLatin1String(""), deCONZ::ZclRead, false);

        if (!attr.readFromStream(stream))
        {
            continue;
        }

        const auto match = matchValueFromMap1(attr.dataType(), zclDataTypes);

        data += QLatin1String(", Attr: 0x") + QString("%1").arg(attr.id(), 4, 16, QLatin1Char('0')).toUpper();
        //data += QLatin1String("Datatype: 0x") + QString("%1").arg(attr.dataType(), 2, 16, QLatin1Char('0')).toUpper() + ", ";
        data += QLatin1String(", Datatype: ") + match.key;
        data += QLatin1String(", Value: ") + attr.toVariant().toString() + QLatin1String(" (0x") + attr.toVariant().toString().toUpper() + QLatin1String(")");
    }

    printDebugMessage(data);

//    DBG_Printf(DBG_INFO_L2, "%s, %s (0x%02X), SeqNo.: %d, Mfc: 0x%04X%s\n",
//            qUtf8Printable(apsData), qUtf8Printable(matchCmd.key), matchCmd.value, zclFrame.sequenceNumber(), zclFrame.manufacturerCode(), qUtf8Printable(data));
}

void dbgWriteAttributesRsp(const deCONZ::ZclFrame &zclFrame, const KeyValMap &matchCmd, const QString &apsData)
{
    QString data;

    QDataStream stream(zclFrame.payload());
    stream.setByteOrder(QDataStream::LittleEndian);

    data += apsData + QLatin1String(", ") + matchCmd.key + QLatin1String(" (0x") + QString("%1").arg(matchCmd.value, 2, 16, QLatin1Char('0')).toUpper() +
            QLatin1String("), SeqNo.: ") + QString::number(zclFrame.sequenceNumber()) + QLatin1String(", Mfc: 0x") +
            QString("%1").arg(zclFrame.manufacturerCode(), 4, 16, QLatin1Char('0')).toUpper();

    while (!stream.atEnd())
    {
        quint8 status;

        stream >> status;

        const auto match = matchKeyFromMap1(status, zclStatusCodes);

        data += QLatin1String(", Status: ") + match.value;
        if (status != deCONZ::ZclSuccessStatus)
        {
            quint16 attributeId;

            stream >> attributeId;

            data += QLatin1String(", Attr: 0x") + QString("%1").arg(attributeId, 2, 16, QLatin1Char('0')).toUpper();

            continue;
        }
    }

    printDebugMessage(data);

//    DBG_Printf(DBG_INFO_L2, "%s, %s (0x%02X), SeqNo.: %d, Mfc: 0x%04X%s\n",
//            qUtf8Printable(apsData), qUtf8Printable(matchCmd.key), matchCmd.value, zclFrame.sequenceNumber(), zclFrame.manufacturerCode(), qUtf8Printable(data));
}

void dbgReportAttributesAndReadAttributesRsp(const deCONZ::ZclFrame &zclFrame, const KeyValMap &matchCmd, const QString &apsData)
{
    QString data;

    QDataStream stream(zclFrame.payload());
    stream.setByteOrder(QDataStream::LittleEndian);

    data += apsData + QLatin1String(", ") + matchCmd.key + QLatin1String(" (0x") + QString("%1").arg(matchCmd.value, 2, 16, QLatin1Char('0')).toUpper() +
            QLatin1String("), SeqNo.: ") + QString::number(zclFrame.sequenceNumber()) + QLatin1String(", Mfc: 0x") +
            QString("%1").arg(zclFrame.manufacturerCode(), 4, 16, QLatin1Char('0')).toUpper();

    while (!stream.atEnd())
    {
        quint16 attributeId;
        quint8 attributeType;

        stream >> attributeId;
        if (matchCmd.value == deCONZ::ZclGeneralCommandId::ZclReadAttributesResponseId)
        {
            quint8 status;
            stream >> status;

            const auto match = matchKeyFromMap1(status, zclStatusCodes);

            data += QLatin1String(", Status: ") + match.value;
            if (status != deCONZ::ZclSuccessStatus)
            {
                data += QLatin1String(", Attr: 0x") + QString("%1").arg(attributeId, 4, 16, QLatin1Char('0')).toUpper();
                continue;
            }
        }
        stream >> attributeType;

        deCONZ::ZclAttribute attr(attributeId, attributeType, QLatin1String(""), deCONZ::ZclRead, false);

        if (!attr.readFromStream(stream))
        {
            continue;
        }

        const auto match = matchValueFromMap1(attr.dataType(), zclDataTypes);

        data += QLatin1String(", Attr: 0x") + QString("%1").arg(attr.id(), 4, 16, QLatin1Char('0')).toUpper();
        data += QLatin1String(", Datatype: ") + match.key;

        if (match.value == 0x38 || match.value == 0x39 || match.value == 0x3A) // Floats
        {
            QString val;
            val.setNum(attr.numericValue().real);
            data += QLatin1String(", Value: ") + val;
        }
        else if (match.value == 0x41 || match.value == 0x42 || match.value == 0x43 || match.value == 0x44) // Strings
        {
            data += QLatin1String(", Value: ") + attr.toVariant().toString();
            data += QLatin1String(" (0x") + attr.toVariant().toByteArray().toHex() + QLatin1String(")");
        }
        //else if (match.value == 0xE0 || match.value == 0xE1 || match.value == 0xE2) // Time / Date
        else if (match.value == 0xE2) // UTC
        {
            QDateTime epoch = QDateTime(QDate(2000, 1, 1), QTime(0, 0), Qt::UTC);
            epoch = epoch.addSecs(attr.toVariant().toInt());

            data += QLatin1String(", Value: ") + epoch.toString() + QLatin1String(" (") + QString::number(attr.toVariant().toInt(), 10) + QLatin1String(")");
        }
        else
        {
            QString val;
            val.setNum(attr.numericValue().s64);

            data += QLatin1String(", Value: ") + val + QLatin1String(" (0x") + QString::number(val.toLongLong(), 16).toUpper() + QLatin1String(")");
        }
    }

    printDebugMessage(data);

//    DBG_Printf(DBG_INFO_L2, "%s, %s (0x%02X), SeqNo.: %d, Mfc: 0x%04X%s\n",
//            qUtf8Printable(apsData), qUtf8Printable(matchCmd.key), matchCmd.value, zclFrame.sequenceNumber(), zclFrame.manufacturerCode(), qUtf8Printable(data));
}

void dbgReadAttributes(const deCONZ::ZclFrame &zclFrame, const KeyValMap &matchCmd, const QString &apsData)
{
    QString data;

    QDataStream stream(zclFrame.payload());
    stream.setByteOrder(QDataStream::LittleEndian);

    data += apsData + QLatin1String(", ") + matchCmd.key + QLatin1String(" (0x") + QString("%1").arg(matchCmd.value, 2, 16, QLatin1Char('0')).toUpper() +
            QLatin1String("), SeqNo.: ") + QString::number(zclFrame.sequenceNumber()) + QLatin1String(", Mfc: 0x") +
            QString("%1").arg(zclFrame.manufacturerCode(), 4, 16, QLatin1Char('0')).toUpper();

    data += QLatin1String(", Attributes: ");

    while (!stream.atEnd())
    {
        quint16 attributeId;

        stream >> attributeId;

        data += QLatin1String("0x") + QString("%1").arg(attributeId, 4, 16, QLatin1Char('0')).toUpper();
        data += QLatin1String(", ");
    }

    printDebugMessage(data);

//    DBG_Printf(DBG_INFO_L2, "%s, %s (0x%02X), SeqNo.: %d, Mfc: 0x%04X%s\n",
//                qUtf8Printable(apsData), qUtf8Printable(matchCmd.key), matchCmd.value, zclFrame.sequenceNumber(), zclFrame.manufacturerCode(), qUtf8Printable(data));
}

void dbgReadReportingConfig(const deCONZ::ZclFrame &zclFrame, const KeyValMap &matchCmd, const QString &apsData)
{
    QString data;

    QDataStream stream(zclFrame.payload());
    stream.setByteOrder(QDataStream::LittleEndian);

    data += apsData + QLatin1String(", ") + matchCmd.key + QLatin1String(" (0x") + QString("%1").arg(matchCmd.value, 2, 16, QLatin1Char('0')).toUpper() +
            QLatin1String("), SeqNo.: ") + QString::number(zclFrame.sequenceNumber()) + QLatin1String(", Mfc: 0x") +
            QString("%1").arg(zclFrame.manufacturerCode(), 4, 16, QLatin1Char('0')).toUpper();

    while (!stream.atEnd())
    {
        quint8 direction;
        quint16 attributeId;

        stream >> direction;
        stream >> attributeId;

        data += QLatin1String(", Dir: 0x") + QString("%1").arg(direction, 2, 16, QLatin1Char('0')).toUpper();
        data += QLatin1String(", Attr: 0x") + QString("%1").arg(attributeId, 4, 16, QLatin1Char('0')).toUpper();
    }

    printDebugMessage(data);

//    DBG_Printf(DBG_INFO_L2, "%s, %s (0x%02X), SeqNo.: %d, Mfc: 0x%04X%s\n",
//                qUtf8Printable(apsData), qUtf8Printable(matchCmd.key), matchCmd.value, zclFrame.sequenceNumber(), zclFrame.manufacturerCode(), qUtf8Printable(data));
}

void dbgReadReportingConfigRsp(const deCONZ::ZclFrame &zclFrame, const KeyValMap &matchCmd, const QString &apsData)
{
    QString data;

    QDataStream stream(zclFrame.payload());
    stream.setByteOrder(QDataStream::LittleEndian);

    data += apsData + QLatin1String(", ") + matchCmd.key + QLatin1String(" (0x") + QString("%1").arg(matchCmd.value, 2, 16, QLatin1Char('0')).toUpper() +
            QLatin1String("), SeqNo.: ") + QString::number(zclFrame.sequenceNumber()) + QLatin1String(", Mfc: 0x") +
            QString("%1").arg(zclFrame.manufacturerCode(), 4, 16, QLatin1Char('0')).toUpper();

    while (!stream.atEnd())
    {
        quint8 status;
        quint8 direction;
        quint16 attributeId;

        stream >> status;
        stream >> direction;
        stream >> attributeId;

        const auto match = matchKeyFromMap1(status, zclStatusCodes);

        data += QLatin1String(", Status: ") + match.value;
        data += QLatin1String(", Dir: ") + QString("0x%1").arg(direction, 2, 16, QLatin1Char('0')).toUpper();
        data += QLatin1String(", Attr: ") + QString("0x%1").arg(attributeId, 4, 16, QLatin1Char('0')).toUpper();

        if (status == deCONZ::ZclSuccessStatus) // successful
        {
            quint8 attributeType;
            quint16 min;
            quint16 max;

            stream >> attributeType;

            deCONZ::ZclAttribute attr(attributeId, attributeType, QLatin1String(""), deCONZ::ZclRead, false);

            stream >> min;
            stream >> max;
            attr.setMinReportInterval(min);
            attr.setMaxReportInterval(max);

            if (!attr.readReportableChangeFromStream(stream))
            {
                continue;
            }

            const auto match = matchValueFromMap1(attr.dataType(), zclDataTypes);

            data += QLatin1String(", Datatype: ") + match.key;
            data += QLatin1String(", Min: ") + QString::number(attr.minReportInterval());
            data += QLatin1String(", Max: ") + QString::number(attr.maxReportInterval());
            data += QLatin1String(", Change: ") + QString::number(attr.reportableChange().s32);
        }
    }

    printDebugMessage(data);

//    DBG_Printf(DBG_INFO_L2, "%s, %s (0x%02X), SeqNo.: %d, Mfc: 0x%04X%s\n",
//            qUtf8Printable(apsData), qUtf8Printable(matchCmd.key), matchCmd.value, zclFrame.sequenceNumber(), zclFrame.manufacturerCode(), qUtf8Printable(data));
}

void dbgConfigureReporting(const deCONZ::ZclFrame &zclFrame, const KeyValMap &matchCmd, const QString &apsData)
{
    QString data;

    QDataStream stream(zclFrame.payload());
    stream.setByteOrder(QDataStream::LittleEndian);

    data += apsData + QLatin1String(", ") + matchCmd.key + QLatin1String(" (0x") + QString("%1").arg(matchCmd.value, 2, 16, QLatin1Char('0')).toUpper() +
            QLatin1String("), SeqNo.: ") + QString::number(zclFrame.sequenceNumber()) + QLatin1String(", Mfc: 0x") +
            QString("%1").arg(zclFrame.manufacturerCode(), 4, 16, QLatin1Char('0')).toUpper();

    while (!stream.atEnd())
    {
        quint8 direction;
        quint16 attributeId;
        quint8 attributeType;
        quint16 min;
        quint16 max;

        stream >> direction;
        stream >> attributeId;
        stream >> attributeType;

        deCONZ::ZclAttribute attr(attributeId, attributeType, QLatin1String(""), deCONZ::ZclRead, false);

        stream >> min;
        stream >> max;
        attr.setMinReportInterval(min);
        attr.setMaxReportInterval(max);

        if (!attr.readReportableChangeFromStream(stream))
        {
            continue;
        }

        const auto match = matchValueFromMap1(attr.dataType(), zclDataTypes);

        data += QLatin1String(", Attr: ") + QString("0x%1").arg(attr.id(), 4, 16, QLatin1Char('0')).toUpper();
        data += QLatin1String(", Datatype: ") + match.key;
        data += QLatin1String(", Min: ") + QString::number(attr.minReportInterval());
        data += QLatin1String(", Max: ") + QString::number(attr.maxReportInterval());
        data += QLatin1String(", Change: ") + QString::number(attr.reportableChange().s64);
    }

    printDebugMessage(data);

//    DBG_Printf(DBG_INFO_L2, "%s, %s (0x%02X), SeqNo.: %d, Mfc: 0x%04X%s\n",
//                qUtf8Printable(apsData), qUtf8Printable(matchCmd.key), matchCmd.value, zclFrame.sequenceNumber(), zclFrame.manufacturerCode(), qUtf8Printable(data));
}

void dbgConfigureReportingRsp(const deCONZ::ZclFrame &zclFrame, const KeyValMap &matchCmd, const QString &apsData)
{
    QString data;

    QDataStream stream(zclFrame.payload());
    stream.setByteOrder(QDataStream::LittleEndian);

    data += apsData + QLatin1String(", ") + matchCmd.key + QLatin1String(" (0x") + QString("%1").arg(matchCmd.value, 2, 16, QLatin1Char('0')).toUpper() +
            QLatin1String("), SeqNo.: ") + QString::number(zclFrame.sequenceNumber()) + QLatin1String(", Mfc: 0x") +
            QString("%1").arg(zclFrame.manufacturerCode(), 4, 16, QLatin1Char('0')).toUpper();

    while (!stream.atEnd())
    {
        quint8 status;

        stream >> status;

        const auto match = matchKeyFromMap1(status, zclStatusCodes);

        data += QLatin1String(", Status: ") + match.value;

        if (status != 0x00) // not successful
        {
            quint8 direction;
            quint16 attributeId;

            stream >> direction;
            stream >> attributeId;

            data += QLatin1String(", Dir: ") + QString("%1").arg(direction, 2, 16, QLatin1Char('0')).toUpper();
            data += QLatin1String(", Attr: ") + QString("%1").arg(attributeId, 4, 16, QLatin1Char('0')).toUpper();
        }
    }

    printDebugMessage(data);

//    DBG_Printf(DBG_INFO_L2, "%s, %s (0x%02X), SeqNo.: %d, Mfc: 0x%04X%s\n",
//            qUtf8Printable(apsData), qUtf8Printable(matchCmd.key), matchCmd.value, zclFrame.sequenceNumber(), zclFrame.manufacturerCode(), qUtf8Printable(data));
}

void printDebugMessage(const QString &debugMessage)
{
    DBG_Printf(DBG_INFO_L2, "%s\n", qUtf8Printable(debugMessage));
}
