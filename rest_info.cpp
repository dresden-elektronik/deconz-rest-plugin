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
int DeRestPluginPrivate::handleInfoApi(const ApiRequest &req, ApiResponse &rsp)
{
    // GET /api/<apikey>/info/timezones
    if ((req.path.size() == 4) && (req.hdr.method() == "GET") && (req.path[3] == "timezones"))
    {
        return getInfoTimezones(req, rsp);
    }

    return REQ_NOT_HANDLED;
}

/*! GET /api/<apikey>/info/timezones
    \return REQ_READY_SEND
            REQ_NOT_HANDLED
 */
int DeRestPluginPrivate::getInfoTimezones(const ApiRequest &req, ApiResponse &rsp)
{
    Q_UNUSED(req);

    rsp.list = getTimezones();

    rsp.httpStatus = HttpStatusOk;
    return REQ_READY_SEND;
}
