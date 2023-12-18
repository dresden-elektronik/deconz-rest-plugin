#ifndef RESOURCE_H
#define RESOURCE_H

#include <QDateTime>
#include <QElapsedTimer>
#include <QVariant>
#include <vector>
#include <deconz.h>
#include "utils/bufstring.h"
#include "state_change.h"
#include "zcl/zcl.h"

class QString;
class QVariant;

enum ApiDataType
{
    DataTypeUnknown,
    DataTypeBool,
    DataTypeUInt8,
    DataTypeUInt16,
    DataTypeUInt32,
    DataTypeUInt64,
    DataTypeInt8,
    DataTypeInt16,
    DataTypeInt32,
    DataTypeInt64,
    DataTypeReal,
    DataTypeString,
    DataTypeTime,
    DataTypeTimePattern
};

struct R_Stats
{
    size_t toString = 0;
    size_t toNumber = 0;
    size_t item = 0;
};

extern R_Stats rStats;

// resource prefixes: /devices, /lights, /sensors, ...
extern const char *RAlarmSystems;
extern const char *RConfig;
extern const char *RDevices;
extern const char *RGroups;
extern const char *RLights;
extern const char *RSensors;

// resource events
extern const char *REventAdded;
extern const char *REventApsConfirm;
extern const char *REventAwake;
extern const char *REventBindingTable;
extern const char *REventBindingTick;
extern const char *REventDeleted;
extern const char *REventDeviceAlarm;
extern const char *REventDeviceAnnounce;
extern const char *REventPermitjoinEnabled;
extern const char *REventPermitjoinDisabled;
extern const char *REventPermitjoinRunning;
extern const char *REventPoll;
extern const char *REventDDFReload;
extern const char *REventDDFInitRequest;
extern const char *REventDDFInitResponse;
extern const char *REventValidGroup;
extern const char *REventCheckGroupAnyOn;
extern const char *REventNodeDescriptor;
extern const char *REventActiveEndpoints;
extern const char *REventSimpleDescriptor;
extern const char *REventStartTimer;
extern const char *REventStopTimer;
extern const char *REventStateEnter;
extern const char *REventStateLeave;
extern const char *REventStateTimeout;
extern const char *REventTick;
extern const char *REventTimerFired;
extern const char *REventZclResponse;
extern const char *REventZclReadReportConfigResponse;
extern const char *REventZdpMgmtBindResponse;
extern const char *REventZdpResponse;

// resouce suffixes: state/buttonevent, config/on, ...
extern const char *RInvalidSuffix;

extern const char *RAttrClass;
extern const char *RAttrConfigId;
extern const char *RAttrExtAddress;
extern const char *RAttrGroupAddress;
extern const char *RAttrId;
extern const char *RAttrLastAnnounced;
extern const char *RAttrLastSeen;
extern const char *RAttrLevelMin;
extern const char *RAttrManufacturerName;
extern const char *RAttrMode;
extern const char *RAttrModelId;
extern const char *RAttrName;
extern const char *RAttrNwkAddress;
extern const char *RAttrPowerOnCt;
extern const char *RAttrPowerOnLevel;
extern const char *RAttrPowerup;
extern const char *RAttrProductId;
extern const char *RAttrProductName;
extern const char *RAttrSwconfigid;
extern const char *RAttrSwVersion;
extern const char *RAttrSwVersionBis;
extern const char *RAttrType;
extern const char *RAttrUniqueId;

extern const char *RActionScene;

extern const char *RStateAction;
extern const char *RStateAirQuality;
extern const char *RStateAirQualityBis;
extern const char *RStateAirQualityPpb; // Deprecated
extern const char *RStateAirQualityPpbBis; // Deprecated
extern const char *RStateAlarm;
extern const char *RStateAlert;
extern const char *RStateAllOn;
extern const char *RStateAngle;
extern const char *RStateAnyOn;
extern const char *RStateArmState;
extern const char *RStateBattery;
extern const char *RStateBri;
extern const char *RStateButtonEvent;
extern const char *RStateCarbonMonoxide;
extern const char *RStateCharging;
extern const char *RStateColorMode;
extern const char *RStateConsumption;
extern const char *RStateConsumption_2;
extern const char *RStateCt;
extern const char *RStateCurrent;
extern const char *RStateCurrent_P1;
extern const char *RStateCurrent_P2;
extern const char *RStateCurrent_P3;
extern const char *RStateDark;
extern const char *RStateDaylight;
extern const char *RStateDeviceRunTime;
extern const char *RStateEffect;
extern const char *RStateErrorCode;
extern const char *RStateEventDuration;
extern const char *RStateExpectedEventDuration;
extern const char *RStateExpectedRotation;
extern const char *RStateFilterRunTime;
extern const char *RStateFire;
extern const char *RStateFlag;
extern const char *RStateFloorTemperature;
extern const char *RStateGPDFrameCounter;
extern const char *RStateGPDLastPair;
extern const char *RStateGesture;
extern const char *RStateGradient;
extern const char *RStateHeating;
extern const char *RStateHue;
extern const char *RStateHumidity;
extern const char *RStateHumidityBis;
extern const char *RStateLastCheckin; // Poll control check-in
extern const char *RStateLastSet;
extern const char *RStateLastUpdated;
extern const char *RStateLift;
extern const char *RStateLightLevel;
extern const char *RStateLocaltime;
extern const char *RStateLockState;
extern const char *RStateLowBattery;
extern const char *RStateLux;
extern const char *RStateMeasuredValue;
extern const char *RStateMoisture;
extern const char *RStateMountingModeActive;
extern const char *RStateMusicSync;
extern const char *RStateOn;
extern const char *RStateOpen;
extern const char *RStateOpenBis;
extern const char *RStateOrientationX;
extern const char *RStateOrientationY;
extern const char *RStateOrientationZ;
extern const char *RStatePM2_5; // Deprecated
extern const char *RStatePanel;
extern const char *RStatePower;
extern const char *RStatePresence;
extern const char *RStatePresenceEvent;
extern const char *RStatePressure;
extern const char *RStatePressureBis;
extern const char *RStateProduction;
extern const char *RStateReachable;
extern const char *RStateReplaceFilter;
extern const char *RStateRotaryEvent;
extern const char *RStateSat;
extern const char *RStateSecondsRemaining;
extern const char *RStateSpectralX;
extern const char *RStateSpectralY;
extern const char *RStateSpectralZ;
extern const char *RStateSpeed;
extern const char *RStateStatus;
extern const char *RStateSunrise;
extern const char *RStateSunset;
extern const char *RStateTampered;
extern const char *RStateTemperature;
extern const char *RStateTemperatureBis;
extern const char *RStateTest;
extern const char *RStateTilt;
extern const char *RStateTiltAngle;
extern const char *RStateUtc;
extern const char *RStateValve;
extern const char *RStateVibration;
extern const char *RStateVibrationStrength;
extern const char *RStateVoltage;
extern const char *RStateWater;
extern const char *RStateWaterBis;
extern const char *RStateWindowOpen;
extern const char *RStateX;
extern const char *RStateY;

extern const char *RCapAlertTriggerEffect;
extern const char *RCapBriMoveWithOnOff;
extern const char *RCapBriMinDimLevel;
extern const char *RCapColorCapabilities;
extern const char *RCapColorCtComputesXy;
extern const char *RCapColorCtMax;
extern const char *RCapColorCtMin;
extern const char *RCapColorEffects;
extern const char *RCapColorGamutType;
extern const char *RCapColorGradientMaxSegments;
extern const char *RCapColorGradientPixelCount;
extern const char *RCapColorGradientPixelLength;
extern const char *RCapColorGradientStyles;
extern const char *RCapColorXyBlueX;
extern const char *RCapColorXyBlueY;
extern const char *RCapColorXyGreenX;
extern const char *RCapColorXyGreenY;
extern const char *RCapColorXyRedX;
extern const char *RCapColorXyRedY;
extern const char *RCapGroup;
extern const char *RCapGroupsNotSupported;
extern const char *RCapMeasuredValueMax;
extern const char *RCapMeasuredValueMin;
extern const char *RCapMeasuredValueQuantity;
extern const char *RCapMeasuredValueSubstance;
extern const char *RCapMeasuredValueUnit;
extern const char *RCapOnOffWithEffect;
extern const char *RCapSleeper;
extern const char *RCapTransitionBlock;

extern const char *RConfigAlarmSystemId;
extern const char *RConfigAlert;
extern const char *RConfigAllowTouchlink;
extern const char *RConfigArmMode;
extern const char *RConfigArmedAwayEntryDelay;
extern const char *RConfigArmedAwayExitDelay;
extern const char *RConfigArmedAwayTriggerDuration;
extern const char *RConfigArmedNightEntryDelay;
extern const char *RConfigArmedNightExitDelay;
extern const char *RConfigArmedNightTriggerDuration;
extern const char *RConfigArmedStayEntryDelay;
extern const char *RConfigArmedStayExitDelay;
extern const char *RConfigArmedStayTriggerDuration;
extern const char *RConfigBattery;
extern const char *RConfigBatteryBis;
extern const char *RConfigBriCoupleCt;
extern const char *RConfigBriExecuteIfOff;
extern const char *RConfigBriMax;
extern const char *RConfigBriMin;
extern const char *RConfigBriOnLevel;
extern const char *RConfigBriOnOffTransitiontime;
extern const char *RConfigBriOptions;
extern const char *RConfigBriStartup;
extern const char *RConfigCheckin;
extern const char *RConfigClickMode;
extern const char *RConfigColorCapabilities; // Deprecated
extern const char *RConfigColorCtStartup;
extern const char *RConfigColorExecuteIfOff;
extern const char *RConfigColorGradientPixelCount;
extern const char *RConfigColorGradientReversed;
extern const char *RConfigColorXyStartupX;
extern const char *RConfigColorXyStartupY;
extern const char *RConfigConfigured;
extern const char *RConfigTuyaUnlock;
extern const char *RConfigControlSequence;
extern const char *RConfigCoolSetpoint;
extern const char *RConfigCtMax; // Deprecated
extern const char *RConfigCtMin; // Deprecated
extern const char *RConfigDelay;
extern const char *RConfigDeviceMode;
extern const char *RConfigDeviceModeBis;
extern const char *RConfigDisarmedEntryDelay;
extern const char *RConfigDisarmedExitDelay;
extern const char *RConfigDisplayFlipped;
extern const char *RConfigDuration;
extern const char *RConfigEnrolled;
extern const char *RConfigExternalTemperatureSensor;
extern const char *RConfigExternalWindowOpen;
extern const char *RConfigFanMode;
extern const char *RConfigFilterLifeTime;
extern const char *RConfigGPDDeviceId;
extern const char *RConfigGPDKey;
extern const char *RConfigGroup;
extern const char *RConfigHeatSetpoint;
extern const char *RConfigHostFlags;
extern const char *RConfigHumiMaxThreshold;
extern const char *RConfigHumiMinThreshold;
extern const char *RConfigInterfaceMode;
extern const char *RConfigLastChangeAmount;
extern const char *RConfigLastChangeSource;
extern const char *RConfigLastChangeTime;
extern const char *RConfigLat;
extern const char *RConfigLedIndication;
extern const char *RConfigLoadBalancing;
extern const char *RConfigLocalTime;
extern const char *RConfigLock;
extern const char *RConfigLocked;
extern const char *RConfigLong;
extern const char *RConfigMelody;
extern const char *RConfigMode;
extern const char *RConfigMountingMode;
extern const char *RConfigOffset;
extern const char *RConfigOn;
extern const char *RConfigOnStartup;
extern const char *RConfigPending;
extern const char *RConfigPreset;
extern const char *RConfigPulseConfiguration;
extern const char *RConfigRadiatorCovered;
extern const char *RConfigReachable;
extern const char *RConfigReportGrid;
extern const char *RConfigResetPresence;
extern const char *RConfigReversed;
extern const char *RConfigSchedule;
extern const char *RConfigScheduleOn;
extern const char *RConfigSelfTest;
extern const char *RConfigSensitivity;
extern const char *RConfigSensitivityBis;
extern const char *RConfigSensitivityMax;
extern const char *RConfigSetValve;
extern const char *RConfigSpeed;
extern const char *RConfigSunriseOffset;
extern const char *RConfigSunsetOffset;
extern const char *RConfigUnoccupiedHeatSetpoint;
extern const char *RConfigSwingMode;
extern const char *RConfigTempMaxThreshold;
extern const char *RConfigTempMinThreshold;
extern const char *RConfigTemperature;
extern const char *RConfigTemperatureMeasurement;
extern const char *RConfigTholdDark;
extern const char *RConfigTholdOffset;
extern const char *RConfigTriggerDistance;
extern const char *RConfigTriggerDistanceBis;
extern const char *RConfigUbisysJ1AdditionalSteps;
extern const char *RConfigUbisysJ1ConfigurationAndStatus;
extern const char *RConfigUbisysJ1InactivePowerThreshold;
extern const char *RConfigUbisysJ1InstalledClosedLimitLift;
extern const char *RConfigUbisysJ1InstalledClosedLimitTilt;
extern const char *RConfigUbisysJ1InstalledOpenLimitLift;
extern const char *RConfigUbisysJ1InstalledOpenLimitTilt;
extern const char *RConfigUbisysJ1LiftToTiltTransitionSteps;
extern const char *RConfigUbisysJ1LiftToTiltTransitionSteps2;
extern const char *RConfigUbisysJ1Mode;
extern const char *RConfigUbisysJ1StartupSteps;
extern const char *RConfigUbisysJ1TotalSteps;
extern const char *RConfigUbisysJ1TotalSteps2;
extern const char *RConfigUbisysJ1TurnaroundGuardTime;
extern const char *RConfigUbisysJ1WindowCoveringType;
extern const char *RConfigUrl;
extern const char *RConfigUsertest;
extern const char *RConfigVolume;
extern const char *RConfigWindowCoveringType;
extern const char *RConfigWindowOpen;
extern const char *RConfigWindowOpenDetectionEnabled;

#define R_ALERT_DEFAULT             QVariant(QLatin1String("none"))
#define R_SENSITIVITY_MAX_DEFAULT   2
#define R_THOLDDARK_DEFAULT         12000
#define R_THOLDOFFSET_DEFAULT       7000

extern const QStringList RStateAlertValues;
extern const QStringList RStateAlertValuesTriggerEffect;

extern const QStringList RStateEffectValues;
#define R_EFFECT_NONE               0
#define R_EFFECT_COLORLOOP          1
extern const QStringList RStateEffectValuesMueller;
extern const QStringList RStateEffectValuesXmasLightStrip;

extern const QStringList RConfigLastChangeSourceValues;

extern const QStringList RConfigDeviceModeValues;

#define R_PENDING_DELAY             (1 << 0)
#define R_PENDING_SENSITIVITY       (1 << 2)
#define R_PENDING_WRITE_CIE_ADDRESS (1 << 4)
#define R_PENDING_ENROLL_RESPONSE   (1 << 5)
#define R_PENDING_MODE              (1 << 6)
#define R_PENDING_WRITE_POLL_CHECKIN_INTERVAL  (1 << 6)
#define R_PENDING_SET_LONG_POLL_INTERVAL       (1 << 7)

// after device announce is received lights can be brought into a defined state
// this might be useful for powerloss and OTA updates or simply providing a default power on configuration
#define R_POWERUP_RESTORE                   (1 << 0)  // restore last known on/off and brightness state
#define R_POWERUP_RESTORE_AT_DAYLIGHT       (1 << 1)  // restore at daylight
#define R_POWERUP_RESTORE_AT_NO_DAYLIGHT    (1 << 2)  // restore when no daylight

namespace deCONZ {
    class ApsDataIndication;
    class ZclFrame;
}

class  ResourceItemDescriptor
{
public:
    enum class Access { Unknown, ReadWrite, ReadOnly };
    ResourceItemDescriptor() = default;
    ResourceItemDescriptor(ApiDataType t, QVariant::Type v, const char *s, qint64 min = 0, qint64 max = 0) :
        type(t),
        qVariantType(v),
        suffix(s),
        validMin((double)min),
        validMax((double)max) { }

    bool isValid() const { return (type != DataTypeUnknown && suffix); }
    Access access = Access::Unknown;
    ApiDataType type = DataTypeUnknown;
    QVariant::Type qVariantType = QVariant::Invalid;
    const char *suffix = RInvalidSuffix;
    double validMin = 0;
    double validMax = 0;
    quint16 flags = 0;
};

class Resource;
class ResourceItem;

using ItemString = BufString<16>;

extern const ResourceItemDescriptor rInvalidItemDescriptor;

class ResourceItem
{
public:
    enum ItemFlags
    {
        FlagNeedPushSet     = 0x01, // set after a value has been set
        FlagNeedPushChange  = 0x02, // set when new value different than previous
        FlagPushOnSet       = 0x04, // events will be generated when item is set (even when value doesn't change)
        FlagPushOnChange    = 0x08, // events will be generated only when item changes
        FlagAwakeOnSet      = 0x10, // REventAwake will be generated when item is set after parse
        FlagImplicit        = 0x20, // the item is always present for a specific resource type
        FlagDynamicDescriptor = 0x40, // ResourceItemDescriptor is dynamic (not specified in code)
    };

    enum ValueSource
    {
        SourceUnknown,
        SourceDevice,
        SourceApi
    };

    ResourceItem(const ResourceItem &other);
    ResourceItem(ResourceItem &&other) noexcept;
    ResourceItem(const ResourceItemDescriptor &rid);
    ResourceItem &operator=(const ResourceItem &other);
    ResourceItem &operator=(ResourceItem &&other) noexcept;
    ~ResourceItem() noexcept;
    bool needPushSet() const;
    bool needPushChange() const;
    void clearNeedPush();
    bool pushOnSet() const;
    void setPushOnSet(bool enable);
    bool pushOnChange() const;
    void setPushOnChange(bool enable);
    bool awake() const;
    void setAwake(bool awake);
    bool implicit() const;
    void setImplicit(bool implicit);
    const QString &toString() const;
    QLatin1String toLatin1String() const;
    const char *toCString() const;
    qint64 toNumber() const;
    qint64 toNumberPrevious() const;
    deCONZ::SteadyTimeRef lastZclReport() const { return m_lastZclReport; }
    void setLastZclReport(deCONZ::SteadyTimeRef t) { m_lastZclReport = t; }
    bool toBool() const;
    QVariant toVariant() const;
    deCONZ::TimeSeconds refreshInterval() const { return m_refreshInterval; }
    void setRefreshInterval(deCONZ::TimeSeconds interval) { m_refreshInterval = interval; }
    void setZclProperties(const ZCL_Param &param) { m_zclParam = param; }
    bool setValue(const QString &val, ValueSource source = SourceUnknown);
    bool setValue(qint64 val, ValueSource source = SourceUnknown);
    bool setValue(const QVariant &val, ValueSource source = SourceUnknown);
    const ResourceItemDescriptor &descriptor() const;
    const QDateTime &lastSet() const;
    const QDateTime &lastChanged() const;
    void setTimeStamps(const QDateTime &t);
    void inRule(int ruleHandle);
    const std::vector<int> &rulesInvolved() const;
    bool isPublic() const;
    void setIsPublic(bool isPublic);
    const ZCL_Param &zclParam() const { return m_zclParam; }
    ParseFunction_t parseFunction() const { return m_parseFunction; }
    void setParseFunction(ParseFunction_t fn) { m_parseFunction = fn; }
    ValueSource valueSource() const { return m_valueSource; }
    void setDdfItemHandle(quint32 handle) { m_ddfItemHandle = handle; }
    quint32 ddfItemHandle() const { return m_ddfItemHandle; }

private:
    ResourceItem() = delete;
    bool setItemString(const QString &str);

    /* New layout

        quint16 flags;
        quint16 m_ridHandle;
        quint32 m_ddfItemHandle;
        qint32 m_lastSet; // ms since epoch - FIX_OSSET
        qint32 m_lastChanged; // ...
        qint64 num;
        qint64 numPrevious;
        ItemString istr;

        // . 40 bytes
     */

    ValueSource m_valueSource = SourceUnknown;
    bool m_isPublic = true;
    quint16 m_flags = 0; // bitmap of ResourceItem::ItemFlags
    union
    {
        struct {
            qint64 m_num = 0;
            qint64 m_numPrev = 0;
        };
        struct {
            double m_double;
            double m_doublePrev;
        };
    };
    deCONZ::SteadyTimeRef m_lastZclReport;

    BufStringCacheHandle m_strHandle; // for strings which don't fit into \c m_istr
    ItemString m_istr; // internal embedded small string
    deCONZ::TimeSeconds m_refreshInterval;
    QString *m_str = nullptr;
    const ResourceItemDescriptor *m_rid = &rInvalidItemDescriptor;
    QDateTime m_lastSet;
    QDateTime m_lastChanged;
    std::vector<int> m_rulesInvolved; // the rules a resource item is trigger
    ZCL_Param m_zclParam{};
    ParseFunction_t m_parseFunction = nullptr;
    quint32 m_ddfItemHandle = 0; // invalid item handle
};

class Resource
{
public:
    struct Handle
    {
        uint hash = 0;     // qHash(uniqueid)
        quint16 index = 0; // index in container
        // 'D' device
        // 'G' group
        // 'L' LightNode
        // 'S' Sensor
        char type = 0;
        quint8 order = 0;
    };

    Resource(const char *prefix);
    ~Resource() = default;
    Resource(const Resource &other);
    Resource(Resource &&other) noexcept;
    Resource &operator=(const Resource &other);
    Resource &operator=(Resource &&other) noexcept;
    const char *prefix() const;
    ResourceItem *addItem(ApiDataType type, const char *suffix);
    void removeItem(const char *suffix);
    ResourceItem *item(const char *suffix);
    const ResourceItem *item(const char *suffix) const;
    bool toBool(const char *suffix) const;
    qint64 toNumber(const char *suffix) const;
    const QString &toString(const char *suffix) const;
    QVariant toVariant(const char *suffix) const;
    virtual void didSetValue(ResourceItem *) {};
    bool setValue(const char *suffix, qint64 val, bool forceUpdate = false);
    bool setValue(const char *suffix, const QString &val, bool forceUpdate = false);
    bool setValue(const char *suffix, const QVariant &val, bool forceUpdate = false);
    int itemCount() const;
    ResourceItem *itemForIndex(size_t idx);
    const ResourceItem *itemForIndex(size_t idx) const;
    void addStateChange(const StateChange &stateChange);
    std::vector<StateChange> &stateChanges() { return m_stateChanges; }
    void cleanupStateChanges();
    Resource *parentResource() { return m_parent; }
    const Resource *parentResource() const { return m_parent; }
    void setParentResource(Resource *parent) { m_parent = parent; }
    Handle handle() const noexcept { return m_handle; }
    void setHandle(Handle handle) { m_handle = handle; }

private:
    Resource() = delete;
    Handle m_handle{};
    const char *m_prefix = nullptr;
    Resource *m_parent = nullptr;
    std::vector<ResourceItem> m_rItems;
    std::vector<StateChange> m_stateChanges;
};

void initResourceDescriptors();
const char *getResourcePrefix(const QString &str);
bool getResourceItemDescriptor(const QString &str, ResourceItemDescriptor &descr);
#define R_SetFlags(item, flags) R_SetFlags1(item, flags, #flags)
bool R_SetFlags1(ResourceItem *item, qint64 flags, const char *strFlags);
#define R_ClearFlags(item, flags) R_ClearFlags1(item, flags, #flags)
bool R_ClearFlags1(ResourceItem *item, qint64 flags, const char *strFlags);
bool R_HasFlags(const ResourceItem *item, qint64 flags);

template <typename V>
bool R_SetValue(Resource *r, const char *suffix, const V &val, ResourceItem::ValueSource source)
{
    Q_ASSERT(r);
    Q_ASSERT(suffix);

    auto *item = r->item(suffix);
    if (!item)
    {
        return false;
    }

    return item->setValue(val, source);
}

template <typename V, typename EventEmitter>
bool R_SetValueEventOnChange(Resource *r, const char *suffix, const V &val, ResourceItem::ValueSource source, EventEmitter *eventEmitter = nullptr)
{
    Q_ASSERT(r);
    Q_ASSERT(suffix);
    Q_ASSERT(eventEmitter);

    auto *item = r->item(suffix);
    if (!item)
    {
        return false;
    }

    const auto result = item->setValue(val, source);

    if (result && item->lastChanged() != item->lastSet())
    {
        const auto *idItem = r->item(RAttrId);
        if (!idItem)
        {
            idItem = r->item(RAttrUniqueId);
        }
        Q_ASSERT(idItem);

        eventEmitter->enqueueEvent({r->prefix(), item->descriptor().suffix, idItem->toString(), item});
    }

    return result;
}

template <typename V, typename EventEmitter>
bool R_SetValueEventOnSet(Resource *r, const char *suffix, const V &val, ResourceItem::ValueSource source, EventEmitter *eventEmitter = nullptr)
{
    Q_ASSERT(r);
    Q_ASSERT(suffix);
    Q_ASSERT(eventEmitter);

    auto *item = r->item(suffix);
    if (!item)
    {
        return false;
    }

    const auto result = item->setValue(val, source);

    if (result)
    {
        const auto *idItem = r->item(RAttrId);
        if (!idItem)
        {
            idItem = r->item(RAttrUniqueId);
        }
        Q_ASSERT(idItem);

        eventEmitter->enqueueEvent({r->prefix(), item->descriptor().suffix, idItem->toString(), item});
    }

    return result;
}

bool R_AddResourceItemDescriptor(const ResourceItemDescriptor &rid);

bool isValidRConfigGroup(const QString &str);

uint8_t DDF_GetSubDeviceOrder(const QString &type);
QLatin1String R_DataTypeToString(ApiDataType type);
inline bool isValid(Resource::Handle hnd) { return hnd.hash != 0 && hnd.index < UINT16_MAX && hnd.type != 0; }
inline bool operator==(Resource::Handle a, Resource::Handle b) { return a.hash == b.hash && a.type == b.type; }

#endif // RESOURCE_H
