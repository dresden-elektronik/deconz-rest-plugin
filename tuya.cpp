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

    LightNode *lightNode = getLightNodeForAddress(ind.srcAddress(), ind.srcEndpoint());
    Sensor *sensorNode = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), ind.srcEndpoint());

    if ((!sensorNode) && (!lightNode))
    {
        return;
    }

    DBG_Printf(DBG_INFO, "Tuya : debug 8\n");
    
    if (zclFrame.commandId() == 0x00)
    {
        // 0x00 : Used to send command
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
            quint32 data = 0;
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
                DBG_Printf(DBG_INFO, "Tuya debug : lenght excess\n");
                length = 4;
            }
            
            for (; length > 0; length--)
            {
                stream >> dummy;
                data = data << 8;
                data = data + dummy;
            }
            
            DBG_Printf(DBG_INFO, "Tuya debug 4: status: %d transid: %d dp: %d fn: %d\n", status , transid , dp , fn );
            DBG_Printf(DBG_INFO, "Tuya debug 5: data:  %lld\n",  data );
            
            // Switch device 3 gang
            switch (dp)
            {
                case 0x0101:
                case 0x0102:
                case 0x0103:
                {
                    bool onoff = false;
                    if (data == 1) { onoff = true; }
                    
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

                            // Update Node light
                            updateEtag(lightNode->etag);
                            updateEtag(gwConfigEtag);
                            lightNode->setNeedSaveDatabase(true);
                            saveDatabaseItems |= DB_LIGHTS;
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
                        
                        //updateSensorEtag(sensorNode->etag);

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
                        
                        //updateSensorEtag(sensorNode->etag);
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
                case 0x404 : // mode
                {
                    QString mode;
                    if (data == 0) { mode = "off"; }
                    if (data == 1) { mode = "auto"; }
                    if (data == 2) { mode = "manual"; }
                    
                    ResourceItem *item = sensorNode->item(RConfigMode);

                    if (item && item->toString() != mode)
                    {
                        item->setValue(mode);
                        Event e(RSensors, RConfigMode, sensorNode->id(), item);
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
    for (uint i = 0; i < data.length(); i++)
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