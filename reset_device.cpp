/*
 * Copyright (c) 2016 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "de_web_plugin_private.h"

#define CHECK_RESET_DEVICES 3000
#define WAIT_CONFIRM 2000
#define WAIT_INDICATION 5000

/*! Init the reset device api
 */
void DeRestPluginPrivate::initResetDeviceApi()
{
    resetDeviceTimer = new QTimer(this);
    resetDeviceTimer->setSingleShot(true);
    connect(resetDeviceTimer, SIGNAL(timeout()),
            this, SLOT(resetDeviceTimerFired()));
    zdpResetSeq = 0;
    lastNodeAddressExt = 0;
    resetDeviceState = ResetIdle;
    resetDeviceTimer->start(CHECK_RESET_DEVICES);
}

/*! Check all light nodes if they need to be reseted.
 */
void DeRestPluginPrivate::checkResetState()
{
    if (!apsCtrl || !isInNetwork() || searchSensorsState == SearchSensorsActive || searchLightsState == SearchLightsActive)
    {
        resetDeviceTimer->start(CHECK_RESET_DEVICES);
        return;
    }

    std::vector<LightNode>::iterator i = nodes.begin();
    std::vector<LightNode>::iterator end = nodes.end();

    for (; i != end; ++i)
    {
        if (/*i->isAvailable() && */ i->state() == LightNode::StateDeleted && i->resetRetryCount() > 0)
        {
            uint8_t retryCount = i->resetRetryCount();
            retryCount--;
            i->setResetRetryCount(retryCount);

            // check if light has already a new pairing
            for (const LightNode &l : nodes)
            {
                if (l.address().ext() == i->address().ext() && l.state() == LightNode::StateNormal)
                {
                    i->setResetRetryCount(0);
                    retryCount = 0; // do nothing
                    break;
                }
            }

            if (retryCount > 0 && i->address().ext() != lastNodeAddressExt) // prefer unhandled nodes
            {
                DBG_Printf(DBG_INFO, "reset device retries: %i\n", retryCount);
                // send mgmt_leave_request
                lastNodeAddressExt = i->address().ext();
                zdpResetSeq += 1;
                i->setZdpResetSeq(zdpResetSeq);

                deCONZ::ApsDataRequest req;

                req.setTxOptions(0);
                req.setDstEndpoint(ZDO_ENDPOINT);
                req.setDstAddressMode(deCONZ::ApsExtAddress);
                req.dstAddress().setExt(i->address().ext());
                req.setProfileId(ZDP_PROFILE_ID);
                req.setClusterId(ZDP_MGMT_LEAVE_REQ_CLID);
                req.setSrcEndpoint(ZDO_ENDPOINT);
                req.setRadius(0);

                QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
                stream.setByteOrder(QDataStream::LittleEndian);
                stream << zdpResetSeq; // seq no.
                stream << (quint64)i->address().ext(); // device address

                uint8_t flags = 0;
                //                    flags |= 0x40; // remove children
                //                    flags |= 0x80; // rejoin
                stream << flags; // flags

                if (apsCtrl->apsdeDataRequest(req) == deCONZ::Success)
                {
                    resetDeviceApsRequestId = req.id();
                    resetDeviceState = ResetWaitConfirm;
                    resetDeviceTimer->start(WAIT_CONFIRM);
                    DBG_Printf(DBG_INFO, "reset device apsdeDataRequest success\n");
                    return;
                }
                else
                {
                    DBG_Printf(DBG_ERROR, "can't send reset device apsdeDataRequest\n");
                }
            }
        }
        lastNodeAddressExt = 0;
    }

    std::vector<Sensor>::iterator si = sensors.begin();
    std::vector<Sensor>::iterator si_end = sensors.end();

    for (; si != si_end; ++si)
    {
        if (si->isAvailable() && si->resetRetryCount() > 0 && si->node())
        {
            if (!si->node()->nodeDescriptor().receiverOnWhenIdle())
            {
                // not supported yet
                continue;
            }

            uint8_t retryCount = si->resetRetryCount();
            retryCount--;
            si->setResetRetryCount(retryCount);
            DBG_Printf(DBG_INFO, "reset device retries: %i\n", retryCount);

            if (retryCount > 0 && si->address().ext() != lastNodeAddressExt) // prefer unhandled nodes
            {
                // send mgmt_leave_request
                lastNodeAddressExt = si->address().ext();
                zdpResetSeq += 1;
                si->setZdpResetSeq(zdpResetSeq);

                deCONZ::ApsDataRequest req;

                req.setTxOptions(0);
                req.setDstEndpoint(ZDO_ENDPOINT);
                req.setDstAddressMode(deCONZ::ApsExtAddress);
                req.dstAddress().setExt(si->address().ext());
                req.setProfileId(ZDP_PROFILE_ID);
                req.setClusterId(ZDP_MGMT_LEAVE_REQ_CLID);
                req.setSrcEndpoint(ZDO_ENDPOINT);
                req.setRadius(0);

                QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
                stream.setByteOrder(QDataStream::LittleEndian);
                stream << zdpResetSeq; // seq no.
                stream << (quint64)si->address().ext(); // device address

                uint8_t flags = 0;
                //                    flags |= 0x40; // remove children
                //                    flags |= 0x80; // rejoin
                stream << flags; // flags

                if (apsCtrl->apsdeDataRequest(req) == deCONZ::Success)
                {
                    resetDeviceApsRequestId = req.id();
                    resetDeviceState = ResetWaitConfirm;
                    resetDeviceTimer->start(WAIT_CONFIRM);
                    DBG_Printf(DBG_INFO, "reset device apsdeDataRequest success\n");
                    return;
                }
                else
                {
                    DBG_Printf(DBG_ERROR, "can't send reset device apsdeDataRequest\n");
                }

            }
        }
        lastNodeAddressExt = 0;
    }

    resetDeviceState = ResetIdle;
    resetDeviceTimer->start(CHECK_RESET_DEVICES);
}

/*! Handle confirmation of ZDP reset device request.
    \param success true on success
 */
void DeRestPluginPrivate::resetDeviceSendConfirm(bool success)
{
    if (resetDeviceState == ResetWaitConfirm)
    {
        resetDeviceTimer->stop();
        if (success)
        {
           resetDeviceState = ResetWaitIndication;
           resetDeviceTimer->start(WAIT_INDICATION);
        }
        else
        {
            resetDeviceState = ResetIdle;
            DBG_Printf(DBG_INFO, "reset device apsdeDataConfirm fail\n");
            resetDeviceTimer->start(CHECK_RESET_DEVICES);
        }
    }
}

/*! Handle mgmt leave response.
    \param ind a ZDP MgmtLeave_rsp
 */
void DeRestPluginPrivate::handleMgmtLeaveRspIndication(const deCONZ::ApsDataIndication &ind)
{
    if (resetDeviceState == ResetWaitIndication)
    {
        if (ind.asdu().size() < 2)
        {
            // at least seq number and status
            return;
        }

        resetDeviceTimer->stop();

        QDataStream stream(ind.asdu());
        stream.setByteOrder(QDataStream::LittleEndian);

        quint8 seqNo;
        quint8 status;

        stream >> seqNo;    // use SeqNo ?
        stream >> status;

        DBG_Printf(DBG_INFO, "MgmtLeave_rsp %s seq: %u, status 0x%02X \n", qPrintable(ind.srcAddress().toStringExt()), seqNo, status);

        if (status == deCONZ::ZdpSuccess || status == deCONZ::ZdpNotSupported)
        {
            // set retryCount and isAvailable for all endpoints of that device
            std::vector<LightNode>::iterator i;
            std::vector<LightNode>::iterator end = nodes.end();

            for (i = nodes.begin(); i != end; ++i)
            {

                if ((ind.srcAddress().hasExt() && i->address().ext() == ind.srcAddress().ext()) ||
                    (ind.srcAddress().hasNwk() && i->address().nwk() == ind.srcAddress().nwk()))
                {
                   i->setResetRetryCount(0);
                   if (i->state() == LightNode::StateDeleted)
                   {
                       i->item(RStateReachable)->setValue(false);
                   }
                }
            }

            std::vector<Sensor>::iterator s;
            std::vector<Sensor>::iterator send = sensors.end();

            for (s = sensors.begin(); s != send; ++s)
            {
                if ((ind.srcAddress().hasExt() && s->address().ext() == ind.srcAddress().ext()) ||
                    (ind.srcAddress().hasNwk() && s->address().nwk() == ind.srcAddress().nwk()))
                {
                   s->setResetRetryCount(0);
                   s->item(RConfigReachable)->setValue(false);
                }
            }
        }

        resetDeviceState = ResetIdle;
        resetDeviceTimer->start(CHECK_RESET_DEVICES);
    }
}

/*! Starts a delayed action based on current delete device state.
 */
void DeRestPluginPrivate::resetDeviceTimerFired()
{
    switch (resetDeviceState)
    {
    case ResetIdle:
    {
        checkResetState();
    }
        break;

    case ResetWaitConfirm:
    {
        DBG_Printf(DBG_INFO, "reset device wait for confirm timeout.\n");
        resetDeviceState = ResetIdle;
        resetDeviceTimer->start(CHECK_RESET_DEVICES);
    }
        break;

    case ResetWaitIndication:
    {
        DBG_Printf(DBG_INFO, "reset device wait for indication timeout.\n");
        resetDeviceState = ResetIdle;
        resetDeviceTimer->start(CHECK_RESET_DEVICES);
    }
        break;

    default:
        DBG_Printf(DBG_INFO, "deleteDeviceTimerFired() unhandled state %d\n", resetDeviceState);
        break;
    }
}
