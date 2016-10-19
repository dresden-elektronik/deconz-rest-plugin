/*
 * Copyright (c) 2016 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef GROUP_INFO_H
#define GROUP_INFO_H

#include <stdint.h>
#include <vector>

class GroupInfo
{
public:
    enum Action
    {
        ActionNone            = 0x00,
        ActionReadScenes      = 0x01,
        ActionAddToGroup      = 0x02,
        ActionRemoveFromGroup = 0x04
    };

    enum Constants
    {
        MaxActionRetries = 3
    };

    enum State
    {
        StateInGroup,
        StateNotInGroup
    };

    GroupInfo();

    State state;
    uint8_t actions;
    uint16_t id;
    std::vector<uint8_t> addScenes;
    std::vector<uint8_t> removeScenes;
    std::vector<uint8_t> modifyScenes;
    int modifyScenesRetries;

    uint8_t sceneCount() const;
    void setSceneCount(uint8_t sceneCount);

private:

    uint8_t m_sceneCount;
};

#endif // GROUP_INFO_H
