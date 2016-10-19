/*
 * Copyright (c) 2016 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef LIGHT_NODE_H
#define LIGHT_NODE_H

#include <QString>
#include <deconz.h>
#include "rest_node_base.h"
#include "group_info.h"

/*! \class LightNode

    Represents a HA or ZLL based light.
 */
class LightNode : public RestNodeBase
{
public:
    enum State
    {
        StateNormal,
        StateDeleted
    };

    LightNode();
    State state() const;
    void setState(State state);
    uint16_t manufacturerCode() const;
    void setManufacturerCode(uint16_t code);
    const QString &manufacturer() const;
    void setManufacturerName(const QString &name);
    const QString &modelId() const;
    void setModelId(const QString &modelId);
    const QString &swBuildId() const;
    void setSwBuildId(const QString & swBuildId);
    const QString &name() const;
    void setName(const QString &name);
    const QString &type() const;
    std::vector<GroupInfo> &groups();
    const std::vector<GroupInfo> &groups() const;
    uint16_t otauClusterId() const;
    void setOtauClusterId(uint16_t clusterId);
    bool isOn() const;
    bool hasColor() const;
    void setIsOn(bool on);
    uint16_t level() const;
    void setLevel(uint16_t level);
    uint8_t hue() const;
    void setHue(uint8_t hue);
    uint16_t enhancedHue() const;
    void setEnhancedHue(uint16_t ehue);
    uint8_t saturation() const;
    void setSaturation(uint8_t sat);
    void setColorXY(uint16_t x, uint16_t y);
    uint16_t colorX() const;
    uint16_t colorY() const;
    uint16_t colorTemperature() const;
    void setColorTemperature(uint16_t colorTemperature);
    const QString &colorMode() const;
    void setColorMode(const QString &colorMode);
    void setColorLoopActive(bool colorLoopActive);
    bool isColorLoopActive() const;
    void setColorLoopSpeed(uint8_t speed);
    uint8_t colorLoopSpeed() const;
    const deCONZ::SimpleDescriptor &haEndpoint() const;
    void setHaEndpoint(const deCONZ::SimpleDescriptor &endpoint);
    uint8_t groupCapacity() const;
    void setGroupCapacity(uint8_t capacity);
    uint8_t resetRetryCount() const;
    void setResetRetryCount(uint8_t resetRetryCount);
    uint8_t zdpResetSeq() const;
    void setZdpResetSeq(uint8_t zdpResetSeq);
    uint8_t groupCount() const;
    void setGroupCount(uint8_t groupCount);
    uint8_t sceneCapacity() const;
    void setSceneCapacity(uint8_t sceneCapacity);

    QString etag;

private:
    State m_state;
    uint8_t m_resetRetryCount;
    uint8_t m_zdpResetSeq;
    uint8_t m_groupCapacity;
    QString m_name;
    QString m_type;
    QString m_manufacturer;
    uint16_t m_manufacturerCode;
    QString m_modelId;
    QString m_swBuildId;
    uint16_t m_otauClusterId;
    std::vector<GroupInfo> m_groups;
    bool m_isOn;
    bool m_hasColor;
    uint16_t m_level;
    uint8_t m_hue;
    uint16_t m_ehue;
    double m_normHue;
    uint8_t m_sat;
    uint16_t m_colorX;
    uint16_t m_colorY;
    uint16_t m_colorTemperature;
    QString m_colorMode;
    bool m_colorLoopActive;
    uint8_t m_colorLoopSpeed;
    deCONZ::SimpleDescriptor m_haEndpoint;
    uint8_t m_groupCount;
    uint8_t m_sceneCapacity;
};

#endif // LIGHT_NODE_H

