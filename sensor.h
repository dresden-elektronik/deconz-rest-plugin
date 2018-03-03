/*
 * Copyright (c) 2017 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef SENSOR_H
#define SENSOR_H

#include <QString>
#include <deconz.h>
#include "resource.h"
#include "rest_node_base.h"

// Sensor State
#define INVALID_ENDPOINT 0xff
#define SENSOR_CHECK_COUNTER_INIT 10
#define S_BUTTON_ACTION_INITIAL_PRESS   0
#define S_BUTTON_ACTION_HOLD            1
#define S_BUTTON_ACTION_SHORT_RELEASED  2
#define S_BUTTON_ACTION_LONG_RELEASED   3
#define S_BUTTON_ACTION_DOUBLE_PRESS    4
#define S_BUTTON_ACTION_TREBLE_PRESS    5
#define S_BUTTON_ACTION_QUADRUPLE_PRESS 6
#define S_BUTTON_ACTION_SHAKE           7

#define S_BUTTON_1   1000
#define S_BUTTON_2   2000
#define S_BUTTON_3   3000
#define S_BUTTON_4   4000
#define S_BUTTON_5   5000
#define S_BUTTON_6   6000
#define S_BUTTON_7   7000
#define S_BUTTON_8   8000

struct SensorFingerprint
{
    SensorFingerprint() : checkCounter(0), endpoint(INVALID_ENDPOINT), profileId(0xffff), deviceId(0xffff) {}
    bool operator==(const SensorFingerprint &rhs) const
    {
        return (endpoint == rhs.endpoint &&
                profileId == rhs.profileId &&
                deviceId == rhs.deviceId &&
                inClusters == rhs.inClusters &&
                outClusters == rhs.outClusters);
    }
    QString toString() const;
    bool readFromJsonString(const QString &json);
    bool hasEndpoint() const { return endpoint != 0xFF; }
    bool hasInCluster(quint16 clusterId) const;
    bool hasOutCluster(quint16 clusterId) const;
    int checkCounter;
    quint8 endpoint;
    quint16 profileId;
    quint16 deviceId;
    std::vector<quint16> inClusters;
    std::vector<quint16> outClusters;
};

/*! \class Sensor

    Represents a HA based Sensor.
 */
class Sensor : public Resource,
               public RestNodeBase
{
public:
    enum SensorMode
    {
        ModeNone = 0,
        ModeScenes = 1,
        ModeTwoGroups = 2,
        ModeColorTemperature = 3,
        ModeDimmer = 4
    };

    enum DeletedState
    {
        StateNormal,
        StateDeleted
    };

    struct ButtonMap
    {
        Sensor::SensorMode mode;
        quint8 endpoint;
        quint16 clusterId;
        quint8 zclCommandId;
        quint16 zclParam0;
        int button;
        const char *name;
    };

    Sensor();

    DeletedState deletedState() const;
    void setDeletedState(DeletedState deletedstate);
    bool isAvailable() const;
    const QString &name() const;
    void setName(const QString &name);
    const QString &type() const;
    void setType(const QString &type);
    const QString &modelId() const;
    void setModelId(const QString &mid);
    const QString &manufacturer() const;
    void setManufacturer(const QString &manufacturer);
    const QString &swVersion() const;
    void setSwVersion(const QString &swversion);
    SensorMode mode() const;
    void setMode(SensorMode mode);
    uint8_t resetRetryCount() const;
    void setResetRetryCount(uint8_t resetRetryCount);
    uint8_t zdpResetSeq() const;
    void setZdpResetSeq(uint8_t zdpResetSeq);
    void updateStateTimestamp();
    void incrementRxCounter();
    int rxCounter() const;

    QString stateToString();
    QString configToString();

    void jsonToState(const QString &json);
    void jsonToConfig(const QString &json);
    SensorFingerprint &fingerPrint();
    const SensorFingerprint &fingerPrint() const;

    QString etag;
    const ButtonMap *buttonMap();
    uint8_t previousDirection;
    QDateTime lastStatePush;
    QDateTime lastConfigPush;
    QDateTime durationDue;

private:
    DeletedState m_deletedstate;
    QString m_manufacturer;
    QString m_swversion;
    SensorFingerprint m_fingerPrint;
    SensorMode m_mode;
    uint8_t m_resetRetryCount;
    uint8_t m_zdpResetSeq;
    const ButtonMap *m_buttonMap;
    int m_rxCounter;
};

#endif // SENSOR_H
