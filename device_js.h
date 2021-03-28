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

    # Javascript API

    This API can be used in expressions in "parse" and "write" functions.
    Beside this anything can be used what is supported by QJSEngine.

    If a expression/script is too long to write it in the DDF file it could saved in an external
    file and referenced as "file://<path>/some-script.js" instead of an JS expression string.
    The file path is relative to the DDF file directory.

    ## Global objects accessible in evaluate() call

    Following globals are scoped to the surrounding item object in the DDF

    R        – access related Resource (Device | Sensor | LightNode)
    Item     — acces related ResourceItem
    Attr     — acces parsed deCONZ::ZclAttribute (if available in "parse")
    ZclFrame — access parsed deCONZ::ZclFrame (if available in "parse)

    ### Object Methods

    R.item(suffix) --> gets Item object, for example the 'config.offset'
    Item.val   -> ResourceItem value (read/write)
    Attr.val   -> attribute value (read only)

    ### under consideration (not implemented)
    R.parent --> get the parent resource object (Device)
    R.subDevice(uniqueId) --> get another sub-device resource object
    Item.lastSet --> timestamp when item was last set
    Item.lastChanged --> timestamp when item was last changed
    Attr.type --> attribute datatype
    ZclFrame.attr(id) --> Attr object (if the ZclFrame has multiple)


    Example expressions:

    "parse"

        Item.val = Attr.val + R.item('config/offset').val
        Item.val = Attr.val
        Item.val = Attr.val << 16

    "write"

        let out = -1;
        if (Item.val === 'heat') out = 2;
        else if (Item.val === 'cool') out = 0;
        out; // becomes the result of the expression
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
    void setItem(ResourceItem *item);
    QVariant result();
    void reset();
    QString errorString() const;

private:
    std::unique_ptr<DeviceJsPrivate> d;
};

#endif // DEVICEJS_H
