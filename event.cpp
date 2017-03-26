#include <QVariantMap>
#include "event.h"
#include "deconz.h"
#include "json.h"

const char *EResourceSensors = "/sensors";
const char *EResourceLights = "/lights";
const char *EResourceGroups = "/groups";
const char *EResourceConfig = "/config";

const char *EStateButtonEvent = "state/buttonevent";
const char *EStatePresence = "state/presence";
const char *EStateOpen = "state/open";
const char *EStateDark = "state/dark";
const char *EStateLightLevel = "state/lightlevel";
const char *EStateTemperature = "state/temperature";
const char *EStateHumidity = "state/humidity";
const char *EStateFlag = "state/flag";
const char *EStateStatus = "state/status";
const char *EConfigOn = "config/on";
const char *EConfigReachable = "config/reachable";

/*! Constructor.
 */
Event::Event() :
    m_resource(0),
    m_what(0),
    m_valueType(ValueTypeBool),
    m_value(0)
{
}

/*! Constructor.
 */
Event::Event(const char *resource, const char *what, const QString &id, bool value) :
    m_resource(resource),
    m_what(what),
    m_id(id),
    m_valueType(ValueTypeBool),
    m_value(value)
{
}

/*! Constructor.
 */
Event::Event(const char *resource, const char *what, const QString &id, int value) :
    m_resource(resource),
    m_what(what),
    m_id(id),
    m_valueType(ValueTypeNumber),
    m_value(value)
{
}

/*! Returns the event as JSON string.
 */
QString Event::toJSON() const
{
    if (!m_resource || !m_what)
    {
        return QString();
    }

    QVariantMap msg;
    msg["t"] = QLatin1String("event");
    msg["r"] = m_resource + 1; // skip front slash (/sensors)
    if (!m_id.isEmpty())
    {
        msg["id"] = m_id;
    }

    QVariant val;
    if      (m_valueType == ValueTypeBool)   { val = (bool)m_value; }
    else if (m_valueType == ValueTypeNumber) { val = (double)m_value; }

    QStringList ls = QString(m_what).split('/', QString::SkipEmptyParts);

    QVariantMap data;
    data[ls.takeLast()] = val;

    while (ls.size() > 1) // nested object
    {
        QVariantMap m;
        m[ls.takeLast()] = data;
        data = m;
    }

    msg[ls.last()] = data;

    return Json::serialize(msg);
}
