/*
 * Copyright (C) 2013 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "de_web_plugin_private.h"

/*! Inits permit join manager.

    The manager will observe and ensure the global permit join state.
 */
void DeRestPluginPrivate::initPermitJoin()
{
    permitJoinTimer = new QTimer(this);
    permitJoinTimer->setSingleShot(false);
    connect(permitJoinTimer, SIGNAL(timeout()),
            this, SLOT(permitJoinTimerFired()));
    permitJoinTimer->start(1000);
    permitJoinLastSendTime = QTime::currentTime();
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
        queSaveDb(DB_CONFIG, DB_SHORT_SAVE_DELAY);

        // force resend
        permitJoinLastSendTime = QTime();
    }
    return true;
}

/*! Handle broadcasting of permit join interval.

    This is done every PERMIT_JOIN_SEND_INTERVAL to ensure
    any node in the network has the same settings.
 */
void DeRestPluginPrivate::permitJoinTimerFired()
{
    if ((gwPermitJoinDuration > 0) && (gwPermitJoinDuration < 255))
    {
        gwPermitJoinDuration--;
    }

    if (!isInNetwork())
    {
        return;
    }

    QTime now = QTime::currentTime();
    int diff = permitJoinLastSendTime.msecsTo(now);

    if (!permitJoinLastSendTime.isValid() || (diff > PERMIT_JOIN_SEND_INTERVAL))
    {
        // only send if nothing else todo
        if (tasks.empty() && runningTasks.empty())
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
}
