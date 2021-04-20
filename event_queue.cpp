#include "de_web_plugin.h"
#include "de_web_plugin_private.h"

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
    int maxEvents = 10;

    while (maxEvents > 0 && !eventQueue.empty())
    {
        maxEvents--;
        const Event &e = eventQueue.front();

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

        emit eventNotify(e);

        if (e.deviceKey() != 0)
        {
            auto *device = DEV_GetDevice(m_devices, e.deviceKey());
            if (device)
            {
                device->handleEvent(e);
            }

            // hack to forward first sub device name to core to show it as node name
            if (device && e.what() == REventDDFInitResponse && e.num() > 0)
            {
                const auto subDevices = device->subDevices();
                if (!subDevices.empty())
                {
                    const auto *i = subDevices.front()->item(RAttrName);
                    if (i && !i->toString().isEmpty())
                    {
                        emit q_ptr->nodeUpdated(e.deviceKey(), QLatin1String("name"), i->toString());
                    }
                }
            }
        }

        handleRuleEvent(e);

        eventQueue.pop_front();
    }

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

    RestNodeBase *restNode = nullptr;

    // workaround to attach DeviceKey to an event
    if (event.deviceKey() == 0 && (event.resource() == RSensors || event.resource() == RLights))
    {
        if (event.resource() == RSensors)
        {
            restNode = getSensorNodeForId(event.id());
            if (!restNode)
            {
                restNode = getSensorNodeForUniqueId(event.id());
            }
        }
        else if (event.resource() == RLights)
        {
            restNode = getLightNodeForId(event.id());
        }
    }

    if (restNode && restNode->address().ext() > 0)
    {
        Event e2 = event;
        e2.setDeviceKey(restNode->address().ext());
        eventQueue.push_back(e2);
    }
    else
    {
        eventQueue.push_back(event);
    }

    if (!eventTimer->isActive())
    {
        eventTimer->start();
    }
}
