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
const char *RStateAlert = "state/alert";
const char *RStateAnyOn = "state/any_on";
const char *RStateButtonEvent = "state/buttonevent";
const char *RStateBri = "state/bri";
const char *RStateColorMode = "state/colormode";
const char *RStateCt = "state/ct";
const char *RStateEffect = "state/effect";
const char *RStateHue = "state/hue";
const char *RStatePresence = "state/presence";
const char *RStateOn = "state/on";
const char *RStateOpen = "state/open";
const char *RStateDark = "state/dark";
const char *RStateLightLevel = "state/lightlevel";
const char *RStateLux = "state/lux";
const char *RStateTemperature = "state/temperature";
const char *RStateHumidity = "state/humidity";
const char *RStateFlag = "state/flag";
const char *RStateReachable = "state/reachable";
const char *RStateSat = "state/sat";
const char *RStateStatus = "state/status";
const char *RStateDaylight = "state/daylight";
const char *RStateLastUpdated = "state/lastupdated";
const char *RStateX = "state/x";
const char *RStateY = "state/y";

const char *RConfigOn = "config/on";
const char *RConfigReachable = "config/reachable";
const char *RConfigConfigured = "config/configured";
const char *RConfigDuration = "config/duration";
const char *RConfigBattery = "config/battery";
const char *RConfigGroup = "config/group";
const char *RConfigUrl = "config/url";
const char *RConfigLat = "config/lat";
const char *RConfigLong = "config/long";
const char *RConfigSunriseOffset = "config/sunriseoffset";
const char *RConfigSunsetOffset = "config/sunsetoffset";

static std::vector<const char*> rPrefixes;
static std::vector<ResourceItemDescriptor> rItemDescriptors;
static std::vector<QString> rItemStrings; // string allocator: only grows, never shrinks

void initResourceDescriptors()
{
    rItemStrings.emplace_back(QString()); // invalid string on index 0

    // init resource lookup
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RStateAlert));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStateAnyOn));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt32, RStateButtonEvent));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, RStateBri));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RStateColorMode));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RStateCt));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RStateEffect));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RStateHue));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStatePresence));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStateOn));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStateOpen));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStateDark));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStateFlag));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RStateLightLevel));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt32, RStateLux));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt32, RStateTemperature));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt32, RStateHumidity));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStateReachable));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, RStateSat));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt32, RStateStatus));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RStateDaylight));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeTime, RStateLastUpdated));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RStateX));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RStateY));

    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RConfigOn));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RConfigReachable));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeBool, RConfigConfigured));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt8, RConfigBattery, 0, 100));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeUInt16, RConfigDuration));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RConfigGroup));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RConfigUrl));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RConfigLat));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeString, RConfigLong));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt8, RConfigSunriseOffset, -120, 120));
    rItemDescriptors.emplace_back(ResourceItemDescriptor(DataTypeInt8, RConfigSunsetOffset, -120, 120));
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

ResourceItem::ResourceItem(const ResourceItemDescriptor &rid) :
    m_num(0),
    m_strIndex(0),
    m_rid(rid)
{
    if (m_rid.type == DataTypeString ||
        m_rid.type == DataTypeTime ||
        m_rid.type == DataTypeTimePattern)
    {
        // alloc
        m_strIndex = rItemStrings.size();
        rItemStrings.emplace_back(QString());
    }
}

const QString &ResourceItem::toString() const
{
    if (m_rid.type == DataTypeString ||
        m_rid.type == DataTypeTimePattern)
    {
        DBG_Assert(m_strIndex < rItemStrings.size());
        if (m_strIndex < rItemStrings.size())
        {
            return rItemStrings[m_strIndex];
        }
    }
    else if (m_rid.type == DataTypeTime)
    {
        DBG_Assert(m_strIndex < rItemStrings.size());
        if (m_strIndex < rItemStrings.size())
        {
            if (m_rid.suffix == RStateLastUpdated)
            {
                QDateTime dt;
                dt.setOffsetFromUtc(0);
                dt.setMSecsSinceEpoch(m_num);
                rItemStrings[m_strIndex] = dt.toString("yyyy-MM-ddTHH:mm:ss");
            }
            else
            {
                rItemStrings[m_strIndex] = QDateTime::fromMSecsSinceEpoch(m_num).toString("yyyy-MM-ddTHH:mm:ss");
            }
            return rItemStrings[m_strIndex];
        }
    }

    DBG_Assert(!rItemStrings.empty());
    return rItemStrings[0]; // invalid string
}

qint64 ResourceItem::toNumber() const
{
    return m_num;
}

bool ResourceItem::toBool() const
{
    return m_num != 0;
}

bool ResourceItem::setValue(const QString &val)
{
    DBG_Assert(m_strIndex < rItemStrings.size());
    if (m_strIndex < rItemStrings.size())
    {
        m_lastSet = QDateTime::currentDateTime();
        if (rItemStrings[m_strIndex] != val)
        {
            rItemStrings[m_strIndex] = val;
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

    if (m_num != val)
    {
        m_num = val;
        m_lastChanged = m_lastSet;
    }

    return true;
}

bool ResourceItem::setValue(const QVariant &val)
{
    QDateTime now = QDateTime::currentDateTime();

    if (m_rid.type == DataTypeString ||
        m_rid.type == DataTypeTimePattern)
    {
        // TODO validate time pattern
        DBG_Assert(m_strIndex < rItemStrings.size());
        if (m_strIndex < rItemStrings.size())
        {
            m_lastSet = now;
            if (rItemStrings[m_strIndex] != val.toString())
            {
                rItemStrings[m_strIndex] = val.toString();
                m_lastChanged = m_lastSet;
            }
            return true;
        }
    }
    else if (m_rid.type == DataTypeBool)
    {
        m_lastSet = now;
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
            QDateTime dt = QDateTime::fromString(val.toString(), "yyyy-MM-ddTHH:mm:ss");

            if (dt.isValid())
            {
                m_lastSet = now;
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

QVariant ResourceItem::toVariant() const
{
    if (m_rid.type == DataTypeString ||
        m_rid.type == DataTypeTimePattern)
    {
        DBG_Assert(m_strIndex < rItemStrings.size());
        if (m_strIndex < rItemStrings.size())
        {
            return rItemStrings[m_strIndex];
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
