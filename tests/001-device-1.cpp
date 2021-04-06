#include <cstdlib>
#include <QCoreApplication>
#include "catch2/catch.hpp"
#include "deconz/aps.h"
#include "deconz/dbg_trace.h"

namespace deCONZ {
  class Node;
}

class Resource;

deCONZ::Node *DEV_GetCoreNode(unsigned long)
{
    return nullptr;
}

quint8 zclNextSequenceNumber()
{
    static quint8 seq;
    return ++seq;
}

Resource *DEV_GetResource(char const*, QString const&)
{
    return nullptr;
}

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


int test1()
{
    deCONZ::ApsDataConfirm conf;
    return conf.id();
}

TEST_CASE( "2: Nothing here yet", "[multi-file:2]" ) {
    REQUIRE(generateUniqueId(1,2,3) == "00:00:00:00:00:00:00:01-02-000r");
}
