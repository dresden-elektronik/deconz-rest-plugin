/*
 * Copyright (c) 2016-2023 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QApplication>
#include <QString>
#include <QTcpSocket>
#include <QVariantMap>
#include "de_web_plugin_private.h"
#include "deconz/u_rand32.h"

// duration to wait for scan responses
#define TL_SCAN_WAIT_TIME 250
#define TL_TRANSACTION_TIMEOUT 7000 // default 8s, subtract 1s for sanity

#define TL_RECONNECT_NOW          100 // small delay to prevent false positives
#define TL_RECONNECT_CHECK_DELAY  5000
#define TL_DISCONNECT_CHECK_DELAY 100

#define TL_SCAN_COUNT (touchlinkChannel == 11 ? 5 : 1)

// Touchlink ZCL command ids
#define TL_CMD_SCAN_REQ                    0x00
#define TL_CMD_SCAN_RSP                    0x01
#define TL_CMD_DEVICE_INFORMATION_REQ      0x02
#define TL_CMD_DEVICE_INFORMATION_RSP      0x03
#define TL_CMD_IDENTIFY_REQ                0x06
#define TL_CMD_RESET_TO_FACTORY_NEW_REQ    0x07
#define TL_CMD_NETWORK_START_REQ           0x10
#define TL_CMD_NETWORK_START_RSP           0x11
#define TL_CMD_NETWORK_JOIN_ROUTER_REQ     0x12
#define TL_CMD_NETWORK_JOIN_ROUTER_RSP     0x13
#define TL_CMD_NETWORK_JOIN_ENDDEVICE_REQ  0x14
#define TL_CMD_NETWORK_JOIN_ENDDEVICE_RSP  0x15
#define TL_CMD_NETWORK_UPDATE_REQ          0x16
#define TL_CMD_ENDPOINT_INFORMATION        0x40

// flag for factory new in scan response 9th byte
#define FACTORY_NEW_FLAG 0x01

#define NETWORK_ATTEMPS 10

/*! Init the touchlink api and helpers.
 */
void DeRestPluginPrivate::initTouchlinkApi()
{
    touchlinkState = TL_Idle;
    touchlinkCtrl = deCONZ::TouchlinkController::instance();

    DBG_Assert(touchlinkCtrl != 0);

    connect(touchlinkCtrl, SIGNAL(startInterpanModeConfirm(deCONZ::TouchlinkStatus)),
            this, SLOT(startTouchlinkModeConfirm(deCONZ::TouchlinkStatus)));

    connect(touchlinkCtrl, SIGNAL(sendInterpanConfirm(deCONZ::TouchlinkStatus)),
            this, SLOT(sendTouchlinkConfirm(deCONZ::TouchlinkStatus)));

    connect(touchlinkCtrl, SIGNAL(interpanIndication(QByteArray)),
            this, SLOT(interpanDataIndication(QByteArray)));

    touchlinkTimer = new QTimer(this);
    touchlinkTimer->setSingleShot(true);
    connect(touchlinkTimer, SIGNAL(timeout()),
            this, SLOT(touchlinkTimerFired()));
}

/*! Touchlink REST API broker.
    \param req - request data
    \param rsp - response data
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::handleTouchlinkApi(const ApiRequest &req, ApiResponse &rsp)
{
    if (req.path[2] != "touchlink")
    {
        return REQ_NOT_HANDLED;
    }

    // POST /api/<apikey>/touchlink/scan
    if ((req.path.size() == 4) && (req.hdr.method() == "POST") && (req.path[3] == "scan"))
    {
        return touchlinkScan(req, rsp);
    }
    // GET /api/<apikey>/touchlink/scan
    if ((req.path.size() == 4) && (req.hdr.method() == "GET") && (req.path[3] == "scan"))
    {
        return getTouchlinkScanResults(req, rsp);
    }
    // POST /api/<apikey>/touchlink/<id>/identify
    if ((req.path.size() == 5) && (req.hdr.method() == "POST") && (req.path[4] == "identify"))
    {
        return identifyLight(req, rsp);
    }
    // POST /api/<apikey>/touchlink/<id>/reset
    if ((req.path.size() == 5) && (req.hdr.method() == "POST") && (req.path[4] == "reset"))
    {
        return resetLight(req, rsp);
    }

    return REQ_NOT_HANDLED;
}

/*! POST /api/<apikey>/touchlink/scan
    \param req - request data
    \param rsp - response data
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::touchlinkScan(const ApiRequest &req, ApiResponse &rsp)
{
    Q_UNUSED(req);

    if (touchlinkState != TL_Idle)
    {
        rsp.httpStatus = HttpStatusServiceUnavailable;
        return REQ_READY_SEND;
    }

    uint32_t transactionId = U_rand32();

    if (transactionId == 0)
    {
        transactionId++;
    }

    touchlinkAction = TouchlinkScan;
    touchlinkChannel = 11; // start channel
    touchlinkScanCount = 0;
    touchlinkScanResponses.clear();
    touchlinkScanTime = QDateTime::currentDateTime();
    touchlinkReq.setTransactionId(transactionId);

    touchlinkDisconnectNetwork();

    rsp.httpStatus = HttpStatusOk;

    return REQ_READY_SEND;
}

/*! GET /api/<apikey>/touchlink/scan
    \param req - request data
    \param rsp - response data
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::getTouchlinkScanResults(const ApiRequest &req, ApiResponse &rsp)
{
    Q_UNUSED(req);
    rsp.httpStatus = HttpStatusOk;

    bool scanning = false;

    if ((touchlinkAction == TouchlinkScan) && (touchlinkState != TL_Idle))
    {
        scanning = true;
    }

    rsp.map["scanstate"] = (scanning ? "scanning" : "idle");
    rsp.map["lastscan"] = touchlinkScanTime.toString("yyyy-MM-ddTHH:mm:ss");

    QVariantMap result;


    std::vector<ScanResponse>::const_iterator i = touchlinkScanResponses.begin();
    std::vector<ScanResponse>::const_iterator end = touchlinkScanResponses.end();

    for (; i != end; ++i)
    {
        QVariantMap item;
        item["address"] = QString("0x%1").arg(i->address.ext(), int(16), int(16), QChar('0'));
        item["factorynew"] = i->factoryNew;
        item["rssi"] = (double)i->rssi;
        item["channel"] = (double)i->channel;
        item["panid"] = (double)i->panid;
        result[i->id] = item;
    }

    rsp.map["result"] = result;

    return REQ_READY_SEND;
}

/*! POST /api/<apikey>/touchlink/<id>/identify
    \param req - request data
    \param rsp - response data
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::identifyLight(const ApiRequest &req, ApiResponse &rsp)
{
    /*
     * - disconnect
     * - start interpan mode
     * - send interpan scan request
     * - send interpan identify
     * - reconnect
     */

    if (touchlinkState != TL_Idle)
    {
        rsp.httpStatus = HttpStatusServiceUnavailable;
        return REQ_READY_SEND;
    }

    QString id = req.path[3];

    touchlinkDevice.id.clear(); // mark as undefined

    { // search the device according to its id
        std::vector<ScanResponse>::const_iterator i = touchlinkScanResponses.begin();
        std::vector<ScanResponse>::const_iterator end = touchlinkScanResponses.end();

        for (; i != end; ++i)
        {
            if (i->id == id)
            {
                touchlinkDevice = *i;
                break;
            }
        }
    }

    if (touchlinkDevice.id.isEmpty())
    {
        rsp.httpStatus = HttpStatusNotFound;
        return REQ_READY_SEND;
    }

    uint32_t transactionId = U_rand32();

    if (transactionId == 0)
    {
        transactionId++;
    }

    touchlinkReq.setTransactionId(transactionId);
    touchlinkAction = TouchlinkIdentify;
    touchlinkChannel = touchlinkDevice.channel;

    touchlinkDisconnectNetwork();

    rsp.httpStatus = HttpStatusOk;

    return REQ_READY_SEND;
}

/*! POST /api/<apikey>/touchlink/<id>/reset
    \param req - request data
    \param rsp - response data
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::resetLight(const ApiRequest &req, ApiResponse &rsp)
{
    /*
     * - disconnect
     * - start interpan mode
     * - send interpan scan request
     * - send interpan reset to factory new request
     * - reconnect
     */

    if (touchlinkState != TL_Idle)
    {
        rsp.httpStatus = HttpStatusServiceUnavailable;
        return REQ_READY_SEND;
    }

    QString id = req.path[3];

    touchlinkDevice.id.clear(); // mark as undefined

    { // search the device according to its id
        std::vector<ScanResponse>::const_iterator i = touchlinkScanResponses.begin();
        std::vector<ScanResponse>::const_iterator end = touchlinkScanResponses.end();

        for (; i != end; ++i)
        {
            if (i->id == id)
            {
                touchlinkDevice = *i;
                break;
            }
        }
    }

    if (touchlinkDevice.id.isEmpty())
    {
        rsp.httpStatus = HttpStatusNotFound;
        return REQ_READY_SEND;
    }

    uint32_t transactionId = U_rand32();

    if (transactionId == 0)
    {
        transactionId++;
    }

    touchlinkReq.setTransactionId(transactionId);
    touchlinkAction = TouchlinkReset;
    touchlinkChannel = touchlinkDevice.channel;

    DBG_Printf(DBG_TLINK, "start touchlink reset for 0x%016llX\n", touchlinkDevice.address.ext());

    touchlinkDisconnectNetwork();

    rsp.httpStatus = HttpStatusOk;

    return REQ_READY_SEND;
}

/*! Starts the interpan mode.
    \param channel the channel which shall be used for interpan communication
 */
void DeRestPluginPrivate::startTouchlinkMode(uint8_t channel)
{
    DBG_Printf(DBG_TLINK, "start interpan mode on channel %u\n", channel);
    touchlinkChannel = channel;
    touchlinkState = TL_StartingInterpanMode;

    if (touchlinkCtrl->startInterpanMode(touchlinkChannel) != 0)
    {
        DBG_Printf(DBG_TLINK, "start interpan mode on channel %u failed\n", channel);
        // abort and restore previous network state
        touchlinkStartReconnectNetwork(TL_RECONNECT_NOW);
    }
}

/*! Callback slot for the touchlink mode confirmation.
    \param status tells if starting the touchlink mode was successful
 */
void DeRestPluginPrivate::startTouchlinkModeConfirm(deCONZ::TouchlinkStatus status)
{
    DBG_Printf(DBG_TLINK, "start touchlink mode %s\n", (status == deCONZ::TouchlinkSuccess ? "success" : "failed"));

    if (touchlinkState != TL_StartingInterpanMode)
    {
        return;
    }

    if (status != deCONZ::TouchlinkSuccess)
    {
        // abort and restore previous network state
        touchlinkStartReconnectNetwork(TL_RECONNECT_NOW);
        return;
    }

    if (touchlinkAction == TouchlinkScan)
    {
        sendTouchlinkScanRequest();
    }
    else if (touchlinkAction == TouchlinkIdentify)
    {
        // must be send prior to identify request because we need a valid transaction id
        sendTouchlinkScanRequest();
    }
    else if (touchlinkAction == TouchlinkReset)
    {
        // must be send prior to identify request because we need a valid transaction id
        sendTouchlinkScanRequest();
    }
    else
    {
        // abort and restore previous network state
        touchlinkStartReconnectNetwork(TL_RECONNECT_NOW);
    }
}

/*! Request to disconnect from network.
 */
void DeRestPluginPrivate::touchlinkDisconnectNetwork()
{
    DBG_Assert(touchlinkState == TL_Idle);

    if (touchlinkState != TL_Idle)
    {
        return;
    }

    DBG_Assert(apsCtrl != 0);

    if (!apsCtrl)
    {
        return;
    }

    touchlinkNetworkDisconnectAttempts = NETWORK_ATTEMPS;
    touchlinkNetworkConnectedBefore = gwRfConnectedExpected;
    touchlinkState = TL_DisconnectingNetwork;

    apsCtrl->setNetworkState(deCONZ::NotInNetwork);

    touchlinkTimer->start(TL_DISCONNECT_CHECK_DELAY);
}

/*! Checks if network is disconnected to proceed with further actions.
 */
void DeRestPluginPrivate::checkTouchlinkNetworkDisconnected()
{
    if (touchlinkState != TL_DisconnectingNetwork)
    {
        return;
    }

    if (touchlinkNetworkDisconnectAttempts > 0)
    {
        touchlinkNetworkDisconnectAttempts--;
    }

    if (isInNetwork())
    {
        if (touchlinkNetworkDisconnectAttempts == 0)
        {
            DBG_Printf(DBG_TLINK, "disconnect from network failed, abort touchlink action\n");

            // even if we seem to be connected force a delayed reconnect attemp to
            // prevent the case that the disconnect happens shortly after here
            touchlinkStartReconnectNetwork(TL_RECONNECT_CHECK_DELAY);
        }
        else
        {
            DBG_Assert(apsCtrl != 0);
            if (apsCtrl)
            {
                DBG_Printf(DBG_TLINK, "disconnect from network failed, try again\n");
                apsCtrl->setNetworkState(deCONZ::NotInNetwork);
                touchlinkTimer->start(TL_DISCONNECT_CHECK_DELAY);
            }
            else
            {   // sanity
                touchlinkState = TL_Idle;
            }
        }

        return;
    }

    startTouchlinkMode(touchlinkChannel);
}

/*! Sends the touchlink scan request as broadcast.
 */
void DeRestPluginPrivate::sendTouchlinkScanRequest()
{
    touchlinkReq.setChannel(touchlinkChannel);
    touchlinkReq.setDstAddressMode(deCONZ::ApsNwkAddress);
    touchlinkReq.dstAddress().setNwk(0xFFFF);
    touchlinkReq.setPanId(0xFFFF);
    touchlinkReq.setClusterId(0x1000);
    touchlinkReq.setProfileId(ZLL_PROFILE_ID);

    touchlinkReq.asdu().clear();

    QDataStream stream(&touchlinkReq.asdu(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    uint8_t cmd = TL_CMD_SCAN_REQ;
    uint8_t frameControl = deCONZ::ZclFCProfileCommand | deCONZ::ZclFCDirectionClientToServer;
    uint8_t seq = touchlinkReq.transactionId();

    uint8_t zigbeeInfo = 0x02; // 0x01 /* router */ | 0x04 /* rx on when idle */;
    uint8_t zllInfo = 0x33;

    stream << frameControl;
    stream << seq;
    stream << cmd;
    stream << touchlinkReq.transactionId();
    stream << zigbeeInfo;
    stream << zllInfo;

    touchlinkScanCount++;

    DBG_Printf(DBG_TLINK, "send scan request TrId: 0x%08X\n", touchlinkReq.transactionId());

    if (touchlinkCtrl->sendInterpanRequest(touchlinkReq) == 0)
    {
        touchlinkState = TL_SendingScanRequest;
    }
    else
    {
        DBG_Printf(DBG_TLINK, "touchlink send scan request failed\n");
        // abort and restore previous network state
        touchlinkStartReconnectNetwork(TL_RECONNECT_NOW);
        return;
    }
}

/*! Sends the touchlink identify request to a device.
 */
void DeRestPluginPrivate::sendTouchlinkIdentifyRequest()
{
    touchlinkReq.setChannel(touchlinkChannel);
    touchlinkReq.setDstAddressMode(deCONZ::ApsExtAddress);
    touchlinkReq.dstAddress() = touchlinkDevice.address;
    touchlinkReq.setPanId(touchlinkDevice.panid);
    touchlinkReq.setClusterId(0x1000);
    touchlinkReq.setProfileId(ZLL_PROFILE_ID);

    touchlinkReq.asdu().clear();

    QDataStream stream(&touchlinkReq.asdu(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    uint8_t cmd = TL_CMD_IDENTIFY_REQ;
    uint8_t frameControl = deCONZ::ZclFCProfileCommand | deCONZ::ZclFCDirectionClientToServer;
    uint8_t seq = touchlinkReq.transactionId();

    uint16_t duration = 5;

    stream << frameControl;
    stream << seq;
    stream << cmd;
    stream << touchlinkReq.transactionId();
    stream << duration;

    DBG_Printf(DBG_TLINK, "send identify request TrId: 0x%08X\n", touchlinkReq.transactionId());
    if (touchlinkCtrl->sendInterpanRequest(touchlinkReq) == 0)
    {
        touchlinkState = TL_SendingIdentifyRequest;
    }
    else
    {
        DBG_Printf(DBG_TLINK, "touchlink send identify request failed\n");
        // abort and restore previous network state
        touchlinkStartReconnectNetwork(TL_RECONNECT_NOW);
    }
}

/*! Sends the touchlink reset request to a device.
 */
void DeRestPluginPrivate::sendTouchlinkResetRequest()
{
    touchlinkReq.setChannel(touchlinkChannel);
    touchlinkReq.setDstAddressMode(deCONZ::ApsExtAddress);
    touchlinkReq.dstAddress() = touchlinkDevice.address;
    touchlinkReq.setPanId(touchlinkDevice.panid);
    touchlinkReq.setClusterId(0x1000);
    touchlinkReq.setProfileId(ZLL_PROFILE_ID);

    touchlinkReq.asdu().clear();

    QDataStream stream(&touchlinkReq.asdu(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    uint8_t cmd = TL_CMD_RESET_TO_FACTORY_NEW_REQ;
    uint8_t frameControl = deCONZ::ZclFCProfileCommand | deCONZ::ZclFCDirectionClientToServer;
    uint8_t seq = touchlinkReq.transactionId();

    stream << frameControl;
    stream << seq;
    stream << cmd;
    stream << touchlinkReq.transactionId();

    DBG_Printf(DBG_TLINK, "send reset request TrId: 0x%08X\n", touchlinkReq.transactionId());
    if (touchlinkCtrl->sendInterpanRequest(touchlinkReq) == 0)
    {
        touchlinkState = TL_SendingResetRequest;
    }
    else
    {
        DBG_Printf(DBG_TLINK, "touchlink send reset request failed\n");
        // abort and restore previous network state
        touchlinkStartReconnectNetwork(TL_RECONNECT_NOW);
    }
}

/*! Starts a delayed action based on current touchlink state.
 */
void DeRestPluginPrivate::touchlinkTimerFired()
{
    switch (touchlinkState)
    {
    case TL_Idle:
        break;

    case TL_WaitScanResponses:
        touchlinkScanTimeout();
        break;

    case TL_ReconnectNetwork:
        touchlinkReconnectNetwork();
        break;

    case TL_DisconnectingNetwork:
        checkTouchlinkNetworkDisconnected();
        break;

    case TL_SendingScanRequest:
        sendTouchlinkScanRequest();
        break;

    default:
        DBG_Printf(DBG_TLINK, "touchlinkTimerFired() unhandled state %d\n", touchlinkState);
        break;
    }
}

/*! Confirmation callback for a interpan request.
    \param status tells if the request was sent
 */
void DeRestPluginPrivate::sendTouchlinkConfirm(deCONZ::TouchlinkStatus status)
{
    if (status != deCONZ::TouchlinkSuccess)
    {
        DBG_Printf(DBG_TLINK, "touchlink confirm status %d for action %d\n", status, touchlinkAction);
    }

    if (touchlinkState == TL_SendingScanRequest)
    {
        switch (touchlinkAction)
        {
        case TouchlinkScan:
        {
            if (touchlinkScanCount > TL_SCAN_COUNT)
            {
                touchlinkState = TL_WaitScanResponses;
                touchlinkTimer->start(TL_SCAN_WAIT_TIME);
            }
            else
            {
                touchlinkTimer->start(1);
            }

        }
            break;

        case TouchlinkIdentify:
        case TouchlinkReset:
        {
            touchlinkState = TL_WaitScanResponses;
            touchlinkTimer->start(TL_TRANSACTION_TIMEOUT);
        }
            break;

        default:
        {
            DBG_Printf(DBG_TLINK, "unknown touchlink action: %d, abort\n", touchlinkAction);
            // abort and restore previous network state
            touchlinkStartReconnectNetwork(TL_RECONNECT_NOW);
        }
            break;
        }
    }
    else if (touchlinkState == TL_WaitScanResponses)
    {
        // empty
    }
    else if ((touchlinkState == TL_SendingIdentifyRequest) ||
             (touchlinkState == TL_SendingResetRequest))
    {
        if (status == deCONZ::TouchlinkSuccess)
        {
            // mark the reset node as not available
            if (touchlinkState == TL_SendingResetRequest)
            {
                std::vector<LightNode>::iterator i = nodes.begin();
                std::vector<LightNode>::iterator end = nodes.end();

                for (; i != end; ++i)
                {
                    if (i->address().ext() == touchlinkDevice.address.ext())
                    {
                        // TODO: remove the node from groups
                        i->item(RStateReachable)->setValue(false);
                        updateEtag(i->etag);
                        updateEtag(gwConfigEtag);
                    }
                }
            }
        }

        // finished go back to normal operationg state and reconnect to network
        touchlinkStartReconnectNetwork(TL_RECONNECT_NOW);
    }
    else if (touchlinkState != TL_Idle)
    {
        DBG_Printf(DBG_TLINK, "touchlink send confirm in unexpected state: %d\n", touchlinkState);
    }
}

/*! Timer callback than duration for scan responses expires.
 */
void DeRestPluginPrivate::touchlinkScanTimeout()
{
    if (touchlinkState != TL_WaitScanResponses)
    {
        return;
    }

    if (touchlinkAction == TouchlinkReset || touchlinkAction == TouchlinkIdentify)
    {
        DBG_Printf(DBG_TLINK, "wait for scan response before reset/identify to fn timeout\n");
        touchlinkStartReconnectNetwork(TL_RECONNECT_NOW);
        return;
    }
    else if (touchlinkAction == TouchlinkScan)
    {
        if (touchlinkChannel < 26)
        {
            touchlinkChannel++;
            touchlinkScanCount = 0;
            startTouchlinkMode(touchlinkChannel);
        }
        else
        {
            DBG_Printf(DBG_TLINK, "scan finished found %u device(s)\n", (uint)touchlinkScanResponses.size());
            touchlinkStartReconnectNetwork(TL_RECONNECT_NOW);
        }
    }
}

/*! Callback if interpan data like touchlink frames are received.
    \param data the indication data
 */
void DeRestPluginPrivate::interpanDataIndication(const QByteArray &data)
{
    if (touchlinkState == TL_Idle)
    {
        DBG_Printf(DBG_TLINK, "discard ipan frame in TL_Idle state\n");
        return;
    }

    QDataStream stream(data);
    stream.setByteOrder(QDataStream::LittleEndian);

    uint16_t srcPanId;
    quint64 srcAddress;
    uint16_t dstPanId;
    uint8_t  dstAddressMode;
    quint64 dstExtAddress; // if dstAddressMode is 0x3
    uint16_t dstNwkAddress; // if dstAddressMode is 0x2 or 0x01
    uint16_t profileId;
    uint16_t clusterId;
    uint8_t asduLength;
    //uint8_t asdu[asduLength];
    uint8_t lqi;
    int8_t rssi;

    stream >> srcPanId;
    stream >> srcAddress;
    stream >> dstPanId;
    stream >> dstAddressMode;

    if (dstAddressMode == 0x03)
    {
        stream >> dstExtAddress;
    }
    else
    {
        stream >> dstNwkAddress;
    }

    stream >> profileId;
    stream >> clusterId;
    stream >> asduLength;

    QByteArray asdu;

    for (uint i = 0; i < asduLength; i++)
    {
        uint8_t byte;
        stream >> byte;
        asdu.append(byte);
    }

    stream >> lqi;
    stream >> rssi;

    // check if ZLL specific
    if ((profileId == ZLL_PROFILE_ID) && (clusterId == 0x1000) && (asdu.size() >= 3))
    {
//        uint8_t frameControl = asdu[0];
//        uint8_t seq = asdu[1];
        uint8_t cmd = asdu[2];

        ScanResponse scanResponse;

        if (cmd == TL_CMD_SCAN_RSP)
        {
            scanResponse.id = QString::number(touchlinkScanResponses.size() + 1);
            scanResponse.address.setExt(srcAddress);
            scanResponse.factoryNew = ((asdu[9] & FACTORY_NEW_FLAG) != 0);
            scanResponse.channel = touchlinkChannel;
            scanResponse.panid = srcPanId;
            scanResponse.transactionId = touchlinkReq.transactionId();
            scanResponse.rssi = rssi;

            DBG_Printf(DBG_TLINK, "scan response " FMT_MAC ", fn=%u, channel=%u rssi=%d TrId=0x%08X in state=%d action=%d\n",
                       (unsigned long long)scanResponse.address.ext(),
                       scanResponse.factoryNew, touchlinkChannel, rssi, scanResponse.transactionId,
                       touchlinkState, touchlinkAction);

            if (touchlinkAction == TouchlinkScan)
            {
                if (asdu.size() >= 9)
                {
                    std::vector<ScanResponse>::iterator i = touchlinkScanResponses.begin();
                    std::vector<ScanResponse>::iterator end = touchlinkScanResponses.end();

                    // check if already known
                    for (; i != end; ++i)
                    {
                        if (i->address.ext() == srcAddress)
                        {
                            // update transaction id
                            i->transactionId = touchlinkReq.transactionId();
                            return;
                        }
                    }

                    touchlinkScanResponses.push_back(scanResponse);
                }
            }
            else if (touchlinkAction == TouchlinkReset)
            {
                if (scanResponse.address.ext() == touchlinkDevice.address.ext())
                {
                    touchlinkTimer->stop();
                    sendTouchlinkResetRequest();
                }
            }
            else if (touchlinkAction == TouchlinkIdentify)
            {
                if (scanResponse.address.ext() == touchlinkDevice.address.ext())
                {
                    touchlinkTimer->stop();
                    sendTouchlinkIdentifyRequest();
                }
            }
        }
    }
}

/*! Reconnect to previous network state, trying serveral times if necessary.
    \param delay - the delay after which reconnecting shall be started
 */
void DeRestPluginPrivate::touchlinkStartReconnectNetwork(int delay)
{
    touchlinkState = TL_ReconnectNetwork;
    touchlinkNetworkReconnectAttempts = NETWORK_ATTEMPS;

    DBG_Printf(DBG_TLINK, "start reconnect to network\n");

    touchlinkTimer->stop();
    if (delay > 0)
    {
        touchlinkTimer->start(delay);
    }
    else
    {
        touchlinkReconnectNetwork();
    }
}

/*! Helper to reconnect to previous network state, trying serveral times if necessary.
 */
void DeRestPluginPrivate::touchlinkReconnectNetwork()
{
    if (touchlinkState != TL_ReconnectNetwork)
    {
        return;
    }

    if (isInNetwork())
    {
        touchlinkState = TL_Idle;
        DBG_Printf(DBG_TLINK, "reconnect network done\n");
        return;
    }

    // respect former state
    if (!touchlinkNetworkConnectedBefore)
    {
        touchlinkState = TL_Idle;
        DBG_Printf(DBG_TLINK, "network was not connected before\n");
        return;
    }

    if (touchlinkNetworkReconnectAttempts > 0)
    {
        if (apsCtrl->networkState() != deCONZ::Connecting)
        {
            touchlinkNetworkReconnectAttempts--;

            if (apsCtrl->setNetworkState(deCONZ::InNetwork) != deCONZ::Success)
            {
                DBG_Printf(DBG_TLINK, "touchlink failed to reconnect to network try=%d\n", (NETWORK_ATTEMPS - touchlinkNetworkReconnectAttempts));
            }
            else
            {
                DBG_Printf(DBG_TLINK, "touchlink try to reconnect to network try=%d\n", (NETWORK_ATTEMPS - touchlinkNetworkReconnectAttempts));
            }
        }

        touchlinkTimer->start(TL_RECONNECT_CHECK_DELAY);
    }
    else
    {
        touchlinkState = TL_Idle;
        DBG_Printf(DBG_TLINK, "reconnect network failed\n");
    }
}

/*! Returns true while touchlink is running.
 */
bool DeRestPluginPrivate::isTouchlinkActive()
{
    return (touchlinkState != TL_Idle);
}
