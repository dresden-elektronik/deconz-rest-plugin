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

typedef bool (*ParseFunction_t)(Resource *r, ResourceItem *item, const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame);
typedef bool (*ReadFunction_t)(const Resource *r, const ResourceItem *item, deCONZ::ApsController *apsCtrl);
typedef bool (*WriteFunction_t)(const Resource *r, const ResourceItem *item, deCONZ::ApsController *apsCtrl);

struct ParseFunction
{
    ParseFunction(const QString &_name, const int _arity, ParseFunction_t _fn) :
        name(_name),
        arity(_arity),
        fn(_fn)
    { }
    QString name;
    int arity = 0; // number of parameters given by the device description file
    ParseFunction_t fn = nullptr;
};

struct ReadFunction
{
    ReadFunction(const QString &_name, const int _arity, ReadFunction_t _fn) :
        name(_name),
        arity(_arity),
        fn(_fn)
    { }
    QString name;
    int arity = 0; // number of parameters given by the device description file
    ReadFunction_t fn = nullptr;
};

struct WriteFunction
{
    WriteFunction(const QString &_name, const int _arity, WriteFunction_t _fn) :
        name(_name),
        arity(_arity),
        fn(_fn)
    { }
    QString name;
    int arity = 0; // number of parameters given by the device description file
    WriteFunction_t fn = nullptr;
};

extern const std::vector<ParseFunction> parseFunctions;
extern const std::vector<ReadFunction> readFunctions;
extern const std::vector<WriteFunction> writeFunctions;

ParseFunction_t getParseFunction(const std::vector<ParseFunction> &functions, const std::vector<QVariant> &params);
ReadFunction_t getReadFunction(const std::vector<ReadFunction> &functions, const std::vector<QVariant> &params);
WriteFunction_t getWriteFunction(const std::vector<WriteFunction> &functions, const std::vector<QVariant> &params);


#endif // DEVICE_ACCESS_FN_H
