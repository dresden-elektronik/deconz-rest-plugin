#include <QDataStream>
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "air_quality.h"

const std::array<KeyValMapAirQuality, 6> RStateAirQualityVocLevelGer = { { {65, QLatin1String("excellent")}, {220, QLatin1String("good")}, {660, QLatin1String("moderate")}, {2200, QLatin1String("poor")},
                                                                            {5000, QLatin1String("unhealthy")}, {65535, QLatin1String("out of scale")} } };

/*! Handle packets related to manufacturer specific clusters for air quality.
    \param ind the APS level data indication containing the ZCL packet
    \param zclFrame the actual ZCL frame which holds the Thermostat cluster command or attribute
 */
void DeRestPluginPrivate::handleAirQualityClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    Sensor *sensor = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), ind.srcEndpoint(), QLatin1String("ZHAAirQuality"));
    if (!sensor)
    {
        DBG_Printf(DBG_INFO, "No air quality sensor found for 0x%016llX, endpoint: 0x%02X\n", ind.srcAddress().ext(), ind.srcEndpoint());
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

            quint32 level = UINT32_MAX; // invalid value
            QString airquality;

            switch (attrId)
            {
            case 0x4004:
            {
                // Bosch air quality sensor
                if (ind.clusterId() == BOSCH_AIR_QUALITY_CLUSTER_ID && sensor->modelId() == QLatin1String("AIR"))
                {
                    level = attr.numericValue().u16;
                    const auto match = lessThenKeyValue(level, RStateAirQualityVocLevelGer);

                    if (match.key)
                    {
                        airquality = match.value;
                    }
                }
            }
                break;

            default:
                break;
            }

            if (level != UINT32_MAX)
            {
                ResourceItem *item = sensor->item(RStateAirQualityPpb);
                if (item)
                {
                    if (updateType == NodeValue::UpdateByZclReport)
                    {
                        stateUpdated = true;
                    }
                    if (item->toNumber() != level)
                    {
                        item->setValue(level);
                        enqueueEvent(Event(RSensors, RStateAirQualityPpb, sensor->id(), item));
                        stateUpdated = true;
                    }
                }

                item = sensor->item(RStateAirQuality);
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
            updateSensorEtag(&*sensor);
            sensor->setNeedSaveDatabase(true);
            queSaveDb(DB_SENSORS, DB_SHORT_SAVE_DELAY);
        }
    }
}
