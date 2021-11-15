#include <QCoreApplication>

// string conversion so catch can print QString
std::ostream& operator << ( std::ostream& os, const QString &str)
{
    os << str.toStdString();
    return os;
}

#include "catch2/catch.hpp"
#include "device_js/device_js.h"

int argc = 0;
QCoreApplication app(argc, nullptr);


TEST_CASE( "001: Basic Math", "[DeviceJs]" )
{
    DeviceJs js;

    REQUIRE(js.evaluate("1 + 2") == JsEvalResult::Ok);

    REQUIRE(js.result().toInt() == 3);
}
