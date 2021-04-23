#include "de_web_plugin.h"
#include "de_web_plugin_private.h"

/*! Handle packets related to the ZCL simple metering cluster.
    \param ind the APS level data indication containing the ZCL packet
    \param zclFrame the actual ZCL frame which holds the Thermostat cluster command or attribute
 */
void DeRestPluginPrivate::handleSimpleMeteringClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
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
            case 0x0300: // Pulse Configuration
            {
                if (zclFrame.manufacturerCode() == VENDOR_DEVELCO && sensor->modelId() == QLatin1String("ZHEMI101"))
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

            case 0x0302: // Interface Mode
            {
                if (zclFrame.manufacturerCode() == VENDOR_DEVELCO)
                {
                    const quint16 interfaceMode = attr.numericValue().u16;
                    quint8 mode = 0;
                    
                    if(sensor->modelId() == QLatin1String("ZHEMI101"))
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
                    else if (sensor->modelId().startsWith(QLatin1String("EMIZB-1")))
                    {
                        if      (interfaceMode == NORWEGIAN_HAN)            { mode = 1; }
                        else if (interfaceMode == NORWEGIAN_HAN_EXTRA_LOAD) { mode = 2; }
                        else if (interfaceMode == AIDON_METER)              { mode = 3; }
                        else if (interfaceMode == KAIFA_KAMSTRUP_METERS)    { mode = 4; }
                        else if (interfaceMode == AUTO_DETECT)              { mode = 5; }                        
                    }
                    
                    item = sensor->item(RConfigInterfaceMode);
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
