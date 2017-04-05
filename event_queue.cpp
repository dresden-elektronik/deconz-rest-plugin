#include <QVariantMap>
#include "de_web_plugin_private.h"
#include "json.h"

/*! Inits the event queue.
 */
void DeRestPluginPrivate::initEventQueue()
{
    eventTimer = new QTimer(this);
    eventTimer->setSingleShot(true);
    eventTimer->setInterval(50);
    connect(eventTimer, SIGNAL(timeout()), this, SLOT(eventQueueTimerFired()));
}

/*! Handles one event and fires again if more are in the queue.
 */
void DeRestPluginPrivate::eventQueueTimerFired()
{
    DBG_Assert(!eventQueue.empty());

    Event &e = eventQueue.front();

    // push sensor state updates through websocket
    if (e.resource() == RSensors &&
        strncmp(e.what(), "state/", 6) == 0)
    {
        Sensor *sensor = getSensorNodeForId(e.id());
        ResourceItem *item = sensor ? sensor->item(e.what()) : 0;
        if (sensor && item)
        {
            QVariantMap map;
            map["t"] = QLatin1String("event");
            map["e"] = QLatin1String("changed");
            map["r"] = QLatin1String("sensors");
            map["id"] = e.id();
            QVariantMap state;
            state[e.what() + 6] = item->toVariant();
            map["state"] = state;

            webSocketServer->broadcastTextMessage(Json::serialize(map));
        }
    }
    else if (e.resource() == RSensors && e.what() == REventAdded)
    {
        Sensor *sensor = getSensorNodeForId(e.id());

        if (sensor)
        {
            QVariantMap res;
            res["name"] = sensor->name();
            findSensorResult[sensor->id()] = res;

            QVariantMap map;
            map["t"] = QLatin1String("event");
            map["e"] = QLatin1String("added");
            map["r"] = QLatin1String("sensors");

            QVariantMap smap;
            sensorToMap(sensor, smap);
            smap["id"] = sensor->id();
            map["sensor"] = smap;

            webSocketServer->broadcastTextMessage(Json::serialize(map));
        }
    }

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
    eventQueue.push_back(event);

    if (!eventTimer->isActive())
    {
        eventTimer->start();
    }
}
