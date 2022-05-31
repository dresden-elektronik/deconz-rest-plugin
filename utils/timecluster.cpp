#include "timecluster.h"

QDateTime Timecluster::getEpoch() {
    return (this->epoch_base == Epoch::J2000)
                    ? QDateTime(QDate(2000, 1, 1), QTime(0, 0), Qt::UTC)
                    : QDateTime(QDate(1970, 1, 1), QTime(0, 0), Qt::UTC);
}

Timecluster Timecluster::getCurrentTime(Epoch epochBase = Epoch::J2000)
{
    Timecluster cluster(epochBase);
    cluster.time_status = {MASTER | SUPERSEEDING | MASTER_ZONE_DST};

    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QDateTime yearStart(QDate(QDate::currentDate().year(), 1, 1), QTime(0, 0), Qt::UTC);
    const QTimeZone timeZone(QTimeZone::systemTimeZoneId());
    const auto epoch = cluster.getEpoch();

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