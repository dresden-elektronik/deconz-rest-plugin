/*
 * basic.cpp
 *
 * Implementation of Basic cluster server.
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
    quint16 manucode = 0xFFFF;

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

    //is there manufacture field in the request, if yes add it.
    if (zclFrame.frameControl() & deCONZ::ZclFCManufacturerSpecific)
    {
        manucode = zclFrame.manufacturerCode();
        outZclFrame.setFrameControl(outZclFrame.frameControl() | deCONZ::ZclFCManufacturerSpecific);
        outZclFrame.setManufacturerCode(manucode);
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

            switch (attr)
            {
            case 0x0000: // ZCL Version
                stream << code;
                stream << (quint8) deCONZ::Zcl8BitUint;
                stream << (quint8) 0x02;
                break;

            case 0x0001: // Application Version
            {
                stream << code;
                stream << (quint8) deCONZ::Zcl8BitUint;

                Sensor *sensor = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), ind.srcEndpoint());
                if (sensor && sensor->modelId() == QLatin1String("TRADFRI remote control"))
                {
                    // Since firmware version 2.3.014 when the large middle button is pressed the remote reads this attribute.
                    // If it isn't 17 as reported by earlier remote firmware, the left/right buttons don't send hold and long press commands anymore.
                    stream << quint8(17);
                }
                else
                {
                    stream << quint8(0x00);
                }
            }
                break;

            case 0x0002: // Stack Version
                stream << code;
                stream << (quint8) deCONZ::Zcl8BitUint;
                stream << (quint8) 0x00;
                break;

            case 0x0003: // HW Version
                stream << code;
                stream << (quint8) deCONZ::Zcl8BitUint;
                stream << (quint8) 0x00;
                break;

            case 0x0004: // Manufacturer Name
            {
                const char *name = "dresden elektronik";
                const quint8 length = strlen(name);

                stream << code;
                stream << (quint8) deCONZ::ZclCharacterString;
                stream << length;
                for (uint i = 0; i < length; i++)
                {
                    stream << (quint8) name[i];
                }
            }
                break;

            case 0x0005: // Model Identifier
            {
                const QByteArray id = apsCtrl->getParameter(deCONZ::ParamDeviceName).toLatin1();
                const quint8 length = id.length();

                stream << code;
                stream << (quint8) deCONZ::ZclCharacterString;
                stream << length;
                for (uint i = 0; i < length; i++)
                {
                    stream << (quint8) id[i];
                }
            }
                break;

            case 0x0007: // Power Source
                stream << code;
                stream << (quint8) deCONZ::Zcl8BitEnum;
                stream << (quint8) 0x04; // DC Power
                break;

            case 0x4000: // SW Build ID
            {
                const QByteArray version = gwFirmwareVersion.toLatin1();
                const quint8 length = version.length();

                stream << code;
                stream << (quint8) deCONZ::ZclCharacterString;
                stream << length;
                for (uint i = 0; i < length; i++)
                {
                    stream << (quint8) version[i];
                }
            }
                break;

            case 0xF000: // Legrand attribute used for pairing
                if (manucode == VENDOR_LEGRAND)
                {
                    stream << code;
                    stream << (quint8) deCONZ::Zcl32BitUint;
                    stream << (quint32) 0x000000d5;
                }
                else
                {
                    stream << (quint8) 0x86;  // unsupported_attribute
                }
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

    if (apsCtrlWrapper.apsdeDataRequest(req) != deCONZ::Success)
    {
        DBG_Printf(DBG_INFO, "Basic failed to send reponse\n");
    }

}
