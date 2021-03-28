#ifndef DEVICE_JS_WRAPPERS_H
#define DEVICE_JS_WRAPPERS_H

#include <QObject>
#include <QJSEngine>
#include <QJSValue>


class Resource;
class ResourceItem;

namespace deCONZ
{
    class ApsDataIndication;
    class ZclAttribute;
    class ZclFrame;
}

class JsResource : public QObject
{
Q_OBJECT

public:
    Resource *r = nullptr;

    JsResource(QJSEngine *parent = nullptr);

public Q_SLOTS:
    QJSValue item(const QString &suffix);
};

class JsResourceItem : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVariant val READ value WRITE setValue NOTIFY valueChanged)

public:
    ResourceItem *item = nullptr;

    JsResourceItem(QObject *parent = nullptr);
    ~JsResourceItem();

public Q_SLOTS:
    QJSValue str() const;
    QVariant value() const;
    void setValue(const QVariant &val);

Q_SIGNALS:
    void valueChanged();
};

#endif // DEVICE_JS_WRAPPERS_H
