#pragma once

#include <QTimeZone>

/**
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
*/

enum TimeStatus : quint8
{
	MASTER = 1U << 1, 
	SYNCHRONIZED = 1U << 2,
	SUPERSEEDING = 1U << 3, 
	MASTER_ZONE_DST = 1U << 4, 
	
};

class Timecluster
{
	static const quint32 default_validity_period = (3600 * 24);

public:

	quint32 utc_time;              // id 0x0000 Time
	quint8 time_status;                   // id 0x0001 TimeStatus Master|MasterZoneDst|Superseding
	qint32 timezone;              // id 0x0002 TimeZone
	quint32 dst_start;        // id 0x0003 DstStart
	quint32 dst_end;          // id 0x0004 DstEnd
	qint32 dst_shift;         // id 0x0005 DstShift
	quint32 standard_time;          // id 0x0006 StandardTime / StandardTime = Time + TimeZone
	quint32 local_time;             // id 0x0007 LocalTime
	quint32 time_valid_until; // id 0x0009 ValidUntilTime

public:
	Timecluster();

	static Timecluster getCurrentTime(bool useJ200Epoch);
};