#include <sys/time.h>
#include <QDateTime>
#include "catch2/catch.hpp"
#include <deconz/timeref.h>


TEST_CASE("timeref") {


    BENCHMARK("QDateTime op <") {
        const auto a = QDateTime::currentDateTime();
        const auto b = QDateTime::currentDateTime();
        return a < b;
    };

    BENCHMARK("steadyTimeRef op <") {
        const auto a = deCONZ::steadyTimeRef();
        const auto b = deCONZ::steadyTimeRef();
        return a < b;
    };

    BENCHMARK("systemTimeRef op <") {
        const auto a = deCONZ::systemTimeRef();
        const auto b = deCONZ::systemTimeRef();
        return a < b;
    };

    BENCHMARK("QDateTime msec since epoch") {
        const auto msec = QDateTime::currentMSecsSinceEpoch();
        return msec;
    };


    BENCHMARK("systemTimeRef msec since epoch") {
        return deCONZ::systemTimeRef().ref;
    };

    BENCHMARK("gettimeofday msec since epoch") {
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        return int64_t(tv.tv_sec) * Q_INT64_C(1000) + tv.tv_usec / 1000;

    };

    BENCHMARK("steadyTimeRef msec") {
        const auto tref = deCONZ::steadyTimeRef();
        return tref.ref;
    };



    /*
    BENCHMARK_ADVANCED("t1 1 path components new")(Catch::Benchmark::Chronometer meter) {
        const QHttpRequestHeader hdr(raw, rawLength);

        meter.measure([&hdr] {
            if (hdr.pathComponentsCount() == 3 && hdr.httpMethod() == HttpGet && hdr.pathAt(1) == QLatin1String("api") && hdr.pathAt(2) == QLatin1String("devices"))
            {
                return size_t(1);
            }

            return hdr.contentLength();
        });
    };

    */

}
