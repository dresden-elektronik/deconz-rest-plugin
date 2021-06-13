/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef APS_CONTROLLER_WRAPPER_H
#define APS_CONTROLLER_WRAPPER_H

namespace  deCONZ {
    class ApsController;
    class ApsDataIndication;
    class ApsDataRequest;
    class ZclFrame;
}

class ApsControllerWrapper;

/*! RAII helper to send a ZCL Default Response after APS indication if needed.

    The class observes outgoing APS requests for specific responses for a request and
    automatically sends a ZCL Default Response if no specifc response was send.
 */
class ZclDefaultResponder
{
public:
    ZclDefaultResponder() = delete;
    explicit ZclDefaultResponder(ApsControllerWrapper *apsCtrlWrapper, const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame);
    ~ZclDefaultResponder();
    void checkApsdeDataRequest(const deCONZ::ApsDataRequest &req);

private:
    enum class State
    {
        Init,
        NoResponseNeeded,
        Watch,
        HasResponse
    };
    ApsControllerWrapper *m_apsCtrlWrapper = nullptr;
    const deCONZ::ApsDataIndication &m_ind;
    const deCONZ::ZclFrame &m_zclFrame;
    State m_state = State::Init;
};

/*! Wraps \c deCONZ::ApsController to intercept apsdeDataRequest().

    The main purpose is to deterministic send ZCL Default Response if needed.
 */
class ApsControllerWrapper
{
public:
    ApsControllerWrapper(deCONZ::ApsController *ctrl);
    int apsdeDataRequest(const deCONZ::ApsDataRequest &req);
    void registerZclDefaultResponder(ZclDefaultResponder *resp) { m_zclDefaultResponder = resp; }
    void clearZclDefaultResponder() { m_zclDefaultResponder = nullptr; };
    deCONZ::ApsController *apsController() { return m_apsCtrl; }

private:
    deCONZ::ApsController *m_apsCtrl = nullptr;
    ZclDefaultResponder *m_zclDefaultResponder = nullptr;
};

#endif // APS_CONTROLLER_WRAPPER_H
