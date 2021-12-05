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

int AS_handleAlarmSystemsApi(const ApiRequest &req, ApiResponse &rsp, AlarmSystems &alarmSystems, EventEmitter *eventEmitter)
{
    Q_UNUSED(eventEmitter);

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
                rsp.list.append(errInvalidValue(id, "config/code0", code0));
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

    if (!alarmSys->isValidCode(code0, 0))
    {
        rsp.list.append(errInvalidValue(id, "attr/code0", code0)); // use attr/ since this gets stripped away
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

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

    if (alarmSys->setTargetArmMode(mode))
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
