/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

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
    QVariant value() const;
    void setValue(const QVariant &val);

Q_SIGNALS:
    void valueChanged();
};

class JsZclAttribute : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVariant val READ value)

public:
    const deCONZ::ZclAttribute *attr = nullptr;

    JsZclAttribute(QObject *parent = nullptr);

public Q_SLOTS:
    QVariant value() const;
};

#endif // DEVICE_JS_WRAPPERS_H
