/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "device_js.h"
#include "device_js_wrappers.h"

#include <QJSEngine>
#include <QMetaProperty>

class DeviceJsPrivate
{
public:
    QJSEngine engine;
    QJSValue result;
    JsResource *jsResource = nullptr;
    JsZclAttribute *jsZclAttribute = nullptr;
    JsResourceItem *jsItem = nullptr;
};

DeviceJs::DeviceJs() :
    d(new DeviceJsPrivate)
{
    d->engine.installExtensions(QJSEngine::ConsoleExtension);

    d->jsResource = new JsResource(&d->engine);
    auto jsR = d->engine.newQObject(d->jsResource);
    d->engine.globalObject().setProperty("R", jsR);

    d->jsZclAttribute = new JsZclAttribute(&d->engine);
    auto jsAttr = d->engine.newQObject(d->jsZclAttribute);
    d->engine.globalObject().setProperty("Attr", jsAttr);

    d->jsItem = new JsResourceItem(&d->engine);
    auto jsItem = d->engine.newQObject(d->jsItem);
    d->engine.globalObject().setProperty("Item", jsItem);
}

DeviceJs::~DeviceJs() = default;

JsEvalResult DeviceJs::evaluate(const QString &expr)
{
    d->result = d->engine.evaluate(expr);
    if (d->result.isError())
    {
        return JsEvalResult::Error;
    }

    return JsEvalResult::Ok;
}

void DeviceJs::setResource(Resource *r)
{
    d->jsResource->r = r;
    d->jsResource->cr = r;
}

void DeviceJs::setResource(const Resource *r)
{
    d->jsResource->r = nullptr;
    d->jsResource->cr = r;
}

void DeviceJs::setApsIndication(const deCONZ::ApsDataIndication &ind)
{
    Q_UNUSED(ind);
}

void DeviceJs::setZclFrame(const deCONZ::ZclFrame &zclFrame)
{
    Q_UNUSED(zclFrame)
}

void DeviceJs::setZclAttribute(const deCONZ::ZclAttribute &attr)
{
    d->jsZclAttribute->attr = &attr;
}

void DeviceJs::setItem(ResourceItem *item)
{
    d->jsItem->item = item;
    d->jsItem->citem = item;
}

void DeviceJs::setItem(const ResourceItem *item)
{
    d->jsItem->item = nullptr;
    d->jsItem->citem = item;
}

QVariant DeviceJs::result()
{
    return d->result.toVariant();
}

void DeviceJs::reset()
{
    d->jsResource->r = nullptr;
    d->jsZclAttribute->attr = nullptr;
    d->engine.collectGarbage();
}

QString DeviceJs::errorString() const
{
    return d->result.toString();
}
