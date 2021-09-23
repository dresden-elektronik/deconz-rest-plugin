#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "mfspecific_cluster_xiaoyan.h"

/*! Handle packets related to the Xiaoyan FCCC cluster.
    \param ind the APS level data indication containing the ZCL packet
    \param zclFrame the actual ZCL frame which holds the Xiaoyan FCCC cluster command or attribute
 */
void DeRestPluginPrivate::handleXiaoyanClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    if (zclFrame.isDefaultResponse())
    {
        return;
    }

    Sensor *sensor = getSensorNodeForAddressEndpointAndCluster(ind.srcAddress(), ind.srcEndpoint(), XIAOYAN_CLUSTER_ID );

    if (!sensor)
    {
        DBG_Printf(DBG_INFO, "No matching sensor found for 0x%016llX, endpoint: 0x%02X\n", ind.srcAddress().ext(), ind.srcEndpoint());
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
            case XIAOYAN_ATTRID_DURATION:
            {
                quint16 duration = attr.numericValue().u16; // TERNCY-SD01
                item = sensor->item(RStateEventDuration);

                if (item)
                {
                    item->setValue(duration);
                    enqueueEvent(Event(RSensors, RStateEventDuration, sensor->id(), item));
                    stateUpdated = true;
                }

                sensor->setZclValue(updateType, ind.srcEndpoint(), XIAOYAN_CLUSTER_ID, XIAOYAN_ATTRID_DURATION, attr.numericValue());
            }
                break;

            case XIAOYAN_ATTRID_ROTATION_ANGLE:
            {
                qint16 angle = attr.numericValue().s16; // TERNCY-SD01
                item = sensor->item(RStateAngle);

                if (item)
                {
                    item->setValue(angle);
                    enqueueEvent(Event(RSensors, RStateAngle, sensor->id(), item));
                    stateUpdated = true;
                }

                sensor->setZclValue(updateType, ind.srcEndpoint(), XIAOYAN_CLUSTER_ID, XIAOYAN_ATTRID_ROTATION_ANGLE, attr.numericValue());
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
