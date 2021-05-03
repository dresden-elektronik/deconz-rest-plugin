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

#include <QString>
#include <QVariant>
#include <vector>

class Resource;
class ResourceItem;


struct DB_ResourceItem
{
    QString name;
    QVariant value;
    qint64 timestampMs = 0; // milliseconds since Epoch
};

bool DB_StoreSubDevice(const QString &parentUniqueId, const QString &uniqueId);
bool DB_StoreSubDeviceItem(const Resource *sub, const ResourceItem *item);
bool DB_StoreSubDeviceItems(const Resource *sub);
std::vector<DB_ResourceItem> DB_LoadSubDeviceItemsOfDevice(const QString &deviceUniqueId);
std::vector<DB_ResourceItem> DB_LoadSubDeviceItems(const QString &uniqueId);



#endif // DATABASE_H
