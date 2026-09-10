/*
 * Copyright (c) 2021-2025 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QIODevice>
#include "device_descriptions.h"
#include "resource.h"
#include "state_change.h"

#define ONOFF_CLUSTER_ID      0x0006
#define ONOFF_COMMAND_OFF     0x00
#define ONOFF_COMMAND_ON      0x01
#define ONOFF_COMMAND_OFF_WITH_EFFECT  0x040

quint8 zclNextSequenceNumber(); // todo defined in de_web_plugin_private.h

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
    \returns 0 when nothing was sent, 1 if an APS request was enqueued
*/
int StateChange::tick(uint64_t extAddr, Resource *r, deCONZ::ApsController *apsCtrl)
{
    int result = 0;

    if (m_state == StateFinished || m_state == StateFailed)
    {
        return result;
    }

    Q_ASSERT(m_stateTimer.isValid());
    Q_ASSERT(m_changeTimer.isValid());

    const char *uniqueId = "";

    {
        const ResourceItem *item = r->item(RAttrUniqueId);
        if (item)
        {
            uniqueId = item->toCString();
        }
    }

    if (m_state == StateWaitSync)
    {
        if (m_stateTimer.elapsed() > m_stateTimeoutMs)
        {
            m_state = StateCallFunction;

            for (auto &i : m_items)
            {
                if (i.verified == VerifyUnknown) // didn't receive a ZCL attribute read or report command
                {
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
    else if (DA_ApsUnconfirmedRequests() > 5)
    {
        // wait
    }
    else if (m_state == StateCallFunction && m_changeFunction)
    {
        DBG_Printf(DBG_INFO, "SC tick --> StateCallFunction\n");
        if (m_changeFunction(r, this, apsCtrl) == 0)
        {
            for (auto &i : m_items)
            {
                if (i.verified == VerifyNotSynced)
                {
                    i.verified = VerifyUnknown; // read again
                }
            }
            m_stateTimer.start();
            m_state = StateWaitSync;
            result = 1;
        }
    }
    else if (m_state == StateRead && DA_ApsUnconfirmedRequestsForExtAddress(extAddr) == 0)
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
        m_readResult = {};
        if (item)
        {
            const auto &ddfItem = DDF_GetItem(item);
            const auto readFunction = DA_GetReadFunction(ddfItem.readParameters);
            if (readFunction && ddfItem.isValid())
            {
                m_readResult = readFunction(r, item, apsCtrl, ddfItem.readParameters);

                if (m_readResult.isEnqueued)
                {
                    DBG_Printf(DBG_INFO, "SC tick --> StateRead %s, %s\n", item->descriptor().suffix, uniqueId);
                    result = 1;
                }

                m_stateTimer.start();
                m_state = StateWaitSync;
            }
        }
    }

    return result;
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
    if (value.isValid())
    {
        m_items.push_back({suffix, value});
    }
    else
    {
        DBG_Printf(DBG_ERROR, "SC add invalid traget value for: %s\n", suffix);
    }
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

bool StateChange::operator==(const StateChange &other) const
{
     if (m_changeFunction == other.m_changeFunction && m_items.size() == other.m_items.size())
     {
         for (size_t i = 0; i < m_items.size(); i++)
         {
             if (m_items[i].suffix != other.m_items[i].suffix)
             {
                 return false;
             }
         }
         return true;
     }
     return false;
}

/*! Calls the ZCL write function of item(s) to write target value(s).

    \returns 0 - if the command has been enqueued, or a negative number on failure.
 */
int SC_WriteZclAttribute(const Resource *r, const StateChange *stateChange, deCONZ::ApsController *apsCtrl)
{
    Q_ASSERT(r);
    Q_ASSERT(stateChange);
    Q_ASSERT(apsCtrl);

    int written = 0;

    for (const auto &i : stateChange->items())
    {
        const auto *item = r->item(i.suffix);

        if (!item)
        {
            return -1;
        }

        const auto ddfItem = DDF_GetItem(item);

        if (ddfItem.writeParameters.isNull())
        {
            return -2;
        }

        const auto fn = DA_GetWriteFunction(ddfItem.writeParameters);

        if (!fn)
        {
            return -3;
        }

        // create a copy since item is const
        ResourceItem copy(item->descriptor());
        copy.setValue(i.targetValue);

        if (!fn(r, &copy, apsCtrl, ddfItem.writeParameters))
        {
            return -4;
        }

        written++;
    }

    return written > 0 ? 0 : -5;
}

/*! Sends a ZCL command to the on/off cluster.

    StateChange::parameters() -> "cmd"

        ONOFF_COMMAND_ON
        ONOFF_COMMAND_OFF
        ONOFF_COMMAND_OFF_WITH_EFFECT

    \returns 0 - if the command has been enqueued, or a negative number on failure.
 */
int SC_SetOnOff(const Resource *r, const StateChange *stateChange, deCONZ::ApsController *apsCtrl)
{
    Q_ASSERT(r);
    Q_ASSERT(stateChange);
    Q_ASSERT(apsCtrl);

    quint8 cmd = 0xff;

    if (r->parentResource())
    {
        r = r->parentResource(); // Device* has nwk/ext address
    }

    for (const auto &i : stateChange->parameters())
    {
        if (i.name == QLatin1String("cmd"))
        {
            bool ok;
            auto val =  i.value.toUInt(&ok);

            if (ok && (val == ONOFF_COMMAND_ON || val == ONOFF_COMMAND_OFF || val == ONOFF_COMMAND_OFF_WITH_EFFECT))
            {
                cmd = static_cast<quint8>(val);
            }
            break;
        }
    }

    if (cmd == 0xff)
    {
        return -1;
    }

    deCONZ::ApsDataRequest req;
    deCONZ::ZclFrame zclFrame;

    req.setClusterId(ONOFF_CLUSTER_ID);
    req.setProfileId(HA_PROFILE_ID);
    req.dstAddress().setNwk(r->item(RAttrNwkAddress)->toNumber());
    req.dstAddress().setExt(r->item(RAttrExtAddress)->toNumber());
    req.setDstAddressMode(deCONZ::ApsNwkAddress);
    req.setDstEndpoint(stateChange->dstEndpoint());
    req.setSrcEndpoint(0x01);

    zclFrame.payload().clear();
    zclFrame.setSequenceNumber(zclNextSequenceNumber());
    zclFrame.setCommandId(cmd);
    zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCDisableDefaultResponse);


    if (cmd == ONOFF_COMMAND_OFF_WITH_EFFECT)
    {
        const quint8 effect = 0;
        const quint8 variant = 0;
        QDataStream stream(&zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream << effect;
        stream << variant;
    }
//    else if (cmd == ONOFF_COMMAND_ON_WITH_TIMED_OFF)
//    {
//        const quint16 offWaitTime = 0;
//        QDataStream stream(&zclFrame.payload(), QIODevice::WriteOnly);
//        stream.setByteOrder(QDataStream::LittleEndian);
//        // stream << (quint8)0x80; // 0x01 accept only when on --> no, 0x80 overwrite ontime (yes, non standard)
//        stream << flags;
//        stream << ontime;
//        stream << offWaitTime;
//    }

    QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    zclFrame.writeToStream(stream);

    DBG_Printf(DBG_INFO, "SC_SetOnOff()\n");

    return apsCtrl->apsdeDataRequest(req) == deCONZ::Success ? 0 : -2;
}
