/*
 * Copyright (c) 2013-2019 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include "de_web_plugin_private.h"
#ifdef Q_OS_UNIX
  #include <unistd.h>
#endif

#ifdef Q_OS_UNIX
    static const char *pwsalt = "$1$8282jdkmskwiu29291"; // $1$ for MD5
#endif

#define AUTH_KEEP_ALIVE 240

ApiAuth::ApiAuth() :
    needSaveDatabase(false),
    state(StateNormal)
{

}

/*! Set and process device type.
 */
void ApiAuth::setDeviceType(const QString &devtype)
{
    devicetype = devtype;
}

/*! Init authentication.
 */
void DeRestPluginPrivate::initAuthentication()
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
        // create base64 encoded version as used in HTTP basic authentication
        QString hash = comb.toLocal8Bit().toBase64();

        gwAdminPasswordHash = encryptString(hash);

        queSaveDb(DB_CONFIG, DB_SHORT_SAVE_DELAY);
    }

}

/*! Use HTTP basic authentication or HMAC token to check if the request
    has valid credentials to create API key.
 */
bool DeRestPluginPrivate::allowedToCreateApikey(const ApiRequest &req, ApiResponse &rsp, QVariantMap &map)
{
    if (req.hdr.hasKey(QLatin1String("Authorization")))
    {
        const QString auth(req.hdr.value(QLatin1String("Authorization")));
        const QStringList ls = auth.split(' ');

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

    if (apsCtrl && map.contains(QLatin1String("hmac-sha256")))
    {
        QDateTime now = QDateTime::currentDateTime();
        QByteArray remoteHmac = map["hmac-sha256"].toByteArray();
        QByteArray sec0 = apsCtrl->getParameter(deCONZ::ParamSecurityMaterial0);
        QByteArray installCode = sec0.mid(0, 16);

        if (!gwLastChallenge.isValid() || gwLastChallenge.secsTo(now) > (60 * 10))
        {
            rsp.list.append(errorToMap(ERR_UNAUTHORIZED_USER, QString("/api/challenge"), QString("no active challange")));
            rsp.httpStatus = HttpStatusForbidden;
            return false;
        }

        QByteArray hmac = QMessageAuthenticationCode::hash(gwChallenge, installCode, QCryptographicHash::Sha256).toHex();

        if (remoteHmac == hmac)
        {
            return true;
        }

        DBG_Printf(DBG_INFO, "expected challenge response: %s\n", qPrintable(hmac));
        rsp.list.append(errorToMap(ERR_UNAUTHORIZED_USER, QString("/api/challenge"), QString("invalid challange response")));
        rsp.httpStatus = HttpStatusForbidden;
        return false;
    }

    rsp.httpStatus = HttpStatusForbidden;
    rsp.list.append(errorToMap(ERR_LINK_BUTTON_NOT_PRESSED, "/", "link button not pressed"));
    return false;
}

/*! Authorise API access for the request.
 */
void DeRestPluginPrivate::authorise(ApiRequest &req, ApiResponse &rsp)
{
    Q_UNUSED(rsp);

    QHostAddress localHost(QHostAddress::LocalHost);
    if (req.sock->peerAddress() == localHost)
    {
        req.auth = ApiAuthLocal;
    }

    if (req.sock == nullptr) // allow internal requests, as they are issued by triggering rules
    {
        req.auth = ApiAuthInternal;
    }

    QString apikey = req.apikey();
    apiAuthCurrent = apiAuths.size();

    if (apikey.isEmpty())
    {
        return;
    }

    std::vector<ApiAuth>::iterator i = apiAuths.begin();
    std::vector<ApiAuth>::iterator end = apiAuths.end();

    for (size_t pos = 0; i != end; ++i, pos++)
    {
        if (apikey == i->apikey && i->state == ApiAuth::StateNormal)
        {
            apiAuthCurrent = pos;
            i->lastUseDate = QDateTime::currentDateTimeUtc();

            // fill in useragent string if not already exist
            if (i->useragent.isEmpty())
            {
                if (req.hdr.hasKey(QLatin1String("User-Agent")))
                {
                    i->useragent = req.hdr.value(QLatin1String("User-Agent"));
                    DBG_Printf(DBG_HTTP, "set useragent '%s' for apikey '%s'\n", qPrintable(i->useragent), qPrintable(i->apikey));
                }
            }

            if (req.sock)
            {
                for (TcpClient &cl : openClients)
                {
                    if (cl.sock == req.sock && cl.closeTimeout > 0)
                    {
                        cl.closeTimeout = AUTH_KEEP_ALIVE;
                        break;
                    }
                }
            }

            if ((!(i->useragent.isEmpty()) && i->useragent.startsWith(QLatin1String("iConnect"))) || i->devicetype.startsWith(QLatin1String("iConnectHue")))
            {
                req.mode = ApiModeStrict;
            }
            else if (i->devicetype.startsWith(QLatin1String("Echo")))
            {
                req.mode = ApiModeEcho;
            }
            else if (i->devicetype.startsWith(QLatin1String("Hue Essentials")))
            {
                // supports deCONZ specifics
            }
            else if (i->devicetype.startsWith(QLatin1String("hue_")) ||
                     i->devicetype.startsWith(QLatin1String("Hue ")) ||
                     gwHueMode)
            {
                req.mode = ApiModeHue;
            }
            DBG_Printf(DBG_HTTP, "ApiMode: %d\n", req.mode);

            i->needSaveDatabase = true;
            if (!apiAuthSaveDatabaseTime.isValid() || apiAuthSaveDatabaseTime.elapsed() > (1000 * 60 * 30))
            {
                apiAuthSaveDatabaseTime.start();
                queSaveDb(DB_AUTH, DB_HUGE_SAVE_DELAY);
            }
            req.auth = ApiAuthFull;
        }
    }

#if 0
    // allow non registered devices to use the api if the link button is pressed
    if (gwLinkButton)
    {
        ApiAuth auth;
        auth.needSaveDatabase = true;
        auth.apikey = apikey;
        auth.devicetype = "unknown";
        auth.createDate = QDateTime::currentDateTimeUtc();
        auth.lastUseDate = QDateTime::currentDateTimeUtc();
        apiAuths.push_back(auth);
        queSaveDb(DB_AUTH, DB_SHORT_SAVE_DELAY);
        return true;
    }
#endif

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
