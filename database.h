/*
 * Copyright (c) 2016-2024 dresden elektronik ingenieurtechnik gmbh.
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

namespace deCONZ {
    class Address;
}

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


 // TODO(mpi) deprecated for DB_ResourceItem2
struct DB_ResourceItem
{
    BufString<64> name;
    QVariant value;
    qint64 timestampMs = 0; // milliseconds since Epoch
};

struct DB_ResourceItem2
{
    BufString<64> name;
    unsigned valueSize;
    char value[160];
    qint64 timestampMs = 0; // milliseconds since Epoch
};

struct DB_LegacyItem
{
    BufString<64> column;
    BufString<64> uniqueId;

    BufString<128> value;
};

struct DB_ZclValue
{
    int64_t data;
    int deviceId;
    uint16_t clusterId;
    uint16_t attrId;
    uint8_t endpoint;

    // internal
    uint8_t loaded;
};

struct DB_IdentifierPair
{
    unsigned modelIdAtomIndex;
    unsigned mfnameAtomIndex;
};

struct DB_Device
{
    int deviceId;
    uint16_t nwk;
    uint64_t mac;
    int64_t creationTime;
};

int DB_StoreDevice(DB_Device &dev);

int DB_GetSubDeviceItemCount(QLatin1String uniqueId);
bool DB_LoadZclValue(DB_ZclValue *val);
bool DB_StoreZclValue(const DB_ZclValue *val);
bool DB_StoreSubDevice(const char *uniqueId);
bool DB_StoreDeviceItem(int deviceId, const DB_ResourceItem2 &item);
bool DB_LoadDeviceItems(int deviceId, std::vector<DB_ResourceItem2> &items);
bool DB_ResourceItem2DbItem(const ResourceItem *rItem, DB_ResourceItem2 *dbItem);
std::vector<DB_IdentifierPair> DB_LoadIdentifierPairs();
bool DB_StoreSubDeviceItem(const Resource *sub, ResourceItem *item);
bool DB_StoreSubDeviceItems(Resource *sub);
std::vector<DB_ResourceItem> DB_LoadSubDeviceItemsOfDevice(QLatin1String deviceUniqueId);
std::vector<DB_ResourceItem> DB_LoadSubDeviceItems(QLatin1String uniqueId);
bool DB_LoadLegacySensorValue(DB_LegacyItem *litem);
std::vector<std::string> DB_LoadLegacySensorUniqueIds(QLatin1String deviceUniqueId, const char *type);
bool DB_LoadLegacyLightValue(DB_LegacyItem *litem);


#endif // DATABASE_H
