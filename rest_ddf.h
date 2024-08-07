/*
 * Copyright (c) 2024 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef REST_DDF_H
#define REST_DDF_H

class ApiRequest;
class ApiResponse;

/*! REST-API endpoint for DDF. */
int REST_DDF_HandleApi(const ApiRequest &req, ApiResponse &rsp);

/*! Callback for POST DDF bundle request to notify device description code
    of updated bundle data.
 */
void DEV_DDF_BundleUpdated(unsigned char *data, unsigned dataSize);

#endif // REST_DDF_H
