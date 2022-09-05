#ifndef EVENT_H
#define EVENT_H

#include <QString>
#include "device.h"

class Resource;
class ResourceItem;

struct EventData;

class Event
{
public:
    Event();
    Event(const char *resource, const char *what, const QString &id, ResourceItem *item, DeviceKey deviceKey = 0);
    Event(const char *resource, const char *what, const QString &id, DeviceKey deviceKey);
    Event(const char *resource, const char *what, const QString &id, int num = 0, DeviceKey deviceKey = 0);
    Event(const char *resource, const char *what, int num, DeviceKey deviceKey = 0);
    //! Don't call following ctor directly use EventWithData() factory function.
    Event(const char *resource, const char *what, const void *data, size_t size, DeviceKey deviceKey = 0);

    const char *resource() const { return m_resource; }
    const char *what() const { return m_what; }
    const QString &id() const { return m_id; }
    int num() const { return m_num; }
    int numPrevious() const { return m_numPrev; }
    DeviceKey deviceKey() const { return m_deviceKey; }
    void setDeviceKey(DeviceKey key) { m_deviceKey = key; }
    bool hasData() const;
    quint16 dataSize() const { return m_dataSize; }
    bool getData(void *dst, size_t size) const;
    bool isUrgent() const { return m_urgent == 1; }
    void setUrgent(bool urgent) { m_urgent = urgent ? 1 : 0; }

private:
    const char *m_resource = nullptr;
    const char *m_what = nullptr;
    QString m_id;
    union
    {
        struct
        {
            int m_num;
            int m_numPrev;
        };

        struct
        {
            quint16 m_dataIndex;
            quint16 m_dataId;
            quint16 m_dataSize;
        };
    };
    DeviceKey m_deviceKey = 0;
    struct
    {
        unsigned char m_hasData : 1;
        unsigned char m_urgent : 1;
        unsigned char _pad : 6;
    };
};

template <typename D>
Event EventWithData(const char *resource, const char *what, const D &data, DeviceKey deviceKey)
{
    static_assert (std::is_trivially_copyable<D>::value, "data needs to be trivially copyable");
    return Event(resource, what, &data, sizeof(data), deviceKey);
}

inline Event EventWithData(const char *resource, const char *what, const void *data, size_t size, DeviceKey deviceKey)
{
    return Event(resource, what, data, size, deviceKey);
}

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

//! Unpacks ZDP sequence number.
inline quint8 EventZdpResponseSequenceNumber(const Event &event)
{
    return event.num() >> 8 & 0xFF;
}

//! Unpacks ZDP sequence number.
inline quint8 EventZdpResponseStatus(const Event &event)
{
    return event.num() & 0xFF;
}

//! Packs APS id and confirm status into an \c int used as `num` parameter for REventApsConfirm.
inline int EventZdpResponsePack(quint8 seq, quint8 status)
{
    return seq << 8 | status;
}

//! Unpacks ZCL ClusterID.
inline unsigned EventZclClusterId(const Event &event)
{
    uint32_t n = static_cast<uint32_t>(event.num());
    return (n >> 16) & 0xFFFF;
}

//! Unpacks ZCL sequence number.
inline quint8 EventZclSequenceNumber(const Event &event)
{
    return (event.num() >> 8) & 0xFF;
}

//! Unpacks ZCL command status.
inline quint8 EventZclStatus(const Event &event)
{
    return event.num() & 0xFF;
}

//! Packs cluster ID, ZCL sequence number and command status into an \c int used as `num` parameter for REventZclResponse.
inline int EventZclResponsePack(uint16_t clusterId, uint8_t seq, uint8_t status)
{
    uint32_t n;

    n = clusterId;
    n <<= 8;
    n |= seq;
    n <<= 8;
    n |= status;

    return static_cast<int>(n);
}

#ifdef DECONZ_DEBUG_BUILD
void EventTestZclPacking();
#endif

//! Packs timer into an \c int used as `num` parameter for REventStartTimer and REventStopTimer.
inline int EventTimerPack(int timerId, int timeoutMs)
{
    Q_ASSERT(timerId <= 0xFF);
    Q_ASSERT(timeoutMs <= 0xFFFFFF);
    return timerId << 24 | timeoutMs;
}

//! Unpacks timer id.
inline int EventTimerId(const Event &event)
{
    return (event.num() >> 24) & 0xFF;
}

//! Unpacks timer timout.
inline int EventTimerTimout(const Event &event)
{
    return event.num() & 0xFFFFFF;
}

#endif // EVENT_H
