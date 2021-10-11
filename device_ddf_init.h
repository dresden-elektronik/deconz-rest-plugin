/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef DEVICE_DDF_INIT_H
#define DEVICE_DDF_INIT_H

class Device;
class DeviceDescription;

bool DEV_InitDeviceBasic(Device *device);
bool DEV_InitDeviceFromDescription(Device *device, const DeviceDescription &ddf);
bool DEV_InitBaseDescriptionForDevice(Device *device, DeviceDescription &ddf);

#endif // DEVICE_DDF_INIT_H
