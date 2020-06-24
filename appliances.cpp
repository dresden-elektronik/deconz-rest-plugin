#include "de_web_plugin.h"
#include "de_web_plugin_private.h"

// server receive
#define CMD_GET_ALERTS              0x00
// server send
#define CMD_GET_ALERTS_RESPONSE     0x00
#define CMD_ALERTS_NOTIFICATION     0x01
#define CMD_EVENT_NOTIFICATION      0x02

// read flags
#define ALERTS_ALERT (1 << 12)

/*! Handle packets related to the Appliance Alerts and Events cluster.
    \param ind the APS level data indication containing the ZCL packet
    \param zclFrame the actual ZCL frame which holds the Appliance Alerts and Events server command
 */
void DeRestPluginPrivate::handleApplianceAlertClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    QDataStream stream(zclFrame.payload());
    stream.setByteOrder(QDataStream::LittleEndian);

    if (!(zclFrame.frameControl() & deCONZ::ZclFCDirectionServerToClient))
    {
        return;
    }
    
    if (zclFrame.commandId() == CMD_ALERTS_NOTIFICATION && zclFrame.isClusterCommand())
    {
        quint8 alertsCount;      // Count is just 4 Bits but doesn't matter right now
        quint16 alertsStructure; // 24 Bit long, but 16 suffice

        stream >> alertsCount;
        stream >> alertsStructure;
        
        // Specific to leakSMART water sensor V2
        // Should be more generic
        SensorFingerprint fp;
        fp.endpoint = 0x01;
        fp.deviceId = 0x0302;
        fp.profileId = 0x0104;
        fp.inClusters.push_back(POWER_CONFIGURATION_CLUSTER_ID);
        fp.inClusters.push_back(APPLIANCE_EVENTS_AND_ALERTS_CLUSTER_ID);
        Sensor *sensor = getSensorNodeForFingerPrint(ind.srcAddress().ext(), fp, "ZHAWater");
        
        ResourceItem *item = sensor ? sensor->item(RStateWater) : nullptr;

        if (sensor && item)
        {
            if (alertsStructure & ALERTS_ALERT)
            {
                item->setValue(true);
            }
            else
            {
                item->setValue(false);
            }
            sensor->updateStateTimestamp();
            enqueueEvent(Event(RSensors, RStateWater, sensor->id(), item));
            enqueueEvent(Event(RSensors, RStateLastUpdated, sensor->id()));
            sensor->setNeedSaveDatabase(true);
            queSaveDb(DB_SENSORS, DB_SHORT_SAVE_DELAY);
            updateSensorEtag(&*sensor);
        }
    }
}