/*
 * Copyright (c) 2016-2018 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QString>
#include <QStringBuilder>
#include <QElapsedTimer>
#include <unistd.h>
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "deconz/dbg_trace.h"
#include "gateway.h"
#include "json.h"

/******************************************************************************
                    Local prototypes
******************************************************************************/
static int sqliteLoadAuthCallback(void *user, int ncols, char **colval , char **colname);
static int sqliteLoadConfigCallback(void *user, int ncols, char **colval , char **colname);
static int sqliteLoadUserparameterCallback(void *user, int ncols, char **colval , char **colname);
static int sqliteLoadLightNodeCallback(void *user, int ncols, char **colval , char **colname);
static int sqliteLoadAllGroupsCallback(void *user, int ncols, char **colval , char **colname);
static int sqliteLoadAllResourcelinksCallback(void *user, int ncols, char **colval , char **colname);
static int sqliteLoadGroupCallback(void *user, int ncols, char **colval , char **colname);
static int sqliteLoadAllScenesCallback(void *user, int ncols, char **colval , char **colname);
static int sqliteLoadSceneCallback(void *user, int ncols, char **colval , char **colname);
static int sqliteLoadAllRulesCallback(void *user, int ncols, char **colval , char **colname);
static int sqliteLoadAllSensorsCallback(void *user, int ncols, char **colval , char **colname);
static int sqliteGetAllLightIdsCallback(void *user, int ncols, char **colval , char **colname);
static int sqliteGetAllSensorIdsCallback(void *user, int ncols, char **colval , char **colname);
static int sqliteLoadAllGatewaysCallback(void *user, int ncols, char **colval , char **colname);

/******************************************************************************
                    Implementation
******************************************************************************/

/*! Inits the database and creates tables/columns if necessary.
 */
void DeRestPluginPrivate::initDb()
{
    DBG_Assert(db != 0);

    if (!db)
    {
        return;
    }

    int rc;
    char *errmsg;

    // create tables

    const char *sql[] = {
        "CREATE TABLE IF NOT EXISTS auth (apikey TEXT PRIMARY KEY, devicetype TEXT)",
        "CREATE TABLE IF NOT EXISTS userparameter (key TEXT PRIMARY KEY, value TEXT)",
        "CREATE TABLE IF NOT EXISTS nodes (mac TEXT PRIMARY KEY, id TEXT, state TEXT, name TEXT, groups TEXT, endpoint TEXT, modelid TEXT, manufacturername TEXT, swbuildid TEXT)",
        "ALTER TABLE nodes add column id TEXT",
        "ALTER TABLE nodes add column state TEXT",
        "ALTER TABLE nodes add column groups TEXT",
        "ALTER TABLE nodes add column endpoint TEXT",
        "ALTER TABLE nodes add column modelid TEXT",
        "ALTER TABLE nodes add column manufacturername TEXT",
        "ALTER TABLE nodes add column swbuildid TEXT",
        "ALTER TABLE auth add column createdate TEXT",
        "ALTER TABLE auth add column lastusedate TEXT",
        "ALTER TABLE auth add column useragent TEXT",
        "CREATE TABLE IF NOT EXISTS groups (gid TEXT PRIMARY KEY, name TEXT, state TEXT, mids TEXT, devicemembership TEXT, lightsequence TEXT, hidden TEXT)",
        "CREATE TABLE IF NOT EXISTS resourcelinks (id TEXT PRIMARY KEY, json TEXT)",
        "CREATE TABLE IF NOT EXISTS rules (rid TEXT PRIMARY KEY, name TEXT, created TEXT, etag TEXT, lasttriggered TEXT, owner TEXT, status TEXT, timestriggered TEXT, actions TEXT, conditions TEXT, periodic TEXT)",
        "CREATE TABLE IF NOT EXISTS sensors (sid TEXT PRIMARY KEY, name TEXT, type TEXT, modelid TEXT, manufacturername TEXT, uniqueid TEXT, swversion TEXT, state TEXT, config TEXT, fingerprint TEXT, deletedState TEXT, mode TEXT)",
        "CREATE TABLE IF NOT EXISTS scenes (gsid TEXT PRIMARY KEY, gid TEXT, sid TEXT, name TEXT, transitiontime TEXT, lights TEXT)",
        "CREATE TABLE IF NOT EXISTS schedules (id TEXT PRIMARY KEY, json TEXT)",
        "CREATE TABLE IF NOT EXISTS gateways (uuid TEXT PRIMARY KEY, name TEXT, ip TEXT, port TEXT, pairing TEXT, apikey TEXT, cgroups TEXT)",
        "ALTER TABLE sensors add column fingerprint TEXT",
        "ALTER TABLE sensors add column deletedState TEXT",
        "ALTER TABLE sensors add column mode TEXT",
        "ALTER TABLE groups add column state TEXT",
        "ALTER TABLE groups add column mids TEXT",
        "ALTER TABLE groups add column devicemembership TEXT",
        "ALTER TABLE groups add column lightsequence TEXT",
        "ALTER TABLE groups add column hidden TEXT",
        "ALTER TABLE groups add column type TEXT",
        "ALTER TABLE groups add column class TEXT",
        "ALTER TABLE scenes add column transitiontime TEXT",
        "ALTER TABLE scenes add column lights TEXT",
        "ALTER TABLE rules add column periodic TEXT",
        NULL
        };

    for (int i = 0; sql[i] != NULL; i++)
    {
        errmsg = NULL;
        rc = sqlite3_exec(db, sql[i], NULL, NULL, &errmsg);

        if (rc != SQLITE_OK)
        {
            if (errmsg)
            {
                DBG_Printf(DBG_ERROR_L2, "SQL exec failed: %s, error: %s\n", sql[i], errmsg);
                sqlite3_free(errmsg);
            }
        }
    }
}

/*! Clears all content of tables of db except auth table
 */
void DeRestPluginPrivate::clearDb()
{
    DBG_Assert(db != 0);

    if (!db)
    {
        return;
    }

    int rc;
    char *errmsg;

    // clear tables

    const char *sql[] = {
        "DELETE FROM auth",
        "DELETE FROM config2",
        "DELETE FROM userparameter",
        "DELETE FROM nodes",
        "DELETE FROM groups",
        "DELETE FROM resourcelinks",
        "DELETE FROM rules",
        "DELETE FROM sensors",
        "DELETE FROM scenes",
        "DELETE FROM schedules",
        NULL
        };

    for (int i = 0; sql[i] != NULL; i++)
    {
        errmsg = NULL;
        rc = sqlite3_exec(db, sql[i], NULL, NULL, &errmsg);

        if (rc != SQLITE_OK)
        {
            if (errmsg)
            {
                DBG_Printf(DBG_ERROR_L2, "SQL exec failed: %s, error: %s\n", sql[i], errmsg);
                sqlite3_free(errmsg);
            }
        }
    }
}


/*! Opens/creates sqlite database.
 */
void DeRestPluginPrivate::openDb()
{
    //DBG_Assert(db == 0);

    if (db)
    {
        ttlDataBaseConnection = idleTotalCounter + DB_CONNECTION_TTL;
        return;
    }

    int rc = sqlite3_open(qPrintable(sqliteDatabaseName), &db);

    if (rc != SQLITE_OK) {
        // failed
        DBG_Printf(DBG_ERROR, "Can't open database: %s\n", sqlite3_errmsg(db));
        db = 0;
        return;
    }

    ttlDataBaseConnection = idleTotalCounter + DB_CONNECTION_TTL;
}

/*! Reads all data sets from sqlite database.
 */
void DeRestPluginPrivate::readDb()
{
    DBG_Assert(db != 0);

    if (!db)
    {
        return;
    }

    loadAuthFromDb();
    loadConfigFromDb();
    loadUserparameterFromDb();
    loadAllGroupsFromDb();
    loadAllResourcelinksFromDb();
    loadAllScenesFromDb();
    loadAllRulesFromDb();
    loadAllSchedulesFromDb();
    loadAllSensorsFromDb();
    loadAllGatewaysFromDb();
}

/*! Sqlite callback to load authentification data.
 */
static int sqliteLoadAuthCallback(void *user, int ncols, char **colval , char **colname)
{
    Q_UNUSED(colname);
    DBG_Assert(user != 0);
    DBG_Assert(ncols == 5);

    if (!user || (ncols != 5))
    {
        return 0;
    }

    DeRestPluginPrivate *d = static_cast<DeRestPluginPrivate*>(user);

    ApiAuth auth;

    auth.apikey = colval[0];
    auth.setDeviceType(colval[1]);

    if (colval[4])
    {
        auth.useragent = colval[4];
    }

    // fill in createdate and lastusedate
    // if they not exist in database yet
    if (colval[2] && colval[3])
    {
        auth.createDate = QDateTime::fromString(colval[2], "yyyy-MM-ddTHH:mm:ss"); // ISO 8601
        auth.lastUseDate = QDateTime::fromString(colval[3], "yyyy-MM-ddTHH:mm:ss"); // ISO 8601
    }
    else
    {
        auth.createDate = QDateTime::currentDateTimeUtc();
        auth.lastUseDate = QDateTime::currentDateTimeUtc();
    }

    if (!auth.createDate.isValid())
    {
        auth.createDate = QDateTime::currentDateTimeUtc();
    }

    if (!auth.lastUseDate.isValid())
    {
        auth.lastUseDate = QDateTime::currentDateTimeUtc();
    }

    auth.createDate.setTimeSpec(Qt::UTC);
    auth.lastUseDate.setTimeSpec(Qt::UTC);

    if (!auth.apikey.isEmpty() && !auth.devicetype.isEmpty())
    {
        d->apiAuths.push_back(auth);
    }

    return 0;
}

/*! Loads all authentification data from database.
 */
void DeRestPluginPrivate::loadAuthFromDb()
{
    int rc;
    char *errmsg = 0;

    DBG_Assert(db != 0);

    if (!db)
    {
        return;
    }

    QString sql = QString(QLatin1String("SELECT apikey,devicetype,createdate,lastusedate,useragent FROM auth"));

    DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
    rc = sqlite3_exec(db, qPrintable(sql), sqliteLoadAuthCallback, this, &errmsg);

    if (rc != SQLITE_OK)
    {
        if (errmsg)
        {
            DBG_Printf(DBG_ERROR, "sqlite3_exec %s, error: %s\n", qPrintable(sql), errmsg);
            sqlite3_free(errmsg);
        }
    }
}

/*! Sqlite callback to load configuration data.
 */
static int sqliteLoadConfigCallback(void *user, int ncols, char **colval , char **colname)
{
    Q_UNUSED(colname);
    DBG_Assert(user != 0);

    if (!user || (ncols != 2))
    {
        return 0;
    }

    DBG_Printf(DBG_INFO_L2, "Load config from db.\n");

    bool ok;
    DeRestPluginPrivate *d = static_cast<DeRestPluginPrivate*>(user);

    QString val = QString::fromUtf8(colval[1]);

    if (strcmp(colval[0], "name") == 0)
    {
        if (!val.isEmpty())
        {
            d->gwName = val;
            d->gwConfig["name"] = val;
        }
    }
    else if (strcmp(colval[0], "announceinterval") == 0)
    {
        if (!val.isEmpty())
        {
            int minutes = val.toInt(&ok);
            if (ok && (minutes >= 0))
            {
                d->gwAnnounceInterval = minutes;
                d->gwConfig["announceinterval"] = (double)minutes;
            }
        }
    }
    else if (strcmp(colval[0], "announceurl") == 0)
    {
        if (!val.isEmpty())
        {
            d->gwAnnounceUrl = val;
            d->gwConfig["announceurl"] = val;
        }
    }
    else if (strcmp(colval[0], "rfconnect") == 0)
    {
        if (!val.isEmpty())
        {
            int conn = val.toInt(&ok);
            if (ok && ((conn == 0) || (conn == 1)))
            {
                d->gwRfConnectedExpected = (conn == 1);
            }
        }
    }
    else if (strcmp(colval[0], "permitjoin") == 0)
    {
        if (!val.isEmpty())
        {
            uint seconds = val.toUInt(&ok);
            if (ok && (seconds <= 255))
            {
                d->setPermitJoinDuration(seconds);
                d->gwConfig["permitjoin"] = (double)seconds;
            }
        }
    }
    else if (strcmp(colval[0], "networkopenduration") == 0)
    {
        if (!val.isEmpty())
        {
            uint seconds = val.toUInt(&ok);
            if (ok)
            {
                d->gwNetworkOpenDuration = seconds;
                d->gwConfig["networkopenduration"] = (double)seconds;
            }
        }
    }
    else if (strcmp(colval[0], "timeformat") == 0)
    {
        if (!val.isEmpty())
        {
            d->gwTimeFormat = val;
            d->gwConfig["timeformat"] = val;
        }
    }
    else if (strcmp(colval[0], "timezone") == 0)
    {
        if (!val.isEmpty())
        {
            d->gwTimezone = val;
            d->gwConfig["timezone"] = val;
        }
    }
    else if (strcmp(colval[0], "rgbwdisplay") == 0)
    {
        if (!val.isEmpty())
        {
            d->gwRgbwDisplay = val;
            d->gwConfig["rgbwdisplay"] = val;
        }
    }
#if 0
    else if (strcmp(colval[0], "groupdelay") == 0)
    {
        if (!val.isEmpty())
        {
            uint milliseconds = val.toUInt(&ok);
            if (ok && (milliseconds <= MAX_GROUP_SEND_DELAY))
            {
                d->gwGroupSendDelay = milliseconds;
                d->gwConfig["groupdelay"] = (double)milliseconds;
            }
        }
    }
#endif
    else if (strcmp(colval[0], "zigbeechannel") == 0)
    {
        if (!val.isEmpty())
        {
            uint zigbeechannel = val.toUInt(&ok);
            if (ok && ((zigbeechannel == 0) || (zigbeechannel == 11) || (zigbeechannel == 15) || (zigbeechannel == 20) || (zigbeechannel == 25)))
            {
                d->gwZigbeeChannel = zigbeechannel;
                d->gwConfig["zigbeechannel"] = (uint)zigbeechannel;
            }
        }
    }
    else if (strcmp(colval[0], "updatechannel") == 0)
    {
        if ((val == "stable") || (val == "alpha") || (val == "beta"))
        {
            d->gwUpdateChannel = val;
            d->gwConfig["updatechannel"] = val;
        }
        else
        {
            DBG_Printf(DBG_ERROR, "DB unexpected value for updatechannel: %s\n", qPrintable(val));
        }
    }
    else if (strcmp(colval[0], "gwusername") == 0)
    {
        if (!val.isEmpty())
        {
            d->gwConfig["gwusername"] = val;
            d->gwAdminUserName = val;
        }
    }
    else if (strcmp(colval[0], "gwpassword") == 0)
    {
        if (!val.isEmpty())
        {
            d->gwConfig["gwpassword"] = val;
            d->gwAdminPasswordHash = val;
        }
    }
    else if (strcmp(colval[0], "uuid") == 0)
    {
        if (!val.isEmpty())
        {
            d->gwConfig["uuid"] = val;
            d->gwUuid = val.replace("{", "").replace("}", "");
        }
    }
    else if (strcmp(colval[0], "otauactive") == 0)
    {
        if (!val.isEmpty())
        {
            uint otauActive = 1;

            if (val == "true")
            {
                otauActive = 1;
            }
            else if (val == "false")
            {
                otauActive = 0;
            }
            else
            {
                otauActive = val.toUInt(&ok);
                if (!ok || (otauActive != 0 && otauActive != 1))
                {
                    otauActive = 1;
                }
            }

            if (d->apsCtrl)
            {
                d->apsCtrl->setParameter(deCONZ::ParamOtauActive, otauActive);
            }
        }
    }
    else if (strcmp(colval[0], "wifi") == 0)
    {
        if (!val.isEmpty())
        {
            d->gwConfig["wifi"] = val;
            d->gwWifi = val;
        }
    }
    else if (strcmp(colval[0], "wifichannel") == 0)
    {
        if (!val.isEmpty())
        {
            d->gwConfig["wifichannel"] = val;
            d->gwWifiChannel = val;
        }
    }
    else if (strcmp(colval[0], "wifiname") == 0)
    {
        if (!val.isEmpty())
        {
            d->gwConfig["wifiname"] = val;
            d->gwWifiName = val;
        }
    }
    else if (strcmp(colval[0], "wificlientname") == 0)
    {
        if (!val.isEmpty())
        {
            d->gwConfig["wificlientname"] = val;
            d->gwWifiClientName = val;
        }
    }
    else if (strcmp(colval[0], "wifiappw") == 0)
    {
        if (!val.isEmpty())
        {
            //d->gwConfig["wifiappw"] = val;
            d->gwWifiPw = val;
        }
    }
    else if (strcmp(colval[0], "wificlientpw") == 0)
    {
        if (!val.isEmpty())
        {
            //d->gwConfig["wificlientpw"] = val;
            d->gwWifiClientPw = val;
        }
    }
    else if (strcmp(colval[0], "wifitype") == 0)
    {
        if (!val.isEmpty())
        {
            d->gwConfig["wifitype"] = val;
            d->gwWifiType = val;
        }
    }
    else if (strcmp(colval[0], "wifiip") == 0)
    {
        if (!val.isEmpty())
        {
            d->gwConfig["wifiip"] = val;
            d->gwWifiIp = val;
        }
    }
    else if (strcmp(colval[0], "userparameter") == 0)
    {
        if (!val.isEmpty())
        {
            d->gwConfig["userparameter"] = Json::parse(val);
            bool ok;
            QVariant var = Json::parse(val, ok);

            if (ok)
            {
                QVariantMap map = var.toMap();
                d->gwUserParameter = map;
            }
        }
    }
    else if (strcmp(colval[0], "bridgeid") == 0)
    {
      if (!val.isEmpty())
      {
          d->gwConfig["bridgeid"] = val;
          d->gwBridgeId = val;
      }
    }
    else if (strcmp(colval[0], "websocketport") == 0)
    {
        quint16 port = val.toUInt(&ok);
        if (!val.isEmpty() && ok)
        {
            d->gwConfig["websocketport"] = port;
        }
    }
    else if (strcmp(colval[0], "websocketnotifyall") == 0)
    {
      if (!val.isEmpty())
      {
          bool notifyAll = val == "true";
          d->gwConfig["websocketnotifyall"] = notifyAll;
          d->gwWebSocketNotifyAll = notifyAll;
      }
    }
    else if (strcmp(colval[0], "proxyaddress") == 0)
    {
      if (!val.isEmpty())
      {
          d->gwConfig["proxyaddress"] = val;
          d->gwProxyAddress = val;
      }
    }
    else if (strcmp(colval[0], "proxyport") == 0)
    {
        quint16 port = val.toUInt(&ok);
        if (!val.isEmpty() && ok)
        {
            d->gwConfig["proxyport"] = port;
            d->gwProxyPort = port;
        }
    }
    else if (strcmp(colval[0], "swupdatestate") == 0)
    {
        if (!val.isEmpty() && ok)
        {
            d->gwConfig["swupdatestate"] = val;
            d->gwSwUpdateState = val;
        }
    }
    return 0;
}

/*! Sqlite callback to load userparameter data.
 */
static int sqliteLoadUserparameterCallback(void *user, int ncols, char **colval , char **colname)
{
    Q_UNUSED(colname);
    DBG_Assert(user != 0);

    if (!user || (ncols != 2))
    {
        return 0;
    }

    DeRestPluginPrivate *d = static_cast<DeRestPluginPrivate*>(user);

    QString key = QString::fromUtf8(colval[0]);
    QString val = QString::fromUtf8(colval[1]);

    if (!val.isEmpty())
    {
        d->gwUserParameter[key] = val;
    }

    return 0;
}

/*! Loads all config from database
 */
void DeRestPluginPrivate::loadConfigFromDb()
{
    int rc;
    char *errmsg = 0;

    DBG_Assert(db != 0);

    if (!db)
    {
        return;
    }

    QString configTable = "config"; // default config table version 1

    // check if config table version 2
    {
        QString sql = QString("SELECT key FROM config2");

        DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
        errmsg = NULL;
        rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

        if (rc == SQLITE_OK)
        {
            configTable = "config2";
        }
    }

    {
        QString sql = QString("SELECT key,value FROM %1").arg(configTable);

        DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
        rc = sqlite3_exec(db, qPrintable(sql), sqliteLoadConfigCallback, this, &errmsg);

        if (rc != SQLITE_OK)
        {
            if (errmsg)
            {
                DBG_Printf(DBG_ERROR, "sqlite3_exec %s, error: %s\n", qPrintable(sql), errmsg);
                sqlite3_free(errmsg);
            }
        }
    }
}

/*! Loads all config from database
 */
void DeRestPluginPrivate::loadSwUpdateStateFromDb()
{
    int rc;
    char *errmsg = 0;

    DBG_Assert(db != 0);

    if (!db)
    {
        return;
    }

    {
        QString sql = QLatin1String("SELECT * FROM config2 WHERE key='swupdatestate'");

        DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
        rc = sqlite3_exec(db, qPrintable(sql), sqliteLoadConfigCallback, this, &errmsg);

        if (rc != SQLITE_OK)
        {
            if (errmsg)
            {
                DBG_Printf(DBG_ERROR, "sqlite3_exec %s, error: %s\n", qPrintable(sql), errmsg);
                sqlite3_free(errmsg);
            }
        }
    }
}

/*! Loads wifi information from database
 */
void DeRestPluginPrivate::loadWifiInformationFromDb()
{
    int rc;
    char *errmsg = 0;

    DBG_Assert(db != 0);

    if (!db)
    {
        return;
    }

    {
        QString sql = QLatin1String("SELECT * FROM config2 WHERE key='availablewifi'");

        DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
        rc = sqlite3_exec(db, qPrintable(sql), sqliteLoadConfigCallback, this, &errmsg);

        if (rc != SQLITE_OK)
        {
            if (errmsg)
            {
                DBG_Printf(DBG_ERROR, "sqlite3_exec %s, error: %s\n", qPrintable(sql), errmsg);
                sqlite3_free(errmsg);
            }
        }
        sql = QLatin1String("SELECT * FROM config2 WHERE key='wifiip'");

        DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
        rc = sqlite3_exec(db, qPrintable(sql), sqliteLoadConfigCallback, this, &errmsg);

        if (rc != SQLITE_OK)
        {
            if (errmsg)
            {
                DBG_Printf(DBG_ERROR, "sqlite3_exec %s, error: %s\n", qPrintable(sql), errmsg);
                sqlite3_free(errmsg);
            }
        }
        sql = QLatin1String("SELECT * FROM config2 WHERE key='wifitype'");

        DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
        rc = sqlite3_exec(db, qPrintable(sql), sqliteLoadConfigCallback, this, &errmsg);

        if (rc != SQLITE_OK)
        {
            if (errmsg)
            {
                DBG_Printf(DBG_ERROR, "sqlite3_exec %s, error: %s\n", qPrintable(sql), errmsg);
                sqlite3_free(errmsg);
            }
        }
        sql = QLatin1String("SELECT * FROM config2 WHERE key='wifi'");

        DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
        rc = sqlite3_exec(db, qPrintable(sql), sqliteLoadConfigCallback, this, &errmsg);

        if (rc != SQLITE_OK)
        {
            if (errmsg)
            {
                DBG_Printf(DBG_ERROR, "sqlite3_exec %s, error: %s\n", qPrintable(sql), errmsg);
                sqlite3_free(errmsg);
            }
        }
        sql = QLatin1String("SELECT * FROM config2 WHERE key='wifiname'");

        DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
        rc = sqlite3_exec(db, qPrintable(sql), sqliteLoadConfigCallback, this, &errmsg);

        if (rc != SQLITE_OK)
        {
            if (errmsg)
            {
                DBG_Printf(DBG_ERROR, "sqlite3_exec %s, error: %s\n", qPrintable(sql), errmsg);
                sqlite3_free(errmsg);
            }
        }
        sql = QLatin1String("SELECT * FROM config2 WHERE key='wificlientname'");

        DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
        rc = sqlite3_exec(db, qPrintable(sql), sqliteLoadConfigCallback, this, &errmsg);

        if (rc != SQLITE_OK)
        {
            if (errmsg)
            {
                DBG_Printf(DBG_ERROR, "sqlite3_exec %s, error: %s\n", qPrintable(sql), errmsg);
                sqlite3_free(errmsg);
            }
        }
    }
}

/*! Loads all userparameter from database
 */
void DeRestPluginPrivate::loadUserparameterFromDb()
{
    int rc;
    char *errmsg = 0;

    DBG_Assert(db != 0);

    if (!db)
    {
        return;
    }

    {
        QString sql = QString("SELECT key,value FROM %1").arg("userparameter");

        DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
        rc = sqlite3_exec(db, qPrintable(sql), sqliteLoadUserparameterCallback, this, &errmsg);

        if (rc != SQLITE_OK)
        {
            if (errmsg)
            {
                DBG_Printf(DBG_ERROR, "sqlite3_exec %s, error: %s\n", qPrintable(sql), errmsg);
                sqlite3_free(errmsg);
            }
        }
    }
}

/*! Sqlite callback to load data for a group.

    Groups will only be added to cache if not already known.
    Known groups will not be overwritten.
 */
static int sqliteLoadAllGroupsCallback(void *user, int ncols, char **colval , char **colname)
{
    DBG_Assert(user != 0);

    if (!user || (ncols <= 0))
    {
        return 0;
    }

    bool ok;
    Group group;
    DeRestPluginPrivate *d = static_cast<DeRestPluginPrivate*>(user);

    for (int i = 0; i < ncols; i++)
    {
        if (colval[i] && (colval[i][0] != '\0'))
        {
            QString val = QString::fromUtf8(colval[i]);

            DBG_Printf(DBG_INFO_L2, "Sqlite group: %s = %s\n", colname[i], qPrintable(val));


            if (strcmp(colname[i], "gid") == 0)
            {
                group.setAddress(val.toUInt(&ok, 16));

                if (!ok)
                {
                    DBG_Printf(DBG_INFO, "Error group in DB has no valid id: %s\n", colval[i]);
                    return 0;
                }
            }
            else if (strcmp(colname[i], "name") == 0)
            {
                group.setName(val);
            }
            else if (strcmp(colname[i], "state") == 0)
            {
                if (val == QLatin1String("deleted"))
                {
                    group.setState(Group::StateDeleted);
                }
            }
            else if (strcmp(colname[i], "mids") == 0)
            {
                group.setMidsFromString(val);
            }
            else if (strcmp(colname[i], "lightsequence") == 0)
            {
                group.setLightsequenceFromString(val);
            }
            else if (strcmp(colname[i], "devicemembership") == 0)
            {
                group.setDmFromString(val);
            }
            else if (strcmp(colname[i], "hidden") == 0)
            {
                bool hidden = (val == QLatin1String("true")) ? true : false;
                group.hidden = hidden;
            }
            else if (strcmp(colname[i], "type") == 0)
            {
                ResourceItem *item = group.item(RAttrType);
                if (item && !val.isEmpty())
                {
                    item->setValue(val);
                }
            }
            else if (strcmp(colname[i], "class") == 0)
            {
                ResourceItem *item = group.item(RAttrClass);
                if (item && !val.isEmpty())
                {
                    item->setValue(val);
                }
            }
        }
    }

    if (!group.id().isEmpty() && !group.name().isEmpty())
    {
        DBG_Printf(DBG_INFO_L2, "DB found group %s 0x%04X\n", qPrintable(group.name()), group.address());
        // check doubles
        Group *g = d->getGroupForId(group.id());
        if (!g)
        {
            // append to cache if not already known
            d->updateEtag(group.etag);
            d->groups.push_back(group);
        }
    }

    return 0;
}

/*! Loads all groups from database.
 */
void DeRestPluginPrivate::loadAllGroupsFromDb()
{
    int rc;
    char *errmsg = 0;

    DBG_Assert(db != 0);

    if (!db)
    {
        return;
    }

    QString sql = QString("SELECT * FROM groups");

    DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
    rc = sqlite3_exec(db, qPrintable(sql), sqliteLoadAllGroupsCallback, this, &errmsg);

    if (rc != SQLITE_OK)
    {
        if (errmsg)
        {
            DBG_Printf(DBG_ERROR_L2, "sqlite3_exec %s, error: %s\n", qPrintable(sql), errmsg);
            sqlite3_free(errmsg);
        }
    }
}

/*! Sqlite callback to load data for all resourcelinks.

    Resourcelinks will only be added to cache if not already known.
 */
static int sqliteLoadAllResourcelinksCallback(void *user, int ncols, char **colval , char **colname)
{
    DBG_Assert(user != 0);

    if (!user || (ncols <= 0))
    {
        return 0;
    }

    Resourcelinks rl;
    DeRestPluginPrivate *d = static_cast<DeRestPluginPrivate*>(user);

    for (int i = 0; i < ncols; i++)
    {
        if (colval[i] && (colval[i][0] != '\0'))
        {
            QString val = QString::fromUtf8(colval[i]);

            DBG_Printf(DBG_INFO_L2, "Sqlite schedule: %s = %s\n", colname[i], qPrintable(val));


            if (strcmp(colname[i], "id") == 0)
            {
                rl.id = val;

                if (rl.id.isEmpty())
                {
                    DBG_Printf(DBG_INFO, "Error resourcelink in DB has no valid id: %s\n", colval[i]);
                    return 0;
                }
            }
            else if (strcmp(colname[i], "json") == 0)
            {
                bool ok;
                rl.data = Json::parse(val, ok).toMap();

                if (!ok)
                {
                    DBG_Printf(DBG_INFO, "Error resourcelink in DB has no valid json string: %s\n", colval[i]);
                    return 0;
                }
            }
        }
    }

    for (const Resourcelinks &r : d->resourcelinks)
    {
        if (r.id == rl.id)
        {
            // already exist in cache
            return 0;
        }
    }

    d->resourcelinks.push_back(rl);

    return 0;
}

/*! Loads all resourcelinks from database.
 */
void DeRestPluginPrivate::loadAllResourcelinksFromDb()
{
    int rc;
    char *errmsg = 0;

    DBG_Assert(db != 0);

    if (!db)
    {
        return;
    }

    QString sql = QString("SELECT * FROM resourcelinks");

    DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
    rc = sqlite3_exec(db, qPrintable(sql), sqliteLoadAllResourcelinksCallback, this, &errmsg);

    if (rc != SQLITE_OK)
    {
        if (errmsg)
        {
            DBG_Printf(DBG_ERROR_L2, "sqlite3_exec %s, error: %s\n", qPrintable(sql), errmsg);
            sqlite3_free(errmsg);
        }
    }
}

/*! Sqlite callback to load data for a scene.

    Scenes will only be added to cache if not already known.
    Known scenes will not be overwritten.
 */
static int sqliteLoadAllScenesCallback(void *user, int ncols, char **colval , char **colname)
{
    DBG_Assert(user != 0);

    if (!user || (ncols <= 0))
    {
        return 0;
    }

    bool ok;
    bool ok1;
    bool ok2;
    Scene scene;
    DeRestPluginPrivate *d = static_cast<DeRestPluginPrivate*>(user);

    for (int i = 0; i < ncols; i++)
    {
        if (colval[i] && (colval[i][0] != '\0'))
        {
            QString val = QString::fromUtf8(colval[i]);

            DBG_Printf(DBG_INFO_L2, "Sqlite scene: %s = %s\n", colname[i], qPrintable(val));

            if (strcmp(colname[i], "gid") == 0)
            {
                scene.groupAddress = val.toUInt(&ok1, 16);
            }
            else if (strcmp(colname[i], "sid") == 0)
            {
                scene.id = val.toUInt(&ok2, 16);
            }
            else if (strcmp(colname[i], "name") == 0)
            {
                scene.name = val;
            }
            else if (strcmp(colname[i], "transitiontime") == 0)
            {
                scene.setTransitiontime(val.toUInt(&ok));
            }
            else if (strcmp(colname[i], "lights") == 0)
            {
                scene.setLights(Scene::jsonToLights(val));
            }
        }
    }

    if (ok1 && ok2)
    {
        DBG_Printf(DBG_INFO_L2, "DB found scene sid: 0x%02X, gid: 0x%04X\n", scene.id, scene.groupAddress);

        Group *g = d->getGroupForId(scene.groupAddress);
        if (g)
        {
            // append scene to group if not already known
            Scene *s = d->getSceneForId(scene.groupAddress,scene.id);
            if (!s)
            {
                // append scene to group if not already known
                d->updateEtag(g->etag);
                g->scenes.push_back(scene);
            }
        }
    }

    return 0;
}

/*! Loads all scenes from database.
 */
void DeRestPluginPrivate::loadAllScenesFromDb()
{
    int rc;
    char *errmsg = 0;

    DBG_Assert(db != 0);

    if (!db)
    {
        return;
    }

    QString sql = QString("SELECT * FROM scenes");

    DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
    rc = sqlite3_exec(db, qPrintable(sql), sqliteLoadAllScenesCallback, this, &errmsg);

    if (rc != SQLITE_OK)
    {
        if (errmsg)
        {
            DBG_Printf(DBG_ERROR_L2, "sqlite3_exec %s, error: %s\n", qPrintable(sql), errmsg);
            sqlite3_free(errmsg);
        }
    }
}

/*! Sqlite callback to load data for a schedule.
 */
static int sqliteLoadAllSchedulesCallback(void *user, int ncols, char **colval , char **colname)
{
    DBG_Assert(user != 0);

    if (!user || (ncols <= 0))
    {
        return 0;
    }

    Schedule schedule;
    DeRestPluginPrivate *d = static_cast<DeRestPluginPrivate*>(user);

    for (int i = 0; i < ncols; i++)
    {
        if (colval[i] && (colval[i][0] != '\0'))
        {
            QString val = QString::fromUtf8(colval[i]);

            DBG_Printf(DBG_INFO_L2, "Sqlite schedule: %s = %s\n", colname[i], qPrintable(val));


            if (strcmp(colname[i], "id") == 0)
            {
                schedule.id = val;

                if (schedule.id.isEmpty())
                {
                    DBG_Printf(DBG_INFO, "Error schedule in DB has no valid id: %s\n", colval[i]);
                    return 0;
                }
            }
            else if (strcmp(colname[i], "json") == 0)
            {
                schedule.jsonString = val;

                if (schedule.jsonString.isEmpty())
                {
                    DBG_Printf(DBG_INFO, "Error schedule in DB has no valid json string: %s\n", colval[i]);
                    return 0;
                }
            }
        }
    }

    std::vector<Schedule>::const_iterator i = d->schedules.begin();
    std::vector<Schedule>::const_iterator end = d->schedules.end();

    for (;i != end; ++i)
    {
        if (i->id == schedule.id)
        {
            // already exist in cache
            return 0;
        }
    }

    if (d->jsonToSchedule(schedule.jsonString, schedule, NULL))
    {
        DBG_Printf(DBG_INFO, "DB parsed schedule %s\n", qPrintable(schedule.id));
        d->schedules.push_back(schedule);
    }

    return 0;
}

/*! Loads all schedules from database.
 */
void DeRestPluginPrivate::loadAllSchedulesFromDb()
{
    int rc;
    char *errmsg = 0;

    DBG_Assert(db != 0);

    if (!db)
    {
        return;
    }

    QString sql = QString("SELECT * FROM schedules");

    DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
    rc = sqlite3_exec(db, qPrintable(sql), sqliteLoadAllSchedulesCallback, this, &errmsg);

    if (rc != SQLITE_OK)
    {
        if (errmsg)
        {
            DBG_Printf(DBG_ERROR_L2, "sqlite3_exec %s, error: %s\n", qPrintable(sql), errmsg);
            sqlite3_free(errmsg);
        }
    }
}

/*! Sqlite callback to load data for a node (identified by its mac address).
 */
static int sqliteLoadLightNodeCallback(void *user, int ncols, char **colval , char **colname)
{
    DBG_Assert(user != 0);

    if (!user || (ncols <= 0))
    {
        return 0;
    }

    LightNode *lightNode = static_cast<LightNode*>(user);

    QString id;
    QString name;
    QStringList groupIds;

    for (int i = 0; i < ncols; i++)
    {
        if (colval[i] && (colval[i][0] != '\0'))
        {
            QString val = QString::fromUtf8(colval[i]);

            if (strcmp(colname[i], "mac") == 0)
            {
                if (val != lightNode->uniqueId())
                {
                    // force update and cleanup of light node db entry
                    lightNode->setNeedSaveDatabase(true);
                }
            }
            else if (strcmp(colname[i], "endpoint") == 0)
            {
                bool ok;
                uint endpoint = val.toUInt(&ok);
                if (ok && endpoint > 0 && endpoint < 255)
                {
                    if (lightNode->haEndpoint().endpoint() != endpoint)
                    {
                        return 0; // not the node
                    }
                }
            }
            else if (strcmp(colname[i], "name") == 0)
            {
                name = val;
            }
            else if (strcmp(colname[i], "modelid") == 0)
            {
                if (!val.isEmpty() && 0 != val.compare(QLatin1String("Unknown"), Qt::CaseInsensitive))
                {
                    lightNode->setModelId(val);
                    lightNode->item(RAttrModelId)->setValue(val);
                    lightNode->clearRead(READ_MODEL_ID);
                }
            }
            else if (strcmp(colname[i], "manufacturername") == 0)
            {
                if (!val.isEmpty() && 0 != val.compare(QLatin1String("Unknown"), Qt::CaseInsensitive))
                {
                    lightNode->setManufacturerName(val);
                    lightNode->clearRead(READ_VENDOR_NAME);
                }
            }
            else if (strcmp(colname[i], "swbuildid") == 0)
            {
                if (!val.isEmpty() && 0 != val.compare(QLatin1String("Unknown"), Qt::CaseInsensitive))
                {
                    lightNode->setSwBuildId(val);
                    lightNode->clearRead(READ_SWBUILD_ID);
                }
            }
            else if (strcmp(colname[i], "id") == 0)
            {
                id = val;
            }
            else if (strcmp(colname[i], "groups") == 0)
            {
                groupIds = val.split(",");
            }
            else if (strcmp(colname[i], "state") == 0)
            {
                if (val == "deleted")
                {
                    lightNode->setState(LightNode::StateDeleted);
                }
                else
                {
                    lightNode->setState(LightNode::StateNormal);
                }
            }
        }
    }

    if (!id.isEmpty())
    {
        lightNode->setId(id);
    }

    if (!name.isEmpty())
    {
        lightNode->setName(name);
    }

    QStringList::const_iterator gi = groupIds.begin();
    QStringList::const_iterator gend = groupIds.end();

    for (; gi != gend; ++gi)
    {
        bool ok;
        quint16 gid = gi->toUShort(&ok);

        if (!ok)
        {
            continue;
        }

        // already known?
        std::vector<GroupInfo>::const_iterator k = lightNode->groups().begin();
        std::vector<GroupInfo>::const_iterator kend = lightNode->groups().end();

        for (; k != kend; ++k)
        {
            if (k->id == gid)
            {
                ok = false;
                break;
            }
        }

        if (ok)
        {
            GroupInfo groupInfo;
            groupInfo.id = gid;
            groupInfo.state = GroupInfo::StateInGroup;
            lightNode->groups().push_back(groupInfo);
        }
    }

    return 0;
}

/*! Loads data (if available) for a LightNode from the database.
 */
void DeRestPluginPrivate::loadLightNodeFromDb(LightNode *lightNode)
{
    int rc;
    char *errmsg = 0;

    DBG_Assert(db != 0);
    DBG_Assert(lightNode != 0);

    if (!db || !lightNode)
    {
        return;
    }

    // check for new uniqueId format
    QString sql = QString("SELECT * FROM nodes WHERE mac='%1' COLLATE NOCASE").arg(lightNode->uniqueId());

    DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
    rc = sqlite3_exec(db, qPrintable(sql), sqliteLoadLightNodeCallback, lightNode, &errmsg);

    if (rc != SQLITE_OK)
    {
        if (errmsg)
        {
            DBG_Printf(DBG_ERROR_L2, "sqlite3_exec %s, error: %s\n", qPrintable(sql), errmsg);
            sqlite3_free(errmsg);
        }
    }

    if (!lightNode->swBuildId().isEmpty())
    {
        lightNode->setLastRead(READ_SWBUILD_ID, idleTotalCounter);
    }

    if (!lightNode->modelId().isEmpty())
    {
        lightNode->setLastRead(READ_MODEL_ID, idleTotalCounter);
    }

    // check for old mac address only format
    if (lightNode->id().isEmpty())
    {
        sql = QString("SELECT * FROM nodes WHERE mac='%1' COLLATE NOCASE").arg(lightNode->address().toStringExt());

        DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
        rc = sqlite3_exec(db, qPrintable(sql), sqliteLoadLightNodeCallback, lightNode, &errmsg);

        if (rc != SQLITE_OK)
        {
            if (errmsg)
            {
                DBG_Printf(DBG_ERROR_L2, "sqlite3_exec %s, error: %s\n", qPrintable(sql), errmsg);
                sqlite3_free(errmsg);
            }
        }

        if (!lightNode->id().isEmpty())
        {
            lightNode->setNeedSaveDatabase(true);
        }
    }

    if (lightNode->needSaveDatabase())
    {
        queSaveDb(DB_LIGHTS, DB_SHORT_SAVE_DELAY);
    }

    // check for unique IDs
    if (!lightNode->id().isEmpty())
    {
        std::vector<LightNode>::iterator i = nodes.begin();
        std::vector<LightNode>::iterator end = nodes.end();

        for (; i != end; ++i)
        {
            if (&(*i) != lightNode)
            {
                // id already set to another node
                // empty it here so a new one will be generated
                if (i->id() == lightNode->id())
                {
                    DBG_Printf(DBG_INFO, "detected already used id %s, force generate new id\n", qPrintable(i->id()));
                    lightNode->setId("");
                    queSaveDb(DB_LIGHTS, DB_LONG_SAVE_DELAY);
                }
            }
        }
    }
}

/*! Sqlite callback to load data for a group (identified by its group id).
 */
static int sqliteLoadGroupCallback(void *user, int ncols, char **colval , char **colname)
{
    DBG_Assert(user != 0);

    if (!user || (ncols <= 0))
    {
        return 0;
    }

    Group *group = static_cast<Group*>(user);

    for (int i = 0; i < ncols; i++)
    {
        if (colval[i] && (colval[i][0] != '\0'))
        {
            QString val = QString::fromUtf8(colval[i]);

            if (strcmp(colname[i], "name") == 0)
            {
                group->setName(val);
            }
            else if (strcmp(colname[i], "state") == 0)
            {
                if (val == "deleted")
                {
                    group->setState(Group::StateDeleted);
                }
            }
        }
    }

    return 0;
}

/*! Loads data (if available) for a Group from the database.
 */
void DeRestPluginPrivate::loadGroupFromDb(Group *group)
{
    int rc;
    char *errmsg = 0;

    DBG_Assert(db != 0);
    DBG_Assert(group != 0);

    if (!db || !group)
    {
        return;
    }

    QString gid;
    gid.sprintf("0x%04X", group->address());

    QString sql = QString("SELECT * FROM groups WHERE gid='%1'").arg(gid);

    DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
    rc = sqlite3_exec(db, qPrintable(sql), sqliteLoadGroupCallback, group, &errmsg);

    if (rc != SQLITE_OK)
    {
        if (errmsg)
        {
            DBG_Printf(DBG_ERROR_L2, "sqlite3_exec %s, error: %s\n", qPrintable(sql), errmsg);
            sqlite3_free(errmsg);
        }
    }
}

/*! Sqlite callback to load data for a scene (identified by its scene id).
 */
static int sqliteLoadSceneCallback(void *user, int ncols, char **colval , char **colname)
{
    DBG_Assert(user != 0);

    if (!user || (ncols <= 0))
    {
        return 0;
    }

    Scene *scene = static_cast<Scene*>(user);

    for (int i = 0; i < ncols; i++) {
        if (colval[i] && (colval[i][0] != '\0'))
        {
            if (strcmp(colname[i], "name") == 0)
            {
                scene->name = QString::fromUtf8(colval[i]);
            }
            if (strcmp(colname[i], "transitiontime") == 0)
            {
                QString tt = QString::fromUtf8(colval[i]);
                scene->setTransitiontime(tt.toUInt());
            }
            if (strcmp(colname[i], "lights") == 0)
            {
                scene->setLights(Scene::jsonToLights(colval[i]));
            }
        }
    }

    return 0;
}

/*! Loads data (if available) for a Scene from the database.
 */
void DeRestPluginPrivate::loadSceneFromDb(Scene *scene)
{
    int rc;
    char *errmsg = 0;

    DBG_Assert(db != 0);
    DBG_Assert(scene != 0);

    if (!db || !scene)
    {
        return;
    }

    QString gsid; // unique key
    gsid.sprintf("0x%04X%02X", scene->groupAddress, scene->id);

    QString sql = QString("SELECT * FROM scenes WHERE gsid='%1'").arg(gsid);

    DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
    rc = sqlite3_exec(db, qPrintable(sql), sqliteLoadSceneCallback, scene, &errmsg);

    if (rc != SQLITE_OK)
    {
        if (errmsg)
        {
            DBG_Printf(DBG_ERROR_L2, "sqlite3_exec %s, error: %s\n", qPrintable(sql), errmsg);
            sqlite3_free(errmsg);
        }
    }
}

/*! Sqlite callback to load data for a rule.

    Rule will only be added to cache if not already known.
 */
static int sqliteLoadAllRulesCallback(void *user, int ncols, char **colval , char **colname)
{
    DBG_Assert(user != 0);

    if (!user || (ncols <= 0))
    {
        return 0;
    }

    Rule rule;
    DeRestPluginPrivate *d = static_cast<DeRestPluginPrivate*>(user);

    for (int i = 0; i < ncols; i++)
    {
        if (colval[i] && (colval[i][0] != '\0'))
        {
            QString val = QString::fromUtf8(colval[i]);

            DBG_Printf(DBG_INFO_L2, "Sqlite rules: %s = %s\n", colname[i], qPrintable(val));


            if (strcmp(colname[i], "rid") == 0)
            {
                rule.setId(val);
            }
            else if (strcmp(colname[i], "name") == 0)
            {
                rule.setName(val);
            }
            else if (strcmp(colname[i], "created") == 0)
            {
                rule.setCreationtime(val);
            }
            else if (strcmp(colname[i], "etag") == 0)
            {
                rule.etag = val;
            }
            else if (strcmp(colname[i], "owner") == 0)
            {
                rule.setOwner(val);
            }
            else if (strcmp(colname[i], "status") == 0)
            {
                rule.setStatus(val);
            }
            else if (strcmp(colname[i], "timestriggered") == 0)
            {
                rule.setTimesTriggered(val.toUInt());
            }
            else if (strcmp(colname[i], "actions") == 0)
            {
                rule.setActions(Rule::jsonToActions(val));
            }
            else if (strcmp(colname[i], "conditions") == 0)
            {
                rule.setConditions(Rule::jsonToConditions(val));
            }
            else if (strcmp(colname[i], "periodic") == 0)
            {
                bool ok;
                int periodic = val.toUInt(&ok);
                if (ok)
                {
                    rule.setTriggerPeriodic(periodic);
                }
            }
        }
    }

    if (!rule.id().isEmpty() && !rule.name().isEmpty())
    {
        DBG_Printf(DBG_INFO_L2, "DB found rule %s %s\n", qPrintable(rule.name()), qPrintable(rule.id()));
        // check doubles
        Rule *r = d->getRuleForId(rule.id());
        if (!r)
        {
            // append to cache if not already known
            d->updateEtag(rule.etag);
            d->rules.push_back(rule);
        }
    }

    return 0;
}

/*! Loads all rules from database
 */
void DeRestPluginPrivate::loadAllRulesFromDb()
{
    int rc;
    char *errmsg = 0;

    DBG_Assert(db != 0);

    if (!db)
    {
        return;
    }

    QString sql = QString("SELECT * FROM rules");

    DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
    rc = sqlite3_exec(db, qPrintable(sql), sqliteLoadAllRulesCallback, this, &errmsg);

    if (rc != SQLITE_OK)
    {
        if (errmsg)
        {
            DBG_Printf(DBG_ERROR_L2, "sqlite3_exec %s, error: %s\n", qPrintable(sql), errmsg);
            sqlite3_free(errmsg);
        }
    }
}

/*! Sqlite callback to load data for a sensor.

    Sensor will only be added to cache if not already known.
 */
static int sqliteLoadAllSensorsCallback(void *user, int ncols, char **colval , char **colname)
{
    DBG_Assert(user != 0);

    if (!user || (ncols <= 0))
    {
        return 0;
    }

    Sensor sensor;
    DeRestPluginPrivate *d = static_cast<DeRestPluginPrivate*>(user);

    int configCol = -1;
    int stateCol = -1;

    for (int i = 0; i < ncols; i++)
    {
        if (colval[i] && (colval[i][0] != '\0'))
        {
            QString val = QString::fromUtf8(colval[i]);

            DBG_Printf(DBG_INFO_L2, "Sqlite sensors: %s = %s\n", colname[i], qPrintable(val));

            if (strcmp(colname[i], "sid") == 0)
            {
                sensor.setId(val);
            }
            else if (strcmp(colname[i], "name") == 0)
            {
                sensor.setName(val);
            }
            else if (strcmp(colname[i], "type") == 0)
            {
                if (val == QLatin1String("ZHALight"))
                {
                    val = QLatin1String("ZHALightLevel");
                    sensor.setNeedSaveDatabase(true);
                }
                sensor.setType(val);
            }
            else if (strcmp(colname[i], "modelid") == 0)
            {
                sensor.setModelId(val.simplified());
            }
            else if (strcmp(colname[i], "mode") == 0)
            {
                sensor.setMode((Sensor::SensorMode)val.toUInt());
            }
            else if (strcmp(colname[i], "etag") == 0)
            {
                sensor.etag = val;
            }
            else if (strcmp(colname[i], "manufacturername") == 0)
            {
                sensor.setManufacturer(val.simplified());
            }
            else if (strcmp(colname[i], "uniqueid") == 0)
            {
                sensor.setUniqueId(val);
            }
            else if (strcmp(colname[i], "swversion") == 0)
            {
                sensor.setSwVersion(val.simplified());
            }
            else if (strcmp(colname[i], "state") == 0)
            {
                stateCol = i;
            }
            else if (strcmp(colname[i], "config") == 0)
            {
                configCol = i;
            }
            else if (strcmp(colname[i], "fingerprint") == 0)
            {
                SensorFingerprint fp;
                if (fp.readFromJsonString(val))
                {
                    sensor.fingerPrint() = fp;
                }
            }
            else if (strcmp(colname[i], "deletedState") == 0)
            {
                if (val == "deleted")
                {
                    sensor.setDeletedState(Sensor::StateDeleted);
                    return 0;
                }
                else
                {
                    sensor.setDeletedState(Sensor::StateNormal);
                }
            }
        }
    }

    if (!sensor.id().isEmpty() && !sensor.name().isEmpty() && !sensor.type().isEmpty())
    {
        bool ok;
        bool isClip = sensor.type().startsWith(QLatin1String("CLIP"));
        ResourceItem *item = 0;
        quint64 extAddr = 0;
        quint16 clusterId = 0;
        quint8 endpoint = sensor.fingerPrint().endpoint;
        DBG_Printf(DBG_INFO_L2, "DB found sensor %s %s\n", qPrintable(sensor.name()), qPrintable(sensor.id()));

        if (!isClip && sensor.type() == QLatin1String("Daylight"))
        {
            isClip = true;
        }

        if (isClip)
        {
            ok = true;
        }
        // convert from old format 0x0011223344556677 to 00:11:22:33:44:55:66:77-AB where AB is the endpoint
        else if (sensor.uniqueId().startsWith(QLatin1String("0x")))
        {
            extAddr = sensor.uniqueId().toULongLong(&ok, 16);
        }
        else
        {
            QString mac = sensor.uniqueId(); // need copy
            mac = mac.remove(':').split('-').first();
            extAddr = mac.toULongLong(&ok, 16);
        }

        if (!isClip && extAddr == 0)
        {
            return 0;
        }

        if (sensor.type().endsWith(QLatin1String("Switch")))
        {
            if (sensor.modelId() == QLatin1String("SML001")) // Hue motion sensor
            {
                // not supported yet, created by older versions
                // ignore for now
                return 0;
            }

            if (sensor.fingerPrint().hasInCluster(COMMISSIONING_CLUSTER_ID))
            {
                clusterId = COMMISSIONING_CLUSTER_ID;
            }

            if (sensor.fingerPrint().hasOutCluster(ONOFF_CLUSTER_ID))
            {
                clusterId = clusterId ? clusterId : ONOFF_CLUSTER_ID;
                sensor.addItem(DataTypeString, RConfigGroup);
            }
            else if (sensor.fingerPrint().hasInCluster(ONOFF_CLUSTER_ID))
            {
                clusterId = clusterId ? clusterId : ONOFF_CLUSTER_ID;
            }
            else if (sensor.fingerPrint().hasInCluster(ANALOG_INPUT_CLUSTER_ID))
            {
                clusterId = clusterId ? clusterId : ANALOG_INPUT_CLUSTER_ID;
            }
            else if (sensor.fingerPrint().hasInCluster(MULTISTATE_INPUT_CLUSTER_ID))
            {
                clusterId = clusterId ? clusterId : MULTISTATE_INPUT_CLUSTER_ID;
            }
            item = sensor.addItem(DataTypeInt32, RStateButtonEvent);
            item->setValue(0);
        }
        else if (sensor.type().endsWith(QLatin1String("LightLevel")))
        {
            if (sensor.fingerPrint().hasInCluster(ILLUMINANCE_MEASUREMENT_CLUSTER_ID))
            {
                clusterId = ILLUMINANCE_MEASUREMENT_CLUSTER_ID;
            }
            item = sensor.addItem(DataTypeUInt16, RStateLightLevel);
            item->setValue(0);
            item = sensor.addItem(DataTypeUInt32, RStateLux);
            item->setValue(0);
            item = sensor.addItem(DataTypeBool, RStateDark);
            item->setValue(true);
            item->setTimeStamps(QDateTime::currentDateTime().addSecs(-120));
            item = sensor.addItem(DataTypeBool, RStateDaylight);
            item->setValue(false);
            item = sensor.addItem(DataTypeUInt16, RConfigTholdDark);
            item->setValue(R_THOLDDARK_DEFAULT);
            item = sensor.addItem(DataTypeUInt16, RConfigTholdOffset);
            item->setValue(R_THOLDOFFSET_DEFAULT);
        }
        else if (sensor.type().endsWith(QLatin1String("Temperature")))
        {
            if (sensor.fingerPrint().hasInCluster(TEMPERATURE_MEASUREMENT_CLUSTER_ID))
            {
                clusterId = TEMPERATURE_MEASUREMENT_CLUSTER_ID;
            }
            item = sensor.addItem(DataTypeInt16, RStateTemperature);
            item->setValue(0);
            item = sensor.addItem(DataTypeInt16, RConfigOffset);
            item->setValue(0);
        }
        else if (sensor.type().endsWith(QLatin1String("Humidity")))
        {
            if (sensor.fingerPrint().hasInCluster(RELATIVE_HUMIDITY_CLUSTER_ID))
            {
                clusterId = RELATIVE_HUMIDITY_CLUSTER_ID;
            }
            item = sensor.addItem(DataTypeUInt16, RStateHumidity);
            item->setValue(0);
        }
        else if (sensor.type().endsWith(QLatin1String("Pressure")))
        {
            if (sensor.fingerPrint().hasInCluster(PRESSURE_MEASUREMENT_CLUSTER_ID))
            {
                clusterId = PRESSURE_MEASUREMENT_CLUSTER_ID;
            }
            item = sensor.addItem(DataTypeInt16, RStatePressure);
            item->setValue(0);
        }
        else if (sensor.type().endsWith(QLatin1String("Presence")))
        {
            if (sensor.fingerPrint().hasInCluster(OCCUPANCY_SENSING_CLUSTER_ID))
            {
                clusterId = OCCUPANCY_SENSING_CLUSTER_ID;
                if (sensor.modelId().startsWith(QLatin1String("FLS")) ||
                    sensor.modelId().startsWith(QLatin1String("SML001")))
                {
                    // TODO write and recover min/max to db
                    deCONZ::NumericUnion dummy;
                    dummy.u64 = 0;
                    sensor.setZclValue(NodeValue::UpdateInvalid, clusterId, 0x0000, dummy);
                    NodeValue &val = sensor.getZclValue(clusterId, 0x0000);
                    val.minInterval = 1;     // value used by Hue bridge
                    val.maxInterval = 300;   // value used by Hue bridge

                    sensor.setNextReadTime(READ_OCCUPANCY_CONFIG, QTime::currentTime());
                    sensor.enableRead(READ_OCCUPANCY_CONFIG);
                    sensor.setLastRead(READ_OCCUPANCY_CONFIG, 0);
                }
            }
            else if (sensor.fingerPrint().hasInCluster(IAS_ZONE_CLUSTER_ID))
            {
                clusterId = IAS_ZONE_CLUSTER_ID;
            }
            else if (sensor.fingerPrint().hasInCluster(COMMISSIONING_CLUSTER_ID))
            {
                clusterId = COMMISSIONING_CLUSTER_ID;
            }
            item = sensor.addItem(DataTypeBool, RStatePresence);
            item->setValue(false);
            if (sensor.modelId() == QLatin1String("SML001")) // Hue motion sensor
            {
                item = sensor.addItem(DataTypeUInt16, RConfigDelay);
                item->setValue(0);
                item = sensor.addItem(DataTypeUInt8, RConfigSensitivity);
                item->setValue(0);
                item = sensor.addItem(DataTypeUInt8, RConfigSensitivityMax);
                item->setValue(R_SENSITIVITY_MAX_DEFAULT);
            }
            else
            {
                item = sensor.addItem(DataTypeUInt16, RConfigDuration);
                item->setValue(60); // presence should be reasonable for physical sensors
            }
        }
        else if (sensor.type().endsWith(QLatin1String("Flag")))
        {
            item = sensor.addItem(DataTypeBool, RStateFlag);
            item->setValue(false);
        }
        else if (sensor.type().endsWith(QLatin1String("Status")))
        {
            item = sensor.addItem(DataTypeInt32, RStateStatus);
            item->setValue(0);
        }
        else if (sensor.type().endsWith(QLatin1String("OpenClose")))
        {
            if (sensor.fingerPrint().hasInCluster(IAS_ZONE_CLUSTER_ID))
            {
                clusterId = IAS_ZONE_CLUSTER_ID;
            }
            else if (sensor.fingerPrint().hasInCluster(ONOFF_CLUSTER_ID))
            {
                clusterId = ONOFF_CLUSTER_ID;
            }
            item = sensor.addItem(DataTypeBool, RStateOpen);
            item->setValue(false);
        }
        else if (sensor.type().endsWith(QLatin1String("Alarm")))
        {
            if (sensor.fingerPrint().hasInCluster(IAS_ZONE_CLUSTER_ID))
            {
                clusterId = IAS_ZONE_CLUSTER_ID;
            }
            item = sensor.addItem(DataTypeBool, RStateAlarm);
            item->setValue(false);
        }
        else if (sensor.type().endsWith(QLatin1String("CarbonMonoxide")))
        {
            if (sensor.fingerPrint().hasInCluster(IAS_ZONE_CLUSTER_ID))
            {
                clusterId = IAS_ZONE_CLUSTER_ID;
            }
            item = sensor.addItem(DataTypeBool, RStateCarbonMonoxide);
            item->setValue(false);
        }
        else if (sensor.type().endsWith(QLatin1String("Fire")))
        {
            if (sensor.fingerPrint().hasInCluster(IAS_ZONE_CLUSTER_ID))
            {
                clusterId = IAS_ZONE_CLUSTER_ID;
            }
            item = sensor.addItem(DataTypeBool, RStateFire);
            item->setValue(false);
        }
        else if (sensor.type().endsWith(QLatin1String("Water")))
        {
            if (sensor.fingerPrint().hasInCluster(IAS_ZONE_CLUSTER_ID))
            {
                clusterId = IAS_ZONE_CLUSTER_ID;
            }
            item = sensor.addItem(DataTypeBool, RStateWater);
            item->setValue(false);
        }
        else if (sensor.type().endsWith(QLatin1String("Consumption")))
        {
            if (sensor.fingerPrint().hasInCluster(METERING_CLUSTER_ID))
            {
                clusterId = METERING_CLUSTER_ID;
                item = sensor.addItem(DataTypeInt16, RStatePower);
                item->setValue(0);
            }
            else if (sensor.fingerPrint().hasInCluster(ANALOG_INPUT_CLUSTER_ID))
            {
                clusterId = ANALOG_INPUT_CLUSTER_ID;
            }
            item = sensor.addItem(DataTypeInt64, RStateConsumption);
            item->setValue(0);
        }
        else if (sensor.type().endsWith(QLatin1String("Power")))
        {
            bool hasVoltage = true;
            if (sensor.fingerPrint().hasInCluster(ELECTRICAL_MEASUREMENT_CLUSTER_ID))
            {
                clusterId = ELECTRICAL_MEASUREMENT_CLUSTER_ID;
                if (sensor.modelId().startsWith(QLatin1String("Plug"))) // OSRAM
                {
                    hasVoltage = false;
                }
            }
            else if (sensor.fingerPrint().hasInCluster(ANALOG_INPUT_CLUSTER_ID))
            {
                clusterId = ANALOG_INPUT_CLUSTER_ID;
                hasVoltage = false;
            }
            item = sensor.addItem(DataTypeInt16, RStatePower);
            item->setValue(0);
            if (hasVoltage)
            {
                item = sensor.addItem(DataTypeUInt16, RStateVoltage);
                item->setValue(0);
                item = sensor.addItem(DataTypeUInt16, RStateCurrent);
                item->setValue(0);
            }
        }
        else if (sensor.type() == QLatin1String("Daylight"))
        {
            d->daylightSensorId = sensor.id();
            sensor.removeItem(RConfigReachable);
            sensor.addItem(DataTypeBool, RConfigConfigured);
            item = sensor.addItem(DataTypeInt8, RConfigSunriseOffset);
            item->setValue(30);
            item =sensor.addItem(DataTypeInt8, RConfigSunsetOffset);
            item->setValue(-30);
            sensor.addItem(DataTypeString, RConfigLat);
            sensor.addItem(DataTypeString, RConfigLong);
            sensor.addItem(DataTypeBool, RStateDaylight);
            sensor.addItem(DataTypeInt32, RStateStatus);
        }

        if (sensor.modelId().startsWith(QLatin1String("RWL02"))) // Hue dimmer switch
        {
            clusterId = VENDOR_CLUSTER_ID;
            endpoint = 2;

            if (!sensor.fingerPrint().hasInCluster(POWER_CONFIGURATION_CLUSTER_ID))
            {
                sensor.fingerPrint().inClusters.push_back(POWER_CONFIGURATION_CLUSTER_ID);
                sensor.setNeedSaveDatabase(true);
            }

            if (!sensor.fingerPrint().hasInCluster(VENDOR_CLUSTER_ID)) // for realtime button feedback
            {
                sensor.fingerPrint().inClusters.push_back(VENDOR_CLUSTER_ID);
                sensor.setNeedSaveDatabase(true);
            }
        }
        else if (sensor.modelId() == QLatin1String("SML001")) // Hue motion sensor
        {
            if (!sensor.fingerPrint().hasInCluster(BASIC_CLUSTER_ID))
            {
                sensor.fingerPrint().inClusters.push_back(BASIC_CLUSTER_ID);
                sensor.setNeedSaveDatabase(true);
            }
            if (!sensor.fingerPrint().hasInCluster(POWER_CONFIGURATION_CLUSTER_ID))
            {
                sensor.fingerPrint().inClusters.push_back(POWER_CONFIGURATION_CLUSTER_ID);
                sensor.setNeedSaveDatabase(true);
            }
            item = sensor.addItem(DataTypeString, RConfigAlert);
            item->setValue(R_ALERT_DEFAULT);
            item = sensor.addItem(DataTypeBool, RConfigLedIndication);
            item->setValue(false);
            item = sensor.addItem(DataTypeUInt8, RConfigPending);
            item->setValue(0);
            item = sensor.addItem(DataTypeBool, RConfigUsertest);
            item->setValue(false);
        }
        else if (sensor.modelId().startsWith(QLatin1String("TRADFRI")))
        {
            // support power configuration cluster for IKEA devices
            if (!sensor.fingerPrint().hasInCluster(POWER_CONFIGURATION_CLUSTER_ID))
            {
                sensor.fingerPrint().inClusters.push_back(POWER_CONFIGURATION_CLUSTER_ID);
            }

            item = sensor.addItem(DataTypeString, RConfigAlert);
            item->setValue(R_ALERT_DEFAULT);
        }
        else if (sensor.modelId().startsWith(QLatin1String("lumi.")))
        {
            item = sensor.addItem(DataTypeUInt8, RConfigBattery);
            item->setValue(100);

            if (!sensor.item(RStateTemperature) &&
                !sensor.modelId().contains(QLatin1String("weather")) &&
                !sensor.modelId().startsWith(QLatin1String("lumi.sensor_ht")))
            {
                item = sensor.addItem(DataTypeInt16, RConfigTemperature);
                item->setValue(0);
                //item = sensor.addItem(DataTypeInt16, RConfigOffset);
                //item->setValue(0);
            }
        }

        if (sensor.fingerPrint().hasInCluster(IAS_ZONE_CLUSTER_ID))
        {
            item = sensor.addItem(DataTypeBool, RStateLowBattery);
            item->setValue(false);
            item = sensor.addItem(DataTypeBool, RStateTampered);
            item->setValue(false);
        }

        if (sensor.fingerPrint().hasInCluster(POWER_CONFIGURATION_CLUSTER_ID))
        {
            item = sensor.addItem(DataTypeUInt8, RConfigBattery);
            item->setValue(100);
        }

        if (stateCol >= 0 &&
            sensor.type() != QLatin1String("CLIPGenericFlag") &&
            sensor.type() != QLatin1String("CLIPGenericStatus"))
        {
            sensor.jsonToState(QLatin1String(colval[stateCol]));
        }

        if (configCol >= 0)
        {
            sensor.jsonToConfig(QLatin1String(colval[configCol]));
        }

        if (extAddr != 0)
        {
            QString uid = d->generateUniqueId(extAddr, endpoint, clusterId);

            if (uid != sensor.uniqueId())
            {
                // update to new format
                sensor.setUniqueId(uid);
                sensor.setNeedSaveDatabase(true);
            }
        }

        // temp. workaround for default value of 'two groups' which is only supported by lighting switch
        if (sensor.mode() == Sensor::ModeTwoGroups)
        {
            if (sensor.modelId() != QLatin1String("Lighting Switch"))
            {
                sensor.setMode(Sensor::ModeScenes);
            }
        }

        // check doubles, split uid into mac address and endpoint

        if (ok)
        {
            Sensor *s = 0;

            if (!isClip)
            {
                s = d->getSensorNodeForFingerPrint(extAddr, sensor.fingerPrint(), sensor.type());
            }

            if (!s)
            {
                sensor.address().setExt(extAddr);
                // append to cache if not already known
                d->sensors.push_back(sensor);
                d->updateSensorEtag(&d->sensors.back());

                if (sensor.needSaveDatabase())
                {
                    d->queSaveDb(DB_SENSORS, DB_SHORT_SAVE_DELAY);
                }
            }
        }
    }

    return 0;
}

/*! Loads all sensors from database
 */
void DeRestPluginPrivate::loadAllSensorsFromDb()
{
    int rc;
    char *errmsg = 0;

    DBG_Assert(db != 0);

    if (!db)
    {
        return;
    }

    QString sql = QString("SELECT * FROM sensors");

    DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
    rc = sqlite3_exec(db, qPrintable(sql), sqliteLoadAllSensorsCallback, this, &errmsg);

    if (rc != SQLITE_OK)
    {
        if (errmsg)
        {
            DBG_Printf(DBG_ERROR_L2, "sqlite3_exec %s, error: %s\n", qPrintable(sql), errmsg);
            sqlite3_free(errmsg);
        }
    }
}

/*! Loads all gateways from database
 */
void DeRestPluginPrivate::loadAllGatewaysFromDb()
{
    int rc;
    char *errmsg = 0;

    DBG_Assert(db != 0);

    if (!db)
    {
        return;
    }

    QString sql(QLatin1String("SELECT * FROM gateways"));

    DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
    rc = sqlite3_exec(db, qPrintable(sql), sqliteLoadAllGatewaysCallback, this, &errmsg);

    if (rc != SQLITE_OK)
    {
        if (errmsg)
        {
            DBG_Printf(DBG_ERROR_L2, "sqlite3_exec %s, error: %s\n", qPrintable(sql), errmsg);
            sqlite3_free(errmsg);
        }
    }
}

/*! Sqlite callback to load all light ids into temporary array.
 */
static int sqliteGetAllLightIdsCallback(void *user, int ncols, char **colval , char **colname)
{
    DBG_Assert(user != 0);

    if (!user || (ncols <= 0))
    {
        return 0;
    }

    DeRestPluginPrivate *d = static_cast<DeRestPluginPrivate*>(user);

    for (int i = 0; i < ncols; i++)
    {
        if (colval[i] && (colval[i][0] != '\0'))
        {
            if (strcmp(colname[i], "id") == 0)
            {
                bool ok;
                int id = QString(colval[i]).toInt(&ok);

                if (ok)
                {
                    d->lightIds.push_back(id);
                }
            }
        }
    }

    return 0;
}

/*! Determines a unused id for a light.
 */
int DeRestPluginPrivate::getFreeLightId()
{
    int rc;
    char *errmsg = 0;

    DBG_Assert(db != 0);

    if (!db)
    {
        return 1;
    }

    lightIds.clear();

    { // append all ids from nodes known at runtime
        std::vector<LightNode>::const_iterator i = nodes.begin();
        std::vector<LightNode>::const_iterator end = nodes.end();
        for (;i != end; ++i)
        {
            lightIds.push_back(i->id().toUInt());
        }
    }

    // append all ids from database (dublicates are ok here)
    QString sql = QString("SELECT * FROM nodes");

    DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
    rc = sqlite3_exec(db, qPrintable(sql), sqliteGetAllLightIdsCallback, this, &errmsg);

    if (rc != SQLITE_OK)
    {
        if (errmsg)
        {
            DBG_Printf(DBG_ERROR_L2, "sqlite3_exec %s, error: %s\n", qPrintable(sql), errmsg);
            sqlite3_free(errmsg);
        }
    }

    int id = 1;
    while (1)
    {
        std::vector<int>::iterator result = std::find(lightIds.begin(), lightIds.end(), id);

        // id not known?
        if (result == lightIds.end())
        {
            return id;
        }
        id++;
    }

    return id;
}

/*! Sqlite callback to load all sensor ids into temporary array.
 */
static int sqliteGetAllSensorIdsCallback(void *user, int ncols, char **colval , char **colname)
{
    DBG_Assert(user != 0);

    if (!user || (ncols <= 0))
    {
        return 0;
    }

    DeRestPluginPrivate *d = static_cast<DeRestPluginPrivate*>(user);

    for (int i = 0; i < ncols; i++)
    {
        if (colval[i] && (colval[i][0] != '\0'))
        {
            if (strcmp(colname[i], "id") == 0)
            {
                bool ok;
                int id = QString(colval[i]).toInt(&ok);

                if (ok)
                {
                    d->sensorIds.push_back(id);
                }
            }
        }
    }

    return 0;
}

static int sqliteLoadAllGatewaysCallback(void *user, int ncols, char **colval , char **colname)
{
    DBG_Assert(user != 0);

    if (!user || (ncols <= 0))
    {
        return 0;
    }

    DeRestPluginPrivate *d = static_cast<DeRestPluginPrivate*>(user);

    int idxUuid = -1;
    int idxName = -1;
    int idxIp = -1;
    int idxPort = -1;
    int idxApikey = -1;
    int idxPairing = -1;
    int idxCgroups = -1;

    for (int i = 0; i < ncols; i++)
    {
        if (colval[i] && (colval[i][0] != '\0'))
        {
            if      (strcmp(colname[i], "uuid") == 0)    { idxUuid = i; }
            else if (strcmp(colname[i], "name") == 0)    { idxName = i; }
            else if (strcmp(colname[i], "ip") == 0)      { idxIp = i; }
            else if (strcmp(colname[i], "port") == 0)    { idxPort = i; }
            else if (strcmp(colname[i], "apikey") == 0)  { idxApikey = i; }
            else if (strcmp(colname[i], "pairing") == 0)  { idxPairing = i; }
            else if (strcmp(colname[i], "cgroups") == 0) { idxCgroups = i; }
        }
    }

    if (idxUuid == -1)
    {
        return 0; // required
    }

    Gateway *gw = new Gateway(d);

    gw->setUuid(colval[idxUuid]);
    if (idxName != -1) { gw->setName(colval[idxName]); }
    if (idxIp != -1) { gw->setAddress(QHostAddress(colval[idxIp])); }
    if (idxPort != -1) { gw->setPort(QString(colval[idxPort]).toUShort()); }
    if (idxApikey != -1) { gw->setApiKey(colval[idxApikey]); }
    if (idxPairing != -1) { gw->setPairingEnabled(colval[idxPairing][0] == '1'); }
    if (idxCgroups != -1 && colval[idxCgroups][0] == '[') // must be json array
    {
        bool ok;
        QVariant var = Json::parse(colval[idxCgroups], ok);

        if (ok && var.type() == QVariant::List)
        {
            QVariantList ls = var.toList();
            for (int i = 0; i < ls.size(); i++)
            {
                QVariantMap e = ls[i].toMap();
                if (e.contains(QLatin1String("lg")) && e.contains(QLatin1String("rg")))
                {
                    double lg = e[QLatin1String("lg")].toDouble();
                    double rg = e[QLatin1String("rg")].toDouble();

                    if (lg > 0 && lg <= 0xfffful && rg > 0 && rg <= 0xfffful)
                    {
                        gw->addCascadeGroup(lg, rg);
                    }
                }
            }
        }
    }
    gw->setNeedSaveDatabase(false);
    d->gateways.push_back(gw);

    return 0;
}

/*! Determines a unused id for a sensor.
 */
int DeRestPluginPrivate::getFreeSensorId()
{
    int rc;
    bool ok;
    char *errmsg = 0;

    DBG_Assert(db != 0);

    if (!db)
    {
        return 1;
    }

    sensorIds.clear();

    { // append all ids from nodes known at runtime
        std::vector<Sensor>::const_iterator i = sensors.begin();
        std::vector<Sensor>::const_iterator end = sensors.end();
        for (;i != end; ++i)
        {
            sensorIds.push_back(i->id().toUInt());
        }
    }

    // add all ids references in rules
    for (const Rule &r : rules)
    {
        for (const RuleCondition &c : r.conditions())
        {
            if (c.address().startsWith(QLatin1String("/sensors/")))
            {
                uint id = c.id().toUInt(&ok);
                if (ok && std::find(sensorIds.begin(), sensorIds.end(), id) == sensorIds.end())
                {
                    sensorIds.push_back(id);
                }
            }
        }
    }

    // append all ids from database (dublicates are ok here)
    QString sql = QString("SELECT * FROM sensors");

    DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
    rc = sqlite3_exec(db, qPrintable(sql), sqliteGetAllSensorIdsCallback, this, &errmsg);

    if (rc != SQLITE_OK)
    {
        if (errmsg)
        {
            DBG_Printf(DBG_ERROR_L2, "sqlite3_exec %s, error: %s\n", qPrintable(sql), errmsg);
            sqlite3_free(errmsg);
        }
    }

    int id = 1;
    while (1)
    {
        std::vector<int>::iterator result = std::find(sensorIds.begin(), sensorIds.end(), id);

        // id not known?
        if (result == sensorIds.end())
        {
            return id;
        }
        id++;
    }

    return id;
}

/*! Saves all nodes, groups and scenes to the database.
 */
void DeRestPluginPrivate::saveDb()
{
    DBG_Assert(db != 0);

    if (!db)
    {
        return;
    }

    if (saveDatabaseItems == 0)
    {
        return;
    }

    int rc;
    char *errmsg;
    QElapsedTimer measTimer;

    measTimer.start();

    {
        // create config table version 2 if not exist
        const char *sql = "CREATE TABLE IF NOT EXISTS config2 (key text PRIMARY KEY, value text)";

        DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
        errmsg = NULL;
        rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);

        if( rc != SQLITE_OK )
        {
            if (errmsg)
            {
                DBG_Printf(DBG_ERROR, "SQL exec failed: %s, error: %s\n", sql, errmsg);
                sqlite3_free(errmsg);
            }
        }
    }

    // make the whole save process one transaction otherwise each insert would become
    // a transaction which is extremly slow
    sqlite3_exec(db, "BEGIN", 0, 0, 0);

    DBG_Printf(DBG_INFO, "save zll database items 0x%08X\n", saveDatabaseItems);

    // dump authentification
    if (saveDatabaseItems & DB_AUTH)
    {
        std::vector<ApiAuth>::iterator i = apiAuths.begin();
        std::vector<ApiAuth>::iterator end = apiAuths.end();

        for (; i != end; ++i)
        {
            if (!i->needSaveDatabase)
            {
                continue;
            }

            i->needSaveDatabase = false;

            if (i->state == ApiAuth::StateDeleted)
            {
                // delete group from db (if exist)
                QString sql = QString(QLatin1String("DELETE FROM auth WHERE apikey='%1'")).arg(i->apikey);

                DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
                errmsg = NULL;
                rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

                if (rc != SQLITE_OK)
                {
                    if (errmsg)
                    {
                        DBG_Printf(DBG_ERROR, "sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
                        sqlite3_free(errmsg);
                    }
                }
            }
            else if (i->state == ApiAuth::StateNormal)
            {
                DBG_Assert(i->createDate.timeSpec() == Qt::UTC);
                DBG_Assert(i->lastUseDate.timeSpec() == Qt::UTC);

                QString sql = QString(QLatin1String("REPLACE INTO auth (apikey, devicetype, createdate, lastusedate, useragent) VALUES ('%1', '%2', '%3', '%4', '%5')"))
                        .arg(i->apikey)
                        .arg(i->devicetype)
                        .arg(i->createDate.toString("yyyy-MM-ddTHH:mm:ss"))
                        .arg(i->lastUseDate.toString("yyyy-MM-ddTHH:mm:ss"))
                        .arg(i->useragent);


                DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
                errmsg = NULL;
                rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

                if (rc != SQLITE_OK)
                {
                    if (errmsg)
                    {
                        DBG_Printf(DBG_ERROR, "sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
                        sqlite3_free(errmsg);
                    }
                }
            }
        }

        saveDatabaseItems &= ~DB_AUTH;
    }

    // dump config
    if (saveDatabaseItems & DB_CONFIG)
    {
        gwConfig["permitjoin"] = (double)gwPermitJoinDuration;
        gwConfig["networkopenduration"] = (double)gwNetworkOpenDuration;
        gwConfig["timeformat"] = gwTimeFormat;
        gwConfig["timezone"] = gwTimezone;
        gwConfig["rgbwdisplay"] = gwRgbwDisplay;
        gwConfig["rfconnect"] = (double)(gwRfConnectedExpected ? 1 : 0);
        gwConfig["announceinterval"] = (double)gwAnnounceInterval;
        gwConfig["announceurl"] = gwAnnounceUrl;
        gwConfig["groupdelay"] = gwGroupSendDelay;
        gwConfig["zigbeechannel"] = gwZigbeeChannel;
        gwConfig["gwusername"] = gwAdminUserName;
        gwConfig["gwpassword"] = gwAdminPasswordHash;
        gwConfig["updatechannel"] = gwUpdateChannel;
        gwConfig["swupdatestate"] = gwSwUpdateState;
        gwConfig["uuid"] = gwUuid;
        gwConfig["otauactive"] = isOtauActive();
        gwConfig["wifi"] = gwWifi;
        gwConfig["wifitype"] = gwWifiType;
        gwConfig["wifiname"] = gwWifiName;
        gwConfig["wificlientname"] = gwWifiClientName;
        gwConfig["wifichannel"] = gwWifiChannel;
        gwConfig["wifiappw"] = gwWifiPw;
        gwConfig["wificlientpw"] = gwWifiClientPw;
        gwConfig["wifiip"] = gwWifiIp;
        gwConfig["bridgeid"] = gwBridgeId;
        gwConfig["websocketnotifyall"] = gwWebSocketNotifyAll;
        gwConfig["proxyaddress"] = gwProxyAddress;
        gwConfig["proxyport"] = gwProxyPort;

        QVariantMap::iterator i = gwConfig.begin();
        QVariantMap::iterator end = gwConfig.end();

        for (; i != end; ++i)
        {
            if (i->canConvert(QVariant::String))
            {
                QString sql = QString(QLatin1String("REPLACE INTO config2 (key, value) VALUES ('%1', '%2')"))
                        .arg(i.key())
                        .arg(i.value().toString());

                DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
                errmsg = NULL;
                rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

                if (rc != SQLITE_OK)
                {
                    if (errmsg)
                    {
                        DBG_Printf(DBG_ERROR, "sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
                        sqlite3_free(errmsg);
                    }
                }
            }
        }

        saveDatabaseItems &= ~DB_CONFIG;
    }

    // save userparameter
    if (saveDatabaseItems & DB_USERPARAM)
    {
        QVariantMap::iterator i = gwUserParameter.begin();
        QVariantMap::iterator end = gwUserParameter.end();

        for (; i != end; ++i)
        {
            if (i->canConvert(QVariant::String))
            {
                QString sql = QString(QLatin1String("REPLACE INTO userparameter (key, value) VALUES ('%1', '%2')"))
                        .arg(i.key())
                        .arg(i.value().toString());

                DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
                errmsg = NULL;
                rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

                if (rc != SQLITE_OK)
                {
                    if (errmsg)
                    {
                        DBG_Printf(DBG_ERROR, "sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
                        sqlite3_free(errmsg);
                    }
                }
            }
        }

        while (!gwUserParameterToDelete.empty())
        {
            QString key = gwUserParameterToDelete.back();

            // delete parameter from db (if exist)
            QString sql = QString(QLatin1String("DELETE FROM userparameter WHERE key='%1'")).arg(key);
            gwUserParameterToDelete.pop_back();

            DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
            errmsg = NULL;
            rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

            if (rc != SQLITE_OK)
            {
                if (errmsg)
                {
                    DBG_Printf(DBG_ERROR, "sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
                    sqlite3_free(errmsg);
                }
            }
        }

        saveDatabaseItems &= ~DB_USERPARAM;
    }

    // save gateways
    if (saveDatabaseItems & DB_GATEWAYS)
    {
        std::vector<Gateway*>::iterator i = gateways.begin();
        std::vector<Gateway*>::iterator end = gateways.end();

        for (; i != end; ++i)
        {
            Gateway *gw = *i;
            if (!gw->needSaveDatabase())
            {
                continue;
            }

            gw->setNeedSaveDatabase(false);

            if (!gw->pairingEnabled())
            {
                // delete gateways from db (if exist)
                QString sql = QString(QLatin1String("DELETE FROM gateways WHERE uuid='%1'")).arg(gw->uuid());

                DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
                errmsg = NULL;
                rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

                if (rc != SQLITE_OK)
                {
                    if (errmsg)
                    {
                        DBG_Printf(DBG_ERROR, "sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
                        sqlite3_free(errmsg);
                    }
                }
            }
            else
            {
                QByteArray cgroups("[]");
                if (!gw->cascadeGroups().empty())
                {
                    QVariantList ls;
                    for (size_t i = 0; i < gw->cascadeGroups().size(); i++)
                    {
                        const Gateway::CascadeGroup &cg = gw->cascadeGroups()[i];
                        QVariantMap e;
                        e[QLatin1String("lg")] = (double)cg.local;
                        e[QLatin1String("rg")] = (double)cg.remote;
                        ls.push_back(e);
                    }
                    cgroups = Json::serialize(ls);
                }

                QString sql = QString(QLatin1String("REPLACE INTO gateways (uuid, name, ip, port, pairing, apikey, cgroups) VALUES ('%1', '%2', '%3', '%4', '%5', '%6', '%7')"))
                        .arg(gw->uuid())
                        .arg(gw->name())
                        .arg(gw->address().toString())
                        .arg(gw->port())
                        .arg((gw->pairingEnabled() ? '1' : '0'))
                        .arg(gw->apiKey())
                        .arg(qPrintable(cgroups));

                DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
                errmsg = NULL;
                rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

                if (rc != SQLITE_OK)
                {
                    if (errmsg)
                    {
                        DBG_Printf(DBG_ERROR, "sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
                        sqlite3_free(errmsg);
                    }
                }
            }
        }

        saveDatabaseItems &= ~DB_GATEWAYS;
    }

    // save nodes
    if (saveDatabaseItems & DB_LIGHTS)
    {
        std::vector<LightNode>::iterator i = nodes.begin();
        std::vector<LightNode>::iterator end = nodes.end();

        for (; i != end; ++i)
        {
            if (!i->needSaveDatabase())
            {
                continue;
            }

            i->setNeedSaveDatabase(false);

            /*
            if (i->state() == LightNode::StateDeleted)
            {
                // delete LightNode from db (if exist)
                QString sql = QString("DELETE FROM nodes WHERE id='%1'").arg(i->id());

                errmsg = NULL;
                rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

                if (rc != SQLITE_OK)
                {
                    if (errmsg)
                    {
                        DBG_Printf(DBG_ERROR, "sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
                        sqlite3_free(errmsg);
                    }
                }

                continue;
            }
            */

            QString lightState((i->state() == LightNode::StateDeleted ? "deleted" : "normal"));

            std::vector<GroupInfo>::const_iterator gi = i->groups().begin();
            std::vector<GroupInfo>::const_iterator gend = i->groups().end();

            QStringList groupIds;
            for ( ;gi != gend; ++gi)
            {
                if (gi->state == GroupInfo::StateInGroup)
                {
                    groupIds.append(QString::number((int)gi->id));
                }
            }

            QString sql = QString(QLatin1String("REPLACE INTO nodes (id, state, mac, name, groups, endpoint, modelid, manufacturername, swbuildid) VALUES ('%1', '%2', '%3', '%4', '%5', '%6', '%7', '%8', '%9')"))
                    .arg(i->id())
                    .arg(lightState)
                    .arg(i->uniqueId().toLower())
                    .arg(i->name())
                    .arg(groupIds.join(","))
                    .arg(i->haEndpoint().endpoint())
                    .arg(i->modelId())
                    .arg(i->manufacturer())
                    .arg(i->swBuildId());


            DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
            errmsg = NULL;
            rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

            if (rc != SQLITE_OK)
            {
                if (errmsg)
                {
                    DBG_Printf(DBG_ERROR, "sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
                    sqlite3_free(errmsg);
                }
            }

            // prevent deletion of nodes with numeric only mac address
            bool deleteUpperCase = false;
            for (int c = 0; c < i->uniqueId().size(); c++)
            {
                char ch = i->uniqueId().at(c).toLatin1();
                if (ch != '-' && ch != ':' && isalpha(ch))
                {
                    deleteUpperCase = true;
                    break;
                }
            }

            if (deleteUpperCase)
            {
                // delete old LightNode with upper case unique id from db (if exist)
                sql = QString("DELETE FROM nodes WHERE mac='%1'").arg(i->uniqueId().toUpper());
            }

            errmsg = NULL;
            rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

            if (rc != SQLITE_OK)
            {
                if (errmsg)
                {
                    DBG_Printf(DBG_ERROR, "sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
                    sqlite3_free(errmsg);
                }
            }
        }

        saveDatabaseItems &= ~DB_LIGHTS;
    }

    // save/delete groups and scenes
    if (saveDatabaseItems & (DB_GROUPS | DB_SCENES))
    {
        std::vector<Group>::const_iterator i = groups.begin();
        std::vector<Group>::const_iterator end = groups.end();

        for (; i != end; ++i)
        {
            QString gid;
            gid.sprintf("0x%04X", i->address());

            if (i->state() == Group::StateDeleted)
            {
                // delete scenes of this group (if exist)
                QString sql = QString(QLatin1String("DELETE FROM scenes WHERE gid='%1'")).arg(gid);

                DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
                errmsg = NULL;
                rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

                if (rc != SQLITE_OK)
                {
                    if (errmsg)
                    {
                        DBG_Printf(DBG_ERROR, "sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
                        sqlite3_free(errmsg);
                    }
                }
            }

            if (i->state() == Group::StateDeleteFromDB)
            {
                // delete group from db (if exist)
                QString sql = QString(QLatin1String("DELETE FROM groups WHERE gid='%1'")).arg(gid);

                DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
                errmsg = NULL;
                rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

                if (rc != SQLITE_OK)
                {
                    if (errmsg)
                    {
                        DBG_Printf(DBG_ERROR, "sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
                        sqlite3_free(errmsg);
                    }
                }
                continue;
            }

            QString grpState((i->state() == Group::StateDeleted ? QLatin1String("deleted") : QLatin1String("normal")));
            QString hidden((i->hidden == true ? QLatin1String("true") : QLatin1String("false")));
            const QString &gtype = i->item(RAttrType)->toString();
            const QString &gclass = i->item(RAttrClass)->toString();

            QString sql = QString(QLatin1String("REPLACE INTO groups (gid, name, state, mids, devicemembership, lightsequence, hidden, type, class) VALUES ('%1', '%2', '%3', '%4', '%5', '%6', '%7', '%8', '%9')"))
                    .arg(gid)
                    .arg(i->name())
                    .arg(grpState)
                    .arg(i->midsToString())
                    .arg(i->dmToString())
                    .arg(i->lightsequenceToString())
                    .arg(hidden)
                    .arg(gtype)
                    .arg(gclass);

            DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
            errmsg = NULL;
            rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

            if (rc != SQLITE_OK)
            {
                if (errmsg)
                {
                    DBG_Printf(DBG_ERROR, "sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
                    sqlite3_free(errmsg);
                }
            }

            if (i->state() == Group::StateNormal)
            {
                std::vector<Scene>::const_iterator si = i->scenes.begin();
                std::vector<Scene>::const_iterator send = i->scenes.end();

                for (; si != send; ++si)
                {
                    QString gsid; // unique key
                    gsid.sprintf("0x%04X%02X", i->address(), si->id);

                    QString sid;
                    sid.sprintf("0x%02X", si->id);

                    QString lights = Scene::lightsToString(si->lights());
                    QString sql;

                    if (si->state == Scene::StateDeleted)
                    {
                        // delete scene from db (if exist)
                        sql = QString(QLatin1String("DELETE FROM scenes WHERE gsid='%1'")).arg(gsid);
                    }
                    else
                    {
                        sql = QString(QLatin1String("REPLACE INTO scenes (gsid, gid, sid, name, transitiontime, lights) VALUES ('%1', '%2', '%3', '%4', '%5', '%6')"))
                            .arg(gsid)
                            .arg(gid)
                            .arg(sid)
                            .arg(si->name)
                            .arg(si->transitiontime())
                            .arg(lights);
                    }
                    DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
                    errmsg = NULL;
                    rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

                    if (rc != SQLITE_OK)
                    {
                        if (errmsg)
                        {
                            DBG_Printf(DBG_ERROR, "sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
                            sqlite3_free(errmsg);
                        }
                    }
                }
            }
        }

        saveDatabaseItems &= ~(DB_GROUPS | DB_SCENES);
    }

    // save/delete rules
    if (saveDatabaseItems & DB_RULES)
    {
        std::vector<Rule>::const_iterator i = rules.begin();
        std::vector<Rule>::const_iterator end = rules.end();

        for (; i != end; ++i)
        {
            const QString &rid = i->id();

            if (i->state() == Rule::StateDeleted)
            {
                // delete rule from db (if exist)
                QString sql = QString(QLatin1String("DELETE FROM rules WHERE rid='%1'")).arg(rid);

                DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
                errmsg = NULL;
                rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

                if (rc != SQLITE_OK)
                {
                    if (errmsg)
                    {
                        DBG_Printf(DBG_ERROR, "sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
                        sqlite3_free(errmsg);
                    }
                }

                continue;
            }

            QString actionsJSON = Rule::actionsToString(i->actions());
            QString conditionsJSON = Rule::conditionsToString(i->conditions());

//            QString sql = QString(QLatin1String("REPLACE INTO rules (rid, name, created, etag, lasttriggered, owner, status, timestriggered, actions, conditions) VALUES ('%1', '%2', '%3', '%4', '%5', '%6', '%7', '%8', '%9', '%10')"))
//                    .arg(rid, i->name(), i->creationtime(),
//                         i->etag, i->lastTriggered(), i->owner(),
//                         i->status(), QString::number(i->timesTriggered()), actionsJSON)
//                    .arg(conditionsJSON);

            QString sql = QLatin1String("REPLACE INTO rules (rid, name, created, etag, lasttriggered, owner, status, timestriggered, actions, conditions, periodic) VALUES ('") +
                    rid + QLatin1String("','") +
                    i->name() + QLatin1String("','") +
                    i->creationtime() + QLatin1String("','") +
                    i->etag + QLatin1String("','") +
                    QLatin1String("none','") +
                    i->owner() + QLatin1String("','") +
                    i->status() + QLatin1String("','") +
                    QString::number(i->timesTriggered()) + QLatin1String("','") +
                    actionsJSON + QLatin1String("','") +
                    conditionsJSON + QLatin1String("','") +
                    QString::number(i->triggerPeriodic()) + QLatin1String("')");


            DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
            errmsg = NULL;
            rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

            if (rc != SQLITE_OK)
            {
                if (errmsg)
                {
                    DBG_Printf(DBG_ERROR, "sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
                    sqlite3_free(errmsg);
                }
            }
        }

        saveDatabaseItems &= ~DB_RULES;
    }

    // save/delete resourcelinks
    if (saveDatabaseItems & DB_RESOURCELINKS)
    {
        for (Resourcelinks &rl : resourcelinks)
        {
            if (!rl.needSaveDatabase())
            {
                continue;
            }

            rl.setNeedSaveDatabase(false);

            if (rl.state == Resourcelinks::StateNormal)
            {
                QString json = Json::serialize(rl.data);
                QString sql = QString(QLatin1String("REPLACE INTO resourcelinks (id, json) VALUES ('%1', '%2')"))
                        .arg(rl.id)
                        .arg(json);

                DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
                errmsg = NULL;
                rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

                if (rc != SQLITE_OK)
                {
                    if (errmsg)
                    {
                        DBG_Printf(DBG_ERROR, "sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
                        sqlite3_free(errmsg);
                    }
                }
            }
            else if (rl.state == Resourcelinks::StateDeleted)
            {
                QString sql = QString(QLatin1String("DELETE FROM resourcelinks WHERE id='%1'")).arg(rl.id);

                DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
                errmsg = NULL;
                rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

                if (rc != SQLITE_OK)
                {
                    if (errmsg)
                    {
                        DBG_Printf(DBG_ERROR, "sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
                        sqlite3_free(errmsg);
                    }
                }
            }
        }

        saveDatabaseItems &= ~DB_RESOURCELINKS;
    }

    // save/delete schedules
    if (saveDatabaseItems & DB_SCHEDULES)
    {
        std::vector<Schedule>::iterator i = schedules.begin();
        std::vector<Schedule>::iterator end = schedules.end();

        for (; i != end; ++i)
        {
            if (i->state ==Schedule::StateNormal)
            {
                QString sql = QString(QLatin1String("REPLACE INTO schedules (id, json) VALUES ('%1', '%2')"))
                        .arg(i->id)
                        .arg(i->jsonString);

                DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
                errmsg = NULL;
                rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

                if (rc != SQLITE_OK)
                {
                    if (errmsg)
                    {
                        DBG_Printf(DBG_ERROR, "sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
                        sqlite3_free(errmsg);
                    }
                }
            }
            else if (i->state == Schedule::StateDeleted)
            {
                QString sql = QString(QLatin1String("DELETE FROM schedules WHERE id='%1'")).arg(i->id);

                DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
                errmsg = NULL;
                rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

                if (rc != SQLITE_OK)
                {
                    if (errmsg)
                    {
                        DBG_Printf(DBG_ERROR, "sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
                        sqlite3_free(errmsg);
                    }
                }
                else
                {
                    //i = schedules.erase(i);
                }
            }
        }

        saveDatabaseItems &= ~DB_SCHEDULES;
    }

    // save/delete sensors
    if (saveDatabaseItems & DB_SENSORS)
    {
        std::vector<Sensor>::iterator i = sensors.begin();
        std::vector<Sensor>::iterator end = sensors.end();

        for (; i != end; ++i)
        {

            if (!i->needSaveDatabase())
            {
                continue;
            }

            i->setNeedSaveDatabase(false);

            /*
            if (i->deletedState() == Sensor::StateDeleted)
            {
                // delete sensor from db (if exist)
                QString sql = QString("DELETE FROM sensors WHERE sid='%1'").arg(sid);

                errmsg = NULL;
                rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

                if (rc != SQLITE_OK)
                {
                    if (errmsg)
                    {
                        DBG_Printf(DBG_ERROR, "sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
                        sqlite3_free(errmsg);
                    }
                }

                continue;
            }
            */
            QString stateJSON = i->stateToString();
            QString configJSON = i->configToString();
            QString fingerPrintJSON = i->fingerPrint().toString();
            QString deletedState((i->deletedState() == Sensor::StateDeleted ? "deleted" : "normal"));

            QString sql = QString(QLatin1String("REPLACE INTO sensors (sid, name, type, modelid, manufacturername, uniqueid, swversion, state, config, fingerprint, deletedState, mode) VALUES ('%1', '%2', '%3', '%4', '%5', '%6', '%7', '%8', '%9', '%10', '%11', '%12')"))
                    .arg(i->id())
                    .arg(i->name())
                    .arg(i->type())
                    .arg(i->modelId())
                    .arg(i->manufacturer())
                    .arg(i->uniqueId())
                    .arg(i->swVersion())
                    .arg(stateJSON)
                    .arg(configJSON)
                    .arg(fingerPrintJSON)
                    .arg(deletedState)
                    .arg(QString::number(i->mode()));

            DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
            errmsg = NULL;
            rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

            if (rc != SQLITE_OK)
            {
                if (errmsg)
                {
                    DBG_Printf(DBG_ERROR, "sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
                    sqlite3_free(errmsg);
                }
            }
        }

        saveDatabaseItems &= ~DB_SENSORS;
    }

    sqlite3_exec(db, "COMMIT", 0, 0, 0);
    DBG_Printf(DBG_INFO, "database saved in %ld ms\n", measTimer.elapsed());

#ifdef Q_OS_LINUX
    measTimer.restart();
    sync();
    DBG_Printf(DBG_INFO, "sync() in %d ms\n", int(measTimer.elapsed()));
#endif
}

/*! Closes the database.
    If closing fails for some reason the db pointer is not 0 and the database left open.
 */
void DeRestPluginPrivate::closeDb()
{
    if (db)
    {
        if (ttlDataBaseConnection > idleTotalCounter)
        {
            DBG_Printf(DBG_INFO, "don't close database yet, keep open for %d seconds\n", (ttlDataBaseConnection - idleTotalCounter));
            return;
        }

        int ret = sqlite3_close(db);
        if (ret == SQLITE_OK)
        {
            db = 0;
            return;
        }

        DBG_Printf(DBG_INFO, "sqlite3_close() failed %d\n", ret);

    }

    DBG_Assert(db == 0);
}

/*! Request saving of database.
   \param items - bitmap of DB_ flags
   \param msec - delay in milliseconds
 */
void DeRestPluginPrivate::queSaveDb(int items, int msec)
{
    saveDatabaseItems |= items;

    if (databaseTimer->isActive())
    {
        // prefer shorter interval
        if (databaseTimer->interval() > msec)
        {
            databaseTimer->stop();
            databaseTimer->start(msec);
        }

        return;
    }

    databaseTimer->start(msec);
}

/*! Checks various data for consistency.
 */
void DeRestPluginPrivate::checkConsistency()
{
    if (gwProxyAddress == QLatin1String("none"))
    {
        gwProxyPort = 0;
    }

    {
        std::vector<Group>::iterator i = groups.begin();
        std::vector<Group>::iterator end = groups.end();

        for (; i != end; ++i)
        {
            for (size_t j = 0; j < i->m_deviceMemberships.size(); j++)
            {
                const QString &sid = i->m_deviceMemberships[j];
                Sensor *sensor = getSensorNodeForId(sid);

                if (!sensor || sensor->deletedState() != Sensor::StateNormal)
                {
                    // sensor isn't available anymore
                    DBG_Printf(DBG_INFO, "remove sensor %s from group 0x%04X\n", qPrintable(sid), i->address());
                    i->m_deviceMemberships[j] = i->m_deviceMemberships.back();
                    i->m_deviceMemberships.pop_back();
                }
                else
                {
                    j++;
                }
            }
        }
    }
}

/*! Timer handler for storing persistent data.
 */
void DeRestPluginPrivate::saveDatabaseTimerFired()
{
    if (otauLastBusyTimeDelta() < OTA_LOW_PRIORITY_TIME)
    {
        if ((idleTotalCounter - saveDatabaseIdleTotalCounter) < (60 * 30))
        {
            databaseTimer->start(DB_SHORT_SAVE_DELAY);
            return;
        }
    }

    if (saveDatabaseItems)
    {
        saveDatabaseIdleTotalCounter = idleTotalCounter;
        openDb();
        saveDb();
        closeDb();

        DBG_Assert(saveDatabaseItems == 0);
    }
}
