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
const std::vector<LightState> &Scene::lights() const
{
    return m_lights;
}

/*! Adds a light to the lightstates of the scene.
    \param light the light that should be added
 */
void Scene::addLight(const LightState& light)
{
    m_lights.push_back(light);
}

/*! Removes a light from the lightstates of the scene if present.
    \param lid the light id that should be removed
    \return true if light was found and removed
 */
bool Scene::removeLight(const QString& lid)
{
    int position = 0;
    for (LightState& l : m_lights)
    {
        if (l.lid() == lid)
        {
            m_lights.erase(m_lights.begin() + position);
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
LightState* Scene::getLight(const QString& lid)
{
    for (LightState& l : m_lights)
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
    std::vector<LightState>::const_iterator i = m_lights.begin();
    std::vector<LightState>::const_iterator i_end = m_lights.end();
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
