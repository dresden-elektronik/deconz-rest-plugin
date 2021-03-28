#include "device_js.h"
#include "device_js_wrappers.h"

#include <QJSEngine>
#include <QMetaProperty>

class DeviceJsPrivate
{
public:
    QJSEngine engine;
    QJSValue result;
    JsResource *resourceWrapper = nullptr;
};

DeviceJs::DeviceJs() :
    d(new DeviceJsPrivate)
{

    d->engine.installExtensions(QJSEngine::ConsoleExtension);
    d->resourceWrapper = new JsResource(&d->engine);

    auto R = d->engine.newQObject(d->resourceWrapper);
    d->engine.globalObject().setProperty("R", R);
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
    d->resourceWrapper->r = r;

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
    Q_UNUSED(attr)
}

void DeviceJs::setItem(const ResourceItem *item)
{
    Q_UNUSED(item)
}

QVariant DeviceJs::result()
{
    return d->result.toVariant();
}

void DeviceJs::reset()
{
    d->resourceWrapper->r = nullptr;
    d->engine.collectGarbage();
}

QString DeviceJs::errorString() const
{
    return d->result.toString();
}
