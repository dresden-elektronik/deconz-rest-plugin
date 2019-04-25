/*
 * Copyright (c) 2016-2019 dresden elektronik ingenieurtechnik gmbh.
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
#include <QDateTime>
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
    enum Type
    {
        LightScene,
        GroupScene
    };

    Scene(const uint16_t gid, const uint8_t sid, const Type type);
    void init(const QString& id, const QString& owner, const QDateTime& lastupdated, const uint16_t version);

    const State& state() const;
    void state(const State& state);
    bool externalMaster() const;
    void externalMaster(const bool externalMaster);

    const QString& id() const;
    uint16_t gid() const;
    uint8_t sid() const;
    const QString& name() const;
    void name(const QString& name);

    const std::vector<LightState>& lights() const;
    void addLight(const LightState& light);
    bool removeLight(const QString& lid);
    LightState* getLight(const QString& lid);

    // deCONZ only
    uint16_t transitiontime() const;
    void transitiontime(const uint16_t transitiontime);

    // Hue only
    const QString& owner() const;
    bool recycle() const;
    void recycle(const bool recycle);
    bool locked() const;
    void locked(const bool locked);
    const QVariantMap& appdata() const;
    void appdata(const QVariantMap& appdata);
    const QString& picture() const;
    void picture(const QString& picture);
    const QDateTime& lastupdated() const;
    void lastupdated(const bool lastupdated);
    uint16_t version() const;

    QVariantMap map() const;

private:
    State m_state;
    Type m_type;
    bool m_externalMaster;

    QString m_id;
    uint16_t m_gid;
    uint8_t m_sid;
    QString m_name;
    std::vector<LightState> m_lights;

    uint16_t m_transitiontime;

    QString m_owner;
    bool m_recycle;
    bool m_locked;
    QVariantMap m_appdata;
    QString m_picture;
    QDateTime m_lastupdated;
    uint16_t m_version;
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

    QVariantMap map() const;
    void map(QVariantMap& map);

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
