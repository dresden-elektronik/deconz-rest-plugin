/*
 * Copyright (c) 2016 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "group_info.h"

/*! Constructor.
 */
GroupInfo::GroupInfo() :
   state(StateInGroup),
   actions(ActionNone),
   id(0),
   modifyScenesRetries(0),
   m_sceneCount(0)
{
}

/*! Returns the scene Count.
 */
uint8_t GroupInfo::sceneCount() const
{
    return m_sceneCount;
}

/*! Sets the sceneCount.
    \param sceneCount the sceneCount
 */
void GroupInfo::setSceneCount(uint8_t sceneCount)
{
    m_sceneCount = sceneCount;
}
