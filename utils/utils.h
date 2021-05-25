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

QString generateUniqueId(quint64 extAddress, quint8 endpoint, quint16 clusterId);
bool startsWith(QLatin1String str, QLatin1String needle);
int indexOf(QLatin1String haystack, QLatin1String needle);
bool contains(QLatin1String haystack, QLatin1String needle);

template <typename K, typename Cont>
decltype(auto) getMappedValue(const K &key, const Cont &cont)
{
    typename Cont::value_type ret{};
    const auto res = std::find_if(cont.cbegin(), cont.cend(), [&key](const auto &i){ return i.key == key; });
    if (res != cont.cend())
    {
        ret = *res;
    }

    return ret;
}

#endif // UTILS_H
