#include "de_web_plugin.h"
#include "de_web_plugin_private.h"

/*! Handle packets related to the ZCL Fan control cluster.
    \param ind the APS level data indication containing the ZCL packet
    \param zclFrame the actual ZCL frame which holds the Thermostat cluster command or attribute
 */
void DeRestPluginPrivate::handleFanControlClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    Sensor *sensor = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), ind.srcEndpoint());

    if (!sensor)
    {
        DBG_Printf(DBG_INFO, "No sensor found for 0x%016llX, endpoint: 0x%08X\n", ind.srcAddress().ext(), ind.srcEndpoint());
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
                if (sensor->modelId() == QLatin1String("AC201"))    // Owon
                {
                    qint8 mode = attr.numericValue().u8;
                    QString mode_set;

                    mode_set = QString("off");
                    if ( mode == 0x00 ) { mode_set = QString("off"); }
                    if ( mode == 0x01 ) { mode_set = QString("low"); }
                    if ( mode == 0x02 ) { mode_set = QString("medium"); }
                    if ( mode == 0x03 ) { mode_set = QString("high"); }
                    if ( mode == 0x04 ) { mode_set = QString("on"); }
                    if ( mode == 0x05 ) { mode_set = QString("auto"); }
                    if ( mode == 0x06 ) { mode_set = QString("smart"); }

                    item = sensor->item(RConfigFanMode);
                    if (item && !item->toString().isEmpty() && item->toString() != mode_set)
                    {
                        item->setValue(mode_set);
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
            updateEtag(sensor->etag);
            updateEtag(gwConfigEtag);
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

    stream << (quint16) attrId;

    if (readOrWriteCmd == deCONZ::ZclWriteAttributesId)
    {
        stream << (quint8) attrType;
        if (attrType == deCONZ::Zcl8BitEnum || attrType == deCONZ::Zcl8BitInt || attrType == deCONZ::Zcl8BitBitMap)
        {
            stream << (quint8) attrValue;
        }
        else if (attrType == deCONZ::Zcl16BitInt || attrType == deCONZ::Zcl16BitBitMap)
        {
            stream << (quint16) attrValue;
        }
        else if (attrType == deCONZ::Zcl24BitUint)
        {
            stream << (qint8) (attrValue & 0xFF);
            stream << (qint8) ((attrValue >> 8) & 0xFF);
            stream << (qint8) ((attrValue >> 16) & 0xFF);
        }
        else
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
