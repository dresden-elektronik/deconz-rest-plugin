/*
 * Copyright (c) 2016 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QApplication>
#include <QDesktopServices>
#include <QFile>
#include <QDir>
#include <QString>
#include <QProcess>
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"

#define FW_IDLE_TIMEOUT (10 * 1000)
#define FW_IDLE_TIMEOUT_LONG (240 * 1000)
#define FW_WAIT_USER_TIMEOUT (60 * 1000)

/*! Inits the firmware update manager.
 */
void DeRestPluginPrivate::initFirmwareUpdate()
{
    fwProcess = 0;
    fwUpdateState = FW_Idle;

    Q_ASSERT(apsCtrl);
    apsCtrl->setParameter(deCONZ::ParamFirmwareUpdateActive, 0);

    fwUpdateTimer = new QTimer(this);
    fwUpdateTimer->setInterval(1000);
    fwUpdateTimer->setSingleShot(true);
    connect(fwUpdateTimer, SIGNAL(timeout()),
            this, SLOT(firmwareUpdateTimerFired()));
    fwUpdateTimer->start(FW_IDLE_TIMEOUT);
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
    if (apsCtrl->getParameter(deCONZ::ParamFirmwareUpdateActive) == 0 ||
        apsCtrl->getParameter(deCONZ::ParamDeviceConnected) == 1)
    {
        DBG_Printf(DBG_INFO, "GW firmware update conditions not met, abort\n");
        fwUpdateState = FW_Idle;
        fwUpdateTimer->start(FW_IDLE_TIMEOUT);
        return;
    }

    QString gcfFlasherBin = qApp->applicationDirPath() + "/GCFFlasher";
#ifdef Q_OS_WIN
    gcfFlasherBin.append(".exe");
    QString bin = gcfFlasherBin;
#else
    QString bin = "sudo";
    fwProcessArgs.prepend(gcfFlasherBin);
#endif

    if (!fwProcess)
    {
        fwProcess = new QProcess(this);
    }

    fwProcessArgs << "-f" << fwUpdateFile;

    fwUpdateState = FW_UpdateWaitFinished;
    fwUpdateTimer->start(1000);

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
        }

        if (fwProcess->state() == QProcess::Starting)
        {
            DBG_Printf(DBG_INFO, "GW firmware update starting ..\n");
        }
        else if (fwProcess->state() == QProcess::Running)
        {
            DBG_Printf(DBG_INFO, "GW firmware update running ..\n");
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
            fwProcess = 0;
        }
    }

    // done
    if (fwProcess == 0)
    {
        apsCtrl->setParameter(deCONZ::ParamFirmwareUpdateActive, 0);
        fwUpdateState = FW_Idle;
        fwUpdateTimer->start(FW_IDLE_TIMEOUT);
    }
    else // recheck
    {
        fwUpdateTimer->start(1000);
    }
}

/*! Starts the device disconnect so that the serial port is released.
 */
void DeRestPluginPrivate::updateFirmwareDisconnectDevice()
{
    Q_ASSERT(apsCtrl);
    if (apsCtrl->getParameter(deCONZ::ParamFirmwareUpdateActive) == 0)
    {
        if (apsCtrl->getParameter(deCONZ::ParamDeviceConnected) == 1)
        {
            DBG_Printf(DBG_INFO, "GW firmware disconnect device before update\n");
        }
        apsCtrl->setParameter(deCONZ::ParamFirmwareUpdateActive, 1);
    }

    if (apsCtrl->getParameter(deCONZ::ParamDeviceConnected) == 1)
    {
        fwUpdateTimer->start(100); // recheck
    }
    else
    {
        DBG_Printf(DBG_INFO, "GW firmware start update (device not connected)\n");
        fwUpdateState = FW_Update;
        fwUpdateTimer->start(0);
    }
}

/*! Starts the firmware update.
 */
bool DeRestPluginPrivate::startUpdateFirmware()
{
    if (fwUpdateState == FW_WaitUserConfirm)
    {
        fwUpdateState = FW_DisconnectDevice;
        fwUpdateTimer->start(100);
        return true;
    }

    return false;
}

/*! Delayed trigger to update the firmware.
 */
void DeRestPluginPrivate::firmwareUpdateTimerFired()
{
    if (fwUpdateState == FW_Idle)
    {
        if (gwFirmwareNeedUpdate)
        {
            gwFirmwareNeedUpdate = false;
            updateEtag(gwConfigEtag);
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
    else // also handle FW_WaitUserConfirm state timeout
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
    Q_ASSERT(apsCtrl);
    if (!apsCtrl)
    {
        return;
    }

    { // check for GCFFlasher binary
        QString gcfFlasherBin = qApp->applicationDirPath() + "/GCFFlasher";
#ifdef Q_OS_WIN
        gcfFlasherBin.append(".exe");
#endif

        if (!QFile::exists(gcfFlasherBin))
        {
            DBG_Printf(DBG_INFO, "GW update firmware failed, %s doesn't exist\n", qPrintable(gcfFlasherBin));
            fwUpdateState = FW_Idle;
            fwUpdateTimer->start(FW_IDLE_TIMEOUT_LONG);
            return;
        }
    }

    // does the update file exist?
    if (fwUpdateFile.isEmpty())
    {
        QString fileName;
        fileName.sprintf("deCONZ_Rpi_0x%08x.bin.GCF", GW_MIN_RPI_FW_VERSION);

        // search in different locations
        std::vector<QString> paths;
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
        DBG_Printf(DBG_ERROR, "GW update firmware not found: %s\n", qPrintable(fwUpdateFile));
        fwUpdateState = FW_Idle;
        fwUpdateTimer->start(FW_IDLE_TIMEOUT);
        return;
    }

    uint8_t devConnected = apsCtrl->getParameter(deCONZ::ParamDeviceConnected);
    uint32_t fwVersion = apsCtrl->getParameter(deCONZ::ParamFirmwareVersion);

    Q_ASSERT(!gwFirmwareNeedUpdate);

    if (devConnected == 0 || fwVersion == 0)
    {
        // if even after 60 seconds no firmware was detected
        // ASSUME that a device is present and reachable but might not have firmware installed
        if (getUptime() >= 60)
        {
            QString str;
            str.sprintf("0x%08x", GW_MIN_RPI_FW_VERSION);

            gwFirmwareVersion = "0x00000000"; // unknown
            gwFirmwareVersionUpdate = str;
            gwConfig["fwversion"] = gwFirmwareVersion;
            gwFirmwareNeedUpdate = true;
            updateEtag(gwConfigEtag);

            fwUpdateState = FW_WaitUserConfirm;
            fwUpdateTimer->start(FW_WAIT_USER_TIMEOUT);
            return;
        }
    }
    else if (devConnected)
    {
        QString str;
        str.sprintf("0x%08x", fwVersion);

        if (gwFirmwareVersion != str)
        {
            gwFirmwareVersion = str;
            gwConfig["fwversion"] = str;
            updateEtag(gwConfigEtag);
        }

        DBG_Printf(DBG_INFO, "GW firmware version: %s\n", qPrintable(gwFirmwareVersion));

        // if the device is detected check that the firmware version is >= min version
        if ((fwVersion & FW_PLATFORM_MASK) == FW_PLATFORM_RPI)
        {
            if (fwVersion < GW_MIN_RPI_FW_VERSION)
            {
                gwFirmwareVersionUpdate.sprintf("0x%08x", GW_MIN_RPI_FW_VERSION);
                gwFirmwareNeedUpdate = true;
                updateEtag(gwConfigEtag);

                DBG_Printf(DBG_INFO, "GW firmware version shall be updated to: 0x%08x\n", GW_MIN_RPI_FW_VERSION);
                fwUpdateState = FW_WaitUserConfirm;
                fwUpdateTimer->start(FW_WAIT_USER_TIMEOUT);
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
    deCONZ::DeviceEnumerator devEnumerator;

    fwProcessArgs.clear();

    devEnumerator.listSerialPorts();
    const std::vector<deCONZ::DeviceEntry> &availPorts = devEnumerator.getList();

    std::vector<deCONZ::DeviceEntry>::const_iterator i = availPorts.begin();
    std::vector<deCONZ::DeviceEntry>::const_iterator end = availPorts.end();

    int raspBeeCount = 0;
    int usbDongleCount = 0;
    QString ttyPath;

    for (; i != end; ++i)
    {
        if (i->friendlyName.contains(QLatin1String("ConBee")))
        {
            usbDongleCount++;
        }
        else if (i->friendlyName.contains(QLatin1String("RaspBee")))
        {
            raspBeeCount = 1;
            ttyPath = i->path;
        }
    }

    if (usbDongleCount > 1)
    {
        DBG_Printf(DBG_INFO, "GW firmware update too many USB devices connected, abort\n");
    }
    else if (usbDongleCount == 1)
    {
        DBG_Printf(DBG_INFO, "GW firmware update select USB device\n");
        fwProcessArgs << "-d" << "0";
    }
    else if (raspBeeCount > 0 && usbDongleCount == 0 && !ttyPath.isEmpty())
    {
        DBG_Printf(DBG_INFO, "GW firmware update select %s device\n", qPrintable(ttyPath));
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
