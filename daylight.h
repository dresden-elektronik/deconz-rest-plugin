/*
 * Copyright (c)2018 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef DAYLIGHT_H
#define DAYLIGHT_H

#define DL_NADIR          100
#define DL_NIGHT_END      110
#define DL_NAUTICAL_DAWN  120
#define DL_DAWN           130
#define DL_SUNRISE_START  140
#define DL_SUNRISE_END    150
#define DL_GOLDENHOUR1    160
#define DL_SOLAR_NOON     170
#define DL_GOLDENHOUR2    180
#define DL_SUNSET_START   190
#define DL_SUNSET_END     200
#define DL_DUSK           210
#define DL_NAUTICAL_DUSK  220
#define DL_NIGHT_START    230

struct DL_Result {
    const char *name;
    int weight;
    quint64 msecsSinceEpoch;
};

void getDaylightTimes(quint64 msecSinceEpoch, double lat, double lng, std::vector<DL_Result> &result);

#endif // DAYLIGHT_H
