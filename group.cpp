/*
 * Copyright (C) 2013 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "group.h"

/*! Constructor.
 */
Group::Group() :
    m_state(StateNormal),
    m_addr(0),
    m_id("0"),
    m_on(false)
{
   sendTime = QTime::currentTime();
   hueReal = 0;
   hue = 0;
   sat = 127;
   level = 127;
   colorX = 0;
   colorY = 0;
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
    return m_name;
}

/*! Sets the group name.
    \param name the group name
 */
void Group::setName(const QString &name)
{
    m_name = name;
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
