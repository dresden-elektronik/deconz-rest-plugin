#include "de_web_plugin.h"
#include "de_web_plugin_private.h"

#define ACTIVE_POWER                            0x050B
#define RMS_VOLTAGE                             0x0505
#define RMS_CURRENT                             0x0508
#define APPARENT_POWER                          0x050F

/*! Handle packets related to the ZCL electrical measurement cluster.
    \param ind the APS level data indication containing the ZCL packet
    \param zclFrame the actual ZCL frame which holds the electrical measurement cluster command or attribute
 */
void DeRestPluginPrivate::handleElectricalMeasurementClusterIndication(const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame)
{
    if (zclFrame.isDefaultResponse())
    {
        return;
    }

    Sensor *sensor = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), ind.srcEndpoint(), QLatin1String("ZHAPower"));

    if (!sensor)
    {
        DBG_Printf(DBG_INFO, "No power sensor found for 0x%016llX, endpoint: 0x%02X\n", ind.srcAddress().ext(), ind.srcEndpoint());
        return;
    }

    QDataStream stream(zclFrame.payload());
    stream.setByteOrder(QDataStream::LittleEndian);

    bool isReadAttr = false;
    bool isReporting = false;
    if (zclFrame.isProfileWideCommand() && zclFrame.commandId() == deCONZ::ZclReadAttributesResponseId)
    {
        isReadAttr = true;
    }
    if (zclFrame.isProfileWideCommand() && zclFrame.commandId() == deCONZ::ZclReportAttributesId)
    {
        isReporting = true;
    }

    // Read ZCL reporting and ZCL Read Attributes Response
    if (isReadAttr || isReporting)
    {
        const auto modelId = sensor->modelId();
        const NodeValue::UpdateType updateType = isReadAttr ? NodeValue::UpdateByZclRead : NodeValue::UpdateByZclReport;

        bool configUpdated = false;
        bool stateUpdated = false;

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

            ResourceItem *item = nullptr;

            switch (attrId)
            {
            case ACTIVE_POWER:
            {
                qint16 power = attr.numericValue().s16;
                item = sensor->item(RStatePower);

                if (item && power != -32768)
                {
                    if (modelId == QLatin1String("SmartPlug") ||                              // Heiman
                        modelId.startsWith(QLatin1String("SKHMP30")) ||                       // GS smart plug
                        modelId.startsWith(QLatin1String("ROB_200")) ||                       // ROBB Smarrt micro dimmer
                        modelId.startsWith(QLatin1String("Micro Smart Dimmer")) ||            // Sunricher Micro Smart Dimmer
                        modelId.startsWith(QLatin1String("lumi.plug.maeu")) ||                // Xiaomi Aqara ZB3.0 smart plug
                        modelId == QLatin1String("RICI01") ||                                 // LifeControl Smart Plug
                        modelId.startsWith(QLatin1String("outlet")) ||                        // Samsung SmartThings IM6001-OTP/IM6001-OTP01
                        modelId == QLatin1String("3200-Sgb") ||                               // Samsung/Centralite smart outlet
                        modelId == QLatin1String("3200-de") ||                                // Samsung/Centralite smart outlet
                        modelId.startsWith(QLatin1String("lumi.switch.n0agl1")) ||            // Xiaomi Aqara Single Switch Module T1 (With Neutral)
                        modelId.startsWith(QLatin1String("lumi.switch.b1naus01")))            // Xiaomi ZB3.0 Smart Wall Switch
                    {
                        power = static_cast<qint16>(round((double)power / 10.0)); // 0.1W -> W
                    }
                    else if (modelId.startsWith(QLatin1String("Plug")) && sensor->manufacturer() == QLatin1String("OSRAM")) // OSRAM
                    {
                        power = power == 28000 ? 0 : power / 10;
                    }
                    else if (modelId.startsWith(QLatin1String("SZ-ESW01")))                   // Sercomm / Telstra smart plug
                    {
                        power = static_cast<qint16>(round(((double)power * 128) / 1000.0));
                    }
                    else if (modelId == QLatin1String("Connected socket outlet"))             // Niko smart socket
                    {
                        power = static_cast<qint16>(round(((double)power * 1123) / 10000.0));
                    }
                    else if (modelId.startsWith(QLatin1String("lumi.relay.c2acn")))           // Xiaomi relay
                    {
                        continue;   // Device seems to always report -1 via this cluster/attribute
                    }

                    if (item->toNumber() != power)
                    {
                        item->setValue(power); // in W
                        enqueueEvent(Event(RSensors, RStatePower, sensor->id(), item));
                    }
                }

                sensor->setZclValue(updateType, ind.srcEndpoint(), ELECTRICAL_MEASUREMENT_CLUSTER_ID, attrId, attr.numericValue());
                stateUpdated = true;
            }
                break;

            case RMS_VOLTAGE:
            {
                quint16 voltage = attr.numericValue().u16;
                item = sensor->item(RStateVoltage);

                if (item && voltage != 65535)
                {
                    if (modelId == QLatin1String("SmartPlug") ||                                       // Heiman
                        modelId.startsWith(QLatin1String("SPLZB-1")) ||                                // Develco smart plug
                        modelId.startsWith(QLatin1String("SMRZB-3")) ||                                // Develco smart relay
                        modelId.startsWith(QLatin1String("SMRZB-1")) ||                                // Develco smart cable
                        modelId.startsWith(QLatin1String("SKHMP30")) ||                                // GS smart plug
                        modelId == QLatin1String("Smart16ARelay51AU") ||                               // Aurora (Develco) smart plug
                        modelId == QLatin1String("PoP"))                                               // Apex Smart Plug
                    {
                        voltage = static_cast<quint16>(round((double)voltage / 100.0)); // 0.01V -> V
                    }
                    else if (modelId == QLatin1String("RICI01") ||                                     // LifeControl Smart Plug
                             modelId.startsWith(QLatin1String("outlet")) ||                            // Samsung SmartThings IM6001-OTP/IM6001-OTP01
                             modelId.startsWith(QLatin1String("ROB_200")) ||                           // ROBB Smarrt micro dimmer
                             modelId.startsWith(QLatin1String("Micro Smart Dimmer")) ||                // Sunricher Micro Smart Dimmer
                             modelId == QLatin1String("Connected socket outlet") ||                    // Niko smart socket
                             modelId.startsWith(QLatin1String("TH112")))                               // Sinope Thermostats
                    {
                        voltage = static_cast<quint16>(round((double)voltage / 10.0)); // 0.1V -> V
                    }
                    else if (modelId.startsWith(QLatin1String("SZ-ESW01")))                            // Sercomm / Telstra smart plug
                    {
                        voltage = static_cast<quint16>(round((double)voltage / 125.0)); // -> V
                    }

                    if (item->toNumber() != voltage)
                    {
                        item->setValue(voltage); // in V
                        enqueueEvent(Event(RSensors, RStateVoltage, sensor->id(), item));
                    }
                }

                sensor->setZclValue(updateType, ind.srcEndpoint(), ELECTRICAL_MEASUREMENT_CLUSTER_ID, attrId, attr.numericValue());
                stateUpdated = true;
            }
                break;

            case RMS_CURRENT:
            {
                quint16 current = attr.numericValue().u16;
                item = sensor->item(RStateCurrent);

                if (item && current != 65535)
                {
                    if (modelId == QLatin1String("SP 120") ||                                     // innr
                        modelId.startsWith(QLatin1String("outlet")) ||                            // Samsung SmartThings IM6001-OTP/IM6001-OTP01
                        modelId == QLatin1String("DoubleSocket50AU") ||                           // Aurora
                        modelId.startsWith(QLatin1String("SPLZB-1")) ||                           // Develco smart plug
                        modelId == QLatin1String("Smart16ARelay51AU") ||                          // Aurora (Develco) smart plug
                        modelId == QLatin1String("RICI01") ||                                     // LifeControl Smart Plug
                        modelId.startsWith(QLatin1String("SZ-ESW01")) ||                          // Sercomm / Telstra smart plug
                        modelId == QLatin1String("TS0121") ||                                     // Tuya smart plug
                        modelId.startsWith(QLatin1String("ROB_200")) ||                           // ROBB Smarrt micro dimmer
                        modelId.startsWith(QLatin1String("Micro Smart Dimmer")) ||                // Sunricher Micro Smart Dimmer
                        modelId == QLatin1String("Connected socket outlet") ||                    // Niko smart socket
                        modelId == QLatin1String("SMRZB-1") ||                                    // Develco smart cable
                        modelId.startsWith(QLatin1String("S1")) ||                                // Ubisys S1/S1-R
                        modelId.startsWith(QLatin1String("S2")) ||                                // Ubisys S2/S2-R
                        modelId.startsWith(QLatin1String("J1")) ||                                // Ubisys J1/J1-R
                        modelId.startsWith(QLatin1String("D1")))                                  // Ubisys D1/D1-R
                    {
                        // already in mA
                    }
                    else if (modelId == QLatin1String("SmartPlug") ||                             // Heiman
                             modelId.startsWith(QLatin1String("EMIZB-1")) ||                      // Develco EMI
                             modelId.startsWith(QLatin1String("SKHMP30")) ||                      // GS smart plug
                             modelId == QLatin1String("3200-Sgb") ||                              // Samsung smart outlet
                             modelId == QLatin1String("3200-de") ||                               // Samsung smart outlet
                             modelId.startsWith(QLatin1String("SPW35Z")) ||                       // RT-RK OBLO SPW35ZD0 smart plug
                             modelId == QLatin1String("TH1300ZB"))                                // Sinope thermostat
                    {
                        current *= 10; // 0.01A -> mA
                    }
                    else
                    {
                        current *= 1000; // A -> mA
                    }

                    if (item->toNumber() != current)
                    {
                        item->setValue(current); // in mA
                        enqueueEvent(Event(RSensors, RStateCurrent, sensor->id(), item));
                    }
                }

                sensor->setZclValue(updateType, ind.srcEndpoint(), ELECTRICAL_MEASUREMENT_CLUSTER_ID, attrId, attr.numericValue());
                stateUpdated = true;
            }
                break;

            case APPARENT_POWER:
            {
                quint16 power = attr.numericValue().u16;
                item = sensor->item(RStatePower);

                if (item && power != 65535)
                {
                    if (modelId == QLatin1String("TH1300ZB")) // Sinope thermostat
                    {
                        power = static_cast<quint16>(round((double)power / 1000.0)); // -> W
                    }

                    if (item->toNumber() != power)
                    {
                        item->setValue(power); // in W
                        enqueueEvent(Event(RSensors, RStatePower, sensor->id(), item));
                    }
                }

                sensor->setZclValue(updateType, ind.srcEndpoint(), ELECTRICAL_MEASUREMENT_CLUSTER_ID, attrId, attr.numericValue());
                stateUpdated = true;
            }
                break;

            default:
                break;
            }
        }

        if (stateUpdated)
        {
            sensor->updateStateTimestamp();
            enqueueEvent(Event(RSensors, RStateLastUpdated, sensor->id()));
        }

        if (configUpdated || stateUpdated)
        {
            updateSensorEtag(&*sensor);
            sensor->setNeedSaveDatabase(true);
            queSaveDb(DB_SENSORS, DB_SHORT_SAVE_DELAY);
        }
    }
}
