/*
 * Copyright (c) 2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <array>
#include <stack>
#include <deconz.h>
#include "backup.h"
#include "json.h"
#include "crypto/random.h"

#define EXT_PROCESS_TIMEOUT 10000

using TmpFiles = std::array<const char*, 3>;

static bool cleanupTemporaryFiles(const QString &path, const TmpFiles &files)
{
    for (const auto &f : files)
    {
        if (!f) { continue; } // some elements can be nullptr

        const QString filePath = path + QLatin1String(f);
        if (QFile::exists(filePath))
        {
            if (!QFile::remove(filePath))
            {
                DBG_Printf(DBG_ERROR, "backup: failed to remove temporary file %s\n", qPrintable(filePath));
                return false;
            }
        }
    }

    return true;
}

/*! Export the deCONZ network settings to a file.
 */
bool BAK_ExportConfiguration(deCONZ::ApsController *apsCtrl)
{
    if (!apsCtrl)
    {
        return false;
    }

    const QString path = deCONZ::getStorageLocation(deCONZ::ApplicationsDataLocation);

    // cleanup older files
    if (!cleanupTemporaryFiles(path, { "/deCONZ.conf", "/deCONZ.tar", "/deCONZ.tar.gz" }))
    {
        return false;
    }

    {
        uint8_t deviceType = apsCtrl->getParameter(deCONZ::ParamDeviceType);
        uint16_t panId = apsCtrl->getParameter(deCONZ::ParamPANID);
        quint64 extPanId = apsCtrl->getParameter(deCONZ::ParamExtendedPANID);
        quint64 apsUseExtPanId = apsCtrl->getParameter(deCONZ::ParamApsUseExtendedPANID);
        uint64_t macAddress = apsCtrl->getParameter(deCONZ::ParamMacAddress);
        uint16_t nwkAddress = apsCtrl->getParameter(deCONZ::ParamNwkAddress);
        uint8_t apsAck = apsCtrl->getParameter(deCONZ::ParamApsAck);
        uint8_t staticNwkAddress = apsCtrl->getParameter(deCONZ::ParamStaticNwkAddress);
        // uint32_t channelMask = apsCtrl->getParameter(deCONZ::ParamChannelMask);
        uint8_t curChannel = apsCtrl->getParameter(deCONZ::ParamCurrentChannel);
        uint8_t otauActive = apsCtrl->getParameter(deCONZ::ParamOtauActive);
        uint8_t securityMode = apsCtrl->getParameter(deCONZ::ParamSecurityMode);
        quint64 tcAddress = apsCtrl->getParameter(deCONZ::ParamTrustCenterAddress);
        QByteArray networkKey = apsCtrl->getParameter(deCONZ::ParamNetworkKey);
        QByteArray tcLinkKey = apsCtrl->getParameter(deCONZ::ParamTrustCenterLinkKey);
        uint8_t nwkUpdateId = apsCtrl->getParameter(deCONZ::ParamNetworkUpdateId);
        QVariantMap endpoint1 = apsCtrl->getParameter(deCONZ::ParamHAEndpoint, 0);
        QVariantMap endpoint2 = apsCtrl->getParameter(deCONZ::ParamHAEndpoint, 1);

        // simple checks to prevent invalid config export
        if (deviceType != deCONZ::Coordinator) { return false; }
        if (securityMode != 3) { return false; } // High - No master but TC link key
        if (nwkAddress != 0x0000) { return  false; }
        if (panId == 0) { return  false; }
        if (macAddress == 0) { return  false; }
        if (tcAddress == 0) { return  false; }
        if (curChannel < 11 || curChannel > 26) { return  false; }

        QVariantMap map;
        map["deviceType"] = deviceType;
        map["panId"] = QString("0x%1").arg(QString::number(panId, 16));
        map["extPanId"] = QString("0x%1").arg(QString::number(extPanId, 16));
        map["apsUseExtPanId"] = QString("0x%1").arg(QString::number(apsUseExtPanId, 16));
        map["macAddress"] = QString("0x%1").arg(QString::number(macAddress, 16));
        map["staticNwkAddress"] = (staticNwkAddress == 0) ? false : true;
        map["nwkAddress"] = QString("0x%1").arg(QString::number(nwkAddress, 16));
        map["apsAck"] = (apsAck == 0) ? false : true;
        //map["channelMask"] = channelMask;
        map["curChannel"] = curChannel;
        map["otauactive"] = otauActive;
        map["securityMode"] = securityMode;
        map["tcAddress"] = QString("0x%1").arg(QString::number(tcAddress,16));
        map["networkKey"] = networkKey.toHex();
        map["tcLinkKey"] = tcLinkKey.toHex();
        map["nwkUpdateId"] = nwkUpdateId;
        map["endpoint1"] = endpoint1;
        map["endpoint2"] = endpoint2;
        map["deconzVersion"] = QString(GW_SW_VERSION).replace(QChar('.'), "");

#if DECONZ_LIB_VERSION >= 0x011002
        {
            quint32 frameCounter = apsCtrl->getParameter(deCONZ::ParamFrameCounter);
            if (frameCounter > 0)
            {
                map["frameCounter"] = frameCounter;
            }
        }
#endif

        bool ok = true;
        QString saveString = Json::serialize(map, ok);

        DBG_Assert(ok);
        if (!ok)
        {
            return false;
        }

        // put config as JSON object in file
        QFile configFile(path + "/deCONZ.conf");
        if (configFile.open(QIODevice::ReadWrite))
        {
            QTextStream stream(&configFile);
            stream << saveString << "\n";
            configFile.close();
        }
    }

    if (QFile::exists(path + "/deCONZ.conf"))
    {
        //create .tar
        QProcess archProcess;

#ifdef Q_OS_WIN
        const QString appPath = qApp->applicationDirPath();
        {
            if (!QFile::exists(appPath + "/7za.exe"))
            {
                DBG_Printf(DBG_INFO, "7z not found: %s\n", qPrintable(appPath + "/7za.exe"));
                return false;
            }
            QString cmd = appPath + "/7za.exe";
            QStringList args;
            args.append("a");
            args.append(path + "/deCONZ.tar");
            args.append(path + "/deCONZ.conf");
            args.append(path + "/zll.db");
            args.append(path + "/session.default");
            archProcess.start(cmd, args);
        }
#endif

#ifdef Q_OS_LINUX
        // clean up old homebridge backup files
        const QStringList filters{ "AccessoryInfo*", "IdentifierCache*" };

        {
            QDir appDir(path);
            const QStringList files = appDir.entryList(filters);

            for (const auto &f : files)
            {
                const QString filePath = path + "/" + f;
                if (QFile::exists(filePath))
                {
                    if (QFile::remove(filePath))
                    {
                        DBG_Printf(DBG_INFO, "backup: removed temporary homebridge file %s\n", qPrintable(filePath));
                    }
                    else
                    {
                        DBG_Printf(DBG_ERROR, "backup: failed to remove temporary homebridge file %s\n", qPrintable(filePath));
                        return false;
                    }
                }
            }
        }

        // backup homebridge files
        const QString homebridgePersistPath = "/home/pi/.homebridge/persist"; // TODO: get mainuser

        QString FirstFileName ="";
        QString SecondFileName ="";

        QDir dir(homebridgePersistPath);
        if (dir.exists())
        {
            QStringList files = dir.entryList(filters);

            if (files.size() > 0)
            {
                FirstFileName = files.at(0);
                DBG_Printf(DBG_INFO, "copy file: %s to backup directory\n", qPrintable(FirstFileName));
                QFile accessoryFile(homebridgePersistPath + "/" + FirstFileName);
                if (!accessoryFile.copy(path + "/" + FirstFileName))
                {
                    DBG_Printf(DBG_INFO, "copy file: %s failed. Do not include it in backup\n", qPrintable(FirstFileName));
                    FirstFileName = "";
                    return false;
                }

            }
            if (files.size() > 1)
            {
                SecondFileName = files.at(1);
                DBG_Printf(DBG_INFO, "copy file: %s to backup directory\n", qPrintable(SecondFileName));
                QFile IdentifierFile(homebridgePersistPath + "/" + SecondFileName);
                if (!IdentifierFile.copy(path + "/" + SecondFileName))
                {
                    DBG_Printf(DBG_INFO, "copy file: %s failed. Do not include it in backup\n", qPrintable(SecondFileName));
                    SecondFileName = "";
                    return false;
                }
            }
        }

        // add homebridge-install logfiles to archive
        QString logfilesDirectories = "";
        QDir homebridgeInstallLogDir(path + "/homebridge-install-logfiles");
        if (homebridgeInstallLogDir.exists())
        {
            logfilesDirectories += QLatin1String("homebridge-install-logfiles");
        }

        {
            QStringList args;
            args.append("-cf");
            args.append(path + "/deCONZ.tar");
            args.append("-C");
            args.append(path);
            args.append("deCONZ.conf");
            args.append("zll.db");
            args.append("session.default");
            args.append(FirstFileName);
            args.append(SecondFileName);
            args.append(logfilesDirectories);

            archProcess.start("tar", args);
        }
#endif
        archProcess.waitForFinished(EXT_PROCESS_TIMEOUT);
        DBG_Printf(DBG_INFO, "%s\n", qPrintable(archProcess.readAllStandardOutput()));

        //create .tar.gz
        {
            QProcess zipProcess;
            QStringList args;
#ifdef Q_OS_WIN
            QString cmd = appPath + "/7za.exe";

            args.append("a");
            args.append(path + "/deCONZ.tar.gz");
            args.append(path + "/deCONZ.tar");
            zipProcess.start(cmd, args);
#endif
#ifdef Q_OS_LINUX
            args.append("-k");
            args.append("-f");
            args.append(path + "/deCONZ.tar");
            zipProcess.start("gzip", args);
#endif
            zipProcess.waitForFinished(EXT_PROCESS_TIMEOUT);
            DBG_Printf(DBG_INFO, "%s\n", qPrintable(zipProcess.readAllStandardOutput()));
        }
    }

    //cleanup
    if (!cleanupTemporaryFiles(path, { "/deCONZ.conf", "/deCONZ.tar" }))
    {
        return false;
    }

    return true;
}

/*! Import the deCONZ network settings from a file.
 */
bool BAK_ImportConfiguration(deCONZ::ApsController *apsCtrl)
{
    if (!apsCtrl)
    {
        return false;
    }

    const QString path = deCONZ::getStorageLocation(deCONZ::ApplicationsDataLocation);

    if (!cleanupTemporaryFiles(path, { "/deCONZ.conf", "/deCONZ.tar" }))
    {
        return false;
    }

#ifdef Q_OS_LINUX
    // clean up old homebridge backup files
    {
        const QStringList filters{ "AccessoryInfo*", "IdentifierCache*" };

         const QDir appDir(path);
         const QStringList files = appDir.entryList(filters);

         for (const auto &f : files)
         {
             const QString filePath = path + "/" + f;
             if (QFile::exists(filePath))
             {
                 if (QFile::remove(filePath))
                 {
                     DBG_Printf(DBG_INFO, "backup: removed temporary homebridge file %s\n", qPrintable(filePath));
                 }
                 else
                 {
                     DBG_Printf(DBG_ERROR, "backup: failed to remove temporary homebridge file %s\n", qPrintable(filePath));
                     return false;
                 }
             }
         }
     }
 #endif

    if (QFile::exists(path + QLatin1String("/deCONZ.tar.gz")))
    {
        // decompress .tar.gz
        QProcess archProcess;
        QStringList args;

#ifdef Q_OS_WIN
        const QString appPath = qApp->applicationDirPath();
        const QString cmd = appPath + "/7za.exe";
        args.append("e");
        args.append("-y");
        args.append(path + "/deCONZ.tar.gz");
        args.append("-o" + path);
        archProcess.start(cmd, args);
#endif
#ifdef Q_OS_LINUX
        args.append("-df");
        args.append(path + "/deCONZ.tar.gz");
        archProcess.start("gzip", args);
#endif
        archProcess.waitForFinished(EXT_PROCESS_TIMEOUT);
        DBG_Printf(DBG_INFO, "%s\n", qPrintable(archProcess.readAllStandardOutput()));
    }

    if (QFile::exists(path + QLatin1String("/deCONZ.tar")))
    {
        // unpack .tar
        QProcess zipProcess;
        QStringList args;
#ifdef Q_OS_WIN
        const  QString appPath = qApp->applicationDirPath();
        const QString cmd = appPath + "/7za.exe";
        args.append("e");
        args.append("-y");
        args.append(path + "/deCONZ.tar");
        args.append("-o" + path);
        zipProcess.start(cmd, args);
#endif
#ifdef Q_OS_LINUX
        args.append("-xf");
        args.append(path + "/deCONZ.tar");
        args.append("-C");
        args.append(path);
        zipProcess.start("tar", args);
#endif
        zipProcess.waitForFinished(EXT_PROCESS_TIMEOUT);
        DBG_Printf(DBG_INFO, "%s\n", qPrintable(zipProcess.readAllStandardOutput()));
    }

    QVariantMap map;
    {
        QFile file(path + QLatin1String("/deCONZ.conf"));
        if (file.exists() && file.open(QIODevice::ReadOnly))
        {
            bool ok = false;
            const QString json = file.readAll();
            QVariant var = Json::parse(json, ok);
            if (ok)
            {
                map = var.toMap();
            }
        }
    }

    cleanupTemporaryFiles(path, { "/deCONZ.conf", "/deCONZ.tar", "/deCONZ.tar.gz" });

    const std::array<const char*, 14> requiredFields = {
        "deviceType", "panId", "extPanId", "apsUseExtPanId", "macAddress", "staticNwkAddress",
        "nwkAddress", "apsAck", "curChannel", "tcAddress", "securityMode", "networkKey", "tcLinkKey",
        "nwkUpdateId"
    };

    for (const auto *key : requiredFields)
    {
        if (!map.contains(QLatin1String(key)))
        {
            return false;
        }
    }

    {
        bool ok = false;
        uint8_t deviceType = map["deviceType"].toUInt(&ok);

        // only coordinator supported currently
        if (!ok || deviceType != deCONZ::Coordinator)
        {
            return false;
        }

        uint16_t panId =  ok ? map["panId"].toString().toUShort(&ok, 16) : 0;
        if (ok && panId == 0) { ok = false; }

        quint64 extPanId =  ok ? map["extPanId"].toString().toULongLong(&ok, 16) : 0;
        if (ok && extPanId == 0) { ok = false; }

        quint64 apsUseExtPanId = ok ? map["apsUseExtPanId"].toString().toULongLong(&ok, 16) : 1;
        if (ok && apsUseExtPanId != 0) { ok = false; } // must be zero

        quint64 curMacAddress = apsCtrl->getParameter(deCONZ::ParamMacAddress);
        quint64 macAddress =  ok ? map["macAddress"].toString().toULongLong(&ok, 16) : 0;
        if (ok && macAddress == 0) { ok = false; }

        uint8_t staticNwkAddress = map["staticNwkAddress"].toBool() ? 1 : 0;
        uint16_t nwkAddress = map["nwkAddress"].toString().toUInt(&ok, 16);
        if (ok && nwkAddress != 0x0000) { ok = false; } // coordinator

        uint8_t apsAck = map["apsAck"].toBool() ? 1 : 0;
        //map["channelMask"] = channelMask;
        uint8_t curChannel = ok ? map["curChannel"].toUInt(&ok) : 0;
        if (ok && (curChannel < 11 || curChannel > 26)) { ok = false; }

        if (ok && map.contains("otauactive"))
        {
            uint8_t otauActive = map["otauactive"].toUInt();
            apsCtrl->setParameter(deCONZ::ParamOtauActive, otauActive);
        }
        uint8_t securityMode = ok ? map["securityMode"].toUInt(&ok) : 0;
        if (ok && securityMode != 3)
        {
            // auto correct, has been seen as 0..2
            securityMode = 3; // High - No Master but TC Link key
        }

        quint64 tcAddress =  ok ? map["tcAddress"].toString().toULongLong(&ok, 16) : 0;
        if (ok && tcAddress != macAddress)
        {
            tcAddress = macAddress; // auto correct
        }
        QByteArray nwkKey = QByteArray::fromHex(map["networkKey"].toByteArray());

        if (map["tcLinkKey"].toString() != QLatin1String("5a6967426565416c6c69616e63653039"))
        {
            // auto correct
            map["tcLinkKey"] = QLatin1String("5a6967426565416c6c69616e63653039"); // HA default TC link key
        }

        QByteArray tcLinkKey = QByteArray::fromHex(map["tcLinkKey"].toByteArray());

        uint8_t currentNwkUpdateId = apsCtrl->getParameter(deCONZ::ParamNetworkUpdateId);
        uint8_t nwkUpdateId = ok ? map["nwkUpdateId"].toUInt(&ok) : 0;

        if (!ok) // TODO as alternative load network configuration from zll.db file
        {
            return false;
        }

        apsCtrl->setParameter(deCONZ::ParamDeviceType, deviceType);
        apsCtrl->setParameter(deCONZ::ParamPredefinedPanId, 1);
        apsCtrl->setParameter(deCONZ::ParamPANID, panId);
        apsCtrl->setParameter(deCONZ::ParamExtendedPANID, extPanId);
        apsCtrl->setParameter(deCONZ::ParamApsUseExtendedPANID, apsUseExtPanId);
        if (curMacAddress != macAddress)
        {
            apsCtrl->setParameter(deCONZ::ParamCustomMacAddress, 1);
        }
        apsCtrl->setParameter(deCONZ::ParamMacAddress, macAddress);
        apsCtrl->setParameter(deCONZ::ParamStaticNwkAddress, staticNwkAddress);
        apsCtrl->setParameter(deCONZ::ParamNwkAddress, nwkAddress);
        apsCtrl->setParameter(deCONZ::ParamApsAck, apsAck);
        // channelMask
        apsCtrl->setParameter(deCONZ::ParamCurrentChannel, curChannel);
        apsCtrl->setParameter(deCONZ::ParamSecurityMode, securityMode);
        apsCtrl->setParameter(deCONZ::ParamTrustCenterAddress, tcAddress);
        apsCtrl->setParameter(deCONZ::ParamNetworkKey, nwkKey);
        apsCtrl->setParameter(deCONZ::ParamTrustCenterLinkKey, tcLinkKey);
        if (currentNwkUpdateId < nwkUpdateId)
        {
            apsCtrl->setParameter(deCONZ::ParamNetworkUpdateId, nwkUpdateId);
        }

#if DECONZ_LIB_VERSION >= 0x011002
        {
            quint32 frameCounter = map["frameCounter"].toUInt(&ok);
            if (ok && frameCounter > 0)
            {
                apsCtrl->setParameter(deCONZ::ParamFrameCounter, frameCounter);
            }
        }
#endif

        // HA endpoint
        QVariantMap endpoint1;
        endpoint1["endpoint"] = QLatin1String("0x01");
        endpoint1["profileId"] = QLatin1String("0x0104");
        endpoint1["deviceId"] = QLatin1String("0x05");
        endpoint1["deviceVersion"] = QLatin1String("0x01");
        endpoint1["inClusters"] = QVariantList({ "0x0000", "0x0019", "0x000A"});
        endpoint1["outClusters"] = QVariantList({ "0x0500"});
        endpoint1["index"] = static_cast<double>(0);

        // green power endpoint
        QVariantMap endpoint2;
        endpoint2["endpoint"] = QLatin1String("0xf2");
        endpoint2["profileId"] = QLatin1String("0xA1E0");
        endpoint2["deviceId"] = QLatin1String("0x0064");
        endpoint2["deviceVersion"] = QLatin1String("0x01");
        endpoint2["inClusters"] = QVariantList();
        endpoint2["outClusters"] = QVariantList({ "0x0021"});
        endpoint2["index"] = static_cast<double>(1);

        apsCtrl->setParameter(deCONZ::ParamHAEndpoint, endpoint1);
        apsCtrl->setParameter(deCONZ::ParamHAEndpoint, endpoint2);
    }

    return true;
}

/*! Reset the deCONZ network settings and/or delete database.
 */
bool BAK_ResetConfiguration(deCONZ::ApsController *apsCtrl, bool resetGW, bool deleteDB)
{
    if (!apsCtrl)
    {
        return false;
    }

    if (resetGW)
    {
        uint16_t panId;
        CRYPTO_RandomBytes((unsigned char*)&panId, sizeof(panId));

        uint8_t deviceType = deCONZ::Coordinator;
        quint64 apsUseExtPanId = 0x0000000000000000;
        uint16_t nwkAddress = 0x0000;
        //uint32_t channelMask = 33554432; // 25
        uint8_t curChannel = 11;
        uint8_t securityMode = 3;
        // TODO: original macAddress
        quint64 macAddress = apsCtrl->getParameter(deCONZ::ParamMacAddress);

        if (macAddress == 0)
        {
            return false;
        }

        QByteArray nwkKey(16, '\x00');
        CRYPTO_RandomBytes((unsigned char*)nwkKey.data(), nwkKey.size());

        QByteArray tcLinkKey = QByteArray::fromHex("5a6967426565416c6c69616e63653039");
        uint8_t nwkUpdateId = 1;

        apsCtrl->setParameter(deCONZ::ParamDeviceType, deviceType);
        apsCtrl->setParameter(deCONZ::ParamPredefinedPanId, 1);
        apsCtrl->setParameter(deCONZ::ParamPANID, panId);
        apsCtrl->setParameter(deCONZ::ParamApsUseExtendedPANID, apsUseExtPanId);
        apsCtrl->setParameter(deCONZ::ParamExtendedPANID, macAddress);
        apsCtrl->setParameter(deCONZ::ParamApsAck, 0);
        apsCtrl->setParameter(deCONZ::ParamNwkAddress, nwkAddress);
        //apsCtrl->setParameter(deCONZ::ParamChannelMask, channelMask);
        apsCtrl->setParameter(deCONZ::ParamCurrentChannel, curChannel);
        apsCtrl->setParameter(deCONZ::ParamSecurityMode, securityMode);
        apsCtrl->setParameter(deCONZ::ParamTrustCenterAddress, macAddress);
        apsCtrl->setParameter(deCONZ::ParamNetworkKey, nwkKey);
        apsCtrl->setParameter(deCONZ::ParamTrustCenterLinkKey, tcLinkKey);
        apsCtrl->setParameter(deCONZ::ParamNetworkUpdateId, nwkUpdateId);
        apsCtrl->setParameter(deCONZ::ParamOtauActive, 1);

        // reset endpoints
        QVariantMap epData;

        epData["index"] = 0;
        epData["endpoint"] = "0x1";
        epData["profileId"] = "0x104";
        epData["deviceId"] = "0x5";
        epData["deviceVersion"] = "0x1";
        epData["inClusters"] = QVariantList({ "0x0019", "0x000a" });
        epData["outClusters"] = QVariantList({ "0x0500" });
        apsCtrl->setParameter(deCONZ::ParamHAEndpoint, epData);

        epData.clear();
        epData["index"] = 1;
        epData["endpoint"] = "0xF2";
        epData["profileId"] = "0xA1E0";
        epData["deviceId"] = "0x0064";
        epData["deviceVersion"] = "0x1";
        epData["outClusters"] = QVariantList({ "0x0021" });
        apsCtrl->setParameter(deCONZ::ParamHAEndpoint, epData);
    }

    if (deleteDB)
    {
        QString path = deCONZ::getStorageLocation(deCONZ::ApplicationsDataLocation);
        QString filename = path + "/zll.db";

        QFile file(filename);
        if (file.exists())
        {
            QDateTime now = QDateTime::currentDateTime();
            QString newFilename = path + "zll_" + now.toString(Qt::ISODate) + ".bak";
            if (QFile::copy(filename, newFilename))
            {
                DBG_Printf(DBG_INFO, "db backup success\n");
            }
            else
            {
                DBG_Printf(DBG_INFO, "db backup failed\n");
            }

            if (file.remove())
            {
                DBG_Printf(DBG_INFO, "db deleted %s\n", qPrintable(file.fileName()));
            }
            else
            {
                DBG_Printf(DBG_INFO, "db failed to delete %s\n", qPrintable(file.fileName()));
            }
        }
    }

    return true;
}

