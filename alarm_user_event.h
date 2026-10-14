#ifndef ALARM_USER_EVENT_H
#define ALARM_USER_EVENT_H

#include "alarm_user_store.h"
#include <QDateTime>
#include <QVariantMap>

namespace AlarmUsers {
// Immutable decisions only. Sensor snapshots are not access attempts.
inline QVariantMap accessEvent(bool managed, const Result &result, const QString &alarm,
                               const QString &sensor, int mode, qint64 timestamp)
{
    QVariantMap event;
    const bool rejected = result.response == 4;
    const bool accepted = (result.response >= 0 && result.response <= 3) || result.response == 6;
    if (!managed || !result.ok || result.duplicate || result.eventId.empty() ||
        (!rejected && !accepted) || (!rejected && result.user.id.empty())) return event;

    static const char *actions[] = {"disarmed", "armed_stay", "armed_night", "armed_away",
                                   "invalid_code", "not_ready", "already_disarmed"};
    event[QLatin1String("t")] = QLatin1String("event");
    event[QLatin1String("e")] = QLatin1String("access");
    event[QLatin1String("r")] = QLatin1String("alarmsystems");
    event[QLatin1String("id")] = alarm;
    event[QLatin1String("sensor_id")] = sensor;
    event[QLatin1String("event_id")] = QString::fromStdString(result.eventId);
    event[QLatin1String("result")] = QLatin1String(rejected ? "rejected" : "accepted");
    event[QLatin1String("action")] = QLatin1String(actions[result.response]);
    event[QLatin1String("timestamp")] = QDateTime::fromMSecsSinceEpoch(timestamp, Qt::UTC).toString(Qt::ISODateWithMs);
    event[QLatin1String("uses_consumed")] = !rejected && mode == 0 && result.user.remaining >= 0 ? 1 : 0;
    if (result.locked) {
        event[QLatin1String("lockout")] = true;
        event[QLatin1String("locked_until")] = qlonglong(result.lockedUntil);
        event[QLatin1String("lockout_level")] = result.lockoutLevel;
    }
    if (!rejected)
    {
        event[QLatin1String("user_id")] = QString::fromStdString(result.user.id);
        event[QLatin1String("remaining_uses")] = result.user.remaining < 0
            ? QVariant() : QVariant(qlonglong(result.user.remaining));
    }
    return event; // Rejections never disclose matched user identity, PIN or hash.
}
// REST commands are a separate event type: NEVER a keypad/door activation.
// An accepted request is not proof that an exit delay has finished or a siren sounded.
inline QVariantMap restEvent(bool managed, const RestResult &result, const QString &alarm,
                             const QString &operation, const QString &eventId, qint64 timestamp,
                             bool applied)
{
    if (!managed || !result.ok || eventId.isEmpty() ||
        (result.accepted && result.user.id.empty()) ||
        (operation != "disarm" && operation != "arm_stay" &&
         operation != "arm_night" && operation != "arm_away")) return {};
    QVariantMap event;
    event["t"] = "event";
    event["e"] = "alarm_command";
    event["r"] = "alarmsystems";
    event["id"] = alarm;
    event["source"] = "rest";
    event["event_id"] = eventId;
    event["action"] = operation;
    event["result"] = !result.accepted ? "rejected" : applied ? "accepted" : "failed";
    event["timestamp"] = QDateTime::fromMSecsSinceEpoch(timestamp, Qt::UTC).toString(Qt::ISODateWithMs);
    event["uses_consumed"] = 0;
    if (result.accepted) {
        event["user_id"] = QString::fromStdString(result.user.id);
    }
    return event;
}

}
#endif

