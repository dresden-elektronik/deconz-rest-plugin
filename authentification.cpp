/*
 * Copyright (C) 2013 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "de_web_plugin_private.h"
#include <QHttpResponseHeader>
#ifdef Q_OS_UNIX
  #include <unistd.h>
#endif

#ifdef Q_OS_UNIX
    static const char *pwsalt = "$1$8282jdkmskwiu29291"; // $1$ for MD5
#endif

ApiAuth::ApiAuth()
{

}

/*! Init authentification and security.
 */
void DeRestPluginPrivate::initAuthentification()
{
    bool ok = false;

    if (gwConfig.contains("gwusername") && gwConfig.contains("gwpassword"))
    {
        gwAdminUserName = gwConfig["gwusername"].toString();
        gwAdminPasswordHash = gwConfig["gwpassword"].toString();

        if (!gwAdminUserName.isEmpty() && !gwAdminPasswordHash.isEmpty())
        {
            ok = true;
        }
    }

    if (!ok)
    {
        // generate default username and password
        gwAdminUserName = "delight";
        gwAdminPasswordHash = "delight";

        DBG_Printf(DBG_INFO, "create default username and password\n");

        // combine username:password
        QString comb = QString("%1:%2").arg(gwAdminUserName).arg(gwAdminPasswordHash);
        // create base64 encoded version as used in HTTP basic authentification
        QString hash = comb.toLocal8Bit().toBase64();

        gwAdminPasswordHash = encryptString(hash);

        needSaveDatabase = true;
    }

}

/*! Use HTTP basic authentification to check if the request
    has valid credentials to create API key.
 */
bool DeRestPluginPrivate::allowedToCreateApikey(const ApiRequest &req)
{
    if (req.hdr.hasKey("Authorization"))
    {
        QStringList ls = req.hdr.value("Authorization").split(' ');
        if ((ls.size() > 1) && ls[0] == "Basic")
        {
            QString enc = encryptString(ls[1]);

            if (enc == gwAdminPasswordHash)
            {
                return true;
            }

            DBG_Printf(DBG_INFO, "Invalid admin password hash: %s\n", qPrintable(enc));
        }
    }

    return false;
}

/*! Checks if the request is authenticated to access the API.
    \retval true if authenticated
    \retval false if not authenticated and the rsp http status is set to 403 Forbidden and JSON error is appended
 */
bool DeRestPluginPrivate::checkApikeyAuthentification(const ApiRequest &req, ApiResponse &rsp)
{
    QString apikey = req.apikey();

    if (apikey.isEmpty())
    {
        return false;
    }

    std::vector<ApiAuth>::const_iterator i = apiAuths.begin();
    std::vector<ApiAuth>::const_iterator end = apiAuths.end();

    for (; i != end; ++i)
    {
        if (apikey == i->apikey)
        {
            return true;
        }
    }

    // allow non registered devices to use the api if the link button is pressed
    if (gwLinkButton)
    {
        ApiAuth auth;
        auth.apikey = apikey;
        auth.devicetype = "unknown";
        apiAuths.push_back(auth);
        needSaveDatabase = true;
        return true;
    }

    rsp.httpStatus = HttpStatusForbidden;
    rsp.list.append(errorToMap(ERR_UNAUTHORIZED_USER, req.path.join("/"), "unauthorized user"));

    return false;
}

/*! Encrypts a string with using crypt() MD5 + salt. (unix only)
    \param str the input string
    \return the encrypted string on success or the unchanged input string on fail
 */
QString DeRestPluginPrivate::encryptString(const QString &str)
{
#ifdef Q_OS_UNIX
        // further encrypt and salt the hash
        const char *enc = crypt(str.toLocal8Bit().constData(), pwsalt);

        if (enc)
        {
            return QString(enc);
        }
        else
        {
            DBG_Printf(DBG_ERROR, "crypt(): %s failed\n", qPrintable(str));
            // fall through and return str
        }

#endif // Q_OS_UNIX
        return str;
}
