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
#include "zdp/zdp.h"

#define CC_CHANNELCHANGE_WAIT_TIME         1000
#define CC_CHANNELCHANGE_WAIT_CONFIRM_TIME 10000
#define CC_CHANNELCHANGE_VERIFY_TIME       1000
#define CC_DISCONNECT_CHECK_DELAY 100
#define NETWORK_ATTEMPS 10
#define CC_RECONNECT_CHECK_DELAY  5000
#define CC_RECONNECT_NOW          100

/*! Init the change channel api and helpers.
 */
void DeRestPluginPrivate::initChangeChannelApi()
{
    channelChangeState = CC_Idle;
    ccRetries = 0;
    channelchangeTimer = new QTimer(this);
    channelchangeTimer->setSingleShot(true);
    connect(channelchangeTimer, SIGNAL(timeout()),
            this, SLOT(channelchangeTimerFired()));

    QTimer *wdTimer = new QTimer(this);
    wdTimer->setSingleShot(false);
    connect(wdTimer, SIGNAL(timeout()), this, SLOT(networkWatchdogTimerFired()));
    wdTimer->start(10000);
}


/*! Starts the whole channel changing process if connected.
    \param channel - user input channel
    \return true if connected.
 */
bool DeRestPluginPrivate::startChannelChange(quint8 channel)
{
    if (!isInNetwork())
    {
        return false;
    }

    ccRetries = 0;
    gwZigbeeChannel = channel;
    queSaveDb(DB_CONFIG, DB_SHORT_SAVE_DELAY);

    if (channelChangeState != CC_Idle)
    {
        DBG_Printf(DBG_INFO, "channel change in progress.\n");
        return true;
    }

    channelChangeState = CC_Verify_Channel;
    DBG_Printf(DBG_INFO_L2, "ChannelChangeState: CC_verify_Channel\n");
    channelchangeTimer->start(CC_CHANNELCHANGE_VERIFY_TIME);
    return true;
}


/*! Check if user input channel equals gateway channel.
    \param channel - user input channel
    \return true if equal
 */
bool DeRestPluginPrivate::verifyChannel(quint8 channel)
{

    DBG_Assert(apsCtrl != nullptr);
    if (!apsCtrl)
    {
        return false;
    }

    if (!isInNetwork())
    {
        return false;
    }

    quint8 currentChannel = apsCtrl->getParameter(deCONZ::ParamCurrentChannel);
    quint64 apsUseExtPanid = apsCtrl->getParameter(deCONZ::ParamApsUseExtendedPANID);
    quint64 tcAddress = apsCtrl->getParameter(deCONZ::ParamTrustCenterAddress);
    quint64 macAddress = apsCtrl->getParameter(deCONZ::ParamMacAddress);
    quint8 deviceType = apsCtrl->getParameter(deCONZ::ParamDeviceType);

    bool ok = true;

    if (currentChannel != channel)
    {
        ok = false;
    }

    if (deviceType == deCONZ::Coordinator)
    {
        if (apsUseExtPanid != 0)
        {
            ok = false;
        }
        else if (tcAddress != macAddress)
        {
            ok = false;
        }
    }

    if (ok)
    {
        DBG_Printf(DBG_INFO, "network configuration verified!\n");
        return true;
    }
    else
    {
        DBG_Printf(DBG_INFO, "network configuration NOT verified!\n");
        return false;
    }
}


/*! Sends request to gateway to change channel
    \param channel - user input channel
 */
void DeRestPluginPrivate::changeChannel(quint8 channel)
{
    if (!apsCtrl)
    {
    }
    else if ((gwDeviceAddress.ext() & deMacPrefix) != deMacPrefix)
    {
        // require valid mac address
    }
    else if (ccRetries < 3)
    {
        DBG_Assert(channel >= 11 && channel <= 26);
        if (apsCtrl && channel >= 11 && channel <= 26)
        {
            uint8_t nwkUpdateId = apsCtrl->getParameter(deCONZ::ParamNetworkUpdateId);
            if (nwkUpdateId < 255)
            {
                nwkUpdateId++;
            }
            else if (nwkUpdateId == 255)
            {
                nwkUpdateId = 1;
            }
            const quint8 zdpSeq = ZDP_NextSequenceNumber();
            const quint32 scanChannels = (1 << static_cast<uint>(channel));
            const quint8 scanDuration = 0xfe; //special value = channel change

            DBG_Printf(DBG_INFO, "change channel with nwkUpdateId = %u\n", nwkUpdateId);

            apsCtrl->setParameter(deCONZ::ParamCurrentChannel, channel);
            apsCtrl->setParameter(deCONZ::ParamNetworkUpdateId, nwkUpdateId);

            deCONZ::ApsDataRequest req;

#if QT_VERSION < QT_VERSION_CHECK(5,15,0)
            req.setTxOptions(nullptr);
#endif
            req.setDstEndpoint(ZDO_ENDPOINT);
            req.setDstAddressMode(deCONZ::ApsNwkAddress);
            req.dstAddress().setNwk(deCONZ::BroadcastRxOnWhenIdle);
            req.setProfileId(ZDP_PROFILE_ID);
            req.setClusterId(ZDP_MGMT_NWK_UPDATE_REQ_CLID);
            req.setSrcEndpoint(ZDO_ENDPOINT);
            req.setRadius(0);

            QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
            stream.setByteOrder(QDataStream::LittleEndian);
            stream << zdpSeq;
            stream << scanChannels;
            stream << scanDuration;
            stream << nwkUpdateId;

            if (apsCtrlWrapper.apsdeDataRequest(req) == deCONZ::Success)
            {
                channelChangeApsRequestId = req.id();
                DBG_Printf(DBG_INFO, "change channel to %d, channel mask = 0x%08X\n", channel, scanChannels);
                channelChangeState = CC_WaitConfirm;
                channelchangeTimer->start(CC_CHANNELCHANGE_WAIT_CONFIRM_TIME);
                DBG_Printf(DBG_INFO_L2, "ChannelChangeState: CC_WaitConfirm\n");
                return;
            }
            else
            {
                DBG_Printf(DBG_ERROR, "cant send change channel\n");
            }
        }

        channelChangeState = CC_Verify_Channel;
        DBG_Printf(DBG_INFO_L2, "ChannelChangeState: CC_verify_Channel\n");
        channelchangeTimer->start(CC_CHANNELCHANGE_VERIFY_TIME);
        return;
    }

    if (apsCtrl && isInNetwork())
    {
        if (gwZigbeeChannel != apsCtrl->getParameter(deCONZ::ParamCurrentChannel))
        {

        }
    }
    ccRetries = 0;
    channelChangeState = CC_Idle;
    DBG_Printf(DBG_INFO_L2, "ChannelChangeState: CC_Idle\n");
    DBG_Printf(DBG_INFO, "channel change not successful.\n");
    return;
}

/*! Handle confirmation of ZDP channel change request.
    \param success true on success
 */
void DeRestPluginPrivate::channelChangeSendConfirm(bool success)
{
    channelchangeTimer->stop();

    if (channelChangeState == CC_WaitConfirm)
    {
        if (success)
        {
            channelChangeDisconnectNetwork();
        }
        else
        {
            channelChangeState = CC_Verify_Channel;
            DBG_Printf(DBG_INFO_L2, "ChannelChangeState: CC_verify_Channel\n");
            channelchangeTimer->start(CC_CHANNELCHANGE_VERIFY_TIME);
        }
    }
}


/*! Request to disconnect from network.
 */
void DeRestPluginPrivate::channelChangeDisconnectNetwork()
{
    DBG_Assert(channelChangeState == CC_WaitConfirm);

    if (channelChangeState != CC_WaitConfirm)
    {
        return;
    }

    DBG_Assert(apsCtrl != 0);

    if (!apsCtrl)
    {
        return;
    }

    ccNetworkDisconnectAttempts = NETWORK_ATTEMPS;
    ccNetworkConnectedBefore = gwRfConnectedExpected;
    channelChangeState = CC_DisconnectingNetwork;
    DBG_Printf(DBG_INFO_L2, "ChannelChangeState: CC_DisconnectingNetwork\n");

    apsCtrl->setNetworkState(deCONZ::NotInNetwork);

    channelchangeTimer->start(CC_DISCONNECT_CHECK_DELAY);
}

/*! Checks if network is disconnected to proceed with further actions.
 */
void DeRestPluginPrivate::checkChannelChangeNetworkDisconnected()
{
    if (channelChangeState != CC_DisconnectingNetwork)
    {
        return;
    }

    if (ccNetworkDisconnectAttempts > 0)
    {
        ccNetworkDisconnectAttempts--;
    }

    if (isInNetwork())
    {
        if (ccNetworkDisconnectAttempts == 0)
        {
            DBG_Printf(DBG_INFO, "disconnect from network failed.\n");

            // even if we seem to be connected force a delayed reconnect attemp to
            // prevent the case that the disconnect happens shortly after here
            channelChangeStartReconnectNetwork(CC_RECONNECT_CHECK_DELAY);
        }
        else
        {
            DBG_Assert(apsCtrl != nullptr);
            if (apsCtrl)
            {
                DBG_Printf(DBG_INFO, "disconnect from network failed, try again\n");
                apsCtrl->setNetworkState(deCONZ::NotInNetwork);
                channelchangeTimer->start(CC_DISCONNECT_CHECK_DELAY);
            }
            else
            {   // sanity
                channelChangeState = CC_Idle;
                DBG_Printf(DBG_INFO_L2, "ChannelChangeState: CC_Idle\n");
            }
        }

        return;
    }
    channelChangeStartReconnectNetwork(CC_RECONNECT_NOW);
}

/*! Reconnect to previous network state, trying serveral times if necessary.
    \param delay - the delay after which reconnecting shall be started
 */
void DeRestPluginPrivate::channelChangeStartReconnectNetwork(int delay)
{
    channelChangeState = CC_ReconnectNetwork;
    DBG_Printf(DBG_INFO_L2, "ChannelChangeState: CC_ReconnectNetwork\n");
    ccNetworkReconnectAttempts = NETWORK_ATTEMPS;

    DBG_Printf(DBG_INFO, "start reconnect to network\n");

    channelchangeTimer->stop();
    if (delay > 0)
    {
        channelchangeTimer->start(delay);
    }
    else
    {
        channelChangeReconnectNetwork();
    }
}

/*! Helper to reconnect to previous network state, trying serveral times if necessary.
 */
void DeRestPluginPrivate::channelChangeReconnectNetwork()
{
    if (channelChangeState != CC_ReconnectNetwork)
    {
        return;
    }

    if (isInNetwork())
    {
        channelChangeState = CC_Verify_Channel;
        DBG_Printf(DBG_INFO_L2, "ChannelChangeState: CC_verify_Channel\n");
        channelchangeTimer->start(CC_CHANNELCHANGE_VERIFY_TIME);
        DBG_Printf(DBG_INFO, "reconnect network done\n");
        return;
    }

    // respect former state
    if (!ccNetworkConnectedBefore)
    {
        channelChangeState = CC_Idle;
        DBG_Printf(DBG_INFO_L2, "ChannelChangeState: CC_Idle\n");
        DBG_Printf(DBG_INFO, "network was not connected before\n");
        return;
    }

    if (ccNetworkReconnectAttempts > 0)
    {
        if (apsCtrl->networkState() != deCONZ::Connecting)
        {
            ccNetworkReconnectAttempts--;

            const quint8 deviceType = apsCtrl->getParameter(deCONZ::ParamDeviceType);

            if (deviceType == deCONZ::Coordinator)
            {
                apsCtrl->setParameter(deCONZ::ParamApsUseExtendedPANID, 0); // will become mac address
                apsCtrl->setParameter(deCONZ::ParamTrustCenterAddress, gwDeviceAddress.ext());
                apsCtrl->setParameter(deCONZ::ParamStaticNwkAddress, false);
                apsCtrl->setParameter(deCONZ::ParamNwkAddress, 0);
            }

            if (apsCtrl->setNetworkState(deCONZ::InNetwork) != deCONZ::Success)
            {
                DBG_Printf(DBG_INFO, "failed to reconnect to network try=%d\n", (NETWORK_ATTEMPS - ccNetworkReconnectAttempts));
            }
            else
            {
                DBG_Printf(DBG_INFO, "try to reconnect to network try=%d\n", (NETWORK_ATTEMPS - ccNetworkReconnectAttempts));
            }
        }

        channelchangeTimer->start(CC_RECONNECT_CHECK_DELAY);
    }
    else
    {
        channelChangeState = CC_Idle;
        DBG_Printf(DBG_INFO_L2, "ChannelChangeState: CC_Idle\n");
        DBG_Printf(DBG_INFO, "reconnect network failed\n");
    }
}

/*! Checks if network uses parameters it's suppoesed to be.
 */
void DeRestPluginPrivate::networkWatchdogTimerFired()
{
    if (!apsCtrl || channelChangeState != CC_Idle)
    {
        return;
    }

    if (!isInNetwork())
    {
        return;
    }

    if (saveDatabaseItems & DB_NOSAVE)
    {
        return; // deCONZ will restart shortly
    }

    quint8 channel = apsCtrl->getParameter(deCONZ::ParamCurrentChannel);
    quint32 channelMask = apsCtrl->getParameter(deCONZ::ParamChannelMask);
    quint64 apsUseExtPanid = apsCtrl->getParameter(deCONZ::ParamApsUseExtendedPANID);
    quint64 tcAddress = apsCtrl->getParameter(deCONZ::ParamTrustCenterAddress);
    quint64 macAddress = apsCtrl->getParameter(deCONZ::ParamMacAddress);
    quint8 deviceType = apsCtrl->getParameter(deCONZ::ParamDeviceType);

    if (gwZigbeeChannel == 0)
    {
        if (channel >= 11 && channel <= 26)
        {
            gwZigbeeChannel = channel;
            queSaveDb(DB_CONFIG, DB_SHORT_SAVE_DELAY);
        }
    }

    if (channel < 11 || channel > 26)
    {
        DBG_Printf(DBG_INFO, "invalid current channel %u (TODO)\n", channel);
        return;
    }

    if (channelMask && (channelMask & (1 << channel)) == 0)
    {
        DBG_Printf(DBG_INFO, "channel %u does not match channel mask 0x%08X (TODO)\n", channel, channelMask);
    }

    if (gwZigbeeChannel == 0)
    {
        DBG_Printf(DBG_INFO, "invalid gwZigbeeChannel %u (TODO)\n", gwZigbeeChannel);
        return;
    }
    else if (deviceType != deCONZ::Coordinator)
    {
        DBG_Printf(DBG_INFO, "unsupported device type %u (TODO)\n", deviceType);
        return;
    }
    else if ((macAddress & deMacPrefix) != deMacPrefix) // only support our mac address
    {
        DBG_Printf(DBG_INFO, "invalid mac address 0x%016llX\n", macAddress);
        return;
    }
    else if (gwZigbeeChannel < 11 || gwZigbeeChannel > 26)
    {
        DBG_Assert(0); // should never happen
        return;
    }

    bool needCheck = false;

    if (channel != gwZigbeeChannel)
    {
        gwZigbeeChannel = channel;
        saveDatabaseItems |= DB_CONFIG;
    }
    else if (deviceType == deCONZ::Coordinator)
    {
        if (apsUseExtPanid != 0)
        {
            needCheck = true;
            DBG_Printf(DBG_INFO, "apsUseExtPanid is 0x%016llX but should be 0, start reconfiguration\n", apsUseExtPanid);
        }

        if (tcAddress != macAddress)
        {
            needCheck = true;
            DBG_Printf(DBG_INFO, "tcAddress is 0x%016llX but should be 0x%016llX, start reconfiguration\n", tcAddress, macAddress);
        }

        if (needCheck)
        {
            gwDeviceAddress.setExt(macAddress);
            gwDeviceAddress.setNwk(0x0000);
        }
    }

    if (needCheck)
    {
        //startChannelChange(gwZigbeeChannel);
        DBG_Printf(DBG_INFO, "Skip automatic channel change, TODO warn user\n");
    }
}

/*! Starts a delayed action based on current channelchange state.
 */
void DeRestPluginPrivate::channelchangeTimerFired()
{
    switch (channelChangeState)
    {
    case CC_Idle:
        break;

    case CC_Verify_Channel:
        if (!verifyChannel(gwZigbeeChannel))
        {
            channelChangeState = CC_Change_Channel;
            DBG_Printf(DBG_INFO_L2, "ChannelChangeState: CC_Change_Channel\n");
            channelchangeTimer->start(CC_CHANNELCHANGE_WAIT_TIME);
        }
        else
        {
            channelChangeState = CC_Idle;
            DBG_Printf(DBG_INFO_L2, "ChannelChangeState: CC_Idle\n");
        }
        break;

    case CC_Change_Channel:
        ccRetries++;
        changeChannel(gwZigbeeChannel);
        break;

    case CC_ReconnectNetwork:
        channelChangeReconnectNetwork();
        break;

    case CC_DisconnectingNetwork:
        checkChannelChangeNetworkDisconnected();
        break;

    case CC_WaitConfirm:
        DBG_Printf(DBG_INFO, "channel change not successful.\n");
        channelChangeState = CC_Idle;
        break;

    default:
        DBG_Printf(DBG_INFO, "channelChangeTimerFired() unhandled state %d\n", channelChangeState);
        break;
    }
}
