#ifndef EVENT_H
#define EVENT_H

#include <QString>
#include "device.h"

class Resource;
class ResourceItem;

class Event
{
public:
    Event();
    Event(const char *resource, const char *what, const QString &id, ResourceItem *item, DeviceKey deviceKey = 0);
    Event(const char *resource, const char *what, const QString &id, DeviceKey deviceKey = 0);
    Event(const char *resource, const char *what, int num, DeviceKey deviceKey = 0);

    const char *resource() const { return m_resource; }
    const char *what() const { return m_what; }
    const QString &id() const { return m_id; }
    int num() const { return m_num; }
    int numPrevious() const { return m_numPrev; }
    DeviceKey deviceKey() const { return m_deviceKey; }
    void setDeviceKey(DeviceKey key) { m_deviceKey = key; }

private:
    const char *m_resource = nullptr;
    const char *m_what = nullptr;
    QString m_id;
    int m_num = 0;
    int m_numPrev = 0;
    DeviceKey m_deviceKey = 0;
};

//! Unpacks APS confirm id.
inline quint8 EventApsConfirmId(const Event &event)
{
    return event.num() >> 8 & 0xFF;
}

//! Unpacks APS confirm status.
inline quint8 EventApsConfirmStatus(const Event &event)
{
    return event.num() & 0xFF;
}

//! Packs APS id and confirm status into an \c int used as `num` parameter for REventApsConfirm.
inline int EventApsConfirmPack(quint8 id, quint8 status)
{
    return id << 8 | status;
}

#endif // EVENT_H
