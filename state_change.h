/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef STATE_CHANGE_H
#define STATE_CHANGE_H

#include <QVariant>
#include <QElapsedTimer>
#include "device_access_fn.h"

class Resource;
class ResourceItem;
class StateChange;

namespace deCONZ {
    class ApsController;
}

int SC_WriteZclAttribute(const Resource *r, const StateChange *stateChange, deCONZ::ApsController *apsCtrl);
int SC_SetOnOff(const Resource *r, const StateChange *stateChange, deCONZ::ApsController *apsCtrl);

/*! \fn StateChangeFunction_t

    A state change function sends a certain ZCL command to a device to set a target state.
    For example: Sending On/off command to the on/off cluster.

    \returns 0 if the command was sent, or a negative number on failure.
 */
typedef int (*StateChangeFunction_t)(const Resource *r, const StateChange *stateChange, deCONZ::ApsController *apsCtrl);

/*! \class StateChange

    A generic helper to robustly set and verify state changes using ResourceItems.

    The main purpose of this helper is to ensure that a state will be set: For example
    a group cast to turn on 20 lights might not turn on all lights, in this case the
    StateChange detects that a light has not been turned on and can retry the respective command.

    A StateChange may have an arbitrary long "change-timeout" to support changing configurations
    for sleeping or not yet powered devices.

    StateChange is bound to a Resource and can be added by Resource::addStateChange(). Multiple
    StateChange items may be added if needed, for example to set on, brightness and color or to verify that a
    scene is called correctly, even if the scene cluster doesn't have the correct values stored in
    the device NVRAM.
 */
class StateChange
{
public:
    enum State
    {
        StateCallFunction, //! Calls the change function.
        StateWaitSync,     //! Waits until state is verified or a state-timeout occurs.
        StateRead,         //! When StateWaitSync timedout without receiving a value from the device.
        StateFinished,     //! The target state has been verified.
        StateFailed        //! The state changed failed after change-timeout.
    };

    enum SyncResult
    {
        VerifyUnknown,
        VerifySynced,
        VerifyNotSynced
    };

    /*! \struct StateChange::Item

        Specifies the target value of a specific item.
        There can be multiple Items involved in one state change.
     */
    struct Item
    {
        Item(const char *s, const QVariant &v) : suffix(s), targetValue(v) { }
        const char *suffix = nullptr; //! RStateOn, RStateBri, ...
        QVariant targetValue; //! The target value.
        SyncResult verified = VerifyUnknown;
    };

    /*! \struct StateChange::Param

        Specifies an extra parameter which might be needed to carry out a command.
        A Param usually isn't available as a ResourceItem, e.g. the transitiontime for a brightness change.
     */
    struct Param
    {
        QString name;
        QVariant value;
    };

    /*! Constructs a new StateChange.
        \param initialState - StateCallFunction or StateWaitSync.
        \param fn - the state change function.
        \param dstEndpoint - the endpoint to which the command should be send.

        StateCallFunction will call the state function in the next tick().
        StateWaitSync should be used when a command has already been sent, the state function will only
        be called when the state change can't be verified after state-timeout.
     */
    explicit StateChange(State initialState, StateChangeFunction_t fn, quint8 dstEndpoint);
    State state() const { return m_state; }
    int tick(uint64_t extAddr, Resource *r, deCONZ::ApsController *apsCtrl);
    void verifyItemChange(const ResourceItem *item);
    void addTargetValue(const char *suffix, const QVariant &value);
    void addParameter(const QString &name, const QVariant &value);
    bool operator==(const StateChange &other) const;
    const std::vector<Item> &items() const { return m_items; }
    const std::vector<Param> &parameters() const { return m_parameters; }
    quint8 dstEndpoint() const { return m_dstEndpoint; }
    void setChangeTimeoutMs(int timeout) { m_changeTimeoutMs = timeout; }
    void setStateTimeoutMs(int timeout) { m_stateTimeoutMs = timeout; }

private:
    State m_state = StateCallFunction;
    StateChangeFunction_t m_changeFunction = nullptr; //! The function to send a respective ZCL command.

    DA_ReadResult m_readResult;
    quint8 m_dstEndpoint;
    int m_stateTimeoutMs = 1000 * 5; //! Inner timeout for states.
    int m_changeTimeoutMs = 1000 * 180; //! Max. duration for the whole change.
    QElapsedTimer m_stateTimer;
    QElapsedTimer m_changeTimer; //! Started once in the constructor.
    std::vector<Item> m_items;
    std::vector<Param> m_parameters;
};

#endif // STATE_CHANGE_H
