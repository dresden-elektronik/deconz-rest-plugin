#include "timecluster.h"

Timecluster::Timecluster()
{
    this->time_status = {MASTER | SUPERSEEDING | MASTER_ZONE_DST};
};

Timecluster Timecluster::getCurrentTime(bool useJ200Epoch = true)
{
    Timecluster cluster;
    cluster.time_status = {MASTER | SUPERSEEDING | MASTER_ZONE_DST};

    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QDateTime yearStart(QDate(QDate::currentDate().year(), 1, 1), QTime(0, 0), Qt::UTC);
    const QTimeZone timeZone(QTimeZone::systemTimeZoneId());

    auto epoch = (useJ200Epoch)
                    ? QDateTime(QDate(2000, 1, 1), QTime(0, 0), Qt::UTC)
                    : QDateTime(QDate(1970, 1, 1), QTime(0, 0), Qt::UTC);

    cluster.utc_time = cluster.standard_time = cluster.local_time = epoch.secsTo(now);
    cluster.timezone = timeZone.offsetFromUtc(yearStart);
    if (timeZone.hasTransitions())
    {
        const QTimeZone::OffsetData dstStartOffsetData = timeZone.nextTransition(yearStart);
        const QTimeZone::OffsetData dstEndOffsetData = timeZone.nextTransition(dstStartOffsetData.atUtc);
        cluster.dst_start = epoch.secsTo(dstStartOffsetData.atUtc);
        cluster.dst_end = epoch.secsTo(dstEndOffsetData.atUtc);
        cluster.dst_shift = dstStartOffsetData.daylightTimeOffset;
        cluster.standard_time = cluster.utc_time + cluster.timezone;

        const bool isDaylightsavingTime = cluster.utc_time >= cluster.dst_start && cluster.utc_time <= cluster.dst_end;
        cluster.local_time = cluster.utc_time + cluster.timezone + ((isDaylightsavingTime) ? cluster.dst_shift : 0);
    }
    cluster.time_valid_until = cluster.utc_time + Timecluster::default_validity_period;

    return cluster;
}