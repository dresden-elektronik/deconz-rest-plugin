/*
 * Copyright (c) 2016-2018 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"

/*! Inits permit join manager.

    The manager will observe and ensure the global permit join state.
 */
void DeRestPluginPrivate::initPermitJoin()
{
    permitJoinFlag = false;
    permitJoinTimer = new QTimer(this);
    permitJoinTimer->setSingleShot(false);
    connect(permitJoinTimer, SIGNAL(timeout()),
            this, SLOT(permitJoinTimerFired()));
    permitJoinTimer->start(1000);
    permitJoinLastSendTime = QTime::currentTime();

    resendPermitJoinTimer = new QTimer(this);
    resendPermitJoinTimer->setSingleShot(true);
    connect(resendPermitJoinTimer, SIGNAL(timeout()),
            this, SLOT(resendPermitJoinTimerFired()));
}

/*! Sets the permit join interval

    \param duration specifies the interval in which joining is enabled
               - 0 disabled
               - 1..254 duration in seconds until joining will be disabled
               - 255 always permit
 * \return
 */
bool DeRestPluginPrivate::setPermitJoinDuration(uint8_t duration)
{
    if (gwPermitJoinDuration != duration)
    {
        gwPermitJoinDuration = duration;
    }

    // force resend
    permitJoinLastSendTime = QTime();
    return true;
}

/*! Handle broadcasting of permit join interval.

    This is done every PERMIT_JOIN_SEND_INTERVAL to ensure
    any node in the network has the same settings.
 */
void DeRestPluginPrivate::permitJoinTimerFired()
{
    Q_Q(DeRestPlugin);
    if (!q->pluginActive() || !apsCtrl)
    {
        return;
    }

    if ((gwPermitJoinDuration > 0) && (gwPermitJoinDuration < 255))
    {
        permitJoinFlag = true;
        gwPermitJoinDuration--;
        updateEtag(gwConfigEtag); // update Etag so that webApp can count down permitJoin duration

        //periodically check if there are deleted lights and undelete them
        if (gwPermitJoinDuration % 10 == 0)
        {
            std::vector<LightNode>::iterator i = nodes.begin();
            std::vector<LightNode>::iterator end = nodes.end();

            for (; i != end; ++i)
            {
                if (i->state() == LightNode::StateDeleted)
                {
                    if (i->isAvailable())
                    {
                        i->setState(LightNode::StateNormal);
                        i->setNeedSaveDatabase(true);
                        queSaveDb(DB_LIGHTS, DB_SHORT_SAVE_DELAY);
                    }
                }
            }
        }
    }

    if (gwPermitJoinDuration == 0 && permitJoinFlag)
    {
        int i = 0;
        const deCONZ::Node *node = nullptr;

        // try to add light nodes even if they existed in deCONZ bevor and therefore
        // no node added event will be triggert in this phase
        while (apsCtrl->getNode(i, &node) == 0)
        {
            if (node && !node->isZombie() &&
                !node->nodeDescriptor().isNull() && node->nodeDescriptor().receiverOnWhenIdle())
            {
                addLightNode(node);
            }
            i++;
        }

        permitJoinFlag = false;
    }

    if (!isInNetwork())
    {
        return;
    }

    if (gwPermitJoinDuration == 0 && otauLastBusyTimeDelta() < (60 * 5))
    {
        // don't pollute channel while OTA is running
        return;
    }

    QTime now = QTime::currentTime();
    int diff = permitJoinLastSendTime.msecsTo(now);

    if (!permitJoinLastSendTime.isValid() || (diff > PERMIT_JOIN_SEND_INTERVAL))
    {
        deCONZ::ApsDataRequest apsReq;
        quint8 tcSignificance = 0x01;

        apsReq.setDstAddressMode(deCONZ::ApsNwkAddress);
        apsReq.dstAddress().setNwk(deCONZ::BroadcastRouters);
        apsReq.setProfileId(ZDP_PROFILE_ID);
        apsReq.setClusterId(ZDP_MGMT_PERMIT_JOINING_REQ_CLID);
        apsReq.setDstEndpoint(ZDO_ENDPOINT);
        apsReq.setSrcEndpoint(ZDO_ENDPOINT);
        apsReq.setTxOptions(0);
        apsReq.setRadius(0);

        QDataStream stream(&apsReq.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        stream << (uint8_t)now.second(); // seqno
        stream << gwPermitJoinDuration;
        stream << tcSignificance;

        DBG_Assert(apsCtrl != 0);

        if (!apsCtrl)
        {
            return;
        }

        // set for own node
        apsCtrl->setPermitJoin(gwPermitJoinDuration);

        // broadcast
        if (apsCtrl->apsdeDataRequest(apsReq) == deCONZ::Success)
        {
            DBG_Printf(DBG_INFO, "send permit join, duration: %d\n", gwPermitJoinDuration);
            permitJoinLastSendTime = now;
        }
        else
        {
            DBG_Printf(DBG_INFO, "send permit join failed\n");
        }
    }
}

/*! Check if permitJoin is > 60 seconds then resend permitjoin with 60 seconds
 */
void DeRestPluginPrivate::resendPermitJoinTimerFired()
{
    resendPermitJoinTimer->stop();
    if (gwPermitJoinDuration <= 1)
    {
        if (gwPermitJoinResend > 0)
        {

            if (gwPermitJoinResend >= 60)
            {
                setPermitJoinDuration(60);
            }
            else
            {
                setPermitJoinDuration(gwPermitJoinResend);
            }
            gwPermitJoinResend -= 60;
            updateEtag(gwConfigEtag);
            if (gwPermitJoinResend <= 0)
            {
                gwPermitJoinResend = 0;
                return;
            }

        }
        else if (gwPermitJoinResend == 0)
        {
            setPermitJoinDuration(0);
            return;
        }
    }
    else if (gwPermitJoinResend == 0)
    {
        setPermitJoinDuration(0);
        return;
    }
    resendPermitJoinTimer->start(1000);
}
