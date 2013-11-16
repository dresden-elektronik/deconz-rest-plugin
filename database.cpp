/*
 * Copyright (C) 2013 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QString>
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
static int sqliteLoadAllGroupsCallback(void *user, int ncols, char **colval , char **colname);
static int sqliteLoadGroupCallback(void *user, int ncols, char **colval , char **colname);
static int sqliteLoadSceneCallback(void *user, int ncols, char **colval , char **colname);
static int sqliteGetAllLightIdsCallback(void *user, int ncols, char **colval , char **colname);

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
        "CREATE TABLE IF NOT EXISTS nodes (mac TEXT PRIMARY KEY, id TEXT, name TEXT)",
        "ALTER TABLE nodes add column id TEXT",
        "ALTER TABLE auth add column createdate TEXT",
        "ALTER TABLE auth add column lastusedate TEXT",
        "ALTER TABLE auth add column useragent TEXT",
        "CREATE TABLE IF NOT EXISTS groups (gid TEXT PRIMARY KEY, name TEXT)",
        "CREATE TABLE IF NOT EXISTS scenes (gsid TEXT PRIMARY KEY, gid TEXT, sid TEXT, name TEXT)",
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

/*! Loads all groups from database
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

    for (int i = 0; i < ncols; i++)
    {
        if (colval[i] && (colval[i][0] != '\0'))
        {
            if (strcmp(colname[i], "name") == 0)
            {
                lightNode->setName(QString::fromUtf8(colval[i]));

                if (lightNode->node())
                {
                    lightNode->node()->setUserDescriptor(lightNode->name());
                }
            }
            else if (strcmp(colname[i], "id") == 0)
            {
                lightNode->setId(QString::fromUtf8(colval[i]));
            }
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

    QString sql = QString("SELECT * FROM nodes WHERE mac='%1'").arg(lightNode->address().toStringExt());

    rc = sqlite3_exec(db, qPrintable(sql), sqliteLoadLightNodeCallback, lightNode, &errmsg);

    if (rc != SQLITE_OK)
    {
        if (errmsg)
        {
            DBG_Printf(DBG_ERROR_L2, "sqlite3_exec %s, error: %s\n", qPrintable(sql), errmsg);
            sqlite3_free(errmsg);
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

    for (int i = 0; i < ncols; i++) {
        if (colval[i] && (colval[i][0] != '\0'))
        {
            if (strcmp(colname[i], "name") == 0)
            {
                group->setName(QString::fromUtf8(colval[i]));
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

    QString sql = QString("SELECT name FROM groups WHERE gid='%1'").arg(gid);

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

    QString sql = QString("SELECT name FROM scenes WHERE gsid='%1'").arg(gsid);

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
            DBG_Assert(i->createDate.timeSpec() == Qt::UTC);
            DBG_Assert(i->lastUseDate.timeSpec() == Qt::UTC);

            QString sql = QString("REPLACE INTO auth (apikey, devicetype, createdate, lastusedate, useragent) VALUES ('%1', '%2', '%3', '%4', '%5')")
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

        saveDatabaseItems &= ~DB_AUTH;
    }

    // dump config
    gwConfig["permitjoin"] = (double)gwPermitJoinDuration;
    gwConfig["announceinterval"] = (double)gwAnnounceInterval;
    gwConfig["announceurl"] = gwAnnounceUrl;
    gwConfig["groupdelay"] = gwGroupSendDelay;
    gwConfig["gwusername"] = gwAdminUserName;
    gwConfig["gwpassword"] = gwAdminPasswordHash;
    gwConfig["uuid"] = gwUuid;

    if (saveDatabaseItems & DB_CONFIG)
    {
        QVariantMap::iterator i = gwConfig.begin();
        QVariantMap::iterator end = gwConfig.end();

        for (; i != end; ++i)
        {
            if (i->canConvert(QVariant::String))
            {
                QString sql = QString("REPLACE INTO config2 (key, value) VALUES ('%1', '%2')")
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
            QString sql = QString("REPLACE INTO nodes (id, mac, name) VALUES ('%1', '%2', '%3')")
                    .arg(i->id())
                    .arg(i->address().toStringExt())
                    .arg(i->name());

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
                // delete group from db (if exist)
                QString sql = QString("DELETE FROM groups WHERE gid='%1'").arg(gid);

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

                // delete also scenes of this group (if exist)
                sql = QString("DELETE FROM scenes WHERE gid='%1'").arg(gid);

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

            QString sql = QString("REPLACE INTO groups (gid, name) VALUES ('%1', '%2')")
                    .arg(gid)
                    .arg(i->name());

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

            std::vector<Scene>::const_iterator si = i->scenes.begin();
            std::vector<Scene>::const_iterator send = i->scenes.end();

            for (; si != send; ++si)
            {
                QString gsid; // unique key
                gsid.sprintf("0x%04X%02X", i->address(), si->id);

                QString sid;
                sid.sprintf("0x%02X", si->id);

                QString sql = QString("REPLACE INTO scenes (gsid, gid, sid, name) VALUES ('%1', '%2', '%3', '%4')")
                        .arg(gsid)
                        .arg(gid)
                        .arg(sid)
                        .arg(si->name);

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

        saveDatabaseItems &= ~(DB_GROUPS | DB_SCENES);
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
    if (saveDatabaseItems)
    {
        openDb();
        saveDb();
        closeDb();

        DBG_Assert(saveDatabaseItems == 0);
    }
}
