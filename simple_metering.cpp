#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "simple_metering.h"

const std::array<KeyValMapInt, 8> RConfigInterfaceModeValuesZHEMI = { { {1, PULSE_COUNTING_ELECTRICITY}, {2, PULSE_COUNTING_GAS}, {3, PULSE_COUNTING_WATER},
                                                                               {4, KAMSTRUP_KMP}, {5, LINKY}, {6, DLMS_COSEM}, {7, DSMR_23}, {8, DSMR_40} } };

const std::array<KeyValMapInt, 5> RConfigInterfaceModeValuesEMIZB = { { {1, NORWEGIAN_HAN}, {2, NORWEGIAN_HAN_EXTRA_LOAD}, {3, AIDON_METER},
                                                                               {4, KAIFA_KAMSTRUP_METERS}, {5, AUTO_DETECT} } };

/*! Handle packets related to the ZCL simple metering cluster.
    \param ind the APS level data indication containing the ZCL packet
    \param zclFrame the actual ZCL frame which holds the simple metering cluster command or attribute
 */
void DeRestPluginPrivate::handleSimpleMeteringClusterIndication(const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame)
{
    if (zclFrame.isDefaultResponse())
    {
        return;
    }
    
    Sensor *sensor = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), ind.srcEndpoint(), QLatin1String("ZHAConsumption"));

    if (!sensor)
    {
        DBG_Printf(DBG_INFO, "No consumption sensor found for 0x%016llX, endpoint: 0x%02X\n", ind.srcAddress().ext(), ind.srcEndpoint());
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
            case METERING_ATTRID_CURRENT_SUMMATION_DELIVERED:
            {
                quint64 consumption = attr.numericValue().u64;
                item = sensor->item(RStateConsumption);

                if (modelId == QLatin1String("SmartPlug") ||                      // Heiman
                    modelId.startsWith(QLatin1String("PSMP5_")) ||                // Climax
                    modelId.startsWith(QLatin1String("SKHMP30")) ||               // GS smart plug
                    modelId.startsWith(QLatin1String("E13-")) ||                  // Sengled PAR38 Bulbs
                    modelId.startsWith(QLatin1String("Z01-A19")) ||               // Sengled smart led
                    modelId == QLatin1String("Connected socket outlet"))          // Niko smart socket
                {
                    consumption = static_cast<quint64>(round((double)consumption / 10.0)); // 0.1 Wh -> Wh
                }
                else if (modelId == QLatin1String("SP 120") ||                    // innr
                         modelId == QLatin1String("Plug-230V-ZB3.0") ||           // Immax
                         modelId == QLatin1String("Smart plug Zigbee PE") ||      // Niko Smart Plug 552-80699
                         modelId == QLatin1String("TS0121"))                      // Tuya / Blitzwolf
                {
                    consumption *= 10; // 0.01 kWh = 10 Wh -> Wh
                }
                else if (modelId.startsWith(QLatin1String("SZ-ESW01"))) // Sercomm / Telstra smart plug
                {
                    consumption = static_cast<quint64>(round((double)consumption / 1000.0)); // -> Wh
                }
                else if (modelId.startsWith(QLatin1String("ROB_200")) ||            // ROBB Smarrt micro dimmer
                         modelId.startsWith(QLatin1String("Micro Smart Dimmer")) || // Sunricher Micro Smart Dimmer
                         modelId.startsWith(QLatin1String("SPW35Z")))               // RT-RK OBLO SPW35ZD0 smart plug
                {
                    consumption = static_cast<quint64>(round((double)consumption / 3600.0)); // -> Wh
                }

                if (item && item->toNumber() != static_cast<qint64>(consumption))
                {
                    item->setValue(consumption); // in Wh (0.001 kWh)
                    enqueueEvent(Event(RSensors, RStateConsumption, sensor->id(), item));
                }

                sensor->setZclValue(updateType, ind.srcEndpoint(), METERING_CLUSTER_ID, attrId, attr.numericValue());
                stateUpdated = true;
            }
                break;

            case METERING_ATTRID_PULSE_CONFIGURATION:
            {
                if (zclFrame.manufacturerCode() == VENDOR_DEVELCO && modelId == QLatin1String("ZHEMI101"))
                {
                    quint16 pulseConfiguration = attr.numericValue().u16;
                    item = sensor->item(RConfigPulseConfiguration);

                    if (item && item->toNumber() != pulseConfiguration)
                    {
                        item->setValue(pulseConfiguration);
                        enqueueEvent(Event(RSensors, RConfigPulseConfiguration, sensor->id(), item));
                        configUpdated = true;
                    }
                }
                sensor->setZclValue(updateType, ind.srcEndpoint(), METERING_CLUSTER_ID, attrId, attr.numericValue());
            }
                break;

            case METERING_ATTRID_INTERFACE_MODE:
            {
                if (zclFrame.manufacturerCode() == VENDOR_DEVELCO)
                {
                    const quint16 interfaceMode = attr.numericValue().u16;
                    item = sensor->item(RConfigInterfaceMode);
                    quint8 mode = 0;
                    
                    if(modelId == QLatin1String("ZHEMI101"))
                    {
                        if      (interfaceMode == PULSE_COUNTING_ELECTRICITY)   { mode = 1; }
                        else if (interfaceMode == PULSE_COUNTING_GAS)           { mode = 2; }
                        else if (interfaceMode == PULSE_COUNTING_WATER)         { mode = 3; }
                        else if (interfaceMode == KAMSTRUP_KMP)                 { mode = 4; }
                        else if (interfaceMode == LINKY)                        { mode = 5; }
                        else if (interfaceMode == DLMS_COSEM)                   { mode = 6; }
                        else if (interfaceMode == DSMR_23)                      { mode = 7; }
                        else if (interfaceMode == DSMR_40)                      { mode = 8; }
                    }
                    else if (modelId.startsWith(QLatin1String("EMIZB-1")))
                    {
                        if      (interfaceMode == NORWEGIAN_HAN)            { mode = 1; }
                        else if (interfaceMode == NORWEGIAN_HAN_EXTRA_LOAD) { mode = 2; }
                        else if (interfaceMode == AIDON_METER)              { mode = 3; }
                        else if (interfaceMode == KAIFA_KAMSTRUP_METERS)    { mode = 4; }
                        else if (interfaceMode == AUTO_DETECT)              { mode = 5; }                        
                    }
                    
                    if (item && mode != 0 && item->toNumber() != mode)
                    {
                        item->setValue(mode);
                        enqueueEvent(Event(RSensors, RConfigInterfaceMode, sensor->id(), item));
                        configUpdated = true;
                    }
                }
                sensor->setZclValue(updateType, ind.srcEndpoint(), METERING_CLUSTER_ID, attrId, attr.numericValue());
            }
                break;

            case METERING_ATTRID_INSTANTANEOUS_DEMAND:
            {
                qint32 power = attr.numericValue().s32;
                item = sensor->item(RStatePower);

                if (modelId == QLatin1String("SmartPlug") ||                  // Heiman
                    modelId == QLatin1String("902010/25") ||                  // Bitron
                    modelId.startsWith(QLatin1String("Z01-A19")) ||           // Sengled smart led
                    modelId.startsWith(QLatin1String("PSMP5_")) ||            // Climax
                    modelId.startsWith(QLatin1String("SKHMP30")) ||           // GS smart plug
                    modelId.startsWith(QLatin1String("160-01")))              // Plugwise smart plug
                {
                    power = static_cast<qint32>(round((double)power / 10.0)); // 0.1W -> W
                }
                else if (modelId.startsWith(QLatin1String("SZ-ESW01")))       // Sercomm / Telstra smart plug
                {
                    power = static_cast<qint32>(round((double)power / 1000.0)); // -> W
                }

                if (item && item->toNumber() != power)
                {
                    item->setValue(power); // in W
                    enqueueEvent(Event(RSensors, RStatePower, sensor->id(), item));
                }

                sensor->setZclValue(updateType, ind.srcEndpoint(), METERING_CLUSTER_ID, attrId, attr.numericValue());
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

/*! Write Attribute on thermostat cluster.
 *  Iterate over every day and get schedule for each day.
   \param task - the task item
   \param attrId
   \param attrType
   \param attrValue
   \return true - on success
           false - on error
 */
bool DeRestPluginPrivate::addTaskSimpleMeteringReadWriteAttribute(TaskItem &task, uint8_t readOrWriteCmd, uint16_t attrId, uint8_t attrType, uint32_t attrValue, uint16_t mfrCode)
{
    if (readOrWriteCmd != deCONZ::ZclReadAttributesId && readOrWriteCmd != deCONZ::ZclWriteAttributesId)
    {
        DBG_Printf(DBG_INFO, "Invalid command for simple metering cluster %d\n", readOrWriteCmd);
        return false;
    }

    task.taskType = TaskSimpleMetering;

    task.req.setClusterId(METERING_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(readOrWriteCmd);
    task.zclFrame.setFrameControl(deCONZ::ZclFCProfileCommand |
            deCONZ::ZclFCDirectionClientToServer |
            deCONZ::ZclFCDisableDefaultResponse);

    if (mfrCode != 0x0000)
    {
        task.zclFrame.setFrameControl(task.zclFrame.frameControl() | deCONZ::ZclFCManufacturerSpecific);
        task.zclFrame.setManufacturerCode(mfrCode);
    }

    // payload
    QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    if (readOrWriteCmd == deCONZ::ZclWriteAttributesId)
    {
        stream << attrId;
        stream << attrType;
        
        deCONZ::ZclAttribute attr(attrId, attrType, QLatin1String(""), deCONZ::ZclWrite, true);
        attr.setValue(QVariant(attrValue));

        if (!attr.writeToStream(stream))
        {
            return false;
        }
    }

    { // ZCL frame
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    return addTask(task);
}
