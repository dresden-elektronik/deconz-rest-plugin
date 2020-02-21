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

static const QStringList weekday({"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Away"});
static TaskItem taskScheduleTimer;
static int dayofweekTimer = 0;

/*! Handle packets related to the ZCL Thermostat cluster.
    \param ind the APS level data indication containing the ZCL packet
    \param zclFrame the actual ZCL frame which holds the Thermostat cluster command or attribute
 */
void DeRestPluginPrivate::handleThermostatClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    Sensor *sensor = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), 0x01);

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

            if (!sensor)
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
                if (sensor->modelId().startsWith(QLatin1String("SPZB"))) // Eurotronic Spirit
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
                    if (item && item->toNumber() != valve)
                    {
                        item->setValue(valve);
                        enqueueEvent(Event(RSensors, RStateValve, sensor->id(), item));
                        stateUpdated = true;
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

            case 0x0025:  // Thermostat Programming Operation Mode, default 0 (bit#0 = disable/enable Scheduler)
            {
                bool on = attr.bitmap() & 0x01 ? true : false;
                item = sensor->item(RConfigSchedulerOn);
                if (item && item->toBool() != on)
                {
                    item->setValue(on);
                    enqueueEvent(Event(RSensors, RConfigSchedulerOn, sensor->id(), item));
                    configUpdated = true;

                }
                sensor->setZclValue(updateType, ind.srcEndpoint(), THERMOSTAT_CLUSTER_ID, attrId, attr.numericValue());
            }
                break;

            case 0x0029:  // Thermostat Running State (bit0=Heat State On/Off, bit1=Cool State On/Off)
            {
                bool on = attr.bitmap() > 0;
                item = sensor->item(RStateOn);
                if (item && item->toBool() != on)
                {
                    item->setValue(on);
                    enqueueEvent(Event(RSensors, RStateOn, sensor->id(), item));
                    configUpdated = true;
                }
                sensor->setZclValue(updateType, ind.srcEndpoint(), THERMOSTAT_CLUSTER_ID, attrId, attr.numericValue());
            }
                break;

            // manufacturerspecific reported by Eurotronic SPZB0001
            // https://eurotronic.org/wp-content/uploads/2019/01/Spirit_ZigBee_BAL_web_DE_view_V9.pdf
            case 0x4000: // enum8 (0x30): value 0x02, TRV mode
            case 0x4001: // U8 (0x20): value 0x00, valve position
            case 0x4002: // U8 (0x20): value 0x00, errors
            {
                if (zclFrame.manufacturerCode() == VENDOR_JENNIC)
                {

                }
            }
                sensor->setZclValue(updateType, ind.srcEndpoint(), THERMOSTAT_CLUSTER_ID, attrId, attr.numericValue());
                break;

            case 0x4003: // Current temperature set point
            {   // this will be reported when manually changing the temperature
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

    // Read ZCL Cluster Command Response
    if (isClusterCmd && zclFrame.commandId() == 0x00)  // get schedule command response
    {
        QVariantMap map;
        QVariantMap mapsorted;
        QStringList schedList;
        QString sched;

        ResourceItem *item = nullptr;
        item = sensor->item(RConfigScheduler);
        QString val;
        QString valstored;

        if (item)
        {
            valstored = item->toString();
        }

        if (item && !valstored.isEmpty())
        {
            schedList = valstored.split(";", QString::SkipEmptyParts);
            for (const QString &dayEntry : schedList)
            {
                QString schedEntry;
                QStringList checkdayList;
                QStringList checkSchedule = dayEntry.split(" ");
                checkdayList = checkSchedule.at(0).split(",");
                if (checkSchedule.length() > 1)
                {
                    checkSchedule.removeFirst();
                    schedEntry = checkSchedule.join(" ");

                }
                for (const QString &checkday : checkdayList)
                {
                    if (weekday.contains(checkday))
                    {
                        map[checkday] = schedEntry;
                    }
                }
            }
        }

        // ZCL Cluster Command Response variables
        int count = 0;
        int daycount = 0;

        while (!stream.atEnd())
        {
            quint8 nrTrans = 0;
            quint8 dayOfWeek = 0;
            quint8 modeSeq = 0;

            if (count == 0)
            {
                stream >> nrTrans;
                stream >> dayOfWeek;
                stream >> modeSeq;

                for (daycount = 0; daycount < 8; daycount++)
                {
                    if ((dayOfWeek >> daycount) & 1U)
                    {
                        break;
                    }
                }
            }

            if (count < nrTrans)
            {
                quint16 transTime;
                qint16 heatSetPoint;
                qint16 coolSetPoint;

                stream >> transTime;

                QTime midnight(0, 0, 0);
                QTime heatTime = midnight.addSecs(transTime * 60);

                val = val + " " + qPrintable(heatTime.toString("HH:mm"));

                if (modeSeq & 0x01)  // bit-0 heat set point
                {
                    stream >> heatSetPoint;
                    val = val + QString(" %1").arg(heatSetPoint);
                }
                if (modeSeq & 0x02)  // bit-1 cool set point
                {
                    stream >> coolSetPoint;
                }
                count++;
            }
        }

        if (stream.status() == QDataStream::ReadPastEnd)
        {
            return;
        }

        map[weekday.at(daycount)] = val.trimmed();

        QStringList weekdayList({"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"});

        for (const QString &key : weekdayList)
        {
            QString schedtime = map[key].toString();
            if (mapsorted.contains(schedtime))
            {
                mapsorted[schedtime] = mapsorted[schedtime].toString() + "," + key;  // e.g. Monday,Tuesday,...
            }
            else if (!schedtime.isEmpty())
            {
                mapsorted[schedtime] = key; // e.g. Monday
            }
        }

        for (const QString &key : mapsorted.keys())
        {
            sched = sched + mapsorted[key].toString() + " " + key + ";";  // e.g "Monday,Tuesday 06:00 2100 22:00 1700"
        }

        // Example: scheduler =
        // "Monday,Tuesday,Wednesday,Thursday,Friday 05:00 2200 06:00 1700 16:30 2200 17:00 2000 18:00 2200 19:00 1800;Saturday,Sunday 06:00 2100 16:30 2200 17:00 2000 18:00 2200 19:00 1800;"
        //
        //                                22.0 °C     -----------            -----------          ------------                                       -----------           -----------
        //                                            |         |            |         |          |          |                           -------------          |          |          |
        //                                20.0 °C     |         |            |         ------------          |                           |                      ------------          |
        //                                            |         |            |                               |                           |                                            |
        //                                18.0 °C  ----         |            |                               --------                -----                                            ----------
        //                                17.0 °C               --------------

        DBG_Printf(DBG_INFO, "Thermostat 0x%04X scheduler = %s\n", ind.srcAddress().nwk(), qPrintable(sched));
        if (item)
        {
            item->setValue(sched);
        }
    }

}


static QByteArray setSchedule(const QString &sched)
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    quint8 dayofweek = 0;
    QStringList schedList = sched.simplified().split(";", QString::SkipEmptyParts);
    for (const QString &dayEntry : schedList)
    {
        QStringList checkdayList;
        QStringList checkSchedule = dayEntry.split(" ", QString::SkipEmptyParts);
        checkdayList = checkSchedule.at(0).split(",");  // e.g. Monday,Tuesday,Wednesday
        for (const QString &checkday : checkdayList)
        {
            if (weekday.contains(checkday))
            {
                int n = weekday.indexOf(checkday);
                dayofweek |= (0x01 << n);
            }
        }

        if (checkSchedule.length() > 1)
        {
            checkSchedule.removeFirst();

            stream << (quint8) (checkSchedule.length() / 2);  // number of transitions
            stream << dayofweek;
            stream << (quint8) 0x01; // mode heat

            // e.g. 06:00 2100 22:00 1700
            for (int i = 0; i < checkSchedule.length(); i++)
            {
                const QString &entry = checkSchedule[i];
                if (i & 1) // odd 1,3... setpoint = 2100 1700
                {
                    stream << (qint16) entry.toInt();  // setpoint
                }
                else // even 0,2,... time = 06:00 22:00
                {
                    QTime midnight(0, 0, 0);
                    QTime heatTime = QTime::fromString(entry, "hh:mm");
                    int heatTimeMinutes = midnight.secsTo(heatTime) / 60;

                    stream << (quint16) heatTimeMinutes;  // transition time
                }
            }
        }
    }
    return payload;
}

/*! Adds a thermostat command task to the queue.

   \param task - the task item
   \param cmdId - 0x00 setpoint raise/lower
                  0x01 set schedule
                  0x02 get schedule
                  0x03 clear schedule
   \param setpoint - raise/lower value
   \param schedule - set schedule e.g. "Monday,Tuesday 06:00 2000 22:00 1700; Saturday,Sunday 06:00 2100 22:00 1700"
   \param days - days to return schedule
   \return true - on success
           false - on error
 */
bool DeRestPluginPrivate::addTaskThermostatCmd(TaskItem &task, uint8_t cmd, int8_t setpoint, const QString &schedule, uint8_t daysToReturn)
{
    task.taskType = TaskThermostat;

    task.req.setClusterId(THERMOSTAT_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(cmd);
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
            deCONZ::ZclFCDirectionClientToServer);

    // payload
    QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    if (cmd == 0x00)
    {
        stream << (qint8) setpoint;  // 8-bit raise/lower
    }
    else if (cmd == 0x01)  // set schedule
    {
        QByteArray payload = setSchedule(schedule);
        stream.writeRawData(payload.data(), payload.size());
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

/*! Set Scheduler on thermostat cluster.
 *  Iterate over every weekday and get schedule.
   \param task - the task item
   \return true - on success
           false - on error
 */
bool DeRestPluginPrivate::addTaskThermostatSetAndGetSchedule(TaskItem &task, const QString &sched)
{
    copyTaskReq(task, taskScheduleTimer);

    if (!sched.isEmpty() && !addTaskThermostatCmd(task, 0x01, 0, sched, 0))  // set schedule
    {
        return false;
    }

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

    addTaskThermostatCmd(task, 0x02, 0, nullptr, dayofweek);  // get schedule
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
