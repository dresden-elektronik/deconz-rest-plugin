/*
 * Copyright (c) 2013-2019 dresden elektronik ingenieurtechnik gmbh.
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
#include "device_descriptions.h"

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
bool DeRestPluginPrivate::addTaskSetOnOff(TaskItem &task, quint8 cmd, quint16 ontime, quint8 flags)
{
    DBG_Assert(cmd == ONOFF_COMMAND_ON || cmd == ONOFF_COMMAND_OFF || cmd == ONOFF_COMMAND_TOGGLE || cmd == ONOFF_COMMAND_OFF_WITH_EFFECT || cmd == ONOFF_COMMAND_ON_WITH_TIMED_OFF);
    if (!(cmd == ONOFF_COMMAND_ON || cmd == ONOFF_COMMAND_OFF || cmd == ONOFF_COMMAND_TOGGLE || cmd == ONOFF_COMMAND_OFF_WITH_EFFECT || cmd == ONOFF_COMMAND_ON_WITH_TIMED_OFF))
    {
        return false;
    }

    if (task.lightNode && task.lightNode->parentResource())
    {
        Device *device = static_cast<Device*>(task.lightNode->parentResource());

        if (device && device->managed())
        {
            uint target = 0;
            ResourceItem *onItem = task.lightNode->item(RStateOn);
            const auto ddfItem = DDF_GetItem(onItem);

            if (cmd == ONOFF_COMMAND_ON || cmd == ONOFF_COMMAND_ON_WITH_TIMED_OFF)
            {
                target = 1;
            }

            if (!ddfItem.writeParameters.isNull())
            {
                StateChange change(StateChange::StateCallFunction, SC_WriteZclAttribute, task.req.dstEndpoint());
                change.addTargetValue(RStateOn, target);
                task.lightNode->addStateChange(change);
                return true;
            }
            else // only verify after classic command
            {
                StateChange change(StateChange::StateWaitSync, SC_SetOnOff, task.req.dstEndpoint());
                change.addTargetValue(RStateOn, target);
                change.addParameter(QLatin1String("cmd"), cmd);
                if (cmd == ONOFF_COMMAND_ON_WITH_TIMED_OFF)
                {
                    change.addParameter(QLatin1String("ontime"), ontime);
                }
                task.lightNode->addStateChange(change);
            }
        }
    }

    task.taskType = TaskSendOnOffToggle;
    task.onOff = cmd == ONOFF_COMMAND_ON || cmd == ONOFF_COMMAND_ON_WITH_TIMED_OFF; // FIXME - what about ONOFF_COMMAND_TOGGLE ?!

    task.req.setClusterId(ONOFF_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(cmd);
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCDisableDefaultResponse);

    if (cmd == ONOFF_COMMAND_OFF_WITH_EFFECT)
    {
        const quint8 effect = 0;
        const quint8 variant = 0;
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream << effect;
        stream << variant;
    }
    else if (cmd == ONOFF_COMMAND_ON_WITH_TIMED_OFF)
    {
        const quint16 offWaitTime = 0;
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        // stream << (quint8)0x80; // 0x01 accept only when on --> no, 0x80 overwrite ontime (yes, non standard)
        stream << flags;
        stream << ontime;
        stream << offWaitTime;
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
    if (task.lightNode && task.lightNode->parentResource())
    {
        Device *device = static_cast<Device*>(task.lightNode->parentResource());

        if (device && device->managed())
        {
            uint target = bri;
            ResourceItem *briItem = task.lightNode->item(RStateBri);
            const auto ddfItem = DDF_GetItem(briItem);

            if (!ddfItem.writeParameters.isNull())
            {
                if (withOnOff) // onoff is a dependency, check if there is a write funtion for it
                {
                    ResourceItem *onItem = task.lightNode->item(RStateOn);
                    const auto ddfItem2 = DDF_GetItem(onItem);

                    if (!ddfItem2.writeParameters.isNull())
                    {
                        StateChange change(StateChange::StateCallFunction, SC_WriteZclAttribute, task.req.dstEndpoint());
                        change.addTargetValue(RStateOn, bri > 0 ? 1 : 0);
                        task.lightNode->addStateChange(change);
                    }
                }

                StateChange change(StateChange::StateCallFunction, SC_WriteZclAttribute, task.req.dstEndpoint());
                change.addTargetValue(RStateBri, target);
                task.lightNode->addStateChange(change);
                return true;
            }
        }
    }

    task.taskType = TaskSetLevel;
    task.level = bri;
    task.onOff = withOnOff; // FIXME abuse of taks.onOff
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
 * \param ct - step size -65534 ..65534, 0 to stop running step
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

    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCDisableDefaultResponse);


    if (ct == 0)
    {
        task.zclFrame.setCommandId(0x47); // Stop move step
    }
    else
    { // payload
        task.zclFrame.setCommandId(0x4C); // Step color temperature

        quint8 direction = ct > 0 ? 1 : 3; // up, down
        quint16 stepSize = ct > 0 ? ct : -ct;

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
 * Add a brightness increase task to the queue
 *
 * \param task - the task item
 * \param bri - step size -254 ..254, 0 to stop running step
 * \return true - on success
 *         false - on error
 */
bool DeRestPluginPrivate::addTaskIncBrightness(TaskItem &task, int16_t bri)
{
    task.taskType = TaskIncBrightness;

    task.inc = bri;
    task.req.setClusterId(LEVEL_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCDisableDefaultResponse);

    if (bri == 0)
    {
        task.zclFrame.setCommandId(0x03); // Stop
    }
    else
    { // payload
        task.zclFrame.setCommandId(0x02); // Step level
        quint8 mode = bri > 0 ? 0 : 1; // up, down
        quint8 stepSize = (bri > 0) ? bri : bri * -1;

        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        stream << mode;
        stream << stepSize;
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
        float sat = (static_cast<float>(ct) - ctMin) / (ctMax - ctMin) * 254;
        if (sat > 254)
        {
            sat = 254;
        }

        bool ret = addTaskSetSaturation(task, static_cast<quint8>(sat));
        // overwrite for later use
        task.taskType = TaskSetColorTemperature;
        task.colorTemperature = ct;
        if (task.lightNode)
        {
            if (task.lightNode->toString(RStateColorMode) != QLatin1String("ct"))
            {
                task.lightNode->setValue(RStateColorMode, QString(QLatin1String("ct")));
            }
        }
        return ret;
    }

    if (task.lightNode)
    {
        ResourceItem *ctMin = task.lightNode->item(RCapColorCtMin);
        ResourceItem *ctMax = task.lightNode->item(RCapColorCtMax);

        // keep ct in supported bounds
        if (ctMin && ctMax && ctMin->toNumber() > 0 && ctMax->toNumber() > 0)
        {
            if      (ct < ctMin->toNumber()) { ct = static_cast<quint16>(ctMin->toNumber()); }
            else if (ct > ctMax->toNumber()) { ct = static_cast<quint16>(ctMax->toNumber()); }
        }

        if (task.lightNode->toString(RStateColorMode) != QLatin1String("ct"))
        {
            task.lightNode->setValue(RStateColorMode, QString(QLatin1String("ct")));
        }

        // If light does not support "ct" but does suport "xy", we can emulate the former:
        ResourceItem *colorCaps = task.lightNode->item(RCapColorCapabilities);
        bool supportsXy = colorCaps && colorCaps->toNumber() & 0x0008;
        bool supportsCt = colorCaps && colorCaps->toNumber() & 0x0010;
        bool useXy = supportsXy && !supportsCt;

        if (useXy)
        {
            quint16 x;
            quint16 y;
            MiredColorTemperatureToXY(ct, &x, &y);
            qreal xr = x / 65535.0;
            qreal yr = y / 65535.0;
            if      (xr < 0) { xr = 0; }
            else if (xr > 1) { xr = 1; }
            if      (yr < 0) { yr = 0; }
            else if (yr > 1) { yr = 1; }
            return addTaskSetXyColor(task, xr, yr);
        }

        DBG_Printf(DBG_INFO, "send move to color temperature %u to 0x%016llX\n", ct, task.lightNode->address().ext());
    }

    task.taskType = TaskSetColorTemperature;
    task.colorTemperature = ct;

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
    task.hueReal = hue / (360.0 * 182.04444);

    if (task.lightNode)
    {
        if (task.lightNode->toString(RStateColorMode) != QLatin1String("hs"))
        {
            task.lightNode->setValue(RStateColorMode, QString(QLatin1String("hs")));
        }
    }

    if (task.hueReal < 0.0)
    {
        task.hueReal = 0.0;
    }
    else if (task.hueReal > 1.0)
    {
        task.hueReal = 1.0;
    }
    task.hue = static_cast<quint8>(task.hueReal * 254.0);
//    task.enhancedHue = hue * 360.0 * 182.04444;
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
        if (task.lightNode->toString(RStateColorMode) != QLatin1String("hs"))
        {
            task.lightNode->setValue(RStateColorMode, QString(QLatin1String("hs")));
        }
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
    task.hueReal = hue / 254.0;
    task.enhancedHue = static_cast<quint16>(task.hueReal * 360.0 * 182.04444);

    if (task.lightNode)
    {
        if (task.lightNode->toString(RStateColorMode) != QLatin1String("hs"))
        {
            task.lightNode->setValue(RStateColorMode, QString(QLatin1String("hs")));
        }
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
        x = 0.00000001;
    }

    // prevent division through zero
    if (y <= 0.0) {
        y = 0.00000001;
    }

    if (task.lightNode)
    {
        ResourceItem *item = task.lightNode->item(RStateBri);
        if (item)
        {
            Y = item->toNumber() / 255.0;
        }
        else
        {
            Y = 1.0;
        }
    }
    else {
        Y = 1.0;
    }
    X = (Y / y) * x;
    Z = (Y / y) * (1.0 - x - y);

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
    h /= 360.0;

    if (h > 1.0)
    {
        h = 1.0;
    }
    else if (h < 0.0)
    {
        h = 0.0;
    }

    uint8_t hue = h * 254.0;
    uint8_t sat = s * 254.0;

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
    DBG_Assert(x >= 0);
    DBG_Assert(x <= 1);
    DBG_Assert(y >= 0);
    DBG_Assert(y <= 1);
    // The CurrentX attribute contains the current value of the normalized chromaticity value x
    // The value of x SHALL be related to the CurrentX attribute by the relationship
    // x = CurrentX / 65536 (CurrentX in the range 0 to 65279 inclusive)

    task.colorX = static_cast<quint16>(x * 65535.0); // current X in range 0 .. 65279
    task.colorY = static_cast<quint16>(y * 65535.0); // current Y in range 0 .. 65279

    if (task.colorX > 65279) { task.colorX = 65279; }
    else if (task.colorX == 0) { task.colorX = 1; }

    if (task.colorY > 65279) { task.colorY = 65279; }
    else if (task.colorY == 0) { task.colorY = 1; }

    if (task.lightNode)
    {
        if (task.lightNode->toString(RStateColorMode) != QLatin1String("xy"))
        {
            task.lightNode->setValue(RStateColorMode, QString(QLatin1String("xy")));
        }

        // convert xy coordinates to hue and saturation
        // due the old FLS-PP don't support this mode
        if (task.lightNode->manufacturerCode() == VENDOR_ATMEL && task.lightNode->modelId() == QLatin1String("FLS-PP"))
        {
            task.lightNode->setValue(RStateX, task.colorX); // update here
            task.lightNode->setValue(RStateY, task.colorY); // update here
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

    if (task.lightNode)
    {
        if (!task.lightNode->supportsColorLoop())
        {
            return false;
        }

        task.lightNode->setColorLoopActive(colorLoopActive);
        task.lightNode->setColorLoopSpeed(speed);
        if (colorLoopActive)
        {
            if (task.lightNode->toString(RStateColorMode) != QLatin1String("hs"))
            {
                task.lightNode->setValue(RStateColorMode, QString(QLatin1String("hs")));
            }
        }
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
        stream << (uint8_t) 0x00; // default effectVariant
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

// Danalock support. To control the lock from the REST API, you need to create a new routine addTaskDoorLock() in zcl_tasks.cpp, cf. the addTaskWarning() I created to control the Siren.
// Based on a lock state parameter, add a task with a Lock Door or Unlock Door command
/*! Add door unlock task to the queue.

    \param task - the task item
    \return true - on success
            false - on error
 */
bool DeRestPluginPrivate::addTaskDoorLockUnlock(TaskItem &task, uint8_t cmd)
{
    task.taskType = TaskDoorLock;

    task.req.setClusterId(DOOR_LOCK_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(cmd); // Start Unlocking
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                                  deCONZ::ZclFCDirectionClientToServer |
                                  deCONZ::ZclFCDisableDefaultResponse);

    { // payload
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
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
    DBG_Assert(task.lightNode);
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
                        stream << (uint8_t)11; // size
                        if (l->colorMode() == QLatin1String("ct"))
                        {
                            quint16 x,y;
                            quint16 enhancedHue = 0;
                            ResourceItem *ctMin = task.lightNode->item(RCapColorCtMin);
                            ResourceItem *ctMax = task.lightNode->item(RCapColorCtMax);

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
                            else if (task.lightNode->modelId().startsWith(QLatin1String("ICZB-F")) ||
                                     task.lightNode->manufacturerCode() == VENDOR_MUELLER)
                            {
                                // quirks mode these lights store color temperature in hue
                                enhancedHue = l->colorTemperature();
                                x = 0;
                                y = 0;
                            }
                            else
                            {
                                quint16 ct = l->colorTemperature();
                                if (ctMin && ctMax && ctMin->toNumber() > 0 && ctMax->toNumber() > 0)
                                {
                                    if      (ct < ctMin->toNumber()) { ct = static_cast<quint16>(ctMin->toNumber()); }
                                    else if (ct > ctMax->toNumber()) { ct = static_cast<quint16>(ctMax->toNumber()); }
                                }

                                MiredColorTemperatureToXY(ct, &x, &y);
                                if (x > 65279) { x = 65279; }
                                else if (x == 0) { x = 1; }

                                if (y > 65279) { y = 65279; }
                                else if (y == 0) { y = 1; }
                            }

                            // view scene command will be used to verify x, y values
                            if (l->x() != x || l->y() != y)
                            {
                                l->setX(x);
                                l->setY(y);
                            }

                            stream << x;
                            stream << y;
                            stream << enhancedHue;
                            stream << (quint8)0; // saturation
                        }
                        else
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
