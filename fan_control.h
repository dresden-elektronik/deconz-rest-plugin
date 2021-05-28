#ifndef FAN_CONTROL_H
#define FAN_CONTROL_H

#include <QString>
#include <array>
#include "utils/utils.h"

static const std::array<KeyValMap, 7> RConfigFanModeValues = { { {QLatin1String("off"), 0}, {QLatin1String("low"), 1}, {QLatin1String("medium"), 2}, {QLatin1String("high"), 3},
                                                                 {QLatin1String("on"), 4}, {QLatin1String("auto"), 5}, {QLatin1String("smart"), 6}} };

#endif // FAN_CONTROL_H
