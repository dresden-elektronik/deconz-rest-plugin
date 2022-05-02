/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef DEVICE_COMPAT_H
#define DEVICE_COMPAT_H

#include "device_descriptions.h"

class Device;

Resource *DEV_InitCompatNodeFromDescription(Device *device, const DeviceDescription &ddf, const DeviceDescription::SubDevice &sub, const QString &uniqueId);

#endif // DEVICE_COMPAT_H
