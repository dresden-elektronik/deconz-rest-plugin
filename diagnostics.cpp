#include "de_web_plugin.h"
#include "de_web_plugin_private.h"

/*! Handle packets related to the ZCL diagnostics cluster.
    \param ind the APS level data indication containing the ZCL packet
    \param zclFrame the actual ZCL frame which holds the Thermostat cluster command or attribute
 */
void DeRestPluginPrivate::handleDiagnosticsClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    Sensor *sensor = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), ind.srcEndpoint(), QLatin1String("ZHAThermostat"));

    if (!sensor)
    {
        DBG_Printf(DBG_INFO, "No sensor found for 0x%016llX, endpoint: 0x%02X\n", ind.srcAddress().ext(), ind.srcEndpoint());
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
            case 0x4000: // SW error code
            {
                if (sensor->modelId() == QLatin1String("eTRV0100") || sensor->modelId() == QLatin1String("TRV001") || sensor->modelId() == QLatin1String("eT093WRO"))
                {
                    quint16 value = attr.numericValue().u16;
                    QString errorCode = QString("%1").arg(value, 4, 16, QLatin1Char('0')).toUpper();

                    if (errorCode == QLatin1String("0A00")) { errorCode = "none"; } // Assuming "0A00" means no error
                    item = sensor->item(RStateErrorCode);

                    if (item && updateType == NodeValue::UpdateByZclReport)
                    {
                        stateUpdated = true;
                    }
                    if (item && item->toString() != errorCode)
                    {
                        item->setValue(errorCode);
                        enqueueEvent(Event(RSensors, RStateErrorCode, sensor->id(), item));
                        stateUpdated = true;
                    }
                }
                sensor->setZclValue(updateType, ind.srcEndpoint(), DIAGNOSTICS_CLUSTER_ID, attrId, attr.numericValue());
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
