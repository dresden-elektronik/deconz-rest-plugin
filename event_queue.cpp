#include "alarm_system_event_handler.h"
#include "de_web_plugin_private.h"

/*! Handles one event and fires again if more are in the queue.
 */
void DeRestPluginPrivate::handleEvent(const Event &e)
{
    if (e.resource() == RSensors)
    {
        handleSensorEvent(e);
        AS_HandleAlarmSystemDeviceEvent(e, alarmSystemDeviceTable.get(), eventEmitter);
    }
    else if (e.resource() == RLights)
    {
        handleLightEvent(e);
        AS_HandleAlarmSystemDeviceEvent(e, alarmSystemDeviceTable.get(), eventEmitter);
    }
    else if (e.resource() == RGroups)
    {
        handleGroupEvent(e);
    }
    else if (e.resource() == RAlarmSystems || e.what() == REventDeviceAlarm)
    {
        if (alarmSystems)
        {
            AS_HandleAlarmSystemEvent(e, *alarmSystems, eventEmitter, webSocketServer);
        }
    }

    handleRuleEvent(e);
}
