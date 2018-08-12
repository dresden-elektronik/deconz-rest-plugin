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

/*! Handle packets related to the ZCL Window Covering cluster.
    \param ind the APS level data indication containing the ZCL packet
    \param zclFrame the actual ZCL frame which holds the Window Covering command or attribute
 */
void DeRestPluginPrivate::handleWindowCoveringClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    Q_UNUSED(ind);

    QDataStream stream(zclFrame.payload());
    stream.setByteOrder(QDataStream::LittleEndian);

    // Read ZCL reporting
    if (zclFrame.isProfileWideCommand() && zclFrame.commandId() == deCONZ::ZclReadAttributesResponseId)
    {


    }

    // More to do ...
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
    task.taskType = TaskStoreScene;

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
