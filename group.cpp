/*
 * Copyright (c) 2016 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "event_emitter.h"
#include "group.h"
#include <QStringList>

/*! Constructor.
 */
Group::Group() :
    Resource(RGroups),
    m_state(StateNormal),
    m_addr(0),
    m_id("0"),
    m_on(false),
    m_colorLoopActive(false)
{
   sendTime = QTime::currentTime();
   hidden = false;
   hueReal = 0;
   hue = 0;
   sat = 127;
   level = 127;
   colorX = 0;
   colorY = 0;
   colorTemperature = 0;
   colormode = QLatin1String("hs");
   alert = QLatin1String("none");

    // add common items
    addItem(DataTypeString, RAttrName);
    addItem(DataTypeBool, RStateAllOn);
    addItem(DataTypeBool, RStateAnyOn);
    addItem(DataTypeString, RActionScene);
    ResourceItem * rtype = addItem(DataTypeString, RAttrType);
    rtype->setValue(QString(QLatin1String("LightGroup")));
    ResourceItem * rclass = addItem(DataTypeString, RAttrClass);
    rclass->setValue(QString(QLatin1String("Other")));
}

/*! Returns the 16 bit group address.
 */
uint16_t Group::address() const
{
    return m_addr;
}

/*! Sets the 16 bit group address.
    \param address the 16 bit group address (ZigBee level)
 */
void Group::setAddress(uint16_t address)
{
    m_addr = address;
    m_id = QString::number(address);
}

/*! Returns the group identifier. */
const QString &Group::id() const
{
    return m_id;
}

/*! Returns the group name.
 */
const QString &Group::name() const
{
    return item(RAttrName)->toString();
}

/*! Sets the group name.
    \param name the group name
 */
void Group::setName(const QString &name)
{
    ResourceItem *it = item(RAttrName);
    it->setValue(name);
}

/*! Returns the group state.
 */
Group::State Group::state() const
{
    return m_state;
}

/*! Sets the group state.
    \param state the group state
 */
void Group::setState(State state)
{
    m_state = state;
}

/*! Returns true if the group is on. */
bool Group::isOn() const
{
    return m_on;
}

/*! Sets the group on state.
    \param on whereever the group is on or off
 */
void Group::setIsOn(bool on)
{
    m_on = on;
}

/*! Sets the group color loop active state.
    \param colorLoopActive whereever the color loop is active
 */
void Group::setColorLoopActive(bool colorLoopActive)
{
    m_colorLoopActive = colorLoopActive;
}

/*! Returns true if the color loop is active. */
bool Group::isColorLoopActive() const
{
    return m_colorLoopActive;
}

/*! Handles admin when ResourceItem value has been set.
 * \param i ResourceItem
 */
void Group::didSetValue(ResourceItem *i)
{
    enqueueEvent(Event(RGroups, i->descriptor().suffix, id(), i));
}

/*! multiDeviceIds to string. */
const QString Group::midsToString() const
{
    QString result = "";

    std::vector<QString>::const_iterator i = m_multiDeviceIds.begin();
    std::vector<QString>::const_iterator end = m_multiDeviceIds.end();

    for (;i != end; ++i)
    {
        result.append(*i);
        if (i != end-1)
        {
            result.append(QLatin1String(","));
        }
    }

    return result;
}

/*! multiDeviceIds String to vector. */
void Group::setMidsFromString(const QString &mids)
{
    QStringList list = mids.split(QLatin1String(","), SKIP_EMPTY_PARTS);

    QStringList::const_iterator i = list.begin();
    QStringList::const_iterator end = list.end();

    for (;i != end; ++i)
    {
        m_multiDeviceIds.push_back(*i);
    }
}

/*! deviceMembership to string. */
const QString Group::dmToString() const
{
    QString result(QLatin1String(""));

    std::vector<QString>::const_iterator i = m_deviceMemberships.begin();
    std::vector<QString>::const_iterator end = m_deviceMemberships.end();

    for (;i != end; ++i)
    {
        result.append(*i);
        if (i != end-1)
        {
            result.append(",");
        }
    }

    return result;
}

/*! deviceMembership String to vector. */
void Group::setDmFromString(const QString &deviceIds)
{
    QStringList list = deviceIds.split(QLatin1String(","), SKIP_EMPTY_PARTS);

    QStringList::const_iterator i = list.begin();
    QStringList::const_iterator end = list.end();

    for (;i != end; ++i)
    {
        m_deviceMemberships.push_back(*i);
    }
}

/*! lightsequence to string. */
const QString Group::lightsequenceToString() const
{
    QString result(QLatin1String(""));

    std::vector<QString>::const_iterator i = m_lightsequence.begin();
    std::vector<QString>::const_iterator end = m_lightsequence.end();

    for (;i != end; ++i)
    {
        result.append(*i);
        if (i != end-1)
        {
            result.append(QLatin1String(","));
        }
    }

    return result;
}

/*! lightsequence String to vector. */
void Group::setLightsequenceFromString(const QString &lightsequence)
{
    QStringList list = lightsequence.split(QLatin1String(","), SKIP_EMPTY_PARTS);

    QStringList::const_iterator i = list.begin();
    QStringList::const_iterator end = list.end();

    for (;i != end; ++i)
    {
        m_lightsequence.push_back(*i);
    }
}

/*! Returns the scene for a given \p sceneId or 0 if not present. */
Scene *Group::getScene(quint8 sceneId)
{
    std::vector<Scene>::iterator i = scenes.begin();
    std::vector<Scene>::iterator end = scenes.end();
    for (; i != end; ++i)
    {
        if (i->id == sceneId && i->state == Scene::StateNormal)
        {
            return &*i;
        }
    }

    return 0;
}

/*! Returns true if device with \p id was added to the group. */
bool Group::addDeviceMembership(const QString &id)
{
    if (std::find(m_deviceMemberships.begin(), m_deviceMemberships.end(), id) == m_deviceMemberships.end())
    {
        m_deviceMemberships.push_back(id);
        return true;
    }

    return false;
}

/*! Returns true if device with \p id was removed from the group. */
bool Group::removeDeviceMembership(const QString &id)
{
    std::vector<QString>::iterator i = std::find(m_deviceMemberships.begin(), m_deviceMemberships.end(), id);

    if (i != m_deviceMemberships.end())
    {
        *i = m_deviceMemberships.back();
        m_deviceMemberships.pop_back();
        return true;
    }

    return false;
}

/*! Returns true if device with \p id controls the group. */
bool Group::deviceIsMember(const QString &id) const
{
    if (std::find(m_deviceMemberships.begin(), m_deviceMemberships.end(), id) == m_deviceMemberships.end())
    {
        return false;
    }
    return true;
}

/*! Returns true if group is controlled by devices. */
bool Group::hasDeviceMembers() const
{
    return !m_deviceMemberships.empty();
}
