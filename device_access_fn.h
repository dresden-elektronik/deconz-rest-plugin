/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef DEVICE_ACCESS_FN_H
#define DEVICE_ACCESS_FN_H

#include <QString>
#include <QVariant>
#include <vector>

class Resource;
class ResourceItem;

namespace deCONZ {
    class ApsController;
    class ApsDataIndication;
    class ZclFrame;
}

struct DA_ReadResult
{
    bool isEnqueued = false;
    quint8 apsReqId = 0;
    quint8 sequenceNumber = 0;
};

typedef bool (*ParseFunction_t)(Resource *r, ResourceItem *item, const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame, const QVariant &parseParameters);
typedef DA_ReadResult (*ReadFunction_t)(const Resource *r, const ResourceItem *item, deCONZ::ApsController *apsCtrl, const QVariant &readParameters);
typedef bool (*WriteFunction_t)(const Resource *r, const ResourceItem *item, deCONZ::ApsController *apsCtrl, const QVariant &writeParameters);

ParseFunction_t DA_GetParseFunction(const QVariant &params);
ReadFunction_t DA_GetReadFunction(const QVariant &params);
WriteFunction_t DA_GetWriteFunction(const QVariant &params);

#endif // DEVICE_ACCESS_FN_H
