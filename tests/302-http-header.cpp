#include <QStringList>
#include <deconz/qhttprequest_compat.h>
#include <deconz/qhttprequest_compat_old.h>
#include "catch2/catch.hpp"


TEST_CASE("http header") {

    const char *raw = "GET http://127.0.0.1:8080/api/38D7042DC3/devices\r\n"
                      "Content-Length: 0\r\n"
                      "Accept: vnd.ddel.v1\r\n"
                      "\r\n";
    const size_t rawLength = strlen(raw);

    static_assert (sizeof(QHttpRequestHeaderOld) == 8, "");


    BENCHMARK("t1 path components new") {
        const QHttpRequestHeader hdr(raw, rawLength);

        if (hdr.pathComponentsCount() == 3 && hdr.httpMethod() == HttpGet && hdr.pathAt(1) == QLatin1String("api") && hdr.pathAt(2) == QLatin1String("devices"))
        {
            return size_t(1);
        }

        return hdr.contentLength();
    };

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

    BENCHMARK("t1 path components old ") {
        QHttpRequestHeaderOld hdr1(QLatin1String(raw, rawLength));
        const QStringList ls = hdr1.path().split('/', SKIP_EMPTY_PARTS);

        if (ls.length() == 3 && hdr1.method() == QLatin1String("GET") && ls[0] == QLatin1String("api") && ls[2] == QLatin1String("devices"))
        {
            return 1;
        }

        return 0;
    };

    BENCHMARK_ADVANCED("t1 2 path components old")(Catch::Benchmark::Chronometer meter) {
        QHttpRequestHeaderOld hdr1(QLatin1String(raw, rawLength));

        meter.measure([&hdr1] {
            const QStringList ls = hdr1.path().split('/', SKIP_EMPTY_PARTS);

            if (ls.length() == 3 && hdr1.method() == QLatin1String("GET") && ls[0] == QLatin1String("api") && ls[2] == QLatin1String("devices"))
            {
                return 1;
            }

            return 0;
        });
    };

    BENCHMARK_ADVANCED("t2 value() new")(Catch::Benchmark::Chronometer meter) {
        const QHttpRequestHeader hdr(raw, rawLength);
        meter.measure([&hdr] { return hdr.value(QLatin1String("Accept")); });
    };

    BENCHMARK_ADVANCED("t2 value() old")(Catch::Benchmark::Chronometer meter) {
        const QHttpRequestHeaderOld hdr1(QLatin1String(raw, rawLength));
        meter.measure([&hdr1] { return hdr1.value(QLatin1String("Accept")); });
    };

}
