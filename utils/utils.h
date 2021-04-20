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

#endif // UTILS_H
