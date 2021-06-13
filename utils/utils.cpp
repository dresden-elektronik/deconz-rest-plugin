/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <deconz/aps.h>
#include <deconz/dbg_trace.h>
#include "utils.h"
#include "resource.h"

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

/*! Returnes an index >= 0 if \p needle is in \p haystack or -1 if not found.

    The strings aren't required to be '\0' terminated.
 */
int indexOf(QLatin1String haystack, QLatin1String needle)
{
    if (needle.size() == 0 || haystack.size() == 0)
    {
        return -1;
    }

    for (int i = 0; i < haystack.size(); i++)
    {
        if (needle.size() > haystack.size() - i)
        {
            return -1;
        }

        int match = 0;
        for (int j = i, n = 0; j < haystack.size() && n < needle.size(); j++, n++)
        {
            if (haystack.data()[j] != needle.data()[n])
            {
                break;
            }
            match++;
        }

        if (match == needle.size())
        {
            return i;
        }
    }

    return -1;
}

/*! Returnes true if \p needle is in \p haystack.

    The strings aren't required to be '\0' terminated.
 */
bool contains(QLatin1String haystack, QLatin1String needle)
{
    return indexOf(haystack, needle) >= 0;
}

// Tests for contains(QLatin1String, QLatin1String)
// const char *haystack = "abc";

// Q_ASSERT(contains(QLatin1String("Content-Type: form-data; foobar"), QLatin1String("form-data")) == true);
// Q_ASSERT(contains(QLatin1String("form-data; barbaz"), QLatin1String("nop-data")) == false);
// Q_ASSERT(contains(QLatin1String(haystack, 3), QLatin1String("abc")) == true);
// Q_ASSERT(contains(QLatin1String(haystack, 3), QLatin1String("bc")) == true);
// Q_ASSERT(contains(QLatin1String(haystack, 3), QLatin1String("c")) == true);
// Q_ASSERT(contains(QLatin1String(haystack, 2), QLatin1String("abc")) == false);
// Q_ASSERT(contains(QLatin1String(haystack, 3), QLatin1String("")) == false);
// Q_ASSERT(contains(QLatin1String(), QLatin1String("")) == false);

/*! Returnes true if \p str starts with \p needle.

    The strings aren't required to be '\0' terminated.
 */
bool startsWith(QLatin1String str, QLatin1String needle)
{
    return indexOf(str, needle) == 0;
}

RestData verifyRestData(const ResourceItemDescriptor &rid, const QVariant &val)
{
    bool ok;
    RestData data;

    if (rid.qVariantType == val.type())
    {
        if (rid.type == DataTypeUInt8 || rid.type == DataTypeUInt16 || rid.type == DataTypeUInt32 || rid.type == DataTypeUInt64)
        {
            auto uintValue = val.toUInt(&ok);
            if (ok)
            {
                data.uinteger = uintValue;
                data.valid = true;
                return data;
            }
            else
            {
                return data;
            }
        }
        else if (rid.type == DataTypeString || rid.type == DataTypeTime || rid.type == DataTypeTimePattern)
        {
            if (!val.toString().isEmpty())
            {
                data.string = val.toString();
                data.valid = true;
                return data;
            }
            else
            {
                return data;
            }
        }
        else if (rid.type == DataTypeBool)
        {
            data.boolean = val.toBool();
            data.valid = true;
            return data;
        }
        else if (rid.type == DataTypeInt8 || rid.type == DataTypeInt16 || rid.type == DataTypeInt32 || rid.type == DataTypeInt64)
        {
            auto intValue = val.toInt(&ok);
            if (ok)
            {
                data.integer = intValue;
                data.valid = true;
                return data;
            }
            else
            {
                return data;
            }
        }
        else if (rid.type == DataTypeReal)
        {
            data.real = val.toReal();
            data.valid = true;
            return data;
        }
        else
        {
            return data;
        }
    }
    else
    {
        return data;
    }
}

/*! Compare addresses where either NWK or MAC address might not be known.
    \returns true if both adresses have same MAC address (strong guaranty).
    \returns true if at least one of the addresses doesn't have MAC but the NWK addresses are equal.
    \returns false otherwise.
*/
bool isSameAddress(const deCONZ::Address &a, const deCONZ::Address &b)
{
    if (a.hasExt() && b.hasExt())
    {
        // nested if statement, so the NWK check won't be made if both MAC addresses are known
        if (a.ext() != b.ext())
        {
             return false;
        }
    }
    else  if (a.hasNwk() && b.hasNwk())
    {
       if (a.nwk() != b.nwk())
       {
            return false;
       }
    }
    else { return false; }

    return true;
}
