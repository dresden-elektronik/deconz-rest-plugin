#include "de_web_plugin.h"
#include "de_web_plugin_private.h"

/*! Handle packets related to manufacturer specific clusters for air quality (VOC).
    \param ind the APS level data indication containing the ZCL packet
    \param zclFrame the actual ZCL frame which holds the Thermostat cluster command or attribute
 */
void DeRestPluginPrivate::handleAirQualityClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    Sensor *sensor = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), ind.srcEndpoint(), QLatin1String("ZHAAirQuality"));
    if (!sensor)
    {
        DBG_Printf(DBG_INFO, "No air quality sensor found for 0x%016llX, endpoint: 0x%08X\n", ind.srcAddress().ext(), ind.srcEndpoint());
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

            quint32 levelPpb = UINT32_MAX; // invalid value

            switch (attrId)
            {
            case 0x0000: // Measured value
            {
                if (ind.clusterId() == 0xFC03 && sensor->modelId() == QLatin1String("AQSZB-110"))    // Develco air quality sensor
                {
                    levelPpb = attr.numericValue().u16;
                }
            }
                break;

            case 0x4004:
            {
                // Bosch air quality sensor
                if (ind.clusterId() == BOSCH_AIR_QUALITY_CLUSTER_ID && sensor->manufacturer() == QLatin1String("BOSCH") && sensor->modelId() == QLatin1String("AIR"))
                {
                    levelPpb = attr.numericValue().u16;
                }
            }
                break;

            default:
                break;
            }

            if (levelPpb != UINT32_MAX)
            {
                QString airquality = QLatin1String("none");

                if (levelPpb <= 65)                      { airquality = QLatin1String("excellent"); }
                if (levelPpb > 65 && levelPpb <= 220)    { airquality = QLatin1String("good"); }
                if (levelPpb > 220 && levelPpb <= 660)   { airquality = QLatin1String("moderate"); }
                if (levelPpb > 660 && levelPpb <= 2200)  { airquality = QLatin1String("poor"); }
                if (levelPpb > 2200 && levelPpb <= 5500) { airquality = QLatin1String("unhealthy"); }
                if (levelPpb > 5500 )                    { airquality = QLatin1String("out of scale"); }

                ResourceItem *item = sensor->item(RStateAirQuality);
                if (item)
                {
                    if (updateType == NodeValue::UpdateByZclReport)
                    {
                        stateUpdated = true;
                    }
                    if (item->toString() != airquality)
                    {
                        item->setValue(airquality);
                        enqueueEvent(Event(RSensors, RStateAirQuality, sensor->id(), item));
                        stateUpdated = true;
                    }
                }

                item = sensor->item(RStateAirQualityPpb);
                if (item)
                {
                    if (updateType == NodeValue::UpdateByZclReport)
                    {
                        stateUpdated = true;
                    }
                    if (item->toNumber() != levelPpb)
                    {
                        item->setValue(levelPpb);
                        enqueueEvent(Event(RSensors, RStateAirQualityPpb, sensor->id(), item));
                        stateUpdated = true;
                    }
                }

                sensor->setZclValue(updateType, ind.srcEndpoint(), ind.clusterId(), attrId, attr.numericValue());
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
