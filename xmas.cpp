/* Merry Christmas!
 *
 * Handle LIDL Melinera Smart LED lightstrip (for Xmas tree).
 */

#include "de_web_plugin_private.h"

#define TUYA_COMMAND_SET 0x00

enum XmasLightStripAttribute
{
    AttrOn = 1,
    AttrMode = 2,
    AttrBri = 3,
    AttrColour = 5,
    AttrEffect = 6
};

enum XmasLightStripDataType
{
    TypeBool = 1,
    TypeNumber = 2,
    TypeString = 3,
    TypeEnum = 4
};

const QStringList RStateEffectValuesXmasLightStrip({
    "none",
    "steady", "snow", "rainbow", "snake",
    "twinkle", "fireworks", "flag", "waves",
    "updown", "vintage", "fading", "collide",
    "strobe", "sparkles", "carnival", "glow"
});

static void initTask(TaskItem &task, quint8 seq)
{
    task.taskType = TaskXmasLightStrip;

    task.req.setClusterId(TUYA_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(seq);
    task.zclFrame.setCommandId(TUYA_COMMAND_SET); // Set
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                                  deCONZ::ZclFCDirectionClientToServer);
}

static void tlvOn(QDataStream &stream, bool on)
{
    stream << (quint8) AttrOn;
    stream << (quint8) TypeBool;
    stream << (quint16) 1;
    stream << (quint8) (on ? 1 : 0);
}

static void tlvMode(QDataStream &stream, XmasLightStripMode mode)
{
    stream << (quint8) AttrMode;
    stream << (quint8) TypeEnum;
    stream << (quint16) 1;
    stream << (quint8) mode;
}

static void tlvBrightness(QDataStream &stream, quint8 bri)
{
    stream << (quint8) AttrBri;
    stream << (quint8) TypeNumber;
    stream << (quint16) 4;
    stream << (quint32) bri * 10;
}

static void tlvColour(QDataStream &stream, quint16 hue, quint8 sat, quint8 bri)
{
    char s[13];
    sprintf(s, "%04x%04x%04x", hue, sat * 10, bri * 10);

    stream << (quint8) AttrColour;
    stream << (quint8) TypeString;
    stream << (quint16) strlen(s);
    stream.writeRawData(s, strlen(s));
}

static void tlvEffect(QDataStream &stream, XmasLightStripEffect effect, quint8 speed, QList<QList<quint8>> &colours)
{
    char s[41];
    sprintf(s, "%02x%02x", effect, speed);
    int i = 4;
    for (const QList<quint8> colour: colours)
    {
        sprintf(s + i, "%02x%02x%02x", colour[0], colour[1], colour[2]);
        i += 6;
    }

    stream << (quint8) AttrEffect;
    stream << (quint8) TypeString;
    stream << (quint16) strlen(s);
    stream.writeRawData(s, strlen(s));
}

/*! Check whether LightNode is the LIDL Melinera Smart LED lightstrip.
    \param lightNode - the indication primitive
 */
bool DeRestPluginPrivate::isXmasLightStrip(LightNode *lightNode)
{
    return lightNode != nullptr &&
           (lightNode->modelId() == QLatin1String("HG06467") ||
            lightNode->manufacturer() == QLatin1String("_TZE200_s8gkrkxk"));
}

QString XmasEffectName(quint8 effect)
{
    return RStateEffectValuesXmasLightStrip[effect];
}

/*! Switch the lightstrip on or off.
    \param task - the task item
    \param on - on (true) or off (false)
    \return true - on success
            false - on error
 */
bool DeRestPluginPrivate::addTaskXmasLightStripOn(TaskItem &task, bool on)
{
    const quint8 seq = zclSeq++;
    initTask(task, seq);

    // payload
    QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian); // !
    stream << (quint8) 0; // Status
    stream << (quint8) seq; // Transaction ID

    tlvOn(stream, on);

    { // ZCL frame
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    return addTask(task);
}

/*! Switch the lightstrip on or off.
    \param task - the task item
    \param on - on (true) or off (false)
    \return true - on success
            false - on error
 */
bool DeRestPluginPrivate::addTaskXmasLightStripMode(TaskItem &task, XmasLightStripMode mode)
{
    const quint8 seq = zclSeq++;
    initTask(task, seq);

    // payload
    QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian); // !
    stream << (quint8) 0; // Status
    stream << (quint8) seq; // Transaction ID

    tlvMode(stream, mode);

    { // ZCL frame
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    return addTask(task);
}

/*! Switch the lightstrip on, set it to white mode, and set the brightness.
    \param task - the task item
    \param bri - the brightness, between 0 and 100
    \return true - on success
            false - on error
 */
bool DeRestPluginPrivate::addTaskXmasLightStripWhite(TaskItem &task, quint8 bri)
{
    const quint8 seq = zclSeq++;
    initTask(task, seq);

    // payload
    QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian); // !
    stream << (quint8) 0; // Status
    stream << (quint8) seq; // Transaction ID

    // tlvOn(stream, true);
    // tlvMode(stream, ModeWhite);
    tlvBrightness(stream, bri);

    { // ZCL frame
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    return addTask(task);
}

/*! Switch the lightstrip on, set it to colour mode, and set the colour.
    \param task - the task item
    \param hue - the hue, between 0 and 360
    \param sat - the saturation, between 0 and 100
    \param bri - the level, between 0 and 100
    \note The lightstrip uses HSL values to set the colour and brightness.
 */
bool DeRestPluginPrivate::addTaskXmasLightStripColour(TaskItem &task, quint16 hue, quint8 sat, quint8 bri)
{
    const quint8 seq = zclSeq++;
    initTask(task, seq);

    // payload
    QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian); // !
    stream << (quint8) 0; // Status
    stream << (quint8) seq; // Transaction ID

    // tlvOn(stream, true);
    // tlvMode(stream, ModeColour);
    tlvColour(stream, hue, sat, bri);

    { // ZCL frame
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    return addTask(task);
}

/*! Switch the lightstrip on, set it to effect mode, and set the effect.
    \param task - the task item
    \param effect - the effect, between 0 and 15
    \param speed - the effect speed, between 0 and 100
    \param colours - a list of 0 to 6 RGB colours.  Each colour is a list of 3 quint8 values.
    \note The lightstrip uses RGB values to set the effect colours.
 */
bool DeRestPluginPrivate::addTaskXmasLightStripEffect(TaskItem &task, XmasLightStripEffect effect, quint8 speed, QList<QList<quint8>> &colours)
{
    const quint8 seq = zclSeq++;
    initTask(task, seq);

    // payload
    QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian); // !
    stream << (quint8) 0; // Status
    stream << (quint8) seq; // Transaction ID

    // tlvOn(stream, true);
    tlvMode(stream, ModeEffect);
    tlvEffect(stream, effect, speed, colours);

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
    b.onTime = a.onTime;
    b.lightNode = a.lightNode;
}

/*! PUT, PATCH /api/<apikey>/lights/<id>/state for Xmas lights.
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::setXmasLightStripState(const ApiRequest &req, ApiResponse &rsp, TaskItem &taskRef, QVariantMap &map)
{
    bool ok;
    QString id = req.path[3];

    bool hasCmd = false;
    bool isOn = false;
    bool hasOn = false;
    bool targetOn = false;
    bool hasBri = false;
    quint8 targetBri = 0;
    bool hasHue = false;
    quint16 targetHue = 0;
    bool hasSat = false;
    quint8 targetSat = 0;
    int effect = -1;
    bool hasEffectSpeed = false;
    quint16 effectSpeed = 50;
    QList<QList<quint8>> effectColours;

    // Check parameters.
    for (QVariantMap::const_iterator p = map.begin(); p != map.end(); p++)
    {
        bool paramOk = false;
        bool valueOk = false;
        QString param = p.key();
        if (param == "on" && taskRef.lightNode->item(RStateOn))
        {
            paramOk = true;
            hasCmd = true;
            if (map[param].type() == QVariant::Bool)
            {
                valueOk = true;
                hasOn = true;
                targetOn = map[param].toBool();
            }
        }
        else if (param == "bri" && taskRef.lightNode->item(RStateBri))
        {
            paramOk = true;
            hasCmd = true;
            if (map[param].type() == QVariant::Double)
            {
                const uint bri = map[param].toUInt(&ok);
                if (ok && bri <= 0xFF)
                {
                    valueOk = true;
                    hasBri = true;
                    targetBri = bri > 0xFE ? 0xFE : bri;
                }
            }
        }
        else if (param == "hue" && taskRef.lightNode->item(RStateHue) && taskRef.lightNode->item(RStateSat))
        {
            paramOk = true;
            hasCmd = true;
            if (map[param].type() == QVariant::Double)
            {
                const uint hue = map[param].toUInt(&ok);
                if (ok && hue <= 0xFFFF)
                {
                    valueOk = true;
                    hasHue = true;
                    targetHue = hue; // Funny: max CurrentHue is 0xFE, max EnhancedCurrentHue is 0xFFFF
                }
            }
        }
        else if (param == "sat" && taskRef.lightNode->item(RStateHue) && taskRef.lightNode->item(RStateSat))
        {
            paramOk = true;
            hasCmd = true;
            if (map[param].type() == QVariant::Double)
            {
                const uint sat = map[param].toUInt(&ok);
                if (ok && sat <= 0xFF)
                {
                    valueOk = true;
                    hasSat = true;
                    targetSat = sat > 0xFE ? 0xFE : sat;
                }
            }
        }
        else if (param == "effect" && taskRef.lightNode->item(RStateEffect))
        {
            paramOk = true;
            hasCmd = true;
            if (map[param].type() == QVariant::String)
            {
                effect = RStateEffectValuesXmasLightStrip.indexOf(map[param].toString());
                valueOk = effect >= 0;
            }
        }
        else if (param == "effectSpeed" && taskRef.lightNode->item(RStateEffect))
        {
            paramOk = true;
            if (map[param].type() == QVariant::Double)
            {
                const uint speed = map[param].toUInt(&ok);
                if (ok && speed <= 100)
                {
                    valueOk = true;
                    effectSpeed = speed < 1 ? 1 : speed;
                }
            }
        }
        else if (param == "effectColours" && taskRef.lightNode->item(RStateEffect))
        {
            paramOk = true;
            ok = true;
            if (map[param].type() == QVariant::List)
            {
                QVariantList colours = map["effectColours"].toList();
                if (colours.length() <= 6) {
                    for (const QVariant colour: colours)
                    {
                        if (colour.type() == QVariant::List)
                        {
                            QVariantList rgb = colour.toList();
                            if (rgb.length() != 3) {
                                ok = false;
                                break;
                            }
                            const uint r = rgb[0].toUInt(&ok);
                            if (!ok)
                            {
                                break;
                            }
                            const uint g = rgb[1].toUInt(&ok);
                            if (!ok)
                            {
                                break;
                            }
                            const uint b = rgb[2].toUInt(&ok);
                            if (!ok)
                            {
                                break;
                            }
                            if (r > 0xFF || g > 0xFF || b > 0xFF)
                            {
                                ok = false;
                                break;
                            }
                            effectColours.push_back({(quint8) r, (quint8) g, (quint8) b});
                        }
                    }
                    if (ok)
                    {
                        valueOk = true;
                    }
                    else
                    {
                        effectColours = {};
                    }
                }
            }
        }
        if (!paramOk)
        {
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/lights/%1/state").arg(id), QString("parameter, %1, not available").arg(param)));
        }
        else if (!valueOk)
        {
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1/state").arg(id), QString("invalid value, %1, for parameter, %2").arg(map[param].toString()).arg(param)));
        }
    }
    if (taskRef.onTime > 0 && !hasOn)
    {
        rsp.list.append(errorToMap(ERR_MISSING_PARAMETER, QString("/lights/%1/state").arg(id), QString("missing parameter, on, for parameter, ontime")));
    }
    if (hasEffectSpeed && effect < 1)
    {
        rsp.list.append(errorToMap(ERR_MISSING_PARAMETER, QString("/lights/%1/state").arg(id), QString("missing parameter, effect, for parameter, effectSpeed")));
    }
    if (effectColours.length() > 0 && effect < 1)
    {
        rsp.list.append(errorToMap(ERR_MISSING_PARAMETER, QString("/lights/%1/state").arg(id), QString("missing parameter, effect, for parameter, effectSpeed")));
    }
    if (!hasCmd)
    {
        rsp.list.append(errorToMap(ERR_MISSING_PARAMETER, QString("/lights/%1/state").arg(id), QString("missing parameter to set light state")));
    }

    // Check whether light is on.
    isOn = taskRef.lightNode->toBool(RStateOn);

    // state.on: true
    if (hasOn && targetOn)
    {
        TaskItem task;
        copyTaskReq(taskRef, task);

        if (addTaskSetOnOff(task, ONOFF_COMMAND_ON, 0, 0))
        {
            isOn = true;
            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/lights/%1/state/on").arg(id)] = true;
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);
            taskRef.lightNode->setValue(RStateOn, targetOn);
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1/state/on").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
        }
    }

    if (effect >= 0)
    {
        TaskItem task;
        copyTaskReq(taskRef, task);

        if (!isOn) {
            rsp.list.append(errorToMap(ERR_DEVICE_OFF, QString("/lights/%1/state").arg(id), QString("parameter, effect, is not modifiable. Device is set to off.")));
        }
        if (effect == R_EFFECT_NONE)
        {
            if (!hasSat)
            {
                targetSat = taskRef.lightNode->toNumber(RStateSat);
            }
            ok = addTaskXmasLightStripMode(task, targetSat > 0 ? ModeColour : ModeWhite);
        }
        else
        {
            ok = addTaskXmasLightStripEffect(task, XmasLightStripEffect(effect - 1), effectSpeed, effectColours);
        }
        if (ok)
        {
            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/lights/%1/state/effect").arg(id)] = RStateEffectValuesXmasLightStrip[effect];
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);

            taskRef.lightNode->setValue(RStateEffect, RStateEffectValuesXmasLightStrip[effect]);
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1/state/effect").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
        }
    }

    if ((hasBri || hasHue || hasSat) && effect <= 0)
    {
        TaskItem task;
        copyTaskReq(taskRef, task);

        if (!isOn)
        {
            if (hasHue)
            {
                rsp.list.append(errorToMap(ERR_DEVICE_OFF, QString("/lights/%1/state").arg(id), QString("parameter, hue, is not modifiable. Device is set to off.")));
            }
            if (hasSat)
            {
                rsp.list.append(errorToMap(ERR_DEVICE_OFF, QString("/lights/%1/state").arg(id), QString("parameter, sat, is not modifiable. Device is set to off.")));
            }
            if (hasBri)
            {
                rsp.list.append(errorToMap(ERR_DEVICE_OFF, QString("/lights/%1/state").arg(id), QString("parameter, bri, is not modifiable. Device is set to off.")));
            }
        }
        if (!hasHue) // only state.sat
        {
            targetHue = taskRef.lightNode->toNumber(RStateHue);
        }
        if (!hasSat) // only state.hue
        {
            targetSat = taskRef.lightNode->toNumber(RStateSat);
        }
        if (!hasBri)
        {
            targetBri = taskRef.lightNode->toNumber(RStateBri);
        }

        if (targetSat == 0)
        {
            quint8 bri = round(targetBri * 100.0 / 0xFF);
            ok = addTaskXmasLightStripWhite(task, bri);
        }
        else
        {
            quint16 h = round(targetHue * 360.0 / 0xFFFF);
            quint8 s = round(targetSat * 100.0 / 0xFF);
            quint8 l = round(targetBri * 100.0 / 0xFF);
            ok = addTaskXmasLightStripColour(task, h, s, l);
        }
        if (ok)
        {
            if (hasBri)
            {
                QVariantMap rspItem;
                QVariantMap rspItemState;
                rspItemState[QString("/lights/%1/state/bri").arg(id)] = targetBri;
                rspItem["success"] = rspItemState;
                rsp.list.append(rspItem);

                taskRef.lightNode->setValue(RStateBri, targetBri);
            }
            if (hasHue)
            {
                QVariantMap rspItem;
                QVariantMap rspItemState;
                rspItemState[QString("/lights/%1/state/hue").arg(id)] = targetHue;
                rspItem["success"] = rspItemState;
                rsp.list.append(rspItem);

                taskRef.lightNode->setValue(RStateHue, targetHue);
            }
            if (hasSat)
            {
                QVariantMap rspItem;
                QVariantMap rspItemState;
                rspItemState[QString("/lights/%1/state/sat").arg(id)] = targetSat;
                rspItem["success"] = rspItemState;
                rsp.list.append(rspItem);

                taskRef.lightNode->setValue(RStateSat, targetSat);
            }
            taskRef.lightNode->setValue(RStateEffect, RStateEffectValuesXmasLightStrip[R_EFFECT_NONE]);
        }
        else
        {
            if (hasBri)
            {
                rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1/state/bri").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
            }
            if (hasHue)
            {
                rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1/state/hue").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
            }
            if (hasSat)
            {
                rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1/state/sat").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
            }
        }
    }

    // state.on: false
    if (hasOn && !targetOn)
    {
        TaskItem task;
        copyTaskReq(taskRef, task);

        if (addTaskSetOnOff(task, ONOFF_COMMAND_OFF, 0, 0))
        {
            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/lights/%1/state/on").arg(id)] = targetOn;
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);
            taskRef.lightNode->setValue(RStateOn, targetOn);
        }
        else
        {
            rsp.list.append(errorToMap(ERR_INTERNAL_ERROR, QString("/lights/%1/state/on").arg(id), QString("Internal error, %1").arg(ERR_BRIDGE_BUSY)));
        }
    }

    rsp.etag = taskRef.lightNode->etag;
    processTasks();
    return REQ_READY_SEND;
}
