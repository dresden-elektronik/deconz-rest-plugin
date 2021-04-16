#include <QDateTime>
#include "catch2/catch.hpp"
#include "resource.h"

TEST_CASE("101: ResourceItem DataTypeTime", "[ResourceItem]")
{
    ResourceItemDescriptor rid;

    initResourceDescriptors();
    getResourceItemDescriptor("state/lastupdated", rid);

    REQUIRE(rid.suffix == RStateLastUpdated);


    SECTION("default initialisation")
    {
        ResourceItem item(rid);
        REQUIRE(item.descriptor().suffix == RStateLastUpdated);
        REQUIRE(item.descriptor().type == DataTypeTime);

        REQUIRE(item.lastSet().isValid() == false);
        REQUIRE(item.lastChanged().isValid() ==  false);
        REQUIRE(item.toBool() == false);
        REQUIRE(item.toNumber() == 0);
        REQUIRE(item.toVariant().toBool() == false);

        REQUIRE(item.toString() == "");
        REQUIRE(item.toVariant().type() == QVariant::Invalid);
    }

    const auto trefSeconds = QString("2021-04-16T18:20:20");
    const auto trefMSeconds = QString("2021-04-16T18:20:20.000");
    const qint64 trefMSecondsSinceEpoch = 1618597220000;

    SECTION("init from UTC QString with seconds and milliseconds")
    {
        ResourceItem item(rid);

        auto i = GENERATE(as<QString>{}, "2021-04-16T18:20:20", "2021-04-16T18:20:20.000");

        CAPTURE(i);

        REQUIRE(item.setValue(i) == true);

        REQUIRE(item.lastSet().isValid() == true);
        REQUIRE(item.lastChanged().isValid() ==  true);
        REQUIRE(item.toBool() == true);
        REQUIRE(item.toNumber() == trefMSecondsSinceEpoch);
        REQUIRE(item.toVariant().toBool() == true);
        REQUIRE(item.toVariant().type() == QVariant::String);

        // note string output _always_ contains milliseconds
        REQUIRE(item.toString() == trefMSeconds);
        REQUIRE(item.toVariant().toString() == trefMSeconds);
    }

    SECTION("init from UTC qint64 milliseconds since Epoch")
    {
        ResourceItem item(rid);

        REQUIRE(item.setValue(trefMSecondsSinceEpoch) == true);

        REQUIRE(item.lastSet().isValid() == true);
        REQUIRE(item.lastChanged().isValid() ==  true);
        REQUIRE(item.toBool() == true);
        REQUIRE(item.toNumber() == trefMSecondsSinceEpoch);
        REQUIRE(item.toVariant().toBool() == true);
        REQUIRE(item.toVariant().type() == QVariant::String);

        // note string output _always_ contains milliseconds
        REQUIRE(item.toString() == trefMSeconds);
        REQUIRE(item.toVariant().toString() == trefMSeconds);
    }

    SECTION("init from UTC QVariant with seconds and milliseconds")
    {
        ResourceItem item(rid);

        auto i = GENERATE(as<QVariant>{}, "2021-04-16T18:20:20", "2021-04-16T18:20:20.000");

        CAPTURE(i);

        REQUIRE(i.type() == QVariant::String);

        REQUIRE(item.setValue(i) == true);

        REQUIRE(item.lastSet().isValid() == true);
        REQUIRE(item.lastChanged().isValid() ==  true);
        REQUIRE(item.toBool() == true);
        REQUIRE(item.toNumber() == trefMSecondsSinceEpoch);
        REQUIRE(item.toVariant().toBool() == true);
        REQUIRE(item.toVariant().type() == QVariant::String);

        // note string output _always_ contains milliseconds
        REQUIRE(item.toString() == trefMSeconds);
        REQUIRE(item.toVariant().toString() == trefMSeconds);
    }
}

