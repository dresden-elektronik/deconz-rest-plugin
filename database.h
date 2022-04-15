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

#include <string>
#include <cinttypes>
#include <vector>
#include <cstddef>
#include <QString>
#include <QVariant>
#include <utils/bufstring.h>

#define DB_MAX_UNIQUEID_SIZE 31

struct DB_Secret
{
    std::string uniqueId;
    std::string secret;
    int state = 0;
};

struct DB_AlarmSystem
{
    int id;
    uint64_t timestamp;
};

struct DB_AlarmSystemResourceItem
{
    int alarmSystemId;
    const char *suffix;
    std::string value;
    uint64_t timestamp;
};

struct DB_AlarmSystemDevice
{
    char uniqueid[DB_MAX_UNIQUEID_SIZE + 1];
    uint64_t timestamp;
    uint32_t flags;
    int alarmSystemId;
};

bool DB_StoreSecret(const DB_Secret &secret);
bool DB_LoadSecret(DB_Secret &secret);

bool DB_StoreAlarmSystem(const DB_AlarmSystem &alarmSys);
bool DB_StoreAlarmSystemResourceItem(const DB_AlarmSystemResourceItem &item);
std::vector<DB_AlarmSystemResourceItem> DB_LoadAlarmSystemResourceItems(int alarmSystemId);

bool DB_StoreAlarmSystemDevice(const DB_AlarmSystemDevice &dev);
std::vector<DB_AlarmSystemDevice> DB_LoadAlarmSystemDevices();
bool DB_DeleteAlarmSystemDevice(const std::string &uniqueId);


// DDF specific 
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
std::vector<std::string> DB_LoadLegacySensorUniqueIds(QLatin1String deviceUniqueId, const char *type);
bool DB_LoadLegacyLightValue(DB_LegacyItem *litem);


#endif // DATABASE_H
