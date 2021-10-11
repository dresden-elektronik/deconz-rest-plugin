/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QTimer>
#include <QElapsedTimer>
#include "event_emitter.h"

static EventEmitter *instance_ = nullptr;

EventEmitter::EventEmitter(QObject *parent) :
    QObject(parent)
{
    m_queue.reserve(64);

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(1);
    connect(m_timer, &QTimer::timeout, this, &EventEmitter::timerFired);

    Q_ASSERT(instance_ == nullptr);
    instance_ = this;
}

void EventEmitter::enqueueEvent(const Event &event)
{
    m_queue.push_back(event);

    if (!m_timer->isActive())
    {
        m_timer->start();
    }
}

EventEmitter::~EventEmitter()
{
    instance_ = nullptr;
}

void EventEmitter::timerFired()
{
    QElapsedTimer t;
    t.start();
    while (m_pos < m_queue.size() && t.elapsed() < 10)
    {
        emit eventNotify(m_queue[m_pos]);
        m_pos++;
        if (m_pos == m_queue.size())
        {
            m_queue.clear();
            m_pos = 0;
        }
    }

    if (!m_queue.empty())
    {
        m_timer->start();
    }
}

/*! Puts an event into the queue.
    \param event - the event
 */
void enqueueEvent(const Event &event)
{
    if (instance_)
    {
        instance_->enqueueEvent(event);
    }
}
