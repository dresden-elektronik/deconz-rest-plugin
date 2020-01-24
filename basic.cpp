/*
 * basic.cpp
 *
 * Full implementation of Basic cluster server.
 * Send ZCL attribute response to read request on Basic Cluster attributes.
 *
 * 0x0000 ZCL Version    / Just to test
 * 0xF000 Running time   / Used for Legrand device
 *
 */

#include "de_web_plugin.h"
#include "de_web_plugin_private.h"


/*! Handle packets related to the ZCL Basic cluster.
    \param ind the APS level data indication containing the ZCL packet
    \param zclFrame the actual ZCL frame which holds the read attribute request
 */
void DeRestPluginPrivate::handleBasicClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    if (zclFrame.isProfileWideCommand() && zclFrame.commandId() == deCONZ::ZclReadAttributesId)
    {
    	sendBasicClusterResponse(ind, zclFrame);
    }
}

/*! Sends read attributes response to Basic client.
    \param ind the APS level data indication containing the ZCL packet
    \param zclFrame the actual ZCL frame which holds the read attributes request
 */
void DeRestPluginPrivate::sendBasicClusterResponse(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    deCONZ::ApsDataRequest req;
    deCONZ::ZclFrame outZclFrame;

    req.setProfileId(ind.profileId());
    req.setClusterId(ind.clusterId());
    req.setDstAddressMode(ind.srcAddressMode());
    req.dstAddress() = ind.srcAddress();
    req.setDstEndpoint(ind.srcEndpoint());
    req.setSrcEndpoint(endpoint());

    outZclFrame.setSequenceNumber(zclFrame.sequenceNumber());
    outZclFrame.setCommandId(deCONZ::ZclReadAttributesResponseId);

    outZclFrame.setFrameControl(deCONZ::ZclFCProfileCommand |
                                deCONZ::ZclFCDirectionServerToClient |
                                deCONZ::ZclFCDisableDefaultResponse);
                                
    //Sensor *sensor = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), 0x01);                          
    if (true)
    {
        outZclFrame.setFrameControl(outZclFrame.frameControl() | deCONZ::ZclFCManufacturerSpecific);
        outZclFrame.setManufacturerCode(0x1021);
    }

    { // payload
        QDataStream stream(&outZclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        QDataStream instream(zclFrame.payload());
        instream.setByteOrder(QDataStream::LittleEndian);
        quint8 code = 0x00; // success
        quint16 attr;

        while (!instream.atEnd())
        {
        	instream >> attr;
        	stream << attr;

        	switch(attr)
        	{
        	case 0x0000:
        	    //ZCL version
        		stream << code;
        		stream << (quint8) deCONZ::Zcl8BitUint;
        		stream << 0x02;
        		break;

        	case 0xF000:
        	    //Legrand attribute used for pairing
        		stream << code;
        		stream << (quint8) deCONZ::Zcl32BitUint;
        		stream << 0x000000d5;
        		break;

        	default:
        	{
        		stream << (quint8) 0x86;  // unsupported_attribute
        	}
        	break;
        	}
        }
    }

    { // ZCL frame
        QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        outZclFrame.writeToStream(stream);
    }

    if (apsCtrl && apsCtrl->apsdeDataRequest(req) != deCONZ::Success)
    {
        DBG_Printf(DBG_INFO, "Basic failed to send reponse\n");
    }

}
