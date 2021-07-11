/*
 * Copyright (c) 2016-2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef DATABASE_H
#define DATABASE_H

#include <cstddef>
#include <QString>
#include <QVariant>
#include <vector>
#include <utils/bufstring.h>

class Resource;
class ResourceItem;


struct DB_ResourceItem
{
    BufString<64> name;
    QVariant value;
    qint64 timestampMs = 0; // milliseconds since Epoch
};

struct DB_LegacyItem
{
    BufString<32> column;
    BufString<64> uniqueId;

    BufString<128> value;
};

int DB_GetSubDeviceItemCount(QLatin1String uniqueId);
bool DB_StoreSubDevice(const QString &parentUniqueId, const QString &uniqueId);
bool DB_StoreSubDeviceItem(const Resource *sub, const ResourceItem *item);
bool DB_StoreSubDeviceItems(const Resource *sub);
std::vector<DB_ResourceItem> DB_LoadSubDeviceItemsOfDevice(QLatin1String deviceUniqueId);
std::vector<DB_ResourceItem> DB_LoadSubDeviceItems(QLatin1String uniqueId);
bool DB_LoadLegacySensorValue(DB_LegacyItem *litem);
bool DB_LoadLegacyLightValue(DB_LegacyItem *litem);


#endif // DATABASE_H
