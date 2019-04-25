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
#include "json.h"

#include "light_state.h"


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

#endif // SCENE_H
