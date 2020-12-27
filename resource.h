#ifndef RESOURCE_H
#define RESOURCE_H

#include <QDateTime>
#include <vector>

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

// resource prefixes: /lights, /sensors, ...
extern const char *RSensors;
extern const char *RLights;
extern const char *RGroups;
extern const char *RConfig;

// resource events
extern const char *REventAdded;
extern const char *REventDeleted;
extern const char *REventValidGroup;
extern const char *REventCheckGroupAnyOn;

// resouce suffixes: state/buttonevent, config/on, ...
extern const char *RInvalidSuffix;

extern const char *RAttrName;
extern const char *RAttrManufacturerName;
extern const char *RAttrModelId;
extern const char *RAttrType;
extern const char *RAttrClass;
extern const char *RAttrId;
extern const char *RAttrUniqueId;
extern const char *RAttrSwVersion;
extern const char *RAttrLastAnnounced;
extern const char *RAttrLastSeen;

extern const char *RActionScene;

extern const char *RStateAirQuality;
extern const char *RStateAirQualityPpb;
extern const char *RStateAlarm;
extern const char *RStateAlert;
extern const char *RStateAllOn;
extern const char *RStateAngle;
extern const char *RStateAnyOn;
extern const char *RStateBattery;
extern const char *RStateBri;
extern const char *RStateButtonEvent;
extern const char *RStateCarbonMonoxide;
extern const char *RStateColorMode;
extern const char *RStateConsumption;
extern const char *RStateCurrent;
extern const char *RStateCt;
extern const char *RStateDark;
extern const char *RStateDaylight;
extern const char *RStateEffect;
extern const char *RStateErrorCode;
extern const char *RStateEventDuration;
extern const char *RStateFire;
extern const char *RStateFlag;
extern const char *RStateFloorTemperature;
extern const char *RStateGesture;
extern const char *RStateHeating;
extern const char *RStateHue;
extern const char *RStateHumidity;
extern const char *RStateLastCheckin; // Poll control check-in
extern const char *RStateLastSet;
extern const char *RStateLastUpdated;
extern const char *RStateLift;
extern const char *RStateLightLevel;
extern const char *RStateLowBattery;
extern const char *RStateLocaltime;
extern const char *RStateLux;
extern const char *RStateMountingModeActive;
extern const char *RStateOn;
extern const char *RStateOpen;
extern const char *RStateOrientationX;
extern const char *RStateOrientationY;
extern const char *RStateOrientationZ;
extern const char *RStatePresence;
extern const char *RStatePressure;
extern const char *RStatePower;
extern const char *RStateReachable;
extern const char *RStateSat;
extern const char *RStateSpectralX;
extern const char *RStateSpectralY;
extern const char *RStateSpectralZ;
extern const char *RStateSpeed;
extern const char *RStateStatus;
extern const char *RStateSunrise;
extern const char *RStateSunset;
extern const char *RStateTampered;
extern const char *RStateTemperature;
extern const char *RStateTest;
extern const char *RStateTilt;
extern const char *RStateTiltAngle;
extern const char *RStateUtc;
extern const char *RStateValve;
extern const char *RStateVibration;
extern const char *RStateVibrationStrength;
extern const char *RStateVoltage;
extern const char *RStateWater;
extern const char *RStateWindowOpen;
extern const char *RStateX;
extern const char *RStateY;

extern const char *RConfigAlert;
extern const char *RConfigBattery;
extern const char *RConfigColorCapabilities;
extern const char *RConfigCtMin;
extern const char *RConfigCtMax;
extern const char *RConfigConfigured;
extern const char *RConfigCoolSetpoint;
extern const char *RConfigDelay;
extern const char *RConfigDisplayFlipped;
extern const char *RConfigDuration;
extern const char *RConfigFanMode;
extern const char *RConfigGroup;
extern const char *RConfigHeatSetpoint;
extern const char *RConfigHostFlags;
extern const char *RConfigLastChangeAmount;
extern const char *RConfigLastChangeSource;
extern const char *RConfigLastChangeTime;
extern const char *RConfigId;
extern const char *RConfigLat;
extern const char *RConfigLedIndication;
extern const char *RConfigLocalTime;
extern const char *RConfigLocked;
extern const char *RConfigSetValve;
extern const char *RConfigLong;
extern const char *RConfigLevelMin;
extern const char *RConfigMode;
extern const char *RConfigMountingMode;
extern const char *RConfigOffset;
extern const char *RConfigOn;
extern const char *RConfigPending;
extern const char *RConfigPowerup;
extern const char *RConfigPowerOnCt;
extern const char *RConfigPowerOnLevel;
extern const char *RConfigPreset;
extern const char *RConfigReachable;
extern const char *RConfigSchedule;
extern const char *RConfigScheduleOn;
extern const char *RConfigSensitivity;
extern const char *RConfigSensitivityMax;
extern const char *RConfigSunriseOffset;
extern const char *RConfigSunsetOffset;
extern const char *RConfigSwingMode;
extern const char *RConfigTemperature;
extern const char *RConfigTemperatureMeasurement;
extern const char *RConfigTholdDark;
extern const char *RConfigTholdOffset;
extern const char *RConfigUrl;
extern const char *RConfigUsertest;
extern const char *RConfigWindowCoveringType;
extern const char *RConfigWindowOpen;
extern const char *RConfigUbisysJ1Mode;
extern const char *RConfigUbisysJ1WindowCoveringType;
extern const char *RConfigUbisysJ1ConfigurationAndStatus;
extern const char *RConfigUbisysJ1InstalledOpenLimitLift;
extern const char *RConfigUbisysJ1InstalledClosedLimitLift;
extern const char *RConfigUbisysJ1InstalledOpenLimitTilt;
extern const char *RConfigUbisysJ1InstalledClosedLimitTilt;
extern const char *RConfigUbisysJ1TurnaroundGuardTime;
extern const char *RConfigUbisysJ1LiftToTiltTransitionSteps;
extern const char *RConfigUbisysJ1TotalSteps;
extern const char *RConfigUbisysJ1LiftToTiltTransitionSteps2;
extern const char *RConfigUbisysJ1TotalSteps2;
extern const char *RConfigUbisysJ1AdditionalSteps;
extern const char *RConfigUbisysJ1InactivePowerThreshold;
extern const char *RConfigUbisysJ1StartupSteps;

#define R_ALERT_DEFAULT             QVariant(QLatin1String("none"))
#define R_SENSITIVITY_MAX_DEFAULT   2
#define R_THOLDDARK_DEFAULT         12000
#define R_THOLDOFFSET_DEFAULT       7000

extern const QStringList RStateEffectValues;
#define R_EFFECT_NONE               0
#define R_EFFECT_COLORLOOP          1
extern const QStringList RStateEffectValuesMueller;
#define R_EFFECT_SUNSET             2
#define R_EFFECT_PARTY              3
#define R_EFFECT_WORKLIGHT          4
#define R_EFFECT_CAMPFIRE           5
#define R_EFFECT_ROMANCE            6
#define R_EFFECT_NIGHTLIGHT         7
extern const QStringList RConfigLastChangeSourceValues;

#define R_PENDING_DELAY             (1 << 0)
#define R_PENDING_LEDINDICATION     (1 << 1)
#define R_PENDING_SENSITIVITY       (1 << 2)
#define R_PENDING_USERTEST          (1 << 3)
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

class  ResourceItemDescriptor
{
public:
    ResourceItemDescriptor() :
        type(DataTypeUnknown),
        suffix(RInvalidSuffix),
        validMin(0),
        validMax(0) { }

    ResourceItemDescriptor(ApiDataType t, const char *s, qint64 min = 0, qint64 max = 0) :
        type(t),
        suffix(s),
        validMin(min),
        validMax(max) { }

    bool isValid() const { return (type != DataTypeUnknown && suffix); }
    ApiDataType type;
    const char *suffix;
    qint64 validMin;
    qint64 validMax;
};

extern const ResourceItemDescriptor rInvalidItemDescriptor;

class ResourceItem
{
public:
    ResourceItem(const ResourceItem &other);
    ResourceItem(ResourceItem &&other);
    ResourceItem(const ResourceItemDescriptor &rid);
    ResourceItem &operator=(const ResourceItem &other);
    ResourceItem &operator=(ResourceItem &&other);
    ~ResourceItem();
    const QString &toString() const;
    qint64 toNumber() const;
    qint64 toNumberPrevious() const;
    bool toBool() const;
    QVariant toVariant() const;
    bool setValue(const QString &val);
    bool setValue(qint64 val);
    bool setValue(const QVariant &val);
    const ResourceItemDescriptor &descriptor() const;
    const QDateTime &lastSet() const;
    const QDateTime &lastChanged() const;
    void setTimeStamps(const QDateTime &t);
    void inRule(int ruleHandle);
    const std::vector<int> rulesInvolved() const;
    bool isPublic() const;
    void setIsPublic(bool isPublic);

private:
    ResourceItem() = delete;

    bool m_isPublic = true;
    qint64 m_num = 0;
    qint64 m_numPrev = 0;
    QString *m_str = nullptr;
    const ResourceItemDescriptor *m_rid = &rInvalidItemDescriptor;
    QDateTime m_lastSet;
    QDateTime m_lastChanged;
    std::vector<int> m_rulesInvolved; // the rules a resource item is trigger
};

class Resource
{
public:
    Resource(const char *prefix);
    ~Resource() = default;
    Resource(const Resource &other);
    Resource(Resource &&other);
    Resource &operator=(const Resource &other);
    Resource &operator=(Resource &&other);
    const char *prefix() const;
    ResourceItem *addItem(ApiDataType type, const char *suffix);
    void removeItem(const char *suffix);
    ResourceItem *item(const char *suffix);
    const ResourceItem *item(const char *suffix) const;
    bool toBool(const char *suffix) const;
    qint64 toNumber(const char *suffix) const;
    const QString &toString(const char *suffix) const;
    QVariant toVariant(const char *suffix) const;
    int itemCount() const;
    ResourceItem *itemForIndex(size_t idx);
    const ResourceItem *itemForIndex(size_t idx) const;
    QDateTime lastStatePush;
    QDateTime lastAttrPush;

private:
    Resource() = delete;
    const char *m_prefix = nullptr;
    std::vector<ResourceItem> m_rItems;
};

void initResourceDescriptors();
const char *getResourcePrefix(const QString &str);
bool getResourceItemDescriptor(const QString &str, ResourceItemDescriptor &descr);

#endif // RESOURCE_H
