/*
 * Copyright (c) 2017 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef POLL_MANAGER_H
#define POLL_MANAGER_H

#include <QObject>
#include <QDateTime>
#include <vector>
#include <deconz.h>

class QTimer;
class DeRestPluginPrivate;
class RestNodeBase;

/*! \class PollItem

    Item representing a node in the polling queue.
 */
class PollItem
{
public:
    // Resource related
    QString id;
    const char *prefix;
    std::vector<const char*> items;
    QDateTime tStart;
    // APS
    quint8 endpoint;
    deCONZ::Address address;
};

/*! \class PollManager

    Service to handle periodically polling of nodes.
 */
class PollManager : public QObject
{
    Q_OBJECT
public:
    enum PollState {
        StateIdle,
        StateWait,
        StateDone
    };
    explicit PollManager(QObject *parent = 0);
    void poll(RestNodeBase *restNode, const QDateTime &tStart = QDateTime());
    void delay(int ms);
    bool hasItems() const { return !items.empty(); }

signals:
    void done();

public slots:
    void apsdeDataConfirm(const deCONZ::ApsDataConfirm &conf);
    void pollTimerFired();

private:
    QTimer *timer;
    std::vector<PollItem> items;
    DeRestPluginPrivate *plugin;
    PollState pollState;
    quint8 apsReqId;
    deCONZ::Address dstAddr;
};

#endif // POLL_MANAGER_H
