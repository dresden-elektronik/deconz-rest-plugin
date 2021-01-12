/*
 * window_covering.cpp
 *
 *
 *
 *
 * ZigBee Home Automation Public Application Profile Document 05-3520-29
 * Chapter 9.3 Window Covering Cluster
 *
 * Cluster Id 0x0102 Window Covering Cluster
 * Attributes
 * 0x0000 enum8,      WindowCoveringType
 * 0x0003 unsinged16, CurrentPositionLift
 * 0x0004 unsinged16, CurrentPositionTilt
 * 0x0008 unsinged8,  CurrentPositionLiftPercentage
 * 0x0009 unsinged8,  CurrentPositionTiltPercentage
 * 0x000A bitmap8,    OperationalStatus (This attribute contains two bits which will be set while the motor is active)
 * 0x0011 unsinged16, InstalledClosedLimitLift (Specifies a bound for the bottom position (lift height), in centimeters)
 * 0x0013 unsinged16, InstalledClosedLimitTilt (Specifies a bound for the closed position (tilt angle), in units of 0.1°)
 * 0x0017 bitmap8,    Mode (bit0=if the motor direction is reversed, bit1=the device is in calibration, bit2=maintenance mode)
 *
 * Commands
 * 0x00 Move up/open, Move upwards, towards the fully open position.
 * 0x01 Move down/close, Move downwards, towards the fully closed position.
 * 0x02 Stop, Stop all motion.
 * 0x04 Go to Lift Value, Moves to the specified lift value. Unsigned 16-bit integer.
 * 0x05 Go to Lift Percentage, Moves to the specified lift percentage. Unsigned 8-bit integer.
 * 0x07 Go to Tilt Value, Move to the specified tilt value. Unsigned 16-bit integer.
 * 0x08 Go to Tilt Percentage, Move to the specified tilt percentage. Unsigned 8-bit integer.
 *
 *
 * Ubisys Shutter Control J1
 *
 * http://www.ubisys.de/downloads/ubisys-j1-technical-reference.pdf
 * page 18, chapter 7.2.5.1. Calibration
 *
 * Step 1
 *    In order to calibrate the device, first choose the appropriate device type.
 *    0 = Roller Shade Lift only, ..., 6 = Shutter Tilt only, ..., 8 Tilt Blind Lift & Tilt
 *    Write attribute 0x10F2:0x0000 (“WindowCoveringType”) accordingly.
 * Step 2
 *    Prepare calibration by setting these values:
 *    Write attribute 0x10F2:0x0010 (“InstalledOpenLimitLift”) as 0x0000 = 0cm.
 *    Write attribute 0x10F2:0x0011 (“InstalledClosedLimitLift”) as 0x00F0 = 240cm.
 *    Write attribute 0x10F2:0x0012 (“InstalledOpenLimitTilt”) as 0x0000 = 0°.
 *    Write attribute 0x10F2:0x0013 (“InstalledClosedLimitTilt”) as 0x0384 = 90.0°.
 *    Write attribute 0x10F2:0x1001 (“LiftToTiltTransitionSteps”) as 0xFFFF = invalid.
 *    Write attribute 0x10F2:0x1002 (“TotalSteps”) as 0xFFFF = invalid.
 *    Write attribute 0x10F2:0x1003 (“LiftToTiltTransitionSteps2”) as 0xFFFF = invalid.
 *    Write attribute 0x10F2:0x1004 (“TotalSteps2”) as 0xFFFF = invalid
 * Step 3
 *    Enter calibration mode:
 *    Write attribute 0x0017 (“Mode”) as 0x02.
 * Step 4
 *    Send the "move down" command and "stop" after a few centimeters.
 * Step 5
 *    Send the “move up” command. When the device reaches its top position,
 *    J1 will recognize the upper bound.
 * Step 6
 *    After J1 has reached the top position and the motor has stopped, send the “move down” command.
 * Step 7
 *    After J1 has reached the lower bound and the motor has stopped, send the “move up” command.
 *    J1 will search for the upper bound. Once the top position is reached,
 *    calibration of the total steps in both directions is complete.
 * Step 8
 *    In case of a tilt blind set attribute 0x10F2:0x1001 and 0x10F2:0x1003 to the time it takes for a lift-to tilt
 *    transition (down) or a tilt-to-lift transition (up), respectively. Otherwise proceed with the next step.
 * Step 9
 *    To leave calibration mode, clear bit #1 in the Mode attribute, e.g. write attribute 0x0017 as 0x00.
 */



#include "de_web_plugin.h"
#include "de_web_plugin_private.h"

int calibrationStep = 0;
int operationalStatus = 0;
TaskItem calibrationTask;

/*! Handle packets related to the ZCL Window Covering cluster.
    \param ind the APS level data indication containing the ZCL packet
    \param zclFrame the actual ZCL frame which holds the Window Covering command or attribute
 */
void DeRestPluginPrivate::handleWindowCoveringClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    // FIXME: You're only handling ZclReadAttributesResponse and ZclReportAttributes - no other commands
    //        why not call this from deCONZ::NodeEvent instead that has already parsed the payload

    Q_UNUSED(ind);

    LightNode *lightNode = getLightNodeForAddress(ind.srcAddress(), ind.srcEndpoint());

    if (!lightNode)
    {
        // was no relevant node
        return;
    }

    deCONZ::NumericUnion numericValue;
    quint16 attrid = 0x0000;
    quint8 attrTypeId = 0x00;
    quint8 attrValue = 0x00;
    quint8 status = 0x00;

    QDataStream stream(zclFrame.payload());
    stream.setByteOrder(QDataStream::LittleEndian);

    NodeValue::UpdateType updateType = NodeValue::UpdateInvalid;
    if (zclFrame.isProfileWideCommand() && zclFrame.commandId() == deCONZ::ZclReadAttributesResponseId)
    {
        updateType = NodeValue::UpdateByZclRead;
    }
    else if (zclFrame.isProfileWideCommand() && zclFrame.commandId() == deCONZ::ZclReportAttributesId)
    {
        updateType = NodeValue::UpdateByZclReport;
    }

    // Read ZCL reporting and ZCL Read Attributes Response
    if (updateType != NodeValue::UpdateInvalid)
    {
        while (!stream.atEnd())
        {
            stream >> attrid;
            if (updateType == NodeValue::UpdateByZclRead)
            {
                stream >> status;  // Read Attribute Response status
                if (status != 0)
                {
                    return;
                }
            }
            stream >> attrTypeId;
            switch (attrTypeId)
            {
                case deCONZ::Zcl8BitData:
                case deCONZ::ZclBoolean:
                case deCONZ::Zcl8BitBitMap:
                case deCONZ::Zcl8BitUint:
                case deCONZ::Zcl8BitInt:
                case deCONZ::Zcl8BitEnum:
                    stream >> attrValue;
                    break;
                case deCONZ::Zcl16BitData:
                case deCONZ::Zcl16BitBitMap:
                case deCONZ::Zcl16BitUint:
                case deCONZ::Zcl16BitInt:
                case deCONZ::Zcl16BitEnum:
                    quint16 attrVal16;
                    stream >> attrVal16;
                    break;
                default:
                    // unsupported data type
                    return;
            }

            NodeValue::UpdateType updateType = NodeValue::UpdateByZclReport;

            if (attrid == 0x0008) // current CurrentPositionLiftPercentage 0-100
            {
                // Update value in the GUI.
                numericValue.u8 = attrValue;
                lightNode->setZclValue(updateType, ind.srcEndpoint(), WINDOW_COVERING_CLUSTER_ID, attrid, numericValue);

                quint8 lift = attrValue;
                // Reverse value for Xiaomi curtain 
                if (lightNode->modelId().startsWith(QLatin1String("lumi.curtain")) || 
                   (lightNode->modelId() == QLatin1String("Motor Controller")) )
                {
                    lift = 100 - lift;
                }
                // Reverse value for Legrand but only for old value
                if ((lightNode->modelId() == QLatin1String("Shutter SW with level control")) ||
                    (lightNode->modelId() == QLatin1String("Shutter switch with neutral")) )
                {
                    bool bStatus = false;
                    uint nHex = lightNode->swBuildId().toUInt(&bStatus,16);
                    if (bStatus && (nHex < 28))
                    {
                        lift = 100 - lift;
                    }
                }
                // Reverse for some tuya covering
                if (lightNode->manufacturer() == QLatin1String("_TZ3000_egq7y6pr"))
                {
                    lift = 100 - lift;
                }

                bool open = lift < 100;

                if (lightNode->setValue(RStateLift, lift))
                {
                    pushZclValueDb(lightNode->address().ext(), lightNode->haEndpoint().endpoint(), WINDOW_COVERING_CLUSTER_ID, attrid, attrValue);
                }
                lightNode->setValue(RStateOpen, open);

                // FIXME: deprecate
                quint8 level = lift * 254 / 100;
                bool on = level > 0;
                lightNode->setValue(RStateBri, level);
                lightNode->setValue(RStateOn, on);
                // END FIXME: deprecate
            }
            else if (attrid == 0x0009) // current CurrentPositionTiltPercentage 0-100
            {
                numericValue.u8 = attrValue;
                lightNode->setZclValue(updateType, ind.srcEndpoint(), WINDOW_COVERING_CLUSTER_ID, attrid, numericValue);

                quint8 tilt = attrValue;
                if (lightNode->setValue(RStateTilt, tilt))
                {
                    pushZclValueDb(lightNode->address().ext(), lightNode->haEndpoint().endpoint(), WINDOW_COVERING_CLUSTER_ID, attrid, attrValue);
                }

                // FIXME: deprecate
                quint8 sat = attrValue * 254 / 100;
                lightNode->setValue(RStateSat, sat);
                // END FIXME: deprecate
            }
            else if (attrid == 0x000A)  // read attribute 0x000A OperationalStatus
            {
                if (calibrationStep != 0 && ind.srcAddress().ext() == calibrationTask.req.dstAddress().ext())
                {
                    operationalStatus = attrValue;
                }
            }
            else if (attrid == 0x0000)  // read attribute 0x0000 WindowConveringType
            {
                Sensor *sensor = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), 0x02);
                if (sensor)
                {
                    ResourceItem *item = sensor->item(RConfigWindowCoveringType);

                    if (item)
                    {
                        item->setValue(attrValue);
                        sensor->setNeedSaveDatabase(true);
                        queSaveDb(DB_SENSORS, DB_SHORT_SAVE_DELAY);
                    }
                }
            }
        }
    }
}

/*! Adds a window covering task to the queue.

   \param task - the task item
   \param cmdId - moveUp/Down/stop/moveTo/moveToPct
   \param pos - position centimeter
   \param pct - position percent
   \return true - on success
           false - on error
 */
bool DeRestPluginPrivate::addTaskWindowCovering(TaskItem &task, uint8_t cmd, uint16_t pos, uint8_t pct)
{
    task.taskType = TaskWindowCovering;

    task.req.setClusterId(WINDOW_COVERING_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(cmd);
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                                  deCONZ::ZclFCDirectionClientToServer |
                                  deCONZ::ZclFCDisableDefaultResponse);

    if (cmd == 0x04 || cmd == 0x05 || cmd == 0x07 || cmd == 0x08)
    { // payload
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        if (cmd == 0x04 || cmd == 0x07)
        {
            stream << pos;  // 16-bit moveToPosition
        }
        if (cmd == 0x05 || cmd == 0x08)
        {
            stream << pct;  // 8-bit moveToPct
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

bool DeRestPluginPrivate::addTaskWindowCoveringSetAttr(TaskItem &task, uint16_t mfrCode, uint16_t attrId, uint8_t attrType, uint16_t attrValue)
{
    DBG_Printf(DBG_INFO, "addTaskWindowCoveringSetAttr: mfrCode = 0x%04x, attrId = 0x%04x, attrType = 0x%02x, attrValue = 0x%04x\n", mfrCode, attrId, attrType, attrValue);

    task.taskType = TaskWindowCovering;

    task.req.setDstEndpoint(0x01);
    task.req.setClusterId(WINDOW_COVERING_CLUSTER_ID);
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

    { // payload
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream << (quint16) attrId;
        stream << (quint8) attrType;
        if (attrType == deCONZ::Zcl8BitEnum || attrType == deCONZ::Zcl8BitBitMap || attrType == deCONZ::Zcl8BitUint)
        {
            stream << (quint8) attrValue;
        }
        else if (attrType == deCONZ::Zcl16BitUint)
        {
            stream << (quint16) attrValue;
        }
        else
        {
            DBG_Printf(DBG_INFO, "unsupported attribute type 0x%04x\n", attrType);
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
    b.transitionTime = a.transitionTime;
    b.lightNode = a.lightNode;
    b.taskType = TaskWindowCovering;
    b.req.setClusterId(WINDOW_COVERING_CLUSTER_ID);
    b.req.setProfileId(HA_PROFILE_ID);
    b.zclFrame.payload().clear();
}

/*! Configures window covering cluster.
 * Creates Binding from Target Device Endpoint to Coordinator Endpoint
 * and configures Reporting on Cluster 0x0102 Attributes 0x0008, 0x0009, 0x000A.
 * and starts calibration on ubisys J1
 *
 * Value   WindowCoveringType      Capabilities
 * 0    Roller Shade                = Lift only
 * 1    Roller Shade two motors     = Lift only
 * 2    Roller Shade exterior       = Lift only
 * 3    Roller Shade two motors ext = Lift only
 * 4    Drapery                     = Lift only
 * 5    Awning                      = Lift only
 * 6    Shutter                     = Tilt only
 * 7    Tilt Blind Lift only        = Tilt only
 * 8    Tilt Blind lift & tilt      = Lift & Tilt
 * 9    Projector Screen            = Lift only

   \param task - the task item
   \parma deviceType - WindowCoveringType 0-9
   \return true - on success
           false - on error
 */
bool DeRestPluginPrivate::addTaskWindowCoveringCalibrate(TaskItem &taskRef, int WindowCoveringType)
{
    LightNode *lightNode = getLightNodeForAddress(taskRef.req.dstAddress(), 0x01);  // Endpoint 0x01

    if (lightNode)
    {
        if (WindowCoveringType == 6 || WindowCoveringType == 7 || WindowCoveringType == 8)
        {
            lightNode->addItem(DataTypeUInt8, RStateSat);  // add sat for Tilt
        }
        else
        {
            lightNode->removeItem(RStateSat);
        }
        lightNode->setNeedSaveDatabase(true);
        saveDatabaseItems |= DB_LIGHTS;
    }

    Sensor *sensor = getSensorNodeForAddressAndEndpoint(taskRef.req.dstAddress(), 0x02); // Endpoint 0x02

    if (!sensor || !sensor->modelId().startsWith(QLatin1String("J1")))
    {
        return false;
    }

    taskRef.req.setDstEndpoint(0x01);  // Window_Covering Server Cluster is on Endpoint 0x01

    TaskItem task;
    copyTaskReq(taskRef, task);
    copyTaskReq(taskRef, calibrationTask);

    // Create Binding
    BindingTask bt;

    bt.state = BindingTask::StateIdle;
    bt.action = BindingTask::ActionBind;
    bt.restNode = sensor; //task.lightNode;
    Binding &bnd = bt.binding;
    bnd.srcAddress = task.req.dstAddress().ext();
    bnd.dstAddrMode = deCONZ::ApsExtAddress;
    bnd.srcEndpoint = task.req.srcEndpoint();
    bnd.clusterId = WINDOW_COVERING_CLUSTER_ID;
    bnd.dstAddress.ext = apsCtrl->getParameter(deCONZ::ParamMacAddress);
    bnd.dstEndpoint = endpoint();

    if (bnd.dstEndpoint > 0) // valid gateway endpoint?
    {
        DBG_Printf(DBG_INFO_L2, "create binding for attribute reporting of cluster 0x%04X\n", WINDOW_COVERING_CLUSTER_ID);
        queueBindingTask(bt);
    }
    else
    {
        return false;
    }

    if (!bindingTimer->isActive())
    {
        bindingTimer->start();
    }

    // Configure Reporting on Cluster 0x0102 Attributes 0x0008, 0x0009, 0x000A
    ConfigureReportingRequest rq;

    rq.zclSeqNum = zclSeq++; // to match in configure reporting response handler
    rq.dataType = deCONZ::Zcl8BitUint;
    rq.attributeId = 0x0008; // CurrentPositionLiftPercentage
    rq.minInterval = 1;
    rq.maxInterval = 600;
    rq.reportableChange8bit = 1;

    ConfigureReportingRequest rq2;
    rq2.dataType = deCONZ::Zcl8BitUint;
    rq2.attributeId = 0x0009; // CurrentPositionTiltPercentage
    rq2.minInterval = 1;
    rq2.maxInterval = 600;
    rq2.reportableChange8bit = 1;

    ConfigureReportingRequest rq3;
    rq3.dataType = deCONZ::Zcl8BitBitMap;
    rq3.attributeId = 0x000A; // OperationalStatus
    rq3.minInterval = 1;
    rq3.maxInterval = 600;

    std::vector<ConfigureReportingRequest> out = {rq, rq2, rq3};

    DBG_Printf(DBG_INFO, "ubisys addTaskWindowCoveringCalibrate task4 deviceType = %d\n", WindowCoveringType);

    TaskItem task2;
    copyTaskReq(taskRef, task2);

    // ZDP Header
    task2.zclFrame.setSequenceNumber(zclSeq++);
    task2.zclFrame.setCommandId(deCONZ::ZclConfigureReportingId);
    task2.zclFrame.setFrameControl(deCONZ::ZclFCProfileCommand |
                                   deCONZ::ZclFCDirectionClientToServer |
                                   deCONZ::ZclFCDisableDefaultResponse);

    { // payload
        QDataStream stream(&task2.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        for (const ConfigureReportingRequest &rq : out)
        {
            stream << rq.direction;
            stream << rq.attributeId;
            stream << rq.dataType;
            stream << rq.minInterval;
            stream << rq.maxInterval;

            if (rq.reportableChange16bit != 0xFFFF)
            {
                stream << rq.reportableChange16bit;
            }
            else if (rq.reportableChange8bit != 0xFF)
            {
                stream << rq.reportableChange8bit;
            }
            else if (rq.reportableChange24bit != 0xFFFFFF)
            {
                stream << (qint8) (rq.reportableChange24bit & 0xFF);
                stream << (qint8) ((rq.reportableChange24bit >> 8) & 0xFF);
                stream << (qint8) ((rq.reportableChange24bit >> 16) & 0xFF);
            }
            else if (rq.reportableChange48bit != 0xFFFFFFFF)
            {
                stream << (qint8) (rq.reportableChange48bit & 0xFF);
                stream << (qint8) ((rq.reportableChange48bit >> 8) & 0xFF);
                stream << (qint8) ((rq.reportableChange48bit >> 16) & 0xFF);
                stream << (qint8) ((rq.reportableChange48bit >> 24) & 0xFF);
                stream << (qint8) 0x00;
                stream << (qint8) 0x00;
            }
            DBG_Printf(DBG_INFO_L2, "configure reporting for 0x%016llX, attribute 0x%04X/0x%04X\n", bt.restNode->address().ext(), bt.binding.clusterId, rq.attributeId);
        }
    }

    { // ZCL frame
        task2.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task2.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task2.zclFrame.writeToStream(stream);
    }

    if (!addTask(task2))
    {
        return false;
    }

    // Calibration Step 1 and Step 2
    TaskItem task3;
    copyTaskReq(taskRef, task3);

    task3.zclFrame.setSequenceNumber(zclSeq++);
    task3.zclFrame.setCommandId(deCONZ::ZclWriteAttributesId);
    task3.zclFrame.setFrameControl(deCONZ::ZclFCProfileCommand |
                                   deCONZ::ZclFCManufacturerSpecific |
                                   deCONZ::ZclFCDirectionClientToServer |
                                   deCONZ::ZclFCDisableDefaultResponse);
    task3.zclFrame.setManufacturerCode(0x10F2);  // Manufacturer code 0x10F2

    { // payload
        QDataStream stream(&task3.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        quint8 attrTypeId = deCONZ::Zcl16BitUint; // type-id 0x21 = unsigned16

        stream << (quint16) 0x0000;
        stream << (quint8) deCONZ::Zcl8BitEnum;
        stream << (quint8) WindowCoveringType; // WindowCoveringType attribute 0x0000

        stream << (quint16) 0x0010;
        stream << attrTypeId;
        stream << (quint16) 0x0000;  // Write attribute 0x10F2:0x0010 as 0x0000 = 0cm, typeid = 0x21

        stream << (quint16) 0x0011;
        stream << attrTypeId;
        stream << (quint16) 0x00F0;  // Write attribute 0x10F2:0x0011 as 0x00F0 = 240cm, typeid = 0x21

        stream << (quint16) 0x0012;
        stream << attrTypeId;
        stream << (quint16) 0x0000;  // Write attribute 0x10F2:0x0012 as 0x0000 = 0°, typeid = 0x21

        stream << (quint16) 0x0013;
        stream << attrTypeId;
        stream << (quint16) 0x0384;  // Write attribute 0x10F2:0x0013 as 0x0384 = 90.0°, typeid = 0x21

        stream << (quint16) 0x1001;
        stream << attrTypeId;
        stream << (quint16) 0xFFFF;  // Write attribute 0x10F2:0x1001 as 0xFFFF = invalid, typeid = 0x21

        stream << (quint16) 0x1002;
        stream << attrTypeId;
        stream << (quint16) 0xFFFF;  // Write attribute 0x10F2:0x1002 as 0xFFFF = invalid, typeid = 0x21

        stream << (quint16) 0x1003;
        stream << attrTypeId;
        stream << (quint16) 0xFFFF;  // Write attribute 0x10F2:0x1003 as 0xFFFF = invalid, typeid = 0x21

        stream << (quint16) 0x1004;
        stream << attrTypeId;
        stream << (quint16) 0xFFFF;  // Write attribute 0x10F2:0x1004 as 0xFFFF = invalid, typeid = 0x21
     }

    { // ZCL frame
        task3.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task3.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task3.zclFrame.writeToStream(stream);
    }

    if (!addTask(task3))
    {
        return false;
    }

    // Calibration Step 3
    TaskItem task4;
    copyTaskReq(taskRef, task4);

    task4.zclFrame.setSequenceNumber(zclSeq++);
    task4.zclFrame.setCommandId(deCONZ::ZclWriteAttributesId);
    task4.zclFrame.setFrameControl(deCONZ::ZclFCProfileCommand |
                                   deCONZ::ZclFCDirectionClientToServer |
                                   deCONZ::ZclFCDisableDefaultResponse);

    { // payload
        QDataStream stream(&task4.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        stream << (quint16) 0x0017;
        stream << (quint8) deCONZ::Zcl8BitBitMap;
        stream << (quint8) 0x02; // Write attribute Mode 0x0017 as 0x02, typeid = 0x18
    }

    { // ZCL frame
        task4.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task4.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task4.zclFrame.writeToStream(stream);
    }

    if (!addTask(task4))
    {
        return false;
    }

    // start timer for next step
    calibrationStep = 3;
    QTimer::singleShot(2000, this, SLOT(calibrateWindowCoveringNextStep()));

    return true;
}

void DeRestPluginPrivate::calibrateWindowCoveringNextStep()
{
    TaskItem task;
    copyTaskReq(calibrationTask, task);

    DBG_Printf(DBG_INFO, "ubisys NextStep calibrationStep = %d, task=%s calibrationTask = %s \n",
               calibrationStep,
               qPrintable(task.req.dstAddress().toStringExt()),
               qPrintable(calibrationTask.req.dstAddress().toStringExt()));

    switch(calibrationStep)
    {
    case 3:
        calibrationStep = 4;
        QTimer::singleShot(2000, this, SLOT(calibrateWindowCoveringNextStep()));
        addTaskWindowCovering(task, 0x01 /*move down*/, 0, 0);
        break;

    case 4:
        calibrationStep = 5;
        QTimer::singleShot(2000, this, SLOT(calibrateWindowCoveringNextStep()));
        addTaskWindowCovering(task, 0x00 /*move up*/, 0, 0);
        break;

    case 5:
        if (operationalStatus == 0)
        {
            calibrationStep = 6;
            addTaskWindowCovering(task, 0x01 /*move down*/, 0, 0);
        }
        QTimer::singleShot(4000, this, SLOT(calibrateWindowCoveringNextStep()));
        break;

    case 6:
        if (operationalStatus == 0)
        {
            calibrationStep = 7;
            addTaskWindowCovering(task, 0x00 /*move up*/, 0, 0);
        }
        QTimer::singleShot(4000, this, SLOT(calibrateWindowCoveringNextStep()));
        break;

    case 7:
        if (operationalStatus == 0)
        {
            calibrationStep = 8;
        }
        QTimer::singleShot(4000, this, SLOT(calibrateWindowCoveringNextStep()));
        break;

    case 8:
        if (operationalStatus == 0)
        {
            calibrationStep = 0;

            // leave calibration mode
            task.zclFrame.setSequenceNumber(zclSeq++);
            task.zclFrame.setCommandId(deCONZ::ZclWriteAttributesId);
            task.zclFrame.setFrameControl(deCONZ::ZclFCProfileCommand |
                                          deCONZ::ZclFCDirectionClientToServer |
                                          deCONZ::ZclFCDisableDefaultResponse);

            { // payload
                QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
                stream.setByteOrder(QDataStream::LittleEndian);

                stream << (quint16) 0x0017;
                stream << (quint8) deCONZ::Zcl8BitBitMap;
                stream << (quint8) 0x00; // Write attribute Mode 0x0017 as 0x00, typeid = 0x18
            }

            { // ZCL frame
                task.req.asdu().clear(); // cleanup old request data if there is any
                QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
                stream.setByteOrder(QDataStream::LittleEndian);
                task.zclFrame.writeToStream(stream);
            }

            addTask(task);

        }
        break;

    default:
        {
        }
        break;
    }
}
