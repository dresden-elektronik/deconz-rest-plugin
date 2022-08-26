#include "alarm_system_event_handler.h"
#include "de_web_plugin_private.h"
#include "ui/device_widget.h"

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
    else if (e.resource() == RConfig)
    {
        if (deviceWidget)
        {
            deviceWidget->handleEvent(e);
        }
    }
    else if (e.resource() == RDevices && e.what() == REventDDFInitResponse)
    {
        needRuleCheck = RULE_CHECK_DELAY;
    }

    if (e.deviceKey() != 0)
    {
        auto *device = DEV_GetDevice(m_devices, e.deviceKey());
        if (device)
        {
            device->handleEvent(e);
        }
    }

    handleRuleEvent(e);
}
