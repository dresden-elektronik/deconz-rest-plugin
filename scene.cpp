/*
 * Copyright (c) 2016 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

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

/*! Transfers lights of the scene into JSONString.
    \param lights vector<LightState>
 */
QString Scene::lightsToString(const std::vector<LightState> &lights)
{
    QString jsonString = "[";
    std::vector<LightState>::const_iterator i = lights.begin();
    std::vector<LightState>::const_iterator i_end = lights.end();

    for (; i != i_end; ++i)
    {
        QString on = (i->on()) ? "true" : "false";
        QString cl = (i->colorloopActive()) ? "true" : "false";

        jsonString.append("{\"lid\":\"" + i->lid() + "\",");
        jsonString.append("\"on\":" + on + ",");
        jsonString.append("\"bri\":" + QString::number(i->bri()) + ",");
        jsonString.append("\"x\":" + QString::number(i->x()) + ",");
        jsonString.append("\"y\":\"" + QString::number(i->y()) + "\",");
        jsonString.append("\"tt\":\"" + QString::number(i->transitionTime()) + "\",");
        jsonString.append("\"cl\":\"" + cl + "\",");
        jsonString.append("\"clTime\":\"" + QString::number(i->colorloopTime()) + "\"},");
    }
    if (lights.size() > 0)
    {
        jsonString.chop(1);
    }
    jsonString.append("]");

    return jsonString;
}

std::vector<LightState> Scene::jsonToLights(const QString &json)
{
    bool ok;
    QVariantList var = (Json::parse(json, ok)).toList();
    QVariantMap map;
    LightState state;
    std::vector<LightState> lights;

    QVariantList::const_iterator i = var.begin();
    QVariantList::const_iterator i_end = var.end();

    for (; i != i_end; ++i)
    {
        map = i->toMap();
        state.setLightId(map["lid"].toString());
        state.setOn(map["on"].toBool());
        state.setBri(map["bri"].toUInt());
        state.setX(map["x"].toUInt());
        state.setY(map["y"].toUInt());
        state.setTransitionTime(map["tt"].toUInt());
        state.setColorloopActive(map["cl"].toBool());
        state.setColorloopTime(map["clTime"].toUInt());

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
    m_bri(0),
    m_x(0),
    m_y(0),
    m_enhancedHue(0),
    m_saturation(0),
    m_colorloopActive(false),
    m_colorloopDirection(0),
    m_colorloopTime(0),
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

/*! Returns the enhancedHue value of the light of the scene.
 */
const uint16_t &LightState::enHue() const
{
    return m_enhancedHue;
}

/*! Sets the enhancedHue value of the light of the scene.
    \param enHue the enhancedHue value of the light
 */
void LightState::setEnHue(const uint16_t &enHue)
{
    m_enhancedHue = enHue;
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
