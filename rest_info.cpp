/*
 * Copyright (c) 2018 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "de_web_plugin.h"
#include "de_web_plugin_private.h"

/*! Info REST API broker.
    \param req - request data
    \param rsp - response data
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::handleInfoApi(ApiRequest &req, ApiResponse &rsp)
{
    rsp.httpStatus = HttpStatusOk;
    return REQ_READY_SEND;
    // return REQ_NOT_HANDLED;
}
