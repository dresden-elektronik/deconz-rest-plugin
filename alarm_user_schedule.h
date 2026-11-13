#ifndef ALARM_USER_SCHEDULE_H
#define ALARM_USER_SCHEDULE_H
#include <cstdint>
#include <QDateTime>
#include <QTimeZone>
#include <QStringList>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <cmath>
#include <string>

namespace AlarmUsers {
// Pure evaluation. No timers, environment TZ changes, device calls or enabled writes.
inline int checkSchedule(const std::string &policy, int64_t now)
{
    if (policy.empty()) return 1;
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(policy), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) return -1;
    const auto o = doc.object();
    const QStringList keys = {"timezone", "not_before", "expires_at", "windows"};
    if (o.size() != keys.size()) return -1;
    for (const auto &k : keys) if (!o.contains(k)) return -1;
    if (!o["timezone"].isString()) return -1;
    const auto tzid = o["timezone"].toString().toUtf8();
    if (!QTimeZone::isTimeZoneIdAvailable(tzid)) return -1;
    const QTimeZone zone(tzid);
    auto bound = [](const QJsonValue &v, qint64 &n) {
        if (v.isNull()) { n = 0; return true; }
        if (!v.isDouble()) return false;
        const double d = v.toDouble();
        if (!std::isfinite(d) || d < 1577836800000.0 || d > 4102444800000.0 || std::floor(d) != d) return false;
        n = qint64(d); return true;
    };
    qint64 start, end;
    if (!bound(o["not_before"],start) || !bound(o["expires_at"],end) || (start && end && start >= end)) return -1;
    if (!o["windows"].isArray()) return -1;
    const auto windows = o["windows"].toArray();
    if (windows.size() > 28) return -1;
    bool allowed = windows.isEmpty(); // no weekly restriction; expiry-only is valid
    const auto local = QDateTime::fromMSecsSinceEpoch(now, zone);
    for (const auto &value : windows) {
        if (!value.isObject()) return -1;
        const auto w = value.toObject();
        if (w.size() != 3 || !w.contains("day") || !w.contains("start") || !w.contains("end")) return -1;
        auto integer = [](const QJsonValue &v, int low, int high) {
            return v.isDouble() && std::isfinite(v.toDouble()) && std::floor(v.toDouble()) == v.toDouble() && v.toDouble() >= low && v.toDouble() <= high;
        };
        if (!integer(w["day"],1,7) || !integer(w["start"],0,1439) || !integer(w["end"],1,1440) || w["start"].toInt() >= w["end"].toInt()) return -1;
        const int minute = local.time().hour()*60 + local.time().minute();
        allowed |= local.date().dayOfWeek() == w["day"].toInt() && minute >= w["start"].toInt() && minute < w["end"].toInt();
    }
    if (now == 0) return 1; // structural validation, including timezone availability
    if (now < 1577836800000LL || now > 4102444800000LL || !local.isValid()) return -1;
    return allowed && (!start || now >= start) && (!end || now < end) ? 1 : 0;
}
}
#endif
