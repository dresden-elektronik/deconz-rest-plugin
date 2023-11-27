/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

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

    R.item(suffix) -> gets Item object, for example the 'config.offset'
    R.endpoints    -> array of active device endpoints, e.g. [1, 2]  (read only)
    Item.val       -> ResourceItem value (read/write)
    Item.name      -> ResourceItem name like 'state/on' (read only)
    Attr.id        -> attribute id (number, read only)
    Attr.dataType  -> attribute datatype (number, read only)
    Attr.val       -> attribute value (read only)
    Attr.index     -> attribute index (read only), 0 based, tells which attribute in a frame is  currently processed

    ### under consideration (not implemented)
    R.parent --> get the parent resource object (Device)
    R.subDevice(uniqueId) --> get another sub-device resource object
    Item.lastSet --> timestamp when item was last set
    Item.lastChanged --> timestamp when item was last changed
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
    JsEvalResult testCompile(const QString &expr);
    void setResource(Resource *r);
    void setResource(const Resource *r);
    void setApsIndication(const deCONZ::ApsDataIndication &ind);
    void setZclFrame(const deCONZ::ZclFrame &zclFrame);
    void setZclAttribute(int attrIndex, const deCONZ::ZclAttribute &attr);
    void setItem(ResourceItem *item);
    void setItem(const ResourceItem *item);
    QVariant result();
    void reset();
    void clearItemsSet();
    QString errorString() const;
    static DeviceJs *instance();
    const std::vector<ResourceItem*> &itemsSet() const;

private:
    std::unique_ptr<DeviceJsPrivate> d;
};

void DeviceJS_ResourceItemValueChanged(ResourceItem *item);

#endif // DEVICEJS_H
