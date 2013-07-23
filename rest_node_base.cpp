/*
 * Copyright (C) 2013 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "de_web_plugin_private.h"

/*! Constructor.
 */
RestNodeBase::RestNodeBase() :
    m_node(0),
    m_available(false)
{

}

/*! Deconstructor.
 */
RestNodeBase::~RestNodeBase()
{
}

/*! Returns the core node object.
 */
deCONZ::Node *RestNodeBase::node()
{
    return m_node;
}

/*! Sets the core node object.
    \param node the core node
 */
void RestNodeBase::setNode(deCONZ::Node *node)
{
    m_node = node;
}

/*! Returns the modifiable address.
 */
deCONZ::Address &RestNodeBase::address()
{
    return m_addr;
}

/*! Returns the const address.
 */
const deCONZ::Address &RestNodeBase::address() const
{
    return m_addr;
}

/*! Returns true if the node is available.
 */
bool RestNodeBase::isAvailable() const
{
    return m_available;
}

/*! Sets the is available state of the node.
    \param available the available state of the node
 */
void RestNodeBase::setIsAvailable(bool available)
{
    m_available = available;
}

/*! Returns the unique identifier of the node.
 */
const QString &RestNodeBase::id() const
{
    return m_id;
}

/*! Sets the identifier of the node.
    \param id the unique identifier
 */
void RestNodeBase::setId(const QString &id)
{
    m_id = id;
}
