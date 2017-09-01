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
    DataTypeInt8,
    DataTypeInt16,
    DataTypeInt32,
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

extern const char *RStateAlert;
extern const char *RStateAllOn;
extern const char *RStateAnyOn;
extern const char *RStateBri;
extern const char *RStateButtonEvent;
extern const char *RStateColorMode;
extern const char *RStateCt;
extern const char *RStateDark;
extern const char *RStateDaylight;
extern const char *RStateEffect;
extern const char *RStateFlag;
extern const char *RStateHue;
extern const char *RStateHumidity;
extern const char *RStateLastUpdated;
extern const char *RStateLightLevel;
extern const char *RStateLux;
extern const char *RStateOn;
extern const char *RStateOpen;
extern const char *RStatePresence;
extern const char *RStatePressure;
extern const char *RStateReachable;
extern const char *RStateSat;
extern const char *RStateStatus;
extern const char *RStateTemperature;
extern const char *RStateX;
extern const char *RStateY;

extern const char *RConfigAlert;
extern const char *RConfigBattery;
extern const char *RConfigConfigured;
extern const char *RConfigDuration;
extern const char *RConfigGroup;
extern const char *RConfigLat;
extern const char *RConfigLedIndication;
extern const char *RConfigLocalTime;
extern const char *RConfigLong;
extern const char *RConfigOn;
extern const char *RConfigReachable;
extern const char *RConfigSensitivity;
extern const char *RConfigSensitivityMax;
extern const char *RConfigSunriseOffset;
extern const char *RConfigSunsetOffset;
extern const char *RConfigTholdDark;
extern const char *RConfigTholdOffset;
extern const char *RConfigUrl;
extern const char *RConfigUsertest;

#define R_ALERT_DEFAULT             QVariant(QLatin1String("none"))
#define R_SENSITIVITY_MAX_DEFAULT   2
#define R_THOLDDARK_DEFAULT         12000
#define R_THOLDOFFSET_DEFAULT       7000

class  ResourceItemDescriptor
{
public:
    ResourceItemDescriptor() :
        type(DataTypeUnknown),
        suffix(RInvalidSuffix),
        validMin(0),
        validMax(0) { }

    ResourceItemDescriptor(ApiDataType t, const char *s, int min = 0, int max = 0) :
        type(t),
        suffix(s),
        validMin(min),
        validMax(max) { }

    bool isValid() const { return (type != DataTypeUnknown && suffix); }
    ApiDataType type;
    const char *suffix;
    int validMin;
    int validMax;
};

class ResourceItem
{
public:
    ResourceItem(const ResourceItemDescriptor &rid);
    const QString &toString() const;
    qint64 toNumber() const;
    bool toBool() const;
    QVariant toVariant() const;
    bool setValue(const QString &val);
    bool setValue(qint64 val);
    bool setValue(const QVariant &val);
    const ResourceItemDescriptor &descriptor() const;
    const QDateTime &lastSet() const;
    const QDateTime &lastChanged() const;
    void setTimeStamps(const QDateTime &t);

private:
    ResourceItem() :
        m_num(-1), m_strIndex(0) {}

    qint64 m_num;
    size_t m_strIndex;
    ResourceItemDescriptor m_rid;
    QDateTime m_lastSet;
    QDateTime m_lastChanged;
};

class Resource
{
public:
    Resource(const char *prefix);
    const char *prefix() const;
    ResourceItem *addItem(ApiDataType type, const char *suffix);
    ResourceItem *item(const char *suffix);
    const ResourceItem *item(const char *suffix) const;
    bool toBool(const char *suffix) const;
    qint64 toNumber(const char *suffix) const;
    const QString &toString(const char *suffix) const;
    int itemCount() const;
    ResourceItem *itemForIndex(size_t idx);
    const ResourceItem *itemForIndex(size_t idx) const;
    void inRule(int ruleHandle);
    const std::vector<int> rulesInvolved() const;

private:
    Resource() {}
    const char *m_prefix;
    std::vector<ResourceItem> m_rItems;
    std::vector<int> m_rulesInvolved; // the rules a resource is involved
};

void initResourceDescriptors();
const char *getResourcePrefix(const QString &str);
bool getResourceItemDescriptor(const QString &str, ResourceItemDescriptor &descr);

#endif // RESOURCE_H
