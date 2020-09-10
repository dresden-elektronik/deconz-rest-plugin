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

static void getTime(quint32 *time, qint32 *tz, quint32 *dstStart, quint32 *dstEnd, qint32 *dstShift, quint32 *standardTime, quint32 *localTime)
{
    QDateTime now = QDateTime::currentDateTimeUtc();
    QDateTime yearStart(QDate(QDate::currentDate().year(), 1, 1), QTime(0, 0), Qt::UTC);
    QTimeZone timeZone(QTimeZone::systemTimeZoneId());

    *time = *standardTime = *localTime = epoch.secsTo(now);
    *tz = timeZone.offsetFromUtc(yearStart);
    if (timeZone.hasTransitions())
    {
        QTimeZone::OffsetData dstStartOffsetData = timeZone.nextTransition(yearStart);
        QTimeZone::OffsetData dstEndOffsetData = timeZone.nextTransition(dstStartOffsetData.atUtc);
        *dstStart = epoch.secsTo(dstStartOffsetData.atUtc);
        *dstEnd = epoch.secsTo(dstEndOffsetData.atUtc);
        *dstShift = dstStartOffsetData.daylightTimeOffset;
        *standardTime += *tz;
        *localTime += *tz + ((*time >= *dstStart && *time <= *dstEnd) ? *dstShift : 0);
    }
}

/*! Handle packets related to the ZCL Time cluster.
    \param ind the APS level data indication containing the ZCL packet
    \param zclFrame the actual ZCL frame which holds the read attribute request
 */
void DeRestPluginPrivate::handleTimeClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    if (zclFrame.isProfileWideCommand())
    {
        if (zclFrame.commandId() == deCONZ::ZclReadAttributesId)
        {
        	   sendTimeClusterResponse(ind, zclFrame);
        }
        else if (zclFrame.commandId() == deCONZ::ZclWriteAttributesResponseId)
        {
            Sensor *sensor = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), ind.srcEndpoint(), QLatin1String("ZHATime"));
            if (sensor)
            {
                DBG_Printf(DBG_INFO, "  >>> %s sensor %s: set READ_TIME from handleTimeClusterIndication()\n", qPrintable(sensor->type()), qPrintable(sensor->name()));
                sensor->setNextReadTime(READ_TIME, queryTime);
                sensor->setLastRead(READ_TIME, idleTotalCounter);
                sensor->enableRead(READ_TIME);
                queryTime = queryTime.addSecs(1);
            }
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

    quint32 time_now = 0xFFFFFFFF;              // id 0x0000 Time
    qint8 time_status = 0x0D;                   // id 0x0001 TimeStatus Master|MasterZoneDst|Superseding
    qint32 time_zone = 0xFFFFFFFF;              // id 0x0002 TimeZone
    quint32 time_dst_start = 0xFFFFFFFF;        // id 0x0003 DstStart
    quint32 time_dst_end = 0xFFFFFFFF;          // id 0x0004 DstEnd
    qint32 time_dst_shift = 0xFFFFFFFF;         // id 0x0005 DstShift
    quint32 time_std_time = 0xFFFFFFFF;         // id 0x0006 StandardTime
    quint32 time_local_time = 0xFFFFFFFF;       // id 0x0007 LocalTime
    quint32 time_valid_until_time = 0xFFFFFFFF; // id 0x0009 ValidUntilTime

    getTime(&time_now, &time_zone, &time_dst_start, &time_dst_end, &time_dst_shift, &time_std_time, &time_local_time);
    time_valid_until_time = time_now + (3600 * 24 * 30 * 12);

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
        		stream << time_now;
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

/*! Sync a sensor's on-device real-time clock.
 * \param sensor the ZHATime sensor
 */
bool DeRestPluginPrivate::addTaskSyncTime(Sensor *sensor)
{
    if (!sensor || !sensor->isAvailable())
    {
        return false;
    }

    TaskItem task;
    task.taskType = TaskSyncTime;

    task.req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
    task.req.setDstEndpoint(sensor->fingerPrint().endpoint);
    task.req.setDstAddressMode(deCONZ::ApsExtAddress);
    task.req.dstAddress() = sensor->address();
    task.req.setClusterId(TIME_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);
    task.req.setSrcEndpoint(getSrcEndpoint(sensor, task.req));

    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(deCONZ::ZclWriteAttributesId);
    task.zclFrame.setFrameControl(deCONZ::ZclFCProfileCommand |
                                  deCONZ::ZclFCDirectionClientToServer |
                                  deCONZ::ZclFCDisableDefaultResponse);

    quint32 time_now = 0xFFFFFFFF;              // id 0x0000 Time
    qint8 time_status = 0x02;                   // id 0x0001 TimeStatus Synchronized
    qint32 time_zone = 0xFFFFFFFF;              // id 0x0002 TimeZone
    quint32 time_dst_start = 0xFFFFFFFF;        // id 0x0003 DstStart
    quint32 time_dst_end = 0xFFFFFFFF;          // id 0x0004 DstEnd
    qint32 time_dst_shift = 0xFFFFFFFF;         // id 0x0005 DstShift
    quint32 time_std_time = 0xFFFFFFFF;         // id 0x0006 StandardTime
    quint32 time_local_time = 0xFFFFFFFF;       // id 0x0007 LocalTime
    quint32 time_valid_until_time = 0xFFFFFFFF; // id 0x0009 ValidUntilTime

    getTime(&time_now, &time_zone, &time_dst_start, &time_dst_end, &time_dst_shift, &time_std_time, &time_local_time);
    time_valid_until_time = time_now + (3600 * 24);

    QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream << (quint16) 0x0000; // Time
    stream << (quint8) deCONZ::ZclUtcTime;
    stream << time_now;

    stream << (quint16) 0x0001; // Time Status
    stream << (quint8) deCONZ::Zcl8BitBitMap;
    stream << time_status;

    stream << (quint16) 0x0002; // Time Zone
    stream << (quint8) deCONZ::Zcl32BitInt;
    stream << time_zone;

    stream << (quint16) 0x0003; // Dst Start
    stream << (quint8) deCONZ::Zcl32BitUint;
    stream << time_dst_start;

    stream << (quint16) 0x0004; // Dst End
    stream << (quint8) deCONZ::Zcl32BitUint;
    stream << time_dst_end;

    stream << (quint16) 0x0005; // Dst Shift
    stream << (quint8) deCONZ::Zcl32BitInt;
    stream << time_dst_shift;

    stream << (quint16) 0x0009; // Valid Until Time
    stream << (quint8) deCONZ::ZclUtcTime;
    stream << time_valid_until_time;

    { // ZCL frame
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    return addTask(task);
}
