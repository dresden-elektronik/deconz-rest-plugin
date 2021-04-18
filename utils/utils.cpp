/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <deconz/dbg_trace.h>
#include "utils.h"

/*! Generates a new uniqueid in various formats based on the input parameters.

    extAddress           endpoint  cluster    result

    0x1a22334455667788   0x00      0x0000     1a:22:33:44:55:66:77:88
    0x1a22334455667788   0x01      0x0000     1a:22:33:44:55:66:77:88-01
    0x1a22334455667788   0x01      0x0500     1a:22:33:44:55:66:77:88-01-0500
    0x1a22334455667788   0xf2      0x0000     1a:22:33:44:55:66:77:88-f2

    special ZGP endpoint (0xf2) case with cluster ignored

    0x1a22334455667788   0xf2      0x0500     1a:22:33:44:55:66:77:88-f2

    \param extAddress - MAC address part
    \param endpoint - Endpoint part, ignored if 0
    \param clusterId - ClusterId part, ignored if 0 or endpoint is 0xf2
 */
QString generateUniqueId(quint64 extAddress, quint8 endpoint, quint16 clusterId)
{
    union _a
    {
        quint8 bytes[8];
        quint64 mac;
    } a;
    a.mac = extAddress;
    int ret = -1;
    char buf[64];

    if (clusterId != 0 && endpoint != 0xf2)
    {
        ret = snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x-%02x-%04x",
                    a.bytes[7], a.bytes[6], a.bytes[5], a.bytes[4],
                    a.bytes[3], a.bytes[2], a.bytes[1], a.bytes[0],
                    endpoint, clusterId);

    }
    else if (endpoint != 0)
    {
        ret = snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x-%02x",
                    a.bytes[7], a.bytes[6], a.bytes[5], a.bytes[4],
                    a.bytes[3], a.bytes[2], a.bytes[1], a.bytes[0],
                    endpoint);
    }
    else
    {
        ret = snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
                    a.bytes[7], a.bytes[6], a.bytes[5], a.bytes[4],
                    a.bytes[3], a.bytes[2], a.bytes[1], a.bytes[0]);
    }
    Q_ASSERT(ret > 0);
    Q_ASSERT(static_cast<size_t>(ret) < sizeof(buf));

    if (ret < 0 || static_cast<size_t>(ret) >= sizeof(buf))
    {
        DBG_Printf(DBG_ERROR, "failed to generate uuid, buffer too small\n");
        Q_ASSERT(0);
        return QString();
    }

    return QString::fromLatin1(buf);
}

