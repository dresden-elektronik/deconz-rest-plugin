#ifndef EVENT_H
#define EVENT_H

#include <QString>

// resources
extern const char *EResourceSensors;
extern const char *EResourceLights;
extern const char *EResourceGroups;
extern const char *EResourceConfig;

// what: resouce suffixes
extern const char *EStateButtonEvent;
extern const char *EStatePresence;
extern const char *EStateOpen;
extern const char *EStateDark;
extern const char *EStateLightLevel;
extern const char *EStateTemperature;
extern const char *EStateHumidity;
extern const char *EStateFlag;
extern const char *EStateStatus;
extern const char *EConfigOn;
extern const char *EConfigReachable;

class Event
{
    enum ValueType
    {
        ValueTypeBool,
        ValueTypeNumber,
        ValueTypeString
    };


public:
    Event();
    Event(const char *resource, const char *what, const QString &id, bool value);
    Event(const char *resource, const char *what, const QString &id, int value);
    QString toJSON() const;

    const char *resource() const { return m_resource; }
    const char *what() const { return m_what; }
    const QString &id() const { return m_id; }
    bool boolValue() const { return m_value == 1; }
    int numericValue() const { return m_value; }

    const char *m_resource;
    const char *m_what;

    QString m_id;
    ValueType m_valueType;
    int m_value;
};

#endif // EVENT_H
