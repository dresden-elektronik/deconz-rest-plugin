#include <array>
#include "event.h"
#include "resource.h"

constexpr const int MaxEventDataBuffers = 64;
constexpr const int MaxEventDataSize = 256;

struct EventData
{
    quint16 id;
    quint8 data[MaxEventDataSize];
};


static size_t _eventDataIter = 0;
static EventData _eventData[MaxEventDataBuffers];


quint16 allocDataBuffer()
{
    _eventDataIter = (_eventDataIter + 1) % MaxEventDataBuffers;
    _eventData[_eventDataIter].id++;
    return int(_eventDataIter);
}

/*! Constructor.
 */
Event::Event()
{
    m_num = 0;
    m_numPrev = 0;
    m_hasData = 0;
    m_urgent = 0;
}

Event::Event(const char *resource, const char *what, const QString &id, ResourceItem *item, DeviceKey deviceKey) :
    m_resource(resource),
    m_what(what),
    m_id(id),
    m_num(0),
    m_numPrev(0),
    m_deviceKey(deviceKey),
    m_hasData(0),
    m_urgent(0)
{
    DBG_Assert(item != 0);
    if (item)
    {
        m_num = item->toNumber();
        m_numPrev = item->toNumberPrevious();
    }
}

/*! Constructor.
 */
Event::Event(const char *resource, const char *what, const QString &id, DeviceKey deviceKey) :
    m_resource(resource),
    m_what(what),
    m_id(id),
    m_num(0),
    m_numPrev(0),
    m_deviceKey(deviceKey),
    m_hasData(0),
    m_urgent(0)
{

}

/*! Constructor.
 */
Event::Event(const char *resource, const char *what, const QString &id, int num, DeviceKey deviceKey) :
    m_resource(resource),
    m_what(what),
    m_id(id),
    m_num(num),
    m_numPrev(0),
    m_deviceKey(deviceKey),
    m_hasData(0),
    m_urgent(0)
{

}

/*! Constructor.
 */
Event::Event(const char *resource, const char *what, int num, DeviceKey deviceKey) :
    m_resource(resource),
    m_what(what),
    m_num(num),
    m_numPrev(0),
    m_deviceKey(deviceKey),
    m_hasData(0),
    m_urgent(0)
{
    if (resource == RGroups)
    {
        m_id = QString::number(num);
    }
}

Event::Event(const char *resource, const char *what, const void *data, size_t size, DeviceKey deviceKey) :
    m_resource(resource),
    m_what(what),
    m_deviceKey(deviceKey),
    m_hasData(1),
    m_urgent(0)
{
    Q_ASSERT(data);
    Q_ASSERT(size > 0 && size <= MaxEventDataSize);
    m_num = 0;
    m_numPrev = 0;
    m_dataIndex = allocDataBuffer();
    m_dataId = _eventData[m_dataIndex].id;
    m_dataSize = size;
    memcpy(_eventData[m_dataIndex].data, data, size);
}

bool Event::hasData() const
{
    if (m_hasData != 1) { return false; }
    if (m_dataIndex >= MaxEventDataSize) { return false; }
    if (m_dataId != _eventData[m_dataIndex].id) { return false; }

    return true;
}

bool Event::getData(void *dst, size_t size) const
{
    if (size == m_dataSize && hasData())
    {
        memcpy(dst, _eventData[m_dataIndex].data, size);
        return true;
    }

    Q_ASSERT(0);

    return false;
}
