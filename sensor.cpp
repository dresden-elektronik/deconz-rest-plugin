/*
 * Copyright (c) 2016 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "sensor.h"

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

/*! Constructor. */
Sensor::Sensor() :
    m_deletedstate(Sensor::StateNormal),
    m_name(""),
    m_type("undefined"),
    m_modelid(""),
    m_manufacturer("unknown"),
    m_swversion("1.0")
{
    sensorTypes.append("CLIPSwitch");
    sensorTypes.append("CLIPOpenClose");
    sensorTypes.append("CLIPPresence");
    sensorTypes.append("CLIPTemperature");
    sensorTypes.append("CLIPGenericFlag");
    sensorTypes.append("CLIPGenericStatus");
    sensorTypes.append("CLIPHumidity");
    sensorTypes.append("Daylight");
    sensorTypes.append("ZGPSwitch");
    sensorTypes.append("ZHASwitch");
    sensorTypes.append("ZHALight");
    sensorTypes.append("ZHAPresence");
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

/*! Returns the sensor name.
 */
const QString &Sensor::name() const
{
   return m_name;
}

/*! Sets the sensor name.
    \param name the sensor name
 */
void Sensor::setName(const QString &name)
{
    m_name = name;
}

/*! Returns the sensor type.
 */
const QString &Sensor::type() const
{
    return m_type;
}

/*! Sets the sensor type.
    \param type the sensor type
 */
void Sensor::setType(const QString &type)
{
    m_type = type;
}

/*! Returns the sensor modelId.
 */
const QString &Sensor::modelId() const
{
    return m_modelid;
}

/*! Sets the sensor modelId.
    \param mid the sensor modelId
 */
void Sensor::setModelId(const QString &mid)
{
    m_modelid = mid.trimmed();
}

/*! Returns the sensor manufacturer.
 */
const QString &Sensor::manufacturer() const
{
    return m_manufacturer;
}

/*! Sets the sensor manufacturer.
    \param manufacturer the sensor manufacturer
 */
void Sensor::setManufacturer(const QString &manufacturer)
{
    m_manufacturer = manufacturer;
}

/*! Returns the sensor software version.
    Not supported for ZGP Sensortype
 */
const QString &Sensor::swVersion() const
{
        return m_swversion;
}

/*! Sets the sensor software version.
    \param swVersion the sensor software version
 */
void Sensor::setSwVersion(const QString &swversion)
{
    m_swversion = swversion;
}

/*! Returns the sensor state.
 */
SensorState &Sensor::state()
{
    return m_state;
}

/*! Returns the sensor state.
 */
const SensorState &Sensor::state() const
{
    return m_state;
}

/*! Sets the sensor state.
    \param state the sensor state
 */
void Sensor::setState(const SensorState &state)
{
    m_state = state;
}

/*! Returns the sensor config.
 */
const SensorConfig &Sensor::config() const
{
    return m_config;
}

/*! Sets the sensor config.
    \param config the sensor config
 */
void Sensor::setConfig(const SensorConfig &config)
{
    m_config = config;
}

/*! Transfers state into JSONString.
    \param state
 */
QString Sensor::stateToString(const SensorState &state)
{
    QString jsonString = QString("{\"lastupdated\":\"%1\",\"flag\":\"%2\",\"status\":\"%3\",\"presence\":\"%4\",\"open\":\"%5\",\"buttonevent\":\"%6\",\"temperature\":\"%7\",\"humidity\":\"%8\",\"daylight\":\"%9\"}")
            .arg(state.lastupdated())
            .arg(state.flag())
            .arg(state.status())
            .arg(state.presence())
            .arg(state.open())
            .arg(state.buttonevent())
            .arg(state.temperature())
            .arg(state.humidity())
            .arg(state.daylight());

    return jsonString;
}

/*! Transfers config into JSONString.
    \param config
 */
QString Sensor::configToString(const SensorConfig &config)
{
    QString jsonString = QString("{\"on\": %1,\"reachable\": %2,\"battery\":\"%3\",\"url\":\"%4\",\"long\":\"%5\",\"lat\":\"%6\",\"sunriseoffset\":\"%7\",\"sunsetoffset\":\"%8\"}")
            .arg(config.on())
            .arg(config.reachable())
            .arg(config.battery())
            .arg(config.url())
            .arg(config.longitude())
            .arg(config.lat())
            .arg(config.sunriseoffset())
            .arg(config.sunsetoffset());

    return jsonString;
}

/*! Parse the sensor state from a JSON string. */
SensorState Sensor::jsonToState(const QString &json)
{
    bool ok;
    QVariant var = (Json::parse(json, ok));
    QVariantMap map = var.toMap();
    SensorState state;

    state.setButtonevent(map["buttonevent"].toDouble());
    state.setDaylight(map["daylight"].toString());
    state.setFlag(map["flag"].toString());
    state.setHumidity(map["humidity"].toString());
    state.setLastupdated(map["lastupdated"].toString());
    state.setOpen(map["open"].toString());
    state.setPresence(map["presence"].toString());
    state.setStatus(map["status"].toString());
    state.setTemperature(map["temperature"].toString());

    return state;
}

/*! Parse the sensor config from a JSON string. */
SensorConfig Sensor::jsonToConfig(const QString &json)
{
    bool ok;
    QVariant var = (Json::parse(json, ok));
    QVariantMap map = var.toMap();
    SensorConfig config;

    config.setOn(map["on"].toBool());
    config.setReachable(map["reachable"].toBool());
    config.setBattery(map["battery"].toString());
    config.setUrl(map["url"].toString());
    config.setLongitude(map["long"].toString());
    config.setLat(map["lat"].toString());
    config.setSunriseoffset(map["sunriseoffset"].toString());
    config.setSunsetoffset(map["sunsetoffset"].toString());

    return config;
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

// Sensor state
/*! Constructor. */
SensorState::SensorState() :
//    m_lastupdated(""),
    m_flag(""),
    m_status(""),
    m_presence(""),
    m_open(""),
    m_buttonevent(-1),
    m_temperature(""),
    m_humidity(""),
    m_daylight(""),
    m_lux(0)
{
    updateTime();
}

/*! Returns the sensor state lastupdated attribute.
    Sensorstypes: all
 */
const QString &SensorState::lastupdated() const
{
    return m_lastupdated;
}

/*! Sets the sensor state lastupdated attribute.
    Sensorstypes: all
    \param lastupdated the sensor state lastupdated
 */
void SensorState::setLastupdated(const QString &lastupdated)
{
    m_lastupdated = lastupdated;
}

/*! Returns the sensor state flag attribute.
    Sensorstypes: CLIPGenericFlag
 */
const QString &SensorState::flag() const
{
        return m_flag;
}

/*! Sets the sensor state flag attribute.
    Sensorstypes: CLIPGenericFlag
    \param flag the sensor state flag
 */
void SensorState::setFlag(const QString &flag)
{
    m_flag = flag;
}

/*! Returns the sensor state status attribute.
    Sensortypes: CLIPGenericStatus
 */
const QString &SensorState::status() const
{
    return m_status;
}

/*! Sets the sensor state status attribute.
    Sensortypes: CLIPGenericStatus
    \param status the sensor config status
 */
void SensorState::setStatus(const QString &status)
{
    m_status = status;
}

/*! Returns the sensor state presence attribute.
 */
const QString &SensorState::presence() const
{
    return m_presence;
}

/*! Sets the sensor state presence attribute.
    Sensortypes: CLIPPresence
    \param presence the sensor state presence
 */
void SensorState::setPresence(const QString &presence)
{
    m_presence = presence;
}

/*! Returns the sensor state open attribute.
    Sensortypes: CLIPOpenClose
 */
const QString &SensorState::open() const
{
    return m_open;
}

/*! Sets the sensor state open attribute.
    Sensortypes: CLIPOpenClose
    \param open the sensor state open
 */
void SensorState::setOpen(const QString &open)
{
    m_open = open;
}

/*! Returns the sensor state buttonevent attribute.
    Sensortypes: ZGPSwitch
 */
int SensorState::buttonevent() const
{
    return m_buttonevent;
}

/*! Sets the sensor state buttonevent attribute.
    Sensortypes: ZGPSwitch
    \param buttonevent the sensor state buttonevent
 */
void SensorState::setButtonevent(int buttonevent)
{
    m_buttonevent = buttonevent;
}

/*! Returns the sensor state temperature attribute.
    Sensortypes: CLIPTemperature
 */
const QString &SensorState::temperature() const
{
    return m_temperature;
}

/*! Sets the sensor state temperature attribute.
    Sensortypes: CLIPTemperature
    \param temperature the sensor state temperature
 */
void SensorState::setTemperature(const QString &temperature)
{
    m_temperature = temperature;
}

/*! Returns the sensor state humidity attribute.
    Sensortypes: CLIPHumidity
 */

const QString &SensorState::humidity() const
{
    return m_humidity;
}

/*! Sets the sensor state daylight attribute.
    Sensortypes: Daylight
    \param daylight the sensor state daylight
 */
void SensorState::setHumidity(const QString &humidity)
{
    m_humidity = humidity;
}

/*! Returns the sensor state daylight attribute.
    Sensortypes: CLIPHumidity
 */
const QString &SensorState::daylight() const
{
    return m_daylight;
}

/*! Sets the sensor state humidity attribute.
    Sensortypes: Daylight
    \param humidity the sensor state humidity
 */
void SensorState::setDaylight(const QString &daylight)
{
    m_daylight = daylight;
}

/*! Returns the sensor state lux attribute.
    Sensortypes: ZHALight
 */
quint16 SensorState::lux() const
{
    return m_lux;
}

/*! Sets the sensor state lux attribute.
    Sensortypes: ZHALight
    \param lux the sensor state measured lux value
 */
void SensorState::setLux(quint16 lux)
{
    m_lux = lux;
}

/*! Refreshs the last updated time.
 */
void SensorState::updateTime()
{
    QDateTime datetime = QDateTime::currentDateTimeUtc();
    m_lastupdated = datetime.toString("yyyy-MM-ddTHH:mm:ss"); // ISO 8601
}

// Sensor Config
/*! Constructor. */
SensorConfig::SensorConfig() :
    m_on(true),
    m_reachable(false),
    m_duration(-1),
    m_battery(""),
    m_url(""),
    m_long(""),
    m_lat(""),
    m_sunriseoffset(""),
    m_sunsetoffset("")
{
}

/*! Returns the sensor config on attribute.
    Sensortypes: all
 */
bool SensorConfig::on() const
{
    return m_on;
}

/*! Sets the sensor config on attribute.
    Sensortypes: all
    \param on the sensor config on
 */
void SensorConfig::setOn(bool on)
{
    m_on = on;
}

/*! Returns the sensor config reachable attribute.
    Sensortypes: all CLIP, Generic, General sensors
 */
bool SensorConfig::reachable() const
{
    return m_reachable;
}

/*! Sets the sensor config reachable attribute.
    Sensortypes: all CLIP, Generic, General sensors
    \param reachable the sensor config reachable
 */
void SensorConfig::setReachable(bool reachable)
{
    m_reachable = reachable;
}

/*! Returns the sensor config duration attribute.
    Sensortypes: ZHAPresence
 */
double SensorConfig::duration() const
{
    return m_duration;
}

/*! Sets the sensor config duration attribute.
    Sensortypes: all ZHAPresence
    \param duration the sensor config duration attribute
 */
void SensorConfig::setDuration(double duration)
{
    DBG_Assert(duration >= 0 && duration < 65535);
    if (duration >= 0 && duration <= 65535)
    {
        m_duration = duration;
    }
}

/*! Returns the sensor config battery attribute.
    Sensortypes: all CLIP, Generic, General sensors
 */
const QString &SensorConfig::battery() const
{
    return m_battery;
}

/*! Sets the sensor config battery attribute.
    Sensortypes: all CLIP, Generic, General sensors
    \param battery the sensor config battery
 */
void SensorConfig::setBattery(const QString &battery)
{
    m_battery = battery;
}

/*! Returns the sensor config url attribute.
    Sensortypes: all CLIP, Generic, General sensors
 */
const QString &SensorConfig::url() const
{
    return m_url;
}

/*! Sets the sensor config url attribute.
    Sensortypes: all CLIP, Generic, General sensors
    \param url the sensor config url
 */
void SensorConfig::setUrl(const QString &url)
{
    m_url = url;
}

/*! Returns the sensor config longitude attribute.
    Sensortypes: Daylight
 */
const QString &SensorConfig::longitude() const
{
    return m_long;
}

/*! Sets the sensor config longitude attribute.
    Sensortypes: Daylight
    \param longitude the sensor config longitude
 */
void SensorConfig::setLongitude(const QString &longitude)
{
    m_long = longitude;
}

/*! Returns the sensor config lat attribute.
    Sensortypes: Daylight
 */
const QString &SensorConfig::lat() const
{
    return m_lat;
}

/*! Sets the sensor config lat attribute.
    Sensortypes: Daylight
    \param lat the sensor config lat
 */
void SensorConfig::setLat(const QString &lat)
{
    m_lat = lat;
}

/*! Returns the sensor config sunriseoffset attribute.
    Sensortypes: Daylight
 */
const QString &SensorConfig::sunriseoffset() const
{
    return m_sunriseoffset;
}

/*! Sets the sensor config url attribute.
    Sensortypes: Daylight
    \param url the sensor config url
 */
void SensorConfig::setSunriseoffset(const QString &sunriseoffset)
{
    m_sunriseoffset = sunriseoffset;
}

/*! Returns the sensor config sunsetoffset attribute.
    Sensortypes: Daylight
 */
const QString &SensorConfig::sunsetoffset() const
{
    return m_sunsetoffset;
}

/*! Sets the sensor config sunsetoffset attribute.
    Sensortypes: Daylight
    \param sunsetoffset the sensor config sunsetoffset
 */
void SensorConfig::setSunsetoffset(const QString &sunsetoffset)
{
    m_sunsetoffset = sunsetoffset;
}
