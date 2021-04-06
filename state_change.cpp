/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "resource.h"
#include "state_change.h"

StateChange::StateChange(StateChange::State initialState, StateChangeFunction_t fn, quint8 dstEndpoint) :
    m_state(initialState),
    m_changeFunction(fn),
    m_dstEndpoint(dstEndpoint)
{
    Q_ASSERT(initialState == StateCallFunction || initialState == StateWaitSync);
    Q_ASSERT(fn);

    m_stateTimer.start();
    m_changeTimer.start();
}

/*! Tick function for the inner state machine.

    Is called from the Device state machine on certain events.
*/
StateChange::State StateChange::tick(Resource *r, deCONZ::ApsController *apsCtrl)
{
    if (m_state == StateFinished || m_state == StateFailed)
    {
        return m_state;
    }

    auto rParent = r->parentResource() ? r->parentResource() : r;

    Q_ASSERT(rParent);
    Q_ASSERT(m_stateTimer.isValid());
    Q_ASSERT(m_changeTimer.isValid());

    if (m_state == StateWaitSync && rParent->item(RConfigReachable)->toBool())
    {
        if (m_stateTimer.elapsed() > m_stateTimeoutMs)
        {
            m_state = StateCallFunction;

            for (auto &i : m_items)
            {
                if (i.verified == VerifyUnknown) // didn't receive a ZCL attribute read or report command
                {
                    DBG_Printf(DBG_INFO, "SC tick --> StateRead\n");
                    m_state = StateRead;
                    break;
                }
            }
        }
    }

    if (m_state == StateFailed)
    {

    }
    else if (m_changeTimeoutMs > 0 && m_changeTimer.elapsed() > m_changeTimeoutMs)
    {
        m_state = StateFailed;
    }
    else if (m_state == StateCallFunction && m_changeFunction)
    {
        DBG_Printf(DBG_INFO, "SC tick --> StateCallFunction\n");
        if (m_changeFunction(r, this, apsCtrl) == 0)
        {
            m_stateTimer.start();
            m_changeCalls++;

            for (auto &i : m_items)
            {
                if (i.verified == VerifyNotSynced)
                {
                    i.verified = VerifyUnknown; // read again
                }
            }
            m_state = StateWaitSync;
        }
    }
    else if (m_state == StateRead)
    {
        ResourceItem *item = nullptr;
        for (auto &i : m_items)
        {
            if (i.verified == VerifyUnknown)
            {
                item = r->item(i.suffix);
                break;
            }
        }

        m_state = StateFailed;
        if (item && !item->readParameters().empty())
        {
            const auto fn = DA_GetReadFunction(item->readParameters());
            if (fn && fn(rParent, item, apsCtrl, &m_readResult))
            {
                if (m_readResult.isEnqueued)
                {
                    m_stateTimer.start();
                    m_state = StateWaitSync;
                }
            }
        }
    }

    return m_state;
}

/*! Should be called when the item was set by a ZCL read or report attribute command.

    When all items are verified the StateChange::state() is set to StateFinished.
 */
void StateChange::verifyItemChange(const ResourceItem *item)
{
    Q_ASSERT(item);

    size_t syncedItems = 0;

    if (item->valueSource() != ResourceItem::SourceDevice)
    {
        return;
    }

    for (auto &i : m_items)
    {
        if (i.suffix == item->descriptor().suffix)
        {
            if (i.targetValue == item->toVariant())
            {
                i.verified = VerifySynced;
                DBG_Printf(DBG_INFO, "SC %s: synced\n", i.suffix);
            }
            else
            {
                i.verified = VerifyNotSynced;
                DBG_Printf(DBG_INFO, "SC %s: not synced\n", i.suffix);
            }
        }

        if (i.verified == VerifySynced)
        {
            syncedItems++;
        }
    }

    if (syncedItems == m_items.size() && m_state != StateFinished)
    {
        m_state = StateFinished;
        DBG_Printf(DBG_INFO, "SC --> StateFinished\n");
    }
}

/*! Adds a target value. */
void StateChange::addTargetValue(const char *suffix, const QVariant &value)
{
    m_items.push_back({suffix, value});
}

/*! Adds a parameter. If the parameter already exsits it will be replaced. */
void StateChange::addParameter(const QString &name, const QVariant &value)
{
    auto i = std::find_if(m_parameters.begin(), m_parameters.end(), [name](const Param &x){
        return x.name == name;
    });

    if (i != m_parameters.end())
    {
        i->value = value;
    }
    else
    {
        m_parameters.push_back({name, value});
    }
}


