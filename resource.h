#ifndef RESOURCE_H
#define RESOURCE_H

#include <QDateTime>
#include <QElapsedTimer>
#include <vector>
#include <deconz.h>

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

// resource prefixes: /devices, /lights, /sensors, ...
extern const char *RDevices;
extern const char *RSensors;
extern const char *RLights;
extern const char *RGroups;
extern const char *RConfig;

// resource events
extern const char *REventAdded;
extern const char *REventAwake;
extern const char *REventBindingTable;
extern const char *REventBindingTick;
extern const char *REventDeleted;
extern const char *REventPoll;
extern const char *REventValidGroup;
extern const char *REventCheckGroupAnyOn;
extern const char *REventNodeDescriptor;
extern const char *REventActiveEndpoints;
extern const char *REventSimpleDescriptor;
extern const char *REventStateEnter;
extern const char *REventStateLeave;
extern const char *REventStateTimeout;
extern const char *REventTick;

// resouce suffixes: state/buttonevent, config/on, ...
extern const char *RInvalidSuffix;

extern const char *RAttrName;
extern const char *RAttrManufacturerName;
extern const char *RAttrModelId;
extern const char *RAttrType;
extern const char *RAttrClass;
extern const char *RAttrId;
extern const char *RAttrUniqueId;
extern const char *RAttrProductId;
extern const char *RAttrSwVersion;
extern const char *RAttrLastAnnounced;
extern const char *RAttrLastSeen;
extern const char *RAttrExtAddress;
extern const char *RAttrNwkAddress;
extern const char *RAttrGroupAddress;

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
extern const char *RStateCt;
extern const char *RStateCurrent;
extern const char *RStateDark;
extern const char *RStateDaylight;
extern const char *RStateEffect;
extern const char *RStateErrorCode;
extern const char *RStateEventDuration;
extern const char *RStateFire;
extern const char *RStateFlag;
extern const char *RStateLockState;
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
extern const char *RConfigLock;
extern const char *RConfigBattery;
extern const char *RConfigColorCapabilities;
extern const char *RConfigConfigured;
extern const char *RConfigCoolSetpoint;
extern const char *RConfigCtMin;
extern const char *RConfigCtMax;
extern const char *RConfigCheckin;
extern const char *RConfigDelay;
extern const char *RConfigDeviceMode;
extern const char *RConfigDisplayFlipped;
extern const char *RConfigDuration;
extern const char *RConfigEnrolled;
extern const char *RConfigFanMode;
extern const char *RConfigGroup;
extern const char *RConfigHeatSetpoint;
extern const char *RConfigHostFlags;
extern const char *RConfigId;
extern const char *RConfigInterfaceMode;
extern const char *RConfigLastChangeAmount;
extern const char *RConfigLastChangeSource;
extern const char *RConfigLastChangeTime;
extern const char *RConfigLat;
extern const char *RConfigLedIndication;
extern const char *RConfigLevelMin;
extern const char *RConfigLocalTime;
extern const char *RConfigLocked;
extern const char *RConfigLong;
extern const char *RConfigMode;
extern const char *RConfigMountingMode;
extern const char *RConfigOffset;
extern const char *RConfigOn;
extern const char *RConfigPending;
extern const char *RConfigPowerup;
extern const char *RConfigPowerOnCt;
extern const char *RConfigPowerOnLevel;
extern const char *RConfigPulseConfiguration;
extern const char *RConfigPreset;
extern const char *RConfigMelody;
extern const char *RConfigTempThreshold;
extern const char *RConfigHumiThreshold;
extern const char *RConfigVolume;
extern const char *RConfigReachable;
extern const char *RConfigSchedule;
extern const char *RConfigScheduleOn;
extern const char *RConfigSensitivity;
extern const char *RConfigSensitivityMax;
extern const char *RConfigSetValve;
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
extern const char *RConfigExternalTemperatureSensor;
extern const char *RConfigExternalWindowOpen;
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

extern const QStringList RConfigDeviceModeValues;

#define R_PENDING_DELAY             (1 << 0)
#define R_PENDING_LEDINDICATION     (1 << 1)
#define R_PENDING_SENSITIVITY       (1 << 2)
#define R_PENDING_USERTEST          (1 << 3)
#define R_PENDING_WRITE_CIE_ADDRESS (1 << 4)
#define R_PENDING_ENROLL_RESPONSE   (1 << 5)
#define R_PENDING_MODE              (1 << 6)
#define R_PENDING_WRITE_POLL_CHECKIN_INTERVAL  (1 << 6)
#define R_PENDING_SET_LONG_POLL_INTERVAL       (1 << 7)
#define R_PENDING_DEVICEMODE        (1 << 8)

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
    ApiDataType type = DataTypeUnknown;
    const char *suffix = nullptr;
    qint64 validMin = 0;
    qint64 validMax = 0;
};

class Resource;
class ResourceItem;
class StateChange;

typedef bool (*ParseFunction_t)(Resource *r, ResourceItem *item, const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame);
typedef bool (*ReadFunction_t)(const Resource *r, const ResourceItem *item, deCONZ::ApsController *apsCtrl);
typedef bool (*WriteFunction_t)(const Resource *r, const ResourceItem *item, deCONZ::ApsController *apsCtrl);

struct ParseFunction
{
    ParseFunction(const QString &_name, const int _arity, ParseFunction_t _fn) :
        name(_name),
        arity(_arity),
        fn(_fn)
    { }
    QString name;
    int arity = 0; // number of parameters given by the device description file
    ParseFunction_t fn = nullptr;
};

struct ReadFunction
{
    ReadFunction(const QString &_name, const int _arity, ReadFunction_t _fn) :
        name(_name),
        arity(_arity),
        fn(_fn)
    { }
    QString name;
    int arity = 0; // number of parameters given by the device description file
    ReadFunction_t fn = nullptr;
};

struct WriteFunction
{
    WriteFunction(const QString &_name, const int _arity, WriteFunction_t _fn) :
        name(_name),
        arity(_arity),
        fn(_fn)
    { }
    QString name;
    int arity = 0; // number of parameters given by the device description file
    WriteFunction_t fn = nullptr;
};

extern const std::vector<ParseFunction> parseFunctions;
extern const std::vector<ReadFunction> readFunctions;
extern const std::vector<WriteFunction> writeFunctions;

ParseFunction_t getParseFunction(const std::vector<ParseFunction> &functions, const std::vector<QVariant> &params);
ReadFunction_t getReadFunction(const std::vector<ReadFunction> &functions, const std::vector<QVariant> &params);
WriteFunction_t getWriteFunction(const std::vector<WriteFunction> &functions, const std::vector<QVariant> &params);


int SC_WriteZclAttribute(const Resource *r, const StateChange *stateChange, deCONZ::ApsController *apsCtrl);
int SC_SetOnOff(const Resource *r, const StateChange *stateChange, deCONZ::ApsController *apsCtrl);

/*! \fn StateChangeFunction_t

    A state change function sends a certain ZCL command to a device to set a target state.
    For example: Sending On/off command to the on/off cluster.

    \returns 0 if the command was sent, or a negative number on failure.
 */
typedef int (*StateChangeFunction_t)(const Resource *r, const StateChange *stateChange, deCONZ::ApsController *apsCtrl);

/*! \class StateChange

    A generic helper to robustly set and verify state changes using ResourceItems.

    The main purpose of this helper is to ensure that a state will be set: For example
    a group cast to turn on 20 lights might not turn on all lights, in this case the
    StateChange detects that a light has not been turned on and can retry the respective command.

    A StateChange may have an arbitrary long "change-timeout" to support changing configurations
    for sleeping or not yet powered devices.

    StateChange is bound to a Resource and can be added by Resource::addStateChange(). Multiple
    StateChange items may be added if needed, for example to set on, brightness and color or to verify that a
    scene is called correctly, even if the scene cluster doesn't have the correct values stored in
    the device NVRAM.
 */
class StateChange
{
public:
    enum State
    {
        StateCallFunction, //! Calls the change function.
        StateWaitSync,     //! Waits until state is verified or a state-timeout occurs.
        StateRead,         //! When StateWaitSync timedout without receiving a value from the device.
        StateFinished,     //! The target state has been verified.
        StateFailed        //! The state changed failed after change-timeout.
    };

    enum SyncResult
    {
        VerifyUnknown,
        VerifySynced,
        VerifyNotSynced
    };

    /*! \struct StateChange::Item

        Specifies the target value of a specific item.
        There can be multiple Items involved in one state change.
     */
    struct Item
    {
        Item(const char *s, const QVariant &v) : suffix(s), targetValue(v) { }
        const char *suffix = nullptr; //! RStateOn, RStateBri, ...
        QVariant targetValue; //! The target value.
        SyncResult verified = VerifyUnknown;
    };

    /*! \struct StateChange::Param

        Specifies an extra parameter which might be needed to carry out a command.
        A Param usually isn't available as a ResourceItem, e.g. the transitiontime for a brightness change.
     */
    struct Param
    {
        QString name;
        QVariant value;
    };

    /*! Constructs a new StateChange.
        \param initialState - StateCallFunction or StateWaitSync.
        \param fn - the state change function.
        \param dstEndpoint - the endpoint to which the command should be send.

        StateCallFunction will call the state function in the next tick().
        StateWaitSync should be used when a command has already been sent, the state function will only
        be called when the state change can't be verified after state-timeout.
     */
    explicit StateChange(State initialState, StateChangeFunction_t fn, quint8 dstEndpoint);
    State state() const { return m_state; }
    State tick(Resource *r, deCONZ::ApsController *apsCtrl);
    void verifyItemChange(const ResourceItem *item);
    void addTargetValue(const char *suffix, const QVariant &value);
    void addParameter(const QString &name, const QVariant &value);
    bool operator==(const StateChange &other) const { return m_changeFunction == other.m_changeFunction; }
    const std::vector<Item> &items() const { return m_items; }
    const std::vector<Param> &parameters() const { return m_parameters; }
    quint8 dstEndpoint() const { return m_dstEndpoint; }
    void setChangeTimeoutMs(int timeout) { m_changeTimeoutMs = timeout; }

private:
    State m_state = StateCallFunction;
    StateChangeFunction_t m_changeFunction = nullptr; //! The function to send a respective ZCL command.

    quint8 m_dstEndpoint;
//    int m_stage = 0;
    int m_changeCalls = 0; //! Counts how often the change function was called (retries).
    int m_stateTimeoutMs = 1000 * 5; //! Inner timeout for states.
    int m_changeTimeoutMs = 1000 * 180; //! Max. duration for the whole change.
    QElapsedTimer m_stateTimer;
    QElapsedTimer m_changeTimer; //! Started once in the constructor.
    std::vector<Item> m_items;
    std::vector<Param> m_parameters;
};

extern const ResourceItemDescriptor rInvalidItemDescriptor;

class ResourceItem
{
    enum ItemFlags
    {
        FlagNeedPushSet     = 0x1, // set after a value has been set
        FlagNeedPushChange  = 0x2  // set when new value different than previous
    };

public:
    enum ValueSource
    {
        SourceUnknown,
        SourceDevice,
        SourceApi
    };

    ResourceItem(const ResourceItem &other);
    ResourceItem(ResourceItem &&other);
    ResourceItem(const ResourceItemDescriptor &rid);
    ResourceItem &operator=(const ResourceItem &other);
    ResourceItem &operator=(ResourceItem &&other);
    ~ResourceItem();
    bool needPushSet() const;
    bool needPushChange() const;
    void clearNeedPush();
    const QString &toString() const;
    qint64 toNumber() const;
    qint64 toNumberPrevious() const;
    bool toBool() const;
    QVariant toVariant() const;
    void setZclProperties(quint16 clusterId, quint16 attrId, quint8 endpoint = 0xff);
    bool setValue(const QString &val, ValueSource source = SourceUnknown);
    bool setValue(qint64 val, ValueSource source = SourceUnknown);
    bool setValue(const QVariant &val, ValueSource source = SourceUnknown);
    const ResourceItemDescriptor &descriptor() const;
    const QDateTime &lastSet() const;
    const QDateTime &lastChanged() const;
    void setTimeStamps(const QDateTime &t);
    void inRule(int ruleHandle);
    const std::vector<int> rulesInvolved() const;
    bool isPublic() const;
    void setIsPublic(bool isPublic);
    quint16 clusterId() const { return m_clusterId; }
    quint16 attributeId() const { return m_attributeId; }
    quint16 endpoint() const { return m_endpoint; }
    ParseFunction_t parseFunction() const { return m_parseFunction; }
    void setParseFunction(ParseFunction_t fn) { m_parseFunction = fn; }
    const std::vector<QVariant> &parseParameters() const { return m_parseParameters; }
    void setParseParameters(const std::vector<QVariant> &params);
    const std::vector<QVariant> &readParameters() const { return m_readParameters; }
    void setReadParameters(const std::vector<QVariant> &params);
    const std::vector<QVariant> &writeParameters() const { return m_writeParameters; }
    void setWriteParameters(const std::vector<QVariant> &params);
    ValueSource valueSource() const { return m_valueSource; }

private:
    ResourceItem() = delete;

    ValueSource m_valueSource = SourceUnknown;
    bool m_isPublic = true;
    quint16 m_flags = 0; // bitmap of ResourceItem::ItemFlags
    qint64 m_num = 0;
    qint64 m_numPrev = 0;
    QString *m_str = nullptr;
    const ResourceItemDescriptor *m_rid = &rInvalidItemDescriptor;
    QDateTime m_lastSet;
    QDateTime m_lastChanged;
    std::vector<int> m_rulesInvolved; // the rules a resource item is trigger
    /*! The clusterId, attributeId and endpoint values are used by the parse function
        To retreive the value from ZCL read/report commands.
    */
    quint16 m_clusterId = 0xFFFF;
    quint16 m_attributeId = 0xFFFF;
    quint8 m_endpoint = 0xFF;
    ParseFunction_t m_parseFunction = nullptr;
    std::vector<QVariant> m_parseParameters;
    std::vector<QVariant> m_readParameters;
    std::vector<QVariant> m_writeParameters;
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
    void addStateChange(const StateChange &stateChange);
    std::vector<StateChange> &stateChanges() { return m_stateChanges; }
    void cleanupStateChanges();
    Resource *parentResource() { return m_parent; }
    const Resource *parentResource() const { return m_parent; }
    void setParentResource(Resource *parent) { m_parent = parent; }

private:
    Resource() = delete;
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
const QString R_GetProductId(Resource *resource);

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

#endif // RESOURCE_H
