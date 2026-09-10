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
#include <deconz/zdp_profile.h>
#include <deconz/aps_controller.h>
#include <deconz/dbg_trace.h>
#include <deconz/zcl.h>
#include "utils/utils.h"
#include "aps_controller_wrapper.h"

// enable domain specific string literals
using namespace deCONZ::literals;

//! Sends a ZCL Default Response based on parameters from the request in \p ind and \p zclFrame.
static bool ZCL_SendDefaultResponse(deCONZ::ApsController *apsCtrl, const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame, quint8 status)
{
    deCONZ::ApsDataRequest apsReq;

    // ZDP Header
    apsReq.dstAddress() = ind.srcAddress();
    apsReq.setDstAddressMode(ind.srcAddressMode());
    apsReq.setDstEndpoint(ind.srcEndpoint());
    apsReq.setSrcEndpoint(ind.dstEndpoint());
    apsReq.setProfileId(ind.profileId());
    apsReq.setRadius(0);
    apsReq.setClusterId(ind.clusterId());

    deCONZ::ZclFrame outZclFrame;
    outZclFrame.setSequenceNumber(zclFrame.sequenceNumber());
    outZclFrame.setCommandId(deCONZ::ZclDefaultResponseId);

    if (zclFrame.frameControl() & deCONZ::ZclFCDirectionServerToClient)
    {
        outZclFrame.setFrameControl(deCONZ::ZclFCProfileCommand | deCONZ::ZclFCDirectionClientToServer | deCONZ::ZclFCDisableDefaultResponse);
    }
    else
    {
        outZclFrame.setFrameControl(deCONZ::ZclFCProfileCommand | deCONZ::ZclFCDirectionServerToClient | deCONZ::ZclFCDisableDefaultResponse);
    }

    if (zclFrame.manufacturerCode_t() != 0x0000_mfcode)
    {
        outZclFrame.setFrameControl(outZclFrame.frameControl() | deCONZ::ZclFCManufacturerSpecific);
        outZclFrame.setManufacturerCode(zclFrame.manufacturerCode_t());
    }

    { // ZCL payload
        QDataStream stream(&outZclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream << zclFrame.commandId();
        stream << status;
    }

    { // ZCL frame
        QDataStream stream(&apsReq.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        outZclFrame.writeToStream(stream);
    }

    return apsCtrl->apsdeDataRequest(apsReq) == deCONZ::Success;
}

//! Returns true if \p zclFrame requires a ZCL Default Response.
static bool ZCL_NeedDefaultResponse(const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame)
{
    if (zclFrame.isDefaultResponse())
    {
        return false;
    }

    if (ind.dstAddressMode() == deCONZ::ApsNwkAddress) // only respond to unicast
    {
        if (!(zclFrame.frameControl() & deCONZ::ZclFCDisableDefaultResponse))
        {
            return true;
        }
    }

    return false;
}

//! Returns true if \p req contains a specific or ZCL Default Response for \p indZclFrame.
static bool ZCL_IsResponse(const deCONZ::ZclFrame &indZclFrame, const deCONZ::ApsDataRequest &req)
{
    if (req.asdu().size() < 3) // need at least frame control | seqno | command id
    {
        return false;
    }

    // frame control | [manufacturer code] | seqno | command id
    quint8 seq;
    quint8 commandId;

    if (req.asdu().size() >= 5 && req.asdu().at(0) & deCONZ::ZclFCManufacturerSpecific)
    {
        seq = static_cast<quint8>(req.asdu().at(3));
        commandId = static_cast<quint8>(req.asdu().at(4));
    }
    else
    {
        seq = static_cast<quint8>(req.asdu().at(1));
        commandId = static_cast<quint8>(req.asdu().at(2));
    }

    if (seq == indZclFrame.sequenceNumber())
    {
        if (commandId == deCONZ::ZclDefaultResponseId)
        {
            return true;
        }

        // Request and response command ids can differ, match for sequence number _should_ be fine.
        // If we see false positives, mappings need to be created on per cluster base.
        return true;
    }

    return false;
}

ApsControllerWrapper::ApsControllerWrapper(deCONZ::ApsController *ctrl) :
    m_apsCtrl(ctrl)
{

}

int ApsControllerWrapper::apsdeDataRequest(const deCONZ::ApsDataRequest &req)
{
    if (!m_apsCtrl)
    {
        return deCONZ::ErrorNotConnected;
    }
    if (m_zclDefaultResponder)
    {
        m_zclDefaultResponder->checkApsdeDataRequest(req);
    }
    return m_apsCtrl->apsdeDataRequest(req);
}

ZclDefaultResponder::ZclDefaultResponder(ApsControllerWrapper *apsCtrlWrapper, const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame) :
    m_apsCtrlWrapper(apsCtrlWrapper),
    m_ind(ind),
    m_zclFrame(zclFrame)
{
    // ZCL only and ignore OTA commands as these are handled by the OTA plugin
    if (m_ind.profileId() != ZDP_PROFILE_ID && m_ind.clusterId() != 0x0019)
    {
        m_apsCtrlWrapper->registerZclDefaultResponder(this);
        m_state = State::Watch;
    }
}

/*! When the APS indication function ends this destructor sends the ZCL Default Response if needed (RAII).
 */
ZclDefaultResponder::~ZclDefaultResponder()
{
    if (m_state == State::Init) // ZDP indications
    {
        return;
    }

    m_apsCtrlWrapper->clearZclDefaultResponder();

    if (m_state == State::Watch)
    {
        if (ZCL_NeedDefaultResponse(m_ind, m_zclFrame))
        {
            ZCL_SendDefaultResponse(m_apsCtrlWrapper->apsController(), m_ind, m_zclFrame, deCONZ::ZclSuccessStatus);
        }
    }
}

/*! During life time checks if \p req is a response to the contained request.
 */
void ZclDefaultResponder::checkApsdeDataRequest(const deCONZ::ApsDataRequest &req)
{
    if (m_state != State::Watch) { return; }

    if (!isSameAddress(m_ind.srcAddress(), req.dstAddress())) {  return; }
    if (req.profileId() != m_ind.profileId()) {  return; }
    if (req.clusterId() != m_ind.clusterId()) {  return; }

    if (ZCL_NeedDefaultResponse(m_ind, m_zclFrame)) // check here since in constructor m_zclFrame isn't parsed yet
    {
        if (ZCL_IsResponse(m_zclFrame, req))
        {
            m_state = State::HasResponse;
        }
    }
    else
    {
        m_state = State::NoResponseNeeded;
    }
}
