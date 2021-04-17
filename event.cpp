#include "event.h"
#include "resource.h"

/*! Constructor.
 */
Event::Event()
{
}

Event::Event(const char *resource, const char *what, const QString &id, ResourceItem *item, DeviceKey deviceKey) :
    m_resource(resource),
    m_what(what),
    m_id(id),
    m_deviceKey(deviceKey)
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
    m_deviceKey(deviceKey)
{
}

/*! Constructor.
 */
Event::Event(const char *resource, const char *what, int num, DeviceKey deviceKey) :
    m_resource(resource),
    m_what(what),
    m_num(num),
    m_numPrev(0),
    m_deviceKey(deviceKey)
{
    if (resource == RGroups)
    {
        m_id = QString::number(num);
    }
}
