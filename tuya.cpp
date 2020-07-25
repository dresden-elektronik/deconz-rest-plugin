/*
 * tuya.cpp
 *
 * Implementation of Tuya cluster.
 *
 */

#include "de_web_plugin.h"
#include "de_web_plugin_private.h"

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


/*! Handle packets related to Tuya 0xEF00 cluster.
    \param ind the APS level data indication containing the ZCL packet
    \param zclFrame the actual ZCL frame which holds the scene cluster reponse
    
    Taken from https://medium.com/@dzegarra/zigbee2mqtt-how-to-add-support-for-a-new-tuya-based-device-part-2-5492707e882d
 */
 
 // For Triple switch dp  = 257 258 259 (for on)
 // For thermostat dp = 514 (Changed temperature target after mode chnage)
 
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

    DBG_Printf(DBG_INFO, "Tuya : debug 8\n");
    
    if (zclFrame.commandId() == 0x00)
    {
        // 0x00 : Used to send command, so not used here
    }
    else if ( (zclFrame.commandId() == 0x01) || (zclFrame.commandId() == 0x02) )
    {
        // 0x01 Used to inform of changes in its state.
        // 0x02 Send after receiving a 0x00 command.
        
        DBG_Printf(DBG_INFO, "Tuya : debug 1 : size %d\n",static_cast<int>(zclFrame.payload().size()));
        
        if (zclFrame.payload().size() >= 7)
        {
            
            QDataStream stream(zclFrame.payload());
            stream.setByteOrder(QDataStream::LittleEndian);

            uint8_t status;
            uint8_t transid;
            int16_t dp;
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
                DBG_Printf(DBG_INFO, "Tuya debug : data length excess\n");
                length = 4;
            }
            
            for (; length > 0; length--)
            {
                stream >> dummy;
                data = data << 8;
                data = data + dummy;
            }

            DBG_Printf(DBG_INFO, "Tuya debug 4: status: %d transid: %d dp: %d fn: %d payload %s\n", status , transid , dp , fn ,  qPrintable(zclFrame.payload().toHex()));
            DBG_Printf(DBG_INFO, "Tuya debug 4: decimal value  %ld\n" , data );
            
            // Switch device 3 gang
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
                    
                        lightNode = getLightNodeForAddress(ind.srcAddress(), ep);

                        if (!lightNode)
                        {
                            return;
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
                case 0x0203: // Thermostat temperature
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
                
                default:
                break;
            }

        }
        else
        {
            DBG_Printf(DBG_INFO, "Tuya : debug 2");
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


// Tuya Devices
//
bool DeRestPluginPrivate::SendTuyaRequest(TaskItem &taskRef, TaskType taskType , qint16 Dp , QByteArray data )
{
    
    DBG_Printf(DBG_INFO, "Tuya debug 77\n");

    TaskItem task;
    copyTaskReq(taskRef, task);
    
    //Tuya task
    task.taskType = taskType;

    task.req.setClusterId(TUYA_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(0x00); // Command 0x00
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
            deCONZ::ZclFCDirectionClientToServer);

    // payload
    QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    
    //Status always 0x00
    stream << (qint8) 0x00;
    //TransID , use 0
    stream << (qint8) 0x00;
    //Dp
    stream << (qint16) Dp;
    //Fn , always 0
    stream << (qint8) 0x00;
    // Data
    stream << (qint8) data.length(); // len
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