#include "de_web_plugin.h"
#include "de_web_plugin_private.h"

/*! Handle packets related to the ZCL Fan control cluster.
    \param ind the APS level data indication containing the ZCL packet
    \param zclFrame the actual ZCL frame which holds the Thermostat cluster command or attribute
 */
void DeRestPluginPrivate::handleFanControlClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    Sensor *sensor = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), ind.srcEndpoint(), QLatin1String("ZHAThermostat"));

    if (!sensor)
    {
        DBG_Printf(DBG_INFO, "No thermostat sensor found for 0x%016llX, endpoint: 0x%02X\n", ind.srcAddress().ext(), ind.srcEndpoint());
        return;
    }

    // Currently only intended for thermostats. Might change later...
    if (sensor->type() != QLatin1String("ZHAThermostat"))
    {
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
            case 0x0000: // Fan mode
            {
                if (sensor->modelId() == QLatin1String("AC201") ||     // Owon
                    sensor->modelId() == QLatin1String("3157100") ||   // Centralite pearl
                    sensor->modelId() == QLatin1String("Zen-01"))      // Zen
                {
                    qint8 mode = attr.numericValue().u8;
                    QString modeSet;

                    modeSet = QLatin1String("off");
                    if ( mode == 0x00 ) { modeSet = QLatin1String("off"); }
                    if ( mode == 0x01 ) { modeSet = QLatin1String("low"); }
                    if ( mode == 0x02 ) { modeSet = QLatin1String("medium"); }
                    if ( mode == 0x03 ) { modeSet = QLatin1String("high"); }
                    if ( mode == 0x04 ) { modeSet = QLatin1String("on"); }
                    if ( mode == 0x05 ) { modeSet = QLatin1String("auto"); }
                    if ( mode == 0x06 ) { modeSet = QLatin1String("smart"); }

                    item = sensor->item(RConfigFanMode);
                    if (item && !item->toString().isEmpty() && item->toString() != modeSet)
                    {
                        item->setValue(modeSet);
                        enqueueEvent(Event(RSensors, RConfigFanMode, sensor->id(), item));
                        configUpdated = true;
                    }
                }
                sensor->setZclValue(updateType, ind.srcEndpoint(), FAN_CONTROL_CLUSTER_ID, attrId, attr.numericValue());
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

/*! Write Attribute on fan control cluster.
   \param task - the task item
   \param attrId
   \param attrType
   \param attrValue
   \return true - on success
           false - on error
 */
bool DeRestPluginPrivate::addTaskFanControlReadWriteAttribute(TaskItem &task, uint8_t readOrWriteCmd, uint16_t attrId, uint8_t attrType, uint32_t attrValue, uint16_t mfrCode)
{
    if (readOrWriteCmd != deCONZ::ZclReadAttributesId && readOrWriteCmd != deCONZ::ZclWriteAttributesId)
    {
        DBG_Printf(DBG_INFO, "Thermostat invalid parameter readOrWriteCmd %d\n", readOrWriteCmd);
        return false;
    }

    task.taskType = TaskThermostat;

    task.req.setClusterId(FAN_CONTROL_CLUSTER_ID);
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
