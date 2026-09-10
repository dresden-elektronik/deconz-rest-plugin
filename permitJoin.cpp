/*
 * Copyright (c) 2016-2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "zdp/zdp.h"

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
}

/*! Sets the permit join interval

    \param duration specifies the interval in which joining is enabled
               - 0 disabled
               - >0 duration in seconds until joining will be disabled

 */
void DeRestPluginPrivate::setPermitJoinDuration(int duration)
{
    if (gwPermitJoinDuration != duration)
    {
        gwPermitJoinDuration = duration;
    }

    // force resend
    permitJoinLastSendTime.invalidate();
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

    if (gwPermitJoinDuration > 0)
    {
        gwPermitJoinDuration--;

        if (!permitJoinFlag)
        {
            permitJoinFlag = true;
            enqueueEvent(Event(RConfig, REventPermitjoinEnabled, gwPermitJoinDuration));
        }
        else
        {
            enqueueEvent(Event(RConfig, REventPermitjoinRunning, gwPermitJoinDuration));
        }

        if (DEV_TestManaged())
        {

        }
        else if ((gwPermitJoinDuration % 10) == 0) // TODO bad this needs to go
        {
            // try to add light nodes even if they existed in deCONZ bevor and therefore
            // no node added event will be triggert in this phase
            int i = 0;
            const deCONZ::Node *node = nullptr;
            while (apsCtrl->getNode(i, &node) == 0)
            {
                if (node && !node->isZombie() &&
                        !node->nodeDescriptor().isNull() && node->nodeDescriptor().receiverOnWhenIdle())
                {
                    addLightNode(node);
                }
                i++;
            }
        }

        updateEtag(gwConfigEtag); // update Etag so that webApp can count down permitJoin duration
    }

    if (!isInNetwork())
    {
        return;
    }

    auto ctrlPermitJoin = apsCtrl->getParameter(deCONZ::ParamPermitJoin);
    if (ctrlPermitJoin > 0 && gwPermitJoinDuration == 0)
    {
        // workaround since the firmware reports cached value instead hot value
        apsCtrl->setPermitJoin(gwPermitJoinDuration);
        permitJoinLastSendTime.invalidate(); // force broadcast
    }

    if (!permitJoinFlag)
    {

    }
    else if (!permitJoinLastSendTime.isValid() || (permitJoinLastSendTime.elapsed() > PERMIT_JOIN_SEND_INTERVAL && !gwdisablePermitJoinAutoOff))
    {
        deCONZ::ApsDataRequest apsReq;
        quint8 tcSignificance = 0x01;

        apsReq.setDstAddressMode(deCONZ::ApsNwkAddress);
        apsReq.dstAddress().setNwk(deCONZ::BroadcastRouters);
        apsReq.setProfileId(ZDP_PROFILE_ID);
        apsReq.setClusterId(ZDP_MGMT_PERMIT_JOINING_REQ_CLID);
        apsReq.setDstEndpoint(ZDO_ENDPOINT);
        apsReq.setSrcEndpoint(ZDO_ENDPOINT);
#if QT_VERSION < QT_VERSION_CHECK(5,15,0)
        apsReq.setTxOptions(0);
#endif
        apsReq.setRadius(0);

        QDataStream stream(&apsReq.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        static_assert (PERMIT_JOIN_SEND_INTERVAL / 1000 < 180, "permit join send interval < 180 seconds");
        static_assert (PERMIT_JOIN_SEND_INTERVAL / 1000 > 30, "permit join send interval > 30 seconds");

        int duration = qMin(gwPermitJoinDuration, (PERMIT_JOIN_SEND_INTERVAL / 1000) + 5);

        stream << ZDP_NextSequenceNumber();
        stream << static_cast<quint8>(duration);
        stream << tcSignificance;

        // set for own node
        apsCtrl->setPermitJoin(duration);

        // broadcast
        if (apsCtrl->apsdeDataRequest(apsReq) == deCONZ::Success)
        {
            DBG_Printf(DBG_INFO, "send permit join, duration: %d\n", duration);
            permitJoinLastSendTime.restart();

            if (gwPermitJoinDuration > 0)
            {
                GP_SendProxyCommissioningMode(apsCtrl, zclSeq++);
            }
        }
        else
        {
            DBG_Printf(DBG_INFO, "send permit join failed\n");
        }
    }

    if (gwPermitJoinDuration == 0 && permitJoinFlag)
    {
        permitJoinApiKey.clear();
        permitJoinFlag = false;
        enqueueEvent(Event(RConfig, REventPermitjoinDisabled, 0));
    }
}

void DeRestPluginPrivate::permitJoin(int seconds)
{
    if (seconds > 0)
    {
        int tmp = gwNetworkOpenDuration; // preserve configured duration
        gwNetworkOpenDuration = seconds;
        startSearchSensors();
        startSearchLights();
        gwNetworkOpenDuration = tmp;
    }
    else
    {
        gwPermitJoinDuration = 0;
    }
}
