#include "de_web_plugin_private.h"

/*! Handles one event and fires again if more are in the queue.
 */
void DeRestPluginPrivate::handleEvent(const Event &e)
{
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
}
