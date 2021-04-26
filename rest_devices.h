/*
 * Copyright (c) 2013-2019 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef REST_DEVICES_H
#define REST_DEVICES_H

#include <QObject>

class DeRestPluginPrivate;
class ApiRequest;
class ApiResponse;

/*! \class RestDevices

    REST-API endpoint for devices.
 */

class RestDevices : public QObject
{
    Q_OBJECT

public:
    explicit RestDevices(QObject *parent = nullptr);
    int handleApi(const ApiRequest &req, ApiResponse &rsp);

    bool deleteDevice(quint64 extAddr);

private:
    int getAllDevices(const ApiRequest &req, ApiResponse &rsp);
    int getDevice(const ApiRequest &req, ApiResponse &rsp);
    int putDeviceInstallCode(const ApiRequest &req, ApiResponse &rsp);

    DeRestPluginPrivate *plugin;
};

#endif // REST_DEVICES_H
