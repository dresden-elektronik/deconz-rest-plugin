/*
 * Copyright (c) 2016 dresden elektronik ingenieurtechnik gmbh.
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
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "deconz/dbg_trace.h"
#include "json.h"

/******************************************************************************
                    Local prototypes
******************************************************************************/
static int sqliteLoadAuthCallback(void *user, int ncols, char **colval , char **colname);
static int sqliteLoadConfigCallback(void *user, int ncols, char **colval , char **colname);
static int sqliteLoadLightNodeCallback(void *user, int ncols, char **colval , char **colname);
static int sqliteLoadSensorNodeCallback(void *user, int ncols, char **colval , char **colname);
static int sqliteLoadAllGroupsCallback(void *user, int ncols, char **colval , char **colname);
static int sqliteLoadGroupCallback(void *user, int ncols, char **colval , char **colname);
static int sqliteLoadSceneCallback(void *user, int ncols, char **colval , char **colname);
static int sqliteLoadAllRulesCallback(void *user, int ncols, char **colval , char **colname);
static int sqliteLoadAllSensorsCallback(void *user, int ncols, char **colval , char **colname);
static int sqliteGetAllLightIdsCallback(void *user, int ncols, char **colval , char **colname);
static int sqliteGetAllSensorIdsCallback(void *user, int ncols, char **colval , char **colname);

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
        "CREATE TABLE IF NOT EXISTS nodes (mac TEXT PRIMARY KEY, id TEXT, name TEXT, groups TEXT)",
        "ALTER TABLE nodes add column id TEXT",
        "ALTER TABLE nodes add column state TEXT",
        "ALTER TABLE nodes add column groups TEXT",
        "ALTER TABLE nodes add column endpoint TEXT",
        "ALTER TABLE auth add column createdate TEXT",
        "ALTER TABLE auth add column lastusedate TEXT",
        "ALTER TABLE auth add column useragent TEXT",
        "CREATE TABLE IF NOT EXISTS groups (gid TEXT PRIMARY KEY, name TEXT, state TEXT, mids TEXT, devicemembership TEXT, lightsequence TEXT)",
        "CREATE TABLE IF NOT EXISTS rules (rid TEXT PRIMARY KEY, name TEXT, created TEXT, etag TEXT, lasttriggered TEXT, owner TEXT, status TEXT, timestriggered TEXT, actions TEXT, conditions TEXT, periodic TEXT)",
        "CREATE TABLE IF NOT EXISTS sensors (sid TEXT PRIMARY KEY, name TEXT, type TEXT, modelid TEXT, manufacturername TEXT, uniqueid TEXT, swversion TEXT, state TEXT, config TEXT, fingerprint TEXT, deletedState TEXT, mode TEXT)",
        "CREATE TABLE IF NOT EXISTS scenes (gsid TEXT PRIMARY KEY, gid TEXT, sid TEXT, name TEXT, transitiontime TEXT, lights TEXT)",
        "CREATE TABLE IF NOT EXISTS schedules (id TEXT PRIMARY KEY, json TEXT)",
        "ALTER TABLE sensors add column fingerprint TEXT",
        "ALTER TABLE sensors add column deletedState TEXT",
        "ALTER TABLE sensors add column mode TEXT",
        "ALTER TABLE groups add column state TEXT",
        "ALTER TABLE groups add column mids TEXT",
        "ALTER TABLE groups add column devicemembership TEXT",
        "ALTER TABLE groups add column lightsequence TEXT",
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

/*! Opens/creates sqlite database.
 */
void DeRestPluginPrivate::openDb()
{
    DBG_Assert(db == 0);

    if (db)
    {
        return;
    }

    int rc;
    db = 0;
    rc = sqlite3_open(qPrintable(sqliteDatabaseName), &db);

    if (rc != SQLITE_OK) {
        // failed
        DBG_Printf(DBG_ERROR, "Can't open database: %s\n", sqlite3_errmsg(db));
        db = 0;
        return;
    }
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
    loadAllGroupsFromDb();
    loadAllRulesFromDb();
    loadAllSchedulesFromDb();
    loadAllSensorsFromDb();
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
    auth.devicetype = colval[1];

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

    QString sql = QString("SELECT apikey,devicetype,createdate,lastusedate,useragent FROM auth");

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

    return 0;
}

/*! Loads all groups from database
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

        errmsg = NULL;
        rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

        if (rc == SQLITE_OK)
        {
            configTable = "config2";
        }
    }

    {
        QString sql = QString("SELECT key,value FROM %1").arg(configTable);

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
                if (val == "deleted")
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

            if (strcmp(colname[i], "endpoint") == 0)
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

        if (lightNode->node())
        {
            lightNode->node()->setUserDescriptor(lightNode->name());
        }
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
    QString sql = QString("SELECT * FROM nodes WHERE mac='%1'").arg(lightNode->uniqueId());

    rc = sqlite3_exec(db, qPrintable(sql), sqliteLoadLightNodeCallback, lightNode, &errmsg);

    if (rc != SQLITE_OK)
    {
        if (errmsg)
        {
            DBG_Printf(DBG_ERROR_L2, "sqlite3_exec %s, error: %s\n", qPrintable(sql), errmsg);
            sqlite3_free(errmsg);
        }
    }

    // check for old mac address only format
    if (lightNode->id().isEmpty())
    {
        sql = QString("SELECT * FROM nodes WHERE mac='%1'").arg(lightNode->address().toStringExt());

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
            queSaveDb(DB_LIGHTS, DB_SHORT_SAVE_DELAY);
        }
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

/*! Sqlite callback to load data for a node (identified by its mac address).
 */
static int sqliteLoadSensorNodeCallback(void *user, int ncols, char **colval , char **colname)
{
    DBG_Assert(user != 0);

    if (!user || (ncols <= 0))
    {
        return 0;
    }

    Sensor *sensorNode = static_cast<Sensor*>(user);

    for (int i = 0; i < ncols; i++)
    {
        if (colval[i] && (colval[i][0] != '\0'))
        {
            if (strcmp(colname[i], "name") == 0)
            {
                sensorNode->setName(QString::fromUtf8(colval[i]));

                if (sensorNode->node())
                {
                    sensorNode->node()->setUserDescriptor(sensorNode->name());
                }
            }
            else if (strcmp(colname[i], "id") == 0)
            {
                sensorNode->setId(QString::fromUtf8(colval[i]));
            }
        }
    }

    return 0;
}

/*! Loads data (if available) for a SensorNode from the database.
 */
void DeRestPluginPrivate::loadSensorNodeFromDb(Sensor *sensorNode)
{
    int rc;
    char *errmsg = 0;

    DBG_Assert(db != 0);
    DBG_Assert(sensorNode != 0);

    if (!db || !sensorNode)
    {
        return;
    }

    QString sql = QString("SELECT * FROM sensors WHERE uniqueid='%1' AND type='%2'").arg(sensorNode->address().toStringExt()).arg(sensorNode->type());

    rc = sqlite3_exec(db, qPrintable(sql), sqliteLoadSensorNodeCallback, sensorNode, &errmsg);

    if (rc != SQLITE_OK)
    {
        if (errmsg)
        {
            DBG_Printf(DBG_ERROR_L2, "sqlite3_exec %s, error: %s\n", qPrintable(sql), errmsg);
            sqlite3_free(errmsg);
        }
    }

    // check for unique IDs
    if (!sensorNode->id().isEmpty())
    {
        std::vector<Sensor>::iterator i = sensors.begin();
        std::vector<Sensor>::iterator end = sensors.end();

        for (; i != end; ++i)
        {
            if (&(*i) != sensorNode)
            {
                // id already set to another node
                // empty it here so a new one will be generated
                if (i->id() == sensorNode->id())
                {
                    DBG_Printf(DBG_INFO, "detected already used SensorNode id %s, force generate new id\n", qPrintable(i->id()));
                    sensorNode->setId("");
                    queSaveDb(DB_SENSORS, DB_LONG_SAVE_DELAY);
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
            else if (strcmp(colname[i], "lasttriggered") == 0)
            {
                rule.setLastTriggered(val);
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
                sensor.setType(val);
            }
            else if (strcmp(colname[i], "modelid") == 0)
            {
                sensor.setModelId(val);
            }
            else if (strcmp(colname[i], "mode") == 0)
            {
                sensor.setMode(val.toUInt());
            }
            else if (strcmp(colname[i], "etag") == 0)
            {
                sensor.etag = val;
            }
            else if (strcmp(colname[i], "manufacturername") == 0)
            {
                sensor.setManufacturer(val);
            }
            else if (strcmp(colname[i], "uniqueid") == 0)
            {
                sensor.setUniqueId(val);
            }
            else if (strcmp(colname[i], "swversion") == 0)
            {
                sensor.setSwVersion(val);
            }
            else if (strcmp(colname[i], "state") == 0)
            {
                sensor.setState(Sensor::jsonToState(val));
            }
            else if (strcmp(colname[i], "config") == 0)
            {
                SensorConfig config = Sensor::jsonToConfig(val);
                config.setReachable(false); // will be set later on
                sensor.setConfig(config);
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
                }
                else
                {
                    sensor.setDeletedState(Sensor::StateNormal);
                }
            }
        }
    }

    if (!sensor.id().isEmpty() && !sensor.name().isEmpty() && !sensor.uniqueId().isEmpty())
    {
        DBG_Printf(DBG_INFO_L2, "DB found sensor %s %s\n", qPrintable(sensor.name()), qPrintable(sensor.id()));
        // check doubles
        bool ok = false;
        quint64 extAddr = sensor.uniqueId().toULongLong(&ok, 16);
        if (ok)
        {
            Sensor *s = d->getSensorNodeForFingerPrint(extAddr, sensor.fingerPrint(), sensor.type());

            if (!s)
            {
                sensor.address().setExt(extAddr);
                // append to cache if not already known
                d->updateEtag(sensor.etag);
                d->sensors.push_back(sensor);
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

/*! Determines a unused id for a sensor.
 */
int DeRestPluginPrivate::getFreeSensorId()
{
    int rc;
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

    // append all ids from database (dublicates are ok here)
    QString sql = QString("SELECT * FROM sensors");

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

    DBG_Printf(DBG_INFO, "save zll database\n");

    // dump authentification
    if (saveDatabaseItems & DB_AUTH)
    {
        std::vector<ApiAuth>::iterator i = apiAuths.begin();
        std::vector<ApiAuth>::iterator end = apiAuths.end();

        for (; i != end; ++i)
        {
            if (i->state == ApiAuth::StateDeleted)
            {
                // delete group from db (if exist)
                QString sql = QString(QLatin1String("DELETE FROM auth WHERE apikey='%1'")).arg(i->apikey);

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
                    i = apiAuths.erase(i);
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
        gwConfig["uuid"] = gwUuid;
        gwConfig["otauactive"] = isOtauActive();

        QVariantMap::iterator i = gwConfig.begin();
        QVariantMap::iterator end = gwConfig.end();

        for (; i != end; ++i)
        {
            if (i->canConvert(QVariant::String))
            {
                QString sql = QString(QLatin1String("REPLACE INTO config2 (key, value) VALUES ('%1', '%2')"))
                        .arg(i.key())
                        .arg(i.value().toString());

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

    // save nodes
    if (saveDatabaseItems & DB_LIGHTS)
    {
        std::vector<LightNode>::const_iterator i = nodes.begin();
        std::vector<LightNode>::const_iterator end = nodes.end();

        for (; i != end; ++i)
        {
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

            QString sql = QString(QLatin1String("REPLACE INTO nodes (id, state, mac, name, groups, endpoint) VALUES ('%1', '%2', '%3', '%4', '%5', '%6')"))
                    .arg(i->id())
                    .arg(lightState)
                    .arg(i->uniqueId())
                    .arg(i->name())
                    .arg(groupIds.join(","))
                    .arg(i->haEndpoint().endpoint());

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

            QString grpState((i->state() == Group::StateDeleted ? "deleted" : "normal"));

            QString sql = QString(QLatin1String("REPLACE INTO groups (gid, name, state, mids, devicemembership, lightsequence) VALUES ('%1', '%2', '%3', '%4', '%5', '%6')"))
                    .arg(gid)
                    .arg(i->name())
                    .arg(grpState)
                    .arg(i->midsToString())
                    .arg(i->dmToString())
                    .arg(i->lightsequenceToString());

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

            if (i->state() != Group::StateDeleted &&i->state() != Group::StateDeleteFromDB)
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

                    QString sql = QString(QLatin1String("REPLACE INTO scenes (gsid, gid, sid, name, transitiontime, lights) VALUES ('%1', '%2', '%3', '%4', '%5', '%6')"))
                            .arg(gsid)
                            .arg(gid)
                            .arg(sid)
                            .arg(si->name)
                            .arg(si->transitiontime())
                            .arg(lights);

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
                    i->lastTriggered() + QLatin1String("','") +
                    i->owner() + QLatin1String("','") +
                    i->status() + QLatin1String("','") +
                    QString::number(i->timesTriggered()) + QLatin1String("','") +
                    actionsJSON + QLatin1String("','") +
                    conditionsJSON + QLatin1String("','") +
                    QString::number(i->triggerPeriodic()) + QLatin1String("')");


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
                    i = schedules.erase(i);
                }
            }
        }

        saveDatabaseItems &= ~DB_SCHEDULES;
    }

    // save/delete sensors
    if (saveDatabaseItems & DB_SENSORS)
    {
        std::vector<Sensor>::const_iterator i = sensors.begin();
        std::vector<Sensor>::const_iterator end = sensors.end();

        for (; i != end; ++i)
        {
            QString sid = i->id();

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
            QString stateJSON = Sensor::stateToString(i->state());
            QString configJSON = Sensor::configToString(i->config());
            QString fingerPrintJSON = i->fingerPrint().toString();
            QString deletedState((i->deletedState() == Sensor::StateDeleted ? "deleted" : "normal"));

            QString sql = QString(QLatin1String("REPLACE INTO sensors (sid, name, type, modelid, manufacturername, uniqueid, swversion, state, config, fingerprint, deletedState, mode) VALUES ('%1', '%2', '%3', '%4', '%5', '%6', '%7', '%8', '%9', '%10', '%11', '%12')"))
                    .arg(sid)
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
}

/*! Closes the database.
    If closing fails for some reason the db pointer is not 0 and the database left open.
 */
void DeRestPluginPrivate::closeDb()
{
    if (db)
    {
        if (sqlite3_close(db) == SQLITE_OK)
        {
            db = 0;
        }
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

/*! Timer handler for storing persistent data.
 */
void DeRestPluginPrivate::saveDatabaseTimerFired()
{
    if (isOtauBusy())
    {
        databaseTimer->start(DB_SHORT_SAVE_DELAY);
        return;
    }

    if (saveDatabaseItems)
    {
        openDb();
        saveDb();
        closeDb();

        DBG_Assert(saveDatabaseItems == 0);
    }
}
