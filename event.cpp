#include "event.h"
#include "deconz.h"
#include "resource.h"

/*! Constructor.
 */
Event::Event() :
    m_resource(0),
    m_what(0),
    m_num(0),
    m_numPrev(0)
{
}

Event::Event(const char *resource, const char *what, const QString &id, ResourceItem *item) :
    m_resource(resource),
    m_what(what),
    m_id(id),
    m_num(0),
    m_numPrev(0)
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
Event::Event(const char *resource, const char *what, const QString &id, int num) :
    m_resource(resource),
    m_what(what),
    m_id(id),
    m_num(num),
    m_numPrev(0)
{
}

/*! Constructor.
 */
Event::Event(const char *resource, const char *what, int num) :
    m_resource(resource),
    m_what(what),
    m_num(num),
    m_numPrev(0)
{
    if (resource == RGroups)
    {
        m_id = QString::number(num);
    }
}
