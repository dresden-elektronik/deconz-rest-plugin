#include <QDateTime>
#include "catch2/catch.hpp"
#include "resource.h"
#include "utils/bufstring.h"

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

TEST_CASE("102: ResourceItem DataTypeString", "[ResourceItem]")
{
    constexpr size_t bufSize = 8;
    using Str = BufString<bufSize>;
    std::unique_ptr<Str> s1 = std::make_unique<Str>();

    const auto overHead = bufSize - s1->capacity();
    REQUIRE(overHead == 2);

    REQUIRE(s1->empty() == true);
    REQUIRE(s1->size() == 0);
    REQUIRE(s1->capacity() == bufSize - overHead);

    s1->setString("foo");
    REQUIRE(s1->empty() == false);
    REQUIRE(s1->size() == 3);
    REQUIRE(s1->capacity() == bufSize - overHead - 3);

    s1->clear();
    REQUIRE(s1->empty() == true);
    REQUIRE(s1->size() == 0);
    REQUIRE(s1->capacity() == bufSize - overHead);

    s1->setString("Ä"); // UTF-8 two chars for Ä
    REQUIRE(s1->empty() == false);
    REQUIRE(s1->size() == 2);
    REQUIRE(s1->capacity() == bufSize - overHead - 2);

    REQUIRE(*s1 == "Ä");
    REQUIRE(*s1 != "A");
    REQUIRE(*s1 != "foo");

    s1->setString("bar");
    REQUIRE(s1->empty() == false);
    REQUIRE(s1->size() == 3);
    REQUIRE(s1->capacity() == bufSize - overHead - 3);

    std::string bar("bar");

    REQUIRE(*s1 == bar);
    REQUIRE(*s1 == "bar");
    REQUIRE(s1->c_str()[3] == '\0');
    REQUIRE(strncmp(s1->c_str(), "bar", 3) == 0);
    REQUIRE(*s1 == QString("bar"));
    REQUIRE(*s1 == QLatin1String("bar"));

    QString s2 = *s1;

    REQUIRE(s2 == QString("bar"));
    REQUIRE(*s1 == s2);
    REQUIRE(s1->startsWith(QString("ba")) == true);
    REQUIRE(s1->startsWith(QString("bar")) == true);
    REQUIRE(s1->startsWith(QString("barr")) == false);
    REQUIRE(s1->startsWith(QLatin1String("ba")) == true);
    REQUIRE(s1->startsWith(QLatin1String("bar")) == true);
    REQUIRE(s1->startsWith(QLatin1String("barr")) == false);

    s1->setString("Äöü");
    REQUIRE(*s1 == QString("Äöü"));
}

