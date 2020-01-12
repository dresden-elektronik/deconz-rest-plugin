/*
 * Copyright (c) 2016 dresden elektronik ingenieurtechnik gmbh.
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
#include <vector>
#include <QElapsedTimer>
#include "json.h"

class LightState;

/*! \class Scene

    Represents a Rest API Scene.
 */
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
    bool externalMaster;
    uint16_t groupAddress;
    uint8_t id;
    QString name;

    const uint16_t &transitiontime() const;
    void setTransitiontime(const uint16_t &transitiontime);

    std::vector<LightState> &lights();
    const std::vector<LightState> &lights() const;
    void setLights(const std::vector<LightState> &lights);
    void addLightState(const LightState &light);
    bool deleteLight(const QString &lid);
    LightState *getLightState(const QString &lid);

    static QString lightsToString(const std::vector<LightState> &lights);
    static std::vector<LightState> jsonToLights(const QString &json);

private:
    uint16_t m_transitiontime;
    std::vector<LightState> m_lights;
};


/*! \class LightState

    Represents the State of a Light of a Scene.
 */
class LightState
{

public:
    LightState();

    const QString &lid() const;
    void setLightId(const QString &lid);
    bool on() const;
    void setOn(const bool &on);
    const uint8_t &bri() const;
    void setBri(const uint8_t &bri);
    const uint16_t &x() const;
    void setX(const uint16_t &x);
    const uint16_t &y() const;
    void setY(const uint16_t &y);
    uint16_t colorTemperature() const;
    void setColorTemperature(uint16_t colorTemperature);
    const uint16_t &enhancedHue() const;
    void setEnhancedHue(const uint16_t &enhancedHue);
    const uint8_t &saturation() const;
    void setSaturation(const uint8_t &sat);
    const bool &colorloopActive() const;
    void setColorloopActive(const bool &active);
    const uint8_t &colorloopDirection() const;
    void setColorloopDirection(const uint8_t &direction);
    const uint8_t &colorloopTime() const;
    void setColorloopTime(const uint8_t &time);
    const QString &colorMode() const;
    void setColorMode(const QString &colorMode);
    const uint16_t &transitionTime() const;
    void setTransitionTime(uint16_t transitionTime);
    bool needRead() const { return m_needRead; }
    void setNeedRead(bool needRead);

    QElapsedTimer tVerified;

private:
    QString m_lid;
    bool m_on;
    bool m_needRead;
    uint8_t m_bri;
    uint16_t m_x;
    uint16_t m_y;
    uint16_t m_colorTemperature;
    uint16_t m_enhancedHue;
    uint8_t m_saturation;
    bool m_colorloopActive;
    uint8_t m_colorloopDirection;
    uint8_t m_colorloopTime;
    QString m_colorMode;
    uint16_t m_transitiontime;
};

#endif // SCENE_H
