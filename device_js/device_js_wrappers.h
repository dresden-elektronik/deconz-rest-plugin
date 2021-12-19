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
    const Resource *cr = nullptr;

    JsResource(QJSEngine *parent = nullptr);

public Q_SLOTS:
    QJSValue item(const QString &suffix);
};

class JsResourceItem : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVariant val READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(QString name READ name)

public:
    ResourceItem *item = nullptr;
    const ResourceItem *citem = nullptr;

    JsResourceItem(QObject *parent = nullptr);
    ~JsResourceItem();

public Q_SLOTS:
    QVariant value() const;
    void setValue(const QVariant &val);
    QString name() const;

Q_SIGNALS:
    void valueChanged();
};

class JsZclAttribute : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVariant val READ value)
    Q_PROPERTY(int id READ id)
    Q_PROPERTY(int dataType READ dataType)

public:
    const deCONZ::ZclAttribute *attr = nullptr;

    JsZclAttribute(QObject *parent = nullptr);

public Q_SLOTS:
    QVariant value() const;
    int id() const;
    int dataType() const;
};

class JsZclFrame : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int cmd READ cmd)
    Q_PROPERTY(int payloadSize READ payloadSize)
    Q_PROPERTY(bool isClCmd READ isClCmd)

public:
    const deCONZ::ZclFrame *zclFrame = nullptr;

    JsZclFrame(QObject *parent = nullptr);

public Q_SLOTS:
    int at(int i) const;
    int cmd() const;
    int payloadSize() const;
    bool isClCmd() const;
};

#endif // DEVICE_JS_WRAPPERS_H
