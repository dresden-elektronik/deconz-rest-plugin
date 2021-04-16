#include <QCoreApplication>
#include "catch2/catch.hpp"
#include "resource.h"
#include "device_js/device_js.h"

int argc = 0;
QCoreApplication app(argc, nullptr);

TEST_CASE( "001: Basic Math", "[DeviceJs]" )
{
    DeviceJs js;

    REQUIRE(js.evaluate("1 + 2") == JsEvalResult::Ok);

    REQUIRE(js.result().toInt() == 3);

}

TEST_CASE("002: ResourceItem DataTypeBool", "[ResourceItem]")
{
    initResourceDescriptors();

    ResourceItemDescriptor rid;

    REQUIRE(rid.suffix == RInvalidSuffix);

    getResourceItemDescriptor("state/on", rid);

    REQUIRE(rid.suffix == RStateOn);

    ResourceItem item(rid);

    REQUIRE(item.descriptor().suffix == RStateOn);
    REQUIRE(item.descriptor().type == DataTypeBool);

    REQUIRE(item.lastSet().isValid() == false);
    REQUIRE(item.lastChanged().isValid() ==  false);
    REQUIRE(item.toBool() == false);
    REQUIRE(item.toNumber() == 0);
    REQUIRE(item.toVariant().toBool() == false);
    REQUIRE(item.toString() == "");  // TODO
    REQUIRE(item.toVariant().type() == QVariant::Invalid);

    item.setValue(true);

    REQUIRE(item.lastSet().isValid() == true);
    REQUIRE(item.lastChanged().isValid() ==  true);
    REQUIRE(item.toBool() == true);
    REQUIRE(item.toNumber() == 1);
    REQUIRE(item.toVariant().toBool() == true);
    REQUIRE(item.toVariant().type() == QVariant::Bool);
    REQUIRE(item.toString() == "");  // TODO
    REQUIRE(item.toVariant().toString() == "true");
}
