/*
 * Copyright (c) 2016-2020 dresden elektronik ingenieurtechnik gmbh.
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

static const char *pragmaUserVersion = "PRAGMA user_version";
static const char *pragmaPageCount = "PRAGMA page_count";
static const char *pragmaPageSize = "PRAGMA page_size";
static const char *pragmaFreeListCount = "PRAGMA freelist_count";

struct DB_Callback {
  DeRestPluginPrivate *d = nullptr;
  LightNode *lightNode = nullptr;
  Sensor *sensorNode = nullptr;
};

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
        DBG_Printf(DBG_ERROR, "DB initDb() failed db not opened\n");
        return;
    }

    DBG_Printf(DBG_INFO, "DB sqlite version %s\n", sqlite3_libversion());

    int pageCount = getDbPragmaInteger(pragmaPageCount);
    int pageSize = getDbPragmaInteger(pragmaPageSize);
    int pageFreeListCount = getDbPragmaInteger(pragmaFreeListCount);
    DBG_Printf(DBG_INFO, "DB file size %d bytes, free pages %d\n", pageCount * pageSize, pageFreeListCount);

    checkDbUserVersion();
}

/*! Checks the sqlite 'user_version' in order to apply database schema updates.
    Updates are applied in recursive manner to have sane upgrade paths from
    certain versions in the field.
 */
void DeRestPluginPrivate::checkDbUserVersion()
{
    bool updated = false;
    int userVersion = getDbPragmaInteger(pragmaUserVersion); // sqlite default is 0

    if (userVersion == 0) // initial and legacy databases
    {
        updated = upgradeDbToUserVersion1();
    }
    else if (userVersion == 1)
    {
        updated = upgradeDbToUserVersion2();
    }
    else if (userVersion >= 2 && userVersion <= 5 )
    {
        updated = upgradeDbToUserVersion6();
    }
    else if (userVersion == 6)
    {
        updated = upgradeDbToUserVersion7();
    }
    else if (userVersion == 7)
    {
        // latest version
    }
    else
    {
        DBG_Printf(DBG_INFO, "DB database file opened with a older deCONZ version\n");
    }

    // if something was upgraded
    if (updated)
    {
        checkDbUserVersion();
    }
    else
    {
        cleanUpDb();
        createTempViews();
    }
}

/*! Cleanup tasks for database maintenance.
 */
void DeRestPluginPrivate::cleanUpDb()
{
    int rc;
    char *errmsg;
    DBG_Printf(DBG_INFO, "DB cleanup\n");

    /* Create SQL statement */
    const char *sql[] = {
        // cleanup invalid sensors created in version 2.05.30
        "DELETE from sensors "
        "   WHERE modelid like 'RWL02%' "
        "   AND type = 'ZHAPresence'",

        // cleanup invalid sensor resource for Centralite motion sensor
        "DELETE FROM sensors WHERE modelid = 'Motion Sensor-A' AND uniqueid LIKE '%02-0406'",

        // cleanup invalid ZHAAlarm resource for Xiaomi motion sensor
        "DELETE from sensors WHERE type = 'ZHAAlarm' AND modelid LIKE 'lumi.sensor_motion%'",

        // delete duplicates in device_descriptors
        //"DELETE FROM device_descriptors WHERE rowid NOT IN"
        //" (SELECT max(rowid) FROM device_descriptors GROUP BY device_id,type,endpoint)",

        // change old default value of zcl data store, from 1 hour to disabled
        "UPDATE config2 SET value = 0 WHERE key = 'zclvaluemaxage' AND value = 3600",

        nullptr
    };

    for (int i = 0; sql[i] != nullptr; i++)
    {
        errmsg = nullptr;

        /* Execute SQL statement */
        rc = sqlite3_exec(db, sql[i], nullptr, nullptr, &errmsg);

        if (rc != SQLITE_OK)
        {
            if (errmsg)
            {
                DBG_Printf(DBG_ERROR_L2, "SQL exec failed: %s, error: %s (%d)\n", sql[i], errmsg, rc);
                sqlite3_free(errmsg);
            }
        }
    }
}

/*! Creates temporary views only valid during this session.
 */
void DeRestPluginPrivate::createTempViews()
{
    int rc;
    char *errmsg;
    DBG_Printf(DBG_INFO, "DB create temporary views\n");

    /* Create SQL statement */
    const char *sql[] = {
        "CREATE TEMP VIEW sensor_device_view "
        "  AS SELECT a.sid, b.mac, b.id FROM sensors a, devices b "
        "  WHERE a.uniqueid like b.mac || '%'",
        "CREATE TEMP VIEW sensor_device_value_view "
        "  AS SELECT a.sid AS sensor_id, b.cluster AS cluster_id, b.data AS data, b.timestamp AS timestamp "
        "  from sensor_device_view a, zcl_values b where a.id = b.device_id "
        "  ORDER BY timestamp ASC ",

        "CREATE TEMP VIEW light_device_view "
        "  AS SELECT a.id as lid, b.mac, b.id FROM nodes a, devices b "
        "  WHERE a.mac like b.mac || '%'",
        "CREATE TEMP VIEW light_device_value_view "
        "  AS SELECT a.lid AS light_id, b.cluster AS cluster_id, b.data AS data, b.timestamp AS timestamp "
        "  from light_device_view a, zcl_values b where a.id = b.device_id "
        "  ORDER BY timestamp ASC ",
        nullptr
    };

    for (int i = 0; sql[i] != NULL; i++)
    {
        errmsg = NULL;

        /* Execute SQL statement */
        rc = sqlite3_exec(db, sql[i], NULL, NULL, &errmsg);

        if (rc != SQLITE_OK)
        {
            if (errmsg)
            {
                DBG_Printf(DBG_ERROR_L2, "SQL exec failed: %s, error: %s (%d)\n", sql[i], errmsg, rc);
                sqlite3_free(errmsg);
            }
        }
        else
        {
            DBG_Printf(DBG_INFO_L2, "DB view [%d] created\n", i);
        }
    }
}

/*! Returns SQLite pragma parameters specified by \p sql.
 */
int DeRestPluginPrivate::getDbPragmaInteger(const char *sql)
{
    int rc;
    int val = -1;
    sqlite3_stmt *res = NULL;

    rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);
    DBG_Assert(rc == SQLITE_OK);
    if (rc == SQLITE_OK) { rc = sqlite3_step(res); }

    DBG_Assert(rc == SQLITE_ROW);
    if (rc == SQLITE_ROW)
    {
        val = sqlite3_column_int(res, 0);
        DBG_Printf(DBG_INFO, "DB %s: %d\n", sql, val);
    }

    DBG_Assert(res != NULL);
    if (res)
    {
        rc = sqlite3_finalize(res);
        DBG_Assert(rc == SQLITE_OK);
    }
    return val;
}

/*! Writes database user_version to \p userVersion. */
bool DeRestPluginPrivate::setDbUserVersion(int userVersion)
{
    int rc;
    char *errmsg;

    DBG_Printf(DBG_INFO, "DB write sqlite user_version %d\n", userVersion);

    const auto sql = QString("PRAGMA user_version = %1").arg(userVersion);

    errmsg = NULL;
    rc = sqlite3_exec(db, qPrintable(sql), NULL, NULL, &errmsg);

    if (rc != SQLITE_OK)
    {
        if (errmsg)
        {
            DBG_Printf(DBG_ERROR_L2, "SQL exec failed: %s, error: %s (%d)\n", qPrintable(sql), errmsg, rc);
            sqlite3_free(errmsg);
        }
        return false;
    }
    return true;
}

/*! Upgrades database to user_version 1. */
bool DeRestPluginPrivate::upgradeDbToUserVersion1()
{
    int rc;
    char *errmsg;
    DBG_Printf(DBG_INFO, "DB upgrade to user_version 1\n");

    // create tables
    const char *sql[] = {
        "CREATE TABLE IF NOT EXISTS auth (apikey TEXT PRIMARY KEY, devicetype TEXT)",
        "CREATE TABLE IF NOT EXISTS userparameter (key TEXT PRIMARY KEY, value TEXT)",
        "CREATE TABLE IF NOT EXISTS nodes (mac TEXT PRIMARY KEY, id TEXT, state TEXT, name TEXT, groups TEXT, endpoint TEXT, modelid TEXT, manufacturername TEXT, swbuildid TEXT)",
        "CREATE TABLE IF NOT EXISTS config2 (key text PRIMARY KEY, value text)",
        "ALTER TABLE nodes add column id TEXT",
        "ALTER TABLE nodes add column state TEXT",
        "ALTER TABLE nodes add column groups TEXT",
        "ALTER TABLE nodes add column endpoint TEXT",
        "ALTER TABLE nodes add column modelid TEXT",
        "ALTER TABLE nodes add column manufacturername TEXT",
        "ALTER TABLE nodes add column swbuildid TEXT",
        "ALTER TABLE nodes add column ritems TEXT",
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
        "ALTER TABLE groups add column uniqueid TEXT",
        "ALTER TABLE scenes add column transitiontime TEXT",
        "ALTER TABLE scenes add column lights TEXT",
        "ALTER TABLE rules add column periodic TEXT",
        "CREATE TABLE IF NOT EXISTS zbconf (conf TEXT)",
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
                DBG_Printf(DBG_ERROR_L2, "SQL exec failed: %s, error: %s (%d)\n", sql[i], errmsg, rc);
                sqlite3_free(errmsg);
            }
        }
    }

    return setDbUserVersion(1);
}

/*! Upgrades database to user_version 2. */
bool DeRestPluginPrivate::upgradeDbToUserVersion2()
{
    int rc;
    char *errmsg;

    DBG_Printf(DBG_INFO, "DB upgrade to user_version 2\n");

    // create tables
    const char *sql[] = {
        "PRAGMA foreign_keys = 1",
        "CREATE TABLE IF NOT EXISTS devices (id INTEGER PRIMARY KEY, mac TEXT UNIQUE, timestamp INTEGER NOT NULL)",

        // zcl_values: table for logging various data
        // zcl_values.data: This field can hold anything (text,integer,blob) since sqlite supports dynamic types on per value level.
        "CREATE TABLE IF NOT EXISTS zcl_values (id INTEGER PRIMARY KEY, device_id INTEGER REFERENCES devices(id) ON DELETE CASCADE, endpoint INTEGER NOT NULL, cluster INTEGER NOT NULL, attribute INTEGER NOT NULL, data INTEGER NOT NULL, timestamp INTEGER NOT NULL)",
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
                DBG_Printf(DBG_ERROR_L2, "SQL exec failed: %s, error: %s (%d)\n", sql[i], errmsg, rc);
                sqlite3_free(errmsg);
            }
            return false;
        }
    }

    return setDbUserVersion(2);
}

/*! Upgrades database to user_version 6. */
bool DeRestPluginPrivate::upgradeDbToUserVersion6()
{
    DBG_Printf(DBG_INFO, "DB upgrade to user_version 6\n");

    // create tables
    const char *sql[] = {
        "DROP TABLE IF EXISTS device_gui", // development version

        "ALTER TABLE devices ADD COLUMN nwk INTEGER",

        // device_descriptors: cache for queried descriptors
        // device_descriptors.data: This field holds the raw descriptor as blob.
        "CREATE TABLE IF NOT EXISTS device_descriptors ("
        " id INTEGER PRIMARY KEY,"
        " device_id INTEGER REFERENCES devices(id) ON DELETE CASCADE,"
        " flags INTEGER NOT NULL DEFAULT 0,"
        " endpoint INTEGER NOT NULL,"
        " type INTEGER NOT NULL,"  // ZDP cluster id which was used to query the descriptor
        " data BLOB NOT NULL,"
        " timestamp INTEGER NOT NULL)",

        "CREATE TABLE if NOT EXISTS device_gui ("
        " id INTEGER PRIMARY KEY,"
        " device_id INTEGER UNIQUE,"
        " flags INTEGER NOT NULL DEFAULT 0,"
        " scene_x REAL NOT NULL,"
        " scene_y REAL NOT NULL,"
        " FOREIGN KEY(device_id) REFERENCES devices(id) ON DELETE CASCADE)",
        nullptr
    };

    for (int i = 0; sql[i] != nullptr; i++)
    {
        char *errmsg = nullptr;
        int rc = sqlite3_exec(db, sql[i], nullptr, nullptr, &errmsg);

        if (rc != SQLITE_OK)
        {
            bool fatalError = true;
            if (errmsg)
            {
                if (strstr(errmsg, "duplicate column name")) // harmless
                {
                    fatalError = false;
                }
                else
                {
                    DBG_Printf(DBG_ERROR_L2, "SQL exec failed: %s, error: %s (%d)\n", sql[i], errmsg, rc);
                }
                sqlite3_free(errmsg);
            }

            if (fatalError)
            {
                return false;
            }
        }
    }

    return setDbUserVersion(6);
}

/*! Upgrades database to user_version 7. */
bool DeRestPluginPrivate::upgradeDbToUserVersion7()
{
    DBG_Printf(DBG_INFO, "DB upgrade to user_version 7\n");

    /*
       The 'source_routes' table references 'devices' so that entries are
       automatically deleted if the destination node is removed.
       Inserting an entry with an existing uuid will automatically replace the old row.

       The 'source_route_hops' table also references 'devices' so that
       entries for a hop get deleted when the respective node is removed.
       In this case the source route entry still exists but the source_routes.hops
       count won't match the number of source_route_hops entries anymore.
     */

    // create tables
    const char *sql[] = {
        "CREATE TABLE IF NOT EXISTS source_routes ("
        " uuid TEXT PRIMARY KEY ON CONFLICT REPLACE,"
        " dest_device_id INTEGER REFERENCES devices(id) ON DELETE CASCADE,"
        " route_order INTEGER NOT NULL,"
        " hops INTEGER NOT NULL,"  // to track number of entries which should be in 'source_route_relays'
        " timestamp INTEGER NOT NULL)",

        "CREATE TABLE if NOT EXISTS source_route_hops ("
        " source_route_uuid TEXT REFERENCES source_routes(uuid) ON DELETE CASCADE,"
        " hop_device_id INTEGER REFERENCES devices(id) ON DELETE CASCADE,"
        " hop INTEGER NOT NULL)",
        nullptr
    };

    for (int i = 0; sql[i] != nullptr; i++)
    {
        char *errmsg = nullptr;
        int rc = sqlite3_exec(db, sql[i], nullptr, nullptr, &errmsg);

        if (rc != SQLITE_OK)
        {
            if (errmsg)
            {
                DBG_Printf(DBG_ERROR_L2, "SQL exec failed: %s, error: %s (%d), line: %d\n", sql[i], errmsg, rc, __LINE__);
                sqlite3_free(errmsg);
            }
            return false;
        }
    }

    return setDbUserVersion(7);
}

#if DECONZ_LIB_VERSION >= 0x010E00
/*! Stores a source route.
    Any existing source route with the same uuid will be replaced automatically.
 */
void DeRestPluginPrivate::storeSourceRoute(const deCONZ::SourceRoute &sourceRoute)
{
    DBG_Assert(sourceRoute.hops().size() > 1);

    if (sourceRoute.hops().size() <= 1)
    {
        return; // at least two hops (incl. destination)
    }

    openDb();
    DBG_Assert(db);
    if (!db)
    {
        return;
    }

    QString sql = QString("INSERT INTO source_routes (uuid,dest_device_id,route_order,hops,timestamp)"
                          " SELECT '%1', (SELECT id FROM devices WHERE mac = '%2'), %3, %4, strftime('%s','now');")
                          .arg(sourceRoute.uuid())
                          .arg(generateUniqueId(sourceRoute.hops().back().ext(), 0, 0))
                          .arg(sourceRoute.order())
                          .arg(sourceRoute.hops().size());

    for (size_t i = 0; i < sourceRoute.hops().size(); i++)
    {
        sql += QString("INSERT INTO source_route_hops (source_route_uuid, hop_device_id, hop)"
                       " SELECT '%1', (SELECT id FROM devices WHERE mac = '%2'), %3;")
                .arg(sourceRoute.uuid())
                .arg(generateUniqueId(sourceRoute.hops().at(i).ext(), 0, 0))
                .arg(i);
    }

    char *errmsg = nullptr;
    int rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

    if (rc != SQLITE_OK)
    {
        if (errmsg)
        {
            DBG_Printf(DBG_ERROR, "DB sqlite3_exec failed: %s, error: %s, line: %d\n", qPrintable(sql), errmsg, __LINE__);
            sqlite3_free(errmsg);
        }
    }

    closeDb();
}

/*! Deletes the source route with \p uuid. */
void DeRestPluginPrivate::deleteSourceRoute(const QString &uuid)
{
    DBG_Assert(!uuid.isEmpty());

    openDb();
    DBG_Assert(db);
    if (!db)
    {
        return;
    }

    char *errmsg = nullptr;
    const auto sql = QString("DELETE FROM source_routes WHERE uuid = '%1'").arg(uuid);
    int rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

    if (rc != SQLITE_OK)
    {
        if (errmsg)
        {
            DBG_Printf(DBG_ERROR, "DB sqlite3_exec failed: %s, error: %s, line: %d\n", qPrintable(sql), errmsg, __LINE__);
            sqlite3_free(errmsg);
        }
    }

    closeDb();
}

/*! Restores and activates all source routes in core. */
void DeRestPluginPrivate::restoreSourceRoutes()
{
    openDb();
    DBG_Assert(db);
    if (!db)
    {
        return;
    }

    const auto loadSourceRoutesCallback = [](void *user, int ncols, char **colval , char **) -> int
    {
        auto *sourceRoutes = static_cast<std::vector<deCONZ::SourceRoute>*>(user);
        DBG_Assert(sourceRoutes);
        DBG_Assert(ncols == 3);
        // TODO verify number of hops in colval[2]
        sourceRoutes->push_back(deCONZ::SourceRoute(colval[0], QString(colval[1]).toInt(), {}));
        return 0;
    };

    char *errmsg = nullptr;
    std::vector<deCONZ::SourceRoute> sourceRoutes;
    const char *sql = "SELECT uuid, route_order, hops FROM source_routes";

    int rc = sqlite3_exec(db, sql, loadSourceRoutesCallback, &sourceRoutes, &errmsg);

    if (rc != SQLITE_OK)
    {
        if (errmsg)
        {
            DBG_Printf(DBG_ERROR, "sqlite3_exec %s, error: %s, line: %d\n", qPrintable(sql), errmsg, __LINE__);
            sqlite3_free(errmsg);
            errmsg = nullptr;
        }
    }

    const auto loadHopsCallback = [](void *user, int ncols, char **colval , char **) -> int
    {
        auto *hops = static_cast<std::vector<deCONZ::Address>*>(user);
        DBG_Assert(hops);
        DBG_Assert(ncols == 2);

        const auto mac = QString("0x%1").arg(colval[0]).remove(':');
        // TODO make use of 'hop' in colval[1]

        bool ok = false;
        deCONZ::Address addr;
        addr.setExt(static_cast<uint64_t>(mac.toULongLong(&ok, 16)));

        if (ok)
        {
            hops->push_back(addr);
        }

        return 0;
    };

    for (auto &sr : sourceRoutes)
    {
        std::vector<deCONZ::Address> hops;
        const auto sql = QString("SELECT mac, hop FROM source_route_hops INNER JOIN devices WHERE hop_device_id = devices.id AND source_route_uuid = '%1';").arg(sr.uuid());

        rc = sqlite3_exec(db, qPrintable(sql), loadHopsCallback, &hops, &errmsg);

        if (rc != SQLITE_OK)
        {
            if (errmsg)
            {
                DBG_Printf(DBG_ERROR, "sqlite3_exec %s, error: %s, line: %d\n", qPrintable(sql), errmsg, __LINE__);
                sqlite3_free(errmsg);
                errmsg = nullptr;
            }
        }
        else if (apsCtrl && hops.size() > 1) // at least two items
        {
            apsCtrl->activateSourceRoute(deCONZ::SourceRoute(sr.uuid(), sr.order(), hops));
        }
    }

    closeDb();
}
#endif // DECONZ_LIB_VERSION >= 0x010E00

/*! Puts a new top level device entry in the db (mac address) or refreshes nwk address.
*/
void DeRestPluginPrivate::refreshDeviceDb(const deCONZ::Address &addr)
{
    if (!addr.hasExt() || !addr.hasNwk())
    {
        return;
    }

    QString sql = QString(QLatin1String(
                              "UPDATE devices SET nwk = %2 WHERE mac = '%1';"
                              "INSERT INTO devices (mac,nwk,timestamp) SELECT '%1', %2, strftime('%s','now') WHERE (SELECT changes() = 0);"))
            .arg(generateUniqueId(addr.ext(), 0, 0)).arg(addr.nwk());
    dbQueryQueue.push_back(sql);

    queSaveDb(DB_QUERY_QUEUE, DB_SHORT_SAVE_DELAY);
}

/*! Push/update a zdp descriptor in the database to cache node data.
  */
void DeRestPluginPrivate::pushZdpDescriptorDb(quint64 extAddress, quint8 endpoint, quint16 type, const QByteArray &data)
{
    DBG_Printf(DBG_INFO_L2, "DB pushZdpDescriptorDb()\n");

    openDb();
    DBG_Assert(db);
    if (!db)
    {
        return;
    }

    // store now to make sure 'devices' table is populated
    if (!dbQueryQueue.empty())
    {
        saveDb();
    }

    qint64 now = QDateTime::currentMSecsSinceEpoch() / 1000;
    const QString uniqueid = generateUniqueId(extAddress, 0, 0);
    char mac[23 + 1];
    strncpy(mac, qPrintable(uniqueid), uniqueid.size());
    mac[23] = '\0';

    // 0) check if exists
    int rc;
    sqlite3_stmt *res = nullptr;
    const char * sql = "SELECT COUNT(*) FROM device_descriptors"
                       " WHERE device_id = (SELECT id FROM devices WHERE mac = ?1)"
                       " AND endpoint = ?2"
                       " AND type = ?3"
                       " AND data = ?4";

    rc = sqlite3_prepare_v2(db, sql, -1, &res, nullptr);
    DBG_Assert(res);
    DBG_Assert(rc == SQLITE_OK);

    if (rc == SQLITE_OK)
    {
        rc = sqlite3_bind_text(res, 1, mac, -1, SQLITE_STATIC);
        DBG_Assert(rc == SQLITE_OK);
    }

    if (rc == SQLITE_OK)
    {
        rc = sqlite3_bind_int(res, 2, endpoint);
        DBG_Assert(rc == SQLITE_OK);
    }

    if (rc == SQLITE_OK)
    {
        rc = sqlite3_bind_int(res, 3, type);
        DBG_Assert(rc == SQLITE_OK);
    }

    if (rc == SQLITE_OK)
    {
        rc = sqlite3_bind_blob(res, 4, data.constData(), data.size(), SQLITE_STATIC);
        DBG_Assert(rc == SQLITE_OK);
    }

    int rows = -1;
    if (rc == SQLITE_OK)
    {
        rc = sqlite3_step(res);
        DBG_Assert(rc == SQLITE_ROW);
        if (rc == SQLITE_ROW)
        {
            rows = sqlite3_column_int(res, 0);
        }
    }

    rc = sqlite3_finalize(res);
    DBG_Assert(rc == SQLITE_OK);

    if (rows != 0) // error or already existing
    {
        return;
    }

    // 1) if exist, try to update existing entry

    sql = "UPDATE device_descriptors SET data = ?1, timestamp = ?2"
          " WHERE device_id = (SELECT id FROM devices WHERE mac = ?3)"
          " AND endpoint = ?4"
          " AND type = ?5";


    rc = sqlite3_prepare_v2(db, sql, -1, &res, nullptr);
    DBG_Assert(res);
    DBG_Assert(rc == SQLITE_OK);

    if (rc == SQLITE_OK)
    {
        rc = sqlite3_bind_blob(res, 1, data.constData(), data.size(), SQLITE_STATIC);
        DBG_Assert(rc == SQLITE_OK);
    }

    if (rc == SQLITE_OK)
    {
        rc = sqlite3_bind_int64(res, 2, now);
        DBG_Assert(rc == SQLITE_OK);
    }

    if (rc == SQLITE_OK)
    {
        rc = sqlite3_bind_text(res, 3, mac, -1, SQLITE_STATIC);
        DBG_Assert(rc == SQLITE_OK);
    }

    if (rc == SQLITE_OK)
    {
        rc = sqlite3_bind_int(res, 4, endpoint);
        DBG_Assert(rc == SQLITE_OK);
    }

    if (rc == SQLITE_OK)
    {
        rc = sqlite3_bind_int(res, 5, type);
        DBG_Assert(rc == SQLITE_OK);
    }

    if (rc != SQLITE_OK)
    {
        DBG_Printf(DBG_INFO, "DB failed %s\n", sqlite3_errmsg(db));
        if (res)
        {
            rc = sqlite3_finalize(res);
            DBG_Assert(rc == SQLITE_OK);
        }
        return;
    }

#if SQLITE_VERSION_NUMBER > 3014000
    auto exp = sqlite3_expanded_sql(res);
    if (exp)
    {
        DBG_Printf(DBG_INFO, "DB %s\n", exp);
        sqlite3_free(exp);
    }
#endif

    int changes = -1;
    rc = sqlite3_step(res);
    if (rc == SQLITE_DONE)
    {
        changes = sqlite3_changes(db);
    }
    DBG_Assert(rc == SQLITE_DONE);

    rc = sqlite3_finalize(res);
    DBG_Assert(rc == SQLITE_OK);

    if (rc != SQLITE_OK)
    {
        return;
    }

    if (changes == 1)
    {
        return; // done updating already existing entry
    }

    // 2) no existing entry, insert new entry
    res = nullptr;
    sql = "INSERT INTO device_descriptors (device_id, endpoint, type, data, timestamp)"
          " SELECT id, ?1, ?2, ?3, ?4"
          " FROM devices WHERE mac = ?5";

    rc = sqlite3_prepare_v2(db, sql, -1, &res, nullptr);
    DBG_Assert(res);
    DBG_Assert(rc == SQLITE_OK);

    if (rc == SQLITE_OK)
    {
        rc = sqlite3_bind_int(res, 1, endpoint);
        DBG_Assert(rc == SQLITE_OK);
    }

    if (rc == SQLITE_OK)
    {
        rc = sqlite3_bind_int(res, 2, type);
        DBG_Assert(rc == SQLITE_OK);
    }

    if (rc == SQLITE_OK)
    {
        rc = sqlite3_bind_blob(res, 3, data.constData(), data.size(), SQLITE_STATIC);
        DBG_Assert(rc == SQLITE_OK);
    }

    if (rc == SQLITE_OK)
    {
        rc = sqlite3_bind_int64(res, 4, now);
        DBG_Assert(rc == SQLITE_OK);
    }

    if (rc == SQLITE_OK)
    {
        rc = sqlite3_bind_text(res, 5, mac, -1, SQLITE_STATIC);
        DBG_Assert(rc == SQLITE_OK);
    }

    if (rc != SQLITE_OK)
    {
        DBG_Printf(DBG_INFO, "DB failed %s\n", sqlite3_errmsg(db));
        if (res)
        {
            rc = sqlite3_finalize(res);
            DBG_Assert(rc == SQLITE_OK);
        }
        return;
    }

#if SQLITE_VERSION_NUMBER > 3014000
    exp = sqlite3_expanded_sql(res);
    if (exp)
    {
        DBG_Printf(DBG_INFO, "DB %s\n", exp);
        sqlite3_free(exp);
    }
#endif

    rc = sqlite3_step(res);
    if (rc == SQLITE_DONE)
    {
        changes = sqlite3_changes(db);
        DBG_Assert(changes == 1);
    }
    rc = sqlite3_finalize(res);
    DBG_Assert(rc == SQLITE_OK);
    closeDb();
}

/*! Push a zcl value sample in the database to keep track of value history.
    The data might be a sensor reading or light state or any ZCL value.
  */
void DeRestPluginPrivate::pushZclValueDb(quint64 extAddress, quint8 endpoint, quint16 clusterId, quint16 attributeId, qint64 data)
{
    if (dbZclValueMaxAge <= 0)
    {
        return; // zcl value datastore disabled
    }

    /*

    select mac, printf('0x%04X', cluster), data, datetime(zcl_values.timestamp,'unixepoch','localtime')
    from zcl_values inner join devices ON zcl_values.device_id = devices.id
    where zcl_values.timestamp > strftime('%s','now') - 300;

    */
    qint64 now = QDateTime::currentMSecsSinceEpoch() / 1000;
    QString sql = QString(QLatin1String(
                              "INSERT INTO zcl_values (device_id,endpoint,cluster,attribute,data,timestamp) "
                              "SELECT id, %2, %3, %4, %5, %6 "
                              "FROM devices WHERE mac = '%1'"))
            .arg(generateUniqueId(extAddress, 0, 0))
            .arg(endpoint)
            .arg(clusterId)
            .arg(attributeId)
            .arg(data)
            .arg(now);

    dbQueryQueue.push_back(sql);
    queSaveDb(DB_QUERY_QUEUE, (dbQueryQueue.size() > 30) ? DB_SHORT_SAVE_DELAY : DB_LONG_SAVE_DELAY);

    // add a cleanup command if not already queued
    for (const QString &q : dbQueryQueue)
    {
        if (q.startsWith(QLatin1String("DELETE FROM zcl_values")))
        {
            return; // already queued
        }
    }

    sql = QString(QLatin1String("DELETE FROM zcl_values WHERE timestamp < %1")).arg(now - dbZclValueMaxAge);
    dbQueryQueue.push_back(sql);
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

    const char *sql = "PRAGMA foreign_keys = ON"; // must be enabled at runtime for each connection
    rc = sqlite3_exec(db, sql, nullptr, nullptr, nullptr);
    DBG_Assert(rc == SQLITE_OK);

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

/*! Sqlite callback to load authorisation data.
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

/*! Loads all authorisation data from database.
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

    if (colval[0] && colval[1] && DBG_IsEnabled(DBG_INFO_L2))
    {
        DBG_Printf(DBG_INFO_L2, "Load config %s: %s from db.\n", colval[0], colval[1]);
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
        // only reload from database if auto reconnect is disabled
        if (!val.isEmpty() && deCONZ::appArgumentNumeric("--auto-connect", 1) == 0)
        {
            int conn = val.toInt(&ok);
            if (ok && ((conn == 0) || (conn == 1)))
            {
                d->gwRfConnectedExpected = (conn == 1);
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
    else if (strcmp(colval[0], "group0") == 0)
    {
        if (!val.isEmpty())
        {
            uint group0 = val.toUInt(&ok);
            if (ok && group0 > 0 && group0 <= 0xfff7) // 0 and larger than 0xfff7 is not valid for Osram Lightify
            {
                d->gwGroup0 = static_cast<quint16>(group0);
                d->gwConfig["group0"] = group0;
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
    else if (strcmp(colval[0], "wifipw") == 0)
    {
        if (!val.isEmpty())
        {
            //d->gwConfig["wifipw"] = val;
            d->gwWifiPw = val;
        }
    }
    else if (strcmp(colval[0], "wifipwenc") == 0)
    {
        if (!val.isEmpty())
        {
            //d->gwConfig["wifipwenc"] = val;
            d->gwWifiPwEnc = val;
        }
    }
    else if (strcmp(colval[0], "workingpwenc") == 0)
    {
        if (!val.isEmpty())
        {
            //d->gwConfig["workingpwenc"] = val;
            d->gwWifiWorkingPwEnc = val;
        }
    }
    else if (strcmp(colval[0], "wifibackuppwenc") == 0)
    {
        if (!val.isEmpty())
        {
            //d->gwConfig["wifibackuppwenc"] = val;
            d->gwWifiBackupPwEnc = val;
        }
    }
    else if (strcmp(colval[0], "wifibackuppw") == 0)
    {
        if (!val.isEmpty())
        {
            //d->gwConfig["wifibackuppw"] = val;
            d->gwWifiBackupPw = val;
        }
    }
    else if (strcmp(colval[0], "wifibackupname") == 0)
    {
        if (!val.isEmpty())
        {
            //d->gwConfig["wifibackupname"] = val;
            d->gwWifiBackupName = val;
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
    else if (strcmp(colval[0], "workingtype") == 0)
    {
        if (!val.isEmpty())
        {
            d->gwConfig["workingtype"] = val;
            d->gwWifiWorkingType = val;
        }
    }
    else if (strcmp(colval[0], "workingname") == 0)
    {
        if (!val.isEmpty())
        {
            d->gwConfig["workingname"] = val;
            d->gwWifiWorkingName = val;
        }
    }
    else if (strcmp(colval[0], "workingpw") == 0)
    {
        if (!val.isEmpty())
        {
            //d->gwConfig["workingpw"] = val;
            d->gwWifiWorkingPw = val;
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
    else if (strcmp(colval[0], "wifilastupdated") == 0)
    {
        if (!val.isEmpty())
        {
            uint lastupdated = val.toUInt(&ok);
            if (ok)
            {
                d->gwConfig["wifilastupdated"] = (uint)lastupdated;
                d->gwWifiLastUpdated = lastupdated;
            }
        }
    }
    else if (strcmp(colval[0], "homebridge") == 0)
    {
        if (!val.isEmpty())
        {
            d->gwConfig["homebridge"] = val;
            d->gwHomebridge = val;
        }
    }
    else if (strcmp(colval[0], "homebridgeversion") == 0)
    {
        if (!val.isEmpty())
        {
            d->gwConfig["homebridgeversion"] = val;
            d->gwHomebridgeVersion = val;
        }
    }
    else if (strcmp(colval[0], "homebridgeupdateversion") == 0)
    {
        if (!val.isEmpty())
        {
            d->gwConfig["homebridgeupdateversion"] = val;
            d->gwHomebridgeUpdateVersion = val;
        }
    }
    else if (strcmp(colval[0], "homebridgeupdate") == 0)
    {
        if (!val.isEmpty())
        {
            if (val == "true")
            {
                d->gwConfig["homebridgeupdate"] = true;
                d->gwHomebridgeUpdate = true;
            }
            else
            {
                d->gwConfig["homebridgeupdate"] = false;
                d->gwHomebridgeUpdate = false;
            }
        }
    }
    else if (strcmp(colval[0], "homebridge-pin") == 0)
    {
        if (!val.isEmpty())
        {
            d->gwConfig["homebridgepin"] = val;
            d->gwHomebridgePin = val;
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
    else if (strcmp(colval[0], "disablePermitJoinAutoOff") == 0)
    {
      if (!val.isEmpty())
      {
          bool v = val == "true";
          d->gwConfig["disablePermitJoinAutoOff"] = v;
          d->gwdisablePermitJoinAutoOff = v;
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
        if (!val.isEmpty())
        {
            d->gwConfig["swupdatestate"] = val;
            d->gwSwUpdateState = val;
        }
    }
    else if (strcmp(colval[0], "zclvaluemaxage") == 0)
    {
        qint64 maxAge = val.toLongLong(&ok);
        if (!val.isEmpty() && ok)
        {
            d->gwConfig["zclvaluemaxage"] = maxAge;
            d->dbZclValueMaxAge = maxAge;
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
        QString sql;
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
            else if (strcmp(colname[i], "uniqueid") == 0)
            {
                ResourceItem *item = 0;
                if (!val.isEmpty())
                {
                    item = group.addItem(DataTypeString, RAttrUniqueId);
                }

                if (item)
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

    if (!rl.data.contains(QLatin1String("description")) || rl.data["description"].toString().isNull())
    {
        rl.data["description"] = QLatin1String("");
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
        DBG_Printf(DBG_INFO_L2, "DB parsed schedule %s\n", qPrintable(schedule.id));
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


/*! Load sensor data from database.
 */
void DeRestPluginPrivate::loadSensorDataFromDb(Sensor *sensor, QVariantList &ls, qint64 fromTime, int max)
{
    DBG_Assert(db);

    if (!db)
    {
        return;
    }

    DBG_Assert(sensor);

    if (!sensor)
    {
        return;
    }

    struct RMap {
        const char *item;
        quint16 clusterId;
        quint16 attributeId;
    };

    const RMap rmap[] = {
        // Item, clusterId, attributeId
        { RStatePresence, 0x0406, 0x0000 },
        { RStatePresence, 0x0500, 0x0000 },
        { RStateLightLevel, 0x0400, 0x0000 },
        { RStateTemperature, 0x0402, 0x0000 },
        { RStateHumidity, 0x0405, 0x0000 },
        { RStateOpen, 0x0006, 0x0000 },
        { RStateOpen, 0x0500, 0x0000 },
        { nullptr, 0, 0 }
    };

    const RMap *r = rmap;

    while (r->item)
    {
        for (int i  = 0; i < sensor->itemCount(); i++)
        {
            ResourceItem *item = sensor->itemForIndex(static_cast<size_t>(i));

            if (r->item != item->descriptor().suffix)
            {
                continue;
            }

            const char *sql = "SELECT data,timestamp FROM sensor_device_value_view "
                              "WHERE sensor_id = ?1 AND timestamp > ?2 AND cluster_id = ?3 limit ?4";

            int rc;
            int sid = sensor->id().toInt();
            sqlite3_stmt *res = nullptr;

            rc = sqlite3_prepare_v2(db, sql, -1, &res, nullptr);
            DBG_Assert(res != nullptr);
            DBG_Assert(rc == SQLITE_OK);

            if (rc == SQLITE_OK)
            {
                rc = sqlite3_bind_int(res, 1, sid);
                DBG_Assert(rc == SQLITE_OK);
            }

            if (rc == SQLITE_OK)
            {
                rc = sqlite3_bind_int64(res, 2, fromTime);
                DBG_Assert(rc == SQLITE_OK);
            }

            if (rc == SQLITE_OK)
            {
                rc = sqlite3_bind_int(res, 3, r->clusterId);
                DBG_Assert(rc == SQLITE_OK);
            }

            if (rc == SQLITE_OK)
            {
                rc = sqlite3_bind_int(res, 4, max);
                DBG_Assert(rc == SQLITE_OK);
            }

            if (rc != SQLITE_OK)
            {
                if (res)
                {
                    rc = sqlite3_finalize(res);
                    DBG_Assert(rc == SQLITE_OK);
                }
                continue;
            }

            while (sqlite3_step(res) == SQLITE_ROW)
            {
                QVariantMap map;
                qint64 val = sqlite3_column_int64(res, 0);
                qint64 timestamp = sqlite3_column_int64(res, 1);

                QDateTime dateTime;
                dateTime.setMSecsSinceEpoch(timestamp * 1000);
                map[item->descriptor().suffix] = val;
                map["t"] = dateTime.toString(QLatin1String("yyyy-MM-ddTHH:mm:ss"));
                ls.append(map);
            }

            rc = sqlite3_finalize(res);
            DBG_Assert(rc == SQLITE_OK);
        }
        r++;
    }
}

/*! Load light from database.
 */
void DeRestPluginPrivate::loadLightDataFromDb(LightNode *lightNode, QVariantList &ls, qint64 fromTime, int max)
{
    DBG_Assert(db);

    if (!db)
    {
        return;
    }

    DBG_Assert(lightNode);

    if (!lightNode)
    {
        return;
    }

    struct RMap {
        const char *item;
        quint16 clusterId;
        quint16 attributeId;
    };

    const RMap rmap[] = {
        // Item, clusterId, attributeId
        { RStateOn, 0x0006, 0x0000 },
        { RStateLightLevel, 0x0008, 0x0000 },
        { nullptr, 0, 0 }
    };

    for (int i  = 0; i < lightNode->itemCount(); i++)
    {
        ResourceItem *item = lightNode->itemForIndex(i);
        const RMap *found = nullptr;
        const RMap *r = rmap;

        while (!found && r->item)
        {
            if (r->item == item->descriptor().suffix)
            {
              found = r;
              break;
            }
            r++;
        }

        if (!found)
        {
            continue;
        }

        const char *sql = "SELECT data,timestamp FROM light_device_value_view "
                          "WHERE light_id = ?1 AND timestamp > ?2 AND cluster_id = ?3 limit ?4";

        int rc;
        int sid = lightNode->id().toInt();
        sqlite3_stmt *res = nullptr;

        rc = sqlite3_prepare_v2(db, sql, -1, &res, 0);
        DBG_Assert(res != nullptr);
        DBG_Assert(rc == SQLITE_OK);

        if (rc == SQLITE_OK)
        {
            rc = sqlite3_bind_int(res, 1, sid);
            DBG_Assert(rc == SQLITE_OK);
        }

        if (rc == SQLITE_OK)
        {
            rc = sqlite3_bind_int(res, 2, fromTime);
            DBG_Assert(rc == SQLITE_OK);
        }

        if (rc == SQLITE_OK)
        {
            rc = sqlite3_bind_int(res, 3, found->clusterId);
            DBG_Assert(rc == SQLITE_OK);
        }

        // TODO zcl attribute

        if (rc == SQLITE_OK)
        {
            rc = sqlite3_bind_int(res, 4, max);
            DBG_Assert(rc == SQLITE_OK);
        }

        if (rc != SQLITE_OK)
        {
            if (res)
            {
                rc = sqlite3_finalize(res);
                DBG_Assert(rc == SQLITE_OK);
            }
            continue;
        }

        while (sqlite3_step(res) == SQLITE_ROW)
        {
            QVariantMap map;
            qint64 val = sqlite3_column_int64(res, 0);
            qint64 timestamp = sqlite3_column_int64(res, 1);

            QDateTime dateTime;
            dateTime.setMSecsSinceEpoch(timestamp * 1000);
            map[item->descriptor().suffix] = val;
            map["t"] = dateTime.toString(QLatin1String("yyyy-MM-ddTHH:mm:ss"));
            ls.append(map);
        }

        rc = sqlite3_finalize(res);
        DBG_Assert(rc == SQLITE_OK);
    }
}

/*! Sqlite callback to load data for a node (identified by its mac address).
 */
static int sqliteLoadLightNodeCallback(void *user, int ncols, char **colval , char **colname)
{
    DBG_Assert(user);

    if (!user || (ncols <= 0))
    {
        return 0;
    }

    DB_Callback *cb = static_cast<DB_Callback*>(user);
    LightNode *lightNode = cb->lightNode;

    DBG_Assert(cb);
    DBG_Assert(cb->d);
    DBG_Assert(lightNode);

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
                    cb->d->setLightNodeStaticCapabilities(lightNode);
                }
            }
            else if (strcmp(colname[i], "manufacturername") == 0)
            {
                if (!val.isEmpty() && 0 != val.compare(QLatin1String("Unknown"), Qt::CaseInsensitive))
                {
                    lightNode->setManufacturerName(val);
                    lightNode->clearRead(READ_VENDOR_NAME);
                    cb->d->setLightNodeStaticCapabilities(lightNode);
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
                if (val == QLatin1String("deleted"))
                {
                    lightNode->setState(LightNode::StateDeleted);
                }
                else
                {
                    lightNode->setState(LightNode::StateNormal);
                }
            }
            else if (strcmp(colname[i], "ritems") == 0 && !val.isEmpty())
            {
                lightNode->jsonToResourceItems(val);
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

/*! Loads data (if available) for a LightNode from the database according to the adress
 */
QString DeRestPluginPrivate::loadDataForLightNodeFromDb(QString extAddress)
{
    QString result;
    DBG_Assert(db != nullptr);

    if (!db || extAddress.isEmpty())
    {
        return result;
    }

    QString sql = QString("SELECT manufacturername FROM nodes WHERE mac LIKE '%1%' COLLATE NOCASE").arg(extAddress);
    DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));

    const char * val = nullptr;
    sqlite3_stmt *res = nullptr;
    int rc;

    rc = sqlite3_prepare_v2(db, qPrintable(sql), -1, &res, nullptr);
    if (rc == SQLITE_OK)
    {
		rc = sqlite3_step(res);
	}

    if (rc == SQLITE_ROW)
    {
        val = reinterpret_cast<const char*>(sqlite3_column_text(res, 0));
        if (val)
        {
            result = val;
            DBG_Printf(DBG_INFO, "DB %s: %s\n", qPrintable(sql), qPrintable(val));
        }
    }

    if (res)
    {
        rc = sqlite3_finalize(res);
    }

    return result;
}

/*! Loads data (if available) for a LightNode from the database.
 */
void DeRestPluginPrivate::loadLightNodeFromDb(LightNode *lightNode)
{
    int rc;
    char *errmsg = nullptr;

    DBG_Assert(db != nullptr);
    DBG_Assert(lightNode != nullptr);

    if (!db || !lightNode)
    {
        return;
    }

    // check for new uniqueId format
    QString sql = QString("SELECT * FROM nodes WHERE mac='%1' COLLATE NOCASE AND state != 'deleted'").arg(lightNode->uniqueId());

    DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));

    DB_Callback cb;
    cb.d = this;
    cb.lightNode = lightNode;

    rc = sqlite3_exec(db, qPrintable(sql), sqliteLoadLightNodeCallback, &cb, &errmsg);

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
        sql = QString("SELECT * FROM nodes WHERE mac='%1' COLLATE NOCASE AND state != 'deleted'").arg(lightNode->address().toStringExt());

        DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
        rc = sqlite3_exec(db, qPrintable(sql), sqliteLoadLightNodeCallback, &cb, &errmsg);

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

    QString gid = QString("%1").arg(group->address(), 4, 16, QLatin1Char('0'));
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

    QString gsid = "0x" + QString("%1%2")
                       .arg(scene->groupAddress, 4, 16, QLatin1Char('0'))
                       .arg(scene->id, 2, 16, QLatin1Char('0')).toUpper(); // unique key

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
    QDateTime startTime = QDateTime::currentDateTimeUtc();
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
                if (val == QLatin1String("deleted"))
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
        ResourceItem *item = nullptr;
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
            const QStringList ls = sensor.uniqueId().split('-', QString::SkipEmptyParts);
            if (ls.size() == 2 && ls[1] == QLatin1String("f2"))
            {
                // Green Power devices, e.g. ZGPSwitch
            }
            else if (ls.size() != 3)
            {
                return 0;
            }

            QString mac = ls[0]; // need copy
            mac.remove(':'); // inplace remove
            extAddr = mac.toULongLong(&ok, 16);

            if (!ok)
            {
                return 0;
            }

            // restore clusterId
            if (ls.size() == 3)
            {
                clusterId = ls[2].toUShort(&ok, 16);

                if (!ok)
                {
                    return 0;
                }
            }
        }

        if (!isClip && extAddr == 0)
        {
            return 0;
        }

        if (sensor.type().endsWith(QLatin1String("Switch")))
        {
            if (sensor.modelId().startsWith(QLatin1String("SML00"))) // Hue motion sensor
            {
                // not supported yet, created by older versions
                // ignore for now
                return 0;
            }

            if (sensor.fingerPrint().hasInCluster(COMMISSIONING_CLUSTER_ID))
            {
                clusterId = clusterId ? clusterId : COMMISSIONING_CLUSTER_ID;
            }

            if (sensor.fingerPrint().hasOutCluster(ONOFF_CLUSTER_ID))
            {
                clusterId = clusterId ? clusterId : ONOFF_CLUSTER_ID;
                if (!sensor.modelId().startsWith(QLatin1String("SYMFONISK")))
                {
                    sensor.addItem(DataTypeString, RConfigGroup);
                }
            }
            else if (sensor.fingerPrint().hasInCluster(ONOFF_CLUSTER_ID))
            {
                clusterId = clusterId ? clusterId : ONOFF_CLUSTER_ID;
            }
            else if (sensor.fingerPrint().hasInCluster(ANALOG_INPUT_CLUSTER_ID))
            {
                clusterId = clusterId ? clusterId : ANALOG_INPUT_CLUSTER_ID;
            }
            else if (sensor.fingerPrint().hasInCluster(DOOR_LOCK_CLUSTER_ID))
            {
                clusterId = clusterId ? clusterId : DOOR_LOCK_CLUSTER_ID;
            }
            else if (sensor.fingerPrint().hasInCluster(MULTISTATE_INPUT_CLUSTER_ID))
            {
                clusterId = clusterId ? clusterId : MULTISTATE_INPUT_CLUSTER_ID;
            }
            else if (sensor.fingerPrint().hasOutCluster(IAS_ACE_CLUSTER_ID))
            {
                clusterId = clusterId ? clusterId : IAS_ACE_CLUSTER_ID;
            }
            item = sensor.addItem(DataTypeInt32, RStateButtonEvent);
            item->setValue(0);

            if (sensor.modelId().startsWith(QLatin1String("lumi.sensor_cube")))
            {
                sensor.addItem(DataTypeInt32, RStateGesture);
            }
            else if (sensor.modelId().startsWith(QLatin1String("RWL02")) ||
                     sensor.modelId().startsWith(QLatin1String("ROM00")) ||
                     sensor.modelId().startsWith(QLatin1String("Z3-1BRL")))
            {
                sensor.addItem(DataTypeUInt16, RStateEventDuration);
            }
            else if (sensor.modelId().startsWith(QLatin1String("ZBT-Remote-ALL-RGBW")))
            {
                sensor.addItem(DataTypeUInt16, RStateX);
                sensor.addItem(DataTypeUInt16, RStateY);
                sensor.addItem(DataTypeUInt16, RStateAngle);
            }
        }
        else if (sensor.type().endsWith(QLatin1String("LightLevel")))
        {
            if (sensor.fingerPrint().hasInCluster(ILLUMINANCE_MEASUREMENT_CLUSTER_ID))
            {
                clusterId = clusterId ? clusterId : ILLUMINANCE_MEASUREMENT_CLUSTER_ID;
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
                clusterId = clusterId ? clusterId : TEMPERATURE_MEASUREMENT_CLUSTER_ID;
            }
            item = sensor.addItem(DataTypeInt16, RStateTemperature);
            item->setValue(0);
            item = sensor.addItem(DataTypeInt16, RConfigOffset);
            item->setValue(0);
        }
        else if (sensor.type().endsWith(QLatin1String("AirQuality")))
        {
            if (sensor.fingerPrint().hasInCluster(BOSCH_AIR_QUALITY_CLUSTER_ID))
            {
                clusterId = clusterId ? clusterId : BOSCH_AIR_QUALITY_CLUSTER_ID;
            }
            else if (sensor.fingerPrint().hasInCluster(0xFC03))  // Develco air quality sensor
            {
                clusterId = clusterId ? clusterId : 0xFC03;
            }
            item = sensor.addItem(DataTypeString, RStateAirQuality);
            item = sensor.addItem(DataTypeUInt16, RStateAirQualityPpb);
        }
        else if (sensor.type().endsWith(QLatin1String("Spectral")))
        {
            if (sensor.fingerPrint().hasInCluster(VENDOR_CLUSTER_ID))
            {
                clusterId = VENDOR_CLUSTER_ID;
            }
            item = sensor.addItem(DataTypeUInt16, RStateSpectralX);
            item->setValue(0);
            item = sensor.addItem(DataTypeUInt16, RStateSpectralY);
            item->setValue(0);
            item = sensor.addItem(DataTypeUInt16, RStateSpectralZ);
            item->setValue(0);
        }
        else if (sensor.type().endsWith(QLatin1String("Tuya")))
        {
            clusterId = clusterId ? clusterId : TUYA_CLUSTER_ID;

            item = sensor.addItem(DataTypeInt16, RStateTemperature);
            item->setValue(0);
            item = sensor.addItem(DataTypeUInt16, RStateHumidity);
            item->setValue(0);
            item = sensor.addItem(DataTypeBool, RStateAlarm);
            item->setValue(false);
        }
        else if (sensor.type().endsWith(QLatin1String("Humidity")))
        {
            if (sensor.fingerPrint().hasInCluster(RELATIVE_HUMIDITY_CLUSTER_ID))
            {
                clusterId = clusterId ? clusterId : RELATIVE_HUMIDITY_CLUSTER_ID;
            }
            item = sensor.addItem(DataTypeUInt16, RStateHumidity);
            item->setValue(0);
            item = sensor.addItem(DataTypeInt16, RConfigOffset);
            item->setValue(0);
        }
        else if (sensor.type().endsWith(QLatin1String("Pressure")))
        {
            if (sensor.fingerPrint().hasInCluster(PRESSURE_MEASUREMENT_CLUSTER_ID))
            {
                clusterId = clusterId ? clusterId : PRESSURE_MEASUREMENT_CLUSTER_ID;
            }
            item = sensor.addItem(DataTypeInt16, RStatePressure);
            item->setValue(0);
        }
        else if (sensor.type().endsWith(QLatin1String("Presence")))
        {
            if (sensor.fingerPrint().hasInCluster(OCCUPANCY_SENSING_CLUSTER_ID))
            {
                clusterId = clusterId ? clusterId : OCCUPANCY_SENSING_CLUSTER_ID;
                if (sensor.modelId().startsWith(QLatin1String("FLS")) ||
                    sensor.modelId().startsWith(QLatin1String("SML00")))
                {
                    // TODO write and recover min/max to db
                    deCONZ::NumericUnion dummy;
                    dummy.u64 = 0;
                    sensor.setZclValue(NodeValue::UpdateInvalid, sensor.fingerPrint().endpoint, clusterId, 0x0000, dummy);
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
                clusterId = clusterId ? clusterId : IAS_ZONE_CLUSTER_ID;
            }
            else if (sensor.fingerPrint().hasInCluster(BINARY_INPUT_CLUSTER_ID))
            {
                clusterId = clusterId ? clusterId : BINARY_INPUT_CLUSTER_ID;
            }
            else if (sensor.fingerPrint().hasInCluster(ONOFF_CLUSTER_ID))
            {
                clusterId = clusterId ? clusterId : ONOFF_CLUSTER_ID;
            }
            item = sensor.addItem(DataTypeBool, RStatePresence);
            item->setValue(false);
            if (sensor.modelId().startsWith(QLatin1String("SML00"))) // Hue motion sensor
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
                if (sensor.modelId().startsWith(QLatin1String("tagv4"))) // SmartThings Arrival sensor
                {
                    item->setValue(310);
                }
                else if (sensor.modelId().startsWith(QLatin1String("lumi.sensor_motion")))
                {
                    item->setValue(90);
                }
                else
                {
                    item->setValue(60); // presence should be reasonable for physical sensors
                }
            }

            if (sensor.modelId() == QLatin1String("TY0202"))
            {
                sensor.setManufacturer(QLatin1String("SILVERCREST"));
            }
        }
        else if (sensor.type().endsWith(QLatin1String("Flag")))
        {
            item = sensor.addItem(DataTypeBool, RStateFlag);
            item->setValue(false);
            item = sensor.item(RStateLastUpdated);
            item->setValue(startTime);
        }
        else if (sensor.type().endsWith(QLatin1String("Status")))
        {
            item = sensor.addItem(DataTypeInt32, RStateStatus);
            item->setValue(0);
            item = sensor.item(RStateLastUpdated);
            item->setValue(startTime);
        }
        else if (sensor.type().endsWith(QLatin1String("OpenClose")))
        {
            if (sensor.fingerPrint().hasInCluster(IAS_ZONE_CLUSTER_ID))
            {
                clusterId = clusterId ? clusterId : IAS_ZONE_CLUSTER_ID;
            }
            else if (sensor.fingerPrint().hasInCluster(ONOFF_CLUSTER_ID))
            {
                clusterId = clusterId ? clusterId : ONOFF_CLUSTER_ID;
            }
            item = sensor.addItem(DataTypeBool, RStateOpen);
            item->setValue(false);

            if (sensor.modelId() == QLatin1String("TY0203"))
            {
                sensor.setManufacturer(QLatin1String("SILVERCREST"));
            }
        }
        else if (sensor.type().endsWith(QLatin1String("Alarm")))
        {
            if (sensor.fingerPrint().hasInCluster(IAS_ZONE_CLUSTER_ID))
            {
                clusterId = clusterId ? clusterId : IAS_ZONE_CLUSTER_ID;
            }
            item = sensor.addItem(DataTypeBool, RStateAlarm);
            item->setValue(false);
        }
        else if (sensor.type().endsWith(QLatin1String("CarbonMonoxide")))
        {
            if (sensor.fingerPrint().hasInCluster(IAS_ZONE_CLUSTER_ID))
            {
                clusterId = clusterId ? clusterId : IAS_ZONE_CLUSTER_ID;
            }
            item = sensor.addItem(DataTypeBool, RStateCarbonMonoxide);
            item->setValue(false);
        }
        else if (sensor.type().endsWith(QLatin1String("Fire")))
        {
            if (sensor.fingerPrint().hasInCluster(IAS_ZONE_CLUSTER_ID))
            {
                clusterId = clusterId ? clusterId : IAS_ZONE_CLUSTER_ID;
            }
            item = sensor.addItem(DataTypeBool, RStateFire);
            item->setValue(false);
        }
        else if (sensor.type().endsWith(QLatin1String("Vibration")))
        {
            if (sensor.fingerPrint().hasInCluster(IAS_ZONE_CLUSTER_ID))
            {
                clusterId = clusterId ? clusterId : IAS_ZONE_CLUSTER_ID;
            }
            else if (sensor.fingerPrint().hasInCluster(DOOR_LOCK_CLUSTER_ID))
            {
                clusterId = clusterId ? clusterId : DOOR_LOCK_CLUSTER_ID;
            }
            else if (sensor.fingerPrint().hasInCluster(SAMJIN_CLUSTER_ID))
            {
                clusterId = clusterId ? clusterId : SAMJIN_CLUSTER_ID;
                item = sensor.addItem(DataTypeInt16, RStateOrientationX);
                item = sensor.addItem(DataTypeInt16, RStateOrientationY);
                item = sensor.addItem(DataTypeInt16, RStateOrientationZ);
            }
            item = sensor.addItem(DataTypeBool, RStateVibration);
            item->setValue(false);
            if (sensor.modelId().startsWith(QLatin1String("lumi.vibration")))
            {
                item = sensor.addItem(DataTypeInt16, RStateOrientationX);
                item = sensor.addItem(DataTypeInt16, RStateOrientationY);
                item = sensor.addItem(DataTypeInt16, RStateOrientationZ);
                item = sensor.addItem(DataTypeUInt16, RStateTiltAngle);
                item = sensor.addItem(DataTypeUInt16, RStateVibrationStrength);
            }
        }
        else if (sensor.type().endsWith(QLatin1String("Water")))
        {
            if (sensor.fingerPrint().hasInCluster(IAS_ZONE_CLUSTER_ID))
            {
                clusterId = clusterId ? clusterId : IAS_ZONE_CLUSTER_ID;
            }
            else if (sensor.fingerPrint().hasInCluster(APPLIANCE_EVENTS_AND_ALERTS_CLUSTER_ID))
            {
                clusterId = clusterId ? clusterId : APPLIANCE_EVENTS_AND_ALERTS_CLUSTER_ID;
            }
            item = sensor.addItem(DataTypeBool, RStateWater);
            item->setValue(false);
        }
        else if (sensor.type().endsWith(QLatin1String("Consumption")))
        {
            if (sensor.fingerPrint().hasInCluster(METERING_CLUSTER_ID))
            {
                clusterId = clusterId ? clusterId : METERING_CLUSTER_ID;
                if ((sensor.modelId() != QLatin1String("SP 120")) &&
                    (sensor.modelId() != QLatin1String("ZB-ONOFFPlug-D0005")) &&
                    (sensor.modelId() != QLatin1String("TS0121")) &&
                    (!sensor.modelId().startsWith(QLatin1String("BQZ10-AU"))) &&
                    (!sensor.modelId().startsWith(QLatin1String("ROB_200"))) &&
                    (!sensor.modelId().startsWith(QLatin1String("lumi.plug.ma"))) &&
                    (sensor.modelId() != QLatin1String("Plug-230V-ZB3.0")) &&
                    (sensor.modelId() != QLatin1String("lumi.switch.b1naus01")) &&
                    (sensor.modelId() != QLatin1String("Connected socket outlet")) &&
                    (!sensor.modelId().startsWith(QLatin1String("SPW35Z"))))
                {
                    item = sensor.addItem(DataTypeInt16, RStatePower);
                    item->setValue(0);
                }
            }
            else if (sensor.fingerPrint().hasInCluster(ANALOG_INPUT_CLUSTER_ID))
            {
                clusterId = clusterId ? clusterId : ANALOG_INPUT_CLUSTER_ID;
            }
            if (sensor.modelId() != QLatin1String("160-01"))
            {
                item = sensor.addItem(DataTypeUInt64, RStateConsumption);
                item->setValue(0);
            }
        }
        else if (sensor.type().endsWith(QLatin1String("Power")))
        {
            bool hasVoltage = true;
            if (sensor.fingerPrint().hasInCluster(ELECTRICAL_MEASUREMENT_CLUSTER_ID))
            {
                clusterId = clusterId ? clusterId : ELECTRICAL_MEASUREMENT_CLUSTER_ID;
                if (sensor.modelId().startsWith(QLatin1String("Plug")) && sensor.manufacturer() == QLatin1String("OSRAM")) // OSRAM
                {
                    DBG_Printf(DBG_INFO, "OSRAM %s: ZHAPower sensor id: %s ignored loading from database\n", qPrintable(sensor.modelId()), qPrintable(sensor.id()));
                    return 0;
                    // hasVoltage = false;
                }
                else if (sensor.modelId() == QLatin1String("ZB-ONOFFPlug-D0005") ||
                         sensor.modelId() == QLatin1String("Plug-230V-ZB3.0") ||
                         sensor.modelId() == QLatin1String("lumi.switch.b1naus01"))
                {
                    hasVoltage = false;
                }
            }
            else if (sensor.fingerPrint().hasInCluster(ANALOG_INPUT_CLUSTER_ID))
            {
                clusterId = clusterId ? clusterId : ANALOG_INPUT_CLUSTER_ID;
                if (!sensor.modelId().startsWith(QLatin1String("lumi.plug.mm"))) // Only available for new ZB3.0 Mi smart plugs?
                {
                    hasVoltage = false;
                }
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
            item = sensor.addItem(DataTypeInt8, RConfigSunsetOffset);
            item->setValue(-30);
            sensor.addItem(DataTypeString, RConfigLat);
            sensor.addItem(DataTypeString, RConfigLong);
            sensor.addItem(DataTypeBool, RStateDaylight);
            sensor.addItem(DataTypeBool, RStateDark);
            sensor.addItem(DataTypeInt32, RStateStatus);
        }
        else if (sensor.type().endsWith(QLatin1String("Thermostat")))
        {
            if (sensor.fingerPrint().hasInCluster(THERMOSTAT_CLUSTER_ID) || sensor.fingerPrint().hasInCluster(TUYA_CLUSTER_ID))
            {
                clusterId = THERMOSTAT_CLUSTER_ID;
            }

            //only for legrand cluster. Add only mode field.
            if ( (sensor.fingerPrint().hasInCluster(LEGRAND_CONTROL_CLUSTER_ID)) &&
                 (sensor.modelId() == QLatin1String("Cable outlet") ) )
            {
                clusterId = LEGRAND_CONTROL_CLUSTER_ID;
                sensor.addItem(DataTypeString, RConfigMode);
            }
            else
            {
                item = sensor.addItem(DataTypeInt16, RStateTemperature);
                item->setValue(0);
                item = sensor.addItem(DataTypeInt16, RConfigOffset);
                item->setValue(0);
                sensor.addItem(DataTypeInt16, RConfigHeatSetpoint);    // Heating set point
                sensor.addItem(DataTypeBool, RStateOn);                // Heating on/off

                if (sensor.modelId().startsWith(QLatin1String("SLR2")) ||   // Hive
                    sensor.modelId() == QLatin1String("SLR1b") ||           // Hive
                    sensor.modelId().startsWith(QLatin1String("TH112")) ||  // Sinope
                    sensor.modelId() == QLatin1String("GbxAXL2") ||         // Tuya
                    sensor.modelId() == QLatin1String("kud7u2l") ||         // Tuya
                    sensor.modelId() == QLatin1String("902010/32") ||       // Bitron
                   (sensor.manufacturer() == QLatin1String("_TZE200_ckud7u2l")) ||          // Tuya
                   (sensor.manufacturer() == QLatin1String("_TZE200_aoclfnxz")))            // Tuya
                {
                    sensor.addItem(DataTypeString, RConfigMode);
                }

                if (sensor.modelId() == QLatin1String("kud7u2l") || // Tuya
                    sensor.modelId() == QLatin1String("GbxAXL2") || // Tuya
                   (sensor.manufacturer() == QLatin1String("_TZE200_ckud7u2l")) )   // Tuya
                {
                    sensor.addItem(DataTypeUInt8, RStateValve);
                    item = sensor.addItem(DataTypeBool, RStateLowBattery);
                    item->setValue(false);
                }

                if (sensor.modelId() == QLatin1String("kud7u2l") || // Tuya
                    sensor.modelId() == QLatin1String("eaxp72v") || // Tuya
                    sensor.modelId() == QLatin1String("fvq6avy") || // Tuya
                    sensor.modelId() == QLatin1String("88teujp") || // Tuya
                   (sensor.manufacturer() == QLatin1String("_TZE200_c88teujp")) ||  // Tuya
                   (sensor.manufacturer() == QLatin1String("_TZE200_ckud7u2l")) ||  // Tuya
                   (sensor.manufacturer() == QLatin1String("_TZE200_aoclfnxz")) )   // Tuya
                {
                    sensor.addItem(DataTypeString, RConfigPreset);
                    sensor.addItem(DataTypeBool, RConfigLocked);
                    sensor.addItem(DataTypeBool, RConfigSetValve);
                }

                if (sensor.modelId() == QLatin1String("kud7u2l") || // Tuya
                    sensor.modelId() == QLatin1String("fvq6avy") || // Tuya
                    sensor.modelId() == QLatin1String("88teujp") || // Tuya
                   (sensor.manufacturer() == QLatin1String("_TZE200_c88teujp")) ||  // Tuya
                   (sensor.manufacturer() == QLatin1String("_TZE200_ckud7u2l")) ||  // Tuya
                   (sensor.manufacturer() == QLatin1String("_TZE200_aoclfnxz")) )   // Tuya
                {
                    sensor.addItem(DataTypeString, RConfigSchedule);
                }

                if (sensor.modelId() == QLatin1String("kud7u2l") || // Tuya
                    sensor.modelId() == QLatin1String("eaxp72v") || // Tuya
                    sensor.modelId() == QLatin1String("88teujp") || // Tuya
                    sensor.modelId() == QLatin1String("fvq6avy") || // Tuya
                   (sensor.manufacturer() == QLatin1String("_TZE200_c88teujp")) ||  // Tuya
                   (sensor.manufacturer() == QLatin1String("_TZE200_ckud7u2l")) )   // Tuya
                {
                    sensor.addItem(DataTypeBool, RConfigWindowOpen);
                }

                if (sensor.modelId().startsWith(QLatin1String("SPZB"))) // Eurotronic Spirit
                {
                    sensor.addItem(DataTypeUInt8, RStateValve);
                    sensor.addItem(DataTypeUInt32, RConfigHostFlags); // hidden
                    sensor.addItem(DataTypeBool, RConfigDisplayFlipped);
                    sensor.addItem(DataTypeBool, RConfigLocked);
                    sensor.addItem(DataTypeString, RConfigMode);
                }
                else if (sensor.modelId() == QLatin1String("Super TR"))   // ELKO
                {
                    sensor.addItem(DataTypeString, RConfigTemperatureMeasurement);
                    sensor.addItem(DataTypeInt16, RStateFloorTemperature);
                    sensor.addItem(DataTypeBool, RStateHeating);
                    sensor.addItem(DataTypeBool, RConfigLocked);
                    sensor.addItem(DataTypeString, RConfigMode);
                }
                else if (sensor.modelId() == QLatin1String("Thermostat")) // ecozy
                {
                    sensor.addItem(DataTypeUInt8, RStateValve);
                    sensor.addItem(DataTypeString, RConfigSchedule);
                    sensor.addItem(DataTypeBool, RConfigScheduleOn);
                    sensor.addItem(DataTypeInt16, RConfigLastChangeAmount);
                    sensor.addItem(DataTypeUInt8, RConfigLastChangeSource);
                    sensor.addItem(DataTypeTime, RConfigLastChangeTime);
                }
                else if (sensor.modelId() == QLatin1String("SORB")) // Stelpro Orleans Fan
                {
                    sensor.addItem(DataTypeInt16, RConfigCoolSetpoint);
                    sensor.addItem(DataTypeUInt8, RStateValve);
                    sensor.addItem(DataTypeBool, RConfigLocked);
                    sensor.addItem(DataTypeString, RConfigMode);
                }
                else if (sensor.modelId() == QLatin1String("Zen-01"))
                {
                    sensor.addItem(DataTypeInt16, RConfigCoolSetpoint);
                    sensor.addItem(DataTypeString, RConfigMode);
                    sensor.addItem(DataTypeString, RConfigFanMode);
                }
                else if (sensor.modelId() == QLatin1String("3157100"))
                {
                    sensor.addItem(DataTypeInt16, RConfigCoolSetpoint);
                    sensor.addItem(DataTypeBool, RConfigLocked);
                    sensor.addItem(DataTypeString, RConfigMode);
                    sensor.addItem(DataTypeString, RConfigFanMode);
                }
                else if ((sensor.modelId() == QLatin1String("eTRV0100")) || // Danfoss Ally
                         (sensor.modelId() == QLatin1String("TRV001")) )    // Hive TRV
                {
                    sensor.addItem(DataTypeUInt8, RStateValve);
                    sensor.addItem(DataTypeString, RStateWindowOpen);
                    sensor.addItem(DataTypeBool, RStateMountingModeActive);
                    sensor.addItem(DataTypeString, RStateErrorCode);
                    sensor.addItem(DataTypeBool, RConfigDisplayFlipped);
                    sensor.addItem(DataTypeBool, RConfigLocked);
                    sensor.addItem(DataTypeBool, RConfigMountingMode);
                }
                else if (sensor.modelId() == QLatin1String("AC201")) // OWON AC201 Thermostat
                {
                    sensor.addItem(DataTypeInt16, RConfigCoolSetpoint);
                    sensor.addItem(DataTypeString, RConfigMode);
                    sensor.addItem(DataTypeString, RConfigFanMode);
                    sensor.addItem(DataTypeString, RConfigSwingMode);
                }
                else
                {
                    if (!sensor.modelId().isEmpty())
                    {
                        sensor.addItem(DataTypeBool, RConfigScheduleOn);
                        sensor.addItem(DataTypeString, RConfigSchedule);
                    }
                }
            }
        }
        else if (sensor.type().endsWith(QLatin1String("Battery")))
        {
            if (sensor.fingerPrint().hasInCluster(POWER_CONFIGURATION_CLUSTER_ID))
            {
                clusterId = POWER_CONFIGURATION_CLUSTER_ID;
            }
            if (sensor.manufacturer() == QLatin1String("_TYST11_xu1rkty3"))
            {
                clusterId = TUYA_CLUSTER_ID;
            }
            item = sensor.addItem(DataTypeUInt8, RStateBattery);
            item->setValue(100);
        }
        else if (sensor.type() == QLatin1String("CLIPDaylightOffset"))
        {
            sensor.addItem(DataTypeInt16, RConfigOffset);
            sensor.addItem(DataTypeString, RConfigMode);
            sensor.addItem(DataTypeTime, RStateLocaltime);
        }
        else if (sensor.type().endsWith(QLatin1String("Time")))
        {
            sensor.addItem(DataTypeTime, RStateUtc);
            sensor.addItem(DataTypeTime, RStateLocaltime);
            sensor.addItem(DataTypeTime, RStateLastSet);
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
        else if (sensor.modelId().startsWith(QLatin1String("ROM00"))) // Hue smart button
        {
            clusterId = VENDOR_CLUSTER_ID;
            endpoint = 1;

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
        else if (sensor.modelId().startsWith(QLatin1String("SML00"))) // Hue motion sensor
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
        else if (sensor.modelId().startsWith(QLatin1String("TRADFRI")) ||
                 sensor.modelId().startsWith(QLatin1String("SYMFONISK")))
        {
            sensor.setManufacturer(QLatin1String("IKEA of Sweden"));

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
            if (!sensor.modelId().startsWith(QLatin1String("lumi.ctrl_")) &&
                !sensor.modelId().startsWith(QLatin1String("lumi.plug")) &&
                sensor.modelId() != QLatin1String("lumi.curtain") &&
                sensor.modelId() != QLatin1String("lumi.sensor_natgas") &&
                sensor.modelId() != QLatin1String("lumi.switch.b1naus01") &&
                !sensor.modelId().startsWith(QLatin1String("lumi.relay.c")) &&
                !sensor.type().endsWith(QLatin1String("Battery")))
            {
                item = sensor.addItem(DataTypeUInt8, RConfigBattery);
                //item->setValue(100); // wait for report
            }

            if (sensor.modelId().startsWith(QLatin1String("lumi.vibration")))
            {
                // low: 0x15, medium: 0x0B, high: 0x01
                item = sensor.addItem(DataTypeUInt8, RConfigSensitivity);
                item = sensor.addItem(DataTypeUInt8, RConfigSensitivityMax);
                item->setValue(0x15); // low
                item = sensor.addItem(DataTypeUInt8, RConfigPending);
            }

            if (!sensor.item(RStateTemperature) &&
                sensor.modelId() != QLatin1String("lumi.sensor_switch") &&
                !sensor.modelId().contains(QLatin1String("weather")) &&
                !sensor.modelId().startsWith(QLatin1String("lumi.sensor_ht")) &&
                !sensor.modelId().endsWith(QLatin1String("86opcn01"))) // exclude Aqara Opple
            {
                item = sensor.addItem(DataTypeInt16, RConfigTemperature);
                item->setValue(0);
                //item = sensor.addItem(DataTypeInt16, RConfigOffset);
                //item->setValue(0);
            }

            if (sensor.modelId().endsWith(QLatin1String("86opcn01")))
            {
                // Aqara Opple switches need to be configured to send proper button events
                item = sensor.addItem(DataTypeUInt8, RConfigPending);
                item->setValue(item->toNumber() | R_PENDING_MODE);
            }
        }
        else if (sensor.modelId().startsWith(QLatin1String("tagv4"))) // SmartThings Arrival sensor
        {
            item = sensor.addItem(DataTypeString, RConfigAlert);
            item->setValue(R_ALERT_DEFAULT);
        }

        if (sensor.fingerPrint().hasInCluster(IAS_ZONE_CLUSTER_ID))
        {
            if (sensor.modelId() == QLatin1String("button") || sensor.modelId().startsWith(QLatin1String("multi")) || sensor.modelId() == QLatin1String("water") ||
                sensor.modelId() == QLatin1String("Motion Sensor-A"))
            {
                // no support for some IAS Zone flags
            }
            else
            {
                item = sensor.addItem(DataTypeBool, RStateLowBattery);
                item->setValue(false);
                if (sensor.modelId().startsWith(QLatin1String("SMSZB-1"))) // Develco smoke detector
                {
                    item = sensor.addItem(DataTypeBool, RStateTest);
                    item->setValue(false);
                }
                else
                {
                    item = sensor.addItem(DataTypeBool, RStateTampered);
                    item->setValue(false);
                }
            }
            item = sensor.addItem(DataTypeUInt8, RConfigPending);
            item->setValue(0);
        }

        if (sensor.fingerPrint().hasInCluster(POWER_CONFIGURATION_CLUSTER_ID))
        {
            if (sensor.manufacturer().startsWith(QLatin1String("Climax")) ||
                sensor.modelId().startsWith(QLatin1String("902010/23")))
            {
                // climax non IAS reports state/lowbattery via battery alarm mask attribute
                item = sensor.addItem(DataTypeBool, RStateLowBattery);
                // don't set value -> null until reported
            }
            else if (sensor.modelId() == QLatin1String("lumi.sensor_natgas") ||
                     sensor.modelId() == QLatin1String("Bell"))
            {
                // Don't expose battery resource item for this device
            }
            else if (!sensor.type().endsWith(QLatin1String("Battery")))
            {
                item = sensor.addItem(DataTypeUInt8, RConfigBattery);
                // item->setValue(100);
            }
        }

        if (stateCol >= 0 &&
            sensor.type() != QLatin1String("CLIPGenericFlag") &&
            sensor.type() != QLatin1String("CLIPGenericStatus") &&
            sensor.type() != QLatin1String("Daylight"))
        {
            sensor.jsonToState(QLatin1String(colval[stateCol]));
        }

        if (configCol >= 0)
        {
            sensor.jsonToConfig(QLatin1String(colval[configCol]));
        }

        {
            ResourceItem *item = sensor.item(RStatePresence);
            if (item && item->toBool())
            {
                item->setValue(false); // reset at startup
            }
        }

        // check for older setups with multiple ZHASwitch sensors per device
        if (sensor.manufacturer() == QLatin1String("ubisys") && sensor.type() == QLatin1String("ZHASwitch"))
        {
            if ((sensor.modelId().startsWith(QLatin1String("D1")) && sensor.fingerPrint().endpoint != 0x02) ||
                (sensor.modelId().startsWith(QLatin1String("S2")) && sensor.fingerPrint().endpoint != 0x03) ||
                (sensor.modelId().startsWith(QLatin1String("C4")) && sensor.fingerPrint().endpoint != 0x01))
            {
                DBG_Printf(DBG_INFO, "ubisys sensor id: %s, endpoint 0x%02X (%s) ignored loading from database\n", qPrintable(sensor.id()), sensor.fingerPrint().endpoint, qPrintable(sensor.modelId()));
                return 0;
            }

            QStringList supportedModes({"momentary", "rocker", "custom"});
            item = sensor.addItem(DataTypeString, RConfigMode);

            bool isWindowCovering = sensor.modelId().startsWith(QLatin1String("J1"));
            ResourceItem *itemWindowCovering = 0;
            if (isWindowCovering)
            {
                itemWindowCovering = sensor.addItem(DataTypeUInt8, RConfigWindowCoveringType);
            }

            if (configCol >= 0)
            {
                sensor.jsonToConfig(QLatin1String(colval[configCol])); // needed again otherwise item isEmpty
            }

            if (isWindowCovering)
            {
                int val = itemWindowCovering->toNumber(); // prevent null value
                itemWindowCovering->setValue(val);
            }

            if (item->toString().isEmpty() || !supportedModes.contains(item->toString()))
            {
                item->setValue(supportedModes.first());
            }

//            QStringList gids;
//            item = sensor.addItem(DataTypeString, RConfigGroup);
//            if (!item->toString().isEmpty())
//            {
//                gids = item->toString().split(',');
//            }

//            int n = 0;
//            if      (sensor.modelId().startsWith(QLatin1String("D1"))) { n = 2; }
//            else if (sensor.modelId().startsWith(QLatin1String("S1-R"))) { n = 2; }
//            else if (sensor.modelId().startsWith(QLatin1String("S1"))) { n = 1; }
//            else if (sensor.modelId().startsWith(QLatin1String("S2"))) { n = 2; }
//            else if (sensor.modelId().startsWith(QLatin1String("C4"))) { n = 4; }

//            while (gids.size() < n)
//            {
//                gids.append("-1"); // not configured, TODO extract from BIND rules if available
//            }

//            QString out = gids.join(',');
//            if (item->toString() != out)
//            {
//                item->setValue(out);
//            }
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
                // if sensor was seen recently set reachable true
                item = sensor.item(RStateLastUpdated);
                if (!isClip && item && item->toNumber() > 0)
                {
                    QDateTime now = QDateTime::currentDateTimeUtc();
                    QDateTime lastSeen = QDateTime::fromMSecsSinceEpoch(item->toNumber());
                    const int minLastSeen = 60 * 60 * 24; // 24 hours
                    const int maxLastSeen = 60 * 60 * 24 * 7; // 1 week

                    item = sensor.item(RConfigReachable);

                    if (item && lastSeen.isValid() && now > lastSeen)
                    {
                        const auto dt = lastSeen.secsTo(now);
                        if (dt < minLastSeen)
                        {
                            sensor.rx();
                            item->setValue(true);
                        }
                        else if (dt > maxLastSeen && item->toBool()) // reachable but way too long ago
                        {
                            item->setValue(false);
                        }
                    }

                    // when reachable and assigned to a group, force check of group membership
                    if (item->toBool())
                    {
                        item = sensor.item(RConfigGroup);
                        if (item && !item->toString().isEmpty() && item->toString() != QLatin1String("0"))
                        {
                            Event e(RSensors, REventValidGroup, sensor.id());
                            d->enqueueEvent(e);
                        }
                    }
                }

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

    int id = sensors.empty() ? 1 : static_cast<int>(sensors.size()); // 'append' only
    while (id < 10000)
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

/*! Saves the current auth with apikey to the database.
 */
void DeRestPluginPrivate::saveApiKey(QString apikey)
{
    int rc;
    char *errmsg;

    std::vector<ApiAuth>::iterator i = apiAuths.begin();
    std::vector<ApiAuth>::iterator end = apiAuths.end();

    for (; i != end; ++i)
    {
        if (i->apikey == apikey)
        {
            DBG_Assert(i->createDate.timeSpec() == Qt::UTC);
            DBG_Assert(i->lastUseDate.timeSpec() == Qt::UTC);

            QString sql = QString(QLatin1String("REPLACE INTO auth (apikey, devicetype, createdate, lastusedate, useragent) VALUES ('%1', '%2', '%3', '%4', '%5')"))
                    .arg(i->apikey)
                    .arg(i->devicetype)
                    .arg(i->createDate.toString("yyyy-MM-ddTHH:mm:ss"))
                    .arg(i->lastUseDate.toString("yyyy-MM-ddTHH:mm:ss"))
                    .arg(i->useragent);

            DBG_Printf(DBG_INFO_L2, "DB sql exec %s\n", qPrintable(sql));
            errmsg = NULL;
            rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

            if (rc != SQLITE_OK)
            {
                if (errmsg)
                {
                    DBG_Printf(DBG_ERROR, "DB sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
                    sqlite3_free(errmsg);
                }
            }
            return;
        }
    }
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

    if (saveDatabaseItems & DB_NOSAVE)
    {
        return;
    }

    int rc;
    char *errmsg;
    QElapsedTimer measTimer;

    measTimer.start();

    // check if former transaction was committed
    if (sqlite3_get_autocommit(db) == 0) // is 1 when all is committed
    {
        errmsg = NULL;
        rc = sqlite3_exec(db, "COMMIT", 0, 0, &errmsg);

        if (rc != SQLITE_OK)
        {
            if (errmsg)
            {
                DBG_Printf(DBG_ERROR, "DB sqlite3_exec failed: COMMIT former transaction, error: %s (%d)\n", errmsg, rc);
                sqlite3_free(errmsg);
            }

            queSaveDb(saveDatabaseItems, DB_SHORT_SAVE_DELAY);
            return;
        }
    }

    // make the whole save process one transaction otherwise each insert would become
    // a transaction which is extremly slow
    errmsg = NULL;
    rc = sqlite3_exec(db, "BEGIN", 0, 0, &errmsg);
    if (rc != SQLITE_OK)
    {
        if (errmsg)
        {
            DBG_Printf(DBG_ERROR, "DB SQL exec failed: BEGIN, error: %s\n", errmsg);
            sqlite3_free(errmsg);
        }

        if (rc == SQLITE_BUSY)
        {
            DBG_Printf(DBG_INFO, "DB locked by another process, retry later\n");
        }

        queSaveDb(saveDatabaseItems, DB_SHORT_SAVE_DELAY);
        return;
    }

    DBG_Printf(DBG_INFO_L2, "DB save zll database items 0x%08X\n", saveDatabaseItems);

    // dump authorisation data
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

                DBG_Printf(DBG_INFO_L2, "DB sql exec %s\n", qPrintable(sql));
                errmsg = NULL;
                rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

                if (rc != SQLITE_OK)
                {
                    if (errmsg)
                    {
                        DBG_Printf(DBG_ERROR, "DB sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
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


                DBG_Printf(DBG_INFO_L2, "DB sql exec %s\n", qPrintable(sql));
                errmsg = NULL;
                rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

                if (rc != SQLITE_OK)
                {
                    if (errmsg)
                    {
                        DBG_Printf(DBG_ERROR, "DB sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
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
        gwConfig["networkopenduration"] = (double)gwNetworkOpenDuration;
        gwConfig["timeformat"] = gwTimeFormat;
        gwConfig["timezone"] = gwTimezone;
        gwConfig["rgbwdisplay"] = gwRgbwDisplay;
        gwConfig["rfconnect"] = (double)(gwRfConnectedExpected ? 1 : 0);
        gwConfig["announceinterval"] = (double)gwAnnounceInterval;
        gwConfig["announceurl"] = gwAnnounceUrl;
        gwConfig["groupdelay"] = gwGroupSendDelay;
        gwConfig["zigbeechannel"] = gwZigbeeChannel;
        gwConfig["group0"] = gwGroup0;
        gwConfig["gwusername"] = gwAdminUserName;
        gwConfig["gwpassword"] = gwAdminPasswordHash;
        gwConfig["homebridge"] = gwHomebridge;
        gwConfig["homebridgeversion"] = gwHomebridgeVersion;
        gwConfig["homebridgeupdateversion"] = gwHomebridgeUpdateVersion;
        gwConfig["homebridgeupdate"] = gwHomebridgeUpdate;
        gwConfig["homebridge-pin"] = gwHomebridgePin;
        gwConfig["updatechannel"] = gwUpdateChannel;
        gwConfig["swupdatestate"] = gwSwUpdateState;
        gwConfig["uuid"] = gwUuid;
        gwConfig["otauactive"] = isOtauActive();
        gwConfig["wifi"] = gwWifi;
        gwConfig["wifitype"] = gwWifiType;
        gwConfig["wifiname"] = gwWifiName;
        gwConfig["wificlientname"] = gwWifiClientName;
        gwConfig["wifichannel"] = gwWifiChannel;
        gwConfig["workingpw"] = gwWifiWorkingPw;
        gwConfig["workingtype"] = gwWifiWorkingType;
        gwConfig["workingname"] = gwWifiWorkingName;
        gwConfig["wificlientpw"] = gwWifiClientPw;
        gwConfig["wifipw"] = gwWifiPw;
        gwConfig["wifipwenc"] = gwWifiPwEnc;
        gwConfig["workingpwenc"] = gwWifiWorkingPwEnc;
        gwConfig["wifibackuppwenc"] = gwWifiBackupPwEnc;
        gwConfig["wifiip"] = gwWifiIp;
        gwConfig["wifipageactive"] = gwWifiPageActive;
        gwConfig["wifibackupname"] = gwWifiBackupName;
        gwConfig["wifibackuppw"] = gwWifiBackupPw;
        gwConfig["wifilastupdated"] = gwWifiLastUpdated;
        gwConfig["bridgeid"] = gwBridgeId;
        gwConfig["websocketnotifyall"] = gwWebSocketNotifyAll;
        gwConfig["disablePermitJoinAutoOff"] = gwdisablePermitJoinAutoOff;
        gwConfig["proxyaddress"] = gwProxyAddress;
        gwConfig["proxyport"] = gwProxyPort;
        gwConfig["zclvaluemaxage"] = dbZclValueMaxAge;

        QVariantMap::iterator i = gwConfig.begin();
        QVariantMap::iterator end = gwConfig.end();

        for (; i != end; ++i)
        {
            if (i->canConvert(QVariant::String))
            {
                QString sql = QString(QLatin1String(
                                          "UPDATE config2 SET value = '%2' WHERE key = '%1';"
                                          "INSERT INTO config2 (key, value) SELECT '%1', '%2' WHERE (SELECT changes() = 0);"))
                        .arg(i.key())
                        .arg(i.value().toString());

                DBG_Printf(DBG_INFO_L2, "DB sql exec %s\n", qPrintable(sql));
                errmsg = NULL;
                rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

                if (rc != SQLITE_OK)
                {
                    if (errmsg)
                    {
                        DBG_Printf(DBG_ERROR, "DB sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
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

                DBG_Printf(DBG_INFO_L2, "DB sql exec %s\n", qPrintable(sql));
                errmsg = NULL;
                rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

                if (rc != SQLITE_OK)
                {
                    if (errmsg)
                    {
                        DBG_Printf(DBG_ERROR, "DB sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
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

            DBG_Printf(DBG_INFO_L2, "DB sql exec %s\n", qPrintable(sql));
            errmsg = NULL;
            rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

            if (rc != SQLITE_OK)
            {
                if (errmsg)
                {
                    DBG_Printf(DBG_ERROR, "DB sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
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

                DBG_Printf(DBG_INFO_L2, "DB sql exec %s\n", qPrintable(sql));
                errmsg = NULL;
                rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

                if (rc != SQLITE_OK)
                {
                    if (errmsg)
                    {
                        DBG_Printf(DBG_ERROR, "DB sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
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
                        DBG_Printf(DBG_ERROR, "DB sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
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
                        DBG_Printf(DBG_ERROR, "DB sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
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

            QString ritems = i->resourceItemsToJson();
            QString sql = QString(QLatin1String("REPLACE INTO nodes (id, state, mac, name, groups, endpoint, modelid, manufacturername, swbuildid, ritems) VALUES ('%1', '%2', '%3', '%4', '%5', '%6', '%7', '%8', '%9', '%10')"))
                    .arg(i->id())
                    .arg(lightState)
                    .arg(i->uniqueId().toLower())
                    .arg(i->name())
                    .arg(groupIds.join(","))
                    .arg(i->haEndpoint().endpoint())
                    .arg(i->modelId())
                    .arg(i->manufacturer())
                    .arg(i->swBuildId())
                    .arg(ritems);

            if (i->state() == LightNode::StateDeleted)
            {
                sql.append(QString("; DELETE FROM devices WHERE mac = '%1'").arg(generateUniqueId(i->address().ext(), 0, 0)));
            }

            DBG_Printf(DBG_INFO_L2, "DB sql exec %s\n", qPrintable(sql));
            errmsg = NULL;
            rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

            if (rc != SQLITE_OK)
            {
                if (errmsg)
                {
                    DBG_Printf(DBG_ERROR, "DB sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
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
                    DBG_Printf(DBG_ERROR, "DB sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
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
            QString gid = "0x" + QString("%1").arg(i->address(), 4, 16, QLatin1Char('0')).toUpper();

            if (i->state() == Group::StateDeleted)
            {
                // delete scenes of this group (if exist)
                QString sql = QString(QLatin1String("DELETE FROM scenes WHERE gid='%1'")).arg(gid);

                DBG_Printf(DBG_INFO_L2, "DB sql exec %s\n", qPrintable(sql));
                errmsg = NULL;
                rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

                if (rc != SQLITE_OK)
                {
                    if (errmsg)
                    {
                        DBG_Printf(DBG_ERROR, "DB sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
                        sqlite3_free(errmsg);
                    }
                }
            }

            if (i->state() == Group::StateDeleteFromDB)
            {
                // delete group from db (if exist)
                QString sql = QString(QLatin1String("DELETE FROM groups WHERE gid='%1'")).arg(gid);

                DBG_Printf(DBG_INFO_L2, "DB sql exec %s\n", qPrintable(sql));
                errmsg = NULL;
                rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

                if (rc != SQLITE_OK)
                {
                    if (errmsg)
                    {
                        DBG_Printf(DBG_ERROR, "DB sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
                        sqlite3_free(errmsg);
                    }
                }
                continue;
            }

            QString grpState((i->state() == Group::StateDeleted ? QLatin1String("deleted") : QLatin1String("normal")));
            QString hidden((i->hidden == true ? QLatin1String("true") : QLatin1String("false")));
            const QString &gtype = i->item(RAttrType)->toString();
            const QString &gclass = i->item(RAttrClass)->toString();
            QString uniqueid;
            const ResourceItem *item = i->item(RAttrUniqueId);
            if (item)
            {
                uniqueid = item->toString();
            }

            QString sql = QString(QLatin1String("REPLACE INTO groups (gid, name, state, mids, devicemembership, lightsequence, hidden, type, class, uniqueid) VALUES ('%1', '%2', '%3', '%4', '%5', '%6', '%7', '%8', '%9', '%10')"))
                    .arg(gid)
                    .arg(i->name())
                    .arg(grpState)
                    .arg(i->midsToString())
                    .arg(i->dmToString())
                    .arg(i->lightsequenceToString())
                    .arg(hidden)
                    .arg(gtype)
                    .arg(gclass)
                    .arg(uniqueid);

            DBG_Printf(DBG_INFO_L2, "DB sql exec %s\n", qPrintable(sql));
            errmsg = NULL;
            rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

            if (rc != SQLITE_OK)
            {
                if (errmsg)
                {
                    DBG_Printf(DBG_ERROR, "DB sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
                    sqlite3_free(errmsg);
                }
            }

            if (i->state() == Group::StateNormal)
            {
                std::vector<Scene>::const_iterator si = i->scenes.begin();
                std::vector<Scene>::const_iterator send = i->scenes.end();

                for (; si != send; ++si)
                {
                    QString gsid = "0x" + QString("%1%2")
                       .arg(i->address(), 4, 16, QLatin1Char('0'))
                       .arg(si->id, 2, 16, QLatin1Char('0')).toUpper(); // unique key

                    QString sid = "0x" + QString("%1").arg(si->id, 2, 16, QLatin1Char('0')).toUpper();

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
                    DBG_Printf(DBG_INFO_L2, "DB sql exec %s\n", qPrintable(sql));
                    errmsg = NULL;
                    rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

                    if (rc != SQLITE_OK)
                    {
                        if (errmsg)
                        {
                            DBG_Printf(DBG_ERROR, "DB sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
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

                DBG_Printf(DBG_INFO_L2, "DB sql exec %s\n", qPrintable(sql));
                errmsg = NULL;
                rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

                if (rc != SQLITE_OK)
                {
                    if (errmsg)
                    {
                        DBG_Printf(DBG_ERROR, "DB sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
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


            DBG_Printf(DBG_INFO_L2, "DB sql exec %s\n", qPrintable(sql));
            errmsg = NULL;
            rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

            if (rc != SQLITE_OK)
            {
                if (errmsg)
                {
                    DBG_Printf(DBG_ERROR, "DB sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
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

                DBG_Printf(DBG_INFO_L2, "DB sql exec %s\n", qPrintable(sql));
                errmsg = NULL;
                rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

                if (rc != SQLITE_OK)
                {
                    if (errmsg)
                    {
                        DBG_Printf(DBG_ERROR, "DB sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
                        sqlite3_free(errmsg);
                    }
                }
            }
            else if (rl.state == Resourcelinks::StateDeleted)
            {
                QString sql = QString(QLatin1String("DELETE FROM resourcelinks WHERE id='%1'")).arg(rl.id);

                DBG_Printf(DBG_INFO_L2, "DB sql exec %s\n", qPrintable(sql));
                errmsg = NULL;
                rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

                if (rc != SQLITE_OK)
                {
                    if (errmsg)
                    {
                        DBG_Printf(DBG_ERROR, "DB sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
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

                DBG_Printf(DBG_INFO_L2, "DB sql exec %s\n", qPrintable(sql));
                errmsg = NULL;
                rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

                if (rc != SQLITE_OK)
                {
                    if (errmsg)
                    {
                        DBG_Printf(DBG_ERROR, "DB sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
                        sqlite3_free(errmsg);
                    }
                }
            }
            else if (i->state == Schedule::StateDeleted)
            {
                QString sql = QString(QLatin1String("DELETE FROM schedules WHERE id='%1'")).arg(i->id);

                DBG_Printf(DBG_INFO_L2, "DB sql exec %s\n", qPrintable(sql));
                errmsg = NULL;
                rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

                if (rc != SQLITE_OK)
                {
                    if (errmsg)
                    {
                        DBG_Printf(DBG_ERROR, "DB sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
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
                        DBG_Printf(DBG_ERROR, "DB sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
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

            if (i->deletedState() == Sensor::StateDeleted)
            {
                sql.append(QString("; DELETE FROM devices WHERE mac = '%1'").arg(generateUniqueId(i->address().ext(), 0, 0)));
            }

            DBG_Printf(DBG_INFO_L2, "DB sql exec %s\n", qPrintable(sql));
            errmsg = NULL;
            rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

            if (rc != SQLITE_OK)
            {
                if (errmsg)
                {
                    DBG_Printf(DBG_ERROR, "DB sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
                    sqlite3_free(errmsg);
                }
            }
        }

        saveDatabaseItems &= ~DB_SENSORS;
    }

    // process query queue
    if (saveDatabaseItems & DB_QUERY_QUEUE)
    {
        for (const QString &sql : dbQueryQueue)
        {
            if (DBG_IsEnabled(DBG_INFO_L2))
            {
                DBG_Printf(DBG_INFO_L2, "DB sql exec %s\n", qPrintable(sql));
            }

            errmsg = NULL;
            rc = sqlite3_exec(db, sql.toUtf8().constData(), NULL, NULL, &errmsg);

            if (rc != SQLITE_OK)
            {
                if (errmsg)
                {
                    DBG_Printf(DBG_ERROR, "DB sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
                    sqlite3_free(errmsg);
                }
            }
        }

        dbQueryQueue.clear();
        saveDatabaseItems &= ~DB_QUERY_QUEUE;
    }

    errmsg = NULL;
    rc = sqlite3_exec(db, "COMMIT", 0, 0, &errmsg);
    if (rc != SQLITE_OK)
    {
        if (errmsg)
        {
            DBG_Printf(DBG_ERROR, "DB sqlite3_exec failed: COMMIT, error: %s (%d)\n", errmsg, rc);
            sqlite3_free(errmsg);
        }

        // if the transaction is still intact (SQLITE_BUSY) it will be committed on the next run of saveDb()
    }

    if (rc == SQLITE_OK)
    {
        DBG_Printf(DBG_INFO_L2, "DB saved in %ld ms\n", measTimer.elapsed());

        if (saveDatabaseItems & DB_SYNC)
        {
#ifdef Q_OS_LINUX
            QElapsedTimer measTimer;
            measTimer.restart();
            sync();
            DBG_Printf(DBG_INFO_L2, "sync() in %d ms\n", int(measTimer.elapsed()));
#endif
            saveDatabaseItems &= ~DB_SYNC;
        }
    }
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
            return;
        }

        int ret = sqlite3_close(db);
        if (ret == SQLITE_OK)
        {
            db = 0;
#ifdef Q_OS_LINUX
            QElapsedTimer measTimer;
            measTimer.restart();
            sync();
            DBG_Printf(DBG_INFO, "sync() in %d ms\n", int(measTimer.elapsed()));
#endif
            return;
        }
        else if (ret == SQLITE_BUSY)
        {
            DBG_Printf(DBG_INFO, "sqlite3_close() busy %d\n", ret);
            return; // close later
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

static int sqliteLastZbconfCallback(void *user, int ncols, char **colval , char **colname)
{
    Q_UNUSED(colname);
    QString *str = static_cast<QString*>(user);
    if (!str || ncols != 1)
    {
        return 0;
    }

    *str = QString::fromUtf8(colval[0]);
    return 0;
}

/* Get the last known working zigbee configuration from database. */
void DeRestPluginPrivate::getLastZigBeeConfigDb(QString &out)
{

    QString sql = QLatin1String("SELECT conf FROM zbconf ORDER BY rowid desc limit 1");

    DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
    char *errmsg = NULL;
    int rc = sqlite3_exec(db, qPrintable(sql), sqliteLastZbconfCallback, &out, &errmsg);

    if (rc != SQLITE_OK)
    {
        if (errmsg)
        {
            DBG_Printf(DBG_ERROR, "sqlite3_exec failed: %s, error: %s\n", qPrintable(sql), errmsg);
            sqlite3_free(errmsg);
        }
    }
}

/*! Returns a list of all Zigbee network configurations. */
void DeRestPluginPrivate::getZigbeeConfigDb(QVariantList &out)
{
    openDb();

    DBG_Assert(db);
    if (!db)
    {
        return;
    }

    int rc;
    sqlite3_stmt *res = nullptr;
    const char * sql = "SELECT rowid, conf FROM zbconf";

    rc = sqlite3_prepare_v2(db, sql, -1, &res, nullptr);
    DBG_Assert(res);
    DBG_Assert(rc == SQLITE_OK);

    while (1)
    {
        rc = sqlite3_step(res);
        DBG_Assert(rc == SQLITE_ROW);
        if (rc != SQLITE_ROW)
        {
            break;
        }

        int rowid = sqlite3_column_int(res, 0);
        const char* conf = reinterpret_cast<const char*>(sqlite3_column_text(res, 1));
        const auto size = sqlite3_column_bytes(res, 1);

        if (!conf || size <= 100 || size > 2048)
        {
            continue;
        }

        QVariantMap map = Json::parse(QLatin1String(conf)).toMap();

        if (map.isEmpty())
        {
            continue;
        }

        map["id"] = rowid;

        out.push_back(map);

        DBG_Printf(DBG_INFO, "ZB rowid %d, conf: %s\n", rowid, conf);
    }

    rc = sqlite3_finalize(res);
    DBG_Assert(rc == SQLITE_OK);

    closeDb();
}

/*! Put working ZigBee configuration in database for later recovery or fail safe operations.
    - An entry is only added when different from last entry.
    - Entries are only added, never modified, this way errors or unwanted changes can be debugged.
    - Too old entries might be delated later on sqlite3 'rowid' provides timed order.
 */
void DeRestPluginPrivate::updateZigBeeConfigDb()
{
    if (!apsCtrl)
    {
        return;
    }

    if (!isInNetwork())
    {
        return;
    }

    if (apsCtrl->getParameter(deCONZ::ParamDeviceConnected) == 0)
    {
        return;
    }

    if (gwFirmwareVersion.startsWith(QLatin1String("0x0000000"))) // 0x00000000 and 0x00000001
    {
        return;
    }

    QString conf;
    getLastZigBeeConfigDb(conf);

    QDateTime now = QDateTime::currentDateTime();
    if (conf.isEmpty()) // initial
    {}
    else if (!zbConfigGood.isValid() || (zbConfigGood.secsTo(now) > CHECK_ZB_GOOD_INTERVAL) || (now < zbConfigGood))
    {
        return;
    }

    uint8_t deviceType = apsCtrl->getParameter(deCONZ::ParamDeviceType);
    uint16_t panId = apsCtrl->getParameter(deCONZ::ParamPANID);
    quint64 extPanId = apsCtrl->getParameter(deCONZ::ParamExtendedPANID);
    quint64 apsUseExtPanId = apsCtrl->getParameter(deCONZ::ParamApsUseExtendedPANID);
    uint64_t macAddress = apsCtrl->getParameter(deCONZ::ParamMacAddress);
    uint16_t nwkAddress = apsCtrl->getParameter(deCONZ::ParamNwkAddress);
    uint8_t staticNwkAddress = apsCtrl->getParameter(deCONZ::ParamStaticNwkAddress);
    uint8_t curChannel = apsCtrl->getParameter(deCONZ::ParamCurrentChannel);
    uint8_t securityMode = apsCtrl->getParameter(deCONZ::ParamSecurityMode);
    quint64 tcAddress = apsCtrl->getParameter(deCONZ::ParamTrustCenterAddress);
    QByteArray networkKey = apsCtrl->getParameter(deCONZ::ParamNetworkKey);
    uint8_t nwkUpdateId = apsCtrl->getParameter(deCONZ::ParamNetworkUpdateId);

    // some basic checks for common configuration as HA coordinator
    if (macAddress == 0)
    {
        return;
    }

    if (deviceType != deCONZ::Coordinator)
    {
        return;
    }

    if (deviceType == deCONZ::Coordinator)
    {
        // 0 is required and means the used extended panid will become
        // coordinator mac address once network is up
        if (apsUseExtPanId != 0)
        {
            return;
        }

        if (tcAddress != macAddress)
        {
            return;
        }
    }
    else
    {
        return; // router currently not supported
    }

    if (curChannel < 11 || curChannel > 26)
    {
        return;
    }

    if (securityMode != 3) // no master but tc link key
    {
        return;
    }

    QVariantMap map;
    map["deviceType"] = deviceType;
    map["panId"] = QString("0x%1").arg(QString::number(panId,16));
    map["extPanId"] = QString("0x%1").arg(QString::number(extPanId,16));
    map["apsUseExtPanId"] = QString("0x%1").arg(QString::number(apsUseExtPanId,16));
    map["macAddress"] = QString("0x%1").arg(QString::number(macAddress,16));
    map["staticNwkAddress"] = (staticNwkAddress == 0) ? false : true;
    map["nwkAddress"] = QString("0x%1").arg(QString::number(nwkAddress,16));
    map["curChannel"] = curChannel;
    map["securityMode"] = securityMode;
    map["tcAddress"] = QString("0x%1").arg(QString::number(tcAddress,16));
    map["networkKey"] = networkKey.toHex();
    map["nwkUpdateId"] = nwkUpdateId;
    map["swversion"] = QLatin1String(GW_SW_VERSION);
    map["fwversion"] = gwFirmwareVersion;

    bool success = true;
    QString curConf = Json::serialize(map, success);
    if (!success)
    {
        return;
    }

    if (conf == curConf) // nothing changed
    {
        return;
    }

    {
        QString sql = QString(QLatin1String("INSERT INTO zbconf (conf) VALUES ('%1')")).arg(curConf);

        DBG_Printf(DBG_INFO_L2, "sql exec %s\n", qPrintable(sql));
        char * errmsg = NULL;
        int rc = sqlite3_exec(db, qPrintable(sql), NULL, NULL, &errmsg);

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

    if (permitJoinFlag) // don't save database while joining devices
    {
        databaseTimer->start(DB_SHORT_SAVE_DELAY);
        return;
    }

    if (saveDatabaseItems & DB_NOSAVE)
    {
        databaseTimer->start(DB_SHORT_SAVE_DELAY);
        return;
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
