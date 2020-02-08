/*
 * Copyright (c) 2016-2019 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */
#include <QStringBuilder>
#include "scene.h"


/*! Constructor.
 */
Scene::Scene(const uint16_t gid, const uint8_t sid, const Type type) :
    m_state(StateNormal),
    m_type(type),
    m_externalMaster(false),
    m_gid(gid),
    m_sid(sid),
    m_transitiontime(0),
    m_owner(QLatin1String("")),
    m_recycle(false),
    m_locked(false),
    m_appdata(QVariantMap()),
    m_picture(QLatin1String("")),
    m_version(2)
{
    m_id.sprintf("0x%04X%02X", gid, sid);
    m_name = "Scene " + m_id;
}

/*! Initializer.
 */
void Scene::init(const QString& id, const QString& owner, const QDateTime& lastupdated, const uint16_t version)
{
    m_id = id;
    m_owner = owner;
    m_lastupdated = lastupdated;
    m_version = version;
    m_name = "Scene " + m_id;
}

/*! Returns the state of the scene.
 */
const Scene::State& Scene::state() const
{
    return m_state;
}

/*! Sets the state of the scene.
    \param state the state of the scene
 */
void Scene::state(const State& state)
{
    m_state = state;
}


/*! Returns the external master state of the scene.
 */
bool Scene::externalMaster() const
{
    return m_externalMaster;
}

/*! Sets the external master state of the scene.
    \param bool the external master state of the scene
 */
void Scene::externalMaster(const bool externalMaster)
{
    m_externalMaster = externalMaster;
}

/*! Returns the id of the scene.
 */
const QString& Scene::id() const
{
    return m_id;
}

/*! Returns the group id of the scene.
 */
uint16_t Scene::gid() const
{
    return m_gid;
}

/*! Returns the scene id of the scene.
 */
uint8_t Scene::sid() const
{
    return m_sid;
}

/*! Returns the name of the scene.
 */
const QString& Scene::name() const
{
    return m_name;
}

/*! Sets the name of the scene.
    \param name the name of the scene
 */
void Scene::name(const QString& name)
{
    m_name = name;
}

/*! Returns the lightstates of the scene.
 */
const std::vector<LightState> &Scene::lightStates() const
{
    return m_lightstates;
}

/*! Adds a light to the lightstates of the scene.
    \param light the light that should be added
 */
void Scene::addLightState(const LightState& state)
{
    m_lightstates.push_back(state);
}

/*! Removes a light from the lightstates of the scene if present.
    \param lid the light id that should be removed
    \return true if light was found and removed
 */
bool Scene::removeLightState(const QString& lid)
{
    int position = 0;
    for (LightState& l : m_lightstates)
    {
        if (l.lid() == lid)
        {
            m_lightstates.erase(m_lightstates.begin() + position);
            return true;
        }
        position++;
    }
    return false;
}

/*! Get the light for the given light id of the scene if present.
    \param lid the light id
    \return the light state or 0 if not found
 */
LightState* Scene::getLightState(const QString& lid)
{
    for (LightState& l : m_lightstates)
    {
        if (l.lid() == lid)
        {
            return &l;
        }
    }
    return 0;
}

/*! Returns the transitiontime of the scene.
 */
uint16_t Scene::transitiontime() const
{
    return m_transitiontime;
}

/*! Sets the transitiontime of the scene.
    \param transitiontime the transitiontime of the scene
 */
void Scene::transitiontime(const uint16_t transitiontime)
{
    m_transitiontime = transitiontime;
}

/*! Returns the owner id of the scene.
 */
const QString& Scene::owner() const
{
    return m_owner;
}

/*! Returns the recycle state of the scene.
 */
bool Scene::recycle() const
{
    return m_recycle;
}

/*! Sets the recycle state of the scene.
    \param bool the recycle state of the scene
 */
void Scene::recycle(const bool recycle)
{
    m_recycle = recycle;
}

/*! Returns the locked state of the scene.
 */
bool Scene::locked() const
{
    return m_locked;
}

/*! Sets the locked state of the scene.
    \param bool the locked state of the scene
 */
void Scene::locked(const bool locked)
{
    m_locked = locked;
}

/*! Returns the appdata of the scene.
 */
const QVariantMap& Scene::appdata() const
{
    return m_appdata;
}

/*! Sets the appdata of the scene.
    \param appdata the appdata of the scene
 */
void Scene::appdata(const QVariantMap& appdata)
{
    m_appdata = appdata;
}

/*! Returns the picture id of the scene.
 */
const QString& Scene::picture() const
{
    return m_picture;
}

/*! Sets the picture id of the scene.
    \param picture the picture id of the scene
 */
void Scene::picture(const QString& picture)
{
    m_picture = picture;
}

/*! Returns the lastupdated date of the scene.
 */
const QDateTime& Scene::lastupdated() const
{
    return m_lastupdated;
}

/*! Update the lastupdated state.
 */
void Scene::lastupdated(const bool lastupdated)
{
    if (lastupdated)
    {
        m_lastupdated = QDateTime::currentDateTimeUtc();
    }
}

/*! Returns the version of the scene.
 */
uint16_t Scene::version() const
{
    return m_version;
}

/*! Put all parameters in a map for later json serialization.
    \return map
 */
QVariantMap Scene::map() const
{
    QVariantMap map;

    map[QLatin1String("name")] = m_name;
    if (m_type == LightScene) {
        map[QLatin1String("type")] = QLatin1String("LightScene");
    }
    else {
        map[QLatin1String("type")] = QLatin1String("GroupScene");
        map[QLatin1String("group")] = QString::number(m_gid);
    }

    QVariantList lights;
    std::vector<LightState>::const_iterator i = m_lightstates.begin();
    std::vector<LightState>::const_iterator i_end = m_lightstates.end();
    for (; i != i_end; ++i)
    {
        lights.append(i->lid());
    }
    map[QLatin1String("lights")] = lights;

    map[QLatin1String("appdata")] = m_appdata;
    map[QLatin1String("picture")] = m_picture;
    map[QLatin1String("owner")] = m_owner;
    map[QLatin1String("locked")] = m_locked;
    map[QLatin1String("recycle")] = m_recycle;
    map[QLatin1String("lastupdated")] = m_lastupdated.toString(QLatin1String("yyyy-MM-ddTHH:mm:ss"));
    map[QLatin1String("version")] = m_version;
    return map;
}


// LightState

/*! Constructor.
 */
LightState::LightState() :
    m_lid(""),
    m_on(false),
    m_needRead(false),
    m_bri(0),
    m_x(0),
    m_y(0),
    m_enhancedHue(0),
    m_saturation(0),
    m_colorloopActive(false),
    m_colorloopDirection(0),
    m_colorloopTime(0),
    m_colorMode(QLatin1String("none")),
    m_transitiontime(0)
{
}

/*! Returns the id of the light of the scene.
 */
const QString &LightState::lid() const
{
    return m_lid;
}

/*! Sets the id of the light of the scene.
    \param state the rule state
 */
void LightState::setLightId(const QString &lid)
{
    m_lid = lid;
}

/*! Returns the on status of the light of the scene.
 */
bool LightState::on() const
{
    return m_on;
}

/*! Sets the on status of the light of the scene.
    \param on the on status of the light
 */
void LightState::setOn(const bool &on)
{
    m_on = on;
}

/*! Returns the brightness of the light of the scene.
 */
const uint8_t &LightState::bri() const
{
    return m_bri;
}

/*! Sets the brightness of the light of the scene.
    \param bri the brightness of the light
 */
void LightState::setBri(const uint8_t &bri)
{
    m_bri = bri;
}

/*! Returns the colorX value of the light of the scene.
 */
const uint16_t &LightState::x() const
{
    return m_x;
}

/*! Sets the colorX value of the light of the scene.
    \param x the colorX value of the light
 */
void LightState::setX(const uint16_t &x)
{
    m_x = x;
}

/*! Returns the colorY value of the light of the scene.
 */
const uint16_t &LightState::y() const
{
    return m_y;
}

/*! Sets the colorY value of the light of the scene.
    \param y the colorY value of the light
 */
void LightState::setY(const uint16_t &y)
{
    m_y = y;
}

/*! Returns the color temperature value of the light in the scene.
 */
uint16_t LightState::colorTemperature() const
{
    return m_colorTemperature;
}

/*! Sets the color temperature value of the light in the scene.
    \param colorTemperature the color temperature value of the light
 */
void LightState::setColorTemperature(uint16_t colorTemperature)
{
    m_colorTemperature = colorTemperature;
}

/*! Returns the enhancedHue value of the light of the scene.
 */
const uint16_t &LightState::enhancedHue() const
{
    return m_enhancedHue;
}

/*! Sets the enhancedHue value of the light of the scene.
    \param enhancedHue the enhancedHue value of the light
 */
void LightState::setEnhancedHue(const uint16_t &enhancedHue)
{
    m_enhancedHue = enhancedHue;
}

/*! Returns the saturation of the light of the scene.
 */
const uint8_t &LightState::saturation() const
{
    return m_saturation;
}

/*! Sets the saturation of the light of the scene.
    \param sat the saturation of the light
 */
void LightState::setSaturation(const uint8_t &sat)
{
    m_saturation = sat;
}

/*! Returns the colorloopActive status of the light of the scene.
 */
const bool &LightState::colorloopActive() const
{
    return m_colorloopActive;
}

/*! Sets the colorloopActive status of the light of the scene.
    \param active the colorloopActive status of the light
 */
void LightState::setColorloopActive(const bool &active)
{
    m_colorloopActive = active;
}

/*! Returns the colorloopDirection of the light of the scene.
 */
const uint8_t &LightState::colorloopDirection() const
{
    return m_colorloopDirection;
}

/*! Sets the colorloopDirection of the light of the scene.
    \param direction the colorloopDirection of the light
 */
void LightState::setColorloopDirection(const uint8_t &direction)
{
    m_colorloopDirection = direction;
}

/*! Returns the colorloopTime of the light of the scene.
 */
const uint8_t &LightState::colorloopTime() const
{
    return m_colorloopTime;
}

/*! Sets the colorloopTime of the light of the scene.
    \param time the colorloopTime of the light
 */
void LightState::setColorloopTime(const uint8_t &time)
{
    m_colorloopTime = time;
}

/*! Returns the color mode of the light in the scene.
 */
const QString &LightState::colorMode() const
{
    return m_colorMode;
}

/*! Sets the color mode of the light in the scene.
    \param colorMode the color mode of the light
 */
void LightState::setColorMode(const QString &colorMode)
{
    if (m_colorMode != colorMode)
    {
        m_colorMode = colorMode;
    }
}

/*! Returns the transitiontime of the scene.
 */
const uint16_t &LightState::transitionTime() const
{
    return m_transitiontime;
}

/*! Sets the transitiontime of the scene.
    \param transitiontime the transitiontime of the scene
 */
void LightState::setTransitionTime(uint16_t transitiontime)
{
    m_transitiontime = transitiontime;
}

/*! Sets need read flag.
    \param needRead - true if attribute should be queried by view scene command
 */
void LightState::setNeedRead(bool needRead)
{
    m_needRead = needRead;
}

/*! Put all parameters in a map for later json serialization.
    \return map
 */
QVariantMap LightState::map() const
{
    QVariantMap map;
    map[QLatin1String("lid")] = lid(); // deCONZ
    map[QLatin1String("on")] = on();
    map[QLatin1String("bri")] = bri();
    map[QLatin1String("cm")] = colorMode(); // deCONZ
    if (colorMode() != QLatin1String("none"))
    {
        if (colorMode() == QLatin1String("hs"))
        {
            map[QLatin1String("hue")] = enhancedHue(); // Hue
            map[QLatin1String("ehue")] = enhancedHue(); // deCONZ
            map[QLatin1String("sat")] = saturation();
        }
        QVariantList xy;
        double dx = x() / 65535.0;
        double dy = y() / 65535.0;
        if      (dx < 0) { dx = 0; }
        else if (dx > 1) { dx = 1; }
        if      (dy < 0) { dy = 0; }
        else if (dy > 1) { dy = 1; }
        xy.append(dx);
        xy.append(dy);
        map[QLatin1String("xy")] = xy; // Hue
        map[QLatin1String("x")] = x(); // deCONZ
        map[QLatin1String("y")] = y(); // deCONZ
        map[QLatin1String("ct")] = colorTemperature();
        map[QLatin1String("effect")] = colorloopActive() ? "colorloop" : "none"; // Hue
        map[QLatin1String("cl")] = colorloopActive(); // deCONZ
        map[QLatin1String("clTime")] = colorloopTime(); // deCONZ
    }
    map[QLatin1String("transitiontime")] = transitionTime();

    return map;
}

/*! Sets the transitiontime of the scene.
    \param transitiontime the transitiontime of the scene
 */
void LightState::map(QVariantMap& map)
{
    setLightId(map[QLatin1String("lid")].toString());
    setOn(map[QLatin1String("on")].toBool());
    setBri(map[QLatin1String("bri")].toUInt());
    setTransitionTime(map[QLatin1String("tt")].toUInt());

    if (map.contains(QLatin1String("x")) && map.contains(QLatin1String("y")))
    {
        setX(map[QLatin1String("x")].toUInt());
        setY(map[QLatin1String("y")].toUInt());

        if (!map.contains(QLatin1String("cm")))
        {
            setColorMode(QLatin1String("xy")); // backward compatibility
        }
    }

    if (map.contains(QLatin1String("cl")) && map.contains(QLatin1String("clTime")))
    {
        setColorloopActive(map[QLatin1String("cl")].toBool());
        setColorloopTime(map[QLatin1String("clTime")].toUInt());
    }

    if (map.contains(QLatin1String("cm")))
    {
        QString colorMode = map[QLatin1String("cm")].toString();
        if (!colorMode.isEmpty())
        {
            setColorMode(colorMode);
        }
    }

    bool ok;
    if (colorMode() == QLatin1String("ct") && map.contains(QLatin1String("ct")))
    {
        quint16 ct = map[QLatin1String("ct")].toUInt(&ok);
        if (ok)
        {
            setColorTemperature(ct);
        }
    }
    else if (colorMode() == QLatin1String("hs") && map.contains(QLatin1String("ehue")) && map.contains(QLatin1String("sat")))
    {
        quint16 ehue = map[QLatin1String("ehue")].toUInt(&ok);
        if (ok)
        {
            quint16 sat = map[QLatin1String("sat")].toUInt(&ok);
            if (ok)
            {
                setEnhancedHue(ehue);
                setSaturation(sat);
            }
        }
    }
}
