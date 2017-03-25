/*
 * Copyright (c) 2016 dresden elektronik ingenieurtechnik gmbh.
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
#include "rest_node_base.h"
#include "json.h"

// Sensor State
#define INVALID_ENDPOINT 0xff

#define S_BUTTON_ACTION_INITIAL_PRESS  0
#define S_BUTTON_ACTION_HOLD           1
#define S_BUTTON_ACTION_SHORT_RELEASED 2
#define S_BUTTON_ACTION_LONG_RELEASED  3

#define S_BUTTON_1   1000
#define S_BUTTON_2   2000
#define S_BUTTON_3   3000
#define S_BUTTON_4   4000
#define S_BUTTON_5   5000
#define S_BUTTON_6   6000
#define S_BUTTON_7   7000
#define S_BUTTON_8   8000

class SensorState
{
public:
    SensorState();

    const QString &lastupdated() const;
    void setLastupdated(const QString &lastupdated);
    void updateTimestamp();
    const QString &flag() const;
    void setFlag(const QString &flag);
    const QString &status() const;
    void setStatus(const QString &status);
    const QString &presence() const;
    void setPresence(const QString &presence);
    const QString &open() const;
    void setOpen(const QString &open);
    int buttonevent() const;
    void setButtonevent(int buttonevent);
    const QString &temperature() const;
    void setTemperature(const QString &temperature);
    const QString &humidity() const;
    void setHumidity(const QString &humidity);
    const QString &daylight() const;
    void setDaylight(const QString &daylight);
    quint32 lux() const;
    void setLux(quint32 lux);

private:
    QString m_lastupdated;
    QString m_flag;//bool
    QString m_status;//int
    QString m_presence;//bool
    QString m_open;//bool
    int m_buttonevent;
    QString m_temperature;//int
    QString m_humidity;//int
    QString m_daylight;//bool
    quint32 m_lux;
};


// Sensor Config

class SensorConfig
{
public:
    SensorConfig();

    bool on() const;
    void setOn(bool on);
    bool reachable() const;
    void setReachable(bool reachable);
    double duration() const;
    void setDuration(double duration);
    quint8 battery() const;
    void setBattery(quint8 battery);
    const QString &url() const;
    void setUrl(const QString &url);
    const QString &longitude() const;
    void setLongitude(const QString &longitude);
    const QString &lat() const;
    void setLat(const QString &lat);
    const QString &sunriseoffset() const;
    void setSunriseoffset(const QString &sunriseoffset);
    const QString &sunsetoffset() const;
    void setSunsetoffset(const QString &sunsetoffset);

private:
    bool m_on;
    bool m_reachable;
    double  m_duration;
    quint8 m_battery;
    QString m_url;
    QString m_long;
    QString m_lat;
    QString m_sunriseoffset;//int8
    QString m_sunsetoffset; //int8
};

struct SensorFingerprint
{
    SensorFingerprint() : endpoint(INVALID_ENDPOINT), profileId(0xffff), deviceId(0xffff) {}
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
    quint8 endpoint;
    quint16 profileId;
    quint16 deviceId;
    std::vector<quint16> inClusters;
    std::vector<quint16> outClusters;
};

/*! \class Sensor

    Represents a HA based Sensor.
 */
class Sensor : public RestNodeBase
{
public:    
    enum SensorMode
    {
        ModeNone = 0,
        ModeScenes = 1,
        ModeTwoGroups = 2,
        ModeColorTemperature = 3
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
    SensorState &state();
    const SensorState &state() const;
    void setState(const SensorState &state);
    const SensorConfig &config() const;
    void setConfig(const SensorConfig &config);
    SensorMode mode() const;
    void setMode(SensorMode mode);
    uint8_t resetRetryCount() const;
    void setResetRetryCount(uint8_t resetRetryCount);
    uint8_t zdpResetSeq() const;
    void setZdpResetSeq(uint8_t zdpResetSeq);

    static QString stateToString(const SensorState &state);
    static QString configToString(const SensorConfig &config);

    static SensorState jsonToState(const QString &json);
    static SensorConfig jsonToConfig(const QString &json);
    SensorFingerprint &fingerPrint();
    const SensorFingerprint &fingerPrint() const;

    QVector<QString> sensorTypes;
    QString etag;
    const ButtonMap *buttonMap();

private:
    DeletedState m_deletedstate;
    QString m_name;
    QString m_type;
    QString m_modelid;
    QString m_manufacturer;
    QString m_swversion;
    SensorState m_state;
    SensorConfig m_config;
    SensorFingerprint m_fingerPrint;
    SensorMode m_mode;
    uint8_t m_resetRetryCount;
    uint8_t m_zdpResetSeq;
    const ButtonMap *m_buttonMap;
};

#endif // SENSOR_H
