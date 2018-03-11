/*
 * Copyright (c) 2016 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QString>
#include <QVariantMap>
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "colorspace.h"

/** @brief Min of A and B */
#define MIN(A,B)	(((A) <= (B)) ? (A) : (B))

/** @brief Max of A and B */
#define MAX(A,B)	(((A) >= (B)) ? (A) : (B))

/** @brief Min of A, B, and C */
#define MIN3(A,B,C)	(((A) <= (B)) ? MIN(A,C) : MIN(B,C))

/** @brief Max of A, B, and C */
#define MAX3(A,B,C)	(((A) >= (B)) ? MAX(A,C) : MAX(B,C))

/*! Add a MoveLevel task to the queue

    \param task - the task item
    \param withOnOff - if true command is send with on/off
    \param upDirection - true is up, false is down
    \param rate - the move rate
    \return true - on success
            false - on error
 */
bool DeRestPluginPrivate::addTaskMoveLevel(TaskItem &task, bool withOnOff, bool upDirection, quint8 rate)
{
    task.taskType = TaskMoveLevel;

    task.req.setClusterId(LEVEL_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    if (rate > 0)
    {
        if (withOnOff)
        {
            task.zclFrame.setCommandId(0x05); // move level with on/off
        }
        else
        {
            task.zclFrame.setCommandId(0x01); // move level
        }
    }
    else
    {
        task.zclFrame.setCommandId(0x03); // stop
    }

    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCDisableDefaultResponse);


    if (rate > 0)
    { // payload
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        quint8 direction = upDirection ? 0x00 : 0x01;
        stream << direction;
        stream << rate;
    }

    { // ZCL frame
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    return addTask(task);
}

/*!
 * Add a OnOff task to the queue
 *
 * \param task - the task item
 * \param cmd - command to send ONOFF_COMMAND_ON, ONOFF_COMMAND_OFF, ONOFF_COMMAND_TOGGLE or ONOFF_COMMAND_ON_WITH_TIMED_OFF
 * \param ontime - ontime, used only for command ONOFF_COMMAND_ON_WITH_TIMED_OFF
 * \return true - on success
 *         false - on error
 */
bool DeRestPluginPrivate::addTaskSetOnOff(TaskItem &task, quint8 cmd, quint16 ontime)
{
    DBG_Assert(cmd == ONOFF_COMMAND_ON || cmd == ONOFF_COMMAND_OFF || cmd == ONOFF_COMMAND_TOGGLE || cmd == ONOFF_COMMAND_ON_WITH_TIMED_OFF);
    if (!(cmd == ONOFF_COMMAND_ON || cmd == ONOFF_COMMAND_OFF || cmd == ONOFF_COMMAND_TOGGLE || cmd == ONOFF_COMMAND_ON_WITH_TIMED_OFF))
    {
        return false;
    }
    task.taskType = TaskSendOnOffToggle;
    task.onOff = cmd;

    task.req.setClusterId(ONOFF_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(cmd);
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCDisableDefaultResponse);

    if (cmd == ONOFF_COMMAND_ON_WITH_TIMED_OFF)
    {
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream << (quint8)0x80; // 0x01 accept only when on --> no, 0x80 overwrite ontime (yes, non standard)
        stream << ontime;
        stream << (quint16)0; // off wait time
    }


    task.req.asdu().clear(); // cleanup old request data if there is any
    QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    task.zclFrame.writeToStream(stream);

    return addTask(task);
}

/*!
 * Add a brightness task to the queue
 *
 * \param task - the task item
 * \param bri - brightness level 0..255
 * \param withOnOff - also set onOff state
 * \return true - on success
 *         false - on error
 */
bool DeRestPluginPrivate::addTaskSetBrightness(TaskItem &task, uint8_t bri, bool withOnOff)
{
    task.taskType = TaskSetLevel;
    task.level = bri;
    task.req.setClusterId(LEVEL_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    if (withOnOff)
    {
        task.zclFrame.setCommandId(0x04); // Move to level (with on/off)
    }
    else
    {
        task.zclFrame.setCommandId(0x00); // Move to level
    }
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCDisableDefaultResponse);

    { // payload
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        stream << task.level;
        stream << task.transitionTime;
    }

    { // ZCL frame
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    return addTask(task);
}

/*!
 * Add a color temperature increase task to the queue
 *
 * \param task - the task item
 * \param ct - step size -65534 ..65534
 * \return true - on success
 *         false - on error
 */
bool DeRestPluginPrivate::addTaskIncColorTemperature(TaskItem &task, int32_t ct)
{
    task.taskType = TaskIncColorTemperature;

    task.inc = ct;
    task.req.setClusterId(COLOR_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(0x4C); // Step color temperature

    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCDisableDefaultResponse);

    quint8 direction = ct > 0 ? 1 : 3; // up, down
    quint16 stepSize = ct > 0 ? ct : -ct;

    { // payload
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        stream << direction;
        stream << stepSize;
        stream << task.transitionTime;
        stream << (quint16)0; // min dummy
        stream << (quint16)0; // max dummy
    }

    { // ZCL frame
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    return addTask(task);
}

/*!
 * Add a stop brightness task to the queue
 *
 * \param task - the task item
 * \return true - on success
 *         false - on error
 */
bool DeRestPluginPrivate::addTaskStopBrightness(TaskItem &task)
{
    task.taskType = TaskStopLevel;
    task.req.setClusterId(LEVEL_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(0x03); // Stop

    task.zclFrame.payload().clear();
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCDisableDefaultResponse);

    { // ZCL frame
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    return addTask(task);
}

/*!
 * Add a set color temperature task to the queue
 *
 * \param task - the task item
 * \param ct - mired color temperature
 * \return true - on success
 *         false - on error
 */
bool DeRestPluginPrivate::addTaskSetColorTemperature(TaskItem &task, uint16_t ct)
{
    // Workaround for interim FLS-H
    // which does not support the color temperature ZCL command
    if (task.lightNode && (task.lightNode->manufacturerCode() == VENDOR_ATMEL) && (task.lightNode->modelId() == "FLS-H"))
    {
        float ctMin = 153;
        float ctMax = 500;
        float sat = ((float)ct - ctMin) / (ctMax - ctMin) * 254;
        if (sat > 254)
        {
            sat = 254;
        }

        bool ret = addTaskSetSaturation(task, sat);
        // overwrite for later use
        task.taskType = TaskSetColorTemperature;
        task.colorTemperature = ct;
        if (task.lightNode)
        {
            task.lightNode->setColorMode("ct");
        }
        return ret;
    }

    task.taskType = TaskSetColorTemperature;
    task.colorTemperature = ct;

    if (task.lightNode)
    {
        task.lightNode->setColorMode("ct");
    }

    task.req.setClusterId(COLOR_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(0x0a); // Move to color temperature
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCDisableDefaultResponse);

    { // payload
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        stream << task.colorTemperature;
        stream << task.transitionTime;
    }

    { // ZCL frame
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    return addTask(task);
}

/*!
 * Add a set enhanced hue task to the queue
 *
 * \param task - the task item
 * \param hue - brightness level 0..65535
 * \return true - on success
 *         false - on error
 */
bool DeRestPluginPrivate::addTaskSetEnhancedHue(TaskItem &task, uint16_t hue)
{
    task.taskType = TaskSetEnhancedHue;
    task.hueReal = (double)hue / (360.0f * 182.04444f);

    if (task.lightNode)
    {
        task.lightNode->setColorMode("hs");
    }

    if (task.hueReal < 0.0f)
    {
        task.hueReal = 0.0f;
    }
    else if (task.hueReal > 1.0f)
    {
        task.hueReal = 1.0f;
    }
    task.hue = task.hueReal * 254.0f;
//    task.enhancedHue = hue * 360.0f * 182.04444f;
    task.enhancedHue = hue;

    task.req.setClusterId(COLOR_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(0x40); // Enhanced move to hue
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCDisableDefaultResponse);

    { // payload
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        uint8_t direction = 0x00;
        stream << task.enhancedHue;
        stream << direction;
        stream << task.transitionTime;
    }

    { // ZCL frame
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    return addTask(task);
}

/*!
 * Add a set saturation task to the queue
 *
 * \param task - the task item
 * \param sat - brightness level 0..255
 * \return true - on success
 *         false - on error
 */
bool DeRestPluginPrivate::addTaskSetSaturation(TaskItem &task, uint8_t sat)
{
    task.taskType = TaskSetSat;
    task.sat = sat;

    if (task.lightNode)
    {
        task.lightNode->setColorMode("hs");
    }

    if (sat == 255)
    {
        sat = 254;
    }

    task.req.setClusterId(COLOR_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(0x03); // Move to saturation
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCDisableDefaultResponse);

    { // payload
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        stream << task.sat;
        stream << task.transitionTime;
    }

    { // ZCL frame
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    return addTask(task);
}

/*!
 * Add a set hue and saturation task to the queue
 *
 * \param task - the task item
 * \param sat - brightness level 0..254
 * \param hue - hue 0..254
 * \return true - on success
 *         false - on error
 */
bool DeRestPluginPrivate::addTaskSetHueAndSaturation(TaskItem &task, uint8_t hue, uint8_t sat)
{
    task.taskType = TaskSetHueAndSaturation;
    task.sat = sat;
    task.hue = hue;
    task.hueReal = hue / 254.0f;
    task.enhancedHue = task.hueReal * 360.0f * 182.04444f;

    if (task.lightNode)
    {
        task.lightNode->setColorMode("hs");
    }

    if (sat == 255)
    {
        sat = 254;
    }

    task.req.setClusterId(COLOR_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(0x06); // Move to hue and saturation
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCDisableDefaultResponse);

    { // payload
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        stream << task.hue;
        stream << task.sat;
        stream << task.transitionTime;
    }

    { // ZCL frame
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    return addTask(task);
}

/*!
 * Add a set xy color task but as hue and saturation task to the queue
 *
 * \param task - the task item
 * \param x - normalized x coordinate  0.0 .. 1.0
 * \param y - normalized y coordinate  0.0 .. 1.0
 * \return true - on success
 *         false - on error
 */
bool DeRestPluginPrivate::addTaskSetXyColorAsHueAndSaturation(TaskItem &task, double x, double y)
{
    num r, g, b;
    num h, s, v;
    num X, Y, Z;

    // prevent division through zero
    if (x <= 0.0) {
        x = 0.00000001f;
    }

    // prevent division through zero
    if (y <= 0.0) {
        y = 0.00000001f;
    }

    if (task.lightNode)
    {
        ResourceItem *item = task.lightNode->item(RStateBri);
        if (item)
        {
            Y = task.lightNode->level() / 255.0f;
        }
        else
        {
            Y = 1.0f;
        }
    }
    else {
        Y = 1.0f;
    }
    X = (Y / y) * x;
    Z = (Y / y) * (1.0f - x - y);

    num min = MIN3(X,Y,Z);
    if (min < 0)
    {
        X += min;
        Y += min;
        Z += min;
    }

    num max = MAX3(X,Y,Z);
    if (max > 1)
    {
        X /= max;
        Y /= max;
        Z /= max;
    }

    DBG_Printf(DBG_INFO, "xy = (%f, %f), XYZ = (%f, %f, %f)\n",x, y,  X, Y, Z);

    r = (num)( 3.2406*X - 1.5372*Y - 0.4986*Z);
    g = (num)(-0.9689*X + 1.8758*Y + 0.0415*Z);
    b = (num)( 0.0557*X - 0.2040*Y + 1.0570*Z);

    // clip
    if (r > 1) r = 1;
    if (r < 0) r = 0;
    if (g > 1) g = 1;
    if (g < 0) g = 0;
    if (b > 1) b = 1;
    if (b < 0) b = 0;

    Rgb2Hsv(&h, &s, &v, r, g, b);

    // normalize
    h /= 360.0f;

    if (h > 1.0f)
    {
        h = 1.0f;
    }
    else if (h < 0.0f)
    {
        h = 0.0f;
    }

    uint8_t hue = h * 254.0f;
    uint8_t sat = s * 254.0f;

    return addTaskSetHueAndSaturation(task, hue, sat);
}

/*!
 * Add a set xy color task to the queue
 *
 * \param task - the task item
 * \param x - normalized x coordinate  0.0 .. 1.0
 * \param y - normalized y coordinate  0.0 .. 1.0
 * \return true - on success
 *         false - on error
 */
bool DeRestPluginPrivate::addTaskSetXyColor(TaskItem &task, double x, double y)
{
    task.taskType = TaskSetXyColor;
    task.colorX = x * 65279.0f; // current X in range 0 .. 65279
    task.colorY = y * 65279.0f; // current Y in range 0 .. 65279

    if (task.lightNode)
    {
        task.lightNode->setColorMode("xy");

        // convert xy coordinates to hue and saturation
        // due the lights itself don't support this mode yet
        if (task.lightNode->manufacturerCode() == VENDOR_ATMEL)
        {
            task.lightNode->setColorXY(task.colorX, task.colorY); // update here
            return addTaskSetXyColorAsHueAndSaturation(task, x, y);
        }
    }

    task.req.setClusterId(COLOR_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(0x07); // Move to color
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCDisableDefaultResponse);

    { // payload
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        stream << task.colorX;
        stream << task.colorY;
        stream << task.transitionTime;
    }

    { // ZCL frame
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    return addTask(task);
}

/*!
 * Add a set color loop task to the queue
 *
 * \param task - the task item
 * \param colorLoopActive - wherever the color loop shall be activated
 * \param speed - time in seconds for a whole color loop cycle
 * \return true - on success
 *         false - on error
 */
bool DeRestPluginPrivate::addTaskSetColorLoop(TaskItem &task, bool colorLoopActive, uint8_t speed)
{
    task.colorLoop = colorLoopActive;
    task.taskType = TaskSetColorLoop;

    if (task.lightNode && colorLoopActive)
    {
        task.lightNode->setColorMode("hs");
    }

    task.req.setClusterId(COLOR_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(0x44); // Color loop set
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCDisableDefaultResponse);

    { // payload
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        uint8_t updateFlags = 0x07; // update action 0x1, direction 0x02, time 0x04
        uint8_t action = colorLoopActive ? 0x02 /* activate color loop from current hue */
                                         : 0x00 /* stop color loop */;
        uint8_t direction = 0x01; // up
        uint16_t time = speed; // seconds to go through whole color loop. default: 15
        uint16_t startHue = 0; // start hue

        stream << updateFlags;
        stream << action;
        stream << direction;
        stream << time;
        stream << startHue;
    }

    { // ZCL frame
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    return addTask(task);
}

/*! Add a identify task to the queue.

   \param task - the task item
   \param identifyTime - the time in seconds to identify
   \return true - on success
           false - on error
 */
bool DeRestPluginPrivate::addTaskIdentify(TaskItem &task, uint16_t identifyTime)
{
    task.taskType = TaskIdentify;
    task.identifyTime = identifyTime;

    task.req.setClusterId(IDENTIFY_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(0x00); // Identify
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCDisableDefaultResponse);

    { // payload
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        stream << task.identifyTime;
    }

    { // ZCL frame
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    return addTask(task);
}

/*! Add a trigger effect task to the queue.

   \param task - the task item
   \param effectIdentifier - the effect
   \return true - on success
           false - on error
 */
bool DeRestPluginPrivate::addTaskTriggerEffect(TaskItem &task, uint8_t effectIdentifier)
{
    task.taskType = TaskTriggerEffect;
    task.effectIdentifier = effectIdentifier;

    task.req.setClusterId(IDENTIFY_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(0x40); // Trigger Effect
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCDisableDefaultResponse);

    { // payload
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        stream << task.effectIdentifier;
        stream << 0x00; // default effectVariant
    }

    { // ZCL frame
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    return addTask(task);
}

/*! Add a warning task to the queue.

   \param task - the task item
   \param options - the options
   \param duration - the duration
   \return true - on success
           false - on error
 */
bool DeRestPluginPrivate::addTaskWarning(TaskItem &task, uint8_t options, uint16_t duration)
{
    task.taskType = TaskWarning;
    task.options = options;
    task.duration = duration;
    uint8_t strobe_duty_cycle = 10;
    uint8_t strobe_level = 0;

    task.req.setClusterId(IAS_WD_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(0x00); // Start Warning
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCDisableDefaultResponse);

    { // payload
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        stream << task.options;
        stream << task.duration;
        stream << strobe_duty_cycle;
        stream << strobe_level;
    }

    { // ZCL frame
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    return addTask(task);
}

/*! Add a add to group task to the queue.

   \param task - the task item
   \param groupId - the group to which a node shall be added
   \return true - on success
           false - on error
 */
bool DeRestPluginPrivate::addTaskAddToGroup(TaskItem &task, uint16_t groupId)
{
    task.taskType = TaskAddToGroup;
    task.groupId = groupId;

    task.req.setClusterId(GROUP_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(0x00); // Add to group
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCDisableDefaultResponse);

    { // payload
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        stream << task.groupId;
        uint8_t cstrlen = 0;
        stream << cstrlen; // mandatory parameter
    }

    { // ZCL frame
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    return addTask(task);
}

/*! Add a view group task to the queue.

   \param task - the task item
   \param groupId - the group which shall be viewed
   \return true - on success
           false - on error
 */
bool DeRestPluginPrivate::addTaskViewGroup(TaskItem &task, uint16_t groupId)
{
    task.taskType = TaskViewGroup;
    task.groupId = groupId;

    task.req.setClusterId(GROUP_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(0x01); // View group
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCDisableDefaultResponse);

    { // payload
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        stream << task.groupId;
    }

    { // ZCL frame
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    return addTask(task);
}

/*! Add a remove from group task to the queue.

   \param task - the task item
   \param groupId - the group from which a node shall be removed
   \return true - on success
           false - on error
 */
bool DeRestPluginPrivate::addTaskRemoveFromGroup(TaskItem &task, uint16_t groupId)
{
    task.taskType = TaskRemoveFromGroup;
    task.groupId = groupId;

    task.req.setClusterId(GROUP_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(0x03); // Remove from group
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCDisableDefaultResponse);

    { // payload
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        stream << task.groupId;
    }

    { // ZCL frame
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    return addTask(task);
}

/*! Adds a store scene task to the queue.

   \param task - the task item
   \param groupId - the group to which the scene belongs
   \param sceneId - the scene which shall be added
   \return true - on success
           false - on error
 */
bool DeRestPluginPrivate::addTaskStoreScene(TaskItem &task, uint16_t groupId, uint8_t sceneId)
{
    task.taskType = TaskStoreScene;

    task.req.setClusterId(SCENE_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(0x04); // store scene
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCDisableDefaultResponse);

    { // payload
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        stream << groupId;
        stream << sceneId;
    }

    { // ZCL frame
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    DBG_Printf(DBG_INFO, "add store scene task, aps-req-id: %u\n", task.req.id());
    return addTask(task);
}

/*! Adds a add scene task to the queue.

   \param task - the task item
   \param groupId - the group to which the scene belongs
   \param sceneId - the scene which shall be added
   \return true - on success
           false - on error
 */
bool DeRestPluginPrivate::addTaskAddEmptyScene(TaskItem &task, quint16 groupId, quint8 sceneId, quint16 transitionTime)
{
    task.taskType = TaskAddScene;
    task.groupId = groupId;
    task.sceneId = sceneId;
    task.transitionTime = transitionTime;
    task.req.setClusterId(SCENE_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);

    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                                  deCONZ::ZclFCDirectionClientToServer |
                                  deCONZ::ZclFCDisableDefaultResponse);

    { // payload
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        if (transitionTime >= 10)
        {
            task.zclFrame.setCommandId(0x00); // add scene
            transitionTime = floor(transitionTime / 10); //deci-seconds -> seconds
        }
        else
        {
            task.zclFrame.setCommandId(0x40); // enhanced add scene
        }

        stream << groupId;
        stream << sceneId;
        stream << transitionTime;

        stream << (uint8_t)0x00; // length of name
        //stream << i->name;     // name not supported
    }

    { // ZCL frame
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    queryTime = queryTime.addSecs(2);
    return addTask(task);
}

/*! Adds a add scene task to the queue.

   \param task - the task item
   \param groupId - the group to which the scene belongs
   \param sceneId - the scene which shall be added
   \return true - on success
           false - on error
 */
bool DeRestPluginPrivate::addTaskAddScene(TaskItem &task, uint16_t groupId, uint8_t sceneId, const QString &lightId)
{
    DBG_Assert(task.lightNode != 0);
    if (!task.lightNode)
    {
        return false;
    }

    Group *group = getGroupForId(groupId);

    std::vector<Scene>::iterator i = group->scenes.begin();
    std::vector<Scene>::iterator end = group->scenes.end();

    for ( ;i != end; ++i)
    {
        if (i->id == sceneId && i->state != Scene::StateDeleted)
        {
            std::vector<LightState>::iterator l = i->lights().begin();
            std::vector<LightState>::iterator lend = i->lights().end();

            for ( ;l != lend; ++l)
            {
                if (l->lid() != lightId)
                {
                    continue;
                }

                task.taskType = TaskAddScene;

                task.req.setClusterId(SCENE_CLUSTER_ID);
                task.req.setProfileId(HA_PROFILE_ID);

                task.zclFrame.payload().clear();
                task.zclFrame.setSequenceNumber(zclSeq++);

                task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                                              deCONZ::ZclFCDirectionClientToServer |
                                              deCONZ::ZclFCDisableDefaultResponse);

                { // payload
                    QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
                    stream.setByteOrder(QDataStream::LittleEndian);

                    uint8_t on = (l->on()) ? 0x01 : 0x00;
                    uint16_t tt;

                    if (l->transitionTime() >= 10)
                    {
                        task.zclFrame.setCommandId(0x00); // add scene
                        tt = floor(l->transitionTime() / 10); //deci-seconds -> seconds
                    }
                    else
                    {
                        task.zclFrame.setCommandId(0x40); // enhanced add scene
                        tt = l->transitionTime();
                    }

                    stream << groupId;
                    stream << sceneId;
                    stream << tt;

                    stream << (uint8_t)0x00; // length of name
                    //stream << i->name;     // name not supported
                    stream << (uint16_t)0x0006; // on/off cluster
                    stream << (uint8_t)0x01;
                    stream << on;
                    stream << (uint16_t)0x0008; // level cluster
                    stream << (uint8_t)0x01;
                    stream << l->bri();

                    ResourceItem *item = task.lightNode->item(RStateColorMode);
                    if (item &&
                            !task.lightNode->modelId().startsWith(QLatin1String("FLS-PP3"))) // color in add scene not supported well
                    {
                        stream << (uint16_t)0x0300; // color cluster
                        stream << (uint8_t)11;
                        if (l->colorMode() == QLatin1String("xy"))
                        {
                            stream << l->x();
                            stream << l->y();
                            stream << l->enhancedHue();
                            stream << l->saturation();
#if 0
                            stream << l->x();
                            stream << l->y();

                            if (task.lightNode->manufacturerCode() == VENDOR_OSRAM ||
                                    task.lightNode->manufacturerCode() == VENDOR_OSRAM_STACK)
                            {
                                stream << l->enhancedHue();
                                stream << l->saturation();
                            }
                            else
                            {
                                stream << (quint16)0; //enhanced hue
                                stream << (quint8)0; // saturation
                            }
#endif
                        }
                        else if (l->colorMode() == QLatin1String("ct"))
                        {
                            quint16 x,y;
                            if (task.lightNode->modelId().startsWith(QLatin1String("FLS-H")))
                            {
                                // quirks mode FLS-H stores color temperature in x
                                x = l->colorTemperature();
                                y = 0;
                            }
                            else if (task.lightNode->modelId().startsWith(QLatin1String("FLS-CT")))
                            {
                                // quirks mode FLS-CT stores color temperature in x
                                x = l->colorTemperature();
                                y = 0;
                            }
                            else if (task.lightNode->modelId().startsWith(QLatin1String("Ribag Air O")))
                            {
                                // quirks mode Ribag Air O stores color temperature in x
                                x = l->colorTemperature();
                                y = 0;
                            }
                            else
                            {
                                MiredColorTemperatureToXY(l->colorTemperature(), &x, &y);
                            }

                            // view scene command will be used to verify x, y values
                            if (l->x() != x || l->y() != y)
                            {
                                l->setX(x);
                                l->setY(y);
                            }

                            stream << x;
                            stream << y;
                            stream << (quint16)0; //enhanced hue
                            stream << (quint8)0; // saturation
                        }
                        else if (l->colorMode() == QLatin1String("hs"))
                        {
                            stream << l->x();
                            stream << l->y();
                            stream << l->enhancedHue();
                            stream << l->saturation();
                        }
                        stream << (quint8)l->colorloopActive();
                        stream << (quint8)l->colorloopDirection();
                        stream << (quint16)l->colorloopTime();
                    }
                }

                { // ZCL frame
                    task.req.asdu().clear(); // cleanup old request data if there is any
                    QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
                    stream.setByteOrder(QDataStream::LittleEndian);
                    task.zclFrame.writeToStream(stream);
                }

                queryTime = queryTime.addSecs(2);

                return addTask(task);
            }
            return false;
        }
    }
    return false;
}

/*! Adds a remove scene task to the queue.

   \param task - the task item
   \param groupId - the group to which the scene belongs
   \param sceneId - the scene which shall be removed
   \return true - on success
           false - on error
 */
bool DeRestPluginPrivate::addTaskRemoveScene(TaskItem &task, uint16_t groupId, uint8_t sceneId)
{
    task.taskType = TaskStoreScene;

    task.req.setClusterId(SCENE_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(0x02); // remove scene
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCDisableDefaultResponse);

    { // payload
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        stream << groupId;
        stream << sceneId;
    }

    { // ZCL frame
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    return addTask(task);
}
