#include "event.h"
#include "deconz.h"

/*! Constructor.
 */
Event::Event() :
    m_resource(0),
    m_what(0)
{
}

/*! Constructor.
 */
Event::Event(const char *resource, const char *what, const QString &id) :
    m_resource(resource),
    m_what(what),
    m_id(id),
    m_num(0)
{
}

/*! Constructor.
 */
Event::Event(const char *resource, const char *what, int num) :
    m_resource(resource),
    m_what(what),
    m_num(num)
{
}

