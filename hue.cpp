/*
 * Handle Hue-specific FC03 cluster.
 */

#include <QString>
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"

#define HUE_EFFECTS_CLUSTER_ID 0xFC03

struct code {
    quint8 value;
    QString name;
};

code effects[] = {
    { 0x01, QLatin1String("candle") },
    { 0x02, QLatin1String("fireplace") },
    { 0x03, QLatin1String("loop") },
    { 0x09, QLatin1String("sunrise") },
    { 0x0a, QLatin1String("sparkle") }
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

/*! Return a list of effect names corresponding to the bitmap of supported effects.

   \param effectBitmap - the bitmap with supported effects (from 0x0011)
   \return QStringList of effect names
 */
QStringList DeRestPluginPrivate::getHueEffectNames(quint64 effectBitmap)
{
    QStringList names = {
        "none", "colorloop"
    };
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

        if (effectName == "none") {
            stream << (quint16) 0x0020; // clear effect
            stream << (quint8) 0; // off
        } else {
            stream << (quint16) 0x0021; // set effect (with on/off)
            stream << (quint8) 1; // on
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

void streamPoint(QDataStream &stream, double x, double y)
{
    const quint16 rawX = (x >= maxX) ? 4095 : floor(x * 4095 / maxX);
    const quint16 rawY = (y >= maxY) ? 4095 : floor(y * 4095 / maxY);
    stream << (quint8) (rawX & 0x0FF);
    stream << (quint8) (((rawX & 0xF00) >> 8) | ((rawY & 0x00F) << 4));
    stream << (quint8) ((rawY & 0xFF0) >> 4);
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
        stream << (quint16) 0x0004; // unknown

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
