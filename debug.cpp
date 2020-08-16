#include "de_web_plugin.h"
#include "de_web_plugin_private.h"

void DeRestPluginPrivate::handleDebugging(const deCONZ::ApsDataRequest &req)
{
    //bool found;
       
    DBG_Printf(DBG_INFO, "[ZB REQUEST] - Sending data to node: 0x%016llX (%04X)\n", req.dstAddress().ext(), req.dstAddress().nwk());
    DBG_Printf(DBG_INFO, "[ZB REQUEST] - 0x%016llX (%04X) - Request for Profile: 0x%04X, Cluster: %04X, Endpoint: %d\n", req.dstAddress().ext(), req.dstAddress().nwk(),
                        req.profileId(), req.clusterId(), req.dstEndpoint());
    DBG_Printf(DBG_INFO, "[ZB REQUEST] - 0x%016llX (%04X) - Request ASDU: %s\n", req.dstAddress().ext(), req.dstAddress().nwk(), qPrintable(req.asdu().toHex()));
    
    if (req.profileId() == ZDP_PROFILE_ID)
    {
        quint8 seq;
        quint16 nwk;
        
        switch (req.clusterId())
        {
            case ZDP_NODE_DESCRIPTOR_CLID:
            {
                QDataStream stream(req.asdu());
                stream.setByteOrder(QDataStream::LittleEndian);

                stream >> seq;
                stream >> nwk;
                
                DBG_Printf(DBG_INFO, "[ZB REQUEST] - 0x%016llX (%04X) - Request node descriptor - Sequence no.: %d, NWK: 0x%04X\n",
                            req.dstAddress().ext(), req.dstAddress().nwk(), seq, nwk);
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

                DBG_Printf(DBG_INFO, "[ZB REQUEST] - 0x%016llX (%04X) - Request simple descriptor - Sequence no.: %d, NWK: 0x%04X, Endpoint: %02X\n",
                            req.dstAddress().ext(), req.dstAddress().nwk(), seq, nwk, ep);
            }
                break;
                
            case ZDP_ACTIVE_ENDPOINTS_CLID:
            {
                QDataStream stream(req.asdu());
                stream.setByteOrder(QDataStream::LittleEndian);

                stream >> seq;
                stream >> nwk;
                
                DBG_Printf(DBG_INFO, "[ZB REQUEST] - 0x%016llX (%04X) - Request active endpoints - Sequence no.: %d, NWK: 0x%04X\n",
                            req.dstAddress().ext(), req.dstAddress().nwk(), seq, nwk);
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
                DBG_Printf(DBG_INFO, "[ZB REQUEST] - 0x%016llX (%04X) - Send bind request - ASDU size: %d\n",
                            req.dstAddress().ext(), req.dstAddress().nwk(), req.asdu().size());
                
                if (req.asdu().size() == 15)
                {
                    quint16 dstGroup;
                    stream >> dstGroup;
                    
                    DBG_Printf(DBG_INFO, "[ZB REQUEST] - 0x%016llX (%04X) - Send bind request - Sequence no.: %d, SrcExt: 0x%016llX, srcEp: %02X, srcCluster: 0x%04X, adrMode: %02X, dstGroup: 0x%04X\n",
                            req.dstAddress().ext(), req.dstAddress().nwk(), seq, srcIeee, srcEp, srcCluster, adrMode, dstGroup);
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
                    
                    DBG_Printf(DBG_INFO, "[ZB REQUEST] - 0x%016llX (%04X) - Send bind request - Sequence no.: %d, srcExt: 0x%016llX, srcEp: %02X, srcCluster: 0x%04X, adrMode: %02X, dstExt: 0x%016llX, dstEp: %02X, dstCluster: 0x%04X\n",
                            req.dstAddress().ext(), req.dstAddress().nwk(), seq, srcIeee, srcEp, srcCluster, adrMode, dstIeee, dstEp, dstCluster);
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
                
                DBG_Printf(DBG_INFO, "[NEWDEBUG] - 0x%016llX (%04X) - Received MGMT bind response - Sequence no.: %d, Status: 0x%02X\n",
                            req.dstAddress().ext(), req.dstAddress().nwk(), seq, status);*/
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
        
        DBG_Printf(DBG_INFO, "[ZB REQUEST] - 0x%016llX (%04X) - ZCL payload (size %d): %s\n", req.dstAddress().ext(), req.dstAddress().nwk(),
                    zclFrame.payload().size(), qPrintable(zclFrame.payload().toHex()));
        
        if (zclFrame.isProfileWideCommand())
        {
            switch (zclFrame.commandId())
            {
                case deCONZ::ZclGeneralCommandId::ZclReadAttributesId:
                {
                    quint16 attribute;
                    QString attributes = "Attributes: ";
                    
                    QDataStream stream(zclFrame.payload());
                    stream.setByteOrder(QDataStream::LittleEndian);
                    
                    while (!stream.atEnd())
                    {
                        stream >> attribute;
                        attributes += QString("%1").arg(attribute, 4, 16, QLatin1Char('0')).toUpper();
                        attributes += ", ";
                    }
                    
                    DBG_Printf(DBG_INFO, "[ZB REQUEST] - 0x%016llX (%04X) - Request read attributes (%02X) - Sequence no.: %d, Mfc: 0x%04X, %s\n",
                                req.dstAddress().ext(), req.dstAddress().nwk(), zclFrame.commandId(), zclFrame.sequenceNumber(), zclFrame.manufacturerCode(), attributes.toUtf8().data());
                }
                    break;

                case deCONZ::ZclGeneralCommandId::ZclWriteAttributesId:
                {
                    quint16 attribute;
                    quint8 dt;
                    quint8 data;
                    QString bla;
                    
                    QDataStream stream(zclFrame.payload());
                    stream.setByteOrder(QDataStream::LittleEndian);
                    
                    stream >> attribute;
                    stream >> dt;
                    
                    while (!stream.atEnd())
                    {
                        stream >> data;
                        bla += QString("%1").arg(data, 2, 16, QLatin1Char('0')).toUpper();
                    }
                    
                    DBG_Printf(DBG_INFO, "[ZB REQUEST] - 0x%016llX (%04X) - Request read attributes (%02X) - Sequence no.: %d, Mfc: 0x%04X, Attribute: %04X, Datatype: %02X, %s\n",
                                req.dstAddress().ext(), req.dstAddress().nwk(), zclFrame.commandId(), zclFrame.sequenceNumber(), zclFrame.manufacturerCode(), attribute, dt, bla.toUtf8().data());
                }
                    break;
                    
                case deCONZ::ZclGeneralCommandId::ZclReadReportingConfigId:
                {
                    quint8 direction;
                    quint16 attribute;
                    
                    QDataStream stream(zclFrame.payload());
                    stream.setByteOrder(QDataStream::LittleEndian);
                    
                    stream >> direction;
                    stream >> attribute;
                    
                    DBG_Printf(DBG_INFO, "[ZB REQUEST] - 0x%016llX (%04X) - Request read reporting config (%02X) - Sequence no.: %d, Mfc: 0x%04X, Attribute: %04X\n",
                                req.dstAddress().ext(), req.dstAddress().nwk(), zclFrame.commandId(), zclFrame.sequenceNumber(), zclFrame.manufacturerCode(), attribute);
                    
                }
                    break;
                    
                case deCONZ::ZclGeneralCommandId::ZclConfigureReportingId:
                {
                    quint8 direction;
                    quint16 attribute;
                    quint8 dt;
                    quint16 min;
                    quint16 max;
                    quint8 data;
                    QString bla;
                    
                    QDataStream stream(zclFrame.payload());
                    stream.setByteOrder(QDataStream::LittleEndian);
                    
                    stream >> direction;
                    stream >> attribute;
                    stream >> dt;
                    stream >> min;
                    stream >> max;
                    //stream >> change;
                    
                    while (!stream.atEnd())
                    {
                        stream >> data;
                        bla += QString("%1").arg(data, 2, 16, QLatin1Char('0')).toUpper();
                    }
                    
                    DBG_Printf(DBG_INFO, "[ZB REQUEST] - 0x%016llX (%04X) - Request configure reporting (%02X) - Sequence no.: %d, Mfc: 0x%04X, Attribute: %04X, Datatype: %02X, Min: %d, Max: %d, Change: %s\n",
                                req.dstAddress().ext(), req.dstAddress().nwk(), zclFrame.commandId(), zclFrame.sequenceNumber(), zclFrame.manufacturerCode(), attribute, dt, min, max, bla.toUtf8().data());
                    
                }
                    break;
                    
                case deCONZ::ZclGeneralCommandId::ZclReadAttributesResponseId:
                case deCONZ::ZclGeneralCommandId::ZclWriteAttributesUndividedId:
                case deCONZ::ZclGeneralCommandId::ZclWriteAttributesResponseId:
                case deCONZ::ZclGeneralCommandId::ZclWriteAttributesNoResponseId:
                case deCONZ::ZclGeneralCommandId::ZclConfigureReportingResponseId:
                case deCONZ::ZclGeneralCommandId::ZclReadReportingConfigResponseId:
                case deCONZ::ZclGeneralCommandId::ZclReportAttributesId:
                case deCONZ::ZclGeneralCommandId::ZclDefaultResponseId:
                case deCONZ::ZclGeneralCommandId::ZclDiscoverAttributesId:
                case deCONZ::ZclGeneralCommandId::ZclDiscoverAttributesResponseId:
                case deCONZ::ZclGeneralCommandId::ZclWriteAttributesStructuredId:
                case deCONZ::ZclGeneralCommandId::ZclWriteAttributesStructuredResponseId:
                {
                    DBG_Printf(DBG_INFO, "[ZB REQUEST] - 0x%016llX (%04X) - Request command: %02X - Sequence no.: %d, Mfc: 0x%04X\n",
                                        req.dstAddress().ext(), req.dstAddress().nwk(), zclFrame.commandId(), zclFrame.sequenceNumber(), zclFrame.manufacturerCode());
                }
                
                default:
                    break;
            }
        }
        else if (zclFrame.isClusterCommand())
        {
            DBG_Printf(DBG_INFO, "[ZB REQUEST] - 0x%016llX (%04X) - Request cluster command: %02X - Sequence no.: %d, Mfc: 0x%04X\n",
                                req.dstAddress().ext(), req.dstAddress().nwk(), zclFrame.commandId(), zclFrame.sequenceNumber(), zclFrame.manufacturerCode());
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
    DBG_Printf(DBG_INFO, "[ZB RESPONSE] - Incoming data from node: 0x%016llX (%04X)\n", ind.srcAddress().ext(), ind.srcAddress().nwk());
    
    QMap<int, QString> zclStatusCodes;
    zclStatusCodes.insert(0x00, "SUCCESS");
    zclStatusCodes.insert(0x01, "FAILURE");
    zclStatusCodes.insert(0x1C, "SOFTWARE_FAILURE");
    zclStatusCodes.insert(0x80, "MALFORMED_COMMAND");
    zclStatusCodes.insert(0x81, "UNSUP_CLUSTER_COMMAND");
    zclStatusCodes.insert(0x82, "UNSUP_GENERAL_COMMAND");
    zclStatusCodes.insert(0x83, "UNSUP_MANUF_CLUSTER_COMMAND");
    zclStatusCodes.insert(0x84, "UNSUP_MANUF_GENERAL_COMMAND");
    zclStatusCodes.insert(0x85, "INVALID_FIELD");
    zclStatusCodes.insert(0x86, "UNSUPPORTED_ATTRIBUTE");
    zclStatusCodes.insert(0x87, "INVALID_VALUE");
    zclStatusCodes.insert(0x88, "READ_ONLY");
    zclStatusCodes.insert(0x89, "INSUFFICIENT_SPACE");
    zclStatusCodes.insert(0x8A, "DUPLICATE_EXISTS");
    zclStatusCodes.insert(0x8B, "NOT_FOUND");
    zclStatusCodes.insert(0x8C, "UNREPORTABLE_ATTRIBUTE");
    zclStatusCodes.insert(0x8D, "INVALID_DATA_TYPE");
    zclStatusCodes.insert(0x8E, "INVALID_SELECTOR");
    zclStatusCodes.insert(0x8F, "WRITE_ONLY");
    zclStatusCodes.insert(0x90, "INCONSISTENT_STARTUP_STATE");
    zclStatusCodes.insert(0x91, "DEFINED_OUT_OF_BAND");

    DBG_Printf(DBG_INFO, "[ZB RESPONSE] - Incoming data from node: 0x%016llX (%04X)\n", ind.srcAddress().ext(), ind.srcAddress().nwk());
    DBG_Printf(DBG_INFO, "[ZB RESPONSE] - 0x%016llX (%04X) - Indication for Profile: 0x%04X, Cluster: %04X, Endpoint: %d, Status: 0x%02X\n", ind.srcAddress().ext(), ind.srcAddress().nwk(),
                        ind.profileId(), ind.clusterId(), ind.srcEndpoint(), ind.status());
    DBG_Printf(DBG_INFO, "[ZB RESPONSE] - 0x%016llX (%04X) - Indication ASDU: %s\n", ind.srcAddress().ext(), ind.srcAddress().nwk(), qPrintable(ind.asdu().toHex()));
    
    
    //DBG_Printf(DBG_INFO, "[FOR_ERIK] - %04X: resp: %d: 0x%016llX-%02X-%04X, payload (ASDU): %s\n", ind.srcAddress().nwk(), zclFrame.sequenceNumber(), ind.srcAddress().ext(),
                //ind.dstEndpoint(), ind.clusterId(), qPrintable(ind.asdu().toHex()));
    
    if (ind.profileId() == ZDP_PROFILE_ID)
    {
        quint8 seq;
        quint8 status;
        quint16 nwk;
        
        switch (ind.clusterId())
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
                
                DBG_Printf(DBG_INFO, "[ZB RESPONSE] - 0x%016llX (%04X) - Received node descriptor response - Sequence no.: %d, Node descriptor: 0x%s\n",
                            ind.srcAddress().ext(), ind.srcAddress().nwk(), seq, qPrintable(nd.toByteArray().toHex()));
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
                        inClusters += QString("%1").arg(cluster, 4, 16, QLatin1Char('0')).toUpper();
                        inClusters += ", ";
                    }
                }
                
                inClusters = inClusters.left(inClusters.lastIndexOf(QChar(',')));   // Strip everything after the last comma, for beauty
                stream >> outClusterCount;
                
                for (int i=0; i<outClusterCount; i++)
                {
                    if (!stream.atEnd())
                    {
                        stream >> cluster;
                        outClusters += QString("%1").arg(cluster, 4, 16, QLatin1Char('0')).toUpper();
                        outClusters += ", ";
                    }
                }
                
                outClusters = outClusters.left(outClusters.lastIndexOf(QChar(',')));    // Strip everything after the last comma, for beauty

                DBG_Printf(DBG_INFO, "[ZB RESPONSE] - 0x%016llX (%04X) - Received simple descriptor response - Sequence no.: %d\n",
                            ind.srcAddress().ext(), ind.srcAddress().nwk(), seq);
                DBG_Printf(DBG_INFO, "[ZB RESPONSE] - 0x%016llX (%04X) - Received simple descriptor response - Ep: %02X, Profile: 0x%04X, DeviceID: 0x%04X, Input clusters: %s, Output clusters: %s\n",
                            ind.srcAddress().ext(), ind.srcAddress().nwk(), ep, profile, appDevice, inClusters.toUtf8().data(), outClusters.toUtf8().data());
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
                        endpoints += QString("%1").arg(ep, 2, 16, QLatin1Char('0')).toUpper();
                        endpoints += ", ";
                    }
                }
                
                endpoints = endpoints.left(endpoints.lastIndexOf(QChar(',')));   // Strip everything after the last comma, for beauty
                
                DBG_Printf(DBG_INFO, "[ZB RESPONSE] - 0x%016llX (%04X) - Received active endpoint response - Sequence no.: %d, Active endpoints: %s\n",
                            ind.srcAddress().ext(), ind.srcAddress().nwk(), seq, endpoints.toUtf8().data());
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
                        inClusters += QString("%1").arg(cluster, 4, 16, QLatin1Char('0')).toUpper();
                        inClusters += ", ";
                    }
                }
                
                inClusters = inClusters.left(inClusters.lastIndexOf(QChar(',')));   // Strip everything after the last comma, for beauty
                
                for (int i=0; i<outClusterCount; i++)
                {
                    if (!stream.atEnd())
                    {
                        stream >> cluster;
                        outClusters += QString("%1").arg(cluster, 4, 16, QLatin1Char('0')).toUpper();
                        outClusters += ", ";
                    }
                }
                
                outClusters = outClusters.left(outClusters.lastIndexOf(QChar(',')));    // Strip everything after the last comma, for beauty

                DBG_Printf(DBG_INFO, "[ZB RESPONSE] - 0x%016llX (%04X) - Received match descriptor request - Sequence no.: %d, Profile: 0x%04X, Input clusters: %s, Output clusters: %s\n\n",
                        ind.srcAddress().ext(), ind.srcAddress().nwk(), seq, profile, inClusters.toUtf8().data(), outClusters.toUtf8().data());

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
                
                DBG_Printf(DBG_INFO, "[ZB RESPONSE] - 0x%016llX (%04X) - Received device announcement - Sequence no.: %d, MAC capabilities: 0x%02X\n",
                            ind.srcAddress().ext(), ind.srcAddress().nwk(), seq, macCapabilities);
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
                
                DBG_Printf(DBG_INFO, "[ZB RESPONSE] - 0x%016llX (%04X) - Received IEEE address request - Sequence no.: %d\n",
                            ind.srcAddress().ext(), ind.srcAddress().nwk(), seq);
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
                
                
                DBG_Printf(DBG_INFO, "[ZB RESPONSE] - 0x%016llX (%04X) - Received NWK address request - Sequence no.: %d\n",
                            ind.srcAddress().ext(), ind.srcAddress().nwk(), seq);
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
                
                DBG_Printf(DBG_INFO, "[ZB RESPONSE] - 0x%016llX (%04X) - Received MGMT LQI response - Sequence no.: %d\n",
                            ind.srcAddress().ext(), ind.srcAddress().nwk(), seq);
            }
                break;

            case ZDP_MGMT_BIND_RSP_CLID:
            {
                QDataStream stream(ind.asdu());
                stream.setByteOrder(QDataStream::LittleEndian);

                stream >> seq;
                stream >> status;
                // Potentially following data is not yet picked up here
                
                DBG_Printf(DBG_INFO, "[ZB RESPONSE] - 0x%016llX (%04X) - Received MGMT bind response - Sequence no.: %d, Status: 0x%02X\n",
                            ind.srcAddress().ext(), ind.srcAddress().nwk(), seq, status);
            }
                break;

            case ZDP_BIND_RSP_CLID:
            {
                QDataStream stream(ind.asdu());
                stream.setByteOrder(QDataStream::LittleEndian);

                stream >> seq;
                stream >> status;
                
                DBG_Printf(DBG_INFO, "[ZB RESPONSE] - 0x%016llX (%04X) - Received bind response - Sequence no.: %d, Status: 0x%02X\n",
                            ind.srcAddress().ext(), ind.srcAddress().nwk(), seq, status);
            }
                break;

            case ZDP_UNBIND_RSP_CLID:
            {
                QDataStream stream(ind.asdu());
                stream.setByteOrder(QDataStream::LittleEndian);

                stream >> seq;
                stream >> status;
                
                DBG_Printf(DBG_INFO, "[ZB RESPONSE] - 0x%016llX (%04X) - Received unbind response - Sequence no.: %d, Status: 0x%02X\n",
                            ind.srcAddress().ext(), ind.srcAddress().nwk(), seq, status);
            }
                break;

            case ZDP_MGMT_LEAVE_RSP_CLID:
            {
                QDataStream stream(ind.asdu());
                stream.setByteOrder(QDataStream::LittleEndian);

                stream >> seq;
                stream >> status;
                
                DBG_Printf(DBG_INFO, "[ZB RESPONSE] - 0x%016llX (%04X) - Received MGMT leave response - Sequence no.: %d, Status: 0x%02X\n",
                            ind.srcAddress().ext(), ind.srcAddress().nwk(), seq);
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
        
        DBG_Printf(DBG_INFO, "[ZB RESPONSE] - 0x%016llX (%04X) - ZCL payload (size %d): %s\n", ind.srcAddress().ext(), ind.srcAddress().nwk(), zclFrame.payload().size(),
                qPrintable(zclFrame.payload().toHex()));
        
        if (zclFrame.isProfileWideCommand())
        {
            //DBG_Printf(DBG_INFO, "[ZB RESPONSE] - 0x%016llX (%04X) - Response command: %02X - Sequence no.: %d, Mfc: 0x%04X, Status: 0x%02X\n",
            //        ind.srcAddress().ext(), ind.srcAddress().nwk(), zclFrame.commandId(), zclFrame.sequenceNumber(), zclFrame.manufacturerCode(), zclFrame.defaultResponseStatus());
            
            switch (zclFrame.commandId())
            {
                case deCONZ::ZclGeneralCommandId::ZclReadAttributesResponseId:
                case deCONZ::ZclGeneralCommandId::ZclReportAttributesId:
                {
                    quint16 attribute;
                    quint8 status = 0xFF;
                    quint8 datatype;
                    quint8 data;
                    QString bla;
                    
                    QDataStream stream(zclFrame.payload());
                    stream.setByteOrder(QDataStream::LittleEndian);

                    stream >> attribute;
                    
                    if (zclFrame.commandId() == deCONZ::ZclGeneralCommandId::ZclReadAttributesResponseId)
                    {
                        stream >> status;
                    }
                    
                    if (status == deCONZ::ZclStatus::ZclSuccessStatus || zclFrame.commandId() == deCONZ::ZclGeneralCommandId::ZclReportAttributesId)
                    {
                        stream >> datatype;
                        
                        while (!stream.atEnd())
                        {
                            stream >> data;
                            bla += QString("%1").arg(data, 2, 16, QLatin1Char('0')).toUpper();
                        }
                        
                        DBG_Printf(DBG_INFO, "[ZB RESPONSE] - 0x%016llX (%04X) - Response report attributes (%02X) - Attribute: 0x%04X, Status: %s, Datatype: 0x%02X, Data: %s\n",
                                ind.srcAddress().ext(), ind.srcAddress().nwk(), zclFrame.commandId(), attribute, zclStatusCodes.value(status, "N/A").toUtf8().data(), datatype, bla.toUtf8().data()); 
                    }
                    else
                    {
                        DBG_Printf(DBG_INFO, "[ZB RESPONSE] - 0x%016llX (%04X) - Response read attributes (%02X) - Attribute: 0x%04X, Status: 0x%02X\n",
                                ind.srcAddress().ext(), ind.srcAddress().nwk(), zclFrame.commandId(), attribute, status);
                    }
                }
                    break;
                
                case deCONZ::ZclGeneralCommandId::ZclConfigureReportingResponseId:
                {
                    quint8 status;
                    quint8 direction;
                    quint16 attribute;
                    QString data;
                    
                    QDataStream stream(zclFrame.payload());
                    stream.setByteOrder(QDataStream::LittleEndian);
                    
                    while (!stream.atEnd())
                    {
                        stream >> status;
                        
                        data += "Status: ";
                        data += zclStatusCodes.value(status, "N/A").toUtf8().data();
                        data += ", ";
                        
                        if (status != 0x00) // not successful
                        {
                            stream >> direction;
                            stream >> attribute;
                            
                            data += "dir: ";
                            data += QString("%1").arg(direction, 2, 16, QLatin1Char('0')).toUpper();
                            data += ", ";
                            data += "attr: ";
                            data += QString("%1").arg(attribute, 4, 16, QLatin1Char('0')).toUpper();
                            data += " || ";
                        }
                    }
                    
                    DBG_Printf(DBG_INFO, "[ZB RESPONSE] - 0x%016llX (%04X) - Response configure reporting (%02X) - Sequence no.: %d, Mfc: 0x%04X, %s\n",
                            ind.srcAddress().ext(), ind.srcAddress().nwk(), zclFrame.commandId(), zclFrame.sequenceNumber(), zclFrame.manufacturerCode(), data.toUtf8().data());
                }
                    break;
                
                case deCONZ::ZclGeneralCommandId::ZclReadReportingConfigResponseId:
                {
                    quint8 status;
                    quint8 direction;
                    quint16 attribute;
                    QString data;
                    
                    QDataStream stream(zclFrame.payload());
                    stream.setByteOrder(QDataStream::LittleEndian);

                    while (!stream.atEnd())
                    {
                        stream >> status;
                        stream >> direction;
                        stream >> attribute;
                            
                        data += "Status: ";
                        data += zclStatusCodes.value(status, "N/A").toUtf8().data();
                        data += ", dir: ";
                        data += QString("%1").arg(direction, 2, 16, QLatin1Char('0')).toUpper();
                        data += ", ";
                        data += "attr: ";
                        data += QString("%1").arg(attribute, 4, 16, QLatin1Char('0')).toUpper();
                        
                        if (status == 0x00) // successful
                        {
                            quint8 dt;
                            quint16 min;
                            quint16 max;
                            //quint8 change;
                            
                            stream >> dt;
                            stream >> min;
                            stream >> max;
                            //stream >> change;
                            
                            data += ", type: ";
                            data += QString("%1").arg(dt, 2, 16, QLatin1Char('0')).toUpper();
                            data += ", min: ";
                            data += QString::number(min);
                            data += ", max: ";
                            data += QString("%1").arg(dt, 2, 16, QLatin1Char('0')).toUpper();
                            data += ", ";
                        }
                    }
                    
                    DBG_Printf(DBG_INFO, "[ZB RESPONSE] - 0x%016llX (%04X) - Response read reporting configuration (%02X) - Sequence no.: %d, Mfc: 0x%04X\n",
                            ind.srcAddress().ext(), ind.srcAddress().nwk(), zclFrame.commandId(), zclFrame.sequenceNumber(), zclFrame.manufacturerCode());
                }
                    break;
                
                case deCONZ::ZclGeneralCommandId::ZclWriteAttributesResponseId:
                {
                    quint8 seq;
                    quint8 cmd;
                    quint8 status;
                    QString bla;
                    
                    QDataStream stream(zclFrame.payload());
                    stream.setByteOrder(QDataStream::LittleEndian);

                    stream >> seq;
                    stream >> cmd;
                    
                    while (!stream.atEnd())
                    {
                        stream >> status;
                        bla += zclStatusCodes.value(status, "N/A").toUtf8().data();
                        bla += ", ";
                    }
                    
                    DBG_Printf(DBG_INFO, "[ZB RESPONSE] - 0x%016llX (%04X) - Response write attributes - Sequence no.: %d, Mfc: 0x%04X, Status: %s\n",
                            ind.srcAddress().ext(), ind.srcAddress().nwk(), zclFrame.commandId(), zclFrame.sequenceNumber(), zclFrame.manufacturerCode(), bla.toUtf8().data());
                }
                    break;
                
                case deCONZ::ZclGeneralCommandId::ZclReadAttributesId:
                case deCONZ::ZclGeneralCommandId::ZclWriteAttributesId:
                case deCONZ::ZclGeneralCommandId::ZclWriteAttributesUndividedId:
                case deCONZ::ZclGeneralCommandId::ZclWriteAttributesNoResponseId:
                case deCONZ::ZclGeneralCommandId::ZclConfigureReportingId:
                case deCONZ::ZclGeneralCommandId::ZclReadReportingConfigId:
                case deCONZ::ZclGeneralCommandId::ZclDefaultResponseId:
                case deCONZ::ZclGeneralCommandId::ZclDiscoverAttributesId:
                case deCONZ::ZclGeneralCommandId::ZclDiscoverAttributesResponseId:
                case deCONZ::ZclGeneralCommandId::ZclWriteAttributesStructuredId:
                case deCONZ::ZclGeneralCommandId::ZclWriteAttributesStructuredResponseId:
                {
                    DBG_Printf(DBG_INFO, "[ZB RESPONSE] - 0x%016llX (%04X) - Response command: %02X - Sequence no.: %d, Mfc: 0x%04X, Status: 0x%02X\n",
                            ind.srcAddress().ext(), ind.srcAddress().nwk(), zclFrame.commandId(), zclFrame.sequenceNumber(), zclFrame.manufacturerCode(), zclFrame.defaultResponseStatus());
                }
                    break;
                
                default:
                    break;
            }
        }
        else if (zclFrame.isClusterCommand())
        {
            DBG_Printf(DBG_INFO, "[ZB RESPONSE] - 0x%016llX (%04X) - Response cluster command: %02X - Sequence no.: %d, Mfc: 0x%04X, Status: 0x%02X\n",
                    ind.srcAddress().ext(), ind.srcAddress().nwk(), zclFrame.commandId(), zclFrame.sequenceNumber(), zclFrame.manufacturerCode(), zclFrame.defaultResponseStatus());
        }
    }
}
