/*
 * Copyright (c) 2017 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QString>

#include "deconz.h"
#include "resource.h"

const char *RSensors = "/sensors";
const char *RLights = "/lights";
const char *RGroups = "/groups";
const char *RConfig = "/config";

const char *REventAdded = "event/added";
const char *REventDeleted = "event/deleted";
const char *REventValidGroup = "event/validgroup";
const char *REventCheckGroupAnyOn = "event/checkgroupanyon";

const char *RInvalidSuffix = "invalid/suffix";

const char *RAttrName = "attr/name";
const char *RAttrModelId = "attr/modelid";
const char *RAttrType = "attr/type";
const char *RAttrClass = "attr/class";
const char *RAttrUniqueId = "attr/uniqueid";

const char *RStateAlarm = "state/alarm";
const char *RStateAlert = "state/alert";
const char *RStateAllOn = "state/all_on";
const char *RStateAnyOn = "state/any_on";
const char *RStateBri = "state/bri";
const char *RStateButtonEvent = "state/buttonevent";
const char *RStateCarbonMonoxide = "state/carbonmonoxide";
const char *RStateColorMode = "state/colormode";
const char *RStateConsumption = "state/consumption";
const char *RStateCurrent = "state/current";
const char *RStateCt = "state/ct";
const char *RStateDark = "state/dark";
const char *RStateDaylight = "state/daylight";
const char *RStateEffect = "state/effect";
const char *RStateFire = "state/fire";
const char *RStateFlag = "state/flag";
const char *RStateHue = "state/hue";
const char *RStateHumidity = "state/humidity";
const char *RStateLastUpdated = "state/lastupdated";
const char *RStateLightLevel = "state/lightlevel";
const char *RStateLowBattery = "state/lowbattery";
const char *RStateLux = "state/lux";
const char *RStateOn = "state/on";
const char *RStateOpen = "state/open";
const char *RStatePresence = "state/presence";
const char *RStatePressure = "state/pressure";
const char *RStatePower = "state/power";
const char *RStateReachable = "state/reachable";
const char *RStateSat = "state/sat";
const char *RStateStatus = "state/status";
const char *RStateTampered = "state/tampered";
const char *RStateTemperature = "state/temperature";
const char *RStateVoltage = "state/voltage";
const char *RStateWater = "state/water";
const char *RStateX = "state/x";
const char *RStateY = "state/y";

const char *RConfigAlert = "config/alert";
const char *RConfigBattery = "config/battery";
const char *RConfigColorCapabilities = "config/colorcapabilities";
const char *RConfigCtMin = "config/ctmin";
const char *RConfigCtMax = "config/ctmax";
const char *RConfigConfigured = "config/configured";
const char *RConfigDelay = "config/delay";
const char *RConfigDuration = "config/duration";
const char *RConfigGroup = "config/group";
const char *RConfigLat = "config/lat";
const char *RConfigLedIndication = "config/ledindication";
const char *RConfigLocalTime = "config/localtime";
const char *RConfigLong = "config/long";
const char *RConfigOffset = "config/offset";
const char *RConfigOn = "config/on";
const char *RConfigPending = "config/pending";
const char *RConfigReachable = "config/reachable";
const char *RConfigSensitivity = "config/sensitivity";
const char *RConfigSensitivityMax = "config/sensitivitymax";
const char *RConfigSunriseOffset = "config/sunriseoffset";
const char *RConfigSunsetOffset = "config/sunsetoffset";
const char *RConfigTemperature = "config/temperature";
const char *RConfigTholdDark = "config/tholddark";
const char *RConfigTholdOffset = "config/tholdoffset";
const char *RConfigUrl = "config/url";
const char *RConfigUsertest = "config/usertest";


static std::vector<const char*> rPrefixes;
static std::vector<ResourceItemDescriptor> rItemDescriptors;
static std::vector<QString> rItemStrings; // string allocator: only grows, never shrinks

void initResourceDescriptors()
{
    rItemStrings.emplace_back(QString()); // invalid string on index 0

    // init resource lookup
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RAttrName));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RAttrModelId));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RAttrType));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RAttrClass));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RAttrUniqueId));

    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStateAlarm));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RStateAlert));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStateAllOn));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStateAnyOn));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, RStateBri));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt32, RStateButtonEvent));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStateCarbonMonoxide));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RStateColorMode));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt64, RStateConsumption, 0, 0xffffffffffff));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RStateCurrent));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RStateCt));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStateDark));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStateDaylight));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RStateEffect));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStateFire));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStateFlag));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RStateHue));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RStateHumidity, 0, 10000));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeTime, RStateLastUpdated));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RStateLightLevel, 0, 0xfffe));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStateLowBattery));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt32, RStateLux));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStateOn));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStateOpen));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStatePresence));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt16, RStatePressure, 0, 32767));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt16, RStatePower));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStateReachable));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, RStateSat));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt32, RStateStatus));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStateTampered));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt16, RStateTemperature, -27315, 32767));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RStateVoltage));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStateWater));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RStateX));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RStateY));

    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RConfigAlert));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, RConfigBattery, 0, 100));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RConfigColorCapabilities));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RConfigCtMin));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RConfigCtMax));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RConfigConfigured));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RConfigDelay));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RConfigDuration));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RConfigGroup));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RConfigLat));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RConfigLedIndication));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeTime, RConfigLocalTime));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RConfigLong));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt16, RConfigOffset, -500, 500));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RConfigOn));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, RConfigPending));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RConfigReachable));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, RConfigSensitivity));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, RConfigSensitivityMax));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt8, RConfigSunriseOffset, -120, 120));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt8, RConfigSunsetOffset, -120, 120));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt16, RConfigTemperature, -27315, 32767));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RConfigTholdDark, 0, 0xfffe));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RConfigTholdOffset, 1, 0xfffe));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RConfigUrl));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RConfigUsertest));
}

const char *getResourcePrefix(const QString &str)
{
    Q_UNUSED(str);
    return 0;
}

bool getResourceItemDescriptor(const QString &str, ResourceItemDescriptor &descr)
{
    std::vector<ResourceItemDescriptor>::const_iterator i = rItemDescriptors.begin();
    std::vector<ResourceItemDescriptor>::const_iterator end = rItemDescriptors.end();

    for (; i != end; ++i)
    {
        if (str.endsWith(QLatin1String(i->suffix)))
        {
                descr = *i;
                return true;
        }
    }

    return false;
}

ResourceItem::ResourceItem(const ResourceItem &other)
{
    *this = other;
    if (m_str)
    {
        m_str = new QString;
        *m_str = other.toString();
    }
}

ResourceItem::~ResourceItem()
{
    DBG_Printf(DBG_INFO_L2, "~ResourceItem() %s -- str %p\n", m_rid.suffix, m_str);
    if (m_str)
    {
        delete m_str;
        m_str = 0;
    }
}

ResourceItem &ResourceItem::operator=(const ResourceItem &other)
{
    // self assignment?
    if (this == &other)
    {
        return *this;
    }

    m_num = other.m_num;
    m_numPrev = other.m_numPrev;
    m_rid = other.m_rid;
    m_lastSet = other.lastSet();
    m_lastChanged = other.lastChanged();
    m_rulesInvolved = other.rulesInvolved();
    m_str = 0;

    if (other.m_str)
    {
        m_str = new QString;
        *m_str = other.toString();
    }

    return *this;
}


ResourceItem::ResourceItem(const ResourceItemDescriptor &rid) :
    m_num(0),
    m_numPrev(0),
    m_str(0),
    m_rid(rid)
{
    if (m_rid.type == DataTypeString ||
        m_rid.type == DataTypeTime ||
        m_rid.type == DataTypeTimePattern)
    {
        m_str = new QString;
    }
}

const QString &ResourceItem::toString() const
{
    if (m_rid.type == DataTypeString ||
        m_rid.type == DataTypeTimePattern)
    {
        if (m_str)
        {
            return *m_str;
        }
    }
    else if (m_rid.type == DataTypeTime)
    {
        if (m_str)
        {
            if (m_rid.suffix == RStateLastUpdated)
            {
                QDateTime dt;
                dt.setOffsetFromUtc(0);
                dt.setMSecsSinceEpoch(m_num);
                *m_str = dt.toString("yyyy-MM-ddTHH:mm:ss");
            }
            else
            {
                *m_str = QDateTime::fromMSecsSinceEpoch(m_num).toString("yyyy-MM-ddTHH:mm:ss");
            }
            return *m_str;
        }
    }

    DBG_Assert(!rItemStrings.empty());
    return rItemStrings[0]; // invalid string
}

qint64 ResourceItem::toNumber() const
{
    return m_num;
}

qint64 ResourceItem::toNumberPrevious() const
{
    return m_numPrev;
}

bool ResourceItem::toBool() const
{
    return m_num != 0;
}

bool ResourceItem::setValue(const QString &val)
{
    if (m_str)
    {
        m_lastSet = QDateTime::currentDateTime();
        if (*m_str != val)
        {
            *m_str = val;
            m_lastChanged = m_lastSet;
        }
        return true;
    }

    return false;
}

bool ResourceItem::setValue(qint64 val)
{
    if (m_rid.validMin != 0 || m_rid.validMax != 0)
    {
        // range check
        if (val < m_rid.validMin || val > m_rid.validMax)
        {
            return false;
        }
    }

    m_lastSet = QDateTime::currentDateTime();
    m_numPrev = m_num;

    if (m_num != val)
    {
        m_num = val;
        m_lastChanged = m_lastSet;
    }

    return true;
}

bool ResourceItem::setValue(const QVariant &val)
{
    if (!val.isValid())
    {
        m_lastSet = QDateTime();
        m_lastChanged = m_lastSet;
        return true;
    }

    QDateTime now = QDateTime::currentDateTime();

    if (m_rid.type == DataTypeString ||
        m_rid.type == DataTypeTimePattern)
    {
        // TODO validate time pattern
        if (m_str)
        {
            m_lastSet = now;
            if (*m_str != val.toString())
            {
                *m_str = val.toString();
                m_lastChanged = m_lastSet;
            }
            return true;
        }
    }
    else if (m_rid.type == DataTypeBool)
    {
        m_lastSet = now;
        m_numPrev = m_num;

        if (m_num != val.toBool())
        {
            m_num = val.toBool();
            m_lastChanged = m_lastSet;
        }
        return true;
    }
    else if (m_rid.type == DataTypeTime)
    {
        if (val.type() == QVariant::String)
        {
            QDateTime dt = QDateTime::fromString(val.toString(), QLatin1String("yyyy-MM-ddTHH:mm:ss"));

            if (dt.isValid())
            {
                m_lastSet = now;
                m_numPrev = m_num;

                if (m_num != dt.toMSecsSinceEpoch())
                {
                    m_num = dt.toMSecsSinceEpoch();
                    m_lastChanged = m_lastSet;
                }
                return true;
            }
        }
        else if (val.type() == QVariant::DateTime)
        {
            m_lastSet = now;
            m_numPrev = m_num;

            if (m_num != val.toDateTime().toMSecsSinceEpoch())
            {
                m_num = val.toDateTime().toMSecsSinceEpoch();
                m_lastChanged = m_lastSet;
            }
            return true;
        }
    }
    else
    {
        bool ok;
        int n = val.toInt(&ok);
        if (ok)
        {
            if (m_rid.validMin == 0 && m_rid.validMax == 0)
            { /* no range check */ }
            else if (n >= m_rid.validMin && n <= m_rid.validMax)
            {   /* range check: ok*/ }
            else {
                return false;
            }

            m_lastSet = now;
            m_numPrev = m_num;

            if (m_num != n)
            {
                m_num = n;
                m_lastChanged = m_lastSet;
            }
            return true;
        }
    }

    return false;
}

const ResourceItemDescriptor &ResourceItem::descriptor() const
{
    return m_rid;
}

const QDateTime &ResourceItem::lastSet() const
{
    return m_lastSet;
}

const QDateTime &ResourceItem::lastChanged() const
{
    return m_lastChanged;
}

void ResourceItem::setTimeStamps(const QDateTime &t)
{
    m_lastSet = t;
    m_lastChanged = t;
}

QVariant ResourceItem::toVariant() const
{
    if (!m_lastSet.isValid())
    {
        return QVariant();
    }

    if (m_rid.type == DataTypeString ||
        m_rid.type == DataTypeTimePattern)
    {
        if (m_str)
        {
            return *m_str;
        }
        return QString();
    }
    else if (m_rid.type == DataTypeBool)
    {
        return (bool)m_num;
    }
    else if (m_rid.type == DataTypeTime)
    {
        return toString();
    }
    else
    {
       return (double)m_num;
    }

    return QVariant();
}

/*! Marks the resource item as involved in a rule. */
void ResourceItem::inRule(int ruleHandle)
{
    for (quint16 handle : m_rulesInvolved)
    {
        if (handle == ruleHandle)
        {
            return;
        }
    }

    m_rulesInvolved.push_back(ruleHandle);
}

/*! Returns the rules handles in which the resource item is involved. */
const std::vector<int> ResourceItem::rulesInvolved() const
{
    return m_rulesInvolved;
}

Resource::Resource(const char *prefix) :
    m_prefix(prefix)
{
}

const char *Resource::prefix() const
{
    return m_prefix;
}

ResourceItem *Resource::addItem(ApiDataType type, const char *suffix)
{
    ResourceItem *it = item(suffix);
    if (!it) // prevent double insertion
    {
        std::vector<ResourceItemDescriptor>::const_iterator i = rItemDescriptors.begin();
        std::vector<ResourceItemDescriptor>::const_iterator end = rItemDescriptors.end();

        for (; i != end; ++i)
        {
            if (i->suffix == suffix && i->type == type)
            {
                m_rItems.emplace_back(ResourceItem(*i));
                return &m_rItems.back();
            }
        }

        DBG_Assert(0);
        DBG_Printf(DBG_ERROR, "unknown datatype:suffix +  %d: %s\n", type, suffix);
    }

    return it;
}

void Resource::removeItem(const char *suffix)
{
    std::vector<ResourceItem>::iterator i = m_rItems.begin();
    std::vector<ResourceItem>::iterator end = m_rItems.end();

    for (; i != end; ++i)
    {
        if (i->descriptor().suffix != suffix)
        {
            continue;
        }

        *i = m_rItems.back();
        m_rItems.pop_back();
        return;
    }
}

ResourceItem *Resource::item(const char *suffix)
{
    for (size_t i = 0; i < m_rItems.size(); i++)
    {
        if (m_rItems[i].descriptor().suffix == suffix)
        {
            return &m_rItems[i];
        }
    }

    return 0;
}

const ResourceItem *Resource::item(const char *suffix) const
{
    for (size_t i = 0; i < m_rItems.size(); i++)
    {
        if (m_rItems[i].descriptor().suffix == suffix)
        {
            return &m_rItems[i];
        }
    }

    return 0;
}

bool Resource::toBool(const char *suffix) const
{
    const ResourceItem *i = item(suffix);
    if (i)
    {
        return i->toBool();
    }
    return false;
}

qint64 Resource::toNumber(const char *suffix) const
{
    const ResourceItem *i = item(suffix);
    if (i)
    {
        return i->toNumber();
    }
    return 0;
}

const QString &Resource::toString(const char *suffix) const
{
    const ResourceItem *i = item(suffix);
    if (i)
    {
        return i->toString();
    }

    DBG_Assert(!rItemStrings.empty());
    return rItemStrings[0]; // invalid string
}

int Resource::itemCount() const
{
    return m_rItems.size();
}

ResourceItem *Resource::itemForIndex(size_t idx)
{
    if (idx < m_rItems.size())
    {
        return &m_rItems[idx];
    }
    return 0;
}

const ResourceItem *Resource::itemForIndex(size_t idx) const
{
    if (idx < m_rItems.size())
    {
        return &m_rItems[idx];
    }
    return 0;
}
