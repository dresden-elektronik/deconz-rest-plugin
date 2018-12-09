/*
 * time.cpp
 *
 * Full implementation of Time cluster server.
 * Send ZCL attribute response to read request on Time Cluster attributes.
 *
 * 0x0000 Time         / UTC Time seconds from 1/1/2000
 * 0x0001 TimeStatus   / Master(bit-0)=1, Superseding(bit-3)= 1, MasterZoneDst(bit-2)=1
 * 0x0002 TimeZone     / offset seconds from UTC
 * 0x0003 DstStart     / daylight savings time start
 * 0x0004 DstEnd       / daylight savings time end
 * 0x0005 DstShift     / daylight savings offset
 * 0x0006 StandardTime / StandardTime = Time + TimeZone
 * 0x0007 LocalTime    / LocalTime = StandardTime (during winter time)
 *                       LocalTime = StandardTime + DstShift (during summer time)
 * 0x0008 LastSetTime
 * 0x0009 ValidUnilTime
 *
 */

#include <QTimeZone>
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"

const QDateTime epoch = QDateTime(QDate(2000, 1, 1), QTime(0, 0), Qt::UTC);

/*! Handle packets related to the ZCL Time cluster.
    \param ind the APS level data indication containing the ZCL packet
    \param zclFrame the actual ZCL frame which holds the read attribute request
 */
void DeRestPluginPrivate::handleTimeClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    if (zclFrame.isProfileWideCommand() && zclFrame.commandId() == deCONZ::ZclReadAttributesId)
    {
    	sendTimeClusterResponse(ind, zclFrame);
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

    quint32 time_now = 0xFFFFFFFF;              // id 0x0000 Time
    qint8 time_status = 0x09;                   // id 0x0001 TimeStatus Master(bit-0)=1, Superseding(bit-3)= 1
    qint32 time_zone = 0xFFFFFFFF;              // id 0x0002 TimeZone
    quint32 time_dst_start = 0xFFFFFFFF;        // id 0x0003 DstStart
    quint32 time_dst_end = 0xFFFFFFFF;          // id 0x0004 DstEnd
    qint32 time_dst_shift = 0xFFFFFFFF;         // id 0x0005 DstShift
    quint32 time_std_time = 0xFFFFFFFF;         // id 0x0006 StandardTime
    quint32 time_local_time = 0xFFFFFFFF;       // id 0x0007 LocalTime
    quint32 time_last_set_time = 0xFFFFFFFF;    // id 0x0008 LastSetTime
    quint32 time_valid_until_time = 0xFFFFFFFF; // id 0x0009 ValidUnilTime

    QDateTime dststart(QDateTime::fromTime_t(0));
    QDateTime dstend(QDateTime::fromTime_t(0));

    QDateTime local(QDateTime::currentDateTimeUtc());

    QDateTime beginYear(QDate(QDate::currentDate().year(), 1, 1), QTime(0, 0), Qt::UTC);

    time_now = epoch.secsTo(local);

    QTimeZone timeZoneLocal(QTimeZone::systemTimeZoneId());

    // if (timeZoneLocal.operator == (QTimeZone("Etc/GMT")))
    // {
    // 	timeZoneLocal = QTimeZone("Europe/Berlin");
    // }

    if (timeZoneLocal.hasTransitions())
    {
    	time_status |= 0x04; // MasterZoneDst(bit-2)=1

    	time_zone = timeZoneLocal.offsetFromUtc(beginYear);

    	QTimeZone::OffsetData dststartoffset = timeZoneLocal.nextTransition(beginYear);
    	dststart =  dststartoffset.atUtc;

    	QTimeZone::OffsetData dstendoffset = timeZoneLocal.nextTransition(dststart);
    	dstend =  dstendoffset.atUtc;

    	time_dst_shift = dststartoffset.daylightTimeOffset;

    	time_dst_start = epoch.secsTo(dststartoffset.atUtc);
    	time_dst_end = epoch.secsTo(dstendoffset.atUtc);
    	time_std_time = time_now +  time_zone;
    	time_local_time = time_now + timeZoneLocal.offsetFromUtc(local);
    	time_last_set_time = time_now;
    	time_valid_until_time = time_now + (3600 * 24 * 30 * 12);
    }

    // DBG_Printf(DBG_INFO, "Time_Cluster time_now       %s\n", local.toString(Qt::ISODate).toStdString().c_str());
    // DBG_Printf(DBG_INFO, "Time_Cluster time_local     %s\n", local.toTimeZone(timeZoneLocal).toString(Qt::ISODate).toStdString().c_str());
    // DBG_Printf(DBG_INFO, "Time_Cluster time_now       %ld \n", (long) time_now);
    // DBG_Printf(DBG_INFO, "Time_Cluster time_local     %ld \n", (long) time_local_time);
    // DBG_Printf(DBG_INFO, "Time_Cluster time_dst_start %s %ld\n", dststart.toUTC().toString(Qt::ISODate).toStdString().c_str(), (long) time_dst_start);
    // DBG_Printf(DBG_INFO, "Time_Cluster time_dst_end   %s %ld\n", dstend.toUTC().toString(Qt::ISODate).toStdString().c_str(), (long) time_dst_end);
    // DBG_Printf(DBG_INFO, "Time_Cluster time_dst_shift %d\n", (int) time_dst_shift);
    // DBG_Printf(DBG_INFO, "Time_Cluster time_zone      %d %s\n", (int) time_zone, timeZoneLocal.abbreviation(local).toStdString().c_str());
    // DBG_Printf(DBG_INFO, "Time_Cluster systemTimeZone %s\n", QTimeZone::systemTimeZone().abbreviation(local).toStdString().c_str());

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
        	// DBG_Printf(DBG_INFO, "Time_Cluster received read request attribute 0x%04X from %s %s\n",
        	// 		(int) attr,
					// ind.srcAddress().toStringNwk().toUtf8().data(),
					// ind.srcAddress().toStringExt().toUtf8().data());
        	switch(attr)
        	{
        	case 0x0000:
        		stream << code;
        		stream << (quint8) deCONZ::ZclUtcTime;
        		stream << time_now;
        		break;

        	case 0x0001:
        		stream << code;
        		stream << (quint8) deCONZ::Zcl8BitBitMap;
        		stream << time_status;
        		break;

        	case 0x0002:
        		stream << code;
        		stream << (quint8) deCONZ::Zcl32BitInt;
        		stream << time_zone;
        		break;

        	case 0x0003:
        		stream << code;
        		stream << (quint8) deCONZ::Zcl32BitUint;
        		stream << time_dst_start;
        		break;

        	case 0x0004:
        		stream << code;
        		stream << (quint8) deCONZ::Zcl32BitUint;
        		stream << time_dst_end;
        		break;

        	case 0x0005:
           		stream << code;
           		stream << (quint8) deCONZ::Zcl32BitInt;
           		stream << time_dst_shift;
        		break;

        	case 0x0006:
        		stream << code;
        		stream << (quint8) deCONZ::Zcl32BitUint;
        		stream << time_std_time;
        		break;

        	case 0x0007:
        		stream << code;
        		stream << (quint8) deCONZ::Zcl32BitUint;
        		stream << time_local_time;
        		break;

        	case 0x0008:
        		stream << code;
        		stream << (quint8) deCONZ::ZclUtcTime;
        		stream << time_last_set_time;
        		break;

        	case 0x0009:
        		stream << code;
        		stream << (quint8) deCONZ::ZclUtcTime;
        		stream << time_valid_until_time;
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
        DBG_Printf(DBG_INFO, "Time failed to send reponse\n");
    }

}

/*! Get all available timezone identifiers.
 */
QVariantList DeRestPluginPrivate::getTimezones()
{
    QVariantList list;
    const auto tzs = QTimeZone::availableTimeZoneIds();

    for (const QByteArray &tz : tzs)
    {
        list.append(tz);
    }

	return list;
}
