/*
 * Copyright (C) 2013 dresden elektronik ingenieurtechnik gmbh.
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

class DeRestPlugin : public QObject,
                     public deCONZ::NodeInterface,
                     public deCONZ::HttpClientHandler
{
    Q_OBJECT
    Q_INTERFACES(deCONZ::NodeInterface)

public:

    enum State
    {
        StateOff,
        StateIdle
    };

    enum Event
    {
        TaskAdded
    };

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

public Q_SLOTS:
    void nodeEvent(int event, const deCONZ::Node *node);
    void idleTimerFired();
    void refreshAll();
    void startReadTimer(int delay);
    void stopReadTimer();
    void checkReadTimerFired();
    void appAboutToQuit();

private:
    void taskHandler(Event event);
    void handleStateOff(Event event);
    void handleStateIdle(Event event);

    QTimer *m_idleTimer;
    QTimer *m_readAttributesTimer;
    State m_state;
    DeRestWidget *m_w;
    DeRestPluginPrivate *d;
};

#endif // REST_PLUGIN_H
