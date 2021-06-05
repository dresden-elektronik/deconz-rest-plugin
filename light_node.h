/*
 * Copyright (c) 2016-2017 dresden elektronik ingenieurtechnik gmbh.
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
#include "resource.h"
#include "rest_node_base.h"
#include "group_info.h"

#define COLOR_CAP_HUE_SAT         (1 << 0)
#define COLOR_CAP_ENHANCED_HUE    (1 << 1)
#define COLOR_CAP_COLORLOOP       (1 << 2)
#define COLOR_CAP_XY              (1 << 3)
#define COLOR_CAP_CT              (1 << 4)

/*! \class LightNode

    Represents a HA or ZLL based light.
 */
class LightNode : public Resource,
                  public RestNodeBase
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
    bool isAvailable() const;
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
    bool hasColor() const;
    void setColorLoopActive(bool colorLoopActive);
    bool isColorLoopActive() const;
    bool supportsColorLoop() const;
    void setColorLoopSpeed(uint8_t speed);
    uint8_t colorLoopSpeed() const;
    void didSetValue(ResourceItem *i);
    bool setValue(const char *suffix, qint64 val, bool forceUpdate = false);
    bool setValue(const char *suffix, const QString &val, bool forceUpdate = false);
    bool setValue(const char *suffix, const QVariant &val, bool forceUpdate = false);
    void rx();
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
    void jsonToResourceItems(const QString &json);
    QString resourceItemsToJson();

    QString etag;

private:
    State m_state;
    uint8_t m_resetRetryCount;
    uint8_t m_zdpResetSeq;
    uint8_t m_groupCapacity;
    uint16_t m_manufacturerCode;
    uint16_t m_otauClusterId;
    std::vector<GroupInfo> m_groups;
    bool m_colorLoopActive;
    uint8_t m_colorLoopSpeed;
    quint8 m_haEndpoint = 255;
    uint8_t m_groupCount;
    uint8_t m_sceneCapacity;
};

#endif // LIGHT_NODE_H
