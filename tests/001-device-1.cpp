#include <cstdlib>
#include <QCoreApplication>
#include "catch2/catch.hpp"
#include "deconz/aps.h"

namespace deCONZ {
  class Node;
}

deCONZ::Node *DEV_GetCoreNode(unsigned long)
{
    return nullptr;
}

quint8 zclNextSequenceNumber()
{
    static quint8 seq;
    return ++seq;
}


int test1()
{
    deCONZ::ApsDataConfirm conf;
    return conf.id();
}

TEST_CASE( "2: Nothing here yet", "[multi-file:2]" ) {
    REQUIRE( test1() == 0 );
}
