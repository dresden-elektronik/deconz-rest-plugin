#include <array>
#include <vector>
#include <QCoreApplication>
#include <QMap>
#include <QString>

#include "catch2/catch.hpp"

#include "utils/utils.h"
#include "utils/bufstring.h"

using BString = BufString<12>;

struct AlertMap
{
    QLatin1String key;
    quint16 val;
};

struct AlertMap2
{
    BString key;
    quint16 val;
};

struct AlertQMap
{
    QString key;
    quint16 val;
};

static const std::array<AlertMap, 5> arrmap{
    {
        {QLatin1String("none"), 60},
        {QLatin1String("lselect"), 23123},
        {QLatin1String("select"), 111},
        {QLatin1String("colorloop"), 10001},
        {QLatin1String("strobo"), UINT16_MAX},
    }
};

static const std::array<AlertMap2, 5> arrmap2{
    {
        {BString("none"), 60},
        {BString("lselect"), 23123},
        {BString("select"), 111},
        {BString("colorloop"), 10001},
        {BString("strobo"), UINT16_MAX},
    }
};

static const std::vector<AlertMap> vecmap{
    {
        {QLatin1String("none"), 60},
        {QLatin1String("lselect"), 23123},
        {QLatin1String("select"), 111},
        {QLatin1String("colorloop"), 10001},
        {QLatin1String("strobo"), UINT16_MAX},
    }
};

static const QMap<QString, quint16> qmap {
    {"none", 60},
    {"lselect", 23123},
    {"select", 111},
    {"colorloop", 10001},
    {"strobo", UINT16_MAX}
};

TEST_CASE("mapped value") {

    const auto v = getMappedValue(QString("strobo"), arrmap);

    REQUIRE(v.key == "strobo");

    REQUIRE(qmap.contains("strobo"));


    BENCHMARK("std::vector<AlertMap> QLatin1String key") {
        const auto v = getMappedValue(QLatin1String("strobo"), vecmap);
        return v.val;
    };

    BENCHMARK("std::array<AlertMap> QLatin1String key") {
        const auto v = getMappedValue(QLatin1String("strobo"), arrmap);
        return v.val;
    };

    BENCHMARK("std::array<AlertMap2> BString key") {
        const auto v = getMappedValue(BString("strobo"), arrmap2);
        return v.val;
    };

    BENCHMARK_ADVANCED("advanced0")(Catch::Benchmark::Chronometer meter) {
        const BString key("strobo");
        meter.measure([&key] { return getMappedValue(key, arrmap2).val; });
    };

    BENCHMARK("std::array<AlertMap> QString key") {
        const auto v = getMappedValue(QString("strobo"), arrmap);
        return v.val;
    };

    BENCHMARK_ADVANCED("advanced1")(Catch::Benchmark::Chronometer meter) {
        const QString key("strobo");
        meter.measure([&key] { return getMappedValue(key, arrmap); });
    };

    BENCHMARK("Stack QMap QString key") {
        const QMap<QString, quint16> qmap2 {
            {"none", 60},
            {"lselect", 23123},
            {"select", 111},
            {"colorloop", 10001},
            {"strobo", UINT16_MAX}
        };
        const auto v = qmap2["strobo"];
        return v;
    };

    BENCHMARK("QMap QString key") {
        const auto v = qmap["strobo"];
        return v;
    };

    BENCHMARK_ADVANCED("advanced2")(Catch::Benchmark::Chronometer meter) {
        const QString key("strobo");
        meter.measure([&key] { return qmap[key]; });
    };

}
