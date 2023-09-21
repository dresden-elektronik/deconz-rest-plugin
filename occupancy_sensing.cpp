#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "device_descriptions.h"

#define OCCUPIED_STATE                  0x0000
#define OCCUPIED_TO_UNOCCUPIED_DELAY    0x0010

/*! Handle packets related to the ZCL occupancy sensing cluster.
    \param ind the APS level data indication containing the ZCL packet
    \param zclFrame the actual ZCL frame which holds the occupancy sensing cluster command or attribute
 */
void DeRestPluginPrivate::handleOccupancySensingClusterIndication(const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame)
{
    if (zclFrame.isDefaultResponse())
    {
        return;
    }

    Sensor *sensor = getSensorNodeForAddressEndpointAndCluster(ind.srcAddress(), ind.srcEndpoint(), OCCUPANCY_SENSING_CLUSTER_ID );

    if (!sensor)
    {
        DBG_Printf(DBG_INFO, "No presence sensor found for 0x%016llX, endpoint: 0x%02X\n", ind.srcAddress().ext(), ind.srcEndpoint());
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
        case OCCUPIED_STATE:
        {
            quint8 occupancy = attr.numericValue().u8;
            item = sensor->item(RStatePresence);

            if (item)
            {
                item->setValue(occupancy);
                enqueueEvent(Event(RSensors, RStatePresence, sensor->id(), item));
                stateUpdated = true;

                DDF_AnnoteZclParse(sensor, item, ind.srcEndpoint(), ind.clusterId(), attrId, "Item.val = Attr.val != 0");

                // The checked sensors support reporting occupancy = false
                if (!sensor->modelId().startsWith(QLatin1String("MOSZB-1")) && !sensor->modelId().startsWith(QLatin1String("SML00")))
                {
                    const NodeValue &val = sensor->getZclValue(OCCUPANCY_SENSING_CLUSTER_ID, OCCUPIED_STATE);

                    // prepare to automatically set presence to false
                    if (item->toBool())
                    {
                        if (val.maxInterval > 0 && updateType == NodeValue::UpdateByZclReport)
                        {
                            // prevent setting presence back to false, when report.maxInterval > config.duration
                            // Add 3 seconds grace time for late reports
                            sensor->durationDue = item->lastSet().addSecs(val.maxInterval + 3);
                        }
                        else
                        {
                            ResourceItem *item2 = sensor->item(RConfigDuration);
                            if (item2 && item2->toNumber() > 0)
                            {
                                // If occupied state is not reportable, add duration seconds after a occupied = true to automatically set to false
                                sensor->durationDue = item->lastSet().addSecs(item2->toNumber());
                            }
                        }
                    }
                }
            }

            sensor->setZclValue(updateType, ind.srcEndpoint(), OCCUPANCY_SENSING_CLUSTER_ID, OCCUPIED_STATE, attr.numericValue());
        }
            break;

        case OCCUPIED_TO_UNOCCUPIED_DELAY:
        {
            if (sensor->modelId() == QLatin1String("LG IP65 HMS"))
            {
                // TODO(mpi): this can be removed, I don't think there are any users of this device (large industrial light+sensor)
                quint16 duration = attr.numericValue().u16;
                item = sensor->item(RConfigDuration);

                if (!item) { item = sensor->addItem(DataTypeUInt16, RConfigDuration); }

                if (item && item->toNumber() != duration)
                {
                    enqueueEvent(Event(RSensors, RConfigDuration, sensor->id(), item));

                    if (item->toNumber() <= 0)
                    {
                        DBG_Printf(DBG_INFO, "got occupied to unoccupied delay %u\n", duration);
                        item->setValue(duration);
                        configUpdated = true;
                    }
                    else
                    {
                        DBG_Printf(DBG_INFO, "occupied to unoccupied delay is %u should be %u, force rewrite\n", duration, (quint16)item->toNumber());
                        if (!sensor->mustRead(WRITE_OCCUPANCY_CONFIG))
                        {
                            sensor->enableRead(WRITE_OCCUPANCY_CONFIG);
                            sensor->setNextReadTime(WRITE_OCCUPANCY_CONFIG, queryTime);
                            queryTime = queryTime.addSecs(1);
                        }

                        if (!sensor->mustRead(READ_OCCUPANCY_CONFIG))
                        {
                            sensor->enableRead(READ_OCCUPANCY_CONFIG);
                            sensor->setNextReadTime(READ_OCCUPANCY_CONFIG, queryTime);
                            queryTime = queryTime.addSecs(5);
                        }

                        Q_Q(DeRestPlugin);
                        q->startZclAttributeTimer(750);
                    }
                }
            }
            else
            {
                quint16 delay = attr.numericValue().u16;
                item = sensor->item(RConfigDelay);

                if (item && item->toNumber() != delay)
                {
                    item->setValue(delay);
                    enqueueEvent(Event(RSensors, RConfigDelay, sensor->id(), item));
                    configUpdated = true;
                }

                if (sensor->mustRead(WRITE_DELAY))
                {
                    ResourceItem *item = sensor->item(RConfigPending);
                    if (item)
                    {
                        quint16 mask = item->toNumber();
                        mask &= ~R_PENDING_DELAY;
                        item->setValue(mask);
                        enqueueEvent(Event(RSensors, RConfigPending, sensor->id(), item));
                    }
                    sensor->clearRead(WRITE_DELAY);
                }
            }

            sensor->setZclValue(updateType, ind.srcEndpoint(), OCCUPANCY_SENSING_CLUSTER_ID, OCCUPIED_TO_UNOCCUPIED_DELAY, attr.numericValue());
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
