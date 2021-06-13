/*
 * tuya.cpp
 *
 * Implementation of Tuya cluster.
 *
 */

#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "tuya.h"
#include "product_match.h"

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

// Value For Various sensor
// -------------------------
// 0x03     Presence detection (with 0x04)
// 0x65     Water leak (with 0x01)

// List of tuya command
// ---------------------
// Cmd ID       Description
// 0x01        Product Information Inquiry / Reporting
// 0x02        Device Status Query / Report
// 0x03        Zigbee Device Reset
// 0x04        Order Issuance
// 0x05        Status Report
// 0x06        Status Search
// 0x07        reserved
// 0x08        Zigbee Device Functional Test
// 0x09        Query key information (only scene switch devices are valid)
// 0x0A        Scene wakeup command (only scene switch device is valid)
// 0x0A-0x23   reserved
// 0x24        Time synchronization

//******************************************************************************************

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
        manufacturer.startsWith(QLatin1String("_TYST11_")))   // Tuya cluster invisible
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

    if (!sensorNode && !lightNode)
    {
        return;
    }
    
    QString productId;
    if (lightNode)
    {
        productId = R_GetProductId(lightNode);
    }
    else
    {
        productId = R_GetProductId(sensorNode);
    }
    
    // DBG_Printf(DBG_INFO, "Tuya debug 4 : Address 0x%016llX, Command 0x%02X, Payload %s\n", ind.srcAddress().ext(), zclFrame.commandId(), qPrintable(zclFrame.payload().toHex()));

    if (zclFrame.commandId() == TUYA_REQUEST)
    {
        // 0x00 : TUYA_REQUEST > Used to send command, so not used here
    }
    else if (zclFrame.commandId() == TUYA_REPORTING || zclFrame.commandId() == TUYA_QUERY)
    {
        // 0x01 : TUYA_REPORTING > Used to inform of changes in its state.
        // 0x02 : TUYA_QUERY > Send after receiving a 0x00 command.
        
        if (zclFrame.payload().size() < 7)
        {
            DBG_Printf(DBG_INFO, "Tuya : Payload too short\n");
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
        dp_identifier = (dp & 0xFF);
        dp_type = ((dp >> 8) & 0xFF);

        DBG_Printf(DBG_INFO, "Tuya debug 4 : Address 0x%016llX Payload %s\n", ind.srcAddress().ext(), qPrintable(zclFrame.payload().toHex()));
        DBG_Printf(DBG_INFO, "Tuya debug 5 : Status: %u Transid: %u Dp: %u (0x%02X,0x%02X) Fn: %u Data %ld\n", status, transid, dp, dp_type, dp_identifier, fn, data);

        if (length > 4) //schedule command
        {

            // Monday = 64, Tuesday = 32, Wednesday = 16, Thursday = 8, Friday = 4, Saturday = 2, Sunday = 1
            // If you want your schedule to run only on workdays, the value would be W124. (64+32+16+8+4 = 124)
            // The API specifies 3 numbers, so a schedule that runs on Monday would be W064.
            //
            // Workday = W124
            // Not working day = W003
            // Saturday = W002
            // Sunday = W001
            // All days = W127

            QString transitions;

            if (zclFrame.payload().size() < ((length * 3) + 6))
            {
                DBG_Printf(DBG_INFO, "Tuya : Schedule data error\n");
                return;
            }

            quint8 hour;
            quint8 minut;
            quint8 heatSetpoint;
            
            quint16 minut16;
            quint16 heatSetpoint16;

            quint8 part = 0;
            QList<int> listday;
            
            switch (dp)
            {
                case 0x0070: //work days (6)
                {
                    part = 1;
                    listday << 124;
                    length = length / 3;
                }
                break;
                case 0x0071: // holiday = Not working day (6)
                {
                    part = 1;
                    listday << 3;
                    length = length / 3;
                }
                break;
                case 0x0065: // Moe thermostat W124 (4) + W002 (4) + W001 (4)
                {
                    part = length / 3;
                    listday << 124 << 2 << 1;
                    length = length / 3;
                }
                break;
                // Daily schedule (mode 8)(minut 16)(temperature 16)(minut 16)(temperature 16)(minut 16)(temperature 16)(minut 16)(temperature 16)
                case 0x007B: // Sunday
                case 0x007C: // Monday
                case 0x007D: // Thuesday
                case 0x007E: // Wednesday
                case 0x007F: // Thursday
                case 0x0080: // Friday
                case 0x0081: // Saturday
                {
                    const std::array<int, 7> t = {1,64,32,46,8,4,2};
                    part = 1;
                    
                    if (dp < 0x007B || (dp - 0x007B) >= static_cast<int>(t.size()))
                    {
                        DBG_Printf(DBG_INFO, "Tuya unsupported daily schedule dp value: 0x%04X\n", dp);
                        return; // bail out early
                    }
                    
                    listday << t[dp - 0x007B];
                    
                    length = (length - 1) / 2;
                    
                    quint8 mode;
                    stream >> mode; // First octet is the mode
                    break;

                }
                default:
                {
                    DBG_Printf(DBG_INFO, "Tuya : Unknow Schedule mode\n");
                }
                break;
            }
            
            for (; part > 0; part--)
            {
                for (; length > 0; length--)
                {
                    if (dp >= 0x007B && dp <= 0x0081)
                    {
                        stream >> minut16;
                        stream >> heatSetpoint16;
                        hour = static_cast<quint8>((minut16 / 60) & 0xff);
                        minut = static_cast<quint8>((minut16 - 60 * hour) & 0xff);
                        heatSetpoint = static_cast<quint8>((heatSetpoint16 / 10) & 0xff);
                    }
                    else
                    {
                        stream >> hour;
                        stream >> minut;
                        stream >> heatSetpoint;
                    }

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
            if (productId == QLatin1String("NAS-AB02B0 Siren"))
            {
                if (dp == 0x0168) // Siren alarm
                {
                    sensorNode = nullptr;
                }
                else
                {
                    lightNode = nullptr;
                }
            }
        }

        //Some device are more than 1 sensors for the same endpoint, so trying to take the good one
        if (sensorNode && productId == QLatin1String("NAS-AB02B0 Siren"))
        {
            switch (dp)
            {
                //temperature
                case 0x0269:
                {
                    sensorNode = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), ind.srcEndpoint(), QLatin1String("ZHATemperature"));
                }
                break;
                //Humidity
                case 0x026A:
                {
                    sensorNode = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), ind.srcEndpoint(), QLatin1String("ZHAHumidity"));
                }
                break;
                default:
                // All other are for the alarm sensor
                {
                    sensorNode = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), ind.srcEndpoint(), QLatin1String("ZHAAlarm"));
                }
                break;
            }
        }

        if (lightNode)
        {
            //Window covering ?
            if (productId.startsWith(QLatin1String("Tuya_COVD")))
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
                        quint8 lift = static_cast<quint8>(data);
                        
                        // Need reverse
                        if (productId.startsWith(QLatin1String("Tuya_COVD YS-MT750")) ||
                            productId.startsWith(QLatin1String("Tuya_COVD DS82")))
                        {
                            lift = 100 - lift;
                        }
                        
                        bool open = lift < 100;
                        lightNode->setValue(RStateLift, lift);
                        lightNode->setValue(RStateOpen, open);

                        quint8 level = lift * 254 / 100;
                        bool on = level > 0;
                        lightNode->setValue(RStateBri, level);
                        lightNode->setValue(RStateOn, on);
                    }
                    break;
                    case 0x0405: // rotation direction
                    {
                        DBG_Printf(DBG_INFO, "Tuya debug 3 : Covering motor direction %ld\n", data);
                    }
                    break;

                    //other
                    default:
                    break;

                }
            }
            //siren
            else if (productId == QLatin1String("NAS-AB02B0 Siren"))
            {
                if (dp == 0x0168)
                {
                    if (data == 0x00)
                    {
                        lightNode->setValue(RStateAlert, QString("none"));
                    }
                    else
                    {
                        lightNode->setValue(RStateAlert, QString("lselect"));
                    }

                     update = true;
                }
            }
            else
            {
                // Switch device 1/2/3 gangs or dimmer
                switch (dp)
                {
                    // State
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

                            //Find model id if missing (modelId().isEmpty ?) and complete it
                            if (lightNode->modelId().isNull() || lightNode->modelId() == QLatin1String("Unknown") || lightNode->manufacturer() == QLatin1String("Unknown"))
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
                    
                    // Dimmer level for mode 1
                    case 0x0202:
                    {
                        if (productId == QLatin1String("Tuya_DIMSWITCH Earda Dimmer") ||
                            productId == QLatin1String("Tuya_DIMSWITCH EDM-1ZAA-EU"))
                        {
                            const qint64 bri = data * 254 / 1000; // 0 to 1000 value
                            
                            ResourceItem *item = lightNode->item(RStateBri);
                            if (item && item->toNumber() != bri)
                            {
                                item->setValue(bri);
                                Event e(RLights, RStateBri, lightNode->id(), item);
                                enqueueEvent(e);
                                update = true;
                            }
                        }
                    }
                    break;
                    // Dimmer level for mode 2
                    case 0x0203:
                    {
                        if (productId == QLatin1String("Tuya_DIMSWITCH Not model found yet"))
                        {
                            const qint64 bri = data * 254 / 1000; // 0 to 1000 value
                            
                            ResourceItem *item = lightNode->item(RStateBri);
                            if (item && item->toNumber() != bri)
                            {
                                item->setValue(bri);
                                Event e(RLights, RStateBri, lightNode->id(), item);
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
            //Special part just for siren
            if (productId == QLatin1String("NAS-AB02B0 Siren")) //siren
            {
                switch (dp)
                {
                    case 0x0171: // Alarm siren temperature
                    {
                        ResourceItem *item = sensorNode->item(RConfigPreset);
                        
                        if (item)
                        {
                            QString mode;
                            if (data == 0)
                            {
                                if (item->toString() == "both")
                                {
                                    mode = QLatin1String("humidity");
                                }
                                else
                                {
                                    mode = QLatin1String("off");
                                }
                            }
                            else if (data == 1)
                            {
                                if (item->toString() == "humidity")
                                {
                                    mode = QLatin1String("both");
                                }
                                else
                                {
                                    mode = QLatin1String("temperature");
                                }
                            }
                            else
                            {
                                return;
                            }

                            if (item->toString() != mode)
                            {
                                update = true;
                                
                                item->setValue(mode);
                                Event e(RSensors, RConfigPreset, sensorNode->id(), item);
                                enqueueEvent(e);
                            }
                        }
                    }
                    break;
                    case 0x0172: // Alarm siren humidity
                    {
                        ResourceItem *item = sensorNode->item(RConfigPreset);
                        
                        if (item)
                        {
                            QString mode;
                            if (data == 0)
                            {
                                if (item->toString() == "both")
                                {
                                    mode = QLatin1String("temperature");
                                }
                                else
                                {
                                    mode = QLatin1String("off");
                                }
                            }
                            else if (data == 1)
                            {
                                if (item->toString() == "temperature")
                                {
                                    mode = QLatin1String("both");
                                }
                                else
                                {
                                    mode = QLatin1String("humidity");
                                }
                            }
                            else
                            {
                                return;
                            }

                            if (item->toString() != mode)
                            {
                                update = true;
                                
                                item->setValue(mode);
                                Event e(RSensors, RConfigPreset, sensorNode->id(), item);
                                enqueueEvent(e);
                            }
                        }
                    }
                    break;
                    case 0x0269: // siren temperature
                    {
                        qint16 temp = static_cast<qint16>(data & 0xFFFF) * 10 + 200;
                        ResourceItem *item = sensorNode->item(RStateTemperature);

                        if (item && item->toNumber() != temp)
                        {
                            item->setValue(temp);
                            Event e(RSensors, RStateTemperature, sensorNode->id(), item);
                            enqueueEvent(e);
                            update = true;
                        }

                    }
                    break;
                    case 0x026A : // Siren Humidity
                    {
                        qint16 Hum = static_cast<qint16>(data & 0xFFFF) * 100;
                        ResourceItem *item = sensorNode->item(RStateHumidity);

                        if (item && item->toNumber() != Hum)
                        {
                            item->setValue(Hum);
                            Event e(RSensors, RStateHumidity, sensorNode->id(), item);
                            enqueueEvent(e);
                            update = true;
                        }
                    }
                    break;
                    case 0x026B : // min alarm temperature threshold
                    {
                        qint8 min = static_cast<qint8>(data & 0xFF);
                        ResourceItem *item = sensorNode->item(RConfigTempMinThreshold);

                        if (item && item->toNumber() != min)
                        {
                            item->setValue(min);
                            Event e(RSensors, RConfigTempMinThreshold, sensorNode->id(), item);
                            enqueueEvent(e);
                            
                            update = true;
                        }
                    }
                    break;
                    case 0x026C : // max alarm temperature threshold
                    {
                        qint8 max = static_cast<qint8>(data & 0xFF);
                        ResourceItem *item = sensorNode->item(RConfigTempMaxThreshold);

                        if (item && item->toNumber() != max)
                        {
                            item->setValue(max);
                            Event e(RSensors, RConfigTempMaxThreshold, sensorNode->id(), item);
                            enqueueEvent(e);
                            
                            update = true;
                        }
                    }
                    break;
                    case 0x026D : // min alarm humidity threshold
                    {
                        qint8 min = static_cast<qint8>(data & 0xFF);
                        ResourceItem *item = sensorNode->item(RConfigHumiMinThreshold);

                        if (item && item->toNumber() != min)
                        {
                            item->setValue(min);
                            Event e(RSensors, RConfigHumiMinThreshold, sensorNode->id(), item);
                            enqueueEvent(e);
                            
                            update = true;
                        }
                    }
                    break;
                    case 0x026E : // max alarm humidity threshold
                    {
                        qint8 max = static_cast<qint8>(data & 0xFF);
                        ResourceItem *item = sensorNode->item(RConfigHumiMaxThreshold);

                        if (item && item->toNumber() != max)
                        {
                            item->setValue(max);
                            Event e(RSensors, RConfigHumiMaxThreshold, sensorNode->id(), item);
                            enqueueEvent(e);
                            
                            update = true;
                        }
                    }
                    break;
                    case 0x0466 : // melody
                    {
                        quint8 melody = static_cast<qint8>(data & 0xFF);

                        ResourceItem *item = sensorNode->item(RConfigMelody);

                        if (item && item->toNumber() != melody)
                        {
                            item->setValue(melody);
                            enqueueEvent(Event(RSensors, RConfigMelody, sensorNode->id(), item));
                            update = true;
                        }
                        
                    }
                    break;
                    case 0x0474 : // volume
                    {
                        quint8 volume = static_cast<qint8>(data & 0xFF);

                        ResourceItem *item = sensorNode->item(RConfigVolume);

                        if (item && item->toNumber() != volume)
                        {
                            item->setValue(volume);
                            enqueueEvent(Event(RSensors, RConfigVolume, sensorNode->id(), item));
                            update = true;
                        }
                    }
                    break;
                    default:
                    break;
                }
            }
            else
            {
                // Generic part
                switch (dp)
                {
                    case 0x0068: // window open information
                    {
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
                    case 0x0114: // Valve state report : on / off
                    {
                        bool onoff = false;
                        if (data == 1) { onoff = true; }

                        ResourceItem *item = sensorNode->item(RConfigSetValve);

                        if (item && item->toBool() != onoff)
                        {
                            item->setValue(onoff);
                            Event e(RSensors, RConfigSetValve, sensorNode->id(), item);
                            enqueueEvent(e);
                            update = true;
                        }
                    }
                    break;
                    case 0x011E :
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
                    case 0x0165: // off / on > [off = off, on = heat] for Saswell devices
                    {
                        QString mode;
                        if      (data == 0) { mode = QLatin1String("off"); }
                        else if (data == 1) { mode = QLatin1String("manu"); }
                        else
                        {
                            return;
                        }

                        ResourceItem *item = sensorNode->item(RConfigMode);

                        if (item && item->toString() != mode && data == 0) // Only change if off
                        {
                            item->setValue(mode);
                            enqueueEvent(Event(RSensors, RConfigMode, sensorNode->id(), item));
                        }
                    }
                    break;
                    case 0x016A: // Away mode for Saswell
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
                            update = true;
                        }
                    }
                    break;
                    case 0x0202: // Thermostat heatsetpoint
                    {
                        qint16 temp = static_cast<qint16>(data & 0xFFFF) * 10;
                        ResourceItem *item = sensorNode->item(RConfigHeatSetpoint);

                        if (item && item->toNumber() != temp)
                        {
                            item->setValue(temp);
                            enqueueEvent(Event(RSensors, RConfigHeatSetpoint, sensorNode->id(), item));

                        }
                    }
                    break;
                    case 0x0203: // Thermostat current temperature
                    {
                        qint16 temp = static_cast<qint16>(data & 0xFFFF) * 10;
                        ResourceItem *item = sensorNode->item(RStateTemperature);

                        if (item && item->toNumber() != temp)
                        {
                            item->setValue(temp);
                            Event e(RSensors, RStateTemperature, sensorNode->id(), item);
                            enqueueEvent(e);
                            update = true;
                        }
                    }
                    break;
                    case 0x0210: // Thermostat heatsetpoint for moe
                    {
                        qint16 temp = static_cast<qint16>(data & 0xFFFF) * 100;
                        
                        if (productId == "Tuya_THD MOES TRV")
                        {
                            temp = static_cast<qint16>(data & 0xFFFF) * 100 / 2;
                        }
                        
                        ResourceItem *item = sensorNode->item(RConfigHeatSetpoint);

                        if (item && item->toNumber() != temp)
                        {
                            item->setValue(temp);
                            Event e(RSensors, RConfigHeatSetpoint, sensorNode->id(), item);
                            enqueueEvent(e);
                            update = true;
                        }
                    }
                    break;
                    case 0x0215: // battery
                    {
                        quint8 bat = static_cast<qint8>(data & 0xFF);
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
                        qint16 temp = static_cast<qint16>(data & 0xFFFF) * 10;
                        ResourceItem *item = sensorNode->item(RStateTemperature);

                        if (item && item->toNumber() != temp)
                        {
                            item->setValue(temp);
                            Event e(RSensors, RStateTemperature, sensorNode->id(), item);
                            enqueueEvent(e);
                            update = true;
                        }
                    }
                    break;
                    case 0x022c : // temperature calibration (offset in degree)
                    {
                        qint16 temp = static_cast<qint16>(data & 0xFFFF) * 10;
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
                        if (productId == "Tuya_THD SEA801-ZIGBEE TRV" ||
                            productId == "Tuya_THD Smart radiator TRV" ||
                            productId == "Tuya_THD WZB-TRVL TRV")
                        {
                            qint16 temp = static_cast<qint16>(data & 0xFFFF) * 10;
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
                        if (productId == "Tuya_THD SEA801-ZIGBEE TRV" ||
                            productId == "Tuya_THD Smart radiator TRV" ||
                            productId == "Tuya_THD WZB-TRVL TRV")
                        {
                            qint16 temp = static_cast<qint16>(data & 0xFFFF) * 10;
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
                    case 0x0269: // Boost time in second or Heatpoint
                    {
                        if (productId == "Tuya_THD MOES TRV")
                        {
                            qint16 temp = static_cast<qint16>(data & 0xFFFF) * 100 / 2;
   
                            ResourceItem *item = sensorNode->item(RConfigHeatSetpoint);

                            if (item && item->toNumber() != temp)
                            {
                                item->setValue(temp);
                                Event e(RSensors, RConfigHeatSetpoint, sensorNode->id(), item);
                                enqueueEvent(e);
                                update = true;
                            }
                        }
                    }
                    break;
                    case 0x026D : // Valve position in %
                    {
                        quint8 valve = static_cast<qint8>(data & 0xFF);
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
                    case 0x0402 : // preset for moe or mode
                    case 0x0403 : // preset for moe
                    {
                        if (productId == "Tuya_THD MOES TRV")
                        {
                            QString mode;
                            if (data == 0) { mode = QLatin1String("auto"); } //schedule
                            else if (data == 1) { mode = QLatin1String("heat"); } //manual
                            else if (data == 2) { mode = QLatin1String("off"); } //away
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
                        else
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
                    }
                    break;
                    case 0x0404 : // preset
                    {
                        QString preset;
                        if (data == 0) { preset = QLatin1String("holiday"); }
                        else if (data == 1) { preset = QLatin1String("auto"); }
                        else if (data == 2) { preset = QLatin1String("manual"); }
                        else if (data == 3) { preset = QLatin1String("comfort"); }
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
                    case 0x046a : // Force mode : normal/open/close
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
        }
        else
        {
            DBG_Printf(DBG_INFO, "Tuya debug 6 : No device found\n");
        }

    }
    // Time sync command
    //https://developer.tuya.com/en/docs/iot/device-development/embedded-software-development/mcu-development-access/zigbee-general-solution/tuya-zigbee-module-uart-communication-protocol
    else if (zclFrame.commandId() == TUYA_TIME_SYNCHRONISATION)
    {
        DBG_Printf(DBG_INFO, "Tuya debug 1 : Time sync request\n");

        QDataStream stream(zclFrame.payload());
        stream.setByteOrder(QDataStream::LittleEndian);

        quint16 unknowHeader;

        stream >> unknowHeader;

        // This is disabled for the moment, need investigations
        // It seem some device send a UnknowHeader = 0x0000
        // it s always 0x0000 for device > gateway
        // And always 0x0008 for gateway > device (0x0008 is the payload size)
        //
        //if (unknowHeader == 0x0000)
        //{
        //}

        quint32 timeNow = 0xFFFFFFFF;              // id 0x0000 Time
        qint32 timeZone = 0xFFFFFFFF;              // id 0x0002 TimeZone
        quint32 timeDstStart = 0xFFFFFFFF;        // id 0x0003 DstStart
        quint32 timeDstEnd = 0xFFFFFFFF;          // id 0x0004 DstEnd
        qint32 timeDstShift = 0xFFFFFFFF;         // id 0x0005 DstShift
        quint32 timeStdTime = 0xFFFFFFFF;         // id 0x0006 StandardTime
        quint32 timeLocalTime = 0xFFFFFFFF;       // id 0x0007 LocalTime

        getTime(&timeNow, &timeZone, &timeDstStart, &timeDstEnd, &timeDstShift, &timeStdTime, &timeLocalTime, UNIX_EPOCH);
        
        QByteArray data;
        QDataStream stream2(&data, QIODevice::WriteOnly);
        stream2.setByteOrder(QDataStream::LittleEndian);
        
        //Add the "magic value"
        stream2 << unknowHeader;
        
        //change byter order
        stream2.setByteOrder(QDataStream::BigEndian);
        
         // Add UTC time
        stream2 << timeNow;
        // Ad local time
        stream2 << timeLocalTime;

        sendTuyaCommand(ind, TUYA_TIME_SYNCHRONISATION, data);

        return;
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
            updateLightEtag(&*lightNode);
            lightNode->setNeedSaveDatabase(true);
            saveDatabaseItems |= DB_LIGHTS;
        }
        if (sensorNode)
        {
            updateSensorEtag(&*sensorNode);

            sensorNode->updateStateTimestamp();
            enqueueEvent(Event(RSensors, RStateLastUpdated, sensorNode->id()));

            sensorNode->setNeedSaveDatabase(true);
        }
    }

}

bool DeRestPluginPrivate::sendTuyaRequestThermostatSetWeeklySchedule(TaskItem &taskRef, quint8 weekdays, const QString &transitions, qint8 Dp_identifier)
{
    QByteArray data;

    const QStringList list = transitions.split("T", QString::SkipEmptyParts);

    quint8 hh;
    quint8 mm;
    quint8 heatSetpoint;

    if (Dp_identifier == DP_IDENTIFIER_THERMOSTAT_SCHEDULE_1)
    {
        //To finish
    }
    else if (Dp_identifier == DP_IDENTIFIER_THERMOSTAT_SCHEDULE_4)
    {
        //To finish
    }
    else
    {
        if (weekdays == 3)
        {
            Dp_identifier = DP_IDENTIFIER_THERMOSTAT_SCHEDULE_3;
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
        hh = attributes.at(0).midRef(0, 2).toUInt();
        mm = attributes.at(0).midRef(3, 2).toUInt();
        heatSetpoint = attributes.at(1).toInt();

        data.append(QByteArray::number(hh, 16));
        data.append(QByteArray::number(mm, 16));
        data.append(QByteArray::number(heatSetpoint, 16));
    }

    return sendTuyaRequest(taskRef, TaskThermostat, DP_TYPE_RAW, Dp_identifier, data);
}

//
// Tuya Devices
//
bool DeRestPluginPrivate::sendTuyaRequest(TaskItem &taskRef, TaskType taskType, qint8 Dp_type, qint8 Dp_identifier, const QByteArray &data)
{
    DBG_Printf(DBG_INFO, "Send Tuya request: Dp_type: 0x%02X, Dp_identifier 0x%02X, data: %s\n", Dp_type, Dp_identifier, qPrintable(data.toHex()));
    
    const quint8 seq = zclSeq++;

    TaskItem task;
    copyTaskReq(taskRef, task);

    //Tuya task
    task.taskType = taskType;

    task.req.setClusterId(TUYA_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(seq);
    task.zclFrame.setCommandId(0x00); // Command 0x00
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand | deCONZ::ZclFCDirectionClientToServer | deCONZ::ZclFCDisableDefaultResponse);

    // payload
    QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    stream << static_cast<qint8>(0x00);          // Status always 0x00
    stream << static_cast<qint8>(seq);           // TransID, use seq
    stream << static_cast<qint8>(Dp_identifier); // Dp_indentifier
    stream << static_cast<qint8>(Dp_type);       // Dp_type
    stream << static_cast<qint8>(0x00);          // Fn, always 0
    // Data
    stream << static_cast<qint8>(data.length()); // length (can be 0 for Dp_identifier = enums)
    for (int i = 0; i < data.length(); i++)
    {
        stream << static_cast<quint8>(data[i]);
    }

    { // ZCL frame
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    if (!addTask(task))
    {
        return false;
    }

    processTasks();

    return true;
}

bool DeRestPluginPrivate::sendTuyaCommand(const deCONZ::ApsDataIndication &ind, qint8 commandId, const QByteArray &data)
{
    DBG_Printf(DBG_INFO, "Send Tuya command 0x%02X, data: %s\n", commandId, qPrintable(data.toHex()));

    TaskItem task;

    //Tuya task
    task.taskType = TaskTuyaRequest;

    task.req.dstAddress() = ind.srcAddress();
    task.req.setDstAddressMode(deCONZ::ApsExtAddress);
    task.req.setDstEndpoint(ind.srcEndpoint());
    task.req.setSrcEndpoint(endpoint());
    task.req.setClusterId(TUYA_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(commandId); // Command
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCDisableDefaultResponse);

    // payload
    QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    // Data
    for (int i = 0; i < data.length(); i++)
    {
        stream << static_cast<quint8>(data[i]);
    }

    { // ZCL frame
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    if (!addTask(task))
    {
        DBG_Printf(DBG_INFO, "Failed to send Tuya command 0x%02X, data: %s\n", commandId, qPrintable(data.toHex()));
        return false;
    }

    processTasks();

    return true;
} 
