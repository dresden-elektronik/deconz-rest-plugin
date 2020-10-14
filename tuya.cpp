/*
 * tuya.cpp
 *
 * Implementation of Tuya cluster.
 *
 */

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

bool UseTuyaCluster(QString manufacturer)
{
    // https://docs.tuya.com/en/iot/device-development/module/zigbee-module/zigbeetyzs11module?id=K989rik5nkhez
    //_TZ3000 don't use tuya cluster
    //_TYZB01 don't use tuya cluster
    
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

        uint8_t status;
        uint8_t transid;
        int16_t dp;
        uint8_t dp_type;
        uint8_t dp_identifier;
        uint8_t fn;
        qint32 data = 0;
        quint8 dummy;
        quint8 length = 0;
        
        stream >> status;
        stream >> transid;
        stream >> dp;
        stream >> fn;

        //Convertion octet string to decimal value
        stream >> length;
        
        //security, it seem 4 is the maximum
        if (length > 4)
        {
            DBG_Printf(DBG_INFO, "Tuya : data length excess, not managed yet\n");
            length = 4;
        }
        
        for (; length > 0; length--)
        {
            stream >> dummy;
            data = data << 8;
            data = data + dummy;
        }
        
        //To be more precise
        dp_identifier = (uint8_t) (dp & 0xFF);
        dp_type = (uint8_t) ((dp >> 8) & 0xFF);

        DBG_Printf(DBG_INFO, "Tuya debug 4 : Address 0x%016llX Payload %s\n" , ind.srcAddress().ext(), qPrintable(zclFrame.payload().toHex()));
        DBG_Printf(DBG_INFO, "Tuya debug 5 : Status: %d Transid: %d Dp: %d (0x%02X,0x%02X) Fn: %d Data %ld\n", status , transid , dp , dp_type, dp_identifier, fn , data);
        
        //Sensor and light use same cluster, so need to make a choice for device that have both
        if ((sensorNode) && (lightNode))
        {
            if (dp == 0x0215) // battery
            {
                lightNode = nullptr;
            }
        }

        if (lightNode)
        {
            //Window covering ?
            if ((lightNode->manufacturer() == QLatin1String("_TYST11_wmcdj3aq")) ||
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
                            if (lightNode->modelId().isNull() || (lightNode->manufacturer() == QLatin1String("Unknown")))
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
                    uint8_t valve = (uint8_t) (dp & 0xFF);
                    uint8_t temperature = (uint8_t) ((dp >> 8) & 0xFF);
                    uint8_t minute = (uint8_t) ((dp >> 16) & 0xFF);
                    
                    DBG_Printf(DBG_INFO, "Tuya debug 9 : windows open info: %d %d %d" ,valve , temperature, minute );
                    
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
                    
                    ResourceItem *item = sensorNode->item(RStateOn);

                    if (item && item->toBool() != onoff)
                    {
                        item->setValue(onoff);
                        Event e(RSensors, RStateOn, sensorNode->id(), item);
                        enqueueEvent(e);
                    }
                }
                break;
                case 0x016c: // manual / auto
                {
                    QString mode;
                    if (data == 0) { mode = "heat"; } // was "manu"
                    if (data == 1) { mode = "auto"; } // back to "auto"
                    
                    ResourceItem *item = sensorNode->item(RConfigMode);

                    if (item && item->toString() != mode)
                    {
                        item->setValue(mode);
                        enqueueEvent(Event(RSensors, RConfigMode, sensorNode->id(), item));
                    }
                }
                break;
                case 0x0165: // off / on
                {
                    QString mode;
                    if (data == 0) { mode = "off"; }
                    if (data == 1) { mode = "manu"; }
                    
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
                    qint16 temp = ((qint16)(data & 0xFFFF)) * 10;
                    ResourceItem *item = sensorNode->item(RConfigHeatSetpoint);

                    if (item && item->toNumber() != temp)
                    {
                        item->setValue(temp);
                        Event e(RSensors, RConfigHeatSetpoint, sensorNode->id(), item);
                        enqueueEvent(e);
                        
                    }
                }
                break;
                case 0x0203: // Thermostat temperature
                case 0x0269: // siren temperature
                {
                    qint16 temp = ((qint16)(data & 0xFFFF)) * 10;
                    ResourceItem *item = sensorNode->item(RStateTemperature);

                    if (item && item->toNumber() != temp)
                    {
                        item->setValue(temp);
                        Event e(RSensors, RStateTemperature, sensorNode->id(), item);
                        enqueueEvent(e);

                    }
                }
                break;
                case 0x0215: // battery
                {
                    quint8 bat = (qint8)(data & 0xFF);
                    if (bat > 100) { bat = 100; }
                    ResourceItem *item = sensorNode->item(RConfigBattery);

                    if (item && item->toNumber() != bat)
                    {
                        item->setValue(bat);
                        Event e(RSensors, RConfigBattery, sensorNode->id(), item);
                        enqueueEvent(e);
                    }
                }
                break;
                case 0x022c : // temperature calibration (offset)
                {
                    qint16 temp = ((qint16)(data & 0xFFFF)) * 10;
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
                        qint16 temp = ((qint16)(data & 0xFFFF)) * 10;
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
                        qint16 temp = ((qint16)(data & 0xFFFF)) * 10;
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
                case 0x026A : // Siren Humidity
                {
                    qint16 RStateHumidity = ((qint16)(data & 0xFFFF)) * 10;
                    ResourceItem *item = sensorNode->item(RStateTemperature);

                    if (item && item->toNumber() != RStateHumidity)
                    {
                        item->setValue(RStateHumidity);
                        Event e(RSensors, RStateTemperature, sensorNode->id(), item);
                        enqueueEvent(e);
                    }
                }
                break;
                case 0x026D : // Valve position
                {
                    quint8 valve = (qint8)(data & 0xFF);
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
                case 0x0404 : // preset
                {
                    QString preset;
                    if (data == 0) { preset = "holiday"; }
                    if (data == 1) { preset = "auto"; }
                    if (data == 2) { preset = "manual"; }
                    if (data == 3) { preset = "confort"; }
                    if (data == 4) { preset = "eco"; }
                    if (data == 5) { preset = "boost"; }
                    if (data == 6) { preset = "complex"; }
                    
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
                    if (data == 0) { mode = "auto"; }
                    if (data == 1) { mode = "heat"; }
                    if (data == 2) { mode = "off"; }
                    
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