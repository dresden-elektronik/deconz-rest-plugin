/*
 * Copyright (c) 2024 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef REST_API_H
#define REST_API_H

#include <QString>
#include <QList>
#include <QVariant>

#include <deconz/qhttprequest_compat.h>
#include <deconz/http_client_handler.h>

/*! JSON generic error message codes */
#define ERR_UNAUTHORIZED_USER          1
#define ERR_INVALID_JSON               2
#define ERR_RESOURCE_NOT_AVAILABLE     3
#define ERR_METHOD_NOT_AVAILABLE       4
#define ERR_MISSING_PARAMETER          5
#define ERR_PARAMETER_NOT_AVAILABLE    6
#define ERR_INVALID_VALUE              7
#define ERR_PARAMETER_NOT_MODIFIABLE   8
#define ERR_TOO_MANY_ITEMS             11
#define ERR_INVALID_DDF_BUNDLE         12
#define ERR_DUPLICATE_EXIST            100 // de extension
#define ERR_NOT_ALLOWED_SENSOR_TYPE    501
#define ERR_SENSOR_LIST_FULL           502
#define ERR_RULE_ENGINE_FULL           601
#define ERR_CONDITION_ERROR            607
#define ERR_ACTION_ERROR               608
#define ERR_INTERNAL_ERROR             901

#define ERR_NOT_CONNECTED              950 // de extension
#define ERR_BRIDGE_BUSY                951 // de extension

#define ERR_LINK_BUTTON_NOT_PRESSED    101
#define ERR_DEVICE_OFF                 201
#define ERR_DEVICE_NOT_REACHABLE       202
#define ERR_BRIDGE_GROUP_TABLE_FULL    301
#define ERR_DEVICE_GROUP_TABLE_FULL    302

#define ERR_DEVICE_SCENES_TABLE_FULL   402 // de extension

// REST API return codes
#define REQ_READY_SEND   0
#define REQ_NOT_HANDLED -1

// HTTP status codes
extern const char *HttpStatusOk;
extern const char *HttpStatusAccepted;
extern const char *HttpStatusNotModified;
extern const char *HttpStatusUnauthorized;
extern const char *HttpStatusBadRequest;
extern const char *HttpStatusForbidden;
extern const char *HttpStatusNotFound;
extern const char *HttpStatusNotImplemented;
extern const char *HttpStatusServiceUnavailable;
extern const char *HttpContentHtml;
extern const char *HttpContentCss;
extern const char *HttpContentJson;
extern const char *HttpContentJS;
extern const char *HttpContentPNG;
extern const char *HttpContentJPG;
extern const char *HttpContentSVG;
extern const char *HttpContentOctetStream;

enum ApiVersion
{
    ApiVersion_1,        //!< common version 1.0
    ApiVersion_1_DDEL,   //!< version 1.0, "Accept: application/vnd.ddel.v1"
    ApiVersion_1_1_DDEL, //!< version 1.1, "Accept: application/vnd.ddel.v1.1"
    ApiVersion_2_DDEL,   //!< version 2.0, "Accept: application/vnd.ddel.v2"
    ApiVersion_3_DDEL    //!< version 3.0, "Accept: application/vnd.ddel.v3"
};

enum ApiAuthorisation
{
    ApiAuthNone,
    ApiAuthLocal,
    ApiAuthInternal,
    ApiAuthFull
};

enum ApiMode
{
    ApiModeNormal,
    ApiModeStrict,
    ApiModeEcho,
    ApiModeHue
};

/*! \class ApiRequest

    Helper to simplify HTTP REST request handling.
 */
class ApiRequest
{
public:
    ApiRequest(const QHttpRequestHeader &h, const QStringList &p, QTcpSocket *s, const QString &c);
    QString apikey() const;
    ApiVersion apiVersion() const { return version; }

    const QHttpRequestHeader &hdr;
    const QStringList &path;
    QTcpSocket *sock;
    QString content;
    ApiVersion version;
    ApiAuthorisation auth;
    ApiMode mode;
};

/*! \class ApiResponse

    Helper to simplify HTTP REST request handling.
 */
class ApiResponse
{
public:
    QString etag;
    const char *httpStatus = nullptr;
    const char *contentType = nullptr;
    unsigned contentLength = 0;
    const char *fileName = nullptr; // for Content-Disposition: attachment: filename="<fileName>"
    QVariantMap map; // json content
    QVariantList list; // json content
    QString str; // json string
    char *bin = nullptr;
};

// REST API common
QVariantMap errorToMap(int id, const QString &ressource, const QString &description);

#endif // REST_API_H
