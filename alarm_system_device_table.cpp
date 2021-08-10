/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "deconz/dbg_trace.h"
#include "deconz/timeref.h"
#include "alarm_system_device_table.h"
#include "database.h"
#include "utils/utils.h"

//! getIterator() is a helper to simplyfy code
static std::vector<AS_DeviceEntry>::iterator getIterator(std::vector<AS_DeviceEntry> &table, quint64 extAddress)
{
    return std::find_if(table.begin(), table.end(), [&](const AS_DeviceEntry &entry) { return entry.extAddress == extAddress; });
}

static std::vector<AS_DeviceEntry>::const_iterator getIterator(const std::vector<AS_DeviceEntry> &table, quint64 extAddress)
{
    return std::find_if(table.cbegin(), table.cend(), [&](const AS_DeviceEntry &entry) { return entry.extAddress == extAddress; });
}

AS_DeviceTable::AS_DeviceTable()
{
    static_assert(sizeof(AS_DeviceEntry) == 64, "expected size of AS_DeviceEntry == 64 bytes");
    static_assert (AS_MAX_UNIQUEID_LENGTH == DB_MAX_UNIQUEID_SIZE, "DB/AS max uniqueid size mismatch");
}

const AS_DeviceEntry &AS_DeviceTable::get(const QString &uniqueId) const
{
    const quint64 extAddress = extAddressFromUniqueId(uniqueId);
    const auto i = getIterator(m_table, extAddress);

    if (i != m_table.cend())
    {
        return *i;
    }

    return m_invalidEntry;
}

const AS_DeviceEntry &AS_DeviceTable::get(quint64 extAddress) const
{
    const auto i = getIterator(m_table, extAddress);

    if (i != m_table.cend())
    {
        return *i;
    }

    return m_invalidEntry;
}

const AS_DeviceEntry &AS_DeviceTable::at(size_t index) const
{
    if (index < m_table.size())
    {
        return m_table[index];
    }

    return m_invalidEntry;
}

static void entryInitArmMask(AS_DeviceEntry &entry)
{
    memset(entry.armMask, 0, sizeof(entry.armMask));
    char *p = entry.armMask;

    if (entry.flags & AS_ENTRY_FLAG_ARMED_AWAY) { *p++ = 'A'; }
    if (entry.flags & AS_ENTRY_FLAG_ARMED_STAY) { *p++ = 'S'; }
    if (entry.flags & AS_ENTRY_FLAG_ARMED_NIGHT) { *p++ = 'N'; }
}

static bool storeDeviceEntry(const AS_DeviceEntry &entry)
{
    DB_AlarmSystemDevice dbDevice;

    copyString(dbDevice.uniqueid, sizeof(dbDevice.uniqueid), entry.uniqueId);
    DBG_Assert(!isEmptyString(dbDevice.uniqueid));
    if (isEmptyString(dbDevice.uniqueid))
    {
        return false;
    }

    dbDevice.alarmSystemId = entry.alarmSystemId;
    dbDevice.flags = entry.flags;
    dbDevice.timestamp = deCONZ::systemTimeRef().ref;

    return DB_StoreAlarmSystemDevice(dbDevice);
}

bool AS_DeviceTable::put(const QString &uniqueId, quint32 flags, quint8 alarmSystemId)
{
    const quint64 extAddress = extAddressFromUniqueId(uniqueId);

    if (extAddress == 0)
    {
        return false;
    }

    // check existing
    auto i = getIterator(m_table, extAddress);

    if (i != m_table.end())
    {
        if (i->flags != flags || i->alarmSystemId != alarmSystemId)
        {
            i->flags = flags;
            i->alarmSystemId = alarmSystemId;
            entryInitArmMask(*i);

            storeDeviceEntry(*i);
        }
        return true;
    }

    // not existing, create new
    m_table.push_back(AS_DeviceEntry());
    AS_DeviceEntry &entry = m_table.back();

    if (size_t(uniqueId.size()) >= sizeof(entry.uniqueId))
    {
        m_table.pop_back();
        return false;
    }

    entry.uniqueIdSize = quint8(uniqueId.size());
    memcpy(entry.uniqueId, qPrintable(uniqueId), entry.uniqueIdSize);
    entry.uniqueId[entry.uniqueIdSize] = '\0';
    entry.extAddress = extAddress;
    entry.alarmSystemId = alarmSystemId;
    entry.flags = flags;
    entryInitArmMask(entry);

    storeDeviceEntry(entry);
    return true;
}

bool AS_DeviceTable::erase(const QLatin1String &uniqueId)
{
    quint64 extAddress = extAddressFromUniqueId(uniqueId);
    auto i = getIterator(m_table, extAddress);

    if (i != m_table.end() && DB_DeleteAlarmSystemDevice(i->uniqueId))
    {
        *i = m_table.back();
        m_table.pop_back();
        return true;
    }

    return false;
}

void AS_DeviceTable::reset(std::vector<AS_DeviceEntry> &&table)
{
    m_table = table;
}

void DB_LoadAlarmSystemDevices(AS_DeviceTable *devTable)
{
    const auto dbDevices = DB_LoadAlarmSystemDevices();

    if (dbDevices.empty())
    {
        return;
    }

    std::vector<AS_DeviceEntry> table;
    table.reserve(dbDevices.size());

    for (const auto &dbDev : dbDevices)
    {
        if (strlen(dbDev.uniqueid) > AS_MAX_UNIQUEID_LENGTH)
        {
            continue;
        }

        table.push_back(AS_DeviceEntry());
        AS_DeviceEntry &entry = table.back();

        entry.extAddress = extAddressFromUniqueId(QLatin1String(dbDev.uniqueid));
        entry.alarmSystemId = dbDev.alarmSystemId;
        entry.uniqueIdSize = quint8(strlen(dbDev.uniqueid));
        memcpy(entry.uniqueId, dbDev.uniqueid, entry.uniqueIdSize);
        entry.uniqueId[entry.uniqueIdSize] = '\0';
        entry.flags = dbDev.flags;
        entryInitArmMask(entry);
    }

    devTable->reset(std::move(table));
}
