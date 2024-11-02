/*
 * Handle Hue-specific FC03 cluster.
 */

#include <QString>
#include <math.h>
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"

#define HUE_EFFECTS_CLUSTER_ID 0xFC03

// Constants for 'timed_effect duration'
#define RESOLUTION_01s_BASE 0xFC
#define RESOLUTION_05s_BASE 0xCC
#define RESOLUTION_15s_BASE 0xA5
#define RESOLUTION_01m_BASE 0x79
#define RESOLUTION_05m_BASE 0x4A

#define RESOLUTION_01s (1 * 10)         // 01s.
#define RESOLUTION_05s (5 * 10)         // 05s.
#define RESOLUTION_15s (15 * 10)        // 15s.
#define RESOLUTION_01m (1 * 60 * 10)    // 01min.
#define RESOLUTION_05m (5 * 60 * 100)   // 05min.

#define RESOLUTION_01s_LIMIT (60 * 10)          // 01min.
#define RESOLUTION_05s_LIMIT (5 * 60 * 10)      // 05min.
#define RESOLUTION_15s_LIMIT (15 * 60 * 10)     // 15min.
#define RESOLUTION_01m_LIMIT (60 * 60 * 10)     // 60min.
#define RESOLUTION_05m_LIMIT (6 * 60 * 60 * 10) // 06hrs.

// List of 'state' keys that can be mapped into the '0xfc03' cluster's '0x00' command.
QList<QString> supportedStateKeys = {"on", "bri", "ct", "xy", "transitiontime", "effect", "effect_duration", "effect_speed"};

struct code {
    quint8 value;
    QString name;
};

code effects[] = {
    { 0x00, QLatin1String("none") },
    { 0x01, QLatin1String("candle") },
    { 0x02, QLatin1String("fire") },
    { 0x03, QLatin1String("prism") },
    { 0x09, QLatin1String("sunrise") },
    { 0x0a, QLatin1String("sparkle") },
    { 0x0b, QLatin1String("opal") },
    { 0x0c, QLatin1String("glisten") },
    { 0x0d, QLatin1String("sunset") },
    { 0x0e, QLatin1String("underwater") },
    { 0x0f, QLatin1String("cosmos") },
    { 0x10, QLatin1String("sunbeam") },
    { 0x11, QLatin1String("enchant") }
};

quint8 effectNameToValue(QString &effectName)
{
    for (auto &e: effects)
    {
        if (e.name == effectName)
        {
            return e.value;
        }
    }
    return 0xFF;
}

/*! Test if a LightNode is a Philips Hue light that supports effects.
    \param lightNode - the light node to test
 */
bool DeRestPluginPrivate::isHueEffectLight(const LightNode *lightNode)
{
    return lightNode != nullptr &&
           lightNode->manufacturerCode() == VENDOR_PHILIPS &&
           lightNode->item(RCapColorEffects);
}

/*! Test whether all the items in a request can be mapped into an '0xfc03' cluster command.
    \param map - the map to test
 */
bool DeRestPluginPrivate::isMappableToManufacturerSpecific(const QVariantMap &map)
{
    const QList<QString> keyList = map.keys();
    for (const QString &key : keyList)
    {
        // Ensure 'effect' is not 'colorloop' - that's handled through the ZCL
        if ((key == "effect") && (map[key].toString() == "colorloop"))
        {
            return false;
        }

        if (!supportedStateKeys.contains(key))
        {
            return false;
        }
    }

    return true;
}

/*! Return a list of effect names corresponding to the bitmap of supported effects.

   \param effectBitmap - the bitmap with supported effects (from 0x0011)
   \return QStringList of effect names
 */
QStringList DeRestPluginPrivate::getHueEffectNames(quint64 effectBitmap, bool colorloop)
{
    QStringList names = { QLatin1String("none") };
    if (colorloop)
    {
        names.append(QLatin1String("colorloop"));
    }
    for (auto &e: effects) {
        if (effectBitmap & (0x01 << e.value))
        {
            names.append(e.name);
        }
    }
    return names;
};

code styles[] = {
    { 0x00, QLatin1String("linear") }, // interpolated_palette
    { 0x02, QLatin1String("scattered") }, // random_pixelated
    { 0x04, QLatin1String("mirrored") } // interpolated_palette_mirrored
};

quint8 styleNameToValue(QString &styleName)
{
    for (auto &s: styles)
    {
        if (s.name == styleName)
        {
            return s.value;
        }
    }
    return 0xFF;
}

/*! Return a list of style names corresponding to the bitmap of supported styles.

   \param styleBitmap - the bitmap with supported styles (from 0x0013)
   \return QStringList of style names
 */
QStringList DeRestPluginPrivate::getHueGradientStyleNames(quint16 styleBitmap)
{
    QStringList names = {};

    for (auto &s: styles) {
        if (styleBitmap & (0x01 << (s.value >> 1)))
        {
            names.append(s.name);
        }
    }
    return names;
};

const double maxX = 0.7347;
const double maxY = 0.8431;

// MARK: - Stream Helpers

void streamPoint(QDataStream &stream, double x, double y)
{
    const quint16 rawX = (x >= maxX) ? 4095 : floor(x * 4095 / maxX);
    const quint16 rawY = (y >= maxY) ? 4095 : floor(y * 4095 / maxY);
    stream << (quint8) (rawX & 0x0FF);
    stream << (quint8) (((rawX & 0xF00) >> 8) | ((rawY & 0x00F) << 4));
    stream << (quint8) ((rawY & 0xFF0) >> 4);
}

void streamHueManufacturerSpecificState(QDataStream &stream, const QVariantMap &items)
{
    // Set payload contents
    quint16 payloadBitmask = 0x00;
    if (items.contains("on")) { payloadBitmask |= (1 << 0); }
    if (items.contains("bri")) { payloadBitmask |= (1 << 1); }
    if (items.contains("ct")) { payloadBitmask |= (1 << 2); }
    if (items.contains("xy")) { payloadBitmask |= (1 << 3); }
    if (items.contains("transitiontime")) { payloadBitmask |= (1 << 4); }
    if (items.contains("effect")) { payloadBitmask |= (1 << 5); }
    if (items.contains("gradient")) { payloadBitmask |= (1 << 6); }
    // 'effect_duration' and 'effect_speed' share the same bit flag.
    if (items.contains("effect_duration")) { payloadBitmask |= (1 << 7); }
    if (items.contains("effect_speed")) { payloadBitmask |= (1 << 7); }
    stream << (quint16)payloadBitmask;

    // !!!: The order the items are processed in is important

    if (items.contains("on"))
    {
        stream << (quint8)(items["on"].toBool() ? 0x01 : 0x00);
    }

    if (items.contains("bri"))
    {
        const quint8 bri = items["bri"].toUInt();
        stream << (quint8)(bri > 0xFE ? 0xFE : bri);
    }

    if (items.contains("ct"))
    {
        stream << (quint16)(items["ct"].toUInt());
    }

    if (items.contains("xy"))
    {
        const QVariantList xy = items["xy"].toList();
        quint16 colorX = static_cast<quint16>(xy[0].toDouble() * 65535.0);
        quint16 colorY = static_cast<quint16>(xy[1].toDouble() * 65535.0);

        if (colorX > 65279) { colorX = 65279; }
        else if (colorX == 0) { colorX = 1; }

        if (colorY > 65279) { colorY = 65279; }
        else if (colorY == 0) { colorY = 1; }

        stream << (quint32)((colorY << 16) + colorX);
    }

    if (items.contains("transitiontime"))
    {
        const quint8 tt = items["transitiontime"].toUInt();
        stream << (quint16)(tt > 0xFFFE ? 0xFFFE : tt);
    }

    if (items.contains("effect"))
    {
        QString e = items["effect"].toString();
        stream << (quint8)(effectNameToValue(e));
    }

    if (items.contains("effect_duration"))
    {
        const uint ed = items["effect_duration"].toUInt();

        const uint resolutionBase = (ed == 0) ? 0 :
                                    (ed < RESOLUTION_01s_LIMIT) ? RESOLUTION_01s_BASE :
                                    (ed < RESOLUTION_05s_LIMIT) ? RESOLUTION_05s_BASE :
                                    (ed < RESOLUTION_15s_LIMIT) ? RESOLUTION_15s_BASE :
                                    (ed < RESOLUTION_01m_LIMIT) ? RESOLUTION_01m_BASE :
                                    (ed < RESOLUTION_05m_LIMIT) ? RESOLUTION_05m_BASE : 0;

        const uint resolution = (ed == 0) ? 1 :
                                (ed < RESOLUTION_01s_LIMIT) ? RESOLUTION_01s :
                                (ed < RESOLUTION_05s_LIMIT) ? RESOLUTION_05s :
                                (ed < RESOLUTION_15s_LIMIT) ? RESOLUTION_15s :
                                (ed < RESOLUTION_01m_LIMIT) ? RESOLUTION_01m :
                                (ed < RESOLUTION_05m_LIMIT) ? RESOLUTION_05m : 1;

        const quint8 effectDuration = resolutionBase - (ed / resolution);
        stream << (quint8)(effectDuration);
    }
    else if (items.contains("effect_speed"))
    {
        const double es = items["effect_speed"].toDouble();
        quint8 effectSpeed = static_cast<quint8>(es * 254.0);
        stream << (quint8)(effectSpeed);
    }
}

void streamHueManufacturerSpecificPalette(QDataStream &stream, const QVariantMap &palette)
{
    // Set payload contents
    quint8 payloadBitmask = 0x00;
    if (palette.contains("transitiontime")) { payloadBitmask |= 0x09; }
    if (palette.contains("bri")) { payloadBitmask |= 0x0A; }
    if (palette.contains("xy")) { payloadBitmask |= 0x0C; }
    stream << (quint8)payloadBitmask;

    // !!!: The order the items are processed in is important

    if (palette.contains("transitiontime"))
    {
        const quint8 tt = palette["transitiontime"].toUInt();
        stream << (quint16)(tt > 0xFFFE ? 0xFFFE : tt);
    }

    if (palette.contains("bri"))
    {
        const quint8 bri = palette["bri"].toUInt();
        stream << (quint8)(bri > 0xFE ? 0xFE : bri);
    }

    if (palette.contains("xy"))
    {
        QVariantList colors = palette["xy"].toList();
        QVariantList color;

        const quint8 nColors = colors.length();
        stream << (quint8) (1 + 3 * (nColors + 1));
        stream << (quint8) (nColors << 4);

        stream << (quint8) 0x00;
        stream << (quint8) 0x00;
        stream << (quint8) 0x00;

        // Palette Colors
        for (auto &color : colors)
        {
            QVariantList xy = color.toList();
            streamPoint(stream, xy[0].toDouble(), xy[1].toDouble());
        }
    }

    if (palette.contains("effect_speed"))
    {
        const double es = palette["effect_speed"].toDouble();
        quint8 effectSpeed = static_cast<quint8>(es * 254.0);
        stream << (quint8)(effectSpeed);
    }
}

// MARK: - Tasks

/*! Add a Hue effect task to the queue.

   \param task - the task item
   \param effectName - the effect
   \return true - on success
           false - on error
 */
bool DeRestPluginPrivate::addTaskHueEffect(TaskItem &task, QString &effectName)
{
    task.taskType = TaskHueEffect;
    task.req.setClusterId(HUE_EFFECTS_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(0x00);
    task.zclFrame.setManufacturerCode(VENDOR_PHILIPS);
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                                  deCONZ::ZclFCManufacturerSpecific |
                                  deCONZ::ZclFCDirectionClientToServer |
                                  deCONZ::ZclFCDisableDefaultResponse);
    { // payload
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        stream << (quint16) 0x0020; // set effect
        // stream << (quint16) 0x0021; // set effect (with on/off)
        // stream << (quint8) 1; // on
        if (effectName == "none")
        {
            stream << (quint8) 0; // none
        }
        else
        {
            stream << effectNameToValue(effectName);
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

/*! Add a Hue gradient task to the queue.

   \param task - the task item
   \param gradient - the gradient
   \return true - on success
           false - on error
 */
bool DeRestPluginPrivate::addTaskHueGradient(TaskItem &task, QVariantMap &gradient)
{
    task.taskType = TaskHueGradient;
    task.req.setClusterId(HUE_EFFECTS_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(0x00);
    task.zclFrame.setManufacturerCode(VENDOR_PHILIPS);
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                                  deCONZ::ZclFCManufacturerSpecific |
                                  deCONZ::ZclFCDirectionClientToServer |
                                  deCONZ::ZclFCDisableDefaultResponse);

    QVariantList points = gradient["points"].toList();
    QVariantList point;
    quint8 style = 0xFF;
    for (auto &s: styles)
    {
        if (gradient["style"] == s.name)
        {
            style = s.value;
            break;
        }
    }

    { // payload
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        stream << (quint16) 0x0150; // set gradient
        stream << (quint16) 0x0004; // transitiontime

        const quint8 nPoints = points.length();
        stream << (quint8) (1 + 3 * (nPoints + 1));
        stream << (quint8) (nPoints << 4);
        stream << (quint8) style;
        stream << (quint8) 0;
        stream << (quint8) 0;
        for (auto &point : points)
        {
            QVariantList xy = point.toList();
            streamPoint(stream, xy[0].toDouble(), xy[1].toDouble());
        }
        stream << (quint8) ((gradient["segments"].toUInt() << 3) | gradient["color_adjustment"].toUInt());
        stream << (quint8) ((gradient["offset"].toUInt() << 3)| gradient["offset_adjustment"].toUInt());
    }

    { // ZCL frame
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }
    return addTask(task);
}

/*! Add a Hue Manufacturer Specific '0xfc03' '0x00' task to the queue.

   \param task - the task item
   \param items - the list of items in the payload
   \return true - on success
           false - on error
 */
bool DeRestPluginPrivate::addTaskHueManufacturerSpecificSetState(TaskItem &task, const QVariantMap &items)
{
    task.taskType = TaskHueManufacturerSpecific;
    task.req.setClusterId(HUE_EFFECTS_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(0x00);
    task.zclFrame.setManufacturerCode(VENDOR_PHILIPS);
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                                  deCONZ::ZclFCManufacturerSpecific |
                                  deCONZ::ZclFCDirectionClientToServer |
                                  deCONZ::ZclFCDisableDefaultResponse);

    { // Payload
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        streamHueManufacturerSpecificState(stream, items);
    }

    { // ZCL frame
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    return addTask(task);
}

/*! Add a Hue Manufacturer Specific '0x0005' '0x02' task to the queue.

   \param task - the task item
   \param groupId - the id of the scene's parent group
   \param sceneId - the id of the scene to modify
   \param payloadItems - the contents in the payload
   \param items - the list of items in the payload
   \return true - on success
           false - on error
 */
bool DeRestPluginPrivate::addTaskHueManufacturerSpecificAddScene(TaskItem &task, const quint16 groupId, const quint8 sceneId, const QVariantMap &items)
{
    task.taskType = TaskHueManufacturerSpecific;
    task.req.setClusterId(SCENE_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(0x02);
    task.zclFrame.setManufacturerCode(VENDOR_PHILIPS);
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                                  deCONZ::ZclFCManufacturerSpecific |
                                  deCONZ::ZclFCDirectionClientToServer |
                                  deCONZ::ZclFCDisableDefaultResponse);

    { // Payload
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        // Group and Scene IDs
        stream << (quint16) groupId;
        stream << (quint8) sceneId;

        streamHueManufacturerSpecificState(stream, items);
    }

    { // ZCL frame
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    return addTask(task);
}

/*! Add a Play Hue Dynamic Scene task to the queue.

   \param task - the task item
   \param groupId - the id of the scene's parent group
   \param sceneId - the id of the scene to recall
   \param palette - the list of palette items in the payload
   \return true - on success
           false - on error
 */
bool DeRestPluginPrivate::addTaskHueDynamicSceneRecall(TaskItem &task, const quint16 groupId, const quint8 sceneId, const QVariantMap &palette)
{
    task.taskType = TaskHueManufacturerSpecific;
    task.req.setClusterId(SCENE_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(0x00);
    task.zclFrame.setManufacturerCode(VENDOR_PHILIPS);
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                                  deCONZ::ZclFCManufacturerSpecific |
                                  deCONZ::ZclFCDirectionClientToServer |
                                  deCONZ::ZclFCDisableDefaultResponse);

    { // Payload
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        // Group and Scene IDs
        stream << (quint16) groupId;
        stream << (quint8) sceneId;

        streamHueManufacturerSpecificPalette(stream, palette);
    }

    { // ZCL frame
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    return addTask(task);
}

// MARK: - Validation Helpers

bool DeRestPluginPrivate::validateHueGradient(const ApiRequest &req, ApiResponse &rsp, QVariantMap &gradient, quint16 styleBitmap = 0x0001)
{
    QString id = req.path[3];
    bool ok = true;
    bool check;

    if (gradient["points"].isNull())
    {
        rsp.list.append(errorToMap(ERR_MISSING_PARAMETER, QString("/lights/%1/state").arg(id), QString("missing parameter, gradient/points, for parameter, gradient")));
        return false;
    }
    if (gradient["points"].type() != QVariant::List)
    {
        rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1/state").arg(id), QString("invalid value, %1, for parameter, gradient/points").arg(gradient["points"].toString())));
        return false;
    }
    QVariantList &points = *reinterpret_cast<QVariantList *>(gradient["points"].data()); // Create reference instead of copy
    const quint8 length = points.length();
    if (length < 2 || length > 9)
    {
        rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1/state").arg(id), QString("invalid length, %1, for parameter, gradient/points").arg(length)));
        return false;
    }
    if (gradient["segments"].isNull()) gradient["segments"] = length;
    if (gradient["color_adjustment"].isNull()) gradient["color_adjustment"] = 0;
    if (gradient["offset"].isNull()) gradient["offset"] = 0;
    if (gradient["offset_adjustment"].isNull()) gradient["offset_adjustment"] = 0;
    if (gradient["style"].isNull()) gradient["style"] = "linear";

    for (QVariantMap::const_iterator p = gradient.begin(); p != gradient.end(); p++)
    {
        QString param = p.key();

        if (param == "points")
        {
            int i = -1;
            for (auto &point : points)
            {
                i++;
                if (point.type() != QVariant::List)
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1/state").arg(id), QString("invalid value, %1, for parameter, gradient/points/%2").arg(point.toString()).arg(i)));
                    ok = false;
                    continue;
                }
                QVariantList &xy = *reinterpret_cast<QVariantList *>(point.data()); // Create reference instead of copy
                if (xy.length() != 2)
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1/state").arg(id), QString("invalid length, %1, for parameter, gradient/points/%2").arg(xy.length()).arg(i)));
                    ok = false;
                    continue;
                }
                double x = xy[0].toDouble(&check);
                if (!check || x < 0 || x > 1)
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1/state").arg(id), QString("invalid value, %1, for parameter, gradient/points/%2/0").arg(xy[0].toString()).arg(i)));
                    ok = false;
                }
                if (x > maxX) xy[0] = maxX; // This is why we needed a reference
                double y = xy[1].toDouble(&check);
                if (!check || y < 0 || y > 1)
                {
                    rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1/state").arg(id), QString("invalid value, %1, for parameter, gradient/points/%2/1").arg(xy[1].toString()).arg(i)));
                    ok = false;
                }
                if (y > maxY) xy[1] = maxY; // This is why we needed a reference
            }
        }
        else if (param == "segments" || param == "offset")
        {
            quint8 value = gradient[param].toUInt(&check);
            if (!check || value > 0x1F)
            {
                rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1/state").arg(id), QString("invalid value, %1, for parameter, gradient/%2").arg(gradient[param].toString()).arg(param)));
                ok = false;
            }
        }
        else if (param == "style")
        {
            QString styleName = gradient[param].toString();
            bool valid = false;
            for (auto &s: styles)
            {
                if (styleName == s.name && (styleBitmap & (0x01 << (s.value >> 1)))) {
                    valid = true;
                    break;
                }
            }
            if (!valid)
            {
                rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1/state").arg(id), QString("invalid value, %1, for parameter, gradient/%2").arg(gradient[param].toString()).arg(param)));
                ok = false;
            }
        }
        else if (param == "color_adjustment" || param == "offset_adjustment")
        {
            quint8 value = gradient[param].toUInt(&check);
            if (!check || value > 0x07)
            {
                rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1/state").arg(id), QString("invalid value, %1, for parameter, gradient/%2").arg(gradient[param].toString()).arg(param)));
                ok = false;
            }
        }
        else
        {
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/lights/%1/state").arg(id), QString("parameter, gradient/%1, not available").arg(param)));
            ok = false;
        }
    }
    return ok;
}

bool DeRestPluginPrivate::validateHueLightState(ApiResponse &rsp, const LightNode *lightNode, QVariantMap &map, QList<QString> &validatedParameters)
{
    bool ok = false;
    bool hasErrors = false;

    for (QVariantMap::const_iterator p = map.begin(); p != map.end(); p++)
    {
        bool paramOk = false;
        bool valueOk = false;
        QString param = p.key();

        if (param == "on" && lightNode->item(RStateOn))
        {
            paramOk = true;
            if (map[param].type() == QVariant::Bool)
            {
                valueOk = true;
                validatedParameters.append(param);
            }
        }
        else if (param == "bri" && lightNode->item(RStateBri))
        {
            paramOk = true;
            if (map[param].type() == QVariant::Double)
            {
                const uint bri = map[param].toUInt(&ok);
                if (ok && bri <= 0xFF)
                {
                    // Clamp to 254
                    valueOk = true;
                    validatedParameters.append(param);
                    map["bri"] = bri > 0xFE ? 0xFE : bri;
                }
            }
        }
        else if (param == "ct"  && lightNode->item(RStateCt))
        {
            paramOk = true;
            if (map[param].type() == QVariant::Double)
            {
                const quint16 ctMin = lightNode->toNumber(RCapColorCtMin);
                const quint16 ctMax = lightNode->toNumber(RCapColorCtMax);
                const uint ct = map[param].toUInt(&ok);
                if (ok && ct <= 0xFFFF)
                {
                    // Clamp between ctMin and ctMax
                    valueOk = true;
                    validatedParameters.append(param);
                    map["ct"] = (ctMin < 500 && ct < ctMin) ? ctMin : (ctMax > ctMin && ct > ctMax) ? ctMax : ct;
                }
            }
        }
        else if (param == "xy" && lightNode->item(RStateX) && lightNode->item(RStateY))
        {
            paramOk = true;
            if (map[param].type() == QVariant::List)
            {
                QVariantList xy = map["xy"].toList();
                if (xy[0].type() == QVariant::Double && xy[1].type() == QVariant::Double)
                {
                    const double x = xy[0].toDouble(&ok);
                    const double y = ok ? xy[1].toDouble(&ok) : 0;
                    if (ok && x >= 0.0 && x <= 1.0 && y >= 0.0 && y <= 1.0)
                    {
                        // Clamp to 0.9961
                        valueOk = true;
                        validatedParameters.append(param);
                        QVariantList xy;
                        xy.append(x > 0.9961 ? 0.9961 : x);
                        xy.append(y > 0.9961 ? 0.9961 : y);
                        map["xy"] = xy;
                    }
                }
            }
        }
        else if (param == "transitiontime")
        {
            paramOk = true;
            if (map[param].type() == QVariant::Double)
            {
                const uint tt = map[param].toUInt(&ok);
                if (ok && tt <= 0xFFFF)
                {
                    valueOk = true;
                    validatedParameters.append(param);
                }
            }
        }
        else if (param == "effect" && lightNode->item(RStateEffect))
        {
            paramOk = true;
            if (map[param].type() == QVariant::String)
            {
                QString e = map[param].toString();
                QStringList effectList = getHueEffectNames(lightNode->item(RCapColorEffects)->toNumber(), false);
                if (effectList.indexOf(e) >= 0)
                {
                    valueOk = true;
                    validatedParameters.append(param);
                }
            }
        }
        else if (param == "effect_duration")
        {
            paramOk = true;
            if (map[param].type() == QVariant::Double)
            {
                const uint ed = map[param].toUInt(&ok);
                if (ok && ed <= 216000)
                {
                    valueOk = true;
                    validatedParameters.append(param);
                }
            }
        }
        else if (param == "effect_speed")
        {
            paramOk = true;
            if (map[param].type() == QVariant::Double)
            {
                const double es = map[param].toDouble(&ok);
                if (ok && es >= 0.0 && es <= 1.0)
                {
                    valueOk = true;
                    validatedParameters.append(param);
                }
            }
        }

        if (!paramOk)
        {
            hasErrors = true;
            rsp.list.append(errorToMap(ERR_PARAMETER_NOT_AVAILABLE, QString("/lights/%1/state").arg(lightNode->id()), QString("parameter, %1, not available").arg(param)));
        }
        else if (!valueOk)
        {
            hasErrors = true;
            rsp.list.append(errorToMap(ERR_INVALID_VALUE, QString("/lights/%1/state").arg(lightNode->id()), QString("invalid value, %1, for parameter, %2").arg(map[param].toString()).arg(param)));
        }
    }

    return !hasErrors;
}

// MARK: - Light State

int DeRestPluginPrivate::setHueLightState(const ApiRequest &req, ApiResponse &rsp, TaskItem &taskRef, QVariantMap &map)
{
    bool ok;
    QList<QString> validatedParameters;

    bool hasErrors = validateHueLightState(rsp, taskRef.lightNode, map, validatedParameters);
    ok = addTaskHueManufacturerSpecificSetState(taskRef, map);

    if (ok)
    {
        if ((map.contains("on")) && (validatedParameters.contains("on")))
        {
            const bool targetOn = map["on"].toBool();

            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/lights/%1/state/on").arg(taskRef.lightNode->id())] = targetOn;
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);

            taskRef.lightNode->setValue(RStateOn, targetOn);
        }

        if ((map.contains("bri")) && (validatedParameters.contains("bri")))
        {
            const uint targetBri = map["bri"].toUInt();

            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/lights/%1/state/bri").arg(taskRef.lightNode->id())] = targetBri;
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);

            taskRef.lightNode->setValue(RStateBri, targetBri);
        }

        if ((map.contains("ct")) && (validatedParameters.contains("ct")))
        {
            const uint targetCt = map["ct"].toUInt();

            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/lights/%1/state/ct").arg(taskRef.lightNode->id())] = targetCt;
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);

            taskRef.lightNode->setValue(RStateCt, targetCt);
            taskRef.lightNode->setValue(RStateColorMode, QString("ct"));
        }

        if ((map.contains("xy")) && (validatedParameters.contains("xy")))
        {
            QVariantList xy = map["xy"].toList();
            const double targetX = xy[0].toDouble();
            const double targetY = xy[1].toDouble();

            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/lights/%1/state/xy").arg(taskRef.lightNode->id())] = xy;
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);

            taskRef.lightNode->setValue(RStateX, targetX * 65535);
            taskRef.lightNode->setValue(RStateY, targetY * 65535);
            taskRef.lightNode->setValue(RStateColorMode, QString("xy"));
        }

        if ((map.contains("transitiontime")) && (validatedParameters.contains("transitiontime")))
        {
            const uint tt = map["transitiontime"].toUInt();

            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/lights/%1/state/transitiontime").arg(taskRef.lightNode->id())] = tt;
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);
        }

        if ((map.contains("effect")) && (validatedParameters.contains("effect")))
        {
            QString effect = map["effect"].toString();

            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/lights/%1/state/effect").arg(taskRef.lightNode->id())] = effect;
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);

            taskRef.lightNode->setValue(RStateEffect, effect);
            taskRef.lightNode->setValue(RStateColorMode, QString("effect"));
        }

        if ((map.contains("effect_duration")) && (validatedParameters.contains("effect_duration")))
        {
            const uint ed = map["effect_duration"].toUInt();

            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/lights/%1/state/effect_duration").arg(taskRef.lightNode->id())] = ed;
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);
        }

        if ((map.contains("effect_speed")) && (validatedParameters.contains("effect_speed")))
        {
            const double es = map["effect_speed"].toDouble();

            QVariantMap rspItem;
            QVariantMap rspItemState;
            rspItemState[QString("/lights/%1/state/effect_speed").arg(taskRef.lightNode->id())] = es;
            rspItem["success"] = rspItemState;
            rsp.list.append(rspItem);
        }
    }

    rsp.httpStatus = HttpStatusOk;
    rsp.etag = taskRef.lightNode->etag;

    return REQ_READY_SEND;
}
