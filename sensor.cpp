/*
 * Copyright (c) 2013-2020 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "sensor.h"
#include "json.h"

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

    return deCONZ::jsonStringFromMap(map);
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
        endpoint = map["ep"].toUInt(&ok);
        if (!ok) { return false; }
        profileId = map["p"].toUInt(&ok);
        if (!ok) { return false; }
        deviceId = map["d"].toUInt(&ok);
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
                quint16 clusterId = i->toUInt(&ok);
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
                quint16 clusterId = i->toUInt(&ok);
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
    m_mode(ModeTwoGroups),
    m_resetRetryCount(0),
    m_rxCounter(0)
{
    QDateTime now = QDateTime::currentDateTime();
    lastStatePush = now;
    lastConfigPush = now;
    durationDue = QDateTime();

    // common sensor items
    addItem(DataTypeString, RAttrName);
    addItem(DataTypeString, RAttrManufacturerName);
    addItem(DataTypeString, RAttrModelId);
    addItem(DataTypeString, RAttrType);
    addItem(DataTypeString, RAttrSwVersion);
    addItem(DataTypeString, RAttrId);
    addItem(DataTypeString, RAttrUniqueId);
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
   return m_mode;
}

/*! Sets the sensor mode (Lighting Switch).
 * 1 = Secenes
 * 2 = Groups
 * 3 = Color Temperature
    \param mode the sensor mode
 */
void Sensor::setMode(SensorMode mode)
{
    m_mode = mode;
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
        m_rxCounter++;
    }
}

/*! Increments the number of received commands during this session. */
void Sensor::incrementRxCounter()
{
    m_rxCounter++;
}

/*! Returns number of received commands during this session. */
int Sensor::rxCounter() const
{
    return m_rxCounter;
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

const std::vector<Sensor::ButtonMap> Sensor::buttonMap(const QMap<QString, std::vector<Sensor::ButtonMap>> &buttonMapData)
{
    if (m_buttonMap.empty())
    {
        const QString &modelid = item(RAttrModelId)->toString();
        const QString &manufacturer = item(RAttrManufacturerName)->toString();
        if (manufacturer == QLatin1String("dresden elektronik"))
        {
            if      (modelid == QLatin1String("Lighting Switch")) { m_buttonMap = buttonMapData.value("deLightingSwitchMap"); }
            else if (modelid == QLatin1String("Scene Switch"))    { m_buttonMap = buttonMapData.value("deSceneSwitchMap"); }
        }
        else if (manufacturer == QLatin1String("Insta"))
        {
            if      (modelid.endsWith(QLatin1String("_1")))       { m_buttonMap = buttonMapData.value("instaRemoteMap"); }
            if      (modelid.contains(QLatin1String("Remote")))   { m_buttonMap = buttonMapData.value("instaRemoteMap"); }
        }
        else if (manufacturer == QLatin1String("Busch-Jaeger"))
        {
            m_buttonMap = buttonMapData.value("bjeSwitchMap");
        }
        else if (manufacturer.startsWith(QLatin1String("IKEA")))
        {
            if      (modelid.startsWith(QLatin1String("TRADFRI remote control"))) { m_buttonMap = buttonMapData.value("ikeaRemoteMap"); }
            else if (modelid.startsWith(QLatin1String("TRADFRI motion sensor"))) { m_buttonMap = buttonMapData.value("ikeaMotionSensorMap"); }
            else if (modelid.startsWith(QLatin1String("TRADFRI wireless dimmer"))) { m_buttonMap = buttonMapData.value("ikeaDimmerMap"); }
            else if (modelid.startsWith(QLatin1String("TRADFRI on/off switch"))) { m_buttonMap = buttonMapData.value("ikeaOnOffMap"); }
            else if (modelid.startsWith(QLatin1String("TRADFRI open/close remote"))) { m_buttonMap = buttonMapData.value("ikeaOpenCloseMap"); }
            else if (modelid.startsWith(QLatin1String("SYMFONISK"))) { m_buttonMap = buttonMapData.value("ikeaSoundControllerMap"); }
        }
        else if (manufacturer.startsWith(QLatin1String("OSRAM")))
        {
            if      (modelid.startsWith(QLatin1String("Lightify Switch Mini")))     { m_buttonMap = buttonMapData.value("osramMiniRemoteMap"); }
            else if (modelid.startsWith(QLatin1String("Switch 4x EU-LIGHTIFY")))    { m_buttonMap = buttonMapData.value("osram4ButRemoteMap"); }
            else if (modelid.startsWith(QLatin1String("Switch 4x-LIGHTIFY")))       { m_buttonMap = buttonMapData.value("osram4ButRemoteMap"); }
            else if (modelid.startsWith(QLatin1String("Switch-LIGHTIFY")))          { m_buttonMap = buttonMapData.value("osram4ButRemoteMap2"); }
        }
        else if (manufacturer == QLatin1String("ubisys"))
        {
            if      (modelid.startsWith(QLatin1String("D1"))) { m_buttonMap = buttonMapData.value("ubisysD1Map"); }
            else if (modelid.startsWith(QLatin1String("C4"))) { m_buttonMap = buttonMapData.value("ubisysC4Map"); }
            else if (modelid.startsWith(QLatin1String("S1"))) { m_buttonMap = buttonMapData.value("ubisysD1Map"); }
            else if (modelid.startsWith(QLatin1String("S2"))) { m_buttonMap = buttonMapData.value("ubisysS2Map"); }
        }
        else if (manufacturer == QLatin1String("LUMI"))
        {
            if      (modelid == QLatin1String("lumi.sensor_switch"))      { m_buttonMap = buttonMapData.value("xiaomiSwitchMap"); }
            else if (modelid == QLatin1String("lumi.sensor_switch.aq2"))  { m_buttonMap = buttonMapData.value("xiaomiSwitchAq2Map"); }
            else if (modelid.startsWith(QLatin1String("lumi.vibration"))) { m_buttonMap = buttonMapData.value("xiaomiVibrationMap"); }
            else if (modelid.endsWith(QLatin1String("86opcn01")))  { m_buttonMap = buttonMapData.value("aqaraOpple6Map"); }
        }
        else if (manufacturer == QLatin1String("Lutron"))
        {
            if      (modelid.startsWith(QLatin1String("LZL4BWHL")))       { m_buttonMap = buttonMapData.value("lutronLZL4BWHLSwitchMap"); }
            else if (modelid.startsWith(QLatin1String("Z3-1BRL")))        { m_buttonMap = buttonMapData.value("lutronAuroraMap"); }

        }
        else if (manufacturer == QLatin1String("Trust"))
        {
            if      (modelid == QLatin1String("ZYCT-202"))      { m_buttonMap = buttonMapData.value("trustZYCT202SwitchMap"); }
        }
        else if (manufacturer == QLatin1String("innr"))
        {
            if      (modelid.startsWith(QLatin1String("RC 110"))) { m_buttonMap = buttonMapData.value("innrRC110Map"); }
        }
        else if (manufacturer == QLatin1String("icasa"))
        {
            if      (modelid.startsWith(QLatin1String("ICZB-KPD1"))) { m_buttonMap = buttonMapData.value("icasaKeypadMap"); }
            else if (modelid.startsWith(QLatin1String("ICZB-RM"))) { m_buttonMap = buttonMapData.value("icasaRemoteMap"); }
        }
        else if (manufacturer == QLatin1String("EcoDim"))
        {
            if      (modelid.startsWith(QLatin1String("ED-1001"))) { m_buttonMap = buttonMapData.value("sunricherMap"); }
        }
        else if (manufacturer == QLatin1String("Samjin"))
        {
            if (modelid == QLatin1String("button")) { m_buttonMap = buttonMapData.value("samjinButtonMap"); }
        }
        else if (manufacturer == QLatin1String("Legrand"))
        {
            if      (modelid == QLatin1String("Remote switch")) { m_buttonMap = buttonMapData.value("legrandSwitchRemote"); }
            else if (modelid == QLatin1String("Double gangs remote switch")) { m_buttonMap = buttonMapData.value("legrandDoubleSwitchRemote"); }
            else if (modelid == QLatin1String("Shutters central remote switch")) { m_buttonMap = buttonMapData.value("legrandShutterSwitchRemote"); }
            else if (modelid == QLatin1String("Remote toggle switch")) { m_buttonMap = buttonMapData.value("legrandToggleRemoteSwitch"); }
            else if (modelid == QLatin1String("Remote motion sensor")) { m_buttonMap = buttonMapData.value("legrandMotionSensor"); }
        }
        else if (manufacturer == QLatin1String("Sunricher"))
        {
            if      (modelid.startsWith(QLatin1String("ZGRC-KEY-012"))) { m_buttonMap = buttonMapData.value("icasaRemoteMap"); }
            else if (modelid.startsWith(QLatin1String("ZG2833K"))) { m_buttonMap = buttonMapData.value("sunricherMap"); }
            else if (modelid.startsWith(QLatin1String("ZG2835"))) { m_buttonMap = buttonMapData.value("sunricherMap"); }
            else if (modelid.startsWith(QLatin1String("ZGRC-KEY-013"))) { m_buttonMap = buttonMapData.value("icasaRemoteMap"); }
        }
        else if (manufacturer == QLatin1String("RGBgenie"))
        {
            if (modelid.startsWith(QLatin1String("RGBgenie ZB-5121"))) { m_buttonMap = buttonMapData.value("rgbgenie5121Map"); }
            else if (modelid.startsWith(QLatin1String("RGBgenie ZB-5001"))) { m_buttonMap = buttonMapData.value("icasaRemoteMap"); }
        }
        else if (manufacturer == QLatin1String("Bitron Home"))
        {
            if (modelid.startsWith(QLatin1String("902010/23"))) { m_buttonMap = buttonMapData.value("bitronRemoteMap"); }
        }
        else if (manufacturer == QLatin1String("Namron AS"))
        {
            if (modelid.startsWith(QLatin1String("45127"))) { m_buttonMap = buttonMapData.value("sunricherMap"); }
        }
        else if (manufacturer == QLatin1String("Heiman"))
        {
            if (modelid == QLatin1String("RC_V14")) { m_buttonMap = buttonMapData.value("rcv14Map"); }
            else if (modelid == QLatin1String("RC-EM")) { m_buttonMap = buttonMapData.value("rcv14Map"); }
        }
        else if (manufacturer == QLatin1String("MLI"))
        {
            if (modelid.startsWith(QLatin1String("ZBT-Remote-ALL-RGBW"))) { m_buttonMap = buttonMapData.value("tintMap"); }
        }
        else if (manufacturer == QLatin1String("Echostar"))
        {
            if (modelid == QLatin1String("Bell")) { m_buttonMap = buttonMapData.value("sageMap"); }
        }
        else if (manufacturer == QLatin1String("LDS"))
        {
            if (modelid == QLatin1String("ZBT-CCTSwitch-D0001")) { m_buttonMap = buttonMapData.value("LDSRemoteMap"); }
        }
        else if (manufacturer == QLatin1String("lk"))
        {
            if (modelid == QLatin1String("ZBT-DIMSwitch-D0001")) { m_buttonMap = buttonMapData.value("linkind1keyMap"); }
        }
        else if (manufacturer == QLatin1String("eWeLink"))
        {
            if (modelid == QLatin1String("WB01")) { m_buttonMap = buttonMapData.value("sonoffOnOffMap"); }
        }
        else if ((manufacturer == QLatin1String("_TZ3000_bi6lpsew")) ||  // can't use model id but manufacture name is device specific
                 (manufacturer == QLatin1String("_TYZB02_keyjqthh")))
        {
            m_buttonMap = buttonMapData.value("Tuya3gangMap");
        }
    }

    return m_buttonMap;
}
