#include "alarm_user_schedule.h"
#include "alarm_user_event.h"
#include <QUuid>
#include "websocket_server.h"
/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "alarm_system_device_table.h"
#include "de_web_plugin_private.h"
#include "rest_alarmsystems.h"
#include <cmath>
#include <QDateTime>

#define ALARMSYS_PREFIX "/alarmsystems"
#define FMT_AS_ID "/alarmsystems/%1"
#define FMT_AS_ID_DEV "/alarmsystems/%1/device/%2"
#define FMT_AS_RESOURCE_NOT_AVAILABLE "resource, /alarmsystems/%1, not available"
#define FMT_AS_RESOURCE_DEV_NOT_AVAILABLE "resource, /alarmsystems/%1/device/%2, not available"

static const QLatin1String paramArmMask("armmask");
static const QLatin1String paramTrigger("trigger");
static const QLatin1String paramName("name");

static int getAllAlarmSystems(const ApiRequest &req, ApiResponse &rsp, const AlarmSystems &alarmSystems);
static int getAlarmSystem(const ApiRequest &req, ApiResponse &rsp, const AlarmSystems &alarmSystems);
static int putAlarmSystemConfig(const ApiRequest &req, ApiResponse &rsp, AlarmSystems &alarmSystems);
static int putAlarmSystemAttributes(const ApiRequest &req, ApiResponse &rsp, AlarmSystems &alarmSystems);
static int putAlarmSystemArmMode(const ApiRequest &req, ApiResponse &rsp, AlarmSystems &alarmSystems);
static int putAlarmSystemDevice(const ApiRequest &req, ApiResponse &rsp, AlarmSystems &alarmSystems);
static int deleteAlarmSystemDevice(const ApiRequest &req, ApiResponse &rsp, AlarmSystems &alarmSystems);

static QVariantMap errAlarmSystemNotAvailable(QLatin1String id)
{
    return errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString(FMT_AS_ID).arg(id),
                                                  QString(FMT_AS_RESOURCE_NOT_AVAILABLE).arg(id));
}

static QVariantMap errBodyContainsInvalidJson(int id)
{
    return errorToMap(ERR_INVALID_JSON, QString(FMT_AS_ID).arg(id),
                                        QString("body contains invalid JSON"));
}

static QVariantMap errInternalError(int id, const QString &reason)
{
    return errorToMap(ERR_INTERNAL_ERROR, QString(FMT_AS_ID).arg(id),
                                        QString("internal error, %1, occured").arg(reason));
}

static QVariantMap errMissingParameter(int id, const QLatin1String &param)
{
    return errorToMap(ERR_MISSING_PARAMETER, QString(FMT_AS_ID).arg(id), QString("missing parameter, %1").arg(param));
}

static QVariantMap errAlarmSystemDeviceNotAvailable(const QLatin1String &id, const QLatin1String &uniqueId)
{
    return errorToMap(ERR_RESOURCE_NOT_AVAILABLE, QString(FMT_AS_ID).arg(id),
                                                  QString(FMT_AS_RESOURCE_DEV_NOT_AVAILABLE).arg(id).arg(uniqueId));
}


static QVariantMap errInvalidDeviceValue(int id, const QLatin1String &uniqueId, const QLatin1String &param, const QString &value)
{
    return errorToMap(ERR_INVALID_VALUE, QString(FMT_AS_ID_DEV "/%3").arg(id).arg(uniqueId).arg(param),
                                         QString("invalid value, %1, for parameter, %2").arg(value).arg(param));
}

static QVariantMap errMissingDeviceParameter(int id, QLatin1String uniqueId, const QLatin1String &param)
{
    return errorToMap(ERR_MISSING_PARAMETER, QString(FMT_AS_ID_DEV).arg(id).arg(uniqueId),
                                             QString("missing parameter, %1").arg(param));
}

static QVariantMap errInvalidValue(int id, const char *suffix, const QString &value)
{
    Q_ASSERT(suffix);

    const char *param = strchr(suffix, '/');
    DBG_Assert(param != nullptr);
    if (!param)
    {
        return {};
    }
    param++;

    if (suffix[0] == 'a' && suffix[1] == 't')
    {
        suffix = param; // strip attr/
    }

    return errorToMap(ERR_INVALID_VALUE, QString(FMT_AS_ID "/%2").arg(id).arg(QLatin1String(suffix)), QString("invalid value, %1, for parameter, %2").arg(value).arg(QLatin1String(param)));
}

static QVariantMap errParameterNotAvailable(int id, const QString &param)
{
    return errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString(FMT_AS_ID "/%2").arg(id).arg(param), QString("parameter, %1, not available").arg(param));
}

static bool isValidAlaramSystemId(QLatin1String id)
{
    if (id.size() == 0)
    {
        return false;
    }

    for (int i = 0; i < id.size(); i++)
    {
        if (!std::isdigit(id.data()[i]))
        {
            return false;
        }
    }

    return true;
}

static int alarmSystemIdToInteger(QLatin1String id)
{
    if (isValidAlaramSystemId(id))
    {
        return std::atoi(id.data());
    }

    return INT_MAX;
}


static QVariantMap alarmSystemToMap(const AlarmSystem *alarmSys)
{
    QVariantMap result;

    QVariantMap config;
    QVariantMap state;
    QVariantMap devices;

    state[QLatin1String("armstate")] = alarmSys->armStateString();
    state[QLatin1String("seconds_remaining")] = alarmSys->secondsRemaining();

    const auto alarmSystemId = alarmSys->id();

    for (size_t i = 0; i < alarmSys->deviceTable()->size(); i++)
    {
        const AS_DeviceEntry &entry = alarmSys->deviceTable()->at(i);
        if (entry.alarmSystemId == alarmSystemId && isValid(entry))
        {
            QVariantMap dev;

            if ((entry.flags & (AS_ENTRY_FLAG_ARMED_AWAY | AS_ENTRY_FLAG_ARMED_STAY | AS_ENTRY_FLAG_ARMED_NIGHT)) != 0)
            {
                dev[paramArmMask] = QLatin1String(entry.armMask);
            }
            else
            {
                dev[paramArmMask] = QLatin1String("none");
            }

            // TODO  "trigger": "state/presence"

            devices[QLatin1String(entry.uniqueId, entry.uniqueIdSize)] = dev;
        }
    }

    for (int i = 0; i < alarmSys->itemCount(); i++)
    {
        const ResourceItem *item = alarmSys->itemForIndex(i);

        if (item && item->isPublic() && memcmp(item->descriptor().suffix, "config/", 7) == 0)
        {
            config[QLatin1String(item->descriptor().suffix + 7)] = item->toVariant();
        }
    }

    result[QLatin1String("name")] = alarmSys->item(RAttrName)->toString();
    result[QLatin1String("config")] = config;
    result[QLatin1String("state")] = state;
    result[QLatin1String("devices")] = devices;

    return result;
}

// Explicit endpoint: names/identities are not included in gateway full-state dumps.
static QVariantMap userToMap(const AlarmUsers::User &u, bool global = false)
{
    QVariantMap map;
    map[QLatin1String("id")] = QString::fromStdString(u.id);

    map[QLatin1String("name")] = QString::fromStdString(u.name);
    map[QLatin1String("enabled")] = u.enabled;
    map["user_revision"] = qlonglong(u.userRevision);
    if (global) { map["revision"] = qlonglong(u.userRevision); return map; }
    map["grant_enabled"] = u.grantEnabled;
    map["owner"] = u.owner;
    map["arm"] = u.arm;
    map["disarm"] = u.disarm;
    map["all_keypads"] = u.allKeypads;
    QVariantList keypads;
    for (const auto &p : u.keypads) {
        QVariantMap row; row["source"] = QString::fromStdString(p.source); row["endpoint"] = p.endpoint; keypads.append(row);
    }
    map["keypads"] = keypads;
    map[QLatin1String("api_arm_disarm")] = u.apiArmDisarm;
    map[QLatin1String("remaining_uses")] = u.remaining < 0 ? QVariant() : QVariant(qlonglong(u.remaining));
    map[QLatin1String("revision")] = qlonglong(u.revision);
    map[QLatin1String("schedule")] = u.schedule.empty() ? QVariant() : QJsonDocument::fromJson(QByteArray::fromStdString(u.schedule)).toVariant();
    return map; // never hash or PIN
}

static int handleAlarmUsers(const ApiRequest &req, ApiResponse &rsp, AlarmSystems &systems)
{
    const bool global = req.hdr.pathAt(3) == QLatin1String("users");
    const int collection = global ? 4 : 5;
    const int count = req.hdr.pathComponentsCount();
    const int alarm = global ? 0 : alarmSystemIdToInteger(req.hdr.pathAt(3));
    AlarmSystem *sys = AS_GetAlarmSystem(alarm, systems);
    auto fail = [&](const char *reason, bool storage = false) {
        rsp.httpStatus = storage ? HttpStatusServiceUnavailable : HttpStatusBadRequest;
        rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/alarmsystems/%1/users").arg(alarm), QLatin1String(reason)));
        return REQ_READY_SEND;
    };
    if (!global && !sys) return fail("alarm_not_found");
    if (!global && req.hdr.pathComponentsCount() == 6 && req.hdr.pathAt(5) == QLatin1String("capabilities") && req.hdr.httpMethod() == HttpGet) {
        bool managed = false;
        if (!sys->userManagementEnabled(managed)) return fail("storage_error", true);
        rsp.map[QLatin1String("managed")] = managed;
        rsp.map[QLatin1String("api_arm_disarm")] = true;
        rsp.map[QLatin1String("schedules")] = true;
        rsp.map[QLatin1String("schedule_version")] = 1;
        rsp.map[QLatin1String("global_users_version")] = 2;
        rsp.map[QLatin1String("per_alarm_grants")] = true;
        rsp.map[QLatin1String("access_event_version")] = 1;
        rsp.map[QLatin1String("rest_command_events")] = true;
        rsp.map[QLatin1String("alarm_timing_version")] = 1;
        rsp.map[QLatin1String("rejected_access_events")] = true;
        rsp.map[QLatin1String("keypad_lockout_version")] = 1;
        rsp.map[QLatin1String("max_users")] = AlarmUsers::MaxUsers;
        rsp.httpStatus = HttpStatusOk;
        return REQ_READY_SEND;
    }
    // Authenticated users API; policy is independent of managed-user opt-in.
    if (!global && req.hdr.pathComponentsCount() == 6 && req.hdr.pathAt(5) == QLatin1String("lockout")) {
        AlarmUsers::LockoutPolicy p; std::vector<AlarmUsers::LockoutState> states;
        const auto integer = [](const QVariant &v,qint64 &out) {
            if(v.type()!=QVariant::Double && v.type()!=QVariant::Int && v.type()!=QVariant::LongLong && v.type()!=QVariant::UInt)return false;
            const double d=v.toDouble();
            if(!std::isfinite(d)||d<0||d>9007199254740991.0||std::floor(d)!=d)return false;
            out=qint64(d);return true;
        };
        if(req.hdr.httpMethod()==HttpPut || req.hdr.httpMethod()==HttpDelete) {
            bool ok=false;const QVariant parsed=Json::parse(req.content,ok);const auto body=parsed.toMap();
            if(!ok||parsed.type()!=QVariant::Map)return fail("invalid_body");
            if(req.hdr.httpMethod()==HttpDelete) {
                if(body.size()!=1||body.value(QLatin1String("reset")).type()!=QVariant::Bool||!body.value(QLatin1String("reset")).toBool())return fail("explicit_reset_required");
                if(!sys->resetLockout())return fail("storage_error",true);
            } else {
                const QStringList keys={"enabled","threshold","window_seconds","durations_seconds","reset_seconds","revision"};
                if(body.size()!=keys.size())return fail("invalid_lockout_policy");
                for(auto i=body.cbegin();i!=body.cend();++i)if(!keys.contains(i.key()))return fail("unknown_field");
                if(body.value("enabled").type()!=QVariant::Bool)return fail("invalid_lockout_policy");
                p.enabled=body.value("enabled").toBool();
                qint64 threshold,window,reset,revision;
                if(!integer(body.value("threshold"),threshold)||threshold>100||!integer(body.value("window_seconds"),window)||window>3600||
                   !integer(body.value("reset_seconds"),reset)||reset>604800||!integer(body.value("revision"),revision))return fail("invalid_lockout_policy");
                p.threshold=int(threshold);p.windowSeconds=int(window);p.resetSeconds=int(reset);
                if(body.value("durations_seconds").type()!=QVariant::List)return fail("invalid_lockout_policy");
                const auto durations=body.value("durations_seconds").toList();
                if(durations.size()!=3)return fail("invalid_lockout_policy");
                for(int i=0;i<3;++i) {qint64 duration;if(!integer(durations[i],duration)||duration>3600)return fail("invalid_lockout_policy");p.durations[i]=int(duration);}
                std::string error;
                if(!sys->configureLockout(p,revision,error))return fail(error.c_str(),error=="storage_error");
            }
        } else if(req.hdr.httpMethod()!=HttpGet)return REQ_NOT_HANDLED;
        if(!sys->lockout(p,states))return fail("storage_error",true);
        QVariantMap policy;policy["enabled"]=p.enabled;policy["threshold"]=p.threshold;policy["window_seconds"]=p.windowSeconds;
        policy["durations_seconds"]=QVariantList{p.durations[0],p.durations[1],p.durations[2]};
        policy["reset_seconds"]=p.resetSeconds;policy["revision"]=qlonglong(p.revision);
        rsp.map["policy"]=policy;QVariantList rows;const qint64 now=QDateTime::currentMSecsSinceEpoch();
        for(const auto &st:states) {
            QVariantMap row;row["source"]=QString::fromStdString(st.source);row["endpoint"]=st.endpoint;
            row["level"]=st.level;row["locked_until"]=qlonglong(st.until);
            row["remaining_seconds"]=qlonglong(p.enabled && st.until>now ? (st.until-now+999)/1000 : 0);
            rows.append(row);
        }
        rsp.map["keypads"]=rows;rsp.httpStatus=HttpStatusOk;return REQ_READY_SEND;
    }
    std::vector<AlarmUsers::User> users;
    if (!AS_UserStore().list(global ? 0 : alarm,users)) return fail("storage_error",true);
    if (count==collection && req.hdr.httpMethod()==HttpGet) {
        for(const auto &u:users)rsp.map[QString::fromStdString(u.id)]=userToMap(u,global);
        if(users.empty())rsp.str=QLatin1String("{}");
        rsp.httpStatus=HttpStatusOk;return REQ_READY_SEND;
    }
    const bool create=count==collection && req.hdr.httpMethod()==HttpPost;
    if(!create && count!=collection+1)return REQ_NOT_HANDLED;
    const QString uid=create?QString():QString(req.hdr.pathAt(collection));
    if(!create && (uid.size()!=32 || std::any_of(uid.begin(),uid.end(),[](QChar c){return !(c>='0'&&c<='9')&&!(c>='a'&&c<='f');})))return fail("invalid_user_id");
    auto found=std::find_if(users.begin(),users.end(),[&](const AlarmUsers::User &u){return QString::fromStdString(u.id)==uid;});
    if(req.hdr.httpMethod()==HttpGet) {
        if(found==users.end())return fail("user_not_found");
        rsp.map=userToMap(*found,global);rsp.httpStatus=HttpStatusOk;return REQ_READY_SEND;
    }
    if(!create && req.hdr.httpMethod()!=HttpPut && req.hdr.httpMethod()!=HttpDelete)return REQ_NOT_HANDLED;
    bool ok=false;const QVariant parsed=Json::parse(req.content,ok);const auto body=parsed.toMap();
    if(!ok||parsed.type()!=QVariant::Map||body.isEmpty())return fail("invalid_body");
    const auto number=[](const QVariant &v,qint64 &out){
        if(v.type()!=QVariant::Double && v.type()!=QVariant::Int && v.type()!=QVariant::LongLong && v.type()!=QVariant::UInt)return false;
        const double d=v.toDouble();if(!std::isfinite(d)||d<0||d>9007199254740991.0||std::floor(d)!=d)return false;
        out=qint64(d);return true;
    };
    qint64 revision=0,userRevision=0;
    if(!number(body.value("revision"),revision)||!number(body.value("user_revision"),userRevision))return fail("revision_required");
    if(req.hdr.httpMethod()==HttpDelete) {
        if(body.size()!=2)return fail("unknown_field");
        if(!AS_UserStore().erase(global?0:alarm,uid.toStdString(),revision,userRevision))return fail("delete_failed_or_revision_conflict");
        rsp.map["deleted"]=uid;rsp.httpStatus=HttpStatusOk;return REQ_READY_SEND;
    }
    QStringList allowed={"revision","user_revision","name","enabled","pin"};
    if(!global)allowed.append({"grant_enabled","owner","arm","disarm","api_arm_disarm","all_keypads","keypads","remaining_uses","schedule"});
    for(auto i=body.cbegin();i!=body.cend();++i)if(!allowed.contains(i.key()))return fail("unknown_field");
    AlarmUsers::User u;
    if(found!=users.end())u=*found;
    else if(!create) {
        if(global)return fail("user_not_found");
        std::vector<AlarmUsers::User> identities;
        if(!AS_UserStore().list(0,identities))return fail("storage_error",true);
        auto identity=std::find_if(identities.begin(),identities.end(),[&](const AlarmUsers::User &x){return QString::fromStdString(x.id)==uid;});
        if(identity==identities.end())return fail("user_not_found");
        u=*identity;
    }
    u.userRevision=userRevision;
    if(body.contains("name")) {
        if(body.value("name").type()!=QVariant::String)return fail("invalid_name");
        u.name=body.value("name").toString().toStdString();
    }
    QString pin;
    if(body.contains("pin")) {
        if(body.value("pin").type()!=QVariant::String||body.value("pin").toString().isEmpty())return fail("invalid_pin");
        pin=body.value("pin").toString();
    }
    const auto boolean=[&](const char *key,bool &out){
        if(!body.contains(key))return true;
        if(body.value(key).type()!=QVariant::Bool)return false;
        out=body.value(key).toBool();return true;
    };
    if(!boolean("enabled",u.enabled)||!boolean("grant_enabled",u.grantEnabled)||!boolean("owner",u.owner)||
       !boolean("arm",u.arm)||!boolean("disarm",u.disarm)||!boolean("api_arm_disarm",u.apiArmDisarm)||!boolean("all_keypads",u.allKeypads))return fail("invalid_user");
    if(body.contains("remaining_uses")) {
        qint64 remaining=0;const auto value=body.value("remaining_uses");
        if(value.isNull())u.remaining=-1;
        else if(number(value,remaining)&&remaining<=1000000)u.remaining=remaining;
        else return fail("invalid_remaining_uses");
    }
    if(body.contains("schedule")) {
        const auto value=body.value("schedule");
        if(value.isNull())u.schedule.clear();
        else if(value.type()==QVariant::Map)u.schedule=QJsonDocument::fromVariant(value).toJson(QJsonDocument::Compact).toStdString();
        else return fail("invalid_schedule");
    }
    if(body.contains("keypads")) {
        const auto value=body.value("keypads");if(value.type()!=QVariant::List)return fail("invalid_keypads");
        u.keypads.clear();
        for(const auto &v:value.toList()) {
            if(v.type()!=QVariant::Map)return fail("invalid_keypads");
            const auto pad=v.toMap();qint64 endpoint;
            if(pad.size()!=2||pad.value("source").type()!=QVariant::String||!number(pad.value("endpoint"),endpoint)||endpoint<1||endpoint>240)return fail("invalid_keypads");
            u.keypads.push_back({pad.value("source").toString().toStdString(),int(endpoint)});
        }
    }
    std::string error;
    const bool saved=global?AS_UserStore().put(0,u,pin.toStdString(),revision,error):sys->putUser(u,pin,revision,error);
    if(!saved)return fail(error.c_str(),error=="storage_error");
    rsp.map=userToMap(u,global);rsp.httpStatus=HttpStatusOk;return REQ_READY_SEND;
}

int AS_handleAlarmSystemsApi(const ApiRequest &req, ApiResponse &rsp, AlarmSystems &alarmSystems, EventEmitter *eventEmitter)
{
    Q_UNUSED(eventEmitter);
    if ((req.hdr.pathComponentsCount() >= 5 && req.hdr.pathAt(4) == QLatin1String("users")) ||
        (req.hdr.pathComponentsCount() >= 4 && req.hdr.pathAt(3) == QLatin1String("users")))
        return handleAlarmUsers(req, rsp, alarmSystems);

    // GET /api/<apikey>/alarmsystems
    if (req.hdr.pathComponentsCount() == 3 && req.hdr.httpMethod() == HttpGet)
    {
        return getAllAlarmSystems(req, rsp, alarmSystems);
    }

    // POST /api/<apikey>/alarmsystems
    if (req.hdr.pathComponentsCount() == 3 && req.hdr.httpMethod() == HttpPost)
    {
        rsp.httpStatus = HttpStatusNotImplemented;
        return REQ_READY_SEND;
    }

    // GET /api/<apikey>/alarmsystems/<id>
    if (req.hdr.pathComponentsCount() == 4 && req.hdr.httpMethod() == HttpGet)
    {
        return getAlarmSystem(req, rsp, alarmSystems);
    }

    // PUT /api/<apikey>/alarmsystems/<id>/config
    if (req.hdr.pathComponentsCount() == 5 && req.hdr.httpMethod() == HttpPut && req.hdr.pathAt(4) == QLatin1String("config"))
    {
        return putAlarmSystemConfig(req, rsp, alarmSystems);
    }

    // PUT /api/<apikey>/alarmsystems/<id>/device/<uniqueid>
    if (req.hdr.pathComponentsCount() == 6 && req.hdr.httpMethod() == HttpPut && req.hdr.pathAt(4) == QLatin1String("device"))
    {
        return putAlarmSystemDevice(req, rsp, alarmSystems);
    }

    // DELETE /api/<apikey>/alarmsystems/<id>/device/<uniqueid>
    if (req.hdr.pathComponentsCount() == 6 && req.hdr.httpMethod() == HttpDelete && req.hdr.pathAt(4) == QLatin1String("device"))
    {
        return deleteAlarmSystemDevice(req, rsp, alarmSystems);
    }

    // PUT /api/<apikey>/alarmsystems/<id>
    if (req.hdr.pathComponentsCount() == 4 && req.hdr.httpMethod() == HttpPut)
    {
        return putAlarmSystemAttributes(req, rsp, alarmSystems);
    }

    // PUT /api/<apikey>/alarmsystems/<id>/(disarm | arm_stay | arm_night | arm_away)
    if (req.hdr.pathComponentsCount() == 5 && req.hdr.httpMethod() == HttpPut)
    {
        const QLatin1String op = req.hdr.pathAt(4);
        if (op == QLatin1String("disarm") || op == QLatin1String("arm_stay") || op == QLatin1String("arm_night") || op == QLatin1String("arm_away"))
        {
            return putAlarmSystemArmMode(req, rsp, alarmSystems);
        }
    }

    return REQ_NOT_HANDLED;
}

QVariantMap AS_AlarmSystemsToMap(const AlarmSystems &alarmSystems)
{
    QVariantMap result;

    for (const AlarmSystem *alarmSys : alarmSystems.alarmSystems)
    {
        result[QString::number(alarmSys->id())] = alarmSystemToMap(alarmSys);
    }

    return result;
}

static int getAllAlarmSystems(const ApiRequest &, ApiResponse &rsp, const AlarmSystems &alarmSystems)
{
    rsp.httpStatus = HttpStatusOk;

    if (alarmSystems.alarmSystems.empty())
    {
        rsp.str = QLatin1String("{}");
        return REQ_READY_SEND;
    }

    rsp.map = AS_AlarmSystemsToMap(alarmSystems);

    return REQ_READY_SEND;
}

static int getAlarmSystem(const ApiRequest &req, ApiResponse &rsp, const AlarmSystems &alarmSystems)
{
    const int id = alarmSystemIdToInteger(req.hdr.pathAt(3));

    const AlarmSystem *alarmSys = AS_GetAlarmSystem(id, alarmSystems);

    if (!alarmSys)
    {
        rsp.list.append(errAlarmSystemNotAvailable(req.hdr.pathAt(3)));
        rsp.httpStatus = HttpStatusNotFound;
        return REQ_READY_SEND;
    }

    rsp.httpStatus = HttpStatusOk;
    rsp.map = alarmSystemToMap(alarmSys);

    return REQ_READY_SEND;
}

// [ { "success": { "/alarmsystems/1/config/configured": true } } ]

QVariantMap addSuccessEntry(int id, const char *suffix, const QVariant &value)
{
    QVariantMap result;
    QVariantMap item;

    item[QString(FMT_AS_ID "/%2").arg(id).arg(QLatin1String(suffix))] = value;
    result[QLatin1String("success")] = item;

    return result;
}

// PUT /api/<apikey>/alarmsystems/<id>/config
static int putAlarmSystemConfig(const ApiRequest &req, ApiResponse &rsp, AlarmSystems &alarmSystems)
{
    const int id = alarmSystemIdToInteger(req.hdr.pathAt(3));

    AlarmSystem *alarmSys = AS_GetAlarmSystem(id, alarmSystems);

    if (!alarmSys)
    {
        rsp.list.append(errAlarmSystemNotAvailable(req.hdr.pathAt(3)));
        rsp.httpStatus = HttpStatusNotFound;
        return REQ_READY_SEND;
    }

    bool ok = false;
    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();

    if (!ok || map.isEmpty())
    {
        rsp.list.append(errBodyContainsInvalidJson(id));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    rsp.httpStatus = HttpStatusOk;

    const auto keys = map.keys();

    for (const auto &key : keys)
    {
        if (key == QLatin1String("code0"))
        {
            const QString code0 = map.value(key).toString();

            if (code0.size() < 4 || code0.size() > 16)
            {
                rsp.list.append(errInvalidValue(id, "config/code0", QLatin1String("[redacted]")));
                rsp.httpStatus = HttpStatusBadRequest;
                return REQ_READY_SEND;
            }

            if (alarmSys->setCode(0, code0))
            {
                rsp.list.append(addSuccessEntry(id, RConfigConfigured, true));
            }
            else
            {
                rsp.list.append(errInternalError(id, QLatin1String("failed to set code")));
                rsp.httpStatus = HttpStatusServiceUnavailable;
                return REQ_READY_SEND;
            }

            continue;
        }

        ResourceItemDescriptor rid;

        if (!getResourceItemDescriptor(QString("config/%1").arg(key), rid))
        {
            rsp.list.append(errParameterNotAvailable(id, key));
            rsp.httpStatus = HttpStatusNotFound;
            return REQ_READY_SEND;
        }

        std::array<const char*, 2> readOnly = { RConfigArmMode, RConfigConfigured };

        if (std::find(readOnly.cbegin(), readOnly.cend(), rid.suffix) != readOnly.cend())
        {
            rsp.list.append(errParameterNotAvailable(id, key));
            rsp.httpStatus = HttpStatusNotFound;
            return REQ_READY_SEND;
        }

        auto val = map.value(key);
        if (alarmSys->setValue(rid.suffix, val))
        {
            rsp.list.append(addSuccessEntry(id, rid.suffix, val));
        }
        else
        {
            rsp.list.append(errInvalidValue(id, rid.suffix, val.toString()));
            rsp.httpStatus = HttpStatusServiceUnavailable;
            return REQ_READY_SEND;
        }
    }

    return REQ_READY_SEND;
}

// PUT /api/<apikey>/alarmsystems/<id>
static int putAlarmSystemAttributes(const ApiRequest &req, ApiResponse &rsp, AlarmSystems &alarmSystems)
{
    const int id = alarmSystemIdToInteger(req.hdr.pathAt(3));

    AlarmSystem *alarmSys = AS_GetAlarmSystem(id, alarmSystems);

    if (!alarmSys)
    {
        rsp.list.append(errAlarmSystemNotAvailable(req.hdr.pathAt(3)));
        rsp.httpStatus = HttpStatusNotFound;
        return REQ_READY_SEND;
    }

    bool ok = false;
    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();

    if (!ok || map.isEmpty())
    {
        rsp.list.append(errBodyContainsInvalidJson(id));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    const auto keys = map.keys();
    rsp.httpStatus = HttpStatusOk;

    for (const auto &key : keys)
    {
        if (key == paramName)
        {
            const auto name = map.value(key).toString();

            if (name.isEmpty() || name.size() > 32)
            {
                rsp.list.append(errInvalidValue(id, RAttrName, name));
                rsp.httpStatus = HttpStatusNotFound;
                return REQ_READY_SEND;
            }

            alarmSys->setValue(RAttrName, name);

            rsp.list.append(addSuccessEntry(id, paramName.data(), name));
        }
        else
        {
            rsp.list.append(errParameterNotAvailable(id, key));
            rsp.httpStatus = HttpStatusNotFound;
            return REQ_READY_SEND;
        }
    }

    return REQ_READY_SEND;
}


// PUT /api/<apikey>/alarmsystems/<id>/(disarm | arm_stay | arm_night | arm_away)
static int putAlarmSystemArmMode(const ApiRequest &req, ApiResponse &rsp, AlarmSystems &alarmSystems)
{
    const int id = alarmSystemIdToInteger(req.hdr.pathAt(3));

    AlarmSystem *alarmSys = AS_GetAlarmSystem(id, alarmSystems);

    if (!alarmSys)
    {
        rsp.list.append(errAlarmSystemNotAvailable(req.hdr.pathAt(3)));
        rsp.httpStatus = HttpStatusNotFound;
        return REQ_READY_SEND;
    }

    bool ok = false;
    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();

    if (!ok || map.isEmpty())
    {
        rsp.list.append(errBodyContainsInvalidJson(id));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    rsp.httpStatus = HttpStatusOk;

    if (!map.contains(QLatin1String("code0")))
    {
        rsp.list.append(errMissingParameter(id, QLatin1String("code0")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    const QString code0 = map.value(QLatin1String("code0")).toString();

    AS_ArmMode mode = AS_ArmModeMax;

    const QLatin1String op = req.hdr.pathAt(4);
    if      (op == QLatin1String("disarm"))    { mode = AS_ArmModeDisarmed; }
    else if (op == QLatin1String("arm_away"))  { mode = AS_ArmModeArmedAway; }
    else if (op == QLatin1String("arm_stay"))  { mode = AS_ArmModeArmedStay; }
    else if (op == QLatin1String("arm_night")) { mode = AS_ArmModeArmedNight; }
    else
    {
        DBG_Assert(0 && "should never happen");
        return REQ_READY_SEND;
    }

    bool managed = false;
    if (!alarmSys->userManagementEnabled(managed)) {
        rsp.list.append(errInternalError(id, QLatin1String("credential storage unavailable")));
        rsp.httpStatus = HttpStatusServiceUnavailable;
        return REQ_READY_SEND;
    }
    AlarmUsers::RestResult decision;
    if (managed) decision = alarmSys->authorizeRest(code0, int(mode));
    else { decision.ok = true; decision.accepted = alarmSys->isValidCode(code0, 0); }
    if (!decision.ok) {
        rsp.list.append(errInternalError(id, QLatin1String("credential storage unavailable")));
        rsp.httpStatus = HttpStatusServiceUnavailable;
        return REQ_READY_SEND;
    }
    const auto publish = [&](bool applied) {
        if (!managed || !plugin->webSocketServer) return;
        const auto event = AlarmUsers::restEvent(managed, decision, alarmSys->idString(), QString(op),
            QUuid::createUuid().toString(QUuid::WithoutBraces).remove('-'),
            QDateTime::currentMSecsSinceEpoch(), applied);
        if (!event.isEmpty()) plugin->webSocketServer->broadcastTextMessage(Json::serialize(event));
    };
    if (!decision.accepted) {
        publish(false);
        rsp.list.append(errInvalidValue(id, "attr/code0", QLatin1String("[redacted]")));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }
    const bool applied = alarmSys->setTargetArmMode(mode);
    publish(applied);
    if (applied)
    {
        // success
        rsp.list.append(addSuccessEntry(id, RConfigArmMode, AS_ArmModeToString(mode)));
    }
    else
    {
        rsp.list.append(errInternalError(id, QString("failed to %1 the alarm system").arg(op)));
        rsp.httpStatus = HttpStatusServiceUnavailable;
        return REQ_READY_SEND;
    }

    return REQ_READY_SEND;
}

static bool isValidArmMask(const QString &armMask)
{
    if (armMask.isEmpty())
    {
        return false;
    }

    for (const auto ch : armMask)
    {
        if (!(ch == 'A' || ch == 'S' || ch == 'N'))
        {
            return false;
        }
    }

    return true;
}


bool isValidAlarmDeviceTrigger(const char *suffix)
{
    const std::array<const char*, 5> triggers = {
        RStatePresence,
        RStateVibration,
        RStateOpen,
        RStateButtonEvent,
        RStateOn
    };

    return std::find(triggers.cbegin(), triggers.cend(), suffix) != triggers.cend();
}

const char *getAlarmTriggerSuffix(const Resource *r)
{
    const std::array<const char*, 5> triggers = {
        RStatePresence,
        RStateVibration,
        RStateOpen,
        RStateButtonEvent,
        RStateOn
    };

    for (const char *suffix : triggers)
    {
        const ResourceItem *item = r->item(suffix);
        if (item)
        {
            return suffix;
        }
    }

    return nullptr;
}

// PUT /api/<apikey>/alarmsystems/<id>/device/<uniqueid>
static int putAlarmSystemDevice(const ApiRequest &req, ApiResponse &rsp, AlarmSystems &alarmSystems)
{
    rsp.httpStatus = HttpStatusOk;

    const int id = alarmSystemIdToInteger(req.hdr.pathAt(3));

    AlarmSystem *alarmSys = AS_GetAlarmSystem(id, alarmSystems);

    if (!alarmSys)
    {
        rsp.list.append(errAlarmSystemNotAvailable(req.hdr.pathAt(3)));
        rsp.httpStatus = HttpStatusNotFound;
        return REQ_READY_SEND;
    }

    const QLatin1String uniqueqId = req.hdr.pathAt(5);
    Sensor *sensor = plugin->getSensorNodeForUniqueId(uniqueqId);
    Resource *dev = sensor;

    if (!dev)
    {
        dev = plugin->getLightNodeForId(uniqueqId);
    }

    if (!dev)
    {
        rsp.list.append(errAlarmSystemDeviceNotAvailable(req.hdr.pathAt(3), uniqueqId));
        rsp.httpStatus = HttpStatusNotFound;
        return REQ_READY_SEND;
    }

    bool ok = false;
    QVariant var = Json::parse(req.content, ok);
    QVariantMap map = var.toMap();

    if (!ok)
    {
        rsp.list.append(errBodyContainsInvalidJson(id));
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    quint32 flags = 0;

    // keypad and keyfobs
    if (sensor && sensor->fingerPrint().hasOutCluster(IAS_ACE_CLUSTER_ID))
    {
        flags |= AS_ENTRY_FLAG_IAS_ACE;
    }

    if (map.isEmpty() && flags == 0) // non IAS ACE devices
    {
        rsp.list.append(errAlarmSystemDeviceNotAvailable(req.hdr.pathAt(3), uniqueqId));
        rsp.httpStatus = HttpStatusNotFound;
        return REQ_READY_SEND;
    }
    else
    {
        ResourceItem *item = nullptr;

        if (map.contains(paramArmMask))
        {
            const auto m = map.value(paramArmMask).toString();

            if (!isValidArmMask(m))
            {
                rsp.list.append(errInvalidDeviceValue(id, uniqueqId, paramArmMask, m));
                rsp.httpStatus = HttpStatusNotFound;
                return REQ_READY_SEND;
            }

            if (m.contains('A')) { flags |= AS_ENTRY_FLAG_ARMED_AWAY; }
            if (m.contains('S')) { flags |= AS_ENTRY_FLAG_ARMED_STAY; }
            if (m.contains('N')) { flags |= AS_ENTRY_FLAG_ARMED_NIGHT; }
        }
        else if (flags == 0) // non IAS ACE devices
        {
            rsp.list.append(errMissingDeviceParameter(id, uniqueqId, paramArmMask));
            rsp.httpStatus = HttpStatusNotFound;
            return REQ_READY_SEND;
        }

        if (map.contains(paramTrigger))
        {
            const auto trigger = map.value(paramTrigger).toString();

            ResourceItemDescriptor rid;
            if (getResourceItemDescriptor(trigger, rid))
            {
                if (isValidAlarmDeviceTrigger(rid.suffix))
                {
                    item = dev->item(rid.suffix);
                }
            }
        }
        else // auto selection
        {
            item = dev->item(getAlarmTriggerSuffix(dev));
        }

        if (!item && flags == 0) // non IAS ACE devices
        {
            rsp.list.append(errAlarmSystemDeviceNotAvailable(req.hdr.pathAt(3), uniqueqId));
            rsp.httpStatus = HttpStatusNotFound;
            return REQ_READY_SEND;
        }
    }

    if (alarmSys->addDevice(uniqueqId, flags))
    {
        QVariantMap rspItem;
        QVariantMap success;
        success[QLatin1String("added")] = QString(FMT_AS_ID_DEV).arg(id).arg(uniqueqId);
        rspItem[QLatin1String("success")] = success;

        rsp.list.append(rspItem);
    }
    else
    {
        rsp.list.append(errAlarmSystemDeviceNotAvailable(req.hdr.pathAt(3), uniqueqId));
        rsp.httpStatus = HttpStatusNotFound;
        return REQ_READY_SEND;
    }

    return REQ_READY_SEND;
}

// DELETE /api/<apikey>/alarmsystems/<id>/device/<uniqueid>
static int deleteAlarmSystemDevice(const ApiRequest &req, ApiResponse &rsp, AlarmSystems &alarmSystems)
{
    rsp.httpStatus = HttpStatusOk;

    const int id = alarmSystemIdToInteger(req.hdr.pathAt(3));

    AlarmSystem *alarmSys = AS_GetAlarmSystem(id, alarmSystems);

    if (!alarmSys)
    {
        rsp.list.append(errAlarmSystemNotAvailable(req.hdr.pathAt(3)));
        rsp.httpStatus = HttpStatusNotFound;
        return REQ_READY_SEND;
    }

    const QLatin1String uniqueqId = req.hdr.pathAt(5);

    {
        const AS_DeviceEntry &entry = alarmSys->deviceTable()->get(QString(uniqueqId));

        if (!isValid(entry))
        {
            rsp.list.append(errAlarmSystemDeviceNotAvailable(req.hdr.pathAt(3), uniqueqId));
            rsp.httpStatus = HttpStatusNotFound;
            return REQ_READY_SEND;
        }
    }

    if (alarmSys->removeDevice(uniqueqId))
    {
        QVariantMap rspItem;
        QVariantMap success;
        success[QLatin1String("removed")] = QString(FMT_AS_ID_DEV).arg(id).arg(uniqueqId);
        rspItem[QLatin1String("success")] = success;

        rsp.list.append(rspItem);
    }
    else
    {
        rsp.list.append(errInternalError(id, QLatin1String("failed remove device")));
        rsp.httpStatus = HttpStatusServiceUnavailable;
        return REQ_READY_SEND;
    }

    return REQ_READY_SEND;
}



