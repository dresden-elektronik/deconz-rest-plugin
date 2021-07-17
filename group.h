/*
 * Copyright (c) 2016 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef GROUP_H
#define GROUP_H

#include <stdint.h>
#include <QString>
#include <QTime>
#include <vector>
#include "resource.h"
#include "scene.h"

/*! \class Group

    Represents the group state of lights.
 */
class Group : public Resource
{
public:
    enum State
    {
        StateNormal,
        StateDeleted,
        StateDeleteFromDB
    };

    Group();
    uint16_t address() const;
    void setAddress(uint16_t address);
    const QString &id() const;
    const QString &name() const;
    void setName(const QString &name);
    State state() const;
    void setState(State state);
    bool isOn() const;
    void setIsOn(bool on);
    void setColorLoopActive(bool colorLoopActive);
    bool isColorLoopActive() const;
    void didSetValue(ResourceItem *i) override;
    const QString midsToString() const;
    void setMidsFromString(const QString &mids);
    const QString dmToString() const;
    void setDmFromString(const QString &deviceIds);
    const QString lightsequenceToString() const;
    void setLightsequenceFromString(const QString &deviceIds);
    Scene *getScene(quint8 sceneId);
    bool addDeviceMembership(const QString &id);
    bool removeDeviceMembership(const QString &id);
    bool deviceIsMember(const QString &id) const;
    bool hasDeviceMembers() const;

    uint16_t colorX;
    uint16_t colorY;
    uint16_t hue;
    qreal hueReal;
    uint16_t sat;
    uint16_t level;
    uint16_t colorTemperature;
    QString etag;
    QString colormode;
    QString alert;
    std::vector<Scene> scenes;
    QTime sendTime;
    bool hidden;
    std::vector<QString> m_multiDeviceIds;
    std::vector<QString> m_lightsequence;
    std::vector<QString> m_deviceMemberships;

private:
    State m_state;
    uint16_t m_addr;
    QString m_id;
    bool m_on;
    bool m_colorLoopActive;
};

#endif // GROUP_H
