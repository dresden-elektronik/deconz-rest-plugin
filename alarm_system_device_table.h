/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef ALARM_SYSTEM_DEVICE_TABLE_H
#define ALARM_SYSTEM_DEVICE_TABLE_H

#include <QString>
#include <vector>

// 28:6d:97:00:01:06:41:79-01-0500  31 characters
#define AS_MAX_UNIQUEID_LENGTH 31

#define AS_ENTRY_FLAG_ARMED_AWAY  0x00000100
#define AS_ENTRY_FLAG_ARMED_STAY  0x00000200
#define AS_ENTRY_FLAG_ARMED_NIGHT 0x00000400
#define AS_ENTRY_FLAG_IAS_ACE     0x00000008

/*! \struct AS_DeviceEntry

    Holds a uniqueid and configuration for a device in an alarm system.
    The size is 64 bytes to fit in one cache line.
 */
struct AS_DeviceEntry
{
    char uniqueId[AS_MAX_UNIQUEID_LENGTH + 1]{0};
    quint64 extAddress = 0;
    quint32 flags = 0;
    quint8 uniqueIdSize = 0;
    quint8 alarmSystemId = 0;
    char armMask[4]{0};
    char padding[14]{0};
};

class AS_DeviceTable
{
public:
    AS_DeviceTable();

    const AS_DeviceEntry &get(const QString &uniqueId) const;
    const AS_DeviceEntry &get(quint64 extAddress) const;
    const AS_DeviceEntry &at(size_t index) const;
    bool put(const QString &uniqueId, quint32 flags, quint8 alarmSystemId);
    bool erase(const QLatin1String &uniqueId);
    size_t size() const { return m_table.size(); }
    void reset(std::vector<AS_DeviceEntry> && table);

private:
    const AS_DeviceEntry m_invalidEntry{};
    std::vector<AS_DeviceEntry> m_table;
};

inline bool isValid(const AS_DeviceEntry &entry)
{
    return entry.uniqueId[0] != '\0' &&
           entry.uniqueIdSize > 0 &&
           entry.alarmSystemId > 0 &&
           entry.extAddress != 0;
}

void DB_LoadAlarmSystemDevices(AS_DeviceTable *devTable);

#endif // ALARM_SYSTEM_DEVICE_TABLE_H
