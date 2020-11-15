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

            ResourceItem *item = nullptr;

            switch (attrId)
            {
            case 0x0000: // Measured value
            {
                if (sensor->modelId() == QLatin1String("AQSZB-110"))    // Develco air quality sensor
                {
                    quint16 level = attr.numericValue().u16;
                    QString airquality = QString("none");

                    if ( level > 0 && level <= 65  ) { airquality = QString("excellent"); }
                    if ( level > 65 && level <= 220 ) { airquality = QString("good"); }
                    if ( level > 220 && level <= 660 ) { airquality = QString("moderate"); }
                    if ( level > 660 && level <= 2200 ) { airquality = QString("poor"); }
                    if ( level > 2200 && level <= 5500 ) { airquality = QString("unhealthy"); }
                    if ( level > 5500 ) { airquality = QString("out of scale"); }

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
                    sensor->setZclValue(updateType, ind.srcEndpoint(), 0xFC03, attrId, attr.numericValue());
                }
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