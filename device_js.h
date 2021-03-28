#ifndef DEVICEJS_H
#define DEVICEJS_H

#include <QObject>
#include <QString>
#include <QVariant>
#include <memory>

class Resource;
class ResourceItem;

namespace deCONZ
{
    class ApsDataIndication;
    class ZclAttribute;
    class ZclFrame;
}

/*
    "parse": ["parseXiaomiSpecialAttribute/4", "0x01", "0x02", "0x21", "$raw"],
    "parse": ["parseXiaomiSpecialAttribute/4", "0x03", "0x00", "0x28", "$raw"]
    "parse": ["parseGenericAttribute/4", 1, "0x0000", "0xff0d", "$old == 0 ? $raw : $old"],
    "write": ["writeGenericAttribute/6", 1, "0x0000", "0xff0d", "0x20", "0x11F5", "$raw"],


    Expressions:

        { old = R.item('state/battery').val; let old1 = Item.val; }
*/

enum class JsEvalResult
{
    Error,
    Ok
};

class DeviceJsPrivate;
class DeviceJs
{
public:

    DeviceJs();
    ~DeviceJs();
    JsEvalResult evaluate(const QString &expr);
    void setResource(Resource *r);
    void setApsIndication(const deCONZ::ApsDataIndication &ind);
    void setZclFrame(const deCONZ::ZclFrame &zclFrame);
    void setZclAttribute(const deCONZ::ZclAttribute &attr);
    void setItem(const ResourceItem *item);
    QVariant result();
    void reset();
    QString errorString() const;

private:
    std::unique_ptr<DeviceJsPrivate> d;
};

#endif // DEVICEJS_H
