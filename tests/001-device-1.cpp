#include <cstdlib>
#include <QCoreApplication>
#include "catch2/catch.hpp"
#include "deconz/aps.h"

int test1()
{
    deCONZ::ApsDataConfirm conf;
    return conf.id();
}

TEST_CASE( "2: Nothing here yet", "[multi-file:2]" ) {
    REQUIRE( test1() == 0 );
}
