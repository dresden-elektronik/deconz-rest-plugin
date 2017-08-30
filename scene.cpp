/*
 * Copyright (c) 2016 dresden elektronik ingenieurtechnik gmbh.
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
Scene::Scene() :
    state(StateNormal),
    externalMaster(false),
    groupAddress(0),
    id(0),
    m_transitiontime(0)
{
}

/*! Returns the transitiontime of the scene.
 */
const uint16_t &Scene::transitiontime() const
{
    return m_transitiontime;
}

/*! Sets the transitiontime of the scene.
    \param transitiontime the transitiontime of the scene
 */
void Scene::setTransitiontime(const uint16_t &transitiontime)
{
    m_transitiontime = transitiontime;
}

/*! Returns the lights of the scene.
 */
std::vector<LightState> &Scene::lights()
{
    return m_lights;
}

/*! Returns the lights of the scene.
 */
const std::vector<LightState> &Scene::lights() const
{
    return m_lights;
}

/*! Sets the lights of the scene.
    \param lights the lights of the scene
 */
void Scene::setLights(const std::vector<LightState> &lights)
{
    m_lights = lights;
}

/*! Adds a light to the lights of the scene.
    \param light the light that should be added
 */
void Scene::addLightState(const LightState &light)
{
    m_lights.push_back(light);
}

/*! removes a light from the lights of the scene if present.
    \param lid the lightId that should be removed
    \return true if light was found and removed
 */
bool Scene::deleteLight(const QString &lid)
{
    std::vector<LightState>::const_iterator l = m_lights.begin();
    std::vector<LightState>::const_iterator lend = m_lights.end();
    int position = 0;
    for (; l != lend; ++l)
    {
        if (l->lid() == lid)
        {
            m_lights.erase(m_lights.begin() + position);
            // delete scene if it contains no lights
            if (m_lights.size() == 0)
            {
                state = Scene::StateDeleted;
            }
            return true;
        }
        position++;
    }
    return false;
}

/*! Returns light satte for given light id of the scene if present.
    \param lid the lightId
    \return the light state or 0 if not found
 */
LightState *Scene::getLightState(const QString &lid)
{
    std::vector<LightState>::iterator i = m_lights.begin();
    std::vector<LightState>::iterator end = m_lights.end();

    for (; i != end; ++i)
    {
        if (i->lid() == lid)
        {
           return &*i;
        }
    }
    return 0;
}

/*! Transfers lights of the scene into JSONString.
    \param lights vector<LightState>
 */
QString Scene::lightsToString(const std::vector<LightState> &lights)
{
    std::vector<LightState>::const_iterator i = lights.begin();
    std::vector<LightState>::const_iterator i_end = lights.end();
    QVariantList ls;

    for (; i != i_end; ++i)
    {
        QVariantMap map;
        map[QLatin1String("lid")] = i->lid();
        map[QLatin1String("on")] = i->on();
        map[QLatin1String("bri")] = (double)i->bri();
        map[QLatin1String("tt")] = (double)i->transitionTime();
        map[QLatin1String("cm")] = i->colorMode();

        if (i->colorMode() != QLatin1String("none"))
        {
            map[QLatin1String("x")] = (double)i->x();
            map[QLatin1String("y")] = (double)i->y();

            if (i->colorMode() == QLatin1String("hs"))
            {
                map[QLatin1String("ehue")] = (double)i->enhancedHue();
                map[QLatin1String("sat")] = (double)i->saturation();
            }
            else if (i->colorMode() == QLatin1String("ct"))
            {
                map[QLatin1String("ct")] = (double)i->colorTemperature();
            }

            map[QLatin1String("cl")] = i->colorloopActive();
            map[QLatin1String("clTime")] = (double)i->colorloopTime();
        }

        ls.append(map);
    }

    return Json::serialize(ls);
}

std::vector<LightState> Scene::jsonToLights(const QString &json)
{
    bool ok;
    QVariantList var = Json::parse(json, ok).toList();
    QVariantMap map;
    std::vector<LightState> lights;

    QVariantList::const_iterator i = var.begin();
    QVariantList::const_iterator i_end = var.end();

    if (!ok)
    {
        return lights;
    }

    for (; i != i_end; ++i)
    {
        LightState state;
        map = i->toMap();
        state.setLightId(map[QLatin1String("lid")].toString());
        state.setOn(map[QLatin1String("on")].toBool());
        state.setBri(map[QLatin1String("bri")].toUInt());
        state.setTransitionTime(map[QLatin1String("tt")].toUInt());

        if (map.contains(QLatin1String("x")) && map.contains(QLatin1String("y")))
        {
            state.setX(map[QLatin1String("x")].toUInt());
            state.setY(map[QLatin1String("y")].toUInt());

            if (!map.contains(QLatin1String("cm")))
            {
                state.setColorMode(QLatin1String("xy")); // backward compatibility
            }
        }

        if (map.contains(QLatin1String("cl")) && map.contains(QLatin1String("clTime")))
        {
            state.setColorloopActive(map[QLatin1String("cl")].toBool());
            state.setColorloopTime(map[QLatin1String("clTime")].toUInt());
        }

        if (map.contains(QLatin1String("cm")))
        {
            QString colorMode = map[QLatin1String("cm")].toString();
            if (!colorMode.isEmpty())
            {
                state.setColorMode(colorMode);
            }
        }

        if (state.colorMode() == QLatin1String("ct") && map.contains(QLatin1String("ct")))
        {
            quint16 ct = map[QLatin1String("ct")].toUInt(&ok);
            if (ok)
            {
                state.setColorTemperature(ct);
            }
        }
        else if (state.colorMode() == QLatin1String("hs") && map.contains(QLatin1String("ehue")) && map.contains(QLatin1String("sat")))
        {
            quint16 ehue = map[QLatin1String("ehue")].toUInt(&ok);
            if (ok)
            {
                quint16 sat = map[QLatin1String("sat")].toUInt(&ok);
                if (ok)
                {
                    state.setEnhancedHue(ehue);
                    state.setSaturation(sat);
                }
            }
        }

        lights.push_back(state);
    }

    return lights;
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
