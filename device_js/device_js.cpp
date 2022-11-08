/*
 * Copyright (c) 2021-2022 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifdef USE_QT_JS_ENGINE
#include "deconz/aps.h"
#include "device_js.h"
#include "device_js_wrappers.h"

#include <QJSEngine>
#include <QMetaProperty>

static DeviceJs *_djs = nullptr; // singleton
static DeviceJsPrivate *_djsPriv = nullptr; // singleton

class DeviceJsPrivate
{
public:
    QJSEngine engine;
    QJSValue result;
    JsResource *jsResource = nullptr;
    JsZclAttribute *jsZclAttribute = nullptr;
    JsZclFrame *jsZclFrame = nullptr;
    JsResourceItem *jsItem = nullptr;
    JsUtils *jsUtils = nullptr;
    const deCONZ::ApsDataIndication *apsInd = nullptr;
    std::vector<ResourceItem*> itemsSet;
};

// Polyfills for older Qt versions
static const char *PF_String_prototype_padStart = "String.prototype.padStart = String.prototype.padStart || "
                                     "function (targetLength, padString) { return Utils.padStart(this, targetLength, padString); } ";
static const char *PF_Math_log10 = "Math.log10 = Math.log10 || function(x) { return Utils.log10(x) };";

DeviceJs::DeviceJs() :
    d(new DeviceJsPrivate)
{
    Q_ASSERT(_djs == nullptr); // enforce singleton

    _djsPriv = d.get();
#if QT_VERSION > 0x050700
    d->engine.installExtensions(QJSEngine::ConsoleExtension);
#endif
    d->jsResource = new JsResource(&d->engine);
    auto jsR = d->engine.newQObject(d->jsResource);
    d->engine.globalObject().setProperty("R", jsR);

    d->jsZclAttribute = new JsZclAttribute(&d->engine);
    auto jsAttr = d->engine.newQObject(d->jsZclAttribute);
    d->engine.globalObject().setProperty("Attr", jsAttr);

    d->jsZclFrame = new JsZclFrame(&d->engine);
    auto jsZclFrame = d->engine.newQObject(d->jsZclFrame);
    d->engine.globalObject().setProperty("ZclFrame", jsZclFrame);

    d->jsItem = new JsResourceItem(&d->engine);
    auto jsItem = d->engine.newQObject(d->jsItem);
    d->engine.globalObject().setProperty("Item", jsItem);

    d->jsUtils = new JsUtils(&d->engine);
    auto jsUtils = d->engine.newQObject(d->jsUtils);
    d->engine.globalObject().setProperty("Utils", jsUtils);


    // apply polyfills
    d->engine.evaluate(PF_String_prototype_padStart);
    d->engine.evaluate(PF_Math_log10);

    _djs = this;
}

DeviceJs::~DeviceJs()
{
    _djs = nullptr;
    _djsPriv = nullptr;
}

DeviceJs *DeviceJs::instance()
{
    Q_ASSERT(_djs);
    return _djs;
}

const std::vector<ResourceItem *> &DeviceJs::itemsSet() const
{
    return d->itemsSet;
}

/* Keep track of all resource items which are set via:
   Item.val = 1 and R.item('..').val = 2)
   during JS evaluation.
 */
void DeviceJS_ResourceItemValueChanged(ResourceItem *item)
{
    Q_ASSERT(_djsPriv);

    const auto i = std::find(_djsPriv->itemsSet.cbegin(), _djsPriv->itemsSet.cend(), item);

    if (i == _djsPriv->itemsSet.cend())
    {
        _djsPriv->itemsSet.push_back(item);
    }
}

JsEvalResult DeviceJs::evaluate(const QString &expr)
{
    d->result = d->engine.evaluate(expr);
    if (d->result.isError())
    {
        return JsEvalResult::Error;
    }

    return JsEvalResult::Ok;
}

JsEvalResult DeviceJs::testCompile(const QString &expr)
{
    Q_UNUSED(expr)
    return JsEvalResult::Ok;
}

void DeviceJs::setResource(Resource *r)
{
    d->jsResource->r = r;
}

void DeviceJs::setResource(const Resource *r)
{
    d->jsResource->r = const_cast<Resource*>(r);
}

void DeviceJs::setApsIndication(const deCONZ::ApsDataIndication &ind)
{
    d->apsInd = &ind;
    d->engine.globalObject().setProperty(QLatin1String("SrcEp"), int(ind.srcEndpoint()));
    d->engine.globalObject().setProperty(QLatin1String("ClusterId"), int(ind.clusterId()));
}

void DeviceJs::setZclFrame(const deCONZ::ZclFrame &zclFrame)
{
    d->jsZclFrame->zclFrame = &zclFrame;
}

void DeviceJs::setZclAttribute(const deCONZ::ZclAttribute &attr)
{
    d->jsZclAttribute->attr = &attr;
}

void DeviceJs::setItem(ResourceItem *item)
{
    d->jsItem->item = item;
}

void DeviceJs::setItem(const ResourceItem *item)
{
    d->jsItem->item = const_cast<ResourceItem*>(item);
}

QVariant DeviceJs::result()
{
    return d->result.toVariant();
}

void DeviceJs::reset()
{
    d->apsInd = nullptr;
    d->jsItem->item = nullptr;
    d->jsResource->r = nullptr;
    d->jsZclAttribute->attr = nullptr;
    d->jsZclFrame->zclFrame = nullptr;
    d->engine.collectGarbage();
}

void DeviceJs::clearItemsSet()
{
    d->itemsSet.clear();
}

QString DeviceJs::errorString() const
{
    return d->result.toString();
}
#endif // USE_QT_JS_ENGINE

#ifdef USE_DUKTAPE_JS_ENGINE

// implementation in device_js_duktape.cpp

#endif // USE_DUKTAPE_JS_ENGINE
