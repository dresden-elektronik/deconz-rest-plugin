/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef EVENT_EMITTER_H
#define EVENT_EMITTER_H

#include <QObject>
#include <vector>
#include "event.h"

class QTimer;

void enqueueEvent(const Event &event);

class EventEmitter : public QObject
{
    Q_OBJECT

public:
    explicit EventEmitter(QObject *parent = nullptr);
    ~EventEmitter();

public Q_SLOTS:
    void process();
    void enqueueEvent(const Event &event);
    void timerFired();

Q_SIGNALS:
    void eventNotify(const Event&);

private:
    size_t m_pos = 0;
    QTimer *m_timer = nullptr;
    std::vector<Event> m_queue;
    std::vector<Event> m_urgentQueue;
};

#endif // EVENT_EMITTER_H
