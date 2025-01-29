/*
 * Copyright (c) 2013-2024 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "de_web_plugin_private.h"
#include "sensor.h"
#include "json.h"
#include "product_match.h"

/*! Returns a fingerprint as JSON string. */
QString SensorFingerprint::toString() const
{
    if (endpoint == 0xFF || profileId == 0xFFFF)
    {
        return QString();
    }

    QVariantMap map;
    map["ep"] = (double)endpoint;
    map["p"] = (double)profileId;
    map["d"] = (double)deviceId;

    if (!inClusters.empty())
    {
        QVariantList ls;
        for (uint  i = 0; i < inClusters.size(); i++)
        {
            ls.append((double)inClusters[i]);
        }
        map["in"] = ls;
    }

    if (!outClusters.empty())
    {
        QVariantList ls;
        for (uint  i = 0; i < outClusters.size(); i++)
        {
            ls.append((double)outClusters[i]);
        }
        map["out"] = ls;
    }

    return Json::serialize(map);
}

/*! Parses a fingerprint from JSON string.
    \returns true on success
*/
bool SensorFingerprint::readFromJsonString(const QString &json)
{
    if (json.isEmpty())
    {
        return false;
    }

    bool ok = false;
    QVariant var = Json::parse(json, ok);

    if (!ok)
    {
        return false;
    }

    QVariantMap map = var.toMap();

    if (map.contains("ep") && map.contains("p") && map.contains("d"))
    {
        endpoint = map["ep"].toString().toUInt(&ok, 0);
        if (!ok) { return false; }
        profileId = map["p"].toString().toUInt(&ok, 0);
        if (!ok) { return false; }
        deviceId = map["d"].toString().toUInt(&ok, 0);
        if (!ok) { return false; }

        inClusters.clear();
        outClusters.clear();

        if (map.contains("in") && map["in"].type() == QVariant::List)
        {
            QVariantList ls = map["in"].toList();
            QVariantList::const_iterator i = ls.constBegin();
            QVariantList::const_iterator end = ls.constEnd();
            for (; i != end; ++i)
            {
                const quint16 clusterId = i->toString().toUInt(&ok, 0);
                if (ok)
                {
                    inClusters.push_back(clusterId);
                }
            }
        }

        if (map.contains("out") && map["out"].type() == QVariant::List)
        {
            QVariantList ls = map["out"].toList();
            QVariantList::const_iterator i = ls.constBegin();
            QVariantList::const_iterator end = ls.constEnd();
            for (; i != end; ++i)
            {
                const quint16 clusterId = i->toString().toUInt(&ok, 0);
                if (ok)
                {
                    outClusters.push_back(clusterId);
                }
            }
        }

        return true;
    }

    return false;
}

/*! Returns true if server cluster is part of the finger print.
 */
bool SensorFingerprint::hasInCluster(quint16 clusterId) const
{
    for (size_t i = 0; i < inClusters.size(); i++)
    {
        if (inClusters[i] == clusterId)
        {
            return true;
        }
    }

    return false;
}

/*! Returns true if server cluster is part of the finger print.
 */
bool SensorFingerprint::hasOutCluster(quint16 clusterId) const
{
    for (size_t i = 0; i < outClusters.size(); i++)
    {
        if (outClusters[i] == clusterId)
        {
            return true;
        }
    }

    return false;
}

/*! Constructor. */
Sensor::Sensor() :
    Resource(RSensors),
    m_deletedstate(Sensor::StateNormal),
    m_resetRetryCount(0)
{
    durationDue = QDateTime();

    // common sensor items
    addItem(DataTypeString, RAttrName);
    addItem(DataTypeString, RAttrManufacturerName);
    addItem(DataTypeUInt32, RAttrMode)->setValue(ModeScenes);
    addItem(DataTypeString, RAttrModelId);
    addItem(DataTypeString, RAttrType);
    addItem(DataTypeString, RAttrSwVersion);
    addItem(DataTypeString, RAttrId)->setIsPublic(false);
    addItem(DataTypeString, RAttrUniqueId);
    addItem(DataTypeTime, RAttrLastAnnounced);
    addItem(DataTypeTime, RAttrLastSeen);
    addItem(DataTypeBool, RConfigOn);
    addItem(DataTypeBool, RConfigReachable);
    addItem(DataTypeTime, RStateLastUpdated);

    previousDirection = 0xFF;
    previousCt = 0xFFFF;
    previousSequenceNumber = 0xFF;
    previousCommandId = 0xFF;
}

/*! Returns the sensor deleted state.
 */
Sensor::DeletedState Sensor::deletedState() const
{
    return m_deletedstate;
}

/*! Sets the sensor deleted state.
    \param deletedState the sensor deleted state
 */
void Sensor::setDeletedState(DeletedState deletedstate)
{
    m_deletedstate = deletedstate;
}

/*! Returns true if the sensor is reachable.
 */
bool Sensor::isAvailable() const
{
    const ResourceItem *i = item(RConfigReachable);
    if (i)
    {
        return i->toBool();
    }
    return true;
}

/*! Returns the sensor name.
 */
const QString &Sensor::name() const
{
   return item(RAttrName)->toString();
}

/*! Sets the sensor name.
    \param name the sensor name
 */
void Sensor::setName(const QString &name)
{
    item(RAttrName)->setValue(name);
}

/*! Returns the sensor mode.
 */
Sensor::SensorMode Sensor::mode() const
{
   return static_cast<Sensor::SensorMode>(item(RAttrMode)->toNumber());
}

/*! Sets the sensor mode
    \param mode the sensor mode
 */
void Sensor::setMode(SensorMode mode)
{
    item(RAttrMode)->setValue(static_cast<qint64>(mode));
}

/*! Returns the sensor type.
 */
const QString &Sensor::type() const
{
    return item(RAttrType)->toString();
}

/*! Sets the sensor type.
    \param type the sensor type
 */
void Sensor::setType(const QString &type)
{
    item(RAttrType)->setValue(type);
}

/*! Returns the sensor modelId.
 */
const QString &Sensor::modelId() const
{
    return item(RAttrModelId)->toString();
}

/*! Sets the sensor modelId.
    \param mid the sensor modelId
 */
void Sensor::setModelId(const QString &mid)
{
    item(RAttrModelId)->setValue(mid.trimmed());
}
/*! Handles admin when ResourceItem value has been set.
 * \param i ResourceItem
 */
void Sensor::didSetValue(ResourceItem *i)
{
    enqueueEvent(Event(RSensors, i->descriptor().suffix, id(), i));
    if (i->descriptor().suffix != RAttrLastSeen) // prevent flooding database writes
    {
        setNeedSaveDatabase(true);
    }
}

/*! Mark received command and update lastseen. */
void Sensor::rx()
{
    RestNodeBase *b = static_cast<RestNodeBase *>(this);
    b->rx();
    if (lastRx() >= item(RAttrLastSeen)->lastChanged().addSecs(plugin->gwLightLastSeenInterval))
    {
        setValue(RAttrLastSeen, lastRx().toUTC());
    }
}

/*! Returns the resetRetryCount.
 */
uint8_t Sensor::resetRetryCount() const
{
    return m_resetRetryCount;
}

/*! Sets the resetRetryCount.
    \param resetRetryCount the resetRetryCount
 */
void Sensor::setResetRetryCount(uint8_t resetRetryCount)
{
    m_resetRetryCount = resetRetryCount;
}

/*! Returns the zdpResetSeq number.
 */
uint8_t Sensor::zdpResetSeq() const
{
    return m_zdpResetSeq;
}

/*! Sets the zdpResetSeq number.
    \param resetRetryCount the resetRetryCount
 */
void Sensor::setZdpResetSeq(uint8_t zdpResetSeq)
{
    m_zdpResetSeq = zdpResetSeq;
}

void Sensor::updateStateTimestamp()
{
    ResourceItem *i = item(RStateLastUpdated);
    if (i)
    {
        i->setValue(QDateTime::currentDateTimeUtc());
    }
}

/*! Returns the sensor manufacturer.
 */
const QString &Sensor::manufacturer() const
{
    return item(RAttrManufacturerName)->toString();
}

/*! Sets the sensor manufacturer.
    \param manufacturer the sensor manufacturer
 */
void Sensor::setManufacturer(const QString &manufacturer)
{
    item(RAttrManufacturerName)->setValue(manufacturer.trimmed());
}

/*! Returns the sensor software version.
    Not supported for ZGP Sensortype
 */
const QString &Sensor::swVersion() const
{
    return item(RAttrSwVersion)->toString();
}

/*! Sets the sensor software version.
    \param swVersion the sensor software version
 */
void Sensor::setSwVersion(const QString &swversion)
{
    item(RAttrSwVersion)->setValue(swversion.trimmed());
}

/*! Returns the sensor last seen timestamp.
 */
const QString &Sensor::lastSeen() const
{
    static const QString s = QString("");

    const ResourceItem *i = item(RAttrLastSeen);
    return i ? i->toString() : s;
}

/*! Sets the sensor last seen timestamp.
    \param lastseen the sensor last seen timestamp
 */
void Sensor::setLastSeen(const QString &lastseen)
{
    ResourceItem *i = item(RAttrLastSeen);
    if (i)
    {
        QDateTime ls = QDateTime::fromString(lastseen, QLatin1String("yyyy-MM-ddTHH:mmZ"));
        ls.setTimeSpec(Qt::UTC);
        i->setValue(ls);
    }
}

/*! Returns the sensor last announced timestamp.
 */
const QString &Sensor::lastAnnounced() const
{
    static const QString s = QString("");

    const ResourceItem *i = item(RAttrLastAnnounced);
    return i ? i->toString() : s;
}

/*! Sets the sensor last announced timestamp.
    \param lastannounced the sensor last announced timestamp
 */
void Sensor::setLastAnnounced(const QString &lastannounced)
{
    ResourceItem *i = item(RAttrLastAnnounced);
    if (i)
    {
        QDateTime la = QDateTime::fromString(lastannounced, QLatin1String("yyyy-MM-ddTHH:mm:ssZ"));
        la.setTimeSpec(Qt::UTC);
        i->setValue(la);
    }
}

/*! Transfers state into JSONString.
 */
QString Sensor::stateToString()
{
    QVariantMap map;

    for (int i = 0; i < itemCount(); i++)
    {
        ResourceItem *item = itemForIndex(i);
        const ResourceItemDescriptor &rid = item->descriptor();

        if (strncmp(rid.suffix, "state/", 6) == 0)
        {
            const char *key = item->descriptor().suffix + 6;
            map[key] = item->toVariant();
        }
    }

    return Json::serialize(map);
}

/*! Transfers config into JSONString.
 */
QString Sensor::configToString()
{
    QVariantMap map;

    for (int i = 0; i < itemCount(); i++)
    {
        ResourceItem *item = itemForIndex(i);
        const ResourceItemDescriptor &rid = item->descriptor();

        if (strncmp(rid.suffix, "config/", 7) == 0)
        {
            const char *key = item->descriptor().suffix + 7;
            map[key] = item->toVariant();
        }
    }

    return Json::serialize(map);
}

/*! Parse the sensor state from a JSON string. */
void Sensor::jsonToState(const QString &json)
{
    bool ok;
    QVariant var = Json::parse(json, ok);

    if (!ok)
    {
        return;
    }

    QVariantMap map = var.toMap();

    if (map.contains("lastset"))
    {
        QString lastset = map["lastset"].toString();
        QString format = QLatin1String("yyyy-MM-ddTHH:mm:ssZ");
        QDateTime ls = QDateTime::fromString(lastset, format);
        ls.setTimeSpec(Qt::UTC);
        map["lastset"] = ls;
    }

    // use old time stamp before deCONZ was started
    QDateTime dt = QDateTime::currentDateTime().addSecs(-120);
    if (map.contains("lastupdated"))
    {
        QString lastupdated = map["lastupdated"].toString();
        QString format = lastupdated.length() == 19 ? QLatin1String("yyyy-MM-ddTHH:mm:ss") : QLatin1String("yyyy-MM-ddTHH:mm:ss.zzz");
        QDateTime lu = QDateTime::fromString(lastupdated, format);
        if (lu < dt)
        {
            dt = lu;
        }
        lu.setTimeSpec(Qt::UTC);
        map["lastupdated"] = lu;
    }

    if (map.contains("localtime"))
    {
        QString localtime = map["localtime"].toString();
        QString format = QLatin1String("yyyy-MM-ddTHH:mm:ss");
        QDateTime lt = QDateTime::fromString(localtime, format);
        map["localtime"] = lt;
    }

    if (map.contains("utc"))
    {
        QString utc = map["utc"].toString();
        QString format = QLatin1String("yyyy-MM-ddTHH:mm:ssZ");
        QDateTime u = QDateTime::fromString(utc, format);
        u.setTimeSpec(Qt::UTC);
        map["utc"] = u;
    }

    for (int i = 0; i < itemCount(); i++)
    {
        ResourceItem *item = itemForIndex(i);
        const ResourceItemDescriptor &rid = item->descriptor();

        if (strncmp(rid.suffix, "state/", 6) == 0)
        {
            const char *key = item->descriptor().suffix + 6;

            if (map.contains(QLatin1String(key)))
            {
                item->setValue(map[key]);
                item->setTimeStamps(dt);
            }
        }
    }
}

/*! Parse the sensor config from a JSON string. */
void Sensor::jsonToConfig(const QString &json)
{
    bool ok;

    QVariant var = Json::parse(json, ok);

    if (!ok)
    {
        return;
    }
    QVariantMap map = var.toMap();

    if (map.contains("lastchange_time"))
    {
        QString lastchange_time = map["lastchange_time"].toString();
        QString format = QLatin1String("yyyy-MM-ddTHH:mm:ssZ");
        QDateTime lct = QDateTime::fromString(lastchange_time, format);
        lct.setTimeSpec(Qt::UTC);
        map["lastchange_time"] = lct;
    }

    if (map.contains("battery") && type().startsWith(QLatin1String("CLIP")))
    {
        addItem(DataTypeUInt8, RConfigBattery);
    }

    QDateTime dt = QDateTime::currentDateTime().addSecs(-120);

    for (int i = 0; i < itemCount(); i++)
    {
        ResourceItem *item = itemForIndex(i);
        const ResourceItemDescriptor &rid = item->descriptor();

        if (type().startsWith(QLatin1String("CLIP")))
        {}
        else if (item->descriptor().suffix == RConfigReachable)
        { // set only from live data
            item->setValue(false);
            continue;
        }

        if (strncmp(rid.suffix, "config/", 7) == 0 && rid.suffix != RConfigPending)
        {
            const char *key = item->descriptor().suffix + 7;

            if (map.contains(QLatin1String(key)))
            {
                QVariant val = map[key];

                if (val.isNull())
                {
                    if (rid.suffix == RConfigOn)
                    {
                        map[key] = true; // default value
                        setNeedSaveDatabase(true);
                    }
                    else
                    {
                        continue;
                    }
                }

                item->setValue(map[key]);
                item->setTimeStamps(dt);
            }
        }
    }
}

/*! Returns the sensor fingerprint. */
SensorFingerprint &Sensor::fingerPrint()
{
    return m_fingerPrint;
}

/*! Returns the sensor fingerprint. */
const SensorFingerprint &Sensor::fingerPrint() const
{
    return m_fingerPrint;
}
