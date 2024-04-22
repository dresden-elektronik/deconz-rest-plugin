/*
 * Copyright (c) 2024 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <deconz/dbg_trace.h>
#include "rest_api.h"

const char *HttpStatusOk           = "200 OK"; // OK
const char *HttpStatusAccepted     = "202 Accepted"; // Accepted but not complete
const char *HttpStatusNotModified  = "304 Not Modified"; // For ETag / If-None-Match
const char *HttpStatusBadRequest   = "400 Bad Request"; // Malformed request
const char *HttpStatusUnauthorized = "401 Unauthorized"; // Unauthorized
const char *HttpStatusForbidden    = "403 Forbidden"; // Understand request but no permission
const char *HttpStatusNotFound     = "404 Not Found"; // Requested uri not found
const char *HttpStatusServiceUnavailable = "503 Service Unavailable";
const char *HttpStatusNotImplemented = "501 Not Implemented";
const char *HttpContentHtml        = "text/html; charset=utf-8";
const char *HttpContentCss         = "text/css";
const char *HttpContentJson        = "application/json; charset=utf-8";
const char *HttpContentJS          = "text/javascript";
const char *HttpContentPNG         = "image/png";
const char *HttpContentJPG         = "image/jpg";
const char *HttpContentSVG         = "image/svg+xml";
const char *HttpContentOctetStream = "application/octet-stream";

/*! Creates a error map used in JSON response.
    \param id - error id
    \param ressource example: "/lights/2"
    \param description example: "resource, /lights/2, not available"
    \return the map
 */
QVariantMap errorToMap(int id, const QString &ressource, const QString &description)
{
    QVariantMap map;
    QVariantMap error;
    error["type"] = (double)id;
    error["address"] = ressource.toHtmlEscaped();
    error["description"] = description.toHtmlEscaped();
    map["error"] = error;

    DBG_Printf(DBG_INFO_L2, "API error %d, %s, %s\n", id, qPrintable(ressource), qPrintable(description));

    return map;
}
