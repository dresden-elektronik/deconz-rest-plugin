/*
 * Copyright (c)2018 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QtMath>
#include <QVariantMap>
#include "daylight.h"

// Qt/C++ port of:
// https://github.com/mourner/suncalc/blob/master/suncalc.js

static const qreal dayMs = 1000 * 60 * 60 * 24;
static const qreal J1970 = 2440588;
static const qreal J2000 = 2451545;
static const double rad = M_PI / 180;
static const double e = rad * 23.4397; // obliquity of the Earth

static double toJulian(double msecSinceEpoch) { return msecSinceEpoch / dayMs - 0.5 + J1970; }
static double fromJulian(double j) { return (j + 0.5 - J1970) * dayMs; }
static double toDays(double msecSinceEpoch) { return toJulian(msecSinceEpoch) - J2000; }
static double declination(double l, double b)    { return asin(sin(b) * cos(e) + cos(b) * sin(e) * sin(l)); }

// general sun calculations

static double solarMeanAnomaly(double d) { return rad * (357.5291 + 0.98560028 * d); }

static double eclipticLongitude(double M) {

    double C = rad * (1.9148 * sin(M) + 0.02 * sin(2 * M) + 0.0003 * sin(3 * M)); // equation of center
    double P = rad * 102.9372; // perihelion of the Earth

    return M + C + P + M_PI;
}

// calculations for sun times

static const double J0 = 0.0009;

static double julianCycle(double d, double lw) { return round(d - J0 - lw / (2 * M_PI)); }

static double approxTransit(double Ht, double lw, double n) { return J0 + (Ht + lw) / (2 * M_PI) + n; }
static double solarTransitJ(double ds, double M, double L)  { return J2000 + ds + 0.0053 * sin(M) - 0.0069 * sin(2 * L); }

static double hourAngle(double h, double phi, double d) { return acos((sin(h) - sin(phi) * sin(d)) / (cos(phi) * cos(d))); }

// returns set time for the given sun altitude
static double getSetJ(double h, double lw, double phi, double dec, double n, double M, double L) {

    double w = hourAngle(h, phi, dec);
    double a = approxTransit(w, lw, n);
    return solarTransitJ(a, M, L);
}

struct TimePin {
    double offset;
    const char *first;
    int firstWeight;
    const char *second;
    int secondWeight;
};

// calculates sun times for a given date and latitude/longitude

void getDaylightTimes(quint64 msecSinceEpoch, double lat, double lng,  std::vector<DL_Result> &result)
{
    std::vector<TimePin> times;
    // sun times configuration (angle, morning name, evening name)
    times.push_back({-0.833, "sunriseStart",  DL_SUNRISE_START, "sunsetEnd",    DL_SUNSET_END});
    times.push_back({-0.3,   "sunriseEnd",    DL_SUNRISE_END,   "sunsetStart",  DL_SUNSET_START});
    times.push_back({-6,     "dawn",          DL_DAWN,          "dusk",         DL_DUSK});
    times.push_back({-12,    "nauticalDawn",  DL_NAUTICAL_DAWN, "nauticalDusk", DL_NAUTICAL_DUSK});
    times.push_back({-18,    "nightEnd",      DL_NIGHT_END,     "nightStart",   DL_NIGHT_START});
    times.push_back({6,      "goldenHour1",   DL_GOLDENHOUR1,   "goldenHour2",  DL_GOLDENHOUR2});

    double lw = rad * -lng,
        phi = rad * lat,

        d = toDays(msecSinceEpoch),
        n = julianCycle(d, lw),
        ds = approxTransit(0, lw, n),

        M = solarMeanAnomaly(ds),
        L = eclipticLongitude(M),
        dec = declination(L, 0),

        Jnoon = solarTransitJ(ds, M, L),

        Jset, Jrise;


    result.push_back({"solarNoon", DL_SOLAR_NOON, (quint64)fromJulian(Jnoon)});
    result.push_back({"nadir", DL_NADIR, (quint64)fromJulian(Jnoon - 0.5)});

    for (const TimePin &time : times)
    {

        Jset = getSetJ(time.offset * rad, lw, phi, dec, n, M, L);
        Jrise = Jnoon - (Jset - Jnoon);

        result.push_back({time.first, time.firstWeight, (quint64)fromJulian(Jrise)});
        result.push_back({time.second, time.secondWeight, (quint64)fromJulian(Jset)});
    }

    std::sort(result.begin(), result.end(),
              [](const DL_Result &a, const DL_Result &b)
              { return a.msecsSinceEpoch < b.msecsSinceEpoch; });
}

