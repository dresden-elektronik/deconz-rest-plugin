#include <QVariantMap>
#include "de_web_plugin_private.h"
#include "json.h"

/*! Inits the event queue.
 */
void DeRestPluginPrivate::initEventQueue()
{
    eventTimer = new QTimer(this);
    eventTimer->setSingleShot(true);
    eventTimer->setInterval(1);
    connect(eventTimer, SIGNAL(timeout()), this, SLOT(eventQueueTimerFired()));
}

/*! Handles one event and fires again if more are in the queue.
 */
void DeRestPluginPrivate::eventQueueTimerFired()
{
    DBG_Assert(!eventQueue.empty());

    Event &e = eventQueue.front();

    if (e.resource() == RSensors)
    {
        handleSensorEvent(e);
    }
    else if (e.resource() == RLights)
    {
        handleLightEvent(e);
    }
    else if (e.resource() == RGroups)
    {
        handleGroupEvent(e);
    }

    handleRuleEvent(e);

    eventQueue.pop_front();

    if (!eventQueue.empty())
    {
        eventTimer->start();
    }
}

/*! Puts an event into the queue.
    \param event - the event
 */
void DeRestPluginPrivate::enqueueEvent(const Event &event)
{
    if (DBG_IsEnabled(DBG_INFO_L2) && event.what() && event.resource())
    {
        DBG_Printf(DBG_INFO_L2, "enqueue event %s for %s/%s\n", event.what(), event.resource(), qPrintable(event.id()));
    }

    eventQueue.push_back(event);

    if (!eventTimer->isActive())
    {
        eventTimer->start();
    }
}
