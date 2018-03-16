/*
 * Copyright (c) 2016 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef BINDINGS_H
#define BINDINGS_H

/*! \class Binding

    Represents a ZigBee ZDO Binding.
 */
class Binding
{
public:
    enum Constans
    {
        GroupAddressMode = 0x01,
        ExtendedAddressMode = 0x03
    };

    Binding();
    bool operator==(const Binding &rhs) const;
    bool operator!=(const Binding &rhs) const;
    /*! The source IEEE address. */
    quint64 srcAddress;
    /*! The source endpoint. */
    quint8 srcEndpoint;
    /*! The cluster on the source device that is bound to the destination device. */
    quint16 clusterId;
    /*! The addressing mode for the destination address.
        0x01 = 16-bit group address (no endpoint)
        0x03 = 64-bit extended address + endpoint
     */
    quint8 dstAddrMode;
    union
    {
        quint16 group; //!< The destination group address (if dstAddrMode = 0x01).
        quint64 ext; //!< The destination extended address (if dstAddrMode = 0x03).
    } dstAddress;
    /*! Destination endpoint (if dstAddrMode = 0x03). */
    quint8 dstEndpoint;

    bool readFromStream(QDataStream &stream);
    bool writeToStream(QDataStream &stream) const;
};

/*! \class BindingTableReader

    Helper class to query full binding table of a node.
 */
class BindingTableReader
{
public:
    enum Constants
    {
        MaxConfirmTime = 10 * 60 * 1000, // 10 min
        MaxResponseTime = 10 * 1000, // 10 sec
        MaxEndDeviceResponseTime = 60 * 60 * 1000 // 60 min
    };

    BindingTableReader() :
        state(StateIdle),
        index(0),
        isEndDevice(false)
    {
    }

    enum State {
        StateIdle,
        StateWaitConfirm,
        StateWaitResponse,
        StateFinished
    };
    State state; //!< State of query
    quint8 index; //!< Current read index
    bool isEndDevice; //!< True if node is an end-device
    QTime time; //!< State timeout reference
    deCONZ::ApsDataRequest apsReq; //!< The APS request to match APS confirm.id
};

class ConfigureReportingRequest
{
public:
    ConfigureReportingRequest() :
        direction(0x00),
        reportableChange8bit(0xFF),
        reportableChange16bit(0xFFFF),
        reportableChange24bit(0xFFFFFF),
        reportableChange48bit(0xFFFFFFFF),  // there's no quint48
        manufacturerCode(0)
    {
    }

    quint8 zclSeqNum;
    quint8 direction;
    quint8 dataType;
    quint16 attributeId;
    quint16 minInterval;
    quint16 maxInterval;
    quint8 reportableChange8bit;
    quint16 reportableChange16bit;
    quint32 reportableChange24bit;          // there's no quint24
    quint32 reportableChange48bit;          // there's no quint48
    quint16 manufacturerCode;
};

#endif // BINDINGS_H
