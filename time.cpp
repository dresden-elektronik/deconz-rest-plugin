/*
 * time.cpp
 *
 * Very basic implementation of Time cluster server for lumi.ctrl_neutral switch.
 * The switch sends a read attribute command for the Time attribute to the coordinator to check whether it's connected to the network.
 */



#include "de_web_plugin.h"
#include "de_web_plugin_private.h"

const QDateTime epoch = QDateTime(QDate(2000, 1, 1), QTime(0, 0), Qt::UTC);

/*! Handle packets related to the ZCL Time cluster.
    \param ind the APS level data indication containing the ZCL packet
    \param zclFrame the actual ZCL frame which holds the Window Covering command or attribute
 */
void DeRestPluginPrivate::handleTimeClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    QDataStream stream(zclFrame.payload());
    stream.setByteOrder(QDataStream::LittleEndian);

    if (zclFrame.isProfileWideCommand() && zclFrame.commandId() == deCONZ::ZclReadAttributesId)
    {
        quint16 attr;
        stream >> attr;
        if (attr == 0x0000) // Time
        {
            sendTimeClusterResponse(ind, zclFrame);
        }
    }
}

/*! Sends read attributes response to Time client.
    \param ind the APS level data indication containing the ZCL packet
    \param zclFrame the actual ZCL frame which holds the read attributes request
 */
void DeRestPluginPrivate::sendTimeClusterResponse(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
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

    { // payload
        QDataStream stream(&outZclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        quint16 attr = 0x0000; // Time
        quint8 code = 0x00; // success
        quint8 datatype = deCONZ::ZclUtcTime;
        // quint32 val = 0xffffffff; // invalid
        quint32 val = epoch.secsTo(QDateTime::currentDateTimeUtc());

        stream << attr;
        stream << code;
        stream << datatype;
        stream << val;
    }

    { // ZCL frame
        QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        outZclFrame.writeToStream(stream);
    }

    if (apsCtrl && apsCtrl->apsdeDataRequest(req) != deCONZ::Success)
    {
        DBG_Printf(DBG_INFO, "Time failed to send reponse\n");
    }

}
