/*
 * Copyright (C) 2013 dresden elektronik ingenieurtechnik gmbh.
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

/*!
 * Add a OnOff task to the queue
 *
 * \param task - the task item
 * \param on - OnOff value
 * \return true - on success
 *         false - on error
 */
bool DeRestPluginPrivate::addTaskSetOnOff(TaskItem &task, bool on)
{
    task.taskType = TaskSetOnOff;
    task.onOff = on;

    task.req.setClusterId(ONOFF_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(task.onOff ? 0x01 : 0x00);
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCEnableDefaultResponse);

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
                             deCONZ::ZclFCEnableDefaultResponse);

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

    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(0x40); // Enhanced move to hue
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCEnableDefaultResponse);

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

    task.req.setClusterId(COLOR_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(0x03); // Move to saturation
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCEnableDefaultResponse);

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
 * \param sat - brightness level 0..255
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

    task.req.setClusterId(COLOR_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

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

    Y = 1.0f;
    X = (Y / y) * x;
    Z = (Y / y) * (1.0f - x - y);

    Y = y;
    X = x;
    Z = (1.0f - x - y);

    Xyz2Rgb(&r, &g, &b, X, Y, Z);
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

    uint8_t hue = (1.0f - h) * 254.0f;
    uint8_t sat = s * 254.0f;

    DBG_Printf(DBG_INFO, "hue %u, %2f, sat %u, %2f\n", hue, h, sat, s);

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

    // convert xy coordinates to hue and saturation
    // due the lights itself don't support this mode yet
    if (task.lightNode->manufacturerCode() == VENDOR_DDEL)
    {
        return addTaskSetXyColorAsHueAndSaturation(task, x, y);
    }

    task.req.setClusterId(COLOR_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(0x07); // Move to color
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCEnableDefaultResponse);

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

    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(0x00); // Add to group
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCEnableDefaultResponse);

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

    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(0x03); // Remove from group
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCEnableDefaultResponse);

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
