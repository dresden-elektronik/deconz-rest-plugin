/*
 * tuya.cpp
 *
 * Implementation of Tuya cluster.
 *
 */

#include <regex>
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "tuya.h"


//***********************************************************************************

// Value for dp_type
// ------------------
// 0x00 	DP_TYPE_RAW 	?
// 0x01 	DP_TYPE_BOOL 	?
// 0x02 	DP_TYPE_VALUE 	4 byte unsigned integer
// 0x03 	DP_TYPE_STRING 	variable length string
// 0x04 	DP_TYPE_ENUM 	1 byte enum
// 0x05 	DP_TYPE_FAULT 	1 byte bitmap (didn't test yet)

// Value for dp_identifier (it s device dependent)
//
// Value for windows covering
//-----------------------------------------------------
// 0x01 	control         	enum 	open, stop, close, continue
// 0x02 	percent_control 	value 	0-100% control
// 0x03 	percent_state 	    value 	Report from motor about current percentage
// 0x04 	control_back     	enum 	Configures motor direction (untested)
// 0x05 	work_state       	enum 	Supposedly shows if motor is opening or closing, always 0 for me though
// 0x06 	situation_set 	    enum 	Configures if 100% equals to fully closed or fully open (untested)
// 0x07 	fault           	bitmap 	Anything but 0 means something went wrong (untested)

// Value for switch
//-------------------
// 0x01 	Button 1
// 0x02 	Button 2
// 0x03 	Button 3
// 0x04 	???
// 0x0D     All buttons

// Value for thermostat
//---------------------
// 0x04     Preset
// 0x6C     Auto / Manu
// 0x65     Manu / Off
// 0x6E     Low battery
// 0x02     Actual temperature
// 0x03     Thermostat temperature
// 0x14     Valve
// 0x15     Battery level
// 0x6A     Mode

// Value For Various sensor
// -------------------------
// 0x03     Presence detection (with 0x04)
// 0x65     Water leak (with 0x01)

//******************************************************************************************

/*! Returns true if the \p manufacturer name referes to a Tuya device. */
bool isTuyaManufacturerName(const QString &manufacturer)
{
    return manufacturer.startsWith(QLatin1String("_T")) && // quick check for performance
           std::regex_match(qPrintable(manufacturer), std::regex("_T[A-Z][A-Z0-9]{4}_[a-z0-9]{8}"));
}

// Tests for Tuya manufacturer name
/*
 Q_ASSERT(isTuyaManufacturerName("_TZ3000_bi6lpsew"));
 Q_ASSERT(isTuyaManufacturerName("_TYZB02_key8kk7r"));
 Q_ASSERT(isTuyaManufacturerName("_TYST11_ckud7u2l"));
 Q_ASSERT(isTuyaManufacturerName("_TYZB02_keyjqthh"));
 Q_ASSERT(!isTuyaManufacturerName("lumi.sensor_switch.aq2"));
*/

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

bool UseTuyaCluster(const QString &manufacturer)
{
    // https://docs.tuya.com/en/iot/device-development/module/zigbee-module/zigbeetyzs11module?id=K989rik5nkhez
    //_TZ3000 don't use tuya cluster
    //_TYZB01 don't use tuya cluster
    //_TYZB02 don't use tuya cluster
    //_TZ3400 don't use tuya cluster

    if (manufacturer.startsWith(QLatin1String("_TZE200_")) || // Tuya clutster visible
        manufacturer.startsWith(QLatin1String("Tuya_C_")) ||  // Used by fake device
        manufacturer.startsWith(QLatin1String("_TYST11_")) )  // Tuya cluster invisible
    {
        return true;
    }
    return false;
}


/*! Handle packets related to Tuya 0xEF00 cluster.
    \param ind the APS level data indication containing the ZCL packet
    \param zclFrame the actual ZCL frame which holds the scene cluster reponse

    Taken from https://medium.com/@dzegarra/zigbee2mqtt-how-to-add-support-for-a-new-tuya-based-device-part-2-5492707e882d
 */

void DeRestPluginPrivate::handleTuyaClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    if (zclFrame.isDefaultResponse())
    {
        return;
    }

    bool update = false;

    LightNode *lightNode = getLightNodeForAddress(ind.srcAddress(), ind.srcEndpoint());
    Sensor *sensorNode = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), ind.srcEndpoint());

    if ((!sensorNode) && (!lightNode))
    {
        return;
    }

    if (zclFrame.commandId() == 0x00)
    {
        // 0x00 : Used to send command, so not used here
    }
    else if (isXmasLightStrip(lightNode) &&
             zclFrame.commandId() == 0x01 &&
             !(zclFrame.frameControl() & deCONZ::ZclFCDisableDefaultResponse))
    {
        sendZclDefaultResponse(ind, zclFrame, deCONZ::ZclSuccessStatus);
    }
    else if ( (zclFrame.commandId() == 0x01) || (zclFrame.commandId() == 0x02) )
    {
        // 0x01 Used to inform of changes in its state.
        // 0x02 Send after receiving a 0x00 command.

        if (zclFrame.payload().size() < 7)
        {
            DBG_Printf(DBG_INFO, "Tuya : Payload too short");
            return;
        }

        QDataStream stream(zclFrame.payload());
        stream.setByteOrder(QDataStream::LittleEndian);

        // "dp" field describes the action/message of a command frame and was composed by a type and an identifier
        // Composed by a type (dp_type) and an identifier (dp_identifier), the identifier is device dependant.
        // "transid" is just a "counter", a response will have the same transif than the command.
        // "Status" and "fn" are always 0
        // More explanations at top of file

        quint8 status;
        quint8 transid;
        quint16 dp;
        quint8 fn;
        quint8 length = 0;
        qint32 data = 0;

        quint8 dp_type;
        quint8 dp_identifier;
        quint8 dummy;

        stream >> status;
        stream >> transid;
        stream >> dp;
        stream >> fn;

        //Convertion octet string to decimal value
        stream >> length;

        //security, it seem 4 is the maximum
        if (length > 4)
        {
            DBG_Printf(DBG_INFO, "Tuya : Schedule command\n");
        }
        else
        {
            for (; length > 0; length--)
            {
                stream >> dummy;
                data = data << 8;
                data = data + dummy;
            }
        }

        //To be more precise
        dp_identifier = (quint8) (dp & 0xFF);
        dp_type = (quint8) ((dp >> 8) & 0xFF);

        DBG_Printf(DBG_INFO, "Tuya debug 4 : Address 0x%016llX Payload %s\n" , ind.srcAddress().ext(), qPrintable(zclFrame.payload().toHex()));
        DBG_Printf(DBG_INFO, "Tuya debug 5 : Status: %d Transid: %d Dp: %d (0x%02X,0x%02X) Fn: %d Data %ld\n", status , transid , dp , dp_type, dp_identifier, fn , data);

        if (length > 4) //schedule command
        {

            // Monday = 64, Tuesday = 32, Wednesday = 16, Thursday = 8, Friday = 4, Saturday = 2, Sunday = 1
            // If you want your schedule to run only on workdays, the value would be W124. (64+32+16+8+4 = 124)
            // The API specifies 3 numbers, so a schedule that runs on Monday would be W064.
            // Workday = W124
            // Not working day = W003
            // Saturday = W002
            // Sunday = W001
            // All days = W127

            QString transitions;

            length = length / 3;

            if (zclFrame.payload().size() < (( length * 3) + 6) )
            {
                DBG_Printf(DBG_INFO, "Tuya : Schedule data error\n");
                return;
            }

            quint8 hour;
            quint8 minut;
            quint8 heatSetpoint;

            quint8 part = 1;
            QList<int> listday;

            if (dp == 0x0070) //work days (6)
            {
                listday << 124;
            }
            else if (dp == 0x00071) // holiday = Not working day (6)
            {
                listday << 003;
            }
            else if (dp == 0x0065) // Moe thermostat W124 (4) + W002 (4) + W001 (4)
            {
                part = length / 3;
                listday << 124 << 2 << 1;
            }

            for (; part > 0; part--)
            {
                for (; length > 0; length--)
                {
                    stream >> hour;
                    stream >> minut;
                    stream >> heatSetpoint;

                    transitions += QString("T%1:%2|%3")
                        .arg(hour, 2, 10, QChar('0'))
                        .arg(minut, 2, 10, QChar('0'))
                        .arg(heatSetpoint);

                    if (part > 0 && listday.size() >= static_cast<int>(part))
                    {
                        updateThermostatSchedule(sensorNode, listday.at(part - 1), transitions);
                    }

                }
            }

            return;
        }

        // Sensor and light use same cluster, so need to make a choice for device that have both
        // Some device have sensornode AND lightnode, so need to use the good one.
        if (sensorNode && lightNode)
        {
            if (dp == 0x0215) // battery
            {
                lightNode = nullptr;
            }

            if (sensorNode->type() == QLatin1String("ZHAThermostat"))
            {
                lightNode = nullptr;
            }
        }

        if (lightNode)
        {
            //Window covering ?
            if ((lightNode->manufacturer() == QLatin1String("_TYST11_wmcdj3aq")) ||
                (lightNode->manufacturer() == QLatin1String("_TZE200_xuzcvlku")) ||
                (lightNode->manufacturer() == QLatin1String("_TZE200_wmcdj3aq")) ||
                (lightNode->manufacturer() == QLatin1String("_TYST11_xu1rkty3")) )
            {

                switch (dp)
                {
                    // 0x0407 > starting moving
                    // 0x0105 > configuration done
                    case 0x0401:
                    {
                        if (data == 0x02) //open
                        {
                            lightNode->setValue(RStateOpen, true);
                            lightNode->setValue(RStateOn, false);
                        }
                        else if (data == 0x00) //close
                        {
                            lightNode->setValue(RStateOpen, false);
                            lightNode->setValue(RStateOn, true);
                        }
                        else if (data == 0x01) //stop
                        {
                        }
                    }
                    break;
                    case 0x0202: // going to position
                    case 0x0203: // position reached (more usefull I think)
                    {
                        quint8 lift = (quint8) data;
                        bool open = lift < 100;
                        lightNode->setValue(RStateLift, lift);
                        lightNode->setValue(RStateOpen, open);

                        quint8 level = lift * 254 / 100;
                        bool on = level > 0;
                        lightNode->setValue(RStateBri, level);
                        lightNode->setValue(RStateOn, on);
                    }
                    break;

                    //other
                    default:
                    break;

                }
            }
            else
            {
                // Switch device 1/2/3 gangs
                switch (dp)
                {
                    case 0x0101:
                    case 0x0102:
                    case 0x0103:
                    {

                        bool onoff = (data == 0) ? false : true;

                        {
                            uint ep = 0x01;
                            if (dp == 0x0102) { ep = 0x02; }
                            if (dp == 0x0103) { ep = 0x03; }

                            LightNode *lightNode2 = lightNode;
                            lightNode = getLightNodeForAddress(ind.srcAddress(), ep);

                            if (!lightNode)
                            {
                                return;
                            }

                            //Find model id if missing ( modelId().isEmpty ?) and complete it
                            if (lightNode->modelId().isNull() || (lightNode->modelId() == QLatin1String("Unknown")) || (lightNode->manufacturer() == QLatin1String("Unknown")))
                            {
                                DBG_Printf(DBG_INFO, "Tuya debug 10 : Updating model ID\n");
                                if (!lightNode2->modelId().isNull())
                                {
                                    lightNode->setModelId(lightNode2->modelId());
                                }
                                if (lightNode2->manufacturer().startsWith(QLatin1String("_T")))
                                {
                                    lightNode->setManufacturerName(lightNode2->manufacturer());
                                }
                            }

                            ResourceItem *item = lightNode->item(RStateOn);
                            if (item && item->toBool() != onoff)
                            {
                                item->setValue(onoff);
                                Event e(RLights, RStateOn, lightNode->id(), item);
                                enqueueEvent(e);

                                update = true;

                            }
                        }
                    }
                    break;

                    //other
                    default:
                    break;

                }
            }
        }
        else if (sensorNode)
        {
            switch (dp)
            {
                case 0x0068: // window open information
                {
                    quint8 valve = (quint8) (dp & 0xFF);
                    quint8 temperature = (quint8) ((dp >> 8) & 0xFF);
                    quint8 minute = (quint8) ((dp >> 16) & 0xFF);

                    DBG_Printf(DBG_INFO, "Tuya debug 9 : windows open info: %d %d %d" ,valve , temperature, minute );

                }
                break;
                case 0x0101: // off / running for Moe
                {
                    QString mode;
                    if      (data == 0) { mode = QLatin1String("off"); }
                    else if (data == 1) { mode = QLatin1String("heat"); }
                    else
                    {
                        return;
                    }

                    ResourceItem *item = sensorNode->item(RConfigMode);

                    if (item && item->toString() != mode)
                    {
                        item->setValue(mode);
                        enqueueEvent(Event(RSensors, RConfigMode, sensorNode->id(), item));
                    }
                }
                break;
                case 0x0107 : // Childlock status
                {
                    bool locked = (data == 0) ? false : true;
                    ResourceItem *item = sensorNode->item(RConfigLocked);

                    if (item && item->toBool() != locked)
                    {
                        item->setValue(locked);
                        Event e(RSensors, RConfigLocked, sensorNode->id(), item);
                        enqueueEvent(e);
                    }
                }
                break;
                case 0x0112 : // Window open status
                {
                    bool winopen = (data == 0) ? false : true;
                    ResourceItem *item = sensorNode->item(RConfigWindowOpen);

                    if (item && item->toBool() != winopen)
                    {
                        item->setValue(winopen);
                        Event e(RSensors, RConfigWindowOpen, sensorNode->id(), item);
                        enqueueEvent(e);
                    }
                }
                break;
                case 0x0114: // Valve state on / off
                {
                    bool onoff = false;
                    if (data == 1) { onoff = true; }

                    ResourceItem *item = sensorNode->item(RConfigSetValve);

                    if (item && item->toBool() != onoff)
                    {
                        item->setValue(onoff);
                        Event e(RSensors, RConfigSetValve, sensorNode->id(), item);
                        enqueueEvent(e);
                    }
                }
                break;
                case 0x0128 : // Childlock status for moe
                {
                    bool locked = (data == 0) ? false : true;
                    ResourceItem *item = sensorNode->item(RConfigLocked);

                    if (item && item->toBool() != locked)
                    {
                        item->setValue(locked);
                        Event e(RSensors, RConfigLocked, sensorNode->id(), item);
                        enqueueEvent(e);
                    }
                }
                break;
                case 0x0165: // off / on > [off = off, on = heat]
                {
                    QString mode;
                    if      (data == 0) { mode = QLatin1String("off"); }
                    else if (data == 1) { mode = QLatin1String("manu"); }
                    else
                    {
                        return;
                    }

                    ResourceItem *item = sensorNode->item(RConfigMode);

                    if ((item && item->toString() != mode) && (data == 0) ) // Only change if off
                    {
                        item->setValue(mode);
                        enqueueEvent(Event(RSensors, RConfigMode, sensorNode->id(), item));
                    }
                }
                break;
                case 0x0168: // Alarm
                {
                    bool alarm = false;
                    if (data == 1) { alarm = true; }

                    ResourceItem *item = sensorNode->item(RStateAlarm);

                    if (item && item->toBool() != alarm)
                    {
                        item->setValue(alarm);
                        Event e(RSensors, RStateAlarm, sensorNode->id(), item);
                        enqueueEvent(e);
                    }
                }
                break;
                case 0x016A: // Away mode
                {
                    //bool away = false;
                    //if (data == 1) { away = true; }
                }
                break;
                case 0x016c: // manual / auto
                {
                    QString mode;
                    if      (data == 0) { mode = QLatin1String("heat"); } // was "manu"
                    else if (data == 1) { mode = QLatin1String("auto"); } // back to "auto"
                    else
                    {
                        return;
                    }

                    ResourceItem *item = sensorNode->item(RConfigMode);

                    if (item && item->toString() != mode)
                    {
                        item->setValue(mode);
                        enqueueEvent(Event(RSensors, RConfigMode, sensorNode->id(), item));
                    }
                }
                break;
                case 0x016E: // Low battery
                {
                    bool bat = false;
                    if (data == 1) { bat = true; }

                    ResourceItem *item = sensorNode->item(RStateLowBattery);

                    if (item && item->toBool() != bat)
                    {
                        item->setValue(bat);
                        Event e(RSensors, RStateLowBattery, sensorNode->id(), item);
                        enqueueEvent(e);
                    }
                }
                break;
                case 0x0202: // Thermostat heatsetpoint
                {
                    qint16 temp = (static_cast<qint16>(data & 0xFFFF)) * 10;
                    ResourceItem *item = sensorNode->item(RConfigHeatSetpoint);

                    if (item && item->toNumber() != temp)
                    {
                        item->setValue(temp);
                        Event e(RSensors, RConfigHeatSetpoint, sensorNode->id(), item);
                        enqueueEvent(e);

                    }
                }
                break;
                case 0x0203: // Thermostat current temperature
                {
                    qint16 temp = (static_cast<qint16>(data & 0xFFFF)) * 10;
                    ResourceItem *item = sensorNode->item(RStateTemperature);

                    if (item && item->toNumber() != temp)
                    {
                        item->setValue(temp);
                        Event e(RSensors, RStateTemperature, sensorNode->id(), item);
                        enqueueEvent(e);

                    }
                }
                break;
                case 0x0210: // Thermostat heatsetpoint for moe
                {
                    qint16 temp = (static_cast<qint16>(data & 0xFFFF)) * 100;
                    ResourceItem *item = sensorNode->item(RConfigHeatSetpoint);

                    if (item && item->toNumber() != temp)
                    {
                        item->setValue(temp);
                        Event e(RSensors, RConfigHeatSetpoint, sensorNode->id(), item);
                        enqueueEvent(e);
                    }
                }
                break;
                case 0x0215: // battery
                {
                    quint8 bat = (static_cast<qint8>(data & 0xFF));
                    if (bat > 100) { bat = 100; }
                    ResourceItem *item = sensorNode->item(RConfigBattery);

                    if (!item && bat > 0) // valid value: create resource item
                    {
                        item = sensorNode->addItem(DataTypeUInt8, RConfigBattery);
                    }

                    if (item && item->toNumber() != bat)
                    {
                        item->setValue(bat);
                        Event e(RSensors, RConfigBattery, sensorNode->id(), item);
                        enqueueEvent(e);
                    }
                }
                break;
                case 0x0218: // Thermostat current temperature for moe
                {
                    qint16 temp = (static_cast<qint16>(data & 0xFFFF)) * 10;
                    ResourceItem *item = sensorNode->item(RStateTemperature);

                    if (item && item->toNumber() != temp)
                    {
                        item->setValue(temp);
                        Event e(RSensors, RStateTemperature, sensorNode->id(), item);
                        enqueueEvent(e);
                    }
                }
                break;
                case 0x022c : // temperature calibration (offset)
                {
                    qint16 temp = (static_cast<qint16>(data & 0xFFFF)) * 10;
                    ResourceItem *item = sensorNode->item(RConfigOffset);

                    if (item && item->toNumber() != temp)
                    {
                        item->setValue(temp);
                        Event e(RSensors, RConfigOffset, sensorNode->id(), item);
                        enqueueEvent(e);
                    }
                }
                break;
                case 0x0266: // min temperature limit
                {
                    //Can be Temperature for some device
                    if (sensorNode->modelId() == QLatin1String("GbxAXL2"))
                    {
                        qint16 temp = (static_cast<qint16>(data & 0xFFFF)) * 10;
                        ResourceItem *item = sensorNode->item(RStateTemperature);

                        if (item && item->toNumber() != temp)
                        {
                            item->setValue(temp);
                            Event e(RSensors, RStateTemperature, sensorNode->id(), item);
                            enqueueEvent(e);

                        }
                    }
                }
                break;
                case 0x0267: // max temperature limit
                {
                    //can be setpoint for some device
                    if (sensorNode->modelId() == QLatin1String("GbxAXL2"))
                    {
                        qint16 temp = (static_cast<qint16>(data & 0xFFFF)) * 10;
                        ResourceItem *item = sensorNode->item(RConfigHeatSetpoint);

                        if (item && item->toNumber() != temp)
                        {
                            item->setValue(temp);
                            Event e(RSensors, RConfigHeatSetpoint, sensorNode->id(), item);
                            enqueueEvent(e);

                        }
                    }
                }
                break;
                case 0x0269: // siren temperature, Boost time
                {
                    if (sensorNode->modelId() == QLatin1String("0yu2xgi")) //siren
                    {
                        qint16 temp = (static_cast<qint16>(data & 0xFFFF)) * 10;
                        ResourceItem *item = sensorNode->item(RStateTemperature);

                        if (item && item->toNumber() != temp)
                        {
                            item->setValue(temp);
                            Event e(RSensors, RStateTemperature, sensorNode->id(), item);
                            enqueueEvent(e);
                        }
                    }
                }
                break;
                case 0x026A : // Siren Humidity
                {
                    qint16 Hum = (static_cast<qint16>(data & 0xFFFF)) * 10;
                    ResourceItem *item = sensorNode->item(RStateHumidity);

                    if (item && item->toNumber() != Hum)
                    {
                        item->setValue(Hum);
                        Event e(RSensors, RStateHumidity, sensorNode->id(), item);
                        enqueueEvent(e);
                    }
                }
                break;
                case 0x026D : // Valve position
                {
                    quint8 valve = (static_cast<qint8>(data & 0xFF));
                    bool on = valve > 3;

                    ResourceItem *item = sensorNode->item(RStateOn);
                    if (item)
                    {
                        if (item->toBool() != on)
                        {
                            item->setValue(on);
                            enqueueEvent(Event(RSensors, RStateOn, sensorNode->id(), item));
                        }
                    }
                    item = sensorNode->item(RStateValve);
                    if (item && item->toNumber() != valve)
                    {
                        item->setValue(valve);
                        enqueueEvent(Event(RSensors, RStateValve, sensorNode->id(), item));
                    }
                }
                break;
                case 0x0402 : // preset for moe
                case 0x0403 : // preset for moe
                {
                    QString preset;
                    if (dp == 0x0402) { preset = QLatin1String("auto"); }
                    else if (dp == 0x0403) { preset = QLatin1String("program"); }
                    else
                    {
                        return;
                    }

                    ResourceItem *item = sensorNode->item(RConfigPreset);

                    if (item && item->toString() != preset)
                    {
                        item->setValue(preset);
                        enqueueEvent(Event(RSensors, RConfigPreset, sensorNode->id(), item));
                    }
                }
                break;
                case 0x0404 : // preset
                {
                    QString preset;
                    if (data == 0) { preset = QLatin1String("holiday"); }
                    else if (data == 1) { preset = QLatin1String("auto"); }
                    else if (data == 2) { preset = QLatin1String("manual"); }
                    else if (data == 3) { preset = QLatin1String("confort"); }
                    else if (data == 4) { preset = QLatin1String("eco"); }
                    else if (data == 5) { preset = QLatin1String("boost"); }
                    else if (data == 6) { preset = QLatin1String("complex"); }
                    else
                    {
                        return;
                    }

                    ResourceItem *item = sensorNode->item(RConfigPreset);

                    if (item && item->toString() != preset)
                    {
                        item->setValue(preset);
                        enqueueEvent(Event(RSensors, RConfigPreset, sensorNode->id(), item));
                    }
                }
                break;
                case 0x046a : // mode
                {
                    QString mode;
                    if (data == 0) { mode = QLatin1String("auto"); }
                    else if (data == 1) { mode = QLatin1String("heat"); }
                    else if (data == 2) { mode = QLatin1String("off"); }
                    else
                    {
                        return;
                    }

                    ResourceItem *item = sensorNode->item(RConfigMode);

                    if (item && item->toString() != mode)
                    {
                        item->setValue(mode);
                        enqueueEvent(Event(RSensors, RConfigMode, sensorNode->id(), item));
                    }
                }
                break;
                case 0x0569 : // Low battery
                {
                    bool bat = false;
                    if (data == 1) { bat = true; }

                    ResourceItem *item = sensorNode->item(RStateLowBattery);

                    if (item && item->toBool() != bat)
                    {
                        item->setValue(bat);
                        Event e(RSensors, RStateLowBattery, sensorNode->id(), item);
                        enqueueEvent(e);
                    }
                }
                break;

                default:
                break;
            }
        }
        else
        {
            DBG_Printf(DBG_INFO, "Tuya debug 6 : No device found");
        }

    }
    else
    {
        return;
    }

    if (update)
    {
        if (lightNode)
        {
            // Update Node light
            updateEtag(lightNode->etag);
            updateEtag(gwConfigEtag);
            lightNode->setNeedSaveDatabase(true);
            saveDatabaseItems |= DB_LIGHTS;
        }
        if (sensorNode)
        {
            // Update Node Sensor
            //updateEtag(sensorNode->etag);
            //updateEtag(gwConfigEtag);
            //sensorNode->setNeedSaveDatabase(true);
            //queSaveDb(DB_SENSORS, DB_SHORT_SAVE_DELAY);
        }
    }

}

bool DeRestPluginPrivate::SendTuyaRequestThermostatSetWeeklySchedule(TaskItem &taskRef, quint8 weekdays , QString transitions , qint8 Dp_identifier )
{
    QByteArray data;

    QStringList list = transitions.split("T", QString::SkipEmptyParts);

    quint8 hh;
    quint8 mm;
    quint8 heatSetpoint;

    if (Dp_identifier == 0x65)
    {
        //To finish
    }
    else
    {
        if (weekdays == 3)
        {
            Dp_identifier = 0x71;
        }
        if (list.size() != 6)
        {
            DBG_Printf(DBG_INFO, "Tuya : Schedule command error, need to have 6 values\n");
        }
    }

    for (const QString &entry : list)
    {
        QStringList attributes = entry.split("|");
        if (attributes.size() != 2)
        {
            return false;
        }
        hh = attributes.at(0).mid(0, 2).toUInt();
        mm = attributes.at(0).mid(3, 2).toUInt();
        heatSetpoint = attributes.at(1).toInt();

        data.append(QByteArray::number(hh,16));
        data.append(QByteArray::number(mm,16));
        data.append(QByteArray::number(heatSetpoint,16));

    }

    return SendTuyaRequest(taskRef, TaskThermostat , DP_TYPE_RAW , Dp_identifier , data );
}

//
// Tuya Devices
//
bool DeRestPluginPrivate::SendTuyaRequest(TaskItem &taskRef, TaskType taskType , qint8 Dp_type, qint8 Dp_identifier , QByteArray data )
{

    DBG_Printf(DBG_INFO, "Send Tuya Request: Dp_type: 0x%02X Dp_ identifier 0x%02X Data: %s\n", Dp_type, Dp_identifier , qPrintable(data.toHex()));

    TaskItem task;
    copyTaskReq(taskRef, task);

    //Tuya task
    task.taskType = taskType;

    task.req.setClusterId(TUYA_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(0x00); // Command 0x00
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand | deCONZ::ZclFCDirectionClientToServer);

    // payload
    QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    //Status always 0x00
    stream << (qint8) 0x00;
    //TransID , use 0
    stream << (qint8) 0x00;
    //Dp_indentifier
    stream << (qint8) Dp_identifier;
    //Dp_type
    stream << (qint8) Dp_type;
    //Fn , always 0
    stream << (qint8) 0x00;
    // Data
    stream << (qint8) data.length(); // length (can be 0 for Dp_identifier = enums)
    for (int i = 0; i < data.length(); i++)
    {
        stream << (quint8) data[i];
    }

    { // ZCL frame
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    if (addTask(task))
    {
        taskToLocalData(task);
    }
    else
    {
        return false;
    }

    processTasks();

    return true;
}
