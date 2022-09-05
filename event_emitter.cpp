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
#include "rest_node_base.h"
#include "de_web_plugin_private.h"

static EventEmitter *instance_ = nullptr;

static bool isDuplicate(size_t pos, const std::vector<Event> &queue, const Event &e)
{
    for (size_t i = pos; i < queue.size(); i++)
    {
        const Event &x = queue[i];

        if (e.deviceKey() != x.deviceKey()) {continue;}
        if (e.resource() != x.resource()) continue;
        if (e.what() != x.what()) continue;
        if (e.num() != x.num()) continue;
        if (e.id() != x.id()) continue;
        if (e.hasData() != x.hasData()) continue;
        if (e.hasData() && e.dataSize() != x.dataSize()) continue;

        return true;
    }

    return false;
}

EventEmitter::EventEmitter(QObject *parent) :
    QObject(parent)
{
    m_queue.reserve(64);

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(0);
    connect(m_timer, &QTimer::timeout, this, &EventEmitter::timerFired);

    Q_ASSERT(instance_ == nullptr);
    instance_ = this;
}

void EventEmitter::enqueueEvent(const Event &event)
{
    RestNodeBase *restNode = nullptr;

    // workaround to attach DeviceKey to an event
    // TODO DDF remove dependency on plugin
    if (event.deviceKey() == 0 && (event.resource() == RSensors || event.resource() == RLights))
    {
        if (event.resource() == RSensors)
        {
            restNode = plugin->getSensorNodeForId(event.id());
            if (!restNode)
            {
                restNode = plugin->getSensorNodeForUniqueId(event.id());
            }
        }
        else if (event.resource() == RLights)
        {
            restNode = plugin->getLightNodeForId(event.id());
        }
    }

    if (event.isUrgent())
    {
        m_urgentQueue.push_back(event);
    }
    else
    {
        if (restNode && restNode->address().ext() > 0)
        {
            Event e2 = event;
            e2.setDeviceKey(restNode->address().ext());
            if (!isDuplicate(m_pos, m_queue, e2))
            {
                m_queue.push_back(e2);
            }
        }
        else if (!isDuplicate(m_pos, m_queue, event))
        {
            m_queue.push_back(event);
        }
    }

    if (!m_timer->isActive())
    {
        m_timer->start();
    }
}

EventEmitter::~EventEmitter()
{
    instance_ = nullptr;
}

void EventEmitter::process()
{
    QElapsedTimer t;
    t.start();

    while (t.elapsed() < 10 && (m_urgentQueue.size() || m_queue.size()))
    {
        size_t i = 0;
        while (i < m_urgentQueue.size())
        {
            emit eventNotify(m_urgentQueue[i]);
            i++;

            if (i == m_urgentQueue.size())
            {
                m_urgentQueue.clear();
                break;
            }
        }

        DBG_Assert(m_urgentQueue.empty());
        if (m_pos < m_queue.size())
        {
            m_pos++;
            emit eventNotify(m_queue[m_pos - 1]);
            if (m_pos == m_queue.size())
            {
                m_queue.clear();
                m_pos = 0;
            }
        }
    }
}

void EventEmitter::timerFired()
{
    process();

    if (!m_queue.empty() && !m_timer->isActive())
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
