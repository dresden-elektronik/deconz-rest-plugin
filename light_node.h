/*
 * Copyright (C) 2013 dresden elektronik ingenieurtechnik gmbh.
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

#define READ_MODEL_ID          (1 << 0)
#define READ_SWBUILD_ID        (1 << 1)
#define READ_ON_OFF            (1 << 2)
#define READ_LEVEL             (1 << 3)
#define READ_COLOR             (1 << 4)
#define READ_GROUPS            (1 << 5)
#define READ_SCENES            (1 << 6)

/*! \class LightNode

    Represents a HA or ZLL based light.
 */
class LightNode : public RestNodeBase
{
public:
    LightNode();
    uint16_t manufacturerCode() const;
    void setManufacturerCode(uint16_t code);
    const QString &manufacturer() const;
    const QString &modelId() const;
    void setModelId(const QString &modelId);
    const QString swBuildId() const;
    void setSwBuildId(const QString & swBuildId);
    const QString &name() const;
    void setName(const QString &name);
    const QString &type() const;
    std::vector<GroupInfo> &groups();
    const std::vector<GroupInfo> &groups() const;
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
    const QString &colorMode() const;
    void setColorMode(const QString &colorMode);
    const deCONZ::SimpleDescriptor &haEndpoint() const;
    void setHaEndpoint(const deCONZ::SimpleDescriptor &endpoint);
    bool mustRead(uint32_t readFlags);
    void enableRead(uint32_t readFlags);
    void clearRead(uint32_t readFlags);
    uint8_t groupCapacity() const;
    void setGroupCapacity(uint8_t capacity);
    const QTime &nextReadTime() const;
    void setNextReadTime(const QTime &time);
    int lastRead() const;
    void setLastRead(int lastRead);

    QString etag;

private:
    int m_lastRead; // copy of idleTotalCounter
    uint8_t m_groupCapacity;
    QString m_name;
    QString m_type;
    uint32_t m_read; // bitmap of READ_* flags
    QString m_manufacturer;
    uint16_t m_manufacturerCode;
    QString m_modelId;
    QString m_swBuildId;
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
    QString m_colorMode;
    deCONZ::SimpleDescriptor m_haEndpoint;
    QTime m_nextReadTime;
};

#endif // LIGHT_NODE_H
