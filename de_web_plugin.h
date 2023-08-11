/*
 * Copyright (c) 2017-2018 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef REST_PLUGIN_H
#define REST_PLUGIN_H

#include <list>
#include <QObject>
#include <deconz.h>

class DeRestWidget;
class DeRestPluginPrivate;
class DeviceWidget;

class QTimer;

#if DECONZ_LIB_VERSION < 0x010800
  #error "The REST plugin requires at least deCONZ library version 1.8.0."
#endif

class DeRestPlugin : public QObject,
                     public deCONZ::NodeInterface,
                     public deCONZ::HttpClientHandler
{
    Q_OBJECT
    Q_INTERFACES(deCONZ::NodeInterface)
#if QT_VERSION >= 0x050000
    Q_PLUGIN_METADATA(IID "org.dresden-elektronik.DeRestPlugin")
#endif
public:
    explicit DeRestPlugin(QObject *parent = 0);
    ~DeRestPlugin();
    // node interface
    const char *name();
    bool hasFeature(Features feature);
    QWidget *createWidget();
    QDialog *createDialog();

    // http client handler interface
    bool isHttpTarget(const QHttpRequestHeader &hdr);
    int handleHttpRequest(const QHttpRequestHeader &hdr, QTcpSocket *sock);
    void clientGone(QTcpSocket *sock);
    bool pluginActive() const;

public Q_SLOTS:
    bool dbSaveAllowed() const;
    void idleTimerFired();
    void refreshAll();
    void startZclAttributeTimer(int delay);
    void stopZclAttributeTimer();
    void checkZclAttributeTimerFired();
    void appAboutToQuit();
    bool startUpdateFirmware();
    const QString &getNodeName(quint64 extAddress) const;

Q_SIGNALS:
    void nodeUpdated(quint64 extAddress, QString key, QString value);

private:
    QTimer *m_idleTimer = nullptr;
    QTimer *m_readAttributesTimer = nullptr;
    friend class DeRestWidget;
    DeRestWidget *m_w = nullptr;
    DeRestPluginPrivate *d = nullptr;
};

#endif // REST_PLUGIN_H
