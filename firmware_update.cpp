/*
 * Copyright (c) 2016-2021 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QApplication>
#include <QDesktopServices>
#include <QProcessEnvironment>
#include <QFile>
#include <QDir>
#include <QString>
#include <QProcess>
#ifdef Q_OS_LINUX
#include <unistd.h>
#endif
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"

#define FW_IDLE_TIMEOUT (10 * 1000)
#define FW_WAIT_UPDATE_READY (2) //s
#define FW_IDLE_TIMEOUT_LONG (240 * 1000)
#define FW_WAIT_USER_TIMEOUT (120 * 1000)

/*! Inits the firmware update manager.
 */
void DeRestPluginPrivate::initFirmwareUpdate()
{
    if (!apsCtrl)
    {
        return;
    }

    fwProcess = nullptr;
    fwUpdateState = FW_Idle;

    apsCtrl->setParameter(deCONZ::ParamFirmwareUpdateActive, deCONZ::FirmwareUpdateIdle);

    fwUpdateStartedByUser = false;
    fwUpdateTimer = new QTimer(this);
    fwUpdateTimer->setSingleShot(true);
    connect(fwUpdateTimer, SIGNAL(timeout()),
            this, SLOT(firmwareUpdateTimerFired()));

    fwUpdateTimer->start(5000);
}

/*! Starts the actual firmware update process.
 */
void DeRestPluginPrivate::updateFirmware()
{
    if (gwFirmwareNeedUpdate)
    {
        gwFirmwareNeedUpdate = false;
    }

    Q_ASSERT(apsCtrl);
    if (apsCtrl->getParameter(deCONZ::ParamFirmwareUpdateActive) == deCONZ::FirmwareUpdateIdle ||
        apsCtrl->getParameter(deCONZ::ParamDeviceConnected) == 1)
    {
        DBG_Printf(DBG_INFO, "GW firmware update conditions not met, abort\n");
        fwUpdateState = FW_Idle;
        fwUpdateTimer->start(FW_IDLE_TIMEOUT);
        updateEtag(gwConfigEtag);
        return;
    }

    bool needSudo = true;

    if (fwDeviceName == QLatin1String("ConBee II"))
    {
        needSudo = false;
    }

    QString bin;
    QString gcfFlasherBin = qApp->applicationDirPath() + "/GCFFlasher";
#ifdef Q_OS_WIN
    gcfFlasherBin.append(".exe");
    bin = gcfFlasherBin;
#elif defined(Q_OS_LINUX) && !defined(ARCH_ARM) // on desktop linux

    if (!needSudo || geteuid() == 0)
    {
        bin = QLatin1String("/usr/bin/GCFFlasher_internal.bin");
    }
    else
    {
        bin = QLatin1String("pkexec");
        gcfFlasherBin = QLatin1String("/usr/bin/GCFFlasher_internal");
        fwProcessArgs.prepend(gcfFlasherBin);
    }
#elif defined(Q_OS_OSX)
    // TODO
    // /usr/bin/osascript -e 'do shell script "make install" with administrator privileges'
    bin = "sudo";
    fwProcessArgs.prepend(gcfFlasherBin);
#else
    if (!needSudo || geteuid() == 0)
    {
        bin = QLatin1String("/usr/bin/GCFFlasher_internal.bin");
    }
    else  // on ARM or Raspbian assume we don't need password (todo find a better solution)
    {
        bin = QLatin1String("sudo");
        gcfFlasherBin = QLatin1String("/usr/bin/GCFFlasher_internal");
        fwProcessArgs.prepend(gcfFlasherBin);
    }
#endif

    if (!fwProcess)
    {
        fwProcess = new QProcess(this);
    }

    fwProcessArgs << "-t" << "60" << "-f" << fwUpdateFile;

    fwUpdateState = FW_UpdateWaitFinished;
    updateEtag(gwConfigEtag);
    fwUpdateTimer->start(250);

    DBG_Printf(DBG_INFO, "exec: %s %s\n", qPrintable(bin), qPrintable(fwProcessArgs.join(' ')));

    fwProcess->start(bin, fwProcessArgs);
}

/*! Observes the firmware update process.
 */
void DeRestPluginPrivate::updateFirmwareWaitFinished()
{
    if (fwProcess)
    {
        if (fwProcess->bytesAvailable())
        {
            QByteArray data = fwProcess->readAllStandardOutput();
            DBG_Printf(DBG_INFO, "%s", qPrintable(data));

            if (apsCtrl->getParameter(deCONZ::ParamFirmwareUpdateActive) != deCONZ::FirmwareUpdateRunning)
            {
                if (data.contains("flashing"))
                {
                    apsCtrl->setParameter(deCONZ::ParamFirmwareUpdateActive, deCONZ::FirmwareUpdateRunning);
                }
            }
        }

        if (fwProcess->state() == QProcess::Starting)
        {
            DBG_Printf(DBG_INFO, "GW firmware update starting ..\n");
        }
        else if (fwProcess->state() == QProcess::Running)
        {
            DBG_Printf(DBG_INFO_L2, "GW firmware update running ..\n");
        }
        else if (fwProcess->state() == QProcess::NotRunning)
        {
            if (fwProcess->exitStatus() == QProcess::NormalExit)
            {
                DBG_Printf(DBG_INFO, "GW firmware update exit code %d\n", fwProcess->exitCode());
            }
            else if (fwProcess->exitStatus() == QProcess::CrashExit)
            {
                DBG_Printf(DBG_INFO, "GW firmware update crashed %s\n", qPrintable(fwProcess->errorString()));
            }

            fwProcess->deleteLater();
            fwProcess = nullptr;
        }
    }

    // done
    if (fwProcess == nullptr)
    {
        gwFirmwareVersion = QLatin1String("0x00000000"); // force reread
        fwUpdateStartedByUser = false;
        gwFirmwareNeedUpdate = false;
        updateEtag(gwConfigEtag);
        apsCtrl->setParameter(deCONZ::ParamFirmwareUpdateActive, deCONZ::FirmwareUpdateIdle);
        fwUpdateState = FW_Idle;
        fwUpdateTimer->start(FW_IDLE_TIMEOUT);
        updateEtag(gwConfigEtag);
    }
    else // recheck
    {
        fwUpdateTimer->start(250);
    }
}

/*! Starts the device disconnect so that the serial port is released.
 */
void DeRestPluginPrivate::updateFirmwareDisconnectDevice()
{
    Q_ASSERT(apsCtrl);
//    if (apsCtrl->getParameter(deCONZ::ParamFirmwareUpdateActive) == deCONZ::FirmwareUpdateIdle)
//    {
//        if (apsCtrl->getParameter(deCONZ::ParamDeviceConnected) == 1)
//        {
//            DBG_Printf(DBG_INFO, "GW firmware disconnect device before update\n");
//        }
//    }

    zbConfigGood = QDateTime(); // clear

    if (apsCtrl->getParameter(deCONZ::ParamDeviceConnected) == 1)
    {
        fwUpdateTimer->start(100); // recheck
    }
    else
    {
        DBG_Printf(DBG_INFO, "GW firmware start update (device not connected)\n");
        fwUpdateState = FW_Update;
        fwUpdateTimer->start(0);
        updateEtag(gwConfigEtag);
    }
}

/*! Starts the firmware update.
 */
bool DeRestPluginPrivate::startUpdateFirmware()
{
    fwUpdateStartedByUser = true;
    if (fwUpdateState == FW_WaitUserConfirm)
    {
        apsCtrl->setParameter(deCONZ::ParamFirmwareUpdateActive, deCONZ::FirmwareUpdateRunning);
        updateEtag(gwConfigEtag);
        fwUpdateState = FW_DisconnectDevice;
        fwUpdateTimer->start(100);
        zbConfigGood = QDateTime();
        return true;
    }

    return false;
}

/*! Delayed trigger to update the firmware.
 */
void DeRestPluginPrivate::firmwareUpdateTimerFired()
{
    if (otauLastBusyTimeDelta() < OTA_LOW_PRIORITY_TIME)
    {
        fwUpdateState = FW_Idle;
        fwUpdateTimer->start(FW_IDLE_TIMEOUT);
    }
    else if (fwUpdateState == FW_Idle)
    {
        if (gwFirmwareNeedUpdate)
        {
            gwFirmwareNeedUpdate = false;
            updateEtag(gwConfigEtag);
        }

        if (gwFirmwareVersion == QLatin1String("0x00000000"))
        {
            uint8_t devConnected = apsCtrl->getParameter(deCONZ::ParamDeviceConnected);
            uint32_t fwVersion = apsCtrl->getParameter(deCONZ::ParamFirmwareVersion);

            if (devConnected && fwVersion)
            {
                gwFirmwareVersion = QString("0x%1").arg(fwVersion, 8, 16, QLatin1Char('0'));
                gwConfig["fwversion"] = gwFirmwareVersion;
                updateEtag(gwConfigEtag);
            }
        }

        fwUpdateState = FW_CheckDevices;
        fwUpdateTimer->start(0);
    }
    else if (fwUpdateState == FW_CheckDevices)
    {
        checkFirmwareDevices();
    }
    else if (fwUpdateState == FW_CheckVersion)
    {
        queryFirmwareVersion();
    }
    else if (fwUpdateState == FW_DisconnectDevice)
    {
        updateFirmwareDisconnectDevice();
    }
    else if (fwUpdateState == FW_Update)
    {
        updateFirmware();
    }
    else if (fwUpdateState == FW_UpdateWaitFinished)
    {
        updateFirmwareWaitFinished();
    }
    else if (fwUpdateState == FW_WaitUserConfirm)
    {
        fwUpdateState = FW_Idle;
        fwUpdateTimer->start(FW_IDLE_TIMEOUT);
    }
    else
    {
        fwUpdateState = FW_Idle;
        fwUpdateTimer->start(FW_IDLE_TIMEOUT);
    }
}

/*! Lazy query of firmware version.
    Because the device might not be connected at first optaining the
    firmware version must be delayed.

    If the firmware is older then the min required firmware for the platform
    and a proper firmware update file exists, the API will announce that a
    firmware update is available.
 */
void DeRestPluginPrivate::queryFirmwareVersion()
{
    if (!apsCtrl)
    {
        return;
    }

    { // check for GCFFlasher binary
        QString gcfFlasherBin = qApp->applicationDirPath() + "/GCFFlasher";
#ifdef Q_OS_WIN
        gcfFlasherBin.append(".exe");
#elif defined(Q_OS_LINUX) && !defined(ARCH_ARM)
        gcfFlasherBin = "/usr/bin/GCFFlasher_internal";
#elif defined(Q_OS_OSX)
        // TODO
#else
        gcfFlasherBin = "/usr/bin/GCFFlasher_internal";
#endif

        if (!QFile::exists(gcfFlasherBin))
        {
            DBG_Printf(DBG_INFO, "GW update firmware failed, %s doesn't exist\n", qPrintable(gcfFlasherBin));
            fwUpdateState = FW_Idle;
            fwUpdateTimer->start(FW_IDLE_TIMEOUT_LONG);
            return;
        }
    }

    const quint8 devConnected = apsCtrl->getParameter(deCONZ::ParamDeviceConnected);
    quint32 fwVersion = apsCtrl->getParameter(deCONZ::ParamFirmwareVersion);

    if (fwUpdateFile.isEmpty() && fwVersion == 0 && idleTotalCounter > (IDLE_READ_LIMIT + 10))
    {
        if (fwDeviceName == QLatin1String("ConBee II"))
        {
            fwVersion = FW_ONLY_R21_BOOTLOADER;
        }
    }

    // does the update file exist?
    // todo if the fwVersion is 0, make a guess on which firmware file to select based on device enumerator
    QString fileName;
    if (fwUpdateFile.isEmpty() && fwVersion > 0)
    {
        if (((fwVersion & FW_PLATFORM_MASK) == FW_PLATFORM_AVR) || fwVersion == FW_ONLY_AVR_BOOTLOADER)
        {
            fileName = QString("deCONZ_Rpi_0x%1.bin.GCF").arg(GW_MIN_AVR_FW_VERSION, 8, 16, QLatin1Char('0'));
        }
        else if (((fwVersion & FW_PLATFORM_MASK) == FW_PLATFORM_R21) || fwVersion == FW_ONLY_R21_BOOTLOADER)
        {
            fileName = QString("deCONZ_ConBeeII_0x%1.bin.GCF").arg(GW_MIN_R21_FW_VERSION, 8, 16, QLatin1Char('0'));
        }

        // search in different locations
        std::vector<QString> paths;
#ifdef Q_OS_LINUX
        paths.push_back(QLatin1String("/usr/share/deCONZ/firmware/"));
#endif
        paths.push_back(deCONZ::getStorageLocation(deCONZ::ApplicationsDataLocation) + QLatin1String("/firmware/"));
        paths.push_back(deCONZ::getStorageLocation(deCONZ::HomeLocation) + QLatin1String("/raspbee_firmware/"));
#ifdef Q_OS_OSX
        QDir dir(qApp->applicationDirPath());
        dir.cdUp();
        dir.cd("Resources");
        paths.push_back(dir.path() + "/");
#endif

        std::vector<QString>::const_iterator i = paths.begin();
        std::vector<QString>::const_iterator end = paths.end();
        for (; i != end; ++i)
        {
            if (QFile::exists(*i + fileName))
            {
                fwUpdateFile = *i + fileName;
                DBG_Printf(DBG_INFO, "GW update firmware found: %s\n", qPrintable(fwUpdateFile));
                break;
            }
        }
    }

    if (fwUpdateFile.isEmpty())
    {
        DBG_Printf(DBG_ERROR, "GW update firmware not found: %s\n", qPrintable(fileName));
        fwUpdateState = FW_Idle;
        fwUpdateTimer->start(FW_IDLE_TIMEOUT);
        return;
    }

    Q_ASSERT(!gwFirmwareNeedUpdate);

    if (devConnected == 0 || fwVersion == 0)
    {
        // if even after some time no firmware was detected
        // ASSUME that a device is present and reachable but might not have firmware installed
        if (fwDeviceName == QLatin1String("ConBee II"))
        {
            // ignore
            fwUpdateState = FW_Idle;
            fwUpdateTimer->start(FW_IDLE_TIMEOUT_LONG);
        }
        else if (getUptime() >= FW_WAIT_UPDATE_READY && fwDeviceName == QLatin1String("RaspBee"))
        {
            gwFirmwareVersion = QLatin1String("0x00000000"); // unknown
            gwFirmwareVersionUpdate = QString("0x%1").arg(GW_MIN_AVR_FW_VERSION, 8, 16, QLatin1Char('0'));
            gwConfig["fwversion"] = gwFirmwareVersion;
            gwFirmwareNeedUpdate = true;
            updateEtag(gwConfigEtag);

            fwUpdateState = FW_WaitUserConfirm;
            fwUpdateTimer->start(FW_WAIT_USER_TIMEOUT);
            apsCtrl->setParameter(deCONZ::ParamFirmwareUpdateActive, deCONZ::FirmwareUpdateReadyToStart);

            if (fwUpdateStartedByUser)
            {
                startUpdateFirmware();
            }
        }
        return;
    }
    else if (devConnected || fwVersion == FW_ONLY_AVR_BOOTLOADER)
    {
        QString str = QString("0x%1").arg(fwVersion, 8, 16, QLatin1Char('0'));

        if (gwFirmwareVersion != str)
        {
            gwFirmwareVersion = str;
            gwConfig["fwversion"] = str;
            updateEtag(gwConfigEtag);
        }

        DBG_Printf(DBG_INFO, "GW firmware version: %s\n", qPrintable(gwFirmwareVersion));

        // if the device is detected check that the firmware version is >= min version
        // if fwVersion is FW_ONLY_AVR_BOOTLOADER, there might be no firmware at all, but update is possible
        if (((fwVersion & FW_PLATFORM_MASK) == FW_PLATFORM_AVR) || fwVersion == FW_ONLY_AVR_BOOTLOADER)
        {
            if (fwVersion < GW_MIN_AVR_FW_VERSION)
            {
                gwFirmwareVersionUpdate = QString("0x%1").arg(GW_MIN_AVR_FW_VERSION, 8, 16, QLatin1Char('0'));
                gwFirmwareNeedUpdate = true;
                updateEtag(gwConfigEtag);

                DBG_Printf(DBG_INFO, "GW firmware version shall be updated to: 0x%08x\n", GW_MIN_AVR_FW_VERSION);
                fwUpdateState = FW_WaitUserConfirm;
                fwUpdateTimer->start(FW_WAIT_USER_TIMEOUT);
                apsCtrl->setParameter(deCONZ::ParamFirmwareUpdateActive, deCONZ::FirmwareUpdateReadyToStart);

                bool autoUpdate = false;

                // auto update factory fresh devices with too old or no firmware
                if (fwVersion == FW_ONLY_AVR_BOOTLOADER)
                {
                    autoUpdate = true;
                }
                else if (apsCtrl->getParameter(deCONZ::ParamDeviceName) == QLatin1String("RaspBee") &&
                    !gwSdImageVersion.isEmpty() && nodes.empty() && sensors.size() < 2)
                {
                    autoUpdate = true;
                }

                if (autoUpdate && fwVersion <= GW_AUTO_UPDATE_AVR_FW_VERSION)
                {
                    DBG_Printf(DBG_INFO, "GW firmware start auto update\n");
                    startUpdateFirmware();
                }

                return;
            }
            else
            {
                DBG_Printf(DBG_INFO, "GW firmware version is up to date: 0x%08x\n", fwVersion);
                fwUpdateState = FW_Idle;
                fwUpdateTimer->start(FW_IDLE_TIMEOUT_LONG);
                return;
            }
        }

        // adapted from above AVR handling
        if ((fwVersion & FW_PLATFORM_MASK) == FW_PLATFORM_R21 && fwDeviceName == QLatin1String("ConBee II"))
        {
            if (fwVersion < GW_MIN_R21_FW_VERSION)
            {
                gwFirmwareVersionUpdate = QString("0x%1").arg(GW_MIN_R21_FW_VERSION, 8, 16, QLatin1Char('0'));
                gwFirmwareNeedUpdate = true;
                updateEtag(gwConfigEtag);

                DBG_Printf(DBG_INFO, "GW firmware version shall be updated to: 0x%08x\n", GW_MIN_R21_FW_VERSION);
                fwUpdateState = FW_WaitUserConfirm;
                fwUpdateTimer->start(FW_WAIT_USER_TIMEOUT);
                apsCtrl->setParameter(deCONZ::ParamFirmwareUpdateActive, deCONZ::FirmwareUpdateReadyToStart);

//                bool autoUpdate = false; // TODO refactor when R21 bootloader v2 arrives

                // auto update factory fresh devices with too old or no firmware
                if (gwRunMode.startsWith(QLatin1String("docker")))
                {
                    // TODO needs to be testet
                }
                else if (fwVersion > FW_ONLY_R21_BOOTLOADER && fwVersion <= GW_AUTO_UPDATE_R21_FW_VERSION)
                {
//                    autoUpdate = true;
                }

//                if (autoUpdate && fwVersion <= GW_AUTO_UPDATE_R21_FW_VERSION)
//                {
//                    DBG_Printf(DBG_INFO, "GW firmware start auto update\n");
//                    startUpdateFirmware();
//                }
                return;
            }
            else
            {
                DBG_Printf(DBG_INFO, "GW firmware version is up to date: 0x%08x\n", fwVersion);
                fwUpdateState = FW_Idle;
                fwUpdateTimer->start(FW_IDLE_TIMEOUT_LONG);
                return;
            }
        }

        if (!gwFirmwareVersionUpdate.isEmpty())
        {
            gwFirmwareVersionUpdate.clear();
            updateEtag(gwConfigEtag);
        }
    }

    fwUpdateState = FW_Idle;
    fwUpdateTimer->start(FW_IDLE_TIMEOUT);
}

/*! Checks if devices for firmware update are present.
 */
void DeRestPluginPrivate::checkFirmwareDevices()
{
    fwProcessArgs.clear();

    const quint8 devConnected = apsCtrl->getParameter(deCONZ::ParamDeviceConnected);
    deCONZ::DeviceEnumerator *devEnumerator = deCONZ::DeviceEnumerator::instance();

    if (devConnected == 0)
    {
        devEnumerator->listSerialPorts();
    }

    const std::vector<deCONZ::DeviceEntry> &availPorts = devEnumerator->getList();

    auto i = availPorts.cbegin();
    const auto end = availPorts.cend();

    int raspBeeCount = 0;
    int usbDongleCount = 0;
    QString ttyPath;
    QString serialNumber;

    ttyPath = apsCtrl->getParameter(deCONZ::ParamDevicePath);

    for (; i != end; ++i)
    {
        if (i->friendlyName.contains(QLatin1String("ConBee")))
        {
            usbDongleCount++;
            if (ttyPath.isEmpty())
            {
                ttyPath = i->path;
            }
        }
        else if (i->friendlyName.contains(QLatin1String("RaspBee")))
        {
            raspBeeCount = 1;
            if (ttyPath.isEmpty())
            {
                ttyPath = i->path;
            }
        }

        if (ttyPath == i->path)
        {
            serialNumber = i->serialNumber;
            fwDeviceName = i->friendlyName;
        }
    }

    if (devConnected > 0 && !ttyPath.isEmpty())
    {
        if (!serialNumber.isEmpty())
        {
            fwProcessArgs << "-s" << serialNumber; // GCFFlasher >= 3.2
        }
        else
        {
            fwProcessArgs << "-d" << ttyPath; // GCFFlasher >= 3.x
        }
    }
    else if (usbDongleCount > 1)
    {
        DBG_Printf(DBG_INFO_L2, "GW firmware update too many USB devices connected, abort\n");
    }
    else if (usbDongleCount == 1)
    {
        DBG_Printf(DBG_INFO_L2, "GW firmware update select USB device\n");
#ifndef Q_OS_WIN // windows does append characters  to the serial number for some reason 'A' (TODO)
        if (!serialNumber.isEmpty())
        {
            fwProcessArgs << "-s" << serialNumber;
        }
        else
#endif
        {
            fwProcessArgs << "-d" << "0";
        }
    }
    else if (raspBeeCount > 0 && usbDongleCount == 0 && !ttyPath.isEmpty())
    {
        DBG_Printf(DBG_INFO_L2, "GW firmware update select %s device\n", qPrintable(ttyPath));
        fwProcessArgs << "-d" << "RaspBee";
    }

    if (!fwProcessArgs.isEmpty())
    {
        fwUpdateState = FW_CheckVersion;
        fwUpdateTimer->start(0);
        return;
    }

    fwUpdateState = FW_Idle;
    fwUpdateTimer->start(FW_IDLE_TIMEOUT);
}
