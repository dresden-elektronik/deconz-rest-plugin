/*
 * thermostat.cpp
 *
 * Add Support for Thermostat Cluster 0x0201 and Bitron Thermostat 902010/32
 *
 * A sensor type ZHAThermostat is created with the following options in state and config:
 *
 * option             | read/write | attribute | description
 * -------------------|------------|-----------|---------------------
 * state.on           | read only  | 0x0029    | running state on/off
 * state.temperature  | read only  | 0x0000    | measured temperature
 * config.heatsetpoint| read write | 0x0012    | heating setpoint
 * config.mode        | read write | 0x001C    | System mode
 * config.scheduleron | read write | 0x0025    | scheduler on/off
 * config.offset      | read write | 0x0010    | temperature offset
 * config.scheduler   | read write | (command) | scheduled setpoints
 *
 *
 * Example sensor:
 *
 * /api/<apikey>/sensors/<id>/
 *    {
 *       config: {
 *          "heatsetpoint": 2200,
 *          "offset": 0,
 *          "scheduler": "Monday,Tuesday,Wednesday,Thursday,Friday 05:00 2200 19:00 1800;Saturday,Sunday 06:00 2100 19:00 1800;"
 *          "scheduleron": true
 *       },
 *       state: {
 *          "on": true,
 *          "temperature": 2150
 *        },
 *       "ep": 1,
 *       "manufacturername": "Bitron Home",
 *       "modelid": "902010/32",
 *        "type": "ZHAThermostat",
 *       ...
 *    }
 *
 * Rest API example commands:
 * -X PUT /api/<apikey>/sensors/<id>/config -d '{ "heatsetpoint": 1800 }'
 * -X PUT /api/<apikey>/sensors/<id>/config -d '{ "scheduleron": true }'
 * -X PUT /api/<apikey>/sensors/<id>/config -d '{ "offset": 0 }'
 * -X PUT /api/<apikey>/sensors/<id>/config -d '{ "scheduler": "Monday 05:00 2200 19:00 1800;" }'
 *                                          -d '{ "scheduler": "" }'  (send get scheduler command)
 *
 *
 * Attributes (only a subset):
 *   ID      Type  Type    Description                     Default
 *   0x0000  0x29  int16s  Local Temperature               --
 *   0x0010  0x28  int8s   Local Temperature Calibration   --
 *   0x0012  0x29  int16s  Occupied Heating Setpoint       2000 (20 °C)
 *   0x0025  0x18  bit8    Programming Operation Mode      0
 *                         Bit#0 of this attribute controls
 *                         the enabling of the Thermostat Scheduler.
 *   0x0029  0x19  bit16   Thermostat Running State        0
 *                         Bit0=Heat State On
 *                         Bit1=Cool State On
 *
 *   Commands Received (Client to Server):
 *   Command-ID  Name
 *   0x00        Setpoint Raise/Lower
 *   0x01        Set Weekly Schedule
 *   0x02        Get Weekly Schedule
 *   0x03        Clear Weekly Schedule
 *
 *   Commands Generated (Server to Client):
 *   Command-ID  Name
 *   0x00        Current Weekly Schedule
 *
 *   Weekly Schedule format
 *   Octets 1           1       1     2           2/0         2/0       ...   2   2/0   2/0
 *   Type   enum8       bit8    bit8  int16u      int16s      int16s
 *   Name   Number      Day of  Mode  Transition  Heat        Cool
 *          Transitions Week    (1,2) Time 1      Setpoint 1  Setpoint1
 *
 *   Day of Week: bitmap8 [Sunday, Monday, Tuesday, Wednesday, Thursday, Friday, Saturday, Away]
 *                bit         0       1       2         3        4         5        6       7
 *   Example:
 *   Monday, Tuesday, Thursday, Friday = 0011 0110 = 0x36
 *        |        |         |     ^-------^|  ||
 *        |        |          ---------------  ||
 *        |        -----------------------------|
 *        ---------------------------------------
 *
 */

#include <bitset>
#include <QJsonDocument>
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"

/*! Covert Zigbee weekdays bitmap to ISO or v.v.
 */
static quint8 convertWeekdayBitmap(const quint8 weekdayBitmap)
{
    quint8 result = 0;
    if (weekdayBitmap & 0b00000001) { result |= 0b00000001; }
    if (weekdayBitmap & 0b00000010) { result |= 0b01000000; }
    if (weekdayBitmap & 0b00000100) { result |= 0b00100000; }
    if (weekdayBitmap & 0b00001000) { result |= 0b00010000; }
    if (weekdayBitmap & 0b00010000) { result |= 0b00001000; }
    if (weekdayBitmap & 0b00100000) { result |= 0b00000100; }
    if (weekdayBitmap & 0b01000000) { result |= 0b00000010; }
    return result;
}

/*! Serialise a list of transitions.
 * \param transitions the list of transitions
 * \param s the serialised transitions
 */
bool DeRestPluginPrivate::serialiseThermostatTransitions(const QVariantList &transitions, QString *s)
{
    *s = "";
    if (transitions.size() < 1 || transitions.size() > 10)
    {
        return false;
    }
    for (const QVariant &entry : transitions)
    {
        QVariantMap transition = entry.toMap();
        for (const QString &key : transition.keys())
        {
            if (key != QLatin1String("localtime") && key != QLatin1String("heatsetpoint"))
            {
                return false;
            }
        }
        if (!transition.contains(QLatin1String("localtime")) || !transition.contains(QLatin1String("heatsetpoint")) ||
            transition[QLatin1String("localtime")].type() != QVariant::String || transition[QLatin1String("heatsetpoint")].type() != QVariant::Double)
        {
            return false;
        }
        bool ok;
        int heatsetpoint = transition[QLatin1String("heatsetpoint")].toInt(&ok);
        if (!ok || heatsetpoint < 500 || heatsetpoint > 3000)
        {
            return false;
        }
        QString localtime = transition[QLatin1String("localtime")].toString();
        int hh, mm;
        ok = (localtime.size() == 6 && localtime.mid(0, 1) == "T" && localtime.mid(3, 1) == ":");
        if (ok)
        {
            hh = localtime.mid(1, 2).toInt(&ok);
        }
        if (ok)
        {
            mm = localtime.mid(4, 2).toInt(&ok);
        }
        if (!ok)
        {
            return false;
        }
        *s += QString("T%1:%2|%3")
            .arg(hh, 2, 10, QChar('0'))
            .arg(mm, 2, 10, QChar('0'))
            .arg(heatsetpoint);
    }
    return true;
}

/*! Deserialise a list of transitions.
 * \param s the serialised transitions
 * \param transitions the list of transitions
 */
bool DeRestPluginPrivate::deserialiseThermostatTransitions(const QString &s, QVariantList *transitions)
{
    transitions->clear();
    QStringList list = s.split("T", QString::SkipEmptyParts);
    for (const QString &entry : list)
    {
        QStringList attributes = entry.split("|");
        if (attributes.size() != 2)
        {
            transitions->clear();
            return false;
        }
        QVariantMap map;
        map[QLatin1String("localtime")] = "T" + attributes.at(0);
        map[QLatin1String("heatsetpoint")] = attributes.at(1).toInt();
        transitions->push_back(map);
    }
    return true;
}

/*! Serialise a thermostat schedule
 * \param schedule the schedule
 * \param s the serialised schedule
 */
bool DeRestPluginPrivate::serialiseThermostatSchedule(const QVariantMap &schedule, QString *s)
{
    *s = "";
    for (const QString &key : schedule.keys())
    {
        QString transitions;

        *s += QString("%1/").arg(key);
        if (!serialiseThermostatTransitions(schedule[key].toList(), &transitions))
        {
            return false;
        }
        *s += transitions;
    }
    return true;
}

/*! Deserialise a thermostat schedule
 * \param s the serialised schedule
 * \param schedule the schedule
 */
bool DeRestPluginPrivate::deserialiseThermostatSchedule(const QString &s, QVariantMap *schedule)
{
    schedule->clear();
    QStringList list = s.split("W", QString::SkipEmptyParts);
    for (const QString &entry : list)
    {
        QStringList attributes = entry.split("/");
        QVariantList list;
        if (attributes.size() != 2 || !deserialiseThermostatTransitions(attributes.at(1), &list))
        {
            schedule->clear();
            return false;
        }
        (*schedule)["W" + attributes.at(0)] = list;
    }
    return true;
}

/*! Update thermostat schedule with new transitions
 * \param sensor the sensorNode
 * \param newWeekdays the ISO bitmap of the weekdays
 * \param transitions the serialised list of transitions
 */
void DeRestPluginPrivate::updateThermostatSchedule(Sensor *sensor, quint8 newWeekdays, QString &transitions)
{
    // Deserialise currently saved schedule, without newWeekdays.
    bool ok = true;
    ResourceItem *item = sensor->item(RConfigSchedule);
    if (!item)
    {
        return;
    }
    QMap<quint8, QString> map;
    QStringList list = item->toString().split("W", QString::SkipEmptyParts);
    for (const QString &entry : list)
    {
        QStringList attributes = entry.split("/");
        quint8 weekdays = attributes.at(0).toUInt(&ok);
        if (!ok)
        {
            break;
        }
        weekdays &= ~newWeekdays;
        if (weekdays != 0)
        {
            map[weekdays] = attributes.at(1);
        }
    }
    if (!ok)
    {
        map.clear();
    }

    // Check if we already have an entry with these transitions.
    if (transitions.size() > 0)
    {
        ok = false;
        for (const quint8 weekdays : map.keys())
        {
            if (map[weekdays] == transitions)
            {
                // Merge the entries.
                map.remove(weekdays);
                map[weekdays | newWeekdays] = transitions;
                ok = true;
                break;
            }
        }
        if (!ok)
        {
            // Create new entry.
            map[newWeekdays] = transitions;
        }
    }

    // Store the updated schedule.
    QString s = QString("");
    for (const quint8 weekdays : map.keys())
    {
        s += QString("W%1/").arg(weekdays) + map[weekdays];
    }
    item->setValue(s);
    enqueueEvent(Event(RSensors, RConfigSchedule, sensor->id(), item));
    updateSensorEtag(&*sensor);
    sensor->setNeedSaveDatabase(true);
    queSaveDb(DB_SENSORS, DB_SHORT_SAVE_DELAY);
}


// static const QStringList weekday({"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Away"});
static TaskItem taskScheduleTimer;
static int dayofweekTimer = 0;

/*! Handle packets related to the ZCL Thermostat cluster.
    \param ind the APS level data indication containing the ZCL packet
    \param zclFrame the actual ZCL frame which holds the Thermostat cluster command or attribute
 */
void DeRestPluginPrivate::handleThermostatClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    Sensor *sensor = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), ind.srcEndpoint(), QLatin1String("ZHAThermostat"));

    if (!sensor)
    {
        DBG_Printf(DBG_INFO, "No thermostat sensor found for 0x%016llX, endpoint: 0x%02X\n", ind.srcAddress().ext(), ind.srcEndpoint());
        return;
    }

    QDataStream stream(zclFrame.payload());
    stream.setByteOrder(QDataStream::LittleEndian);

    bool isReadAttr = false;
    bool isReporting = false;
    bool isClusterCmd = false;
    if (zclFrame.isProfileWideCommand() && zclFrame.commandId() == deCONZ::ZclReadAttributesResponseId)
    {
        isReadAttr = true;
    }
    if (zclFrame.isProfileWideCommand() && zclFrame.commandId() == deCONZ::ZclReportAttributesId)
    {
        isReporting = true;
    }
    if ((zclFrame.frameControl() & 0x09) == (deCONZ::ZclFCDirectionServerToClient | deCONZ::ZclFCClusterCommand))
    {
        isClusterCmd = true;
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
            case 0x0000: // Local Temperature
            {
                qint16 temperature = attr.numericValue().s16;
                item = sensor->item(RStateTemperature);
                if (item)
                {
                    if (updateType == NodeValue::UpdateByZclReport)
                    {
                        stateUpdated = true;
                    }
                    if (item->toNumber() != temperature)
                    {
                        item->setValue(temperature);
                        enqueueEvent(Event(RSensors, RStateTemperature, sensor->id(), item));
                        stateUpdated = true;
                    }
                }
                sensor->setZclValue(updateType, ind.srcEndpoint(), THERMOSTAT_CLUSTER_ID, attrId, attr.numericValue());
            }
                break;

            case 0x0008:  // Pi Heating Demand
            {
                if (sensor->modelId().startsWith(QLatin1String("SPZB")) || // Eurotronic Spirit
                    sensor->modelId() == QLatin1String("eTRV0100") ||      // Danfoss Ally
                    sensor->modelId() == QLatin1String("TRV001") ||        // Hive TRV
                    sensor->modelId() == QLatin1String("Thermostat"))      // eCozy
                {
                    quint8 valve = attr.numericValue().u8;
                    bool on = valve > 3;
                    item = sensor->item(RStateOn);
                    if (item)
                    {
                        if (updateType == NodeValue::UpdateByZclReport)
                        {
                            stateUpdated = true;
                        }
                        if (item->toBool() != on)
                        {
                            item->setValue(on);
                            enqueueEvent(Event(RSensors, RStateOn, sensor->id(), item));
                            stateUpdated = true;
                        }
                    }
                    item = sensor->item(RStateValve);
                    if (item)
                    {
                        if (updateType == NodeValue::UpdateByZclReport)
                        {
                            stateUpdated = true;
                        }
                        if (item && item->toNumber() != valve)
                        {
                            item->setValue(valve);
                            enqueueEvent(Event(RSensors, RStateValve, sensor->id(), item));
                            stateUpdated = true;
                        }
                    }
                }
                sensor->setZclValue(updateType, ind.srcEndpoint(), THERMOSTAT_CLUSTER_ID, attrId, attr.numericValue());
            }
                break;

            case 0x0010: // Local Temperature Calibration (offset in 0.1 °C steps, from -2,5 °C to +2,5 °C)
            {
                qint8 config = attr.numericValue().s8 * 10;
                item = sensor->item(RConfigOffset);
                if (item && item->toNumber() != config)
                {
                    item->setValue(config);
                    enqueueEvent(Event(RSensors, RConfigOffset, sensor->id(), item));
                    configUpdated = true;
                }
                sensor->setZclValue(updateType, ind.srcEndpoint(), THERMOSTAT_CLUSTER_ID, attrId, attr.numericValue());
            }
                break;

            case 0x0011: // Occupied Cooling Setpoint
            {
                qint16 coolSetpoint = attr.numericValue().s16;
                item = sensor->item(RConfigCoolSetpoint);
                if (item && item->toNumber() != coolSetpoint)
                {
                    item->setValue(coolSetpoint);
                    enqueueEvent(Event(RSensors, RConfigCoolSetpoint, sensor->id(), item));
                    configUpdated = true;
                }
                sensor->setZclValue(updateType, ind.srcEndpoint(), THERMOSTAT_CLUSTER_ID, attrId, attr.numericValue());
            }
                break;

            case 0x0012: // Occupied Heating Setpoint
            {
                if (sensor->modelId().startsWith(QLatin1String("SPZB"))) // Eurotronic Spirit
                {
                    // Use 0x4003 instead.
                }
                else
                {
                    qint16 heatSetpoint = attr.numericValue().s16;
                    item = sensor->item(RConfigHeatSetpoint);
                    if (item && item->toNumber() != heatSetpoint)
                    {
                        item->setValue(heatSetpoint);
                        enqueueEvent(Event(RSensors, RConfigHeatSetpoint, sensor->id(), item));
                        configUpdated = true;
                    }
                }
                sensor->setZclValue(updateType, ind.srcEndpoint(), THERMOSTAT_CLUSTER_ID, attrId, attr.numericValue());
            }
                break;

            case 0x001C: // System Mode
            {
                if (sensor->modelId().startsWith(QLatin1String("SLR2")) ||   // Hive
                    sensor->modelId().startsWith(QLatin1String("SLR1b")) ||  // Hive
                    sensor->modelId().startsWith(QLatin1String("TH112")) ||  // Sinope
                    sensor->modelId().startsWith(QLatin1String("Zen-01")) || // Zen
                    sensor->modelId().startsWith(QLatin1String("AC201")))    // OWON
                {
                    qint8 mode = attr.numericValue().s8;
                    QString mode_set;

                    mode_set = QString("off");
                    if ( mode == 0x01 ) { mode_set = QString("auto"); }
                    if ( mode == 0x03 ) { mode_set = QString("cool"); }
                    if ( mode == 0x04 ) { mode_set = QString("heat"); }
                    if ( mode == 0x05 ) { mode_set = QString("emergency heating"); }
                    if ( mode == 0x06 ) { mode_set = QString("precooling"); }
                    if ( mode == 0x07 ) { mode_set = QString("fan only"); }
                    if ( mode == 0x08 ) { mode_set = QString("dry"); }
                    if ( mode == 0x09 ) { mode_set = QString("sleep"); }

                    item = sensor->item(RConfigMode);
                    if (item && !item->toString().isEmpty() && item->toString() != mode_set)
                    {
                        item->setValue(mode_set);
                        enqueueEvent(Event(RSensors, RConfigMode, sensor->id(), item));
                        configUpdated = true;
                    }
                }
                sensor->setZclValue(updateType, ind.srcEndpoint(), THERMOSTAT_CLUSTER_ID, attrId, attr.numericValue());
            }
                break;

            case 0x0023: // Temperature Setpoint Hold
            {
                if (sensor->modelId() == QLatin1String("Thermostat")) // eCozy
                {
                    bool scheduleOn = (attr.numericValue().u8 == 0x00); // setpoint hold off -> schedule enabled

                    item = sensor->item(RConfigScheduleOn);
                    if (item && item->toBool() != scheduleOn)
                    {
                        item->setValue(scheduleOn);
                        enqueueEvent(Event(RSensors, RConfigScheduleOn, sensor->id(), item));
                        configUpdated = true;
                    }
                }
                sensor->setZclValue(updateType, ind.srcEndpoint(), THERMOSTAT_CLUSTER_ID, attrId, attr.numericValue());
            }
                break;

            case 0x0025:  // Thermostat Programming Operation Mode, default 0 (bit#0 = disable/enable Scheduler)
            {
                bool on = attr.bitmap() & 0x01 ? true : false;
                item = sensor->item(RConfigScheduleOn);
                if (item && item->toBool() != on)
                {
                    item->setValue(on);
                    enqueueEvent(Event(RSensors, RConfigScheduleOn, sensor->id(), item));
                    configUpdated = true;

                }
                sensor->setZclValue(updateType, ind.srcEndpoint(), THERMOSTAT_CLUSTER_ID, attrId, attr.numericValue());
            }
                break;

            case 0x0029:  // Thermostat Running State (bit0=Heat State On/Off, bit1=Cool State On/Off)
            {
                bool on = attr.bitmap() > 0;
                item = sensor->item(RStateOn);

                if (item && updateType == NodeValue::UpdateByZclReport)
                {
                    stateUpdated = true;
                }
                if (item && item->toBool() != on)
                {
                    item->setValue(on);
                    enqueueEvent(Event(RSensors, RStateOn, sensor->id(), item));
                    stateUpdated = true;
                }
                sensor->setZclValue(updateType, ind.srcEndpoint(), THERMOSTAT_CLUSTER_ID, attrId, attr.numericValue());
            }
                break;

            case 0x0030: // Setpoint Change Source
            {
                quint8 source = attr.numericValue().u8;
                item = sensor->item(RConfigLastChangeSource);
                if (item && item->toNumber() != source && source <= 2)
                {
                    item->setValue(source);
                    enqueueEvent(Event(RSensors, RConfigLastChangeSource, sensor->id(), item));
                    configUpdated = true;
                }
                sensor->setZclValue(updateType, ind.srcEndpoint(), THERMOSTAT_CLUSTER_ID, attrId, attr.numericValue());
            }
                break;

            case 0x0031: // Setpoint Change Amount
            {
                qint16 amount = attr.numericValue().s16;
                item = sensor->item(RConfigLastChangeAmount);
                if (item && item->toNumber() != amount && amount > -32768)
                {
                    item->setValue(amount);
                    enqueueEvent(Event(RSensors, RConfigLastChangeAmount, sensor->id(), item));
                    configUpdated = true;
                }
                sensor->setZclValue(updateType, ind.srcEndpoint(), THERMOSTAT_CLUSTER_ID, attrId, attr.numericValue());
            }
                break;

            case 0x0032: // Setpoint Change Timestamp
            {
                const QDateTime epoch = QDateTime(QDate(2000, 1, 1), QTime(0, 0), Qt::UTC);
                QDateTime time = epoch.addSecs(attr.numericValue().u32 - QDateTime::currentDateTime().offsetFromUtc());
                item = sensor->item(RConfigLastChangeTime);
                if (item) // && item->toVariant().toDateTime().toMSecsSinceEpoch() != time.toMSecsSinceEpoch())
                {
                    item->setValue(time);
                    enqueueEvent(Event(RSensors, RConfigLastChangeTime, sensor->id(), item));
                    configUpdated = true;
                }
                sensor->setZclValue(updateType, ind.srcEndpoint(), THERMOSTAT_CLUSTER_ID, attrId, attr.numericValue());
            }
                break;

            case 0x0045: // AC Louvers Position
            {
                qint8 mode = attr.numericValue().s8;
                QString modeSet;

                modeSet = QLatin1String("fully closed");
                if ( mode == 0x01 ) { modeSet = QLatin1String("fully closed"); }
                else if ( mode == 0x02 ) { modeSet = QLatin1String("fully open"); }
                else if ( mode == 0x03 ) { modeSet = QLatin1String("quarter open"); }
                else if ( mode == 0x04 ) { modeSet = QLatin1String("half open"); }
                else if ( mode == 0x05 ) { modeSet = QLatin1String("three quarters open"); }

                item = sensor->item(RConfigSwingMode);
                if (item && !item->toString().isEmpty() && item->toString() != modeSet)
                {
                    item->setValue(modeSet);
                    enqueueEvent(Event(RSensors, RConfigSwingMode, sensor->id(), item));
                    configUpdated = true;
                }

                sensor->setZclValue(updateType, ind.srcEndpoint(), THERMOSTAT_CLUSTER_ID, attrId, attr.numericValue());
            }
                break;

            case 0x0403: // Temperature measurement
            {
                if (sensor->modelId().startsWith(QLatin1String("Super TR"))) // ELKO
                {
                    quint8 mode = attr.numericValue().u8;
                    QString mode_set;

                    if ( mode == 0x00 ) { mode_set = QString("air sensor"); }
                    if ( mode == 0x01 ) { mode_set = QString("floor sensor"); }
                    if ( mode == 0x03 ) { mode_set = QString("floor protection"); }

                    item = sensor->item(RConfigTemperatureMeasurement);

                    if (item && item->toString() != mode_set)
                    {
                        item->setValue(mode_set);
                        enqueueEvent(Event(RSensors, RConfigTemperatureMeasurement, sensor->id(), item));
                        configUpdated = true;
                    }
                }
                sensor->setZclValue(updateType, ind.srcEndpoint(), THERMOSTAT_CLUSTER_ID, attrId, attr.numericValue());
            }
                break;

            case 0x0406: // Device on/off
            {
                if (sensor->modelId() == QLatin1String("Super TR")) // ELKO
                {
                    bool on = attr.numericValue().u8 > 0 ? true : false;
                    item = sensor->item(RStateOn);

                    if (item && updateType == NodeValue::UpdateByZclReport)
                    {
                        stateUpdated = true;
                    }
                    if (item && item->toBool() != on)
                    {
                        item->setValue(on);
                        enqueueEvent(Event(RSensors, RStateOn, sensor->id(), item));
                        stateUpdated  = true;
                    }

                    // Set config/mode to have an adequate representation based on this attribute
                    QString mode_set;
                    if ( on == false ) { mode_set = QString("off"); }
                    if ( on == true ) { mode_set = QString("heat"); }

                    item = sensor->item(RConfigMode);
                    if (item && !item->toString().isEmpty() && item->toString() != mode_set)
                    {
                        item->setValue(mode_set);
                        enqueueEvent(Event(RSensors, RConfigMode, sensor->id(), item));
                        configUpdated = true;
                    }
                    sensor->setZclValue(updateType, ind.srcEndpoint(), THERMOSTAT_CLUSTER_ID, attrId, attr.numericValue());
                }
            }
                break;

            case 0x0409: // Floor temperature
            {
                if (sensor->modelId().startsWith(QLatin1String("Super TR"))) // ELKO
                {
                    qint16 floortemp = attr.numericValue().s16;
                    item = sensor->item(RStateFloorTemperature);

                    if (item && updateType == NodeValue::UpdateByZclReport)
                    {
                        stateUpdated = true;
                    }
                    if (item && item->toNumber() != floortemp)
                    {
                        item->setValue(floortemp);
                        enqueueEvent(Event(RSensors, RStateFloorTemperature, sensor->id(), item));
                        stateUpdated = true;
                    }
                }
                sensor->setZclValue(updateType, ind.srcEndpoint(), THERMOSTAT_CLUSTER_ID, attrId, attr.numericValue());
            }
                break;

            case 0x0413: // Child lock
            {
                if (sensor->modelId() == QLatin1String("Super TR")) // ELKO
                {
                    bool enabled = attr.numericValue().u8 > 0 ? true : false;
                    item = sensor->item(RConfigLocked);
                    if (item && item->toBool() != enabled)
                    {
                        item->setValue(enabled);
                        enqueueEvent(Event(RSensors, RConfigLocked, sensor->id(), item));
                        configUpdated = true;
                    }
                    sensor->setZclValue(updateType, ind.srcEndpoint(), THERMOSTAT_CLUSTER_ID, attrId, attr.numericValue());
                }
            }
                break;

            case 0x0415: // Heating active/inactive
            {
                if (sensor->modelId() == QLatin1String("Super TR")) // ELKO
                {
                    bool on = attr.numericValue().u8 > 0 ? true : false;
                    item = sensor->item(RStateHeating);

                    if (item && updateType == NodeValue::UpdateByZclReport)
                    {
                        stateUpdated = true;
                    }
                    if (item && item->toBool() != on)
                    {
                        item->setValue(on);
                        enqueueEvent(Event(RSensors, RStateHeating, sensor->id(), item));
                        stateUpdated  = true;
                    }
                    sensor->setZclValue(updateType, ind.srcEndpoint(), THERMOSTAT_CLUSTER_ID, attrId, attr.numericValue());
                }
            }
                break;

            // manufacturerspecific reported by Eurotronic SPZB0001
            // https://eurotronic.org/wp-content/uploads/2019/01/Spirit_ZigBee_BAL_web_DE_view_V9.pdf
            case 0x4000: // enum8 (0x30): value 0x02, TRV mode
            {
                if (zclFrame.manufacturerCode() == VENDOR_JENNIC)
                {
                }
                else if (zclFrame.manufacturerCode() == VENDOR_DANFOSS)
                {
                    quint8 windowmode = attr.numericValue().u8;
                    QString windowmode_set;

                    if ( windowmode == 0x01 ) { windowmode_set = QString("Closed"); }
                    if ( windowmode == 0x02 ) { windowmode_set = QString("Hold"); }
                    if ( windowmode == 0x03 ) { windowmode_set = QString("Open"); }
                    if ( windowmode == 0x04 ) { windowmode_set = QString("Open (external), closed (internal)"); }

                    item = sensor->item(RStateWindowOpen);
                    if (item && updateType == NodeValue::UpdateByZclReport)
                    {
                        stateUpdated = true;
                    }
                    if (item && item->toString() != windowmode_set)
                    {
                        item->setValue(windowmode_set);
                        enqueueEvent(Event(RSensors, RStateWindowOpen, sensor->id(), item));
                        stateUpdated = true;
                    }
                }
                sensor->setZclValue(updateType, ind.srcEndpoint(), THERMOSTAT_CLUSTER_ID, attrId, attr.numericValue());
            }
                break;

            case 0x4001: // U8 (0x20): value 0x00, valve position
            case 0x4002: // U8 (0x20): value 0x00, errors
            {
                if (zclFrame.manufacturerCode() == VENDOR_JENNIC)
                {

                }
            }
                sensor->setZclValue(updateType, ind.srcEndpoint(), THERMOSTAT_CLUSTER_ID, attrId, attr.numericValue());
                break;

            case 0x4003:
            {   // Current temperature set point - this will be reported when manually changing the temperature
                if (zclFrame.manufacturerCode() == VENDOR_JENNIC && sensor->modelId().startsWith(QLatin1String("SPZB"))) // Eurotronic Spirit
                {
                    qint16 heatSetpoint = attr.numericValue().s16;
                    item = sensor->item(RConfigHeatSetpoint);
                    if (item)
                    {
                        if (updateType == NodeValue::UpdateByZclReport)
                        {
                            stateUpdated = true;
                        }
                        if (item->toNumber() != heatSetpoint)
                        {
                            item->setValue(heatSetpoint);
                            enqueueEvent(Event(RSensors, RConfigHeatSetpoint, sensor->id(), item));
                            stateUpdated = true;
                        }
                    }
                }

                // External Window Open signal
                if (zclFrame.manufacturerCode() == VENDOR_DANFOSS && (sensor->modelId() == QLatin1String("eTRV0100") ||
                                                                      sensor->modelId() == QLatin1String("TRV001")))
                {
                    bool enabled = attr.numericValue().u8 > 0 ? true : false;
                    item = sensor->item(RConfigExternalWindowOpen);
                    if (item && item->toBool() != enabled)
                    {
                        item->setValue(enabled);
                        enqueueEvent(Event(RSensors, RConfigExternalWindowOpen, sensor->id(), item));
                        configUpdated = true;
                    }
                }
                
                sensor->setZclValue(updateType, ind.srcEndpoint(), THERMOSTAT_CLUSTER_ID, attrId, attr.numericValue());
            }
                break;

            case 0x4008: // U24 (0x22): 0x000001, host flags
            {
                if (zclFrame.manufacturerCode() == VENDOR_JENNIC && sensor->modelId().startsWith(QLatin1String("SPZB"))) // Eurotronic Spirit
                {
                    quint32 hostFlags = attr.numericValue().u32;
                    bool flipped = hostFlags & 0x000002;
                    QString mode = hostFlags & 0x000010 ? "off" : hostFlags & 0x000004 ? "heat" : "auto";
                    bool locked = hostFlags & 0x000080;
                    item = sensor->item(RConfigHostFlags);
                    if (item && item->toNumber() != hostFlags)
                    {
                        item->setValue(hostFlags);
                        // Hidden attribute - no event
                        configUpdated = true; // but do save database
                    }
                    item = sensor->item(RConfigDisplayFlipped);
                    if (item && item->toBool() != flipped)
                    {
                        item->setValue(flipped);
                        enqueueEvent(Event(RSensors, RConfigDisplayFlipped, sensor->id(), item));
                        configUpdated = true;
                    }
                    item = sensor->item(RConfigLocked);
                    if (item && item->toBool() != locked)
                    {
                        item->setValue(locked);
                        enqueueEvent(Event(RSensors, RConfigLocked, sensor->id(), item));
                        configUpdated = true;
                    }
                    item = sensor->item(RConfigMode);
                    if (item && item->toString() != mode)
                    {
                        item->setValue(mode);
                        enqueueEvent(Event(RSensors, RConfigMode, sensor->id(), item));
                        configUpdated = true;
                    }
                }
                sensor->setZclValue(updateType, ind.srcEndpoint(), THERMOSTAT_CLUSTER_ID, attrId, attr.numericValue());
            }
                break;

            case 0x4012: // Mounting mode active
            {
                if (zclFrame.manufacturerCode() == VENDOR_DANFOSS && (sensor->modelId() == QLatin1String("eTRV0100") ||
                                                                      sensor->modelId() == QLatin1String("TRV001")))
                {
                    bool enabled = attr.numericValue().u8 > 0 ? true : false;
                    item = sensor->item(RStateMountingModeActive);
                    if (item && item->toBool() != enabled)
                    {
                        item->setValue(enabled);
                        enqueueEvent(Event(RSensors, RStateMountingModeActive, sensor->id(), item));
                        configUpdated = true;
                    }
                }
                sensor->setZclValue(updateType, ind.srcEndpoint(), THERMOSTAT_CLUSTER_ID, attrId, attr.numericValue());
            }
                break;

            case 0x4013: // Mounting mode control
            {
                if (zclFrame.manufacturerCode() == VENDOR_DANFOSS && (sensor->modelId() == QLatin1String("eTRV0100") ||
                                                                      sensor->modelId() == QLatin1String("TRV001")))
                {
                    bool enabled = attr.numericValue().u8 > 0 ? true : false;
                    item = sensor->item(RConfigMountingMode);
                    if (item && item->toBool() != enabled)
                    {
                        item->setValue(enabled);
                        enqueueEvent(Event(RSensors, RConfigMountingMode, sensor->id(), item));
                        configUpdated = true;
                    }
                }
                sensor->setZclValue(updateType, ind.srcEndpoint(), THERMOSTAT_CLUSTER_ID, attrId, attr.numericValue());
            }
                break;

            case 0x4015: // External Measured Room Sensor
            {
                if (zclFrame.manufacturerCode() == VENDOR_DANFOSS && (sensor->modelId() == QLatin1String("eTRV0100") ||
                                                                      sensor->modelId() == QLatin1String("TRV001")))
                {
                    qint16 externalMeasurement = attr.numericValue().s16;
                    item = sensor->item(RConfigExternalTemperatureSensor);
                    if (item)
                    {
                        if (updateType == NodeValue::UpdateByZclReport)
                        {
                            configUpdated = true;
                        }
                        if (item->toNumber() != externalMeasurement)
                        {
                            item->setValue(externalMeasurement);
                            enqueueEvent(Event(RSensors, RConfigExternalTemperatureSensor, sensor->id(), item));
                            configUpdated = true;
                        }
                    }
                }
                sensor->setZclValue(updateType, ind.srcEndpoint(), THERMOSTAT_CLUSTER_ID, attrId, attr.numericValue());
            }
                break;

            // Manufacturer Specific for Danfoss Icon Floor Heating Controller
            case 0x4110:  // Danfoss Output Status
            {
                if (sensor->modelId() == QLatin1String("0x8020") || // Danfoss RT24V Display thermostat
                    sensor->modelId() == QLatin1String("0x8021") || // Danfoss RT24V Display thermostat with floor sensor
                    sensor->modelId() == QLatin1String("0x8030") || // Danfoss RTbattery Display thermostat
                    sensor->modelId() == QLatin1String("0x8031") || // Danfoss RTbattery Display thermostat with infrared
                    sensor->modelId() == QLatin1String("0x8034") || // Danfoss RTbattery Dial thermostat
                    sensor->modelId() == QLatin1String("0x8035"))   // Danfoss RTbattery Dial thermostat with infrared
                {
                    quint8 outputStatus = attr.numericValue().u8;
                    bool on = outputStatus > 0;
                    item = sensor->item(RStateOn);
                    if (item)
                    {
                        if (updateType == NodeValue::UpdateByZclReport)
                        {
                            stateUpdated = true;
                        }
                        if (item->toBool() != on)
                        {
                            item->setValue(on);
                            enqueueEvent(Event(RSensors, RStateOn, sensor->id(), item));
                            stateUpdated = true;
                        }
                    }
                }
                sensor->setZclValue(updateType, ind.srcEndpoint(), THERMOSTAT_CLUSTER_ID, attrId, attr.numericValue());
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

    // Read ZCL Cluster Command Response
    if (isClusterCmd && zclFrame.commandId() == 0x00) // Get Weekly Schedule Response
    {
        // Read command parameters into serialised string.
        QString transitions = QString("");
        quint8 numberOfTransitions = 0;
        quint8 dayOfWeek = 0;
        quint8 mode = 0;

        stream >> numberOfTransitions;
        stream >> dayOfWeek;
        stream >> mode;

        for (quint8 i = 0; i < numberOfTransitions; i++)
        {
            quint16 transitionTime;
            qint16 heatSetpoint;
            qint16 coolSetpoint;

            stream >> transitionTime;
            if (mode & 0x01) // bit 0: heat setpoint
            {
                stream >> heatSetpoint;
                transitions += QString("T%1:%2|%3")
                    .arg(transitionTime / 60, 2, 10, QChar('0'))
                    .arg(transitionTime % 60, 2, 10, QChar('0'))
                    .arg(heatSetpoint);
            }
            if (mode & 0x02) // bit 1: cold setpoint
            {
                stream >> coolSetpoint;
                // ignored for now
                break;
            }
        }
        if (stream.status() == QDataStream::ReadPastEnd)
        {
            return;
        }

        const quint8 newWeekdays = convertWeekdayBitmap(dayOfWeek);
        updateThermostatSchedule(sensor, newWeekdays, transitions);
    }
}

/*! Adds a thermostat command task to the queue.

   \param task - the task item
   \param cmdId - 0x00 setpoint raise/lower
                  0x02 get schedule
                  0x03 clear schedule
   \param setpoint - raise/lower value
   \param days - days to return schedule
   \return true - on success
           false - on error
 */
bool DeRestPluginPrivate::addTaskThermostatCmd(TaskItem &task, uint16_t mfrCode, uint8_t cmd, int16_t setpoint, uint8_t daysToReturn)
{
    task.taskType = TaskThermostat;

    task.req.setClusterId(THERMOSTAT_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(cmd);
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
            deCONZ::ZclFCDirectionClientToServer);

    if (mfrCode != 0x0000)
    {
        task.zclFrame.setFrameControl(task.zclFrame.frameControl() | deCONZ::ZclFCManufacturerSpecific);
        task.zclFrame.setManufacturerCode(mfrCode);
    }

    // payload
    QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    if (cmd == 0x00)
    {
        stream << (qint8) 0x02;  // Enum 8 Both (adjust Heat Setpoint and Cool Setpoint)
        stream << (qint8) setpoint;  // 8-bit raise/lower
    }
    else if (cmd == 0x02)  // get schedule
    {
        stream << (quint8) daysToReturn;
        stream << (quint8) 0x01; // mode heat
    }
    else if (cmd == 0x03)  // clear schedule
    {
        // no payload
    }
    else if (cmd == 0x40) // Danfoss/Hive manufacturer command
    {
        stream << (qint8) 0x01;       // Large valve movement
        stream << (qint16) setpoint;  // temperature
    }
    else
    {
        return false;
    }

    { // ZCL frame
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    return addTask(task);
}

/*! Adds a Set Weekly Schedule command to the queue.
   \param task - the task item
   \param weekdays - the ISO-bitmap of weekdays
   \param transitions - the serialised list of transitions
   \return true - on success
           false - on error
 */
bool DeRestPluginPrivate::addTaskThermostatSetWeeklySchedule(TaskItem &task, quint8 weekdays, const QString &transitions)
{
    task.taskType = TaskThermostat;

    task.req.setClusterId(THERMOSTAT_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(0x01); // Set Weekly Schedule
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                                  deCONZ::ZclFCDirectionClientToServer);

    QStringList list = transitions.split("T", QString::SkipEmptyParts);
    quint8 numberOfTransitions = list.size();

    // payload
    QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream << numberOfTransitions;
    stream << convertWeekdayBitmap(weekdays);
    stream << (quint8) 0x01; // Mode: heat

    for (const QString &entry : list)
    {
        QStringList attributes = entry.split("|");
        if (attributes.size() != 2)
        {
            return false;
        }
        const quint16 hh = attributes.at(0).mid(0, 2).toUInt();
        const quint16 mm = attributes.at(0).mid(3, 2).toUInt();
        const quint16 time = 60 * hh + mm;
        const qint16 heatSetpoint = attributes.at(1).toInt();
        stream << time;
        stream << heatSetpoint;
    }

    { // ZCL frame
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    return addTask(task);
}

/*! Helper to generate a new task with new task and req id based on a reference */
static void copyTaskReq(TaskItem &a, TaskItem &b)
{
    b.req.dstAddress() = a.req.dstAddress();
    b.req.setDstAddressMode(a.req.dstAddressMode());
    b.req.setSrcEndpoint(a.req.srcEndpoint());
    b.req.setDstEndpoint(a.req.dstEndpoint());
    b.req.setRadius(a.req.radius());
    b.req.setTxOptions(a.req.txOptions());
    b.req.setSendDelay(a.req.sendDelay());
    b.zclFrame.payload().clear();
}

bool DeRestPluginPrivate::addTaskThermostatGetSchedule(TaskItem &task)
{
    copyTaskReq(task, taskScheduleTimer);

    dayofweekTimer = 0;

    for (int i = 0; i < 7; i++)
    {
        // use QTimer to send a command once every second to battery Endpoints
        QTimer::singleShot(1000 * (i + 2), this, SLOT(addTaskThermostatGetScheduleTimer()));
    }

    return true;
}

void DeRestPluginPrivate::addTaskThermostatGetScheduleTimer()
{
    TaskItem task;
    copyTaskReq(taskScheduleTimer, task);

    uint8_t dayofweek = (1 << dayofweekTimer);
    dayofweekTimer++;

    addTaskThermostatCmd(task, 0, 0x02, 0, dayofweek);  // get schedule
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
bool DeRestPluginPrivate::addTaskThermostatReadWriteAttribute(TaskItem &task, uint8_t readOrWriteCmd, uint16_t mfrCode, uint16_t attrId, uint8_t attrType, uint32_t attrValue)
{
    if (readOrWriteCmd != deCONZ::ZclReadAttributesId && readOrWriteCmd != deCONZ::ZclWriteAttributesId)
    {
        DBG_Printf(DBG_INFO, "Thermostat invalid parameter readOrWriteCmd %d\n", readOrWriteCmd);
        return false;
    }

    task.taskType = TaskThermostat;

    task.req.setClusterId(THERMOSTAT_CLUSTER_ID);
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

/*! Write Attribute List on thermostat cluster.
   \param task - the task item
   \param AttributeList
   \return true - on success
           false - on error
 */
bool DeRestPluginPrivate::addTaskThermostatWriteAttributeList(TaskItem &task, uint16_t mfrCode, QMap<quint16, quint32> &AttributeList )
{

    task.taskType = TaskThermostat;

    task.req.setClusterId(THERMOSTAT_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(deCONZ::ZclWriteAttributesId);
    task.zclFrame.setFrameControl(deCONZ::ZclFCProfileCommand |
            deCONZ::ZclFCDirectionClientToServer |
            deCONZ::ZclFCDisableDefaultResponse);

    if (mfrCode != 0x0000)
    {
        task.zclFrame.setFrameControl(task.zclFrame.frameControl() | deCONZ::ZclFCManufacturerSpecific);
        task.zclFrame.setManufacturerCode(mfrCode);
    }

    // payload
    quint16 attrId;
    quint32 attrValue;

    QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    QMapIterator<quint16, quint32> i(AttributeList);
    while (i.hasNext()) {
        i.next();
        attrId = i.key();
        attrValue = i.value();

        //attribute
        stream << (quint16) attrId;

        //type and value
        switch (attrId)
        {
            case 0x0023:
            case 0x001C:
                stream << (quint8) deCONZ::Zcl8BitEnum;
                stream << (quint8) attrValue;
                break;
            case 0x0012:
            case 0x0024:
                stream << (quint8) deCONZ::Zcl16BitInt;
                stream << (quint16) attrValue;
                break;
            default:
            {
                // to avoid
            }
            break;
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

/*! Adds a control mode command task to the queue. Used by Legrand

   \param task - the task item
   \param cmdId - 0x00 set heating mode
   \param mode
   \return true - on success
           false - on error
 */
bool DeRestPluginPrivate::addTaskControlModeCmd(TaskItem &task, uint8_t cmdId, int8_t mode)
{
    task.taskType = TaskThermostat;

    task.req.setClusterId(LEGRAND_CONTROL_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(cmdId);
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
            deCONZ::ZclFCDirectionClientToServer);

    // payload
    QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    if (cmdId == 0x00)
    {
        stream << (qint8) mode;
    }
    else
    {
        return false;
    }

    { // ZCL frame
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    return addTask(task);
}
