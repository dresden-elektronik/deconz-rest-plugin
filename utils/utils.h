/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef UTILS_H
#define UTILS_H

#include <QString>
#include <QVariant>
#include <array>
#include "resource.h"

namespace deCONZ {
    class Address;
    class ApsController;
    class Node;
}

struct KeyMap
{
    QLatin1String key;
};

struct KeyValMap
{
    QLatin1String key;
    quint8 value;
};

struct KeyValMapInt
{
    quint8 key = 0;
    quint16 value = 0;
};

struct KeyValMapTuyaSingle
{
    QLatin1String key;
    char value[1];
};

struct RestData
{
    bool boolean;
    int integer;
    uint uinteger;
    QString string;
    float real;
    bool valid = false;
};

QString generateUniqueId(quint64 extAddress, quint8 endpoint, quint16 clusterId);
bool startsWith(QLatin1String str, QLatin1String needle);
int indexOf(QLatin1String haystack, QLatin1String needle);
bool contains(QLatin1String haystack, QLatin1String needle);
RestData verifyRestData(const ResourceItemDescriptor &rid, const QVariant &val);
bool isSameAddress(const deCONZ::Address &a, const deCONZ::Address &b);

inline bool isValid(const KeyMap &entry) { return entry.key.size() != 0; }
inline bool isValid(const KeyValMap &entry) { return entry.key.size() != 0; }
inline bool isValid(const KeyValMapTuyaSingle &entry) { return entry.key.size() != 0; }
// following is needed for GCC versions < 7
constexpr KeyMap invalidValue(KeyMap) { return KeyMap{QLatin1String("")}; }
constexpr KeyValMap invalidValue(KeyValMap) { return KeyValMap{QLatin1String(""), 0 }; }
constexpr KeyValMapInt invalidValue(KeyValMapInt) { return KeyValMapInt{ 0, 0 }; }
constexpr KeyValMapTuyaSingle invalidValue(KeyValMapTuyaSingle) { return KeyValMapTuyaSingle{QLatin1String(""), {0} }; }

template <typename K, typename Cont, typename V = typename Cont::value_type>
decltype(auto) matchKeyValue(const K &key, const Cont &cont)
{
    V ret = invalidValue(ret);
    const auto res = std::find_if(cont.cbegin(), cont.cend(), [&key](const auto &i){ return i.key == key; });
    if (res != cont.cend())
    {
        ret = *res;
    }

    return ret;
}

const deCONZ::Node *getCoreNode(quint64 extAddress, deCONZ::ApsController *apsCtrl);

#endif // UTILS_H
