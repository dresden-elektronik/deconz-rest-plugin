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
#define S_BUTTON_ACTION_DROP            8
#define S_BUTTON_ACTION_TILT            9
#define S_BUTTON_ACTION_MANY_PRESS      10

#define GESTURE_NONE                     0
#define GESTURE_SHAKE                    1
#define GESTURE_DROP                     2
#define GESTURE_FLIP_90                  3
#define GESTURE_FLIP_180                 4
#define GESTURE_PUSH                     5
#define GESTURE_DOUBLE_TAP               6
#define GESTURE_ROTATE_CLOCKWISE         7
#define GESTURE_ROTATE_COUNTER_CLOCKWISE 8

#define S_BUTTON_1   1000
#define S_BUTTON_2   2000
#define S_BUTTON_3   3000
#define S_BUTTON_4   4000
#define S_BUTTON_5   5000
#define S_BUTTON_6   6000
#define S_BUTTON_7   7000
#define S_BUTTON_8   8000
#define S_BUTTON_9   9000
#define S_BUTTON_10  10000
#define S_BUTTON_11  11000
#define S_BUTTON_12  12000

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
        QString name;
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
    const std::vector<Sensor::ButtonMap> buttonMap(const QMap<QString, std::vector<Sensor::ButtonMap>> &buttonMapData, QMap<QString, QString> &buttonMapForModelId);
    uint8_t previousDirection;
    quint16 previousCt;
    QDateTime durationDue;
    uint16_t previousSequenceNumber = 0xffff;
    uint8_t previousCommandId;
    

private:
    DeletedState m_deletedstate;
    SensorFingerprint m_fingerPrint;
    SensorMode m_mode;
    uint8_t m_resetRetryCount;
    uint8_t m_zdpResetSeq;
    std::vector<Sensor::ButtonMap> m_buttonMap;
    int m_rxCounter;
};

#endif // SENSOR_H
