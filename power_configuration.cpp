#include "device_descriptions.h"
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "utils/utils.h"

#define POWER_CONFIG_ATTRID_BATTERY_VOLTAGE               0x0020
#define POWER_CONFIG_ATTRID_BATTERY_PERCENTAGE_REMAINING  0x0021
#define POWER_CONFIG_ATTRID_BATTERY_ALARM_MASK            0x0035

static quint8 calculateBatteryPercentageRemaining(const Resource *r, ResourceItem *item, const quint8 batteryVoltage, const float vmin, const float vmax)
{
    float batteryPercentage = batteryVoltage;

    if      (batteryPercentage > vmax) { batteryPercentage = vmax; }
    else if (batteryPercentage < vmin) { batteryPercentage = vmin; }

    batteryPercentage = ((batteryPercentage - vmin) / (vmax - vmin)) * 100;

    if      (batteryPercentage > 100) { batteryPercentage = 100; }
    else if (batteryPercentage <= 0)  { batteryPercentage = 1; } // ?

    if (r && item)
    {
        const int maxSize = 384;
        auto jsEval = std::make_unique<char[]>(maxSize);
        int ret = snprintf(jsEval.get(), maxSize,
                          "const vmin = %u;"
                          " const vmax = %u;"
                          " let bat = Attr.val;"

                          " if (bat > vmax) { bat = vmax; }"
                          " else if (bat < vmin) { bat = vmin; }"

                          " bat = ((bat - vmin) / (vmax - vmin)) * 100;"

                          " if (bat > 100) { bat = 100; }"
                          " else if (bat <= 0)  { bat = 1; }"

                          " Item.val = bat;", unsigned(vmin), unsigned(vmax));

        DBG_Assert(ret < maxSize);
        if (ret > 0 && ret < maxSize && jsEval[ret] == '\0')
        {
            DDF_AnnoteZclParse(r, item, 255, POWER_CONFIGURATION_CLUSTER_ID, POWER_CONFIG_ATTRID_BATTERY_VOLTAGE, jsEval.get());
        }
    }

    return static_cast<quint8>(batteryPercentage);
}

/*! Handle packets related to the ZCL power configuration cluster.
    \param ind the APS level data indication containing the ZCL packet
    \param zclFrame the actual ZCL frame which holds the power configuration cluster command or attribute
 */
void DeRestPluginPrivate::handlePowerConfigurationClusterIndication(const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame)
{
    if (zclFrame.isDefaultResponse())
    {
        return;
    }

    QDataStream stream(zclFrame.payload());
    stream.setByteOrder(QDataStream::LittleEndian);

    bool isReadAttr = false;

    if (zclFrame.isProfileWideCommand() && zclFrame.commandId() == deCONZ::ZclReadAttributesResponseId)
    {
        isReadAttr = true;
    }
    else if (zclFrame.isProfileWideCommand() && zclFrame.commandId() == deCONZ::ZclReportAttributesId)
    {
    }
    else
    {
        return; // neither ZCL Report nor ZCL Read Attributes Response
    }

    const NodeValue::UpdateType updateType = isReadAttr ? NodeValue::UpdateByZclRead : NodeValue::UpdateByZclReport;

    while (!stream.atEnd())
    {
        quint16 attrId;
        quint8 attrTypeId;

        stream >> attrId;
        if (isReadAttr)
        {
            quint8 status;
            stream >> status;  // Read Attribute Response status
            if (status != deCONZ::ZclSuccessStatus)
            {
                continue;
            }
        }
        stream >> attrTypeId;

        deCONZ::ZclAttribute attr(attrId, attrTypeId, QLatin1String(""), deCONZ::ZclRead, false);

        if (!attr.readFromStream(stream))
        {
            continue;
        }

        for (Sensor &sensor: sensors)
        {
            // Interate through all sensors of a device, but we don't care about the endpoint to distrubute the battery value
            if (sensor.deletedState() != Sensor::StateNormal || !sensor.node())     { continue; }
            if (!isSameAddress(sensor.address(), ind.srcAddress()))                 { continue; }

            if (sensor.mustRead(READ_BATTERY))
            {
                sensor.clearRead(READ_BATTERY);
            }

            switch (attrId)
            {
            case POWER_CONFIG_ATTRID_BATTERY_PERCENTAGE_REMAINING:
            {
                // Specifies the remaining battery life as a half integer percentage of the full battery capacity (e.g., 34.5%, 45%,
                // 68.5%, 90%) with a range between zero and 100%, with 0x00 = 0%, 0x64 = 50%, and 0xC8 = 100%. This is
                // particularly suited for devices with rechargeable batteries.

                uint divider = 2;

                if (sensor.modelId().startsWith(QLatin1String("TRADFRI")) || // IKEA
                    sensor.modelId().startsWith(QLatin1String("KADRILJ")) || // IKEA
                    sensor.modelId().startsWith(QLatin1String("SYMFONISK")) || // IKEA
                    sensor.modelId().startsWith(QLatin1String("Remote Control N2")) || // IKEA
                    sensor.modelId().startsWith(QLatin1String("ICZB-")) || // iCasa keypads and remote
                    sensor.modelId().startsWith(QLatin1String("ZGR904-S")) || // Envilar remote
                    sensor.modelId().startsWith(QLatin1String("ZGRC-KEY")) || //  Sunricher wireless CCT remote
                    sensor.modelId().startsWith(QLatin1String("iTRV")) || // Drayton Wiser Radiator Thermostat
                    sensor.modelId().startsWith(QLatin1String("SV01-")) || // Keen Home vent
                    sensor.modelId().startsWith(QLatin1String("SV02-")) || // Keen Home vent
                    sensor.modelId() == QLatin1String("4512705") || // Namron remote control
                    sensor.modelId() == QLatin1String("4512726") || // Namron rotary switch
                    sensor.modelId().startsWith(QLatin1String("S57003")) || // SLC 4-ch remote controller
                    sensor.modelId().startsWith(QLatin1String("RGBgenie ZB-5")) || // RGBgenie remote control
                    sensor.modelId().startsWith(QLatin1String("VOC_Sensor")) || // LifeControl Enviroment sensor
                    sensor.modelId().startsWith(QLatin1String("TY0203")) || // SilverCrest / lidl
                    sensor.modelId().startsWith(QLatin1String("TY0202")) || // SilverCrest / lidl
                    sensor.modelId().startsWith(QLatin1String("ZG2835")))   // SR-ZG2835 Zigbee Rotary Switch
                {
                    divider = 1;
                }

                int bat = attr.numericValue().u8 / divider;

                if (sensor.modelId() == QLatin1String("0x8020") || // Danfoss RT24V Display thermostat
                    sensor.modelId() == QLatin1String("0x8021") || // Danfoss RT24V Display thermostat with floor sensor
                    sensor.modelId() == QLatin1String("0x8030") || // Danfoss RTbattery Display thermostat
                    sensor.modelId() == QLatin1String("0x8031") || // Danfoss RTbattery Display thermostat with infrared
                    sensor.modelId() == QLatin1String("0x8034") || // Danfoss RTbattery Dial thermostat
                    sensor.modelId() == QLatin1String("0x8035"))   // Danfoss RTbattery Dial thermostat with infrared
                {
                    // The Danfoss Icon Zigbee module exposes each in-room thermostat in its controller
                    // as an endpoint. Each endpoint has the battery measurement for the device it represents.
                    // This check makes sure none of the other endpoints get their battery value overwritten.
                    if (ind.srcEndpoint() != sensor.fingerPrint().endpoint)
                    {
                        continue;
                    }
                }

                ResourceItem *item = nullptr;

                if (sensor.type().endsWith(QLatin1String("Battery")))
                {
                    item = sensor.item(RStateBattery);

                    if (item)
                    {
                        item->setValue(bat);
                        sensor.updateStateTimestamp();
                        sensor.setNeedSaveDatabase(true);
                        queSaveDb(DB_SENSORS, DB_HUGE_SAVE_DELAY);
                        enqueueEvent(Event(RSensors, RStateBattery, sensor.id(), item));
                        enqueueEvent(Event(RSensors, RStateLastUpdated, sensor.id()));
                        updateSensorEtag(&sensor);
                    }
                }
                else
                {
                    item = sensor.item(RConfigBattery);

                    if (!item && attr.numericValue().u8 > 0) // valid value: create resource item
                    {
                        item = sensor.addItem(DataTypeUInt8, RConfigBattery);
                    }

                    if (item)
                    {
                        item->setValue(bat);
                        enqueueEvent(Event(RSensors, RConfigBattery, sensor.id(), item));
                        updateSensorEtag(&sensor);
                        sensor.setNeedSaveDatabase(true);
                        queSaveDb(DB_SENSORS, DB_SHORT_SAVE_DELAY);
                    }
                }

                if (item)
                {
                    if (divider == 1)
                    {
                        DDF_AnnoteZclParse(&sensor, item, ind.srcEndpoint(), ind.clusterId(), attrId, "Item.val = Attr.val");
                    }
                    else if (divider == 2)
                    {
                        DDF_AnnoteZclParse(&sensor, item, ind.srcEndpoint(), ind.clusterId(), attrId, "Item.val = Attr.val / 2");
                    }
                }

                // Correct incomplete sensor fingerprint
                if (!sensor.fingerPrint().hasInCluster(POWER_CONFIGURATION_CLUSTER_ID))
                {
                    sensor.fingerPrint().inClusters.push_back(POWER_CONFIGURATION_CLUSTER_ID);
                }

                sensor.setZclValue(updateType, ind.srcEndpoint(), POWER_CONFIGURATION_CLUSTER_ID, POWER_CONFIG_ATTRID_BATTERY_PERCENTAGE_REMAINING, attr.numericValue());
            }
                break;

            case POWER_CONFIG_ATTRID_BATTERY_VOLTAGE:
            {
                if (sensor.modelId().startsWith(QLatin1String("tagv4")) ||   // SmartThings Arrival sensor
                    sensor.modelId().startsWith(QLatin1String("motionv4")) ||// SmartThings motion sensor
                    sensor.modelId().startsWith(QLatin1String("moisturev4")) ||// SmartThings water leak sensor
                    sensor.modelId().startsWith(QLatin1String("multiv4")) ||// SmartThings multi sensor 2016
                    sensor.modelId().startsWith(QLatin1String("3305-S")) ||  // SmartThings 2014 motion sensor
                    sensor.modelId() == QLatin1String("Remote switch") ||    // Legrand switch
                    sensor.modelId() == QLatin1String("Pocket remote") ||    // Legrand wireless switch scene x 4
                    sensor.modelId() == QLatin1String("Double gangs remote switch") ||    // Legrand switch double
                    sensor.modelId() == QLatin1String("Shutters central remote switch") || // Legrand switch module
                    sensor.modelId() == QLatin1String("Remote toggle switch") || // Legrand shutter switch
                    sensor.modelId() == QLatin1String("Remote motion sensor") || // Legrand motion sensor
                    sensor.modelId() == QLatin1String("lumi.sensor_magnet.agl02") || // Xiaomi Aqara T1 open/close sensor MCCGQ12LM
                    sensor.modelId() == QLatin1String("lumi.flood.agl02") ||         // Xiaomi Aqara T1 water leak sensor SJCGQ12LM
                    sensor.modelId() == QLatin1String("lumi.motion.agl04") ||        // Xiaomi Aqara RTCGQ13LM high precision motion sensor
                    sensor.modelId() == QLatin1String("Zen-01") ||           // Zen thermostat
                    sensor.modelId() == QLatin1String("Thermostat") ||       // eCozy thermostat
                    sensor.modelId() == QLatin1String("Bell") ||             // Sage doorbell sensor
                    sensor.modelId() == QLatin1String("ISW-ZPR1-WP13") ||    // Bosch motion sensor
                    sensor.modelId() == QLatin1String("3AFE14010402000D") ||   // Konke motion sensor
                    sensor.modelId() == QLatin1String("3AFE28010402000D") ||   // Konke motion sensor v2
                    sensor.modelId() == QLatin1String("FB56-DOS06HM1.3") ||    // Feibit FB56-DOS06HM1.3 door/window sensor
                    sensor.modelId().endsWith(QLatin1String("86opcn01")) ||    // Aqara Opple
                    sensor.modelId().startsWith(QLatin1String("FLSZB-1")) ||   // Develco water leak sensor
                    sensor.modelId().startsWith(QLatin1String("SIRZB-1")) ||   // Develco siren
                    sensor.modelId().startsWith(QLatin1String("ZHMS101")) ||   // Wattle (Develco) magnetic sensor
                    sensor.modelId().startsWith(QLatin1String("MotionSensor51AU")) || // Aurora (Develco) motion sensor
                    sensor.modelId().startsWith(QLatin1String("RFDL-ZB-MS")) ||// Bosch motion sensor
                    sensor.modelId().startsWith(QLatin1String("1116-S")) ||    // iris contact sensor v3
                    sensor.modelId().startsWith(QLatin1String("1117-S")) ||    // iris motion sensor v3
                    sensor.modelId().startsWith(QLatin1String("3326-L")) ||    // iris motion sensor v2
                    sensor.modelId().startsWith(QLatin1String("3300")) ||      // Centralite contact sensor
                    sensor.modelId().startsWith(QLatin1String("3320-L")) ||    // Centralite contact sensor
                    sensor.modelId().startsWith(QLatin1String("3323")) ||      // Centralite contact sensor
                    sensor.modelId().startsWith(QLatin1String("3315")) ||      // Centralite water sensor
                    sensor.modelId().startsWith(QLatin1String("3157100")) ||      // Centralite pearl thermostat
                    sensor.modelId().startsWith(QLatin1String("4655BC0")) ||      // Ecolink contact sensor
                    sensor.modelId().startsWith(QLatin1String("SZ-DWS04"))   || // Sercomm open/close sensor
                    sensor.modelId().startsWith(QLatin1String("SZ-WTD02N_CAR")) || // Sercomm water sensor
                    sensor.modelId().startsWith(QLatin1String("GZ-PIR02"))   || // Sercomm motion sensor
                    sensor.modelId() == QLatin1String("URC4450BC0-X-R")   || // Xfinity Keypad XHK1-UE
                    sensor.modelId() == QLatin1String("3405-L") ||           // IRIS 3405-L Keypad
                    sensor.modelId().startsWith(QLatin1String("Tripper")) || // Quirky Tripper (Sercomm) open/close
                    sensor.modelId().startsWith(QLatin1String("Lightify Switch Mini")) ||  // Osram 3 button remote
                    sensor.modelId().startsWith(QLatin1String("Switch 4x EU-LIGHTIFY")) || // Osram 4 button remote
                    sensor.modelId().startsWith(QLatin1String("Switch 4x-LIGHTIFY")) || // Osram 4 button remote
                    sensor.modelId().startsWith(QLatin1String("Switch-LIGHTIFY")) ) // Osram 4 button remote
                {  }
                else
                {
                    continue;
                }

                ResourceItem *item = sensor.item(RConfigBattery);

                if (!item && attr.numericValue().u8 > 0) // valid value: create resource item
                {
                    sensor.addItem(DataTypeUInt8, RConfigBattery);
                }

                // Correct incomplete sensor fingerprint
                if (!sensor.fingerPrint().hasInCluster(POWER_CONFIGURATION_CLUSTER_ID))
                {
                    sensor.fingerPrint().inClusters.push_back(POWER_CONFIGURATION_CLUSTER_ID);
                }

                quint8 battery = attr.numericValue().u8; // in 0.1 V

                float vmin = 20; // TODO: check - I've seen 24
                float vmax = 30; // TODO: check - I've seen 29

                if (sensor.modelId() == QLatin1String("Zen-01") ||
                    sensor.modelId() == QLatin1String("URC4450BC0-X-R")) // 4x LR6 AA 1.5 V
                {
                    vmin = 36; // according to attribute 0x0036
                    vmax = 60;
                }

                battery = calculateBatteryPercentageRemaining(&sensor, item, battery, vmin, vmax);
                
                if (item)
                {
                    item->setValue(battery);
                    enqueueEvent(Event(RSensors, RConfigBattery, sensor.id(), item));
                    updateSensorEtag(&sensor);
                    sensor.setNeedSaveDatabase(true);
                    queSaveDb(DB_SENSORS, DB_SHORT_SAVE_DELAY);
                }
                
                sensor.setZclValue(updateType, ind.srcEndpoint(), POWER_CONFIGURATION_CLUSTER_ID, POWER_CONFIG_ATTRID_BATTERY_VOLTAGE, attr.numericValue());
            }
                break;

            case POWER_CONFIG_ATTRID_BATTERY_ALARM_MASK:
            {
                ResourceItem *item = sensor.item(RStateLowBattery);

                if (!item)
                {
                    item = sensor.addItem(DataTypeBool, RStateLowBattery);
                }

                bool lowBat = (attr.numericValue().u8 & 0x01);

                sensor.setZclValue(updateType, ind.srcEndpoint(), POWER_CONFIGURATION_CLUSTER_ID, POWER_CONFIG_ATTRID_BATTERY_ALARM_MASK, attr.numericValue());

                if (item)
                {
                    item->setValue(lowBat);
                    enqueueEvent(Event(RSensors, RConfigBattery, sensor.id(), item));
                    updateSensorEtag(&sensor);
                    sensor.setNeedSaveDatabase(true);
                    queSaveDb(DB_SENSORS, DB_SHORT_SAVE_DELAY);

                    DDF_AnnoteZclParse(&sensor, item, ind.srcEndpoint(), ind.clusterId(), attrId, "Item.val = (Attr.val & 1) != 0");
                }
            }
                break;

            default:
                break;
            }
        }
    }
}
