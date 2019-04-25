/*
 * Copyright (c) 2019 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef LIGHT_STATE_H
#define LIGHT_STATE_H

#include <stdint.h>
#include <QString>
#include <QTime>
#include <QVariantMap>


/*! \class LightState
    Represents the State of a Light of a Scene.
 */
class LightState
{

public:
    LightState();
    void copy(LightState& state);

    const QString& lid() const;
    void setLightId(const QString& lid);
    bool on() const;
    void setOn(const bool& on);
    const uint8_t& bri() const;
    void setBri(const uint8_t& bri);
    const uint16_t& x() const;
    void setX(const uint16_t& x);
    const uint16_t& y() const;
    void setY(const uint16_t& y);
    uint16_t colorTemperature() const;
    void setColorTemperature(uint16_t colorTemperature);
    const uint16_t& enhancedHue() const;
    void setEnhancedHue(const uint16_t& enhancedHue);
    const uint8_t& saturation() const;
    void setSaturation(const uint8_t& sat);
    const bool& colorloopActive() const;
    void setColorloopActive(const bool& active);
    const uint8_t& colorloopDirection() const;
    void setColorloopDirection(const uint8_t& direction);
    const uint8_t& colorloopTime() const;
    void setColorloopTime(const uint8_t& time);
    const QString& colorMode() const;
    void setColorMode(const QString& colorMode);
    const uint16_t& transitionTime() const;
    void setTransitionTime(uint16_t transitionTime);
    bool needRead() const { return m_needRead; }
    void setNeedRead(bool needRead);

    QTime tVerified;

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

#endif // LIGHT_STATE_H
