/*
 * Copyright (C) 2013 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef SCENE_H
#define SCENE_H

#include <stdint.h>
#include <QString>

class Scene
{
public:
    enum State
    {
        StateNormal,
        StateDeleted
    };

    Scene();
    State state;
    uint16_t groupAddress;
    uint8_t id;
    QString name;
};

#endif // SCENE_H
