/*
 * Copyright (c) 2017-2018 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QCoreApplication>
#include <QDebug>
#include <QDesktopServices>
#include <QtPlugin>
#include <QtCore/qmath.h>
#include <QNetworkAccessManager>
#include <QPushButton>
#include <QTextCodec>
#include <QTime>
#include <QTimer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QUrl>
#include <QCryptographicHash>
#include <QFile>
#include <QProcess>
#include <QSettings>
#include <queue>
#include <cmath>
#ifdef ARCH_ARM
  #include <unistd.h>
  #include <sys/reboot.h>
  #include <errno.h>
#endif
#include "colorspace.h"
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "de_web_widget.h"
#include "gateway_scanner.h"
#include "json.h"
#include "poll_manager.h"

const char *HttpStatusOk           = "200 OK"; // OK
const char *HttpStatusAccepted     = "202 Accepted"; // Accepted but not complete
const char *HttpStatusNotModified  = "304 Not Modified"; // For ETag / If-None-Match
const char *HttpStatusBadRequest   = "400 Bad Request"; // Malformed request
const char *HttpStatusUnauthorized = "401 Unauthorized"; // Unauthorized
const char *HttpStatusForbidden    = "403 Forbidden"; // Understand request but no permission
const char *HttpStatusNotFound     = "404 Not Found"; // Requested uri not found
const char *HttpStatusServiceUnavailable = "503 Service Unavailable";
const char *HttpStatusNotImplemented = "501 Not Implemented";
const char *HttpContentHtml        = "text/html; charset=utf-8";
const char *HttpContentCss         = "text/css";
const char *HttpContentJson        = "application/json; charset=utf-8";
const char *HttpContentJS          = "text/javascript";
const char *HttpContentPNG         = "image/png";
const char *HttpContentJPG         = "image/jpg";
const char *HttpContentSVG         = "image/svg+xml";

static int checkZclAttributesDelay = 750;
//static int ReadAttributesLongDelay = 5000;
//static int ReadAttributesLongerDelay = 60000;
static uint MaxGroupTasks = 4;

const quint64 macPrefixMask       = 0xffffff0000000000ULL;

const quint64 ikeaMacPrefix       = 0x000b570000000000ULL;
const quint64 silabsMacPrefix     = 0x90fd9f0000000000ULL;
const quint64 emberMacPrefix      = 0x000d6f0000000000ULL;
const quint64 instaMacPrefix      = 0x000f170000000000ULL;
const quint64 tiMacPrefix         = 0x00124b0000000000ULL;
const quint64 netvoxMacPrefix     = 0x00137a0000000000ULL;
const quint64 boschMacPrefix      = 0x00155f0000000000ULL;
const quint64 jennicMacPrefix     = 0x00158d0000000000ULL;
const quint64 philipsMacPrefix    = 0x0017880000000000ULL;
const quint64 ubisysMacPrefix     = 0x001fee0000000000ULL;
const quint64 deMacPrefix         = 0x00212e0000000000ULL;
const quint64 keenhomeMacPrefix   = 0x0022a30000000000ULL;
const quint64 heimanMacPrefix     = 0x0050430000000000ULL;
const quint64 stMacPrefix         = 0x24fd5b0000000000ULL;
const quint64 osramMacPrefix      = 0x8418260000000000ULL;
const quint64 bjeMacPrefix        = 0xd85def0000000000ULL;
const quint64 xalMacPrefix        = 0xf8f0050000000000ULL;
const quint64 lutronMacPrefix     = 0xffff000000000000ULL;

struct SupportedDevice {
    quint16 vendorId;
    const char *modelId;
    quint64 mac;
};

static const SupportedDevice supportedDevices[] = {
    { VENDOR_BUSCH_JAEGER, "RB01", bjeMacPrefix },
    { VENDOR_BUSCH_JAEGER, "RM01", bjeMacPrefix },
    { VENDOR_BOSCH, "ISW-ZDL1-WP11G", boschMacPrefix },
    { VENDOR_BOSCH, "ISW-ZPR1-WP13", boschMacPrefix },
    { VENDOR_CENTRALITE, "Motion Sensor-A", emberMacPrefix },
    { VENDOR_CENTRALITE, "3325-S", emberMacPrefix },
    { VENDOR_CENTRALITE, "3321-S", emberMacPrefix },
    { VENDOR_NONE, "LM_",  tiMacPrefix },
    { VENDOR_NONE, "LMHT_", tiMacPrefix },
    { VENDOR_NONE, "IR_", tiMacPrefix },
    { VENDOR_NONE, "DC_", tiMacPrefix },
    { VENDOR_NONE, "BX_", tiMacPrefix }, // Climax siren
    { VENDOR_NONE, "PSMD_", tiMacPrefix }, // Climax smart plug
    { VENDOR_NONE, "OJB-IR715-Z", tiMacPrefix },
    { VENDOR_NONE, "902010/21A", tiMacPrefix }, // Bitron: door/window sensor
    { VENDOR_NONE, "902010/25", tiMacPrefix }, // Bitron: smart plug
    { VENDOR_BITRON, "902010/32", emberMacPrefix }, // Bitron: thermostat
    { VENDOR_DDEL, "Lighting Switch", deMacPrefix },
    { VENDOR_DDEL, "Scene Switch", deMacPrefix },
    { VENDOR_DDEL, "FLS-NB1", deMacPrefix },
    { VENDOR_DDEL, "FLS-NB2", deMacPrefix },
    { VENDOR_IKEA, "TRADFRI remote control", ikeaMacPrefix },
    { VENDOR_IKEA, "TRADFRI remote control", silabsMacPrefix },
    { VENDOR_IKEA, "TRADFRI motion sensor", ikeaMacPrefix },
    { VENDOR_IKEA, "TRADFRI wireless dimmer", ikeaMacPrefix },
    { VENDOR_IKEA, "TRADFRI on/off switch", ikeaMacPrefix },
    { VENDOR_INSTA, "Remote", instaMacPrefix },
    { VENDOR_INSTA, "HS_4f_GJ_1", instaMacPrefix },
    { VENDOR_INSTA, "WS_4f_J_1", instaMacPrefix },
    { VENDOR_INSTA, "WS_3f_G_1", instaMacPrefix },
    { VENDOR_NYCE, "3011", emberMacPrefix }, // door/window sensor
    { VENDOR_PHILIPS, "RWL020", philipsMacPrefix }, // Hue dimmer switch
    { VENDOR_PHILIPS, "RWL021", philipsMacPrefix }, // Hue dimmer switch
    { VENDOR_PHILIPS, "SML001", philipsMacPrefix }, // Hue motion sensor
    { VENDOR_JENNIC, "lumi.sensor_ht", jennicMacPrefix },
    { VENDOR_JENNIC, "lumi.weather", jennicMacPrefix },
    { VENDOR_JENNIC, "lumi.sensor_magnet", jennicMacPrefix },
    { VENDOR_JENNIC, "lumi.sensor_motion", jennicMacPrefix },
    { VENDOR_JENNIC, "lumi.sensor_switch.aq2", jennicMacPrefix }, // Xiaomi WXKG11LM 2016
    { VENDOR_JENNIC, "lumi.remote.b1acn01", jennicMacPrefix },    // Xiaomi WXKG11LM 2018
    { VENDOR_JENNIC, "lumi.sensor_switch.aq3", jennicMacPrefix }, // Xiaomi WXKG12LM
    { VENDOR_JENNIC, "lumi.sensor_cube", jennicMacPrefix },
    { VENDOR_JENNIC, "lumi.sensor_86sw1", jennicMacPrefix },      // Xiaomi single button wall switch WXKG03LM 2016
    { VENDOR_JENNIC, "lumi.remote.b186acn01", jennicMacPrefix },  // Xiaomi single button wall switch WXKG03LM 2018
    { VENDOR_JENNIC, "lumi.sensor_86sw2", jennicMacPrefix },      // Xiaomi dual button wall switch WXKG02LM 2016
    { VENDOR_JENNIC, "lumi.remote.b286acn01", jennicMacPrefix },  // Xiaomi dual button wall switch WXKG02LM 2018
    { VENDOR_JENNIC, "lumi.sensor_switch", jennicMacPrefix },     // Xiaomi WXKG11LM and WXKG12LM (fallback)
    { VENDOR_JENNIC, "lumi.ctrl_neutral", jennicMacPrefix }, // Xioami Wall Switch (end-device)
    { VENDOR_JENNIC, "lumi.vibration", jennicMacPrefix }, // Xiaomi Aqara vibration/shock sensor
    { VENDOR_JENNIC, "lumi.sensor_wleak", jennicMacPrefix },
    { VENDOR_JENNIC, "lumi.sensor_smoke", jennicMacPrefix },
    { VENDOR_115F, "lumi.plug", jennicMacPrefix }, // Xiaomi smart plug (router)
    { VENDOR_115F, "lumi.ctrl_ln", jennicMacPrefix}, // Xiaomi Wall Switch (router)
    // { VENDOR_115F, "lumi.curtain", jennicMacPrefix}, // Xiaomi curtain controller (router) - exposed only as light
    { VENDOR_UBISYS, "D1", ubisysMacPrefix },
    { VENDOR_UBISYS, "C4", ubisysMacPrefix },
    { VENDOR_UBISYS, "S2", ubisysMacPrefix },
    { VENDOR_UBISYS, "J1", ubisysMacPrefix },
    { VENDOR_NONE, "Z716A", netvoxMacPrefix },
    // { VENDOR_OSRAM_STACK, "Plug", osramMacPrefix }, // OSRAM plug - exposed only as light
    { VENDOR_OSRAM_STACK, "CO_", heimanMacPrefix }, // Heiman CO sensor
    { VENDOR_OSRAM_STACK, "DOOR_", heimanMacPrefix }, // Heiman door/window sensor
    { VENDOR_OSRAM_STACK, "PIR_", heimanMacPrefix }, // Heiman motion sensor
    { VENDOR_OSRAM_STACK, "GAS_", heimanMacPrefix }, // Heiman gas sensor
    { VENDOR_OSRAM_STACK, "TH-H_", heimanMacPrefix }, // Heiman temperature/humidity sensor
    { VENDOR_OSRAM_STACK, "TH-T_", heimanMacPrefix }, // Heiman temperature/humidity sensor
    { VENDOR_OSRAM_STACK, "SMOK_", heimanMacPrefix }, // Heiman fire sensor - older model
    { VENDOR_OSRAM_STACK, "WATER_", heimanMacPrefix }, // Heiman water sensor
    { VENDOR_LGE, "LG IP65 HMS", emberMacPrefix },
    { VENDOR_EMBER, "SmartPlug", emberMacPrefix }, // Heiman smart plug
    { VENDOR_120B, "Smoke", emberMacPrefix }, // Heiman fire sensor - newer model
    { VENDOR_120B, "WarningDevice", emberMacPrefix }, // Heiman siren
    { VENDOR_LUTRON, "LZL4BWHL01", lutronMacPrefix }, // Lutron LZL-4B-WH-L01 Connected Bulb Remote
    { VENDOR_KEEN_HOME , "SV01-", keenhomeMacPrefix}, // Keen Home Vent
    { VENDOR_INNR, "SP 120", jennicMacPrefix}, // innr smart plug
    { VENDOR_PHYSICAL, "tagv4", stMacPrefix}, // SmartThings Arrival sensor
    { VENDOR_JENNIC, "VMS_ADUROLIGHT", jennicMacPrefix }, // Trust motion sensor ZPIR-8000
    { VENDOR_JENNIC, "ZYCT-202", jennicMacPrefix }, // Trust remote control ZYCT-202
    { VENDOR_INNR, "RC 110", jennicMacPrefix }, // innr remote RC 110
    { VENDOR_VISONIC, "MCT-340", emberMacPrefix }, // Visonic MCT-340 E temperature/motion
    { 0, nullptr, 0 }
};

int TaskItem::_taskCounter = 1; // static rolling taskcounter

ApiRequest::ApiRequest(const QHttpRequestHeader &h, const QStringList &p, QTcpSocket *s, const QString &c) :
    hdr(h), path(p), sock(s), content(c), version(ApiVersion_1), strict(false)
{
    if (hdr.hasKey("Accept"))
    {
        if (hdr.value("Accept").contains("vnd.ddel.v1"))
        {
            version = ApiVersion_1_DDEL;
        }
    }

    // some client may not be prepared for some responses
    if (hdr.hasKey(QLatin1String("User-Agent")))
    {
        QString ua = hdr.value(QLatin1String("User-Agent"));
        if (ua.startsWith(QLatin1String("iConnect")))
        {
            strict = true;
        }
    }
}

/*! Returns the apikey of a request or a empty string if not available
 */
QString ApiRequest::apikey() const
{
    if (path.length() > 1)
    {
        return path.at(1);
    }

    return QLatin1String("");
}

/*! Constructor for pimpl.
    \param parent - the main plugin
 */
DeRestPluginPrivate::DeRestPluginPrivate(QObject *parent) :
    QObject(parent)
{
    pollManager = new PollManager(this);

    databaseTimer = new QTimer(this);
    databaseTimer->setSingleShot(true);

    initEventQueue();
    initResourceDescriptors();

    connect(databaseTimer, SIGNAL(timeout()),
            this, SLOT(saveDatabaseTimerFired()));

    webSocketServer = 0;

    gwScanner = new GatewayScanner(this);
    connect(gwScanner, SIGNAL(foundGateway(QHostAddress,quint16,QString,QString)),
            this, SLOT(foundGateway(QHostAddress,quint16,QString,QString)));
    gwScanner->startScan();

    QString dataPath = deCONZ::getStorageLocation(deCONZ::ApplicationsDataLocation);
    db = 0;
    saveDatabaseItems = 0;
    saveDatabaseIdleTotalCounter = 0;
    dbZclValueMaxAge = 60 * 60; // 1 hour
    sqliteDatabaseName = dataPath + QLatin1String("/zll.db");

    idleLimit = 0;
    idleTotalCounter = IDLE_READ_LIMIT;
    idleLastActivity = 0;
    idleUpdateZigBeeConf = idleTotalCounter + 15;
    sensorIndIdleTotalCounter = 0;
    queryTime = QTime::currentTime();
    udpSock = 0;
    haEndpoint = 0;
    gwGroupSendDelay = deCONZ::appArgumentNumeric("--group-delay", GROUP_SEND_DELAY);
    supportColorModeXyForGroups = false;
    groupDeviceMembershipChecked = false;
    gwLinkButton = false;
    gwWebSocketNotifyAll = true;

    // preallocate memory to get consistent pointers
    nodes.reserve(150);
    sensors.reserve(150);

    fastProbeTimer = new QTimer(this);
    fastProbeTimer->setInterval(500);
    fastProbeTimer->setSingleShot(true);
    connect(fastProbeTimer, SIGNAL(timeout()), this, SLOT(delayedFastEnddeviceProbe()));

    apsCtrl = deCONZ::ApsController::instance();
    DBG_Assert(apsCtrl != 0);

    apsCtrl->setParameter(deCONZ::ParamOtauActive, 0);

    // starttime reference counts from here
    starttimeRef.start();

    initConfig();

    updateEtag(gwConfigEtag);
    updateEtag(gwSensorsEtag);
    updateEtag(gwGroupsEtag);
    updateEtag(gwLightsEtag);

    // set some default might be overwritten by database
    gwAnnounceInterval = ANNOUNCE_INTERVAL;
    gwAnnounceUrl = "http://dresden-light.appspot.com/discover";
    inetDiscoveryManager = 0;

    archProcess = 0;
    zipProcess = 0;

    // lights
    searchLightsState = SearchLightsIdle;
    searchLightsTimeout = 0;

    // sensors
    searchSensorsState = SearchSensorsIdle;
    searchSensorsTimeout = 0;

    ttlDataBaseConnection = 0;
    openDb();
    initDb();
    readDb();
    closeDb();

    initTimezone();

    checkConsistency();

    if (!gwUserParameter.contains("groupssequenceleft"))
    {
        gwUserParameter["groupssequenceleft"] = "[]";
    }
    if (!gwUserParameter.contains("groupssequenceright"))
    {
        gwUserParameter["groupssequenceright"] = "[]";
    }
    if (gwUuid.isEmpty())
    {
        generateGatewayUuid();
    }

    // create default group
    Group group;
    group.setAddress(0);
    group.setName("All");
    groups.push_back(group);

    connect(apsCtrl, SIGNAL(apsdeDataConfirm(const deCONZ::ApsDataConfirm&)),
            this, SLOT(apsdeDataConfirm(const deCONZ::ApsDataConfirm&)));

    connect(apsCtrl, SIGNAL(apsdeDataIndication(const deCONZ::ApsDataIndication&)),
            this, SLOT(apsdeDataIndication(const deCONZ::ApsDataIndication&)));

    connect(apsCtrl, SIGNAL(nodeEvent(deCONZ::NodeEvent)),
            this, SLOT(nodeEvent(deCONZ::NodeEvent)));

    deCONZ::GreenPowerController *gpCtrl = deCONZ::GreenPowerController::instance();

    if (gpCtrl)
    {
        bool ok =
        connect(gpCtrl, SIGNAL(gpDataIndication(deCONZ::GpDataIndication)),
                this, SLOT(gpDataIndication(deCONZ::GpDataIndication)));

        DBG_Assert(ok);
    }

    taskTimer = new QTimer(this);
    taskTimer->setSingleShot(false);
    connect(taskTimer, SIGNAL(timeout()),
            this, SLOT(processTasks()));
    taskTimer->start(100);

    groupTaskTimer = new QTimer(this);
    groupTaskTimer->setSingleShot(false);
    connect(groupTaskTimer, SIGNAL(timeout()),
            this, SLOT(processGroupTasks()));
    groupTaskTimer->start(250);

    verifyRulesTimer = new QTimer(this);
    verifyRulesTimer->setSingleShot(false);
    verifyRulesTimer->setInterval(100);
    connect(verifyRulesTimer, SIGNAL(timeout()),
            this, SLOT(verifyRuleBindingsTimerFired()));
    verifyRulesTimer->start();

    fastRuleCheckTimer = new QTimer(this);
    fastRuleCheckTimer->setInterval(5);
    fastRuleCheckTimer->setSingleShot(true);
    connect(fastRuleCheckTimer, SIGNAL(timeout()),
            this, SLOT(fastRuleCheckTimerFired()));

    checkSensorsTimer = new QTimer(this);
    checkSensorsTimer->setSingleShot(false);
    checkSensorsTimer->setInterval(CHECK_SENSOR_INTERVAL);
    connect(checkSensorsTimer, SIGNAL(timeout()),
            this, SLOT(checkSensorStateTimerFired()));
    checkSensorsTimer->start();
    sensorCheckFast = 0;

    bindingTimer = new QTimer(this);
    bindingTimer->setSingleShot(true);
    bindingTimer->setInterval(1000);
    connect(bindingTimer, SIGNAL(timeout()),
            this, SLOT(bindingTimerFired()));

    bindingTableReaderTimer = new QTimer(this);
    bindingTableReaderTimer->setSingleShot(true);
    bindingTableReaderTimer->setInterval(1000);
    connect(bindingTableReaderTimer, SIGNAL(timeout()),
            this, SLOT(bindingTableReaderTimerFired()));

    bindingToRuleTimer = new QTimer(this);
    bindingToRuleTimer->setSingleShot(true);
    bindingToRuleTimer->setInterval(50);
    connect(bindingToRuleTimer, SIGNAL(timeout()),
            this, SLOT(bindingToRuleTimerFired()));

    lockGatewayTimer = new QTimer(this);
    lockGatewayTimer->setSingleShot(true);
    connect(lockGatewayTimer, SIGNAL(timeout()),
            this, SLOT(lockGatewayTimerFired()));

    openClientTimer = new QTimer(this);
    openClientTimer->setSingleShot(false);
    connect(openClientTimer, SIGNAL(timeout()),
            this, SLOT(openClientTimerFired()));
    openClientTimer->start(1000);

    quint16 wsPort = deCONZ::appArgumentNumeric(QLatin1String("--ws-port"), gwConfig["websocketport"].toUInt());
    webSocketServer = new WebSocketServer(this, wsPort);
    gwConfig["websocketport"] = webSocketServer->port();

    initNetworkInfo();
    initUpnpDiscovery();

    initAuthentification();
    initInternetDicovery();
    initSchedules();
    initPermitJoin();
    initOtau();
    initTouchlinkApi();
    initChangeChannelApi();
    initResetDeviceApi();
    initFirmwareUpdate();
    //restoreWifiState();
    indexRulesTriggers();

    QTimer::singleShot(3000, this, SLOT(initWiFi()));
}

/*! Deconstructor for pimpl.
 */
DeRestPluginPrivate::~DeRestPluginPrivate()
{
    if (inetDiscoveryManager)
    {
        inetDiscoveryManager->deleteLater();
        inetDiscoveryManager = 0;
    }
}

/*! APSDE-DATA.indication callback.
    \param ind - the indication primitive
    \note Will be called from the main application for each incoming indication.
    Any filtering for nodes, profiles, clusters must be handled by this plugin.
 */
void DeRestPluginPrivate::apsdeDataIndication(const deCONZ::ApsDataIndication &ind)
{
    Q_Q(DeRestPlugin);
    if (!q->pluginActive())
    {
        return;
    }

    if ((ind.profileId() == HA_PROFILE_ID) || (ind.profileId() == ZLL_PROFILE_ID))
    {
        deCONZ::ZclFrame zclFrame;

        {
            QDataStream stream(ind.asdu());
            stream.setByteOrder(QDataStream::LittleEndian);
            zclFrame.readFromStream(stream);
        }

        TaskItem task;

        switch (ind.clusterId())
        {
        case GROUP_CLUSTER_ID:
            handleGroupClusterIndication(task, ind, zclFrame);
            break;

        case SCENE_CLUSTER_ID:
            handleSceneClusterIndication(task, ind, zclFrame);
            handleClusterIndicationGateways(ind, zclFrame);
            break;

        case OTAU_CLUSTER_ID:
            otauDataIndication(ind, zclFrame);
            break;

        case COMMISSIONING_CLUSTER_ID:
            handleCommissioningClusterIndication(task, ind, zclFrame);
            break;

        case LEVEL_CLUSTER_ID:
            handleClusterIndicationGateways(ind, zclFrame);
            break;

        case ONOFF_CLUSTER_ID:
             handleOnOffClusterIndication(task, ind, zclFrame);
             handleClusterIndicationGateways(ind, zclFrame);
            break;

        case IAS_ZONE_CLUSTER_ID:
            handleIasZoneClusterIndication(ind, zclFrame);
            break;

        case DE_CLUSTER_ID:
            handleDEClusterIndication(ind, zclFrame);
            break;

        case XAL_CLUSTER_ID:
            handleXalClusterIndication(ind, zclFrame);
            break;

        case TIME_CLUSTER_ID:
            handleTimeClusterIndication(ind, zclFrame);
            break;

        case WINDOW_COVERING_CLUSTER_ID:
            handleWindowCoveringClusterIndication(ind, zclFrame);
            break;

        case THERMOSTAT_CLUSTER_ID:
            handleThermostatClusterIndication(ind, zclFrame);
            break;

        default:
        {
        }
            break;
        }

        handleIndicationSearchSensors(ind, zclFrame);

        if (ind.dstAddressMode() == deCONZ::ApsGroupAddress || ind.clusterId() == VENDOR_CLUSTER_ID ||
            !(zclFrame.frameControl() & deCONZ::ZclFCDirectionServerToClient) ||
            (zclFrame.isProfileWideCommand() && zclFrame.commandId() == deCONZ::ZclReportAttributesId))
        {
            Sensor *sensorNode = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), ind.srcEndpoint());
            if (!sensorNode)
            {
                // No sensorNode found for endpoint - check for multiple endpoints mapped to the same resource
                sensorNode = getSensorNodeForAddress(ind.srcAddress());
                if (sensorNode)
                {
                    if (zclFrame.manufacturerCode() == VENDOR_PHILIPS)
                    {
                        // Hue dimmer switch
                    }
                    else if (sensorNode->modelId().startsWith("D1"))
                    {
                        sensorNode = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), 0x02);
                    }
                    else if (sensorNode->modelId().startsWith("C4"))
                    {
                        sensorNode = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), 0x01);
                    }
                    else if (sensorNode->modelId().startsWith("S2"))
                    {
                        sensorNode = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), 0x03);
                    }
                    // else if (sensorNode->modelId().startsWith("RC 110"))
                    // {
                    //     sensorNode = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), 0x01);
                    // }
                    else
                    {
                        sensorNode = 0; // not supported
                    }
                }
            }

            if (sensorNode)
            {
                sensorNode->rx();
                sensorNode->incrementRxCounter();
                ResourceItem *item = sensorNode->item(RConfigReachable);
                if (item && !item->toBool())
                {
                    item->setValue(true);
                    Event e(RSensors, RConfigReachable, sensorNode->id(), item);
                    enqueueEvent(e);
                }
                checkSensorButtonEvent(sensorNode, ind, zclFrame);
            }
        }

        if (zclFrame.isProfileWideCommand() && zclFrame.commandId() == deCONZ::ZclReportAttributesId)
        {
            zbConfigGood = QDateTime::currentDateTime();
            handleZclAttributeReportIndication(ind, zclFrame);
        }
        else if (zclFrame.isProfileWideCommand() && zclFrame.commandId() == deCONZ::ZclReadAttributesResponseId)
        {
            zbConfigGood = QDateTime::currentDateTime();
        }
        else if (zclFrame.isProfileWideCommand() && zclFrame.commandId() == deCONZ::ZclConfigureReportingResponseId)
        {
            handleZclConfigureReportingResponseIndication(ind, zclFrame);
        }
    }
    else if (ind.profileId() == ZDP_PROFILE_ID)
    {
        deCONZ::ZclFrame zclFrame; // dummy

        switch (ind.clusterId())
        {
        case ZDP_NODE_DESCRIPTOR_RSP_CLID:
        case ZDP_SIMPLE_DESCRIPTOR_RSP_CLID:
        case ZDP_ACTIVE_ENDPOINTS_RSP_CLID:
        {
            handleIndicationSearchSensors(ind, zclFrame);
        }
            break;

        case ZDP_DEVICE_ANNCE_CLID:
        {
            handleDeviceAnnceIndication(ind);
            handleIndicationSearchSensors(ind, zclFrame);
        }
            break;

        case ZDP_IEEE_ADDR_CLID:
        {
            handleIeeeAddressReqIndication(ind);
        }
            break;

        case ZDP_NWK_ADDR_CLID:
        {
            handleNwkAddressReqIndication(ind);
        }
            break;

        case ZDP_MGMT_LQI_RSP_CLID:
        {
            handleMgmtLqiRspIndication(ind);
        }
            break;

        case ZDP_MGMT_BIND_RSP_CLID:
            handleMgmtBindRspIndication(ind);
            break;

        case ZDP_BIND_RSP_CLID:
        case ZDP_UNBIND_RSP_CLID:
            handleBindAndUnbindRspIndication(ind);
            break;

        case ZDP_MGMT_LEAVE_RSP_CLID:
            handleMgmtLeaveRspIndication(ind);
            break;

        default:
            break;
        }

        handleZdpIndication(ind);
    }
    else if (ind.profileId() == DE_PROFILE_ID)
    {
        otauDataIndication(ind, deCONZ::ZclFrame());
    }
    else if (ind.profileId() == ATMEL_WSNDEMO_PROFILE_ID)
    {
        wsnDemoDataIndication(ind);
    }
}

/*! APSDE-DATA.confirm callback.
    \param conf - the confirm primitive
    \note Will be called from the main application for each incoming confirmation,
    even if the APSDE-DATA.request was not issued by this plugin.
 */
void DeRestPluginPrivate::apsdeDataConfirm(const deCONZ::ApsDataConfirm &conf)
{
    pollManager->apsdeDataConfirm(conf);

    std::list<TaskItem>::iterator i = runningTasks.begin();
    std::list<TaskItem>::iterator end = runningTasks.end();

    for (;i != end; ++i)
    {
        TaskItem &task = *i;

        if (task.req.id() != conf.id())
        {
            continue;
        }

        if (conf.dstAddressMode() == deCONZ::ApsNwkAddress &&
            task.req.dstAddressMode() == deCONZ::ApsNwkAddress &&
            conf.dstAddress().hasNwk() && task.req.dstAddress().hasNwk() &&
            conf.dstAddress().nwk() != task.req.dstAddress().nwk())
        {
            DBG_Printf(DBG_INFO, "warn APSDE-DATA.confirm: 0x%02X nwk mismatch\n", conf.id());
            //continue;
        }

        QDateTime now = QDateTime::currentDateTime();

        if (conf.status() != deCONZ::ApsSuccessStatus)
        {
            DBG_Printf(DBG_INFO, "0x%016llX error APSDE-DATA.confirm: 0x%02X on task\n", task.req.dstAddress().ext(), conf.status());
        }
        else if (task.req.dstAddressMode() == deCONZ::ApsGroupAddress &&
                 (task.req.clusterId() == ONOFF_CLUSTER_ID ||
                  task.req.clusterId() == LEVEL_CLUSTER_ID ||
                  task.req.clusterId() == COLOR_CLUSTER_ID))
        {
            quint16 groupId = task.req.dstAddress().group();
            quint16 attrId = 0x0000;
            if (task.req.clusterId() == COLOR_CLUSTER_ID)
            {
                attrId = 0x0003; // currentX
            }

            for (LightNode &l : nodes)
            {
                if (!l.isAvailable() ||
                    !l.lastRx().isValid() /*||
                    l.manufacturerCode() == VENDOR_IKEA ||
                    l.manufacturerCode() == VENDOR_OSRAM ||
                    l.manufacturerCode() == VENDOR_OSRAM_STACK ||
                    l.manufacturer().startsWith(QLatin1String("IKEA")) ||
                    l.manufacturer().startsWith(QLatin1String("OSRAM"))*/)
                {
                    continue;
                }


                // fast poll lights which don't support or have active ZCL reporting
                const NodeValue &val = l.getZclValue(ONOFF_CLUSTER_ID, attrId);
                if ((!val.timestampLastReport.isValid() || val.timestampLastReport.secsTo(now) > (60 * 5)) &&
                    isLightNodeInGroup(&l, groupId))
                {
                    DBG_Printf(DBG_INFO_L2, "\t0x%016llX force poll\n", l.address().ext());
                    queuePollNode(&l);
                }
            }
        }
        else if (task.lightNode)
        {
            switch (task.taskType)
            {
            case TaskSendOnOffToggle:
            case TaskSetLevel:
            case TaskSetXyColor:
            case TaskSetEnhancedHue:
            case TaskSetSat:
            case TaskSetColorTemperature:
            case TaskSetHue:
            case TaskSetHueAndSaturation:
            case TaskIncColorTemperature:
                {
                    DBG_Printf(DBG_INFO, "\t0x%016llX force poll (2)\n", task.lightNode->address().ext());
                    queuePollNode(task.lightNode);
                }
                break;
            default:
                break;
            }
        }

        if (DBG_IsEnabled(DBG_INFO_L2))
        {
            DBG_Printf(DBG_INFO_L2, "Erase task req-id: %u, type: %d zcl seqno: %u send time %d, profileId: 0x%04X, clusterId: 0x%04X\n",
                       task.req.id(), task.taskType, task.zclFrame.sequenceNumber(), idleTotalCounter - task.sendTime, task.req.profileId(), task.req.clusterId());
        }
        runningTasks.erase(i);
        processTasks();
        break;
    }

    if (channelChangeApsRequestId == conf.id() && channelChangeState == CC_WaitConfirm)
    {
        channelChangeSendConfirm(conf.status() == deCONZ::ApsSuccessStatus);
    }
    else if (resetDeviceApsRequestId == conf.id() && resetDeviceState == ResetWaitConfirm)
    {
        resetDeviceSendConfirm(conf.status() == deCONZ::ApsSuccessStatus);
    }

    if (handleMgmtBindRspConfirm(conf))
    {
        return;
    }
}

/*! Process incoming green power button event.
    \param ind - the data indication
 */
void DeRestPluginPrivate::gpProcessButtonEvent(const deCONZ::GpDataIndication &ind)
{
    /*
        PTM 215Z DEMO

        A0 B0
        A1 B1

        DeviceId 0x02 (On/Off Switch)


             A0,B0 Press    0x64 Press   2 of 2
             A0,B0 Release  0x65 Release 2 of 2

        A0 0x10 Scene0      B0 0x22 Toggle
        A1 0x11 Scene1      B1 0x12 Scene2

             A1,B1 Press    0x62 Press   1 of 2
             A1,B1 Release  0x63 Release 1 of 2

     */

    Sensor *sensor = getSensorNodeForAddress(ind.gpdSrcId());
    ResourceItem *item = sensor ? sensor->item(RStateButtonEvent) : 0;

    if (!sensor || !item || sensor->deletedState() == Sensor::StateDeleted)
    {
        return;
    }

    updateSensorEtag(sensor);
    sensor->updateStateTimestamp();
    item->setValue(ind.gpdCommandId());

    Event e(RSensors, RStateButtonEvent, sensor->id(), item);
    enqueueEvent(e);
    enqueueEvent(Event(RSensors, RStateLastUpdated, sensor->id()));
}

/*! Returns the number of tasks for a specific address.
    \param address - the destination address
 */
int DeRestPluginPrivate::taskCountForAddress(const deCONZ::Address &address)
{
    int count = 0;

    {
        std::list<TaskItem>::const_iterator i = tasks.begin();
        std::list<TaskItem>::const_iterator end = tasks.end();

        for (; i != end; ++i)
        {
            if (i->req.dstAddress() == address)
            {
                count++;
            }

        }
    }

    {
        std::list<TaskItem>::const_iterator i = runningTasks.begin();
        std::list<TaskItem>::const_iterator end = runningTasks.end();

        for (; i != end; ++i)
        {
            if (i->req.dstAddress() == address)
            {
                count++;
            }

        }
    }

    return count;
}

/*! Process incoming green power data frame.
    \param ind - the data indication
 */
void DeRestPluginPrivate::gpDataIndication(const deCONZ::GpDataIndication &ind)
{
    switch (ind.gpdCommandId())
    {
    case deCONZ::GpCommandIdScene0:
    case deCONZ::GpCommandIdScene1:
    case deCONZ::GpCommandIdScene2:
    case deCONZ::GpCommandIdScene3:
    case deCONZ::GpCommandIdScene4:
    case deCONZ::GpCommandIdScene5:
    case deCONZ::GpCommandIdScene6:
    case deCONZ::GpCommandIdScene7:
    case deCONZ::GpCommandIdScene8:
    case deCONZ::GpCommandIdScene9:
    case deCONZ::GpCommandIdScene10:
    case deCONZ::GpCommandIdScene11:
    case deCONZ::GpCommandIdScene12:
    case deCONZ::GpCommandIdScene13:
    case deCONZ::GpCommandIdScene14:
    case deCONZ::GpCommandIdScene15:
    case deCONZ::GpCommandIdOn:
    case deCONZ::GpCommandIdOff:
    case deCONZ::GpCommandIdToggle:
    case deCONZ::GpCommandIdRelease:
    case deCONZ::GpCommandIdPress1Of1:
    case deCONZ::GpCommandIdRelease1Of1:
    case deCONZ::GpCommandIdPress1Of2:
    case deCONZ::GpCommandIdRelease1Of2:
    case deCONZ::GpCommandIdPress2Of2:
    case deCONZ::GpCommandIdRelease2Of2:
    {
        gpProcessButtonEvent(ind);
    }
        break;

    case deCONZ::GpCommandIdCommissioning:
    {
        // 1    8-bit enum    GPD DeviceID
        // 1    8-bit bmp     Options
        // 0/1  8-bit bmp     Extended Options
        // 0/16 Security Key  GPD Key
        // 0/4  u32           GPD Key MIC
        // 0/4  u32           GPD outgoing counter

        quint8 gpdDeviceId;
        quint8 gpdKey[16];
        quint32 gpdMIC = 0;
        quint32 gpdOutgoingCounter = 0;
        deCONZ::GPCommissioningOptions options;
        deCONZ::GpExtCommissioningOptions extOptions;
        options.byte = 0;
        extOptions.byte = 0;

        QDataStream stream(ind.payload());
        stream.setByteOrder(QDataStream::LittleEndian);

        if (stream.atEnd()) { return; }
        stream >> gpdDeviceId;

        if (stream.atEnd()) { return; }
        stream >> options.byte;

        if (options.bits.extOptionsField)
        {
            if (stream.atEnd()) { return; }
            stream >> extOptions.byte;
        }

        if (extOptions.bits.gpdKeyPresent)
        {
            for (int i = 0; i < 16; i++)
            {
                if (stream.atEnd()) { return; }
                stream >> gpdKey[i];

            }

            if (extOptions.bits.gpdKeyEncryption)
            {
                // TODO decrypt key

                if (stream.atEnd()) { return; }
                stream >> gpdMIC;
            }
        }

        switch (extOptions.bits.securityLevelCapabilities)
        {
        case 0:
        default:
            break;
        }

        if (extOptions.bits.gpdOutgoingCounterPresent)
        {
            if (stream.atEnd()) { return; }
            stream >> gpdOutgoingCounter;
        }


        SensorFingerprint fp;
        fp.endpoint = GREEN_POWER_ENDPOINT;
        fp.deviceId = gpdDeviceId;
        fp.profileId = GP_PROFILE_ID;
        fp.outClusters.push_back(GREEN_POWER_CLUSTER_ID);

        Sensor *sensor = getSensorNodeForFingerPrint(ind.gpdSrcId(), fp, "ZGPSwitch");

        if (!sensor)
        {
            if (searchSensorsState != SearchSensorsActive)
            {
                return;
            }

            // create new sensor
            Sensor sensorNode;

            if (gpdDeviceId == deCONZ::GpDeviceIdOnOffSwitch)
            {
                sensorNode.setType("ZGPSwitch");
                sensorNode.setModelId("ZGPSWITCH");
                sensorNode.setManufacturer("Philips");
                sensorNode.setSwVersion("1.0");
            }
            else
            {
                DBG_Printf(DBG_INFO, "unsupported green power device 0x%02X\n", gpdDeviceId);
                return;
            }

            sensorNode.address().setExt(ind.gpdSrcId());
            sensorNode.fingerPrint() = fp;
            sensorNode.setUniqueId(generateUniqueId(sensorNode.address().ext(), sensorNode.fingerPrint().endpoint, GREEN_POWER_CLUSTER_ID));
            sensorNode.setMode(Sensor::ModeNone);

            ResourceItem *item;
            item = sensorNode.item(RConfigOn);
            item->setValue(true);

            item = sensorNode.addItem(DataTypeInt32, RStateButtonEvent);
            item->setValue(ind.gpdCommandId());

            if (sensorNode.id().isEmpty())
            {
                openDb();
                sensorNode.setId(QString::number(getFreeSensorId()));
                closeDb();
            }

            if (sensorNode.name().isEmpty())
            {
                sensorNode.setName(QString("Hue Tap %2").arg(sensorNode.id()));
            }

            checkSensorGroup(&sensorNode);

            DBG_Printf(DBG_INFO, "SensorNode %u: %s added\n", sensorNode.id().toUInt(), qPrintable(sensorNode.name()));
            updateSensorEtag(&sensorNode);

            sensorNode.setNeedSaveDatabase(true);
            sensors.push_back(sensorNode);

            Event e(RSensors, REventAdded, sensorNode.id());
            enqueueEvent(e);
            queSaveDb(DB_SENSORS , DB_SHORT_SAVE_DELAY);

            indexRulesTriggers();
        }
        else if (sensor && sensor->deletedState() == Sensor::StateDeleted)
        {
            if (searchSensorsState == SearchSensorsActive)
            {
                sensor->setDeletedState(Sensor::StateNormal);
                checkSensorGroup(sensor);
                sensor->setNeedSaveDatabase(true);
                DBG_Printf(DBG_INFO, "SensorNode %u: %s reactivated\n", sensor->id().toUInt(), qPrintable(sensor->name()));
                updateSensorEtag(sensor);

                Event e(RSensors, REventAdded, sensor->id());
                enqueueEvent(e);
                queSaveDb(DB_SENSORS , DB_SHORT_SAVE_DELAY);
            }
        }
        else
        {
            DBG_Printf(DBG_INFO, "SensorNode %s already known\n", qPrintable(sensor->name()));
        }
    }
        break;

    default:
        break;
    }
}

/*! Returns true if the ZigBee network is connected.
 */
bool DeRestPluginPrivate::isInNetwork()
{
    if (apsCtrl)
    {
        return (apsCtrl->networkState() == deCONZ::InNetwork);
    }
    return false;
}

/*! Creates a error map used in JSON response.
    \param id - error id
    \param ressource example: "/lights/2"
    \param description example: "resource, /lights/2, not available"
    \return the map
 */
QVariantMap DeRestPluginPrivate::errorToMap(int id, const QString &ressource, const QString &description)
{
    QVariantMap map;
    QVariantMap error;
    error["type"] = (double)id;
    error["address"] = ressource;
    error["description"] = description;
    map["error"] = error;

    DBG_Printf(DBG_INFO_L2, "API error %d, %s, %s\n", id, qPrintable(ressource), qPrintable(description));

    return map;
}

/*! Creates a new unique ETag for a resource.
 */
void DeRestPluginPrivate::updateEtag(QString &etag)
{
    QTime time = QTime::currentTime();
#if QT_VERSION < 0x050000
    etag = QString(QCryptographicHash::hash(time.toString().toAscii(), QCryptographicHash::Md5).toHex());
#else
    etag = QString(QCryptographicHash::hash(time.toString().toLatin1(), QCryptographicHash::Md5).toHex());
#endif
    // quotes are mandatory as described in w3 spec
    etag.prepend('"');
    etag.append('"');
}

/*! Returns the system uptime in seconds.
 */
qint64 DeRestPluginPrivate::getUptime()
{
    DBG_Assert(starttimeRef.isValid());

    if (!starttimeRef.isValid())
    {
        starttimeRef.start();
    }

    if (starttimeRef.isValid())
    {
        qint64 uptime = starttimeRef.elapsed();
        if (uptime > 1000)
        {
            return uptime / 1000;
        }
    }

    return 0;
}

/*! Child end-device polled for data.
    \param event - the related node event
 */
void DeRestPluginPrivate::handleMacDataRequest(const deCONZ::NodeEvent &event)
{
    DBG_Assert(event.node());
    if (!event.node())
    {
        return;
    }

    for (auto &s : sensors)
    {
        if (s.deletedState() != Sensor::StateNormal)
        {
            continue;
        }

        if (s.address().ext() != event.node()->address().ext())
        {
            continue;
        }

        s.rx();
        checkSensorNodeReachable(&s, &event);
        //checkSensorBindingsForAttributeReporting(&s);
        if (searchSensorsState == SearchSensorsActive && fastProbeAddr.ext() == s.address().ext())
        {
            delayedFastEnddeviceProbe(&event);
            checkSensorBindingsForClientClusters(&s);
        }

        if (s.lastAttributeReportBind() < (idleTotalCounter - IDLE_ATTR_REPORT_BIND_LIMIT))
        {
            if (checkSensorBindingsForAttributeReporting(&s))
            {
                s.setLastAttributeReportBind(idleTotalCounter);
            }
        }
    }
}

/*! Adds new node(s) to node cache.
    Only supported ZLL and HA nodes will be added.
    \param node - the base for the LightNode
 */
void DeRestPluginPrivate::addLightNode(const deCONZ::Node *node)
{
    DBG_Assert(node != nullptr);
    if (!node)
    {
        return;
    }
    if (node->nodeDescriptor().manufacturerCode() == VENDOR_KEEN_HOME || // Keen Home Vent
        node->nodeDescriptor().manufacturerCode() == VENDOR_JENNIC || // Xiaomi lumi.ctrl_neutral1, lumi.ctrl_neutral2
        node->nodeDescriptor().manufacturerCode() == VENDOR_EMBER || // atsmart Z6-03 switch
        node->nodeDescriptor().manufacturerCode() == VENDOR_NONE) // Climax Siren
    {
        // whitelist
    }
    else if (!node->nodeDescriptor().receiverOnWhenIdle())
    {
        return;
    }

    QList<deCONZ::SimpleDescriptor>::const_iterator i = node->simpleDescriptors().constBegin();
    QList<deCONZ::SimpleDescriptor>::const_iterator end = node->simpleDescriptors().constEnd();

    for (;i != end; ++i)
    {
        bool hasServerOnOff = false;
        bool hasServerLevel = false;
        bool hasServerColor = false;

        for (int c = 0; c < i->inClusters().size(); c++)
        {
            if      (i->inClusters()[c].id() == ONOFF_CLUSTER_ID) { hasServerOnOff = true; }
            else if (i->inClusters()[c].id() == LEVEL_CLUSTER_ID) { hasServerLevel = true; }
            else if (i->inClusters()[c].id() == COLOR_CLUSTER_ID) { hasServerColor = true; }
            else if (i->inClusters()[c].id() == WINDOW_COVERING_CLUSTER_ID) { hasServerOnOff = true; }
        }

        // check if node already exist
        LightNode *lightNode2 = getLightNodeForAddress(node->address(), i->endpoint());

        if (lightNode2)
        {
            if (lightNode2->state() == LightNode::StateDeleted)
            {
                if (searchLightsState == SearchLightsActive || permitJoinFlag)
                {
                    lightNode2->setState(LightNode::StateNormal);
                }
                else
                {
                    continue;
                }
            }

            if (lightNode2->node() != node)
            {
                lightNode2->setNode(const_cast<deCONZ::Node*>(node));
                DBG_Printf(DBG_INFO, "LightNode %s set node %s\n", qPrintable(lightNode2->id()), qPrintable(node->address().toStringExt()));
            }

            lightNode2->setManufacturerCode(node->nodeDescriptor().manufacturerCode());
            ResourceItem *reachable = lightNode2->item(RStateReachable);

            DBG_Assert(reachable);
            bool avail = !node->isZombie();
            if (reachable->toBool() != avail)
            {
                // the node existed before
                // refresh all with new values
                DBG_Printf(DBG_INFO, "LightNode %u: %s updated\n", lightNode2->id().toUInt(), qPrintable(lightNode2->name()));
                reachable->setValue(avail);
                Event e(RLights, RStateReachable, lightNode2->id(), reachable);
                enqueueEvent(e);

                if (avail)
                {
                    lightNode2->enableRead(READ_VENDOR_NAME |
                                           READ_MODEL_ID |
                                           READ_SWBUILD_ID |
                                           READ_COLOR |
                                           READ_LEVEL |
                                           READ_ON_OFF |
                                           READ_GROUPS |
                                           READ_SCENES |
                                           READ_BINDING_TABLE);

                    for (uint32_t j = 0; j < 32; j++)
                    {
                        uint32_t item = 1 << j;
                        if (lightNode2->mustRead(item))
                        {
                            lightNode2->setNextReadTime(item, queryTime);
                            lightNode2->setLastRead(item, idleTotalCounter);
                        }

                    }

                    queryTime = queryTime.addSecs(1);

                    //lightNode2->setLastRead(idleTotalCounter);
                    updateEtag(lightNode2->etag);
                }
            }

            if (lightNode2->uniqueId().isEmpty() || lightNode2->uniqueId().startsWith(QLatin1String("0x")))
            {
                QString uid = generateUniqueId(lightNode2->address().ext(), lightNode2->haEndpoint().endpoint(), 0);
                lightNode2->setUniqueId(uid);
                lightNode2->setNeedSaveDatabase(true);
                updateEtag(lightNode2->etag);
            }

            queuePollNode(lightNode2);
            continue;
        }

        LightNode lightNode;
        lightNode.setNode(nullptr);
        lightNode.item(RStateReachable)->setValue(true);

        // new light node
        // if (searchLightsState != SearchLightsActive)
        // {
        //     return;
        // }

        if (!i->inClusters().isEmpty())
        {
            if (i->profileId() == HA_PROFILE_ID)
            {
                // filter for supported devices
                switch (i->deviceId())
                {
                case DEV_ID_Z30_ONOFF_PLUGIN_UNIT:
                case DEV_ID_Z30_DIMMABLE_PLUGIN_UNIT:
                case DEV_ID_Z30_EXTENDED_COLOR_LIGHT:
                case DEV_ID_Z30_COLOR_TEMPERATURE_LIGHT:
                case DEV_ID_HA_WINDOW_COVERING_DEVICE:
                {
                    if (hasServerOnOff)
                    {
                        lightNode.setHaEndpoint(*i);
                    }
                }
                break;

                case DEV_ID_MAINS_POWER_OUTLET:
                case DEV_ID_HA_ONOFF_LIGHT:
                case DEV_ID_ONOFF_OUTPUT:
                case DEV_ID_LEVEL_CONTROLLABLE_OUTPUT:
                case DEV_ID_HA_DIMMABLE_LIGHT:
                case DEV_ID_HA_COLOR_DIMMABLE_LIGHT:
                case DEV_ID_SMART_PLUG:
                case DEV_ID_ZLL_ONOFF_LIGHT:
                case DEV_ID_ZLL_ONOFF_PLUGIN_UNIT:
                case DEV_ID_ZLL_ONOFF_SENSOR:
    //            case DEV_ID_ZLL_DIMMABLE_LIGHT: // same as DEV_ID_HA_ONOFF_LIGHT
                case DEV_ID_ZLL_DIMMABLE_PLUGIN_UNIT:
                case DEV_ID_ZLL_COLOR_LIGHT:
                case DEV_ID_ZLL_EXTENDED_COLOR_LIGHT:
                case DEV_ID_ZLL_COLOR_TEMPERATURE_LIGHT:
                    {
                        if (hasServerOnOff)
                        {
                            if ((node->address().ext() & macPrefixMask) == jennicMacPrefix &&
                                node->nodeDescriptor().manufacturerCode() == VENDOR_JENNIC && i->endpoint() != 0x02 && i->endpoint() != 0x03)
                            {
                                // TODO better filter for lumi. devices (i->deviceId(), modelid?)
                                // blacklist switch endpoints for lumi.ctrl_neutral1 and lumi.ctrl_neutral1
                            }
                            else
                            {
                                lightNode.setHaEndpoint(*i);
                            }
                        }
                    }
                    break;

                case DEV_ID_ZLL_COLOR_CONTROLLER:
                    {
                        // FIXME special temporary filter to detect xxx 4 key switch
                        if (i->endpoint() == 0x01 && hasServerColor && hasServerLevel)
                        {
                            lightNode.setHaEndpoint(*i);
                            lightNode.item(RStateOn)->setValue(true);
                            break;
                        }
                    }
                    break;

                case DEV_ID_XIAOMI_SMART_PLUG:
                    {
                        if (node->nodeDescriptor().manufacturerCode() == VENDOR_115F &&
                            (i->endpoint() == 0x01 || i->endpoint() == 0x02) && hasServerOnOff)
                        {
                            // Xiaomi lumi.plug and wall switch lumi.ctrl_ln1.aq2, lumi.ctrl_ln2.aq2
                            lightNode.setHaEndpoint(*i);
                        }
                        else if (node->nodeDescriptor().manufacturerCode() == VENDOR_JENNIC &&
                                 (i->endpoint() == 0x02 || i->endpoint() == 0x03) && hasServerOnOff)
                        {
                            // Xiaomi wall switch lumi.ctrl_neutral1, lumi.ctrl_neutral2
                            // TODO exclude endpoint 0x03 for lumi.ctrl_neutral1
                            lightNode.setHaEndpoint(*i);
                        }
                    }
                    break;

                case DEV_ID_IAS_WARNING_DEVICE:
                    {
                        lightNode.setHaEndpoint(*i);
                    }
                    break;

                default:
                    {
                    }
                    break;
                }
            }
            else if (i->profileId() == ZLL_PROFILE_ID)
            {
                // filter for supported devices
                switch (i->deviceId())
                {
                case DEV_ID_ZLL_COLOR_LIGHT:
                case DEV_ID_ZLL_EXTENDED_COLOR_LIGHT:
                case DEV_ID_Z30_EXTENDED_COLOR_LIGHT:
                case DEV_ID_ZLL_COLOR_TEMPERATURE_LIGHT:
                case DEV_ID_Z30_COLOR_TEMPERATURE_LIGHT:
                case DEV_ID_ZLL_DIMMABLE_LIGHT:
                case DEV_ID_ZLL_DIMMABLE_PLUGIN_UNIT:
                case DEV_ID_Z30_DIMMABLE_PLUGIN_UNIT:
                case DEV_ID_ZLL_ONOFF_LIGHT:
                case DEV_ID_ZLL_ONOFF_PLUGIN_UNIT:
                case DEV_ID_Z30_ONOFF_PLUGIN_UNIT:
                //case DEV_ID_ZLL_ONOFF_SENSOR:
                    {
                        if (hasServerOnOff)
                        {
                            lightNode.setHaEndpoint(*i);
                        }
                    }
                    break;

                default:
                    break;
                }
            }
        }

        if (!lightNode.haEndpoint().isValid())
        {
            continue;
        }

        Q_Q(DeRestPlugin);
        lightNode.setNode(const_cast<deCONZ::Node*>(node));
        lightNode.address() = node->address();
        lightNode.setManufacturerCode(node->nodeDescriptor().manufacturerCode());

        QString uid = generateUniqueId(lightNode.address().ext(), lightNode.haEndpoint().endpoint(), 0);
        lightNode.setUniqueId(uid);

        if ((node->address().ext() & macPrefixMask) == deMacPrefix)
        {
            ResourceItem *item = lightNode.addItem(DataTypeUInt32, RConfigPowerup);
            DBG_Assert(item != 0);
            item->setValue(R_POWERUP_RESTORE | R_POWERUP_RESTORE_AT_DAYLIGHT | R_POWERUP_RESTORE_AT_NO_DAYLIGHT);
        }

        openDb();
        loadLightNodeFromDb(&lightNode);
        closeDb();

        if (lightNode.manufacturerCode() == VENDOR_115F)
        {
            if (lightNode.manufacturer() != QLatin1String("LUMI"))
            {
                lightNode.setManufacturerName(QLatin1String("LUMI"));
                lightNode.setNeedSaveDatabase(true);
            }
        }

        if (lightNode.state() == LightNode::StateDeleted)
        {
            if (searchLightsState == SearchLightsActive || permitJoinFlag)
            {
                lightNode.setState(LightNode::StateNormal);
            }
        }

        ResourceItem *reachable = lightNode.item(RStateReachable);
        DBG_Assert(reachable);
        if (reachable)
        {
            reachable->setValue(!node->isZombie());
        }

        if (lightNode.id().isEmpty())
        {
            if (!(searchLightsState == SearchLightsActive || permitJoinFlag))
            {
                // don't add new light node when search is not active
                return;
            }

            openDb();
            lightNode.setId(QString::number(getFreeLightId()));
            closeDb();
            lightNode.setNeedSaveDatabase(true);
        }

        if ((node->address().ext() & macPrefixMask) == osramMacPrefix)
        {
            if (lightNode.manufacturer() != QLatin1String("OSRAM"))
            {
                lightNode.setManufacturerName(QLatin1String("OSRAM"));
                lightNode.setNeedSaveDatabase(true);
            }
        }

        if ((node->address().ext() & macPrefixMask) == philipsMacPrefix)
        {
            if (lightNode.manufacturer() != QLatin1String("Philips"))
            { // correct vendor name, was atmel, de sometimes
                lightNode.setManufacturerName(QLatin1String("Philips"));
                lightNode.setNeedSaveDatabase(true);
            }
        }

        if (lightNode.modelId() == QLatin1String("FLS-PP3 White"))
        { } // only push data from FLS-PP3 color endpoint
        else
        {
            if (lightNode.name().isEmpty())
                lightNode.setName(QString("Light %1").arg(lightNode.id()));

            if (!lightNode.name().isEmpty())
            { q->nodeUpdated(lightNode.address().ext(), QLatin1String("name"), lightNode.name()); }

            if (!lightNode.swBuildId().isEmpty())
            { q->nodeUpdated(lightNode.address().ext(), QLatin1String("version"), lightNode.swBuildId()); }

            if (!lightNode.manufacturer().isEmpty())
            { q->nodeUpdated(lightNode.address().ext(), QLatin1String("vendor"), lightNode.manufacturer()); }

            if (!lightNode.modelId().isEmpty())
            { q->nodeUpdated(lightNode.address().ext(), QLatin1String("modelid"), lightNode.modelId()); }
        }

        // force reading attributes
        lightNode.enableRead(READ_VENDOR_NAME |
                             READ_MODEL_ID |
                             READ_SWBUILD_ID |
                             READ_COLOR |
                             READ_LEVEL |
                             READ_ON_OFF |
                             READ_GROUPS |
                             READ_SCENES |
                             READ_BINDING_TABLE);
        for (uint32_t j = 0; j < 32; j++)
        {
            uint32_t item = 1 << j;
            if (lightNode.mustRead(item))
            {
                lightNode.setNextReadTime(item, queryTime);
                lightNode.setLastRead(item, idleTotalCounter);
            }
        }
        lightNode.setLastAttributeReportBind(idleTotalCounter);
        queryTime = queryTime.addSecs(1);

        DBG_Printf(DBG_INFO, "LightNode %u: %s added\n", lightNode.id().toUInt(), qPrintable(lightNode.name()));

        nodes.push_back(lightNode);
        lightNode2 = &nodes.back();
        queuePollNode(lightNode2);

        if (searchLightsState == SearchLightsActive || permitJoinFlag)
        {
            Event e(RLights, REventAdded, lightNode2->id());
            enqueueEvent(e);
        }

        indexRulesTriggers();

        q->startZclAttributeTimer(checkZclAttributesDelay);
        updateLightEtag(lightNode2);

        if (lightNode2->needSaveDatabase())
        {
            queSaveDb(DB_LIGHTS, DB_LONG_SAVE_DELAY);
        }
    }
}

/*! Force polling if the node has updated simple descriptors in setup phase.
    \param node - the base for the LightNode
 */
void DeRestPluginPrivate::updatedLightNodeEndpoint(const deCONZ::NodeEvent &event)
{
    if (!event.node())
    {
        return;
    }

    for (LightNode &lightNode : nodes)
    {
        if (lightNode.address().ext() != event.node()->address().ext())
        {
            continue;
        }

        if (event.clusterId() != ZDP_SIMPLE_DESCRIPTOR_RSP_CLID)
        {
            continue;
        }

        if (event.endpoint() != lightNode.haEndpoint().endpoint())
        {
            continue;
        }

        lightNode.rx();
        queuePollNode(&lightNode);
    }
}

/*! Checks if a known node changed its reachable state changed.
    \param node - the base for the LightNode
    \return the related LightNode or 0
 */
void DeRestPluginPrivate::nodeZombieStateChanged(const deCONZ::Node *node)
{
    if (!node)
    {
        return;
    }

    bool available = !node->isZombie();

    { // lights
        std::vector<LightNode>::iterator i = nodes.begin();
        std::vector<LightNode>::iterator end = nodes.end();

        for (; i != end; ++i)
        {
            if (i->address().ext() == node->address().ext())
            {
                if (i->node() != node)
                {
                    i->setNode(const_cast<deCONZ::Node*>(node));
                    DBG_Printf(DBG_INFO, "LightNode %s set node %s\n", qPrintable(i->id()), qPrintable(node->address().toStringExt()));
                }

                ResourceItem *item = i->item(RStateReachable);
                DBG_Assert(item != 0);
                if (item && (item->toBool() != available || !item->lastSet().isValid()))
                {
                    if (available && node->endpoints().end() == std::find(node->endpoints().begin(),
                                                                          node->endpoints().end(),
                                                                          i->haEndpoint().endpoint()))
                    {
                        available = false;
                    }

                    if (item && item->toBool() != available)
                    {
                        item->setValue(available);
                        updateLightEtag(&*i);
                        Event e(RLights, RStateReachable, i->id(), item);
                        enqueueEvent(e);
                    }
                }
            }
        }
    }

    { // sensors
        std::vector<Sensor>::iterator i = sensors.begin();
        std::vector<Sensor>::iterator end = sensors.end();

        for (; i != end; ++i)
        {
            if (i->address().ext() == node->address().ext())
            {
                if (i->node() != node)
                {
                    i->setNode(const_cast<deCONZ::Node*>(node));
                    DBG_Printf(DBG_INFO, "Sensor %s set node %s\n", qPrintable(i->id()), qPrintable(node->address().toStringExt()));
                }

                checkSensorNodeReachable(&(*i));
            }
        }
    }
}

/*! Updates/adds a LightNode from a Node.
    If the node does not exist it will be created
    otherwise the values will be checked for change
    and updated in the internal representation.
    \param node - holds up to date data
    \return the updated or added LightNode
 */
LightNode *DeRestPluginPrivate::updateLightNode(const deCONZ::NodeEvent &event)
{
    if (!event.node())
    {
        return 0;
    }

    bool updated = false;
    LightNode *lightNode = getLightNodeForAddress(event.node()->address(), event.endpoint());

    if (!lightNode)
    {
        // was no relevant node
        return 0;
    }

    if (lightNode->node() != event.node())
    {
        lightNode->setNode(const_cast<deCONZ::Node*>(event.node()));
        DBG_Printf(DBG_INFO, "LightNode %s set node %s\n", qPrintable(lightNode->id()), qPrintable(event.node()->address().toStringExt()));
    }

    lightNode->rx();

    ResourceItem *reachable = lightNode->item(RStateReachable);
    if (reachable->toBool())
    {
        if ((event.node()->state() == deCONZ::FailureState) || event.node()->isZombie())
        {
            reachable->setValue(false);
            Event e(RLights, RStateReachable, lightNode->id(), reachable);
            enqueueEvent(e);
            updated = true;
        }
    }
    else
    {
        if (event.node()->state() != deCONZ::FailureState)
        {
            reachable->setValue(true);
            Event e(RLights, RStateReachable, lightNode->id(), reachable);
            enqueueEvent(e);
            updated = true;
        }
    }

    // filter
    if ((event.profileId() != HA_PROFILE_ID) && (event.profileId() != ZLL_PROFILE_ID))
    {
        return lightNode;
    }

    QList<deCONZ::SimpleDescriptor>::const_iterator i = event.node()->simpleDescriptors().constBegin();
    QList<deCONZ::SimpleDescriptor>::const_iterator end = event.node()->simpleDescriptors().constEnd();

    for (;i != end; ++i)
    {
        if (i->endpoint() != lightNode->haEndpoint().endpoint())
        {
            continue;
        }

        if (i->inClusters().isEmpty())
        {
            continue;
        }

        if (i->profileId() == HA_PROFILE_ID)
        {
            switch(i->deviceId())
            {
            case DEV_ID_MAINS_POWER_OUTLET:
            case DEV_ID_SMART_PLUG:
            case DEV_ID_HA_COLOR_DIMMABLE_LIGHT:
            case DEV_ID_ZLL_COLOR_LIGHT:
            case DEV_ID_ZLL_EXTENDED_COLOR_LIGHT:
            case DEV_ID_Z30_EXTENDED_COLOR_LIGHT:
            case DEV_ID_ZLL_COLOR_TEMPERATURE_LIGHT:
            case DEV_ID_Z30_COLOR_TEMPERATURE_LIGHT:
            case DEV_ID_HA_DIMMABLE_LIGHT:
            //case DEV_ID_ZLL_DIMMABLE_LIGHT: // same as DEV_ID_HA_ONOFF_LIGHT
            case DEV_ID_ZLL_DIMMABLE_PLUGIN_UNIT:
            case DEV_ID_Z30_DIMMABLE_PLUGIN_UNIT:
            case DEV_ID_HA_ONOFF_LIGHT:
            case DEV_ID_ONOFF_OUTPUT:
            case DEV_ID_LEVEL_CONTROLLABLE_OUTPUT:
            case DEV_ID_ZLL_ONOFF_LIGHT:
            case DEV_ID_ZLL_ONOFF_PLUGIN_UNIT:
            case DEV_ID_Z30_ONOFF_PLUGIN_UNIT:
            case DEV_ID_HA_WINDOW_COVERING_DEVICE:
            case DEV_ID_ZLL_ONOFF_SENSOR:
            case DEV_ID_XIAOMI_SMART_PLUG:
            case DEV_ID_IAS_WARNING_DEVICE:
                break;

            default:
                continue;
            }
        }
        else if (i->profileId() == ZLL_PROFILE_ID)
        {
            switch(i->deviceId())
            {
            case DEV_ID_ZLL_COLOR_LIGHT:
            case DEV_ID_ZLL_EXTENDED_COLOR_LIGHT:
            case DEV_ID_Z30_EXTENDED_COLOR_LIGHT:
            case DEV_ID_ZLL_COLOR_TEMPERATURE_LIGHT:
            case DEV_ID_Z30_COLOR_TEMPERATURE_LIGHT:
            case DEV_ID_ZLL_DIMMABLE_LIGHT:
            case DEV_ID_ZLL_DIMMABLE_PLUGIN_UNIT:
            case DEV_ID_Z30_DIMMABLE_PLUGIN_UNIT:
            case DEV_ID_ZLL_ONOFF_LIGHT:
            case DEV_ID_ZLL_ONOFF_PLUGIN_UNIT:
            case DEV_ID_Z30_ONOFF_PLUGIN_UNIT:
            case DEV_ID_ZLL_ONOFF_SENSOR:
                break;

            default:
                continue;
            }
        }
        else
        {
            continue;
        }

        // copy whole endpoint as reference
        lightNode->setHaEndpoint(*i);

        QList<deCONZ::ZclCluster>::const_iterator ic = lightNode->haEndpoint().inClusters().constBegin();
        QList<deCONZ::ZclCluster>::const_iterator endc = lightNode->haEndpoint().inClusters().constEnd();

        NodeValue::UpdateType updateType = NodeValue::UpdateInvalid;
        if (event.event() == deCONZ::NodeEvent::UpdatedClusterDataZclRead)
        {
            updateType = NodeValue::UpdateByZclRead;
        }
        else if (event.event() == deCONZ::NodeEvent::UpdatedClusterDataZclReport)
        {
            updateType = NodeValue::UpdateByZclReport;
        }

        for (; ic != endc; ++ic)
        {
            if (updateType == NodeValue::UpdateInvalid)
            {
                break;
            }

            if (ic->id() == COLOR_CLUSTER_ID && (event.clusterId() == COLOR_CLUSTER_ID))
            {
                std::vector<deCONZ::ZclAttribute>::const_iterator ia = ic->attributes().begin();
                std::vector<deCONZ::ZclAttribute>::const_iterator enda = ic->attributes().end();
                for (;ia != enda; ++ia)
                {
                    if (std::find(event.attributeIds().begin(),
                                  event.attributeIds().end(),
                                  ia->id()) == event.attributeIds().end())
                    {
                        continue;
                    }

                    lightNode->setZclValue(updateType, event.clusterId(), ia->id(), ia->numericValue());

                    if (ia->id() == 0x0000) // current hue
                    {
                        uint8_t hue = ia->numericValue().u8;
                        if (lightNode->hue() != hue)
                        {
                            if (hue > 254)
                            {
                                hue = 254;
                            }

                            lightNode->setHue(hue);
                            ResourceItem *item = lightNode->item(RStateHue);
                            if (item && item->toNumber() != lightNode->enhancedHue())
                            {
                                item->setValue(lightNode->enhancedHue());
                                Event e(RLights, RStateHue, lightNode->id(), item);
                                enqueueEvent(e);
                            }

                            updated = true;
                        }
                    }
                    else if (ia->id() == 0x4000) // enhanced current hue
                    {
                        quint16 hue = ia->numericValue().u16;
                        ResourceItem *item = lightNode->item(RStateHue);

                        if (item && item->toNumber() != hue)
                        {
                            item->setValue(hue);
                            Event e(RLights, RStateHue, lightNode->id(), item);
                            enqueueEvent(e);
                            updated = true;
                        }
                    }
                    else if (ia->id() == 0x0001) // current saturation
                    {
                        uint8_t sat = ia->numericValue().u8;
                        ResourceItem *item = lightNode->item(RStateSat);
                        if (item && item->toNumber() != sat)
                        {
                            item->setValue(sat);
                            Event e(RLights, RStateSat, lightNode->id(), item);
                            enqueueEvent(e);
                            updated = true;
                        }
                    }
                    else if (ia->id() == 0x0003) // current x
                    {
                        uint16_t colorX = ia->numericValue().u16;

                        // sanity for colorX
                        if (colorX > 65279) { colorX = 65279; }

                        ResourceItem *item = lightNode->item(RStateX);
                        if (item && item->toNumber() != colorX)
                        {
                            item->setValue(colorX);
                            Event e(RLights, RStateX, lightNode->id(), item);
                            enqueueEvent(e);
                            updated = true;
                        }
                    }
                    else if (ia->id() == 0x0004) // current y
                    {
                        uint16_t colorY = ia->numericValue().u16;
                        // sanity for colorY
                        if (colorY > 65279) { colorY = 65279; }

                        ResourceItem *item = lightNode->item(RStateY);
                        if (item && item->toNumber() != colorY)
                        {
                            item->setValue(colorY);
                            Event e(RLights, RStateY, lightNode->id(), item);
                            enqueueEvent(e);
                            updated = true;
                        }
                    }
                    else if (ia->id() == 0x0007) // color temperature
                    {
                        uint16_t ct = ia->numericValue().u16;
                        ResourceItem *item = lightNode->item(RStateCt);

                        if (!item)
                        {
                            item = lightNode->addItem(DataTypeUInt16, RStateCt);
                            DBG_Assert(item != 0);
                        }

                        if (item && item->toNumber() != ct)
                        {
                            item->setValue(ct);
                            Event e(RLights, RStateCt, lightNode->id(), item);
                            enqueueEvent(e);
                            updated = true;
                        }
                    }
                    else if (ia->id() == 0x0008 || ia->id() == 0x4001) // color mode | enhanced color mode
                    {
                        uint8_t cm = ia->numericValue().u8;
                        {
                            ResourceItem *item = lightNode->item(RConfigColorCapabilities);
                            if (item && item->toNumber() > 0)
                            {
                                quint16 cap = item->toNumber();
                                if (cap == 0x0010 && cm != 2) // color temperature only light
                                {
                                    cm = 2; // fix unsupported color modes (IKEA ct light)
                                }
                            }
                        }

                        const char *modes[4] = {"hs", "xy", "ct", "hs"};

                        if (cm < 4)
                        {
                            ResourceItem *item = lightNode->item(RStateColorMode);
                            if (item && item->toString() != modes[cm])
                            {
                                item->setValue(QVariant(modes[cm]));
                                Event e(RLights, RStateColorMode, lightNode->id());
                                enqueueEvent(e);
                                updated = true;
                            }
                        }
                    }
                    else if (ia->id() == 0x4002) // color loop active
                    {
                        bool colorLoopActive = ia->numericValue().u8 == 0x01;

                        if (lightNode->isColorLoopActive() != colorLoopActive)
                        {
                            lightNode->setColorLoopActive(colorLoopActive);
                            updated = true;
                        }
                    }
                    else if (ia->id() == 0x4004) // color loop time
                    {
                        uint8_t clTime = ia->numericValue().u8;

                        if (lightNode->colorLoopSpeed() != clTime)
                        {
                            lightNode->setColorLoopSpeed(clTime);
                            updated = true;
                        }
                    }
                    else if (ia->id() == 0x400a) // color capabilities
                    {
                        quint16 cap = ia->numericValue().u16;
                        ResourceItem *item = lightNode->addItem(DataTypeUInt16, RConfigColorCapabilities);
                        DBG_Assert(item != 0);
                        if (item && item->toNumber() != cap)
                        {
                            lightNode->setNeedSaveDatabase(true);
                            item->setValue(cap);
                            Event e(RLights, RConfigColorCapabilities, lightNode->id(), item);
                            enqueueEvent(e);
                            updated = true;
                        }
                    }
                    else if (ia->id() == 0x400b) // color temperature min
                    {
                        quint16 cap = ia->numericValue().u16;
                        ResourceItem *item = lightNode->addItem(DataTypeUInt16, RConfigCtMin);
                        DBG_Assert(item != 0);
                        if (item && item->toNumber() != cap)
                        {
                            item->setValue(cap);
                            lightNode->setNeedSaveDatabase(true);
                            Event e(RLights, RConfigCtMin, lightNode->id(), item);
                            enqueueEvent(e);
                            updated = true;
                        }
                    }
                    else if (ia->id() == 0x400c) // color temperature max
                    {
                        quint16 cap = ia->numericValue().u16;
                        ResourceItem *item = lightNode->addItem(DataTypeUInt16, RConfigCtMax);
                        DBG_Assert(item != 0);
                        if (item && item->toNumber() != cap)
                        {
                            item->setValue(cap);
                            lightNode->setNeedSaveDatabase(true);
                            Event e(RLights, RConfigCtMax, lightNode->id(), item);
                            enqueueEvent(e);
                            updated = true;
                        }
                    }
                }
            }
            else if (ic->id() == LEVEL_CLUSTER_ID && (event.clusterId() == LEVEL_CLUSTER_ID))
            {
                std::vector<deCONZ::ZclAttribute>::const_iterator ia = ic->attributes().begin();
                std::vector<deCONZ::ZclAttribute>::const_iterator enda = ic->attributes().end();
                for (;ia != enda; ++ia)
                {
                    if (ia->id() == 0x0000) // current level
                    {
                        uint8_t level = ia->numericValue().u8;
                        ResourceItem *item = lightNode->item(RStateBri);
                        if (item && item->toNumber() != level)
                        {
                            DBG_Printf(DBG_INFO, "0x%016llX level %u --> %u\n", lightNode->address().ext(), (uint)item->toNumber(), level);
                            lightNode->clearRead(READ_LEVEL);
                            item->setValue(level);
                            Event e(RLights, RStateBri, lightNode->id(), item);
                            enqueueEvent(e);
                            updated = true;
                        }
                        lightNode->setZclValue(updateType, event.clusterId(), 0x0000, ia->numericValue());
                        pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().u8);
                        break;
                    }
                }
                break;
            }
            else if (ic->id() == ONOFF_CLUSTER_ID && (event.clusterId() == ONOFF_CLUSTER_ID))
            {
                if (lightNode->modelId().startsWith(QLatin1String("lumi.curtain")))
                {
                    continue; // ignore OnOff cluster
                }
                std::vector<deCONZ::ZclAttribute>::const_iterator ia = ic->attributes().begin();
                std::vector<deCONZ::ZclAttribute>::const_iterator enda = ic->attributes().end();
                for (;ia != enda; ++ia)
                {
                    if (ia->id() == 0x0000) // OnOff
                    {
                        bool on = ia->numericValue().u8;
                        ResourceItem *item = lightNode->item(RStateOn);
                        if (item && item->toBool() != on)
                        {
                            DBG_Printf(DBG_INFO, "0x%016llX onOff %u --> %u\n", lightNode->address().ext(), (uint)item->toNumber(), on);
                            item->setValue(on);
                            Event e(RLights, RStateOn, lightNode->id(), item);
                            enqueueEvent(e);
                            updated = true;
                        }
                        else
                        {
                            // since light event won't trigger a group check, do it here
                            for (const GroupInfo &gi : lightNode->groups())
                            {
                                if (gi.state == GroupInfo::StateInGroup)
                                {
                                    Event e(RGroups, REventCheckGroupAnyOn, int(gi.id));
                                    enqueueEvent(e);
                                }
                            }
                        }
                        lightNode->setZclValue(updateType, event.clusterId(), 0x0000, ia->numericValue());
                        pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().u8);
                        break;
                    }
                }
            }
            else if (ic->id() == BASIC_CLUSTER_ID && (event.clusterId() == BASIC_CLUSTER_ID))
            {
                std::vector<deCONZ::ZclAttribute>::const_iterator ia = ic->attributes().begin();
                std::vector<deCONZ::ZclAttribute>::const_iterator enda = ic->attributes().end();
                for (;ia != enda; ++ia)
                {
                    if (std::find(event.attributeIds().begin(),
                                  event.attributeIds().end(),
                                  ia->id()) == event.attributeIds().end())
                    {
                        continue;
                    }

                    if (ia->id() == 0x0004) // Manufacturer name
                    {
                        QString str = ia->toString();
                        if (!str.isEmpty() && str != lightNode->manufacturer())
                        {
                            lightNode->setManufacturerName(str);
                            lightNode->setNeedSaveDatabase(true);
                            queSaveDb(DB_LIGHTS, DB_LONG_SAVE_DELAY);
                            updated = true;
                        }
                    }
                    else if (ia->id() == 0x0005) // Model identifier
                    {
                        QString str = ia->toString().trimmed();
                        ResourceItem *item = lightNode->item(RAttrModelId);
                        if (item && !str.isEmpty() && str != item->toString())
                        {
                            lightNode->setModelId(str);
                            item->setValue(str);
                            lightNode->setNeedSaveDatabase(true);
                            queSaveDb(DB_LIGHTS, DB_LONG_SAVE_DELAY);
                            updated = true;
                        }
                    }
                    else if (ia->id() == 0x0006) // Date code
                    {
                        QString str = ia->toString();
                        ResourceItem *item = lightNode->item(RAttrSwVersion);

                        if (item && !str.isEmpty() && str != item->toString())
                        {
                            item->setValue(str);
                            lightNode->setSwBuildId(str);
                            lightNode->setNeedSaveDatabase(true);
                            queSaveDb(DB_LIGHTS, DB_LONG_SAVE_DELAY);
                            updated = true;
                        }
                    }
                    else if (ia->id() == 0x4000) // Software build identifier
                    {
                        QString str = ia->toString();
                        ResourceItem *item = lightNode->item(RAttrSwVersion);

                        if (item && !str.isEmpty() && str != item->toString())
                        {
                            item->setValue(str);
                            lightNode->setSwBuildId(str);
                            lightNode->setNeedSaveDatabase(true);
                            queSaveDb(DB_LIGHTS, DB_LONG_SAVE_DELAY);
                            updated = true;
                        }
                    }
                }
            }
        }

        break;
    }

    if (updated)
    {
        updateEtag(lightNode->etag);
        updateEtag(gwConfigEtag);
        lightNode->setNeedSaveDatabase(true);
        saveDatabaseItems |= DB_LIGHTS;
    }

    return lightNode;
}

/*! Returns a LightNode for a given MAC or NWK address or 0 if not found.
 */
LightNode *DeRestPluginPrivate::getLightNodeForAddress(const deCONZ::Address &addr, quint8 endpoint)
{
    std::vector<LightNode>::iterator i = nodes.begin();
    std::vector<LightNode>::iterator end = nodes.end();

    if (addr.hasExt())
    {
        for (; i != end; ++i)
        {
            if (i->address().ext() == addr.ext())
            {
                if ((endpoint == 0) || (endpoint == i->haEndpoint().endpoint()))
                {
                    return &(*i);
                }
            }
        }
    }
    else if (addr.hasNwk())
    {
        for (; i != end; ++i)
        {
            if (i->address().nwk() == addr.nwk())
            {
                if ((endpoint == 0) || (endpoint == i->haEndpoint().endpoint()))
                {
                    return &(*i);
                }
            }
        }
    }

    return 0;
}

/*! Returns the number of Endpoints of a device.
 */
int DeRestPluginPrivate::getNumberOfEndpoints(quint64 extAddr)
{
    int count = 0;
    std::vector<LightNode>::iterator i;
    std::vector<LightNode>::iterator end = nodes.end();

    for (i = nodes.begin(); i != end; ++i)
    {
        if (i->address().ext() == extAddr)
        {
            count++;
        }
    }

    return count;
}

/*! Returns a LightNode for its given \p id or uniqueid, or 0 if not found.
 */
LightNode *DeRestPluginPrivate::getLightNodeForId(const QString &id)
{
    std::vector<LightNode>::iterator i;
    std::vector<LightNode>::iterator end = nodes.end();

    if (id.length() < MIN_UNIQUEID_LENGTH)
    {
        for (i = nodes.begin(); i != end; ++i)
        {
            if (i->id() == id)
            {
                return &*i;
            }
        }
    }
    else
    {
        for (i = nodes.begin(); i != end; ++i)
        {
            if (i->uniqueId() == id)
            {
                return &*i;
            }
        }
    }

    return nullptr;
}

/*! Returns a Rule for its given \p id or 0 if not found.
 */
Rule *DeRestPluginPrivate::getRuleForId(const QString &id)
{
    std::vector<Rule>::iterator i;
    std::vector<Rule>::iterator end = rules.end();

    for (i = rules.begin(); i != end; ++i)
    {
        if (i->id() == id && i->state() != Rule::StateDeleted)
        {
            return &(*i);
        }
    }

    end = rules.end();

    for (i = rules.begin(); i != end; ++i)
    {
        if (i->id() == id)
        {
            return &(*i);
        }
    }

    return 0;
}

/*! Returns a Rule for its given \p name or 0 if not found.
 */
Rule *DeRestPluginPrivate::getRuleForName(const QString &name)
{
    std::vector<Rule>::iterator i;
    std::vector<Rule>::iterator end = rules.end();

    for (i = rules.begin(); i != end; ++i)
    {
        if (i->name() == name)
        {
            return &(*i);
        }
    }

    return 0;
}

/*! Checks if a SensorNode is reachable.
    \param sensor - the SensorNode
    \param event - the related NodeEvent (optional)
 */
void DeRestPluginPrivate::checkSensorNodeReachable(Sensor *sensor, const deCONZ::NodeEvent *event)
{
    Q_UNUSED(event);
    if (!sensor || sensor->deletedState() != Sensor::StateNormal)
    {
        return;
    }

    bool updated = false;
    bool reachable = false;
    QDateTime now = QDateTime::currentDateTime();

    if (!sensor->fingerPrint().hasEndpoint())
    {
        reachable = true; // assumption for GP device
    }
    if (sensor->node() && !sensor->node()->nodeDescriptor().receiverOnWhenIdle() &&
        sensor->lastRx().isValid() &&
        sensor->lastRx().secsTo(now) < (60 * 60 * 24)) // if end device was active in last 24 hours
    {
        reachable = true;
    }
    else if (sensor->node() && !sensor->node()->isZombie())
    {
        // look if fingerprint endpoint is in active endpoint list
        std::vector<quint8>::const_iterator it;

        it = std::find(sensor->node()->endpoints().begin(),
                       sensor->node()->endpoints().end(),
                       sensor->fingerPrint().endpoint);

        if (it != sensor->node()->endpoints().end())
        {
            if (sensor->lastRx().isValid() && sensor->lastRx().secsTo(now) < (60 * 60 * 24))
            {
                reachable = true;
            }

            // check that all clusters from fingerprint are present
            for (const deCONZ::SimpleDescriptor &sd : sensor->node()->simpleDescriptors())
            {
                if (!reachable)
                {
                    break;
                }

                if (sd.endpoint() != sensor->fingerPrint().endpoint)
                {
                    continue;
                }

                for (quint16 clusterId : sensor->fingerPrint().inClusters)
                {
                    bool found = false;
                    for (const deCONZ::ZclCluster &cl : sd.inClusters())
                    {
                        if (clusterId == cl.id())
                        {
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                    {
                        reachable = false;
                        break;
                    }
                }

                for (quint16 clusterId : sensor->fingerPrint().outClusters)
                {
                    bool found = false;
                    for (const deCONZ::ZclCluster &cl : sd.outClusters())
                    {
                        if (clusterId == cl.id())
                        {
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                    {
                        reachable = false;
                        break;
                    }
                }

            }
        }
    }

    ResourceItem *item = sensor->item(RConfigReachable);

    if (reachable)
    {
        if (item && !item->toBool())
        {
            // the node existed before
            // refresh all with new values
            DBG_Printf(DBG_INFO_L2, "SensorNode id: %s (%s) available\n", qPrintable(sensor->id()), qPrintable(sensor->name()));
            if (sensor->node() && sensor->node()->nodeDescriptor().receiverOnWhenIdle())
            {
                sensor->setNextReadTime(READ_BINDING_TABLE, queryTime);
                sensor->enableRead(READ_BINDING_TABLE/* | READ_MODEL_ID | READ_SWBUILD_ID | READ_VENDOR_NAME*/);
                queryTime = queryTime.addSecs(5);
            }
            //sensor->setLastRead(READ_BINDING_TABLE, idleTotalCounter);
            //checkSensorBindingsForAttributeReporting(sensor);

            updated = true;
/*
            if (event &&
                (event->event() == deCONZ::NodeEvent::UpdatedClusterDataZclRead ||
                 event->event() == deCONZ::NodeEvent::UpdatedClusterDataZclReport))
            {
            }
            else if (sensor->rxCounter() == 0)
            {
                reachable = false; // wait till received something from sensor
            }
*/
        }
    }
    else
    {
        if (item && item->toBool())
        {
            DBG_Printf(DBG_INFO, "SensorNode id: %s (%s) no longer available\n", qPrintable(sensor->id()), qPrintable(sensor->name()));
            updated = true;
        }
    }

    if (item && (item->toBool() != reachable || !item->lastSet().isValid()))
    {
        item->setValue(reachable);
        Event e(RSensors, RConfigReachable, sensor->id(), item);
        enqueueEvent(e);
    }

    if (updated)
    {
        updateSensorEtag(sensor);
    }
}

void DeRestPluginPrivate::checkSensorButtonEvent(Sensor *sensor, const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame)
{
    DBG_Assert(sensor != 0);

    if (!sensor)
    {
        return;
    }

    bool checkReporting = false;
    const Sensor::ButtonMap *buttonMap = sensor->buttonMap();
    if (!buttonMap)
    {
        quint8 pl0 = zclFrame.payload().isEmpty() ? 0 : zclFrame.payload().at(0);
        DBG_Printf(DBG_INFO, "no button map for: %s ep: 0x%02X cl: 0x%04X cmd: 0x%02X pl[0]: 0%02X\n",
                   qPrintable(sensor->modelId()), ind.srcEndpoint(), ind.clusterId(), zclFrame.commandId(), pl0);
        return;
    }

    checkInstaModelId(sensor);

    // DE Lighting Switch: probe for mode changes
    if (sensor->modelId() == QLatin1String("Lighting Switch") && ind.dstAddressMode() == deCONZ::ApsGroupAddress)
    {
        Sensor::SensorMode mode = sensor->mode();

        if (ind.srcEndpoint() == 2 && mode != Sensor::ModeTwoGroups)
        {
            mode = Sensor::ModeTwoGroups;
        }
        else if (ind.clusterId() == SCENE_CLUSTER_ID && mode != Sensor::ModeScenes)
        {
            mode = Sensor::ModeScenes;
        }
        else if (ind.clusterId() == COLOR_CLUSTER_ID && mode != Sensor::ModeColorTemperature)
        {
            mode = Sensor::ModeColorTemperature;
        }

        Sensor *other = getSensorNodeForAddressAndEndpoint(sensor->address(), (sensor->fingerPrint().endpoint == 2) ? 1 : 2);

        if (mode != sensor->mode())
        {
            sensor->setMode(mode);
            updateSensorEtag(sensor);
            sensor->setNeedSaveDatabase(true);
            queSaveDb(DB_SENSORS, DB_SHORT_SAVE_DELAY);

            // set changed mode for sensor endpoints 1 and 2
            if (other)
            {
                other->setMode(mode);
                other->setNeedSaveDatabase(true);
                updateSensorEtag(other);
            }
        }

        if (other && ind.srcEndpoint() == 2 && other->fingerPrint().endpoint == 1)
        {   // forward button events 300x and 400x to first endpoint sensor
            checkSensorButtonEvent(other, ind, zclFrame);
        }
    }
    // Busch-Jaeger
    else if (sensor->modelId() == QLatin1String("RM01") || sensor->modelId() == QLatin1String("RB01"))
    {
        // setup during add sensor
    }
    else if (sensor->modelId() == QLatin1String("TRADFRI remote control"))
    {
        checkReporting = true;
        if (sensor->mode() != Sensor::ModeColorTemperature) // only supported mode yet
        {
            sensor->setMode(Sensor::ModeColorTemperature);
            updateSensorEtag(sensor);
        }
    }
    else if (sensor->modelId() == QLatin1String("TRADFRI wireless dimmer"))
    {
        if (sensor->mode() != Sensor::ModeDimmer)
        {
            sensor->setMode(Sensor::ModeDimmer);
        }
    }
    else if (sensor->modelId() == QLatin1String("TRADFRI on/off switch"))
    {
        checkReporting = true;
    }
    else if (sensor->modelId() == QLatin1String("TRADFRI motion sensor"))
    {
        checkReporting = true;
    }
    else if (sensor->modelId().startsWith(QLatin1String("RWL02"))) // Hue dimmer switch
    {
        checkReporting = true;
    }
    else if (ind.dstAddressMode() == deCONZ::ApsGroupAddress)
    {
        if (sensor->mode() == Sensor::ModeTwoGroups) // only supported for DE Lighting Switch
        {
            sensor->setMode(Sensor::ModeScenes);
            updateSensorEtag(sensor);
        }
    }

    if (ind.dstAddressMode() == deCONZ::ApsGroupAddress)
    {
        QStringList gids;
        ResourceItem *item = sensor->addItem(DataTypeString, RConfigGroup);

        quint16 groupId = ind.dstAddress().group();

        if (sensor->modelId() == QLatin1String("Lighting Switch"))
        {
            // adjust groupId for endpoints
            // ep 1: <gid>
            // ep 2: <gid> + 1
            if (sensor->fingerPrint().endpoint == 2 && ind.srcEndpoint() == 1)
            {
                groupId++;
            }
            else if (sensor->fingerPrint().endpoint == 1 && ind.srcEndpoint() == 2)
            {
                groupId--;
            }
        }

        QString gid = QString::number(groupId);

        if (item)
        {
            gids = item->toString().split(',');
        }

        if (sensor->manufacturer() == QLatin1String("ubisys"))
        {
            // TODO
        }
        // if (sensor->modelId().startsWith("RC 110")) // innr remote
        // {
        //     // 7 controller endpoints: 0x01, 0x03, 0x04, ..., 0x08
        //     if (gids.length() != 7)
        //     {
        //         // initialise list of groups: one for each endpoint
        //         gids = QStringList();
        //         gids << "0" << "0" << "0" << "0" << "0" << "0" << "0";
        //     }
        //
        //     // check group corresponding to source endpoint
        //     int i = ind.srcEndpoint();
        //     i -= i == 1 ? 1 : 2;
        //     if (gids.value(i) != gid)
        //     {
        //         // replace group corresponding to source endpoint
        //         gids.replace(i, gid);
        //         item->setValue(gids.join(','));
        //         sensor->setNeedSaveDatabase(true);
        //         updateSensorEtag(sensor);
        //         enqueueEvent(Event(RSensors, RConfigGroup, sensor->id(), item));
        //     }
        //
        //     Event e(RSensors, REventValidGroup, sensor->id());
        //     enqueueEvent(e);
        // }
        else
        {
            if (!gids.contains(gid))
            {
                item->setValue(gid);
                sensor->setNeedSaveDatabase(true);
                updateSensorEtag(sensor);
                enqueueEvent(Event(RSensors, RConfigGroup, sensor->id(), item));
            }

            Event e(RSensors, REventValidGroup, sensor->id());
            enqueueEvent(e);
        }
    }

    bool ok = false;
    while (buttonMap->mode != Sensor::ModeNone && !ok)
    {
        if (buttonMap->mode == sensor->mode() &&
            buttonMap->endpoint == ind.srcEndpoint() &&
            buttonMap->clusterId == ind.clusterId() &&
            buttonMap->zclCommandId == zclFrame.commandId())
        {
            ok = true;

            if (zclFrame.isProfileWideCommand() &&
                zclFrame.commandId() == deCONZ::ZclReportAttributesId &&
                zclFrame.payload().size() >= 4)
            {
                QDataStream stream(zclFrame.payload());
                stream.setByteOrder(QDataStream::LittleEndian);
                quint16 attrId;
                quint8 dataType;
                stream >> attrId;
                stream >> dataType;

                // Xiaomi
                if (ind.clusterId() == ONOFF_CLUSTER_ID && sensor->manufacturer() == QLatin1String("LUMI"))
                {
                    ok = false;
                    // payload: u16 attrId, u8 datatype, u8 data
                    if (attrId == 0x0000 && dataType == 0x10 && // onoff attribute
                        buttonMap->zclParam0 == zclFrame.payload().at(3))
                    {
                        ok = true;
                    }
                    else if (attrId == 0x8000 && dataType == 0x20 && // custom attribute for multi press
                        buttonMap->zclParam0 == zclFrame.payload().at(3))
                    {
                        ok = true;
                    }
                }
                else if (ind.clusterId() == DOOR_LOCK_CLUSTER_ID && sensor->manufacturer() == QLatin1String("LUMI"))
                {
                    ok = false;
                    if (attrId == 0x0055 && dataType == 0x21 && // Xiaomi non-standard attribute
                        buttonMap->zclParam0 == zclFrame.payload().at(3))
                    {
                        ok = true;
                    }
                }
            }
            else if (zclFrame.isProfileWideCommand())
            {
            }
            else if (ind.clusterId() == SCENE_CLUSTER_ID && zclFrame.commandId() == 0x05) // recall scene
            {
                ok = false; // payload: groupId, sceneId
                if (zclFrame.payload().size() >= 3 && buttonMap->zclParam0 == zclFrame.payload().at(2))
                {
                    ok = true;
                }
            }
            else if (ind.clusterId() == SCENE_CLUSTER_ID &&
                     sensor->modelId().startsWith(QLatin1String("TRADFRI"))) // IKEA non-standard scene
            {
                ok = false;
                if (zclFrame.commandId() == 0x07 || // short release
                    zclFrame.commandId() == 0x08)   // hold
                {
                    if (zclFrame.payload().size() >= 1 && buttonMap->zclParam0 == zclFrame.payload().at(0)) // next, prev scene
                    {
                        sensor->previousDirection = buttonMap->zclParam0;
                        ok = true;
                    }
                }
                else if (zclFrame.commandId() == 0x09) // long release
                {
                    if (buttonMap->zclParam0 == sensor->previousDirection)
                    {
                        sensor->previousDirection = 0xFF;
                        ok = true;
                    }
                }
            }
            else if (ind.clusterId() == VENDOR_CLUSTER_ID && zclFrame.manufacturerCode() == VENDOR_PHILIPS && zclFrame.commandId() == 0x00) // Philips dimmer switch non-standard
            {
                ok = false;
                if (zclFrame.payload().size() >= 8)
                {
                    deCONZ::NumericUnion val = {0};
                    val.u8 = zclFrame.payload().at(0) << 4 /*button*/ | zclFrame.payload().at(4); // action
                    if (buttonMap->zclParam0 == val.u8)
                    {
                        ok = true;
                        sensor->setZclValue(NodeValue::UpdateByZclReport, VENDOR_CLUSTER_ID, 0x0000, val);
                    }
                }
            }
            else if (ind.clusterId() == LEVEL_CLUSTER_ID &&
                     (zclFrame.commandId() == 0x01 ||  // move
                      zclFrame.commandId() == 0x02 ||  // step
                      zclFrame.commandId() == 0x04 ||  // move to level (with on/off)
                      zclFrame.commandId() == 0x05 ||  // move (with on/off)
                      zclFrame.commandId() == 0x06))   // step (with on/off)
            {
                ok = false;
                if (zclFrame.payload().size() >= 1 && buttonMap->zclParam0 == zclFrame.payload().at(0)) // direction
                {
                    sensor->previousDirection = zclFrame.payload().at(0);
                    ok = true;
                }
            }
            else if (ind.clusterId() == LEVEL_CLUSTER_ID &&
                       (zclFrame.commandId() == 0x03 ||  // stop
                        zclFrame.commandId() == 0x07) )  // stop (with on/off)
            {
                ok = false;
                if (buttonMap->zclParam0 == sensor->previousDirection) // direction of previous move/step
                {
                    sensor->previousDirection = 0xFF;
                    ok = true;
                }
            }
            else if (ind.clusterId() == COLOR_CLUSTER_ID &&
                     (zclFrame.commandId() == 0x4b && zclFrame.payload().size() >= 7) )  // move to color temperature
            {
                ok = false;
                // u8 move mode
                // u16 rate
                // u16 ctmin = 0
                // u16 ctmax = 0
                quint8 moveMode = zclFrame.payload().at(0);
                quint16 param = moveMode;

                if (moveMode == 0x01 || moveMode == 0x03)
                {
                    sensor->previousDirection = moveMode;
                }
                else if (moveMode == 0x00)
                {
                    param = sensor->previousDirection;
                    param <<= 4;
                }

                // byte-2 most likely 0, but include anyway
                param |= (quint16)zclFrame.payload().at(2) & 0xff;
                param <<= 8;
                param |= (quint16)zclFrame.payload().at(1) & 0xff;

                if (buttonMap->zclParam0 == param)
                {
                    if (moveMode == 0x00)
                    {
                        sensor->previousDirection = 0xFF;
                    }
                    ok = true;
                }
            }

            if (ok)
            {
                DBG_Printf(DBG_INFO, "button %u %s\n", buttonMap->button, buttonMap->name);
                ResourceItem *item = sensor->item(RStateButtonEvent);
                if (item)
                {
                    if (item->toNumber() == buttonMap->button)
                    {
                        QDateTime now = QDateTime::currentDateTime();
                        const auto dt = item->lastSet().msecsTo(now);

                        if (dt > 0 && dt < 500)
                        {
                            DBG_Printf(DBG_INFO, "button %u %s, discard too fast event (dt = %d)\n", buttonMap->button, buttonMap->name, dt);
                            break;
                        }
                    }

                    item->setValue(buttonMap->button);

                    Event e(RSensors, RStateButtonEvent, sensor->id(), item);
                    enqueueEvent(e);
                    updateSensorEtag(sensor);
                    sensor->updateStateTimestamp();
                    sensor->setNeedSaveDatabase(true);
                    enqueueEvent(Event(RSensors, RStateLastUpdated, sensor->id()));
                }

                item = sensor->item(RStatePresence);
                if (item)
                {
                    item->setValue(true);
                    Event e(RSensors, RStatePresence, sensor->id(), item);
                    enqueueEvent(e);
                    updateSensorEtag(sensor);
                    sensor->updateStateTimestamp();
                    sensor->setNeedSaveDatabase(true);
                    enqueueEvent(Event(RSensors, RStateLastUpdated, sensor->id()));

                    ResourceItem *item2 = sensor->item(RConfigDuration);
                    if (item2 && item2->toNumber() > 0)
                    {
                        sensor->durationDue = QDateTime::currentDateTime().addSecs(item2->toNumber());
                    }
                }
                break;
            }
        }
        buttonMap++;
    }

    if (checkReporting && sensor->node() &&
        sensor->lastAttributeReportBind() < (idleTotalCounter - BUTTON_ATTR_REPORT_BIND_LIMIT))
    {
        checkSensorBindingsForAttributeReporting(sensor);
        sensor->setLastAttributeReportBind(idleTotalCounter);
        if (sensor->mustRead(READ_BINDING_TABLE))
        {
            sensor->setNextReadTime(READ_BINDING_TABLE, queryTime);
            queryTime = queryTime.addSecs(1);
        }
        DBG_Printf(DBG_INFO_L2, "Force binding of attribute reporting for sensor %s\n", qPrintable(sensor->name()));
    }

    if (ok)
    {
        return;
    }

#if 0
    // check if hue dimmer switch is configured
    if (sensor->modelId().startsWith(QLatin1String("RWL02"))) // Hue dimmer switch
    {
        bool ok = true;
        // attribute reporting for power configuration cluster should fire every 5 minutes
        if (idleTotalCounter > (IDLE_READ_LIMIT + 600))
        {
            ResourceItem *item = sensor->item(RConfigBattery);
            if (item && item->lastSet().isValid() && item->toNumber() > 0) // seems to be ok
            {
                const NodeValue &val = sensor->getZclValue(POWER_CONFIGURATION_CLUSTER_ID, 0x0021);
                if (!val.timestampLastReport.isValid())
                {
                    ok = false; // not received battery report
                }
            }
            else
            {
                ok = false; // not received anything
            }
        }

        // is vendor specific cluster bound yet?
        ResourceItem *item = ok ? sensor->item(RStateButtonEvent) : 0;
        if (!item || !item->lastSet().isValid() || item->toNumber() < 1000)
        {
            ok = false;
        }

        if (!ok)
        {
            checkSensorBindingsForAttributeReporting(sensor);
        }
    }
#endif

    quint8 pl0 = zclFrame.payload().isEmpty() ? 0 : zclFrame.payload().at(0);
    DBG_Printf(DBG_INFO, "no button handler for: %s ep: 0x%02X cl: 0x%04X cmd: 0x%02X pl[0]: 0%02X\n",
                 qPrintable(sensor->modelId()), ind.srcEndpoint(), ind.clusterId(), zclFrame.commandId(), pl0);
}

/*! Adds a new sensor node to node cache.
    Only supported ZLL and HA nodes will be added.
    \param node - the base for the SensorNode
    \param event - the related NodeEvent (optional)
 */
void DeRestPluginPrivate::addSensorNode(const deCONZ::Node *node, const deCONZ::NodeEvent *event)
{
    DBG_Assert(node != 0);

    if (!node)
    {
        return;
    }

    { // check existing sensors
        std::vector<Sensor>::iterator i = sensors.begin();
        std::vector<Sensor>::iterator end = sensors.end();

        for (; i != end; ++i)
        {
            if (i->address().ext() == node->address().ext())
            {
                if (i->node() != node)
                {
                    i->setNode(const_cast<deCONZ::Node*>(node));
                    DBG_Printf(DBG_INFO, "SensorNode %s set node %s\n", qPrintable(i->id()), qPrintable(node->address().toStringExt()));

                    pushSensorInfoToCore(&*i);
                }

                checkSensorNodeReachable(&*i, event);
            }
        }
    }

    if (searchSensorsState != SearchSensorsActive)
    {
        return;
    }

    if (fastProbeAddr.hasExt() && fastProbeAddr.ext() != node->address().ext())
    {
        return;
    }

    // check for new sensors
    QString modelId;
    QString manufacturer;
    QList<deCONZ::SimpleDescriptor>::const_iterator i = node->simpleDescriptors().constBegin();
    QList<deCONZ::SimpleDescriptor>::const_iterator end = node->simpleDescriptors().constEnd();

    // Trust specific
    if (node->nodeDescriptor().manufacturerCode() == VENDOR_JENNIC && modelId.isEmpty())
    {
        // check Trust motion sensor ZPIR-8000
        if (node->simpleDescriptors().size() == 1 &&
                node->simpleDescriptors().first().endpoint() == 0x01 &&
                node->simpleDescriptors().first().profileId() == HA_PROFILE_ID &&
                node->simpleDescriptors().first().deviceId() == DEV_ID_IAS_ZONE &&
                node->simpleDescriptors().first().inClusters().size() == 5)
        {
            // server clusters: 0x0000, 0x0003, 0x0500, 0xffff, 0x0001
            modelId = QLatin1String("VMS_ADUROLIGHT"); // would be returned by reading the modelid
            manufacturer = QLatin1String("Trust");
        }
        // check Trust remote control ZYCT-202
        else if (node->simpleDescriptors().size() == 2 &&
                 node->simpleDescriptors()[0].endpoint() == 0x01 &&
                 node->simpleDescriptors()[0].profileId() == ZLL_PROFILE_ID &&
                 node->simpleDescriptors()[0].deviceId() == DEV_ID_ZLL_NON_COLOR_CONTROLLER &&
                 node->simpleDescriptors()[1].endpoint() == 0x02 &&
                 node->simpleDescriptors()[1].profileId() == ZLL_PROFILE_ID &&
                 node->simpleDescriptors()[1].deviceId() == 0x03f2)
        {
            // server clusters endpoint 0x01: 0x0000, 0x0004, 0x0003, 0x0006, 0x0008, 0x1000
            // client clusters endpoint 0x01: 0x0000, 0x0004, 0x0003, 0x0006, 0x0008, 0x1000
            // server clusters endpoint 0x02: 0x1000
            // client clusters endpoint 0x02: 0x1000
            modelId = QLatin1String("ZYCT-202"); //  the modelid returned by device is empty
            manufacturer = QLatin1String("Trust");
        }
    }

    for (;i != end; ++i)
    {
        SensorFingerprint fpAlarmSensor;
        SensorFingerprint fpCarbonMonoxideSensor;
        SensorFingerprint fpConsumptionSensor;
        SensorFingerprint fpFireSensor;
        SensorFingerprint fpHumiditySensor;
        SensorFingerprint fpLightSensor;
        SensorFingerprint fpOpenCloseSensor;
        SensorFingerprint fpPowerSensor;
        SensorFingerprint fpPresenceSensor;
        SensorFingerprint fpPressureSensor;
        SensorFingerprint fpSwitch;
        SensorFingerprint fpTemperatureSensor;
        SensorFingerprint fpVibrationSensor;
        SensorFingerprint fpWaterSensor;
        SensorFingerprint fpThermostatSensor;

        {   // scan server clusters of endpoint
            QList<deCONZ::ZclCluster>::const_iterator ci = i->inClusters().constBegin();
            QList<deCONZ::ZclCluster>::const_iterator cend = i->inClusters().constEnd();
            for (; ci != cend; ++ci)
            {
                switch (ci->id())
                {
                case BASIC_CLUSTER_ID:
                {
                    if (modelId.isEmpty() || manufacturer.isEmpty())
                    {
                        std::vector<deCONZ::ZclAttribute>::const_iterator j = ci->attributes().begin();
                        std::vector<deCONZ::ZclAttribute>::const_iterator jend = ci->attributes().end();

                        for (; j != jend; ++j)
                        {
                            if (manufacturer.isEmpty() && j->id() == 0x0004) // manufacturer id
                            {
                                manufacturer = j->toString().trimmed();
                            }
                            else if (modelId.isEmpty() && j->id() == 0x0005) // model id
                            {
                                modelId = j->toString().trimmed();
                            }
                        }
                    }

                    fpSwitch.inClusters.push_back(ci->id());
                    if (node->nodeDescriptor().manufacturerCode() == VENDOR_PHILIPS)
                    {
                        fpPresenceSensor.inClusters.push_back(ci->id());
                        fpLightSensor.inClusters.push_back(ci->id());
                        fpTemperatureSensor.inClusters.push_back(ci->id());
                    }
                    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_JENNIC &&
                             modelId.startsWith(QLatin1String("lumi.sensor_wleak")))
                    {
                        fpWaterSensor.inClusters.push_back(IAS_ZONE_CLUSTER_ID);
                    }
                    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_JENNIC &&
                             modelId.startsWith(QLatin1String("lumi.sensor_smoke")))
                    {
                        fpFireSensor.inClusters.push_back(IAS_ZONE_CLUSTER_ID);
                    }
                }
                    break;

                case POWER_CONFIGURATION_CLUSTER_ID:
                {
                    // @manup: is it safe to skip these tests?
                    // if (node->nodeDescriptor().manufacturerCode() == VENDOR_PHILIPS ||
                    //     node->nodeDescriptor().manufacturerCode() == VENDOR_NYCE ||
                    //     node->nodeDescriptor().manufacturerCode() == VENDOR_IKEA)
                    // {
                        fpAlarmSensor.inClusters.push_back(ci->id());
                        fpCarbonMonoxideSensor.inClusters.push_back(ci->id());
                        fpFireSensor.inClusters.push_back(ci->id());
                        fpHumiditySensor.inClusters.push_back(ci->id());
                        fpLightSensor.inClusters.push_back(ci->id());
                        fpOpenCloseSensor.inClusters.push_back(ci->id());
                        fpPresenceSensor.inClusters.push_back(ci->id());
                        fpPressureSensor.inClusters.push_back(ci->id());
                        fpSwitch.inClusters.push_back(ci->id());
                        fpTemperatureSensor.inClusters.push_back(ci->id());
                        fpVibrationSensor.inClusters.push_back(ci->id());
                        fpWaterSensor.inClusters.push_back(ci->id());
                        fpThermostatSensor.inClusters.push_back(ci->id());
                    // }
                }
                    break;

                case COMMISSIONING_CLUSTER_ID:
                {
                    if (modelId == QLatin1String("ZYCT-202") && i->endpoint() != 0x01)
                    {
                        // ignore second endpoint
                    }
                    else
                    {
                        fpSwitch.inClusters.push_back(ci->id());
                        fpPresenceSensor.inClusters.push_back(ci->id());
                    }
                }
                    break;

                case ONOFF_CLUSTER_ID:
                {
                    if (modelId.startsWith(QLatin1String("lumi.sensor_magnet")))
                    {
                        fpOpenCloseSensor.inClusters.push_back(ci->id());
                    }
                    else if (modelId.startsWith(QLatin1String("lumi.sensor_86sw")))
                    {
                        if (i->endpoint() == 0x01) // create sensor only for first endpoint
                        {
                            fpSwitch.inClusters.push_back(ci->id());
                        }
                    }
                    else if (modelId.startsWith(QLatin1String("lumi.ctrl_neutral")))
                    {
                        if (i->endpoint() == 0x04) // create sensor only for first switch endpoint
                        {
                            fpSwitch.inClusters.push_back(ci->id());
                        }
                    }
                    else if (modelId.startsWith(QLatin1String("lumi.sensor_switch")))
                    {
                        fpSwitch.inClusters.push_back(ci->id());
                    }
                    else if (modelId == QLatin1String("lumi.remote.b1acn01"))
                    {
                        fpSwitch.inClusters.push_back(ci->id());
                    }
                }
                    break;

                case ONOFF_SWITCH_CONFIGURATION_CLUSTER_ID:
                {
                    fpSwitch.inClusters.push_back(ci->id());
                }
                    break;

                case IAS_ZONE_CLUSTER_ID:
                {
                    if (modelId.startsWith(QLatin1String("CO_")))                     // Heiman CO sensor
                    {
                        fpCarbonMonoxideSensor.inClusters.push_back(ci->id());
                    }
                    else if (modelId.startsWith(QLatin1String("DOOR_")))              // Heiman door/window sensor
                    {
                        fpOpenCloseSensor.inClusters.push_back(ci->id());
                    }
                    else if (modelId.startsWith(QLatin1String("PIR_")))               // Heiman motion sensor
                    {
                        fpPresenceSensor.inClusters.push_back(ci->id());
                    }
                    else if (modelId.startsWith(QLatin1String("GAS_")) ||             // Heiman gas sensor
                             modelId.startsWith(QLatin1String("SMOK_")) ||            // Heiman fire sensor
                             modelId.startsWith(QLatin1String("lumi.sensor_smoke")))  // Xiaomi Mi smoke sensor
                    {
                        // Gas sensor detects combustable gas, so fire is more appropriate than CO.
                        fpFireSensor.inClusters.push_back(ci->id());
                    }
                    else if (modelId.startsWith(QLatin1String("WATER_")) ||           // Heiman water sensor
                             modelId.startsWith(QLatin1String("lumi.sensor_wleak")))  // Xiaomi Aqara flood sensor
                    {
                        fpWaterSensor.inClusters.push_back(ci->id());
                    }
                    else if (modelId == QLatin1String("WarningDevice"))               // Heiman siren
                    {
                        fpAlarmSensor.inClusters.push_back(ci->id());
                    }
                    else if (!modelId.isEmpty())
                    {
                        for (const deCONZ::ZclAttribute &attr : ci->attributes())
                        {
                            if (attr.id() == 0x0001) // IAS Zone type
                            {
                                // Might not work as intended, when IAS Zone Type hasn't been read.
                                switch (attr.numericValue().u16) {
                                    case IAS_ZONE_TYPE_MOTION_SENSOR:
                                        fpPresenceSensor.inClusters.push_back(ci->id());
                                        break;
                                    case IAS_ZONE_TYPE_CONTACT_SWITCH:
                                        fpOpenCloseSensor.inClusters.push_back(ci->id());
                                        break;
                                    case IAS_ZONE_TYPE_CARBON_MONOXIDE_SENSOR:
                                        fpCarbonMonoxideSensor.inClusters.push_back(ci->id());
                                        break;
                                    case IAS_ZONE_TYPE_FIRE_SENSOR:
                                        fpFireSensor.inClusters.push_back(ci->id());
                                        break;
                                    case IAS_ZONE_TYPE_VIBRATION_SENSOR:
                                        fpVibrationSensor.inClusters.push_back(ci->id());
                                        break;
                                    case IAS_ZONE_TYPE_WATER_SENSOR:
                                        fpWaterSensor.inClusters.push_back(ci->id());
                                        break;
                                    case IAS_ZONE_TYPE_WARNING_DEVICE:
                                    case IAS_ZONE_TYPE_STANDARD_CIE:
                                    default:
                                        if (manufacturer == QLatin1String("Trust"))
                                        {
                                            // ignore for ZHAAlarm
                                        }
                                        else
                                        {
                                            fpAlarmSensor.inClusters.push_back(ci->id());
                                        }
                                        break;
                                }
                            }
                        }
                    }
                }
                    break;

                case OCCUPANCY_SENSING_CLUSTER_ID:
                {
                    // @manup: Does this sensor indeed have an OCCUPANCY_SENSING_CLUSTER_ID custer?
                    if (node->nodeDescriptor().manufacturerCode() == VENDOR_CENTRALITE &&
                        i->endpoint() == 0x02 && modelId == QLatin1String("Motion Sensor-A"))
                    {
                        // only use endpoint 0x01 of this sensor
                    }
                    else
                    {
                        fpPresenceSensor.inClusters.push_back(ci->id());
                    }
                }
                    break;

                case ILLUMINANCE_MEASUREMENT_CLUSTER_ID:
                case ILLUMINANCE_LEVEL_SENSING_CLUSTER_ID:
                {
                    fpLightSensor.inClusters.push_back(ci->id());
                }
                    break;

                case TEMPERATURE_MEASUREMENT_CLUSTER_ID:
                {
                    fpTemperatureSensor.inClusters.push_back(ci->id());
                }
                    break;

                case RELATIVE_HUMIDITY_CLUSTER_ID:
                {
                    fpHumiditySensor.inClusters.push_back(ci->id());
                }
                    break;

                case PRESSURE_MEASUREMENT_CLUSTER_ID:
                {
                    fpPressureSensor.inClusters.push_back(ci->id());
                }
                    break;

                case ANALOG_INPUT_CLUSTER_ID:
                {
                    if (modelId.startsWith(QLatin1String("lumi.sensor_cube")))
                    {
                        fpSwitch.inClusters.push_back(ci->id());
                    }
                    else if (modelId == QLatin1String("lumi.plug"))
                    {
                        if (i->endpoint() == 0x02)
                        {
                            fpPowerSensor.inClusters.push_back(ci->id());
                        }
                        else if (i->endpoint() == 0x03)
                        {
                            fpConsumptionSensor.inClusters.push_back(ci->id());
                        }
                    }
                    else if (modelId.startsWith(QLatin1String("lumi.ctrl_ln")))
                    {
                        if (i->endpoint() == 0x03)
                        {
                            fpPowerSensor.inClusters.push_back(ci->id());
                        }
                        else if (i->endpoint() == 0x04)
                        {
                            fpConsumptionSensor.inClusters.push_back(ci->id());
                        }
                    }
                }
                    break;

                case MULTISTATE_INPUT_CLUSTER_ID:
                {
                    if (modelId.startsWith(QLatin1String("lumi.sensor_cube")) && i->endpoint() == 0x02)
                    {
                        fpSwitch.inClusters.push_back(ci->id());
                    }
                    else if (modelId.startsWith(QLatin1String("lumi.ctrl_ln")) && i->endpoint() == 0x05)
                    {
                        fpSwitch.inClusters.push_back(ci->id());
                    }
                    else if (modelId == QLatin1String("lumi.sensor_switch.aq3"))
                    {
                        fpSwitch.inClusters.push_back(ci->id());
                    }
                    else if (modelId == QLatin1String("lumi.remote.b1acn01"))
                    {
                        fpSwitch.inClusters.push_back(ci->id());
                    }
                    else if (modelId == QLatin1String("lumi.remote.b186acn01") && i->endpoint() == 0x01)
                    {
                        fpSwitch.inClusters.push_back(ci->id());
                    }
                    else if (modelId == QLatin1String("lumi.remote.b286acn01") && i->endpoint() == 0x01)
                    {
                        fpSwitch.inClusters.push_back(ci->id());
                    }
                }
                    break;

                case BINARY_INPUT_CLUSTER_ID:
                {
                    if (modelId.startsWith(QLatin1String("tagv4"))) // SmartThings Arrival sensor
                    {
                        fpPresenceSensor.inClusters.push_back(ci->id());
                    }
                }
                    break;

                case DOOR_LOCK_CLUSTER_ID:
                {
                    if (modelId.startsWith(QLatin1String("lumi.vibration"))) // lumi.vibration
                    {
                        fpSwitch.inClusters.push_back(DOOR_LOCK_CLUSTER_ID);
                    }
                }
                    break;

                case METERING_CLUSTER_ID:
                {
                    fpConsumptionSensor.inClusters.push_back(ci->id());
                }
                    break;

                case ELECTRICAL_MEASUREMENT_CLUSTER_ID:
                {
                    fpPowerSensor.inClusters.push_back(ci->id());
                }
                    break;

                case THERMOSTAT_CLUSTER_ID:
                {
                    fpThermostatSensor.inClusters.push_back(ci->id());
                }
                    break;

                default:
                    break;
                }
            }
        }

        {   // scan client clusters of endpoint
            QList<deCONZ::ZclCluster>::const_iterator ci = i->outClusters().constBegin();
            QList<deCONZ::ZclCluster>::const_iterator cend = i->outClusters().constEnd();
            for (; ci != cend; ++ci)
            {
                switch (ci->id())
                {
                case ONOFF_CLUSTER_ID:
                case LEVEL_CLUSTER_ID:
                case SCENE_CLUSTER_ID:
                case WINDOW_COVERING_CLUSTER_ID:
                {
                    if (modelId == QLatin1String("ZYCT-202"))
                    {
                        fpSwitch.outClusters.push_back(ci->id());
                    }
                    // else if (modelId.startsWith(QLatin1String("RC 110")))
                    // {
                    //     if (i->endpoint() == 0x01) // create sensor only for first endpoint
                    //     {
                    //         fpSwitch.outClusters.push_back(ci->id());
                    //     }
                    // }
                    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_JENNIC)
                    {
                        // prevent creation of ZHASwitch, till supported
                    }
                    else if (i->deviceId() == DEV_ID_ZLL_ONOFF_SENSOR &&
                        node->nodeDescriptor().manufacturerCode() == VENDOR_IKEA)
                    {
                        fpPresenceSensor.outClusters.push_back(ci->id());
                    }
                    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_UBISYS)
                    {
                        if ((modelId.startsWith(QLatin1String("D1")) && i->endpoint() == 0x02) ||
                            (modelId.startsWith(QLatin1String("J1")) && i->endpoint() == 0x02) ||
                            (modelId.startsWith(QLatin1String("C4")) && i->endpoint() == 0x01) ||
                            (modelId.startsWith(QLatin1String("S2")) && i->endpoint() == 0x03))
                        {
                            // Combine multiple switch endpoints into a single ZHASwitch resource
                            fpSwitch.outClusters.push_back(ci->id());
                        }
                    }
                    else if (!node->nodeDescriptor().isNull())
                    {
                        fpSwitch.outClusters.push_back(ci->id());
                    }
                }
                    break;

                default:
                    break;
                }
            }
        }

        if (modelId.isEmpty())
        {
            Sensor *sensor = getSensorNodeForAddress(node->address()); // extract from other sensors if possible
            if (sensor && sensor->deletedState() == Sensor::StateNormal && !sensor->modelId().isEmpty())
            {
                modelId = sensor->modelId();
            }
            else
            { // extract from light if possible
                LightNode *lightNode = getLightNodeForAddress(node->address());
                if (lightNode && !lightNode->modelId().isEmpty())
                {
                    modelId = lightNode->modelId();
                }
            }
        }

        if (!isDeviceSupported(node, modelId))
        {
            continue;
        }

        Sensor *sensor = nullptr;

        // ZHASwitch
        if (fpSwitch.hasInCluster(ONOFF_SWITCH_CONFIGURATION_CLUSTER_ID) ||
            fpSwitch.hasInCluster(ONOFF_CLUSTER_ID) ||
            fpSwitch.hasInCluster(ANALOG_INPUT_CLUSTER_ID) ||
            fpSwitch.hasInCluster(MULTISTATE_INPUT_CLUSTER_ID) ||
            fpSwitch.hasInCluster(DOOR_LOCK_CLUSTER_ID) ||
            !fpSwitch.outClusters.empty())
        {
            fpSwitch.endpoint = i->endpoint();
            fpSwitch.deviceId = i->deviceId();
            fpSwitch.profileId = i->profileId();

            if (modelId.startsWith(QLatin1String("RWL02")))
            {
                sensor = getSensorNodeForAddress(node->address().ext()); // former created with with endpoint 1
                if (sensor && sensor->deletedState() != Sensor::StateNormal)
                {
                    sensor = nullptr;
                }
                fpSwitch.endpoint = 2;
            }

            if (!sensor)
            {
                sensor = getSensorNodeForFingerPrint(node->address().ext(), fpSwitch, "ZHASwitch");
            }

            if (modelId == QLatin1String("OJB-IR715-Z"))
            {
                // don't create ZHASwitch, IAS Presence only
            }
            else if (!sensor || sensor->deletedState() != Sensor::StateNormal)
            {
                addSensorNode(node, fpSwitch, "ZHASwitch", modelId, manufacturer);
            }
            else
            {
                checkSensorNodeReachable(sensor);
            }
        }

        // ZHAPresence
        if (fpPresenceSensor.hasInCluster(OCCUPANCY_SENSING_CLUSTER_ID) ||
            fpPresenceSensor.hasInCluster(IAS_ZONE_CLUSTER_ID) ||
            fpPresenceSensor.hasInCluster(BINARY_INPUT_CLUSTER_ID) ||
            fpPresenceSensor.hasOutCluster(ONOFF_CLUSTER_ID))
        {
            fpPresenceSensor.endpoint = i->endpoint();
            fpPresenceSensor.deviceId = i->deviceId();
            fpPresenceSensor.profileId = i->profileId();

            sensor = getSensorNodeForFingerPrint(node->address().ext(), fpPresenceSensor, "ZHAPresence");
            if (!sensor || sensor->deletedState() != Sensor::StateNormal)
            {
                addSensorNode(node, fpPresenceSensor, "ZHAPresence", modelId, manufacturer);
            }
            else
            {
                checkSensorNodeReachable(sensor);
            }
        }

        // ZHAOpenClose
        if (fpOpenCloseSensor.hasInCluster(IAS_ZONE_CLUSTER_ID) ||
            fpOpenCloseSensor.hasInCluster(ONOFF_CLUSTER_ID))
        {
            fpOpenCloseSensor.endpoint = i->endpoint();
            fpOpenCloseSensor.deviceId = i->deviceId();
            fpOpenCloseSensor.profileId = i->profileId();

            sensor = getSensorNodeForFingerPrint(node->address().ext(), fpOpenCloseSensor, "ZHAOpenClose");
            if (!sensor || sensor->deletedState() != Sensor::StateNormal)
            {
                addSensorNode(node, fpOpenCloseSensor, "ZHAOpenClose", modelId, manufacturer);
            }
            else
            {
                checkSensorNodeReachable(sensor);
            }
        }

        // ZHATemperature
        if (fpTemperatureSensor.hasInCluster(TEMPERATURE_MEASUREMENT_CLUSTER_ID))
        {
            fpTemperatureSensor.endpoint = i->endpoint();
            fpTemperatureSensor.deviceId = i->deviceId();
            fpTemperatureSensor.profileId = i->profileId();

            sensor = getSensorNodeForFingerPrint(node->address().ext(), fpTemperatureSensor, "ZHATemperature");
            if (!sensor || sensor->deletedState() != Sensor::StateNormal)
            {
                addSensorNode(node, fpTemperatureSensor, "ZHATemperature", modelId, manufacturer);
            }
            else
            {
                checkSensorNodeReachable(sensor);
            }
        }

        // ZHALightLevel
        if (fpLightSensor.hasInCluster(ILLUMINANCE_MEASUREMENT_CLUSTER_ID))
        {
            fpLightSensor.endpoint = i->endpoint();
            fpLightSensor.deviceId = i->deviceId();
            fpLightSensor.profileId = i->profileId();

            sensor = getSensorNodeForFingerPrint(node->address().ext(), fpLightSensor, "ZHALightLevel");
            if (!sensor || sensor->deletedState() != Sensor::StateNormal)
            {
                addSensorNode(node, fpLightSensor, "ZHALightLevel", modelId, manufacturer);
            }
            else
            {
                checkSensorNodeReachable(sensor);
            }
        }

        // ZHAHumidity
        if (fpHumiditySensor.hasInCluster(RELATIVE_HUMIDITY_CLUSTER_ID))
        {
            fpHumiditySensor.endpoint = i->endpoint();
            fpHumiditySensor.deviceId = i->deviceId();
            fpHumiditySensor.profileId = i->profileId();

            sensor = getSensorNodeForFingerPrint(node->address().ext(), fpHumiditySensor, "ZHAHumidity");
            if (!sensor || sensor->deletedState() != Sensor::StateNormal)
            {
                addSensorNode(node, fpHumiditySensor, "ZHAHumidity", modelId, manufacturer);
            }
            else
            {
                checkSensorNodeReachable(sensor);
            }
        }

        // ZHAPressure
        if (fpPressureSensor.hasInCluster(PRESSURE_MEASUREMENT_CLUSTER_ID))
        {
            fpPressureSensor.endpoint = i->endpoint();
            fpPressureSensor.deviceId = i->deviceId();
            fpPressureSensor.profileId = i->profileId();

            sensor = getSensorNodeForFingerPrint(node->address().ext(), fpPressureSensor, "ZHAPressure");
            if (!sensor || sensor->deletedState() != Sensor::StateNormal)
            {
                addSensorNode(node, fpPressureSensor, "ZHAPressure", modelId, manufacturer);
            }
            else
            {
                checkSensorNodeReachable(sensor);
            }
        }

        // ZHAAlarm
        if (fpAlarmSensor.hasInCluster(IAS_ZONE_CLUSTER_ID))
        {
            fpAlarmSensor.endpoint = i->endpoint();
            fpAlarmSensor.deviceId = i->deviceId();
            fpAlarmSensor.profileId = i->profileId();

            sensor = getSensorNodeForFingerPrint(node->address().ext(), fpAlarmSensor, "ZHAAlarm");
            if (!sensor || sensor->deletedState() != Sensor::StateNormal)
            {
                addSensorNode(node, fpAlarmSensor, "ZHAAlarm", modelId, manufacturer);
            }
            else
            {
                checkSensorNodeReachable(sensor);
            }
        }

        // ZHACarbonMonoxide
        if (fpCarbonMonoxideSensor.hasInCluster(IAS_ZONE_CLUSTER_ID))
        {
            fpCarbonMonoxideSensor.endpoint = i->endpoint();
            fpCarbonMonoxideSensor.deviceId = i->deviceId();
            fpCarbonMonoxideSensor.profileId = i->profileId();

            sensor = getSensorNodeForFingerPrint(node->address().ext(), fpCarbonMonoxideSensor, "ZHACarbonMonoxide");
            if (!sensor || sensor->deletedState() != Sensor::StateNormal)
            {
                addSensorNode(node, fpCarbonMonoxideSensor, "ZHACarbonMonoxide", modelId, manufacturer);
            }
            else
            {
                checkSensorNodeReachable(sensor);
            }
        }

        // ZHAFire
        if (fpFireSensor.hasInCluster(IAS_ZONE_CLUSTER_ID))
        {
            fpFireSensor.endpoint = i->endpoint();
            fpFireSensor.deviceId = i->deviceId();
            fpFireSensor.profileId = i->profileId();

            sensor = getSensorNodeForFingerPrint(node->address().ext(), fpFireSensor, "ZHAFire");
            if (!sensor || sensor->deletedState() != Sensor::StateNormal)
            {
                addSensorNode(node, fpFireSensor, "ZHAFire", modelId, manufacturer);
            }
            else
            {
                checkSensorNodeReachable(sensor);
            }
        }

        // ZHAVibration
        if (fpVibrationSensor.hasInCluster(IAS_ZONE_CLUSTER_ID))
        {
            fpVibrationSensor.endpoint = i->endpoint();
            fpVibrationSensor.deviceId = i->deviceId();
            fpVibrationSensor.profileId = i->profileId();

            sensor = getSensorNodeForFingerPrint(node->address().ext(), fpVibrationSensor, "ZHAVibration");
            if (!sensor || sensor->deletedState() != Sensor::StateNormal)
            {
                addSensorNode(node, fpVibrationSensor, "ZHAVibration", modelId, manufacturer);
            }
            else
            {
                checkSensorNodeReachable(sensor);
            }
        }

        // ZHAWater
        if (fpWaterSensor.hasInCluster(IAS_ZONE_CLUSTER_ID))
        {
            fpWaterSensor.endpoint = i->endpoint();
            fpWaterSensor.deviceId = i->deviceId();
            fpWaterSensor.profileId = i->profileId();

            sensor = getSensorNodeForFingerPrint(node->address().ext(), fpWaterSensor, "ZHAWater");
            if (!sensor || sensor->deletedState() != Sensor::StateNormal)
            {
                addSensorNode(node, fpWaterSensor, "ZHAWater", modelId, manufacturer);
            }
            else
            {
                checkSensorNodeReachable(sensor);
            }
        }

        // ZHAConsumption
        if (fpConsumptionSensor.hasInCluster(METERING_CLUSTER_ID) ||
            fpConsumptionSensor.hasInCluster(ANALOG_INPUT_CLUSTER_ID))
        {
            fpConsumptionSensor.endpoint = i->endpoint();
            fpConsumptionSensor.deviceId = i->deviceId();
            fpConsumptionSensor.profileId = i->profileId();

            sensor = getSensorNodeForFingerPrint(node->address().ext(), fpConsumptionSensor, "ZHAConsumption");
            if (!sensor || sensor->deletedState() != Sensor::StateNormal)
            {
                addSensorNode(node, fpConsumptionSensor, "ZHAConsumption", modelId, manufacturer);
            }
            else
            {
                checkSensorNodeReachable(sensor);
            }
        }

        // ZHAPower
        if (fpPowerSensor.hasInCluster(ELECTRICAL_MEASUREMENT_CLUSTER_ID) ||
            fpPowerSensor.hasInCluster(ANALOG_INPUT_CLUSTER_ID))
        {
            fpPowerSensor.endpoint = i->endpoint();
            fpPowerSensor.deviceId = i->deviceId();
            fpPowerSensor.profileId = i->profileId();

            sensor = getSensorNodeForFingerPrint(node->address().ext(), fpPowerSensor, "ZHAPower");
            if (!sensor || sensor->deletedState() != Sensor::StateNormal)
            {
                addSensorNode(node, fpPowerSensor, "ZHAPower", modelId, manufacturer);
            }
            else
            {
                checkSensorNodeReachable(sensor);
            }
        }

        // ZHAThermostat
        if (fpThermostatSensor.hasInCluster(THERMOSTAT_CLUSTER_ID))
        {
            fpThermostatSensor.endpoint = i->endpoint();
            fpThermostatSensor.deviceId = i->deviceId();
            fpThermostatSensor.profileId = i->profileId();

            sensor = getSensorNodeForFingerPrint(node->address().ext(), fpThermostatSensor, "ZHAThermostat");
            if (!sensor || sensor->deletedState() != Sensor::StateNormal)
            {
                addSensorNode(node, fpThermostatSensor, "ZHAThermostat", modelId, manufacturer);
            }
            else
            {
                checkSensorNodeReachable(sensor);
            }
        }

    }
}

void DeRestPluginPrivate::addSensorNode(const deCONZ::Node *node, const SensorFingerprint &fingerPrint, const QString &type, const QString &modelId, const QString &manufacturer)
{
    DBG_Assert(node != 0);
    if (!node)
    {
        return;
    }

    Sensor sensorNode;
    sensorNode.setMode(Sensor::ModeScenes);
    sensorNode.setNode(const_cast<deCONZ::Node*>(node));
    sensorNode.address() = node->address();
    sensorNode.setType(type);
    sensorNode.fingerPrint() = fingerPrint;
    sensorNode.setModelId(modelId);
    quint16 clusterId = 0;

    // simple check if existing device needs to be updated
    Sensor *sensor2 = 0;
    if (node->endpoints().size() == 1)
    {
        quint8 ep = node->endpoints()[0];
        std::vector<Sensor>::iterator i = sensors.begin();
        std::vector<Sensor>::iterator end = sensors.end();

        for (; i != end; ++i)
        {
            if (i->address().ext() == node->address().ext() &&
                ep == i->fingerPrint().endpoint &&
                i->deletedState() != Sensor::StateDeleted &&
                i->type() == type)
            {
                sensor2 = &*i;
                break;
            }
        }

        if (sensor2)
        {
            sensorNode.setId(sensor2->id()); // preserve
        }
    }

    ResourceItem *item;
    item = sensorNode.item(RConfigOn);
    item->setValue(true);

    item = sensorNode.item(RConfigReachable);
    item->setValue(true);

    if (sensorNode.fingerPrint().hasInCluster(POWER_CONFIGURATION_CLUSTER_ID))
    {
        if (manufacturer.startsWith(QLatin1String("Climax")))
        {
            // climax non IAS reports state/lowbattery via battery alarm mask attribute
            sensorNode.addItem(DataTypeBool, RStateLowBattery);
            // don't set value -> null until reported
        }
        else
        {
            sensorNode.addItem(DataTypeUInt8, RConfigBattery);
        }
    }

    if (sensorNode.type().endsWith(QLatin1String("Switch")))
    {
        if (sensorNode.fingerPrint().hasInCluster(COMMISSIONING_CLUSTER_ID))
        {
            clusterId = COMMISSIONING_CLUSTER_ID;
        }
        else if (sensorNode.fingerPrint().hasInCluster(ONOFF_CLUSTER_ID) ||
                 sensorNode.fingerPrint().hasOutCluster(ONOFF_CLUSTER_ID))
        {
            clusterId = ONOFF_CLUSTER_ID;
        }
        else if (sensorNode.fingerPrint().hasInCluster(ANALOG_INPUT_CLUSTER_ID))
        {
            clusterId = ANALOG_INPUT_CLUSTER_ID;
        }
        else if (sensorNode.fingerPrint().hasInCluster(DOOR_LOCK_CLUSTER_ID))
        {
            clusterId = DOOR_LOCK_CLUSTER_ID;
        }
        else if (sensorNode.fingerPrint().hasInCluster(MULTISTATE_INPUT_CLUSTER_ID))
        {
            clusterId = MULTISTATE_INPUT_CLUSTER_ID;
        }
        sensorNode.addItem(DataTypeInt32, RStateButtonEvent);
    }
    else if (sensorNode.type().endsWith(QLatin1String("LightLevel")))
    {
        if (sensorNode.fingerPrint().hasInCluster(ILLUMINANCE_MEASUREMENT_CLUSTER_ID))
        {
            clusterId = ILLUMINANCE_MEASUREMENT_CLUSTER_ID;
        }
        sensorNode.addItem(DataTypeUInt16, RStateLightLevel);
        sensorNode.addItem(DataTypeUInt32, RStateLux);
        sensorNode.addItem(DataTypeBool, RStateDark);
        sensorNode.addItem(DataTypeBool, RStateDaylight);
        item = sensorNode.addItem(DataTypeUInt16, RConfigTholdDark);
        item->setValue(R_THOLDDARK_DEFAULT);
        item = sensorNode.addItem(DataTypeUInt16, RConfigTholdOffset);
        item->setValue(R_THOLDOFFSET_DEFAULT);
    }
    else if (sensorNode.type().endsWith(QLatin1String("Temperature")))
    {
        if (sensorNode.fingerPrint().hasInCluster(TEMPERATURE_MEASUREMENT_CLUSTER_ID))
        {
            clusterId = TEMPERATURE_MEASUREMENT_CLUSTER_ID;
        }
        sensorNode.addItem(DataTypeInt16, RStateTemperature);
        item = sensorNode.addItem(DataTypeInt16, RConfigOffset);
        item->setValue(0);
    }
    else if (sensorNode.type().endsWith(QLatin1String("Humidity")))
    {
        if (sensorNode.fingerPrint().hasInCluster(RELATIVE_HUMIDITY_CLUSTER_ID))
        {
            clusterId = RELATIVE_HUMIDITY_CLUSTER_ID;
        }
        sensorNode.addItem(DataTypeUInt16, RStateHumidity);
        item = sensorNode.addItem(DataTypeInt16, RConfigOffset);
        item->setValue(0);
    }
    else if (sensorNode.type().endsWith(QLatin1String("Pressure")))
    {
        if (sensorNode.fingerPrint().hasInCluster(PRESSURE_MEASUREMENT_CLUSTER_ID))
        {
            clusterId = PRESSURE_MEASUREMENT_CLUSTER_ID;
        }
        sensorNode.addItem(DataTypeInt16, RStatePressure);
    }
    else if (sensorNode.type().endsWith(QLatin1String("Presence")))
    {
        if (sensorNode.fingerPrint().hasInCluster(OCCUPANCY_SENSING_CLUSTER_ID))
        {
            clusterId = OCCUPANCY_SENSING_CLUSTER_ID;
        }
        else if (sensorNode.fingerPrint().hasInCluster(IAS_ZONE_CLUSTER_ID))
        {
            clusterId = IAS_ZONE_CLUSTER_ID;
        }
        else if (sensorNode.fingerPrint().hasInCluster(BINARY_INPUT_CLUSTER_ID))
        {
            clusterId = BINARY_INPUT_CLUSTER_ID;
        }
        else if (sensorNode.fingerPrint().hasOutCluster(ONOFF_CLUSTER_ID))
        {
            clusterId = ONOFF_CLUSTER_ID;
        }
        item = sensorNode.addItem(DataTypeBool, RStatePresence);
        item->setValue(false);
        item = sensorNode.addItem(DataTypeUInt16, RConfigDuration);
        if (modelId.startsWith(QLatin1String("tagv4"))) // SmartThings Arrival sensor
        {
            item->setValue(310); // Sensor will be configured to report every 5 minutes
        }
        else
        {
            item->setValue(60); // default 60 seconds
        }
    }
    else if (sensorNode.type().endsWith(QLatin1String("OpenClose")))
    {
        if (sensorNode.fingerPrint().hasInCluster(IAS_ZONE_CLUSTER_ID))
        {
            clusterId = IAS_ZONE_CLUSTER_ID;
        }
        else if (sensorNode.fingerPrint().hasInCluster(ONOFF_CLUSTER_ID))
        {
            clusterId = ONOFF_CLUSTER_ID;
        }
        item = sensorNode.addItem(DataTypeBool, RStateOpen);
        item->setValue(false);
    }
    else if (sensorNode.type().endsWith(QLatin1String("Alarm")))
    {
        if (sensorNode.fingerPrint().hasInCluster(IAS_ZONE_CLUSTER_ID))
        {
            clusterId = IAS_ZONE_CLUSTER_ID;
        }
        item = sensorNode.addItem(DataTypeBool, RStateAlarm);
        item->setValue(false);
    }
    else if (sensorNode.type().endsWith(QLatin1String("CarbonMonoxide")))
    {
        if (sensorNode.fingerPrint().hasInCluster(IAS_ZONE_CLUSTER_ID))
        {
            clusterId = IAS_ZONE_CLUSTER_ID;
        }
        item = sensorNode.addItem(DataTypeBool, RStateCarbonMonoxide);
        item->setValue(false);
    }
    else if (sensorNode.type().endsWith(QLatin1String("Fire")))
    {
        if (sensorNode.fingerPrint().hasInCluster(IAS_ZONE_CLUSTER_ID))
        {
            clusterId = IAS_ZONE_CLUSTER_ID;
        }
        item = sensorNode.addItem(DataTypeBool, RStateFire);
        item->setValue(false);
    }
    else if (sensorNode.type().endsWith(QLatin1String("Vibration")))
    {
        if (sensorNode.fingerPrint().hasInCluster(IAS_ZONE_CLUSTER_ID))
        {
            clusterId = IAS_ZONE_CLUSTER_ID;
        }
        item = sensorNode.addItem(DataTypeBool, RStateVibration);
        item->setValue(false);
    }
    else if (sensorNode.type().endsWith(QLatin1String("Water")))
    {
        if (sensorNode.fingerPrint().hasInCluster(IAS_ZONE_CLUSTER_ID))
        {
            clusterId = IAS_ZONE_CLUSTER_ID;
        }
        item = sensorNode.addItem(DataTypeBool, RStateWater);
        item->setValue(false);
    }
    else if (sensorNode.type().endsWith(QLatin1String("Consumption")))
    {
        if (sensorNode.fingerPrint().hasInCluster(METERING_CLUSTER_ID))
        {
            clusterId = METERING_CLUSTER_ID;
            item = sensorNode.addItem(DataTypeUInt64, RStateConsumption);
            if (modelId != QLatin1String("SP 120"))
            {
                item = sensorNode.addItem(DataTypeInt16, RStatePower);
            }
        }
        else if (sensorNode.fingerPrint().hasInCluster(ANALOG_INPUT_CLUSTER_ID))
        {
            clusterId = ANALOG_INPUT_CLUSTER_ID;
            item = sensorNode.addItem(DataTypeUInt64, RStateConsumption);
        }
    }
    else if (sensorNode.type().endsWith(QLatin1String("Power")))
    {
        if (sensorNode.fingerPrint().hasInCluster(ELECTRICAL_MEASUREMENT_CLUSTER_ID))
        {
            clusterId = ELECTRICAL_MEASUREMENT_CLUSTER_ID;
            item = sensorNode.addItem(DataTypeInt16, RStatePower);
            if (!modelId.startsWith(QLatin1String("Plug"))) // OSRAM
            {
                item = sensorNode.addItem(DataTypeUInt16, RStateVoltage);
                item = sensorNode.addItem(DataTypeUInt16, RStateCurrent);
            }
        }
        else if (sensorNode.fingerPrint().hasInCluster(ANALOG_INPUT_CLUSTER_ID))
        {
            clusterId = ANALOG_INPUT_CLUSTER_ID;
            item = sensorNode.addItem(DataTypeInt16, RStatePower);
        }
    }
    else if (sensorNode.type().endsWith(QLatin1String("Thermostat")))
    {
        if (sensorNode.fingerPrint().hasInCluster(THERMOSTAT_CLUSTER_ID))
        {
            clusterId = THERMOSTAT_CLUSTER_ID;
        }
        sensorNode.addItem(DataTypeInt16, RStateTemperature);
        item = sensorNode.addItem(DataTypeInt16, RConfigOffset);
        item->setValue(0);
        sensorNode.addItem(DataTypeInt16, RConfigHeating);    // Heating set point
        sensorNode.addItem(DataTypeBool, RConfigSchedulerOn); // Scheduler state on/off
        sensorNode.addItem(DataTypeBool, RStateOn);           // Heating on/off
        sensorNode.addItem(DataTypeString, RConfigScheduler); // Scheduler setting
    }

    if (node->nodeDescriptor().manufacturerCode() == VENDOR_DDEL)
    {
        sensorNode.setManufacturer("dresden elektronik");

        if (modelId == QLatin1String("Lighting Switch"))
        {
            sensorNode.setMode(Sensor::ModeTwoGroups); // inital
        }
        else if (modelId.startsWith(QLatin1String("FLS-NB")))
        {
            sensorNode.setManufacturer("nimbus group");;
        }
    }
    else if ((node->nodeDescriptor().manufacturerCode() == VENDOR_OSRAM_STACK) || (node->nodeDescriptor().manufacturerCode() == VENDOR_OSRAM))
    {
        if (modelId.startsWith(QLatin1String("CO_")) ||   // Heiman CO sensor
            modelId.startsWith(QLatin1String("DOOR_")) || // Heiman door/window sensor
            modelId.startsWith(QLatin1String("PIR_")) ||  // Heiman motion sensor
            modelId.startsWith(QLatin1String("GAS_")) ||  // Heiman conbustable gas sensor
            modelId.startsWith(QLatin1String("TH-H_")) || // Heiman temperature/humidity sensor
            modelId.startsWith(QLatin1String("TH-T_")) || // Heiman temperature/humidity sensor
            modelId.startsWith(QLatin1String("SMOK_")) || // Heiman fire sensor
            modelId.startsWith(QLatin1String("WATER_")))  // Heiman water sensor
        {
            sensorNode.setManufacturer("Heiman");
        }
        else
        {
            sensorNode.setManufacturer("OSRAM");
        }
    }
    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_UBISYS)
    {
        sensorNode.setManufacturer("ubisys");

        if (type == QLatin1String("ZHASwitch"))
        {
            sensorNode.addItem(DataTypeString, RConfigGroup);
            item = sensorNode.addItem(DataTypeString, RConfigMode);
            item->setValue(QString("momentary"));

            if (sensorNode.modelId().startsWith(QLatin1String("J1")))
            {
            	item = sensorNode.addItem(DataTypeUInt8, RConfigWindowCoveringType);
            	item->setValue(0);
            }
        }
    }
    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_BUSCH_JAEGER)
    {
        sensorNode.setManufacturer("Busch-Jaeger");

        if (node->endpoints().size() >= 4)
        {
            sensorNode.setMode(Sensor::ModeScenes);
        }
        else if (node->endpoints().size() >= 2)
        {
            deCONZ::SimpleDescriptor sd;

            // has light endpoint?
            if (node->copySimpleDescriptor(0x12, &sd) == 0)
            {
                sensorNode.setMode(Sensor::ModeDimmer);
            }
            else
            {
                sensorNode.setMode(Sensor::ModeScenes);
            }
        }
        else
        {
            sensorNode.setMode(Sensor::ModeDimmer);
        }
    }
    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_PHILIPS)
    {
        sensorNode.setManufacturer(QLatin1String("Philips"));

        if (modelId.startsWith(QLatin1String("RWL02"))) // Hue dimmer switch
        {
            sensorNode.fingerPrint().endpoint = 2;
            clusterId = VENDOR_CLUSTER_ID;

            if (!sensorNode.fingerPrint().hasInCluster(POWER_CONFIGURATION_CLUSTER_ID))
            {   // this cluster is on endpoint 2 and hence not detected
                sensorNode.fingerPrint().inClusters.push_back(POWER_CONFIGURATION_CLUSTER_ID);
            }

            if (!sensorNode.fingerPrint().hasInCluster(VENDOR_CLUSTER_ID)) // for realtime button feedback
            {   // this cluster is on endpoint 2 and hence not detected
                sensorNode.fingerPrint().inClusters.push_back(VENDOR_CLUSTER_ID);
            }
        }
        else if (modelId == QLatin1String("SML001")) // Hue motion sensor
        {
            if (type == QLatin1String("ZHASwitch"))
            {
                // not supported yet
                return;
            }
            else if (type == QLatin1String("ZHAPresence"))
            {
                item = sensorNode.addItem(DataTypeUInt8, RConfigSensitivity);
                item->setValue(0);
                item = sensorNode.addItem(DataTypeUInt8, RConfigSensitivityMax);
                item->setValue(R_SENSITIVITY_MAX_DEFAULT);
                sensorNode.removeItem(RConfigDuration);
                item = sensorNode.addItem(DataTypeUInt16, RConfigDelay);
                item->setValue(0);
            }
            item = sensorNode.addItem(DataTypeString, RConfigAlert);
            item->setValue(R_ALERT_DEFAULT);
            item = sensorNode.addItem(DataTypeBool, RConfigLedIndication);
            item->setValue(false);
            item = sensorNode.addItem(DataTypeUInt8, RConfigPending);
            item->setValue(0);
            item = sensorNode.addItem(DataTypeBool, RConfigUsertest);
            item->setValue(false);
        }
    }
    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_BEGA)
    {
        sensorNode.setManufacturer("BEGA Gantenbrink-Leuchten KG");
    }
    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_BOSCH)
    {
        sensorNode.setManufacturer("BOSCH");
    }
    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_IKEA)
    {
        sensorNode.setManufacturer("IKEA of Sweden");

        if (modelId == QLatin1String("TRADFRI wireless dimmer"))
        {
            sensorNode.setMode(Sensor::ModeDimmer);
        }
        else
        {
            item = sensorNode.addItem(DataTypeString, RConfigAlert);
            item->setValue(R_ALERT_DEFAULT);
        }

        sensorNode.setName(QString("%1 %2").arg(modelId).arg(sensorNode.id()));
    }
    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_INSTA)
    {
        sensorNode.setManufacturer("Insta");
        checkInstaModelId(&sensorNode);
    }
    else if (modelId.startsWith(QLatin1String("lumi")))
    {
        sensorNode.setManufacturer("LUMI");
        if (!sensorNode.modelId().startsWith(QLatin1String("lumi.ctrl_")) &&
            sensorNode.modelId() != QLatin1String("lumi.plug") &&
            !sensorNode.modelId().startsWith(QLatin1String("lumi.curtain")))
        {
            sensorNode.addItem(DataTypeUInt8, RConfigBattery);
        }

        if (!sensorNode.item(RStateTemperature) &&
            !sensorNode.modelId().contains(QLatin1String("weather")) &&
            !sensorNode.modelId().startsWith(QLatin1String("lumi.sensor_ht")))
        {
            sensorNode.addItem(DataTypeInt16, RConfigTemperature);
            //sensorNode.addItem(DataTypeInt16, RConfigOffset);
        }
    }
    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_EMBER ||
             node->nodeDescriptor().manufacturerCode() == VENDOR_120B)
    {
        sensorNode.setManufacturer("Heiman");
    }
    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_LGE)
    {
        sensorNode.setManufacturer("LG Electronics");
    }
    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_LUTRON)
    {
        sensorNode.setManufacturer("Lutron");

        if (modelId.startsWith(QLatin1String("LZL4BWHL")))
        {
            sensorNode.setMode(Sensor::ModeDimmer);
        }
    }
    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_KEEN_HOME)
    {
        sensorNode.setManufacturer("Keen Home Inc");
    }
    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_PHYSICAL)
    {
        sensorNode.setManufacturer("SmartThings");

        item = sensorNode.addItem(DataTypeString, RConfigAlert);
        item->setValue(R_ALERT_DEFAULT);
    }
    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_INNR)
    {
        sensorNode.setManufacturer("innr");
    }
    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_VISONIC)
    {
        sensorNode.setManufacturer("Visonic");
    }

    if (sensorNode.manufacturer().isEmpty() && !manufacturer.isEmpty())
    {
        sensorNode.setManufacturer(manufacturer);
    }

    if (sensorNode.manufacturer().isEmpty())
    {
        return; // required
    }

    if (clusterId == IAS_ZONE_CLUSTER_ID) {
        item = sensorNode.addItem(DataTypeBool, RStateLowBattery);
        item->setValue(false);
        item = sensorNode.addItem(DataTypeBool, RStateTampered);
        item->setValue(false);
    }

    QString uid = generateUniqueId(sensorNode.address().ext(), sensorNode.fingerPrint().endpoint, clusterId);
    sensorNode.setUniqueId(uid);

    if (!sensor2 && sensorNode.id().isEmpty())
    {
        openDb();
        sensorNode.setId(QString::number(getFreeSensorId()));
        sensorNode.setNeedSaveDatabase(true);
        closeDb();
    }

    if (sensorNode.name().isEmpty())
    {
        QString name = type;
        if (name.startsWith("ZHA"))
        {
            name.remove(0, 3);
        }
        sensorNode.setName(QString("%1 %2").arg(name).arg(sensorNode.id()));
    }

    // force reading attributes
    if (node->isRouter())
    {
        sensorNode.setNextReadTime(READ_BINDING_TABLE, queryTime);
        sensorNode.enableRead(READ_BINDING_TABLE);
        sensorNode.setLastRead(READ_BINDING_TABLE, idleTotalCounter);
        queryTime = queryTime.addSecs(1);
    }
    {
        std::vector<quint16>::const_iterator ci = fingerPrint.inClusters.begin();
        std::vector<quint16>::const_iterator cend = fingerPrint.inClusters.end();
        for (;ci != cend; ++ci)
        {
            if (*ci == OCCUPANCY_SENSING_CLUSTER_ID)
            {
                sensorNode.setNextReadTime(READ_OCCUPANCY_CONFIG, queryTime);
                sensorNode.enableRead(READ_OCCUPANCY_CONFIG);
                sensorNode.setLastRead(READ_OCCUPANCY_CONFIG, idleTotalCounter);
                queryTime = queryTime.addSecs(1);
            }
            else if (*ci == BASIC_CLUSTER_ID)
            {
                if (sensorNode.modelId().isEmpty())
                {
                    DBG_Printf(DBG_INFO, "SensorNode %u: %s read model id and vendor name\n", sensorNode.id().toUInt(), qPrintable(sensorNode.name()));
                    sensorNode.setNextReadTime(READ_MODEL_ID, queryTime);
                    sensorNode.setLastRead(READ_MODEL_ID, idleTotalCounter);
                    sensorNode.enableRead(READ_MODEL_ID);
                    queryTime = queryTime.addSecs(1);
                }

                if (sensorNode.manufacturer().isEmpty())
                {
                    sensorNode.setNextReadTime(READ_VENDOR_NAME, queryTime);
                    sensorNode.setLastRead(READ_VENDOR_NAME, idleTotalCounter);
                    sensorNode.enableRead(READ_VENDOR_NAME);
                    queryTime = queryTime.addSecs(1);
                }
            }
        }
    }

    sensorNode.setNeedSaveDatabase(true);

    if (sensor2)
    {
        DBG_Printf(DBG_INFO, "[7] update existing sensor %s (%s)\n", qPrintable(sensor2->id()), qPrintable(modelId));
        *sensor2 = sensorNode;
    }
    else
    {
        DBG_Printf(DBG_INFO, "SensorNode %s: %s added\n", qPrintable(sensorNode.id()), qPrintable(sensorNode.name()));
        sensors.push_back(sensorNode);
        sensor2 = &sensors.back();
        updateSensorEtag(sensor2);
    }

    if (searchSensorsState == SearchSensorsActive)
    {
        Event e(RSensors, REventAdded, sensorNode.id());
        enqueueEvent(e);

        // check missing queries
        if (!fastProbeTimer->isActive())
        {
            fastProbeTimer->start(100);
        }
    }

    sensor2->rx();
    checkSensorBindingsForAttributeReporting(sensor2);

    Q_Q(DeRestPlugin);
    q->startZclAttributeTimer(checkZclAttributesDelay);

    queSaveDb(DB_SENSORS , DB_SHORT_SAVE_DELAY);
}

/*! Updates  SensorNode fingerprint if needed.
    \param node - holds up to date data
    \param endpoint - related endpoint
    \param sensorNOde - optional sensor filter, might be 0
 */
void DeRestPluginPrivate::checkUpdatedFingerPrint(const deCONZ::Node *node, quint8 endpoint, Sensor *sensorNode)
{
    if (!node)
    {
        return;
    }

    deCONZ::SimpleDescriptor sd;
    if (node->copySimpleDescriptor(endpoint, &sd) != 0)
    {
        return;
    }

    std::vector<Sensor>::iterator i = sensors.begin();
    std::vector<Sensor>::iterator end = sensors.end();

    for (; i != end; ++i)
    {
        if (sensorNode && &*i != sensorNode)
        {
            continue;
        }

        if (i->address().ext() != node->address().ext())
        {
            continue;
        }

        if (i->deletedState() != Sensor::StateNormal)
        {
            continue;
        }

        // different endpoints for different versions of FLS-NB
        if (i->fingerPrint().endpoint != endpoint &&
            i->modelId().startsWith(QLatin1String("FLS-NB")))
        {

            bool update = false;
            SensorFingerprint &fp = i->fingerPrint();
            quint16 clusterId = 0;

            for (size_t c = 0; !update && c < fp.inClusters.size(); c++)
            {
                if (sd.cluster(fp.inClusters[c], deCONZ::ServerCluster))
                {
                    update = true;
                    break;
                }
            }

            for (size_t c = 0; !update && c < fp.outClusters.size(); c++)
            {
                if (sd.cluster(fp.outClusters[c], deCONZ::ClientCluster))
                {
                    update = true;
                    break;
                }
            }

            if (!update)
            {
                continue;
            }

            if      (i->type().endsWith(QLatin1String("Switch")))     { clusterId = ONOFF_CLUSTER_ID; }
            else if (i->type().endsWith(QLatin1String("LightLevel"))) { clusterId = ILLUMINANCE_MEASUREMENT_CLUSTER_ID; }
            else if (i->type().endsWith(QLatin1String("Presence")))   { clusterId = OCCUPANCY_SENSING_CLUSTER_ID; }

            DBG_Printf(DBG_INFO, "change 0x%016llX finger print ep: 0x%02X --> 0x%02X\n", i->address().ext(), fp.endpoint, endpoint);

            fp.endpoint = sd.endpoint();
            fp.profileId = sd.profileId();

            updateSensorEtag(&*i);
            i->setUniqueId(generateUniqueId(i->address().ext(), fp.endpoint, clusterId));
            i->setNeedSaveDatabase(true);
            queSaveDb(DB_SENSORS, DB_SHORT_SAVE_DELAY);
        }
    }
}

/*! Updates  ZHALightLevel sensor /state: lightlevel, lux, dark and daylight.
    \param sensor - the sensor
    \param measuredValue - 16-bit light level
 */
void DeRestPluginPrivate::updateSensorLightLevel(Sensor &sensor, quint16 measuredValue)
{
    const quint16 measuredValueIn = measuredValue;
    ResourceItem *item = sensor.item(RStateLightLevel);

    if (!item)
    {
        return;
    }

    if (sensor.modelId().startsWith(QLatin1String("lumi.sensor_motion")))
    {
        // measured value is given as lux: transform
        // ZCL Attribute = 10.000 * log10(Illuminance (lx)) + 1
        double ll = 10000 * std::log10(measuredValue) + 1;
        if (ll > 0xfffe) { measuredValue = 0xfffe; }
        else             { measuredValue = ll; }
    }

    if (item)
    {
        item->setValue(measuredValue);
        sensor.updateStateTimestamp();
        sensor.setNeedSaveDatabase(true);
        Event e(RSensors, RStateLightLevel, sensor.id(), item);
        enqueueEvent(e);
        enqueueEvent(Event(RSensors, RStateLastUpdated, sensor.id()));
    }

    quint16 tholddark = R_THOLDDARK_DEFAULT;
    quint16 tholdoffset = R_THOLDOFFSET_DEFAULT;
    item = sensor.item(RConfigTholdDark);
    if (item)
    {
        tholddark = item->toNumber();
    }
    item = sensor.item(RConfigTholdOffset);
    if (item)
    {
        tholdoffset = item->toNumber();
    }
    bool dark = measuredValue <= tholddark;
    bool daylight = measuredValue >= tholddark + tholdoffset;

    item = sensor.item(RStateDark);
    // if (!item)
    // {
    //     item = sensor.addItem(DataTypeBool, RStateDark);
        DBG_Assert(item != 0);
    // }
    if (item && item->setValue(dark))
    {
        if (item->lastChanged() == item->lastSet())
        {
            Event e(RSensors, RStateDark, sensor.id(), item);
            enqueueEvent(e);
        }
    }

    item = sensor.item(RStateDaylight);
    // if (!item)
    // {
    //     item = sensor.addItem(DataTypeBool, RStateDaylight);
        DBG_Assert(item != 0);
    // }
    if (item && item->setValue(daylight))
    {
        if (item->lastChanged() == item->lastSet())
        {
            Event e(RSensors, RStateDaylight, sensor.id(), item);
            enqueueEvent(e);
        }
    }

    item = sensor.item(RStateLux);

    // if (!item)
    // {
    //     item = sensor.addItem(DataTypeUInt32, RStateLux);
        DBG_Assert(item != 0);
    // }

    if (item)
    {
        quint32 lux = 0;
        if (sensor.modelId().startsWith(QLatin1String("lumi.sensor_motion")))
        {   // measured values is actually given in lux
            lux = measuredValueIn;
        }
        else if (measuredValue > 0 && measuredValue < 0xffff)
        {
            // valid values are 1 - 0xfffe
            // 0, too low to measure
            // 0xffff invalid value

            // ZCL Attribute = 10.000 * log10(Illuminance (lx)) + 1
            // lux = 10^((ZCL Attribute - 1)/10.000)
            qreal exp = measuredValue - 1;
            qreal l = qPow(10, exp / 10000.0f);
            l += 0.5;   // round value
            lux = static_cast<quint32>(l);
        }
        item->setValue(lux);
        if (item->lastChanged() == item->lastSet())
        {
            Event e(RSensors, RStateLux, sensor.id(), item);
            enqueueEvent(e);
        }
    }
}

/*! Updates/adds a SensorNode from a Node.
    If the node does not exist it will be created
    otherwise the values will be checked for change
    and updated in the internal representation.
    \param node - holds up to date data
 */
void DeRestPluginPrivate::updateSensorNode(const deCONZ::NodeEvent &event)
{
    if (!event.node())
    {
        return;
    }

    std::vector<Sensor>::iterator i = sensors.begin();
    std::vector<Sensor>::iterator end = sensors.end();

    for (; i != end; ++i)
    {
        if (i->address().ext() != event.node()->address().ext())
        {
            continue;
        }

        if (i->deletedState() != Sensor::StateNormal)
        {
            continue;
        }

        if (i->node() != event.node())
        {
            i->setNode(const_cast<deCONZ::Node*>(event.node()));
            DBG_Printf(DBG_INFO, "Sensor %s set node %s\n", qPrintable(i->id()), qPrintable(event.node()->address().toStringExt()));
        }

        if (event.event() == deCONZ::NodeEvent::UpdatedClusterDataZclReport ||
            event.event() == deCONZ::NodeEvent::UpdatedClusterDataZclRead)
        {
            i->rx();
            i->incrementRxCounter();
        }

        checkSensorNodeReachable(&*i, &event);

        if (!i->isAvailable())
        {
            continue;
        }

        if (event.event() == deCONZ::NodeEvent::UpdatedPowerDescriptor)
        {
            if (event.node()->powerDescriptor().isValid())
            {
                ResourceItem *item = i->item(RConfigBattery);
                int battery = 255; // invalid

                if (event.node()->powerDescriptor().currentPowerSource() == deCONZ::PowerSourceRechargeable ||
                    event.node()->powerDescriptor().currentPowerSource() == deCONZ::PowerSourceDisposable)
                {
                    switch (event.node()->powerDescriptor().currentPowerLevel())
                    {
                    case deCONZ::PowerLevel100:      battery = 100; break;
                    case deCONZ::PowerLevel66:       battery = 66; break;
                    case deCONZ::PowerLevel33:       battery = 33; break;
                    case deCONZ::PowerLevelCritical: battery = 0; break;
                    default:
                        break;
                    }
                }

                if (item)
                {
                    item->setValue(battery);
                    Event e(RSensors, RConfigBattery, i->id(), item);
                    enqueueEvent(e);
                }
                updateSensorEtag(&*i);
            }
            return;
        }

        // filter for relevant clusters
        if (event.profileId() == HA_PROFILE_ID || event.profileId() == ZLL_PROFILE_ID)
        {
            switch (event.clusterId())
            {
            case ILLUMINANCE_MEASUREMENT_CLUSTER_ID:
            case TEMPERATURE_MEASUREMENT_CLUSTER_ID:
            case RELATIVE_HUMIDITY_CLUSTER_ID:
            case PRESSURE_MEASUREMENT_CLUSTER_ID:
            case OCCUPANCY_SENSING_CLUSTER_ID:
            case POWER_CONFIGURATION_CLUSTER_ID:
            case BASIC_CLUSTER_ID:
            case ONOFF_CLUSTER_ID:
            case ANALOG_INPUT_CLUSTER_ID:
            case MULTISTATE_INPUT_CLUSTER_ID:
            case BINARY_INPUT_CLUSTER_ID:
            case METERING_CLUSTER_ID:
            case ELECTRICAL_MEASUREMENT_CLUSTER_ID:
                break;

            case VENDOR_CLUSTER_ID:
            {
                // ubisys device management (UBISYS_DEVICE_SETUP_CLUSTER_ID)
                if (event.endpoint() == 0xE8 && (event.node()->address().ext() & macPrefixMask) == ubisysMacPrefix)
                {
                    break;
                }
            }
                continue; // ignore

            default:
                continue; // don't process further
            }
        }
        else
        {
            continue;
        }


        if (event.clusterId() != BASIC_CLUSTER_ID && event.clusterId() != POWER_CONFIGURATION_CLUSTER_ID && event.clusterId() != VENDOR_CLUSTER_ID)
        {
            // filter endpoint
            if (event.endpoint() != i->fingerPrint().endpoint)
            {
                if ((event.node()->address().ext() & macPrefixMask) == jennicMacPrefix)
                {
                    if (i->modelId().startsWith(QLatin1String("lumi.sensor_86sw")) ||
                        i->modelId().startsWith(QLatin1String("lumi.ctrl_neutral")) ||
                        (i->modelId().startsWith(QLatin1String("lumi.ctrl_ln")) && event.clusterId() == MULTISTATE_INPUT_CLUSTER_ID) ||
                        (i->modelId().startsWith(QLatin1String("lumi.remote")) && event.clusterId() == MULTISTATE_INPUT_CLUSTER_ID))
                    { // 3 endpoints: 1 sensor
                    }
                    else
                    {
                        continue;
                    }
                }
                else
                {
                    continue;
                }
            }

            // assume data must be in server cluster attribute
            bool found = false;
            std::vector<quint16>::const_iterator ci = i->fingerPrint().inClusters.begin();
            std::vector<quint16>::const_iterator cend = i->fingerPrint().inClusters.end();
            for (; ci != cend; ++ci)
            {
                if (*ci == event.clusterId())
                {
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                continue;
            }
        }

        deCONZ::SimpleDescriptor sd;
        if (event.node()->copySimpleDescriptor(event.endpoint(), &sd) == 0)
        {
            QList<deCONZ::ZclCluster>::const_iterator ic = sd.inClusters().constBegin();
            QList<deCONZ::ZclCluster>::const_iterator endc = sd.inClusters().constEnd();

            for (; ic != endc; ++ic)
            {
                if (ic->id() == event.clusterId())
                {
                    std::vector<deCONZ::ZclAttribute>::const_iterator ia = ic->attributes().begin();
                    std::vector<deCONZ::ZclAttribute>::const_iterator enda = ic->attributes().end();

                    NodeValue::UpdateType updateType = NodeValue::UpdateInvalid;
                    if (event.event() == deCONZ::NodeEvent::UpdatedClusterDataZclRead)
                    {
                        updateType = NodeValue::UpdateByZclRead;
                    }
                    else if (event.event() == deCONZ::NodeEvent::UpdatedClusterDataZclReport)
                    {
                        updateType = NodeValue::UpdateByZclReport;
                    }

                    if (event.clusterId() == POWER_CONFIGURATION_CLUSTER_ID)
                    {
                        for (;ia != enda; ++ia)
                        {
                            if (!ia->isAvailable())
                            {
                                continue;
                            }

                            if (std::find(event.attributeIds().begin(),
                                          event.attributeIds().end(),
                                          ia->id()) == event.attributeIds().end())
                            {
                                continue;
                            }

                            if (ia->id() == 0x0021) // battery percentage remaining
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.clusterId(), ia->id(), ia->numericValue());
                                    pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().u8);
                                }

                                ResourceItem *item = i->item(RConfigBattery);

                                if (!item && ia->numericValue().u8 > 0) // valid value: create resource item
                                {
                                    item = i->addItem(DataTypeUInt8, RConfigBattery);
                                }

                                // Specifies the remaining battery life as a half integer percentage of the full battery capacity (e.g., 34.5%, 45%,
                                // 68.5%, 90%) with a range between zero and 100%, with 0x00 = 0%, 0x64 = 50%, and 0xC8 = 100%. This is
                                // particularly suited for devices with rechargeable batteries.
                                if (item)
                                {
                                    int bat = ia->numericValue().u8 / 2;

                                    if (i->modelId().startsWith("TRADFRI"))
                                    {
                                        bat = ia->numericValue().u8;
                                    }

                                    if (item->toNumber() != bat)
                                    {
                                        i->setNeedSaveDatabase(true);
                                        queSaveDb(DB_SENSORS, DB_HUGE_SAVE_DELAY);
                                    }
                                    item->setValue(bat);
                                    Event e(RSensors, RConfigBattery, i->id(), item);
                                    enqueueEvent(e);
                                }

                                updateSensorEtag(&*i);
                            }
                            else if (ia->id() == 0x0020) // battery voltage
                            {
                                if (!i->modelId().startsWith(QLatin1String("tagv4"))) // SmartThings Arrival sensor
                                {
                                    continue;
                                }

                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.clusterId(), ia->id(), ia->numericValue());
                                    pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().u8);
                                }

                                ResourceItem *item = i->item(RConfigBattery);

                                if (!item && ia->numericValue().u8 > 0) // valid value: create resource item
                                {
                                    item = i->addItem(DataTypeUInt8, RConfigBattery);
                                }

                                if (item)
                                {
                                    int battery = ia->numericValue().u8; // in 0.1 V
                                    const float vmin = 20; // TODO: check - I've seen 24
                                    const float vmax = 30; // TODO: check - I've seen 29
                                    float bat = battery;

                                    if      (bat > vmax) { bat = vmax; }
                                    else if (bat < vmin) { bat = vmin; }

                                    bat = ((bat - vmin) / (vmax - vmin)) * 100;

                                    if      (bat > 100) { bat = 100; }
                                    else if (bat <= 0)  { bat = 1; } // ?

                                    if (item->toNumber() != bat)
                                    {
                                        i->setNeedSaveDatabase(true);
                                        queSaveDb(DB_SENSORS, DB_HUGE_SAVE_DELAY);
                                    }
                                    item->setValue(bat);
                                    Event e(RSensors, RConfigBattery, i->id(), item);
                                    enqueueEvent(e);
                                }
                                updateSensorEtag(&*i);
                            }
                            else if (ia->id() == 0x0035) // battery alarm mask
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.clusterId(), ia->id(), ia->numericValue());
                                    pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().u8);
                                }

                                ResourceItem *item = i->item(RStateLowBattery);
                                if (!item)
                                {
                                    item = i->addItem(DataTypeBool, RStateLowBattery);
                                    i->setNeedSaveDatabase(true);
                                    queSaveDb(DB_SENSORS, DB_SHORT_SAVE_DELAY);
                                }

                                if (item)
                                {
                                    bool lowBat = (ia->numericValue().u8 & 0x01);
                                    if (!item->lastSet().isValid() || item->toBool() != lowBat)
                                    {
                                        item->setValue(lowBat);
                                        enqueueEvent(Event(RSensors, RStateLowBattery, i->id(), item));
                                        i->setNeedSaveDatabase(true);
                                        queSaveDb(DB_SENSORS, DB_HUGE_SAVE_DELAY);
                                    }
                                }
                            }
                        }
                    }
                    else if (event.clusterId() == ILLUMINANCE_MEASUREMENT_CLUSTER_ID)
                    {
                        for (;ia != enda; ++ia)
                        {
                            if (std::find(event.attributeIds().begin(),
                                          event.attributeIds().end(),
                                          ia->id()) == event.attributeIds().end())
                            {
                                continue;
                            }

                            if (ia->id() == 0x0000) // measured illuminance (lux)
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.clusterId(), 0x0000, ia->numericValue());
                                    pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().u16);
                                }

                                updateSensorLightLevel(*i, ia->numericValue().u16); // ZigBee uses a 16-bit measured value
                                updateSensorEtag(&*i);
                            }
                        }
                    }
                    else if (event.clusterId() == TEMPERATURE_MEASUREMENT_CLUSTER_ID)
                    {
                        for (;ia != enda; ++ia)
                        {
                            if (ia->id() == 0x0000) // temperature (0.01 °C)
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.clusterId(), 0x0000, ia->numericValue());
                                    pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().s16);
                                }

                                int temp = ia->numericValue().s16;
                                ResourceItem *item = i->item(RStateTemperature);

                                if (item)
                                {
                                    ResourceItem *item2 = i->item(RConfigOffset);
                                    if (item2 && item2->toNumber() != 0)
                                    {
                                        temp += item2->toNumber();
                                    }
                                    item->setValue(temp);
                                    i->updateStateTimestamp();
                                    i->setNeedSaveDatabase(true);
                                    Event e(RSensors, RStateTemperature, i->id(), item);
                                    enqueueEvent(e);
                                    enqueueEvent(Event(RSensors, RStateLastUpdated, i->id()));
                                }

                                updateSensorEtag(&*i);
                            }
                        }
                    }
                    else if (event.clusterId() == RELATIVE_HUMIDITY_CLUSTER_ID)
                    {
                        for (;ia != enda; ++ia)
                        {
                            if (ia->id() == 0x0000) // relative humidity
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.clusterId(), 0x0000, ia->numericValue());
                                    pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().u16);
                                }

                                int humidity = ia->numericValue().u16;
                                ResourceItem *item = i->item(RStateHumidity);

                                if (item)
                                {
                                    ResourceItem *item2 = i->item(RConfigOffset);
                                    if (item2 && item2->toNumber() != 0)
                                    {
                                        qint16 _humidity = humidity + item2->toNumber();
                                        humidity = _humidity < 0 ? 0 : _humidity > 10000 ? 10000 : _humidity;
                                    }
                                    item->setValue(humidity);
                                    i->updateStateTimestamp();
                                    i->setNeedSaveDatabase(true);
                                    Event e(RSensors, RStateHumidity, i->id(), item);
                                    enqueueEvent(e);
                                    enqueueEvent(Event(RSensors, RStateLastUpdated, i->id()));
                                }

                                updateSensorEtag(&*i);
                            }
                        }
                    }
                    else if (event.clusterId() == PRESSURE_MEASUREMENT_CLUSTER_ID)
                    {
                        for (;ia != enda; ++ia)
                        {
                            if (ia->id() == 0x0000) // pressure
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.clusterId(), 0x0000, ia->numericValue());
                                    pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().s16);
                                }

                                qint16 pressure = ia->numericValue().s16;
                                ResourceItem *item = i->item(RStatePressure);

                                if (item)
                                {
                                    item->setValue(pressure);
                                    i->updateStateTimestamp();
                                    i->setNeedSaveDatabase(true);
                                    Event e(RSensors, RStatePressure, i->id(), item);
                                    enqueueEvent(e);
                                    enqueueEvent(Event(RSensors, RStateLastUpdated, i->id()));
                                }

                                updateSensorEtag(&*i);
                            }
                        }
                    }
                    else if (event.clusterId() == OCCUPANCY_SENSING_CLUSTER_ID)
                    {
                        for (;ia != enda; ++ia)
                        {
                            if (std::find(event.attributeIds().begin(),
                                          event.attributeIds().end(),
                                          ia->id()) == event.attributeIds().end())
                            {
                                continue;
                            }

                            if (ia->id() == 0x0000) // occupied state
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.clusterId(), 0x0000, ia->numericValue());
                                    pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().u8);
                                }

                                const NodeValue &val = i->getZclValue(event.clusterId(), 0x0000);

                                ResourceItem *item = i->item(RStatePresence);

                                if (item)
                                {
                                    item->setValue(ia->numericValue().u8);
                                    i->updateStateTimestamp();
                                    i->setNeedSaveDatabase(true);
                                    Event e(RSensors, RStatePresence, i->id(), item);
                                    enqueueEvent(e);
                                    enqueueEvent(Event(RSensors, RStateLastUpdated, i->id()));

                                    // prepare to automatically set presence to false
                                    if (item->toBool())
                                    {
                                        if (val.clusterId == event.clusterId() && val.maxInterval > 0 &&
                                            updateType == NodeValue::UpdateByZclReport)
                                        {
                                            // prevent setting presence back to false, when report.maxInterval > config.duration
                                            i->durationDue = item->lastSet().addSecs(val.maxInterval);
                                        }
                                        else
                                        {
                                            ResourceItem *item2 = i->item(RConfigDuration);
                                            if (item2 && item2->toNumber() > 0)
                                            {
                                                i->durationDue = item->lastSet().addSecs(item2->toNumber());
                                            }
                                        }
                                    }
                                }
                                updateSensorEtag(&*i);

                            }
                            else if ((i->modelId().startsWith(QLatin1String("FLS-NB")) ||
                                      i->modelId() == QLatin1String("LG IP65 HMS"))
                                     && ia->id() == 0x0010) // occupied to unoccupied delay
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.clusterId(), ia->id(), ia->numericValue());
                                }

                                quint16 duration = ia->numericValue().u16;
                                ResourceItem *item = i->item(RConfigDuration);

                                if (!item)
                                {
                                    item = i->addItem(DataTypeUInt16, RConfigDuration);
                                }

                                if (item && item->toNumber() != duration)
                                {
                                    Event e(RSensors, RConfigDuration, i->id(), item);
                                    enqueueEvent(e);

                                    if (item->toNumber() <= 0)
                                    {
                                        DBG_Printf(DBG_INFO, "got occupied to unoccupied delay %u\n", ia->numericValue().u16);
                                        item->setValue(duration);
                                        i->setNeedSaveDatabase(true);
                                        updateSensorEtag(&*i);
                                    }
                                    else
                                    {
                                        DBG_Printf(DBG_INFO, "occupied to unoccupied delay is %u should be %u, force rewrite\n", ia->numericValue().u16, (quint16)item->toNumber());
                                        if (!i->mustRead(WRITE_OCCUPANCY_CONFIG))
                                        {
                                            i->enableRead(WRITE_OCCUPANCY_CONFIG);
                                            i->setNextReadTime(WRITE_OCCUPANCY_CONFIG, queryTime);
                                            queryTime = queryTime.addSecs(1);
                                        }

                                        if (!i->mustRead(READ_OCCUPANCY_CONFIG))
                                        {
                                            i->enableRead(READ_OCCUPANCY_CONFIG);
                                            i->setNextReadTime(READ_OCCUPANCY_CONFIG, queryTime);
                                            queryTime = queryTime.addSecs(5);
                                        }
                                        Q_Q(DeRestPlugin);
                                        q->startZclAttributeTimer(checkZclAttributesDelay);
                                    }
                                }
                            }
                            else if (ia->id() == 0x0010) // occupied to unoccupied delay
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.clusterId(), ia->id(), ia->numericValue());
                                }

                                quint16 delay = ia->numericValue().u16;
                                ResourceItem *item = i->item(RConfigDelay);

                                if (item && item->toNumber() != delay)
                                {
                                    item->setValue(delay);
                                    i->setNeedSaveDatabase(true);
                                    Event e(RSensors, RConfigDelay, i->id(), item);
                                    enqueueEvent(e);
                                }

                                updateSensorEtag(&*i);
                            }
                            else if (ia->id() == 0x0030) // sensitivity
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.clusterId(), ia->id(), ia->numericValue());
                                    NodeValue &val = i->getZclValue(event.clusterId(), ia->id());
                                    // allow proper binding checks
                                    if (val.minInterval == 0 || val.maxInterval == 0)
                                    {
                                        val.minInterval = 5;      // value used by Hue bridge
                                        val.maxInterval = 7200;   // value used by Hue bridge
                                    }
                                }

                                quint8 sensitivity = ia->numericValue().u8;
                                ResourceItem *item = i->item(RConfigSensitivity);

                                if (item && item->toNumber() != sensitivity)
                                {
                                    item->setValue(sensitivity);
                                    i->setNeedSaveDatabase(true);
                                    Event e(RSensors, RConfigSensitivity, i->id(), item);
                                    enqueueEvent(e);
                                }

                                updateSensorEtag(&*i);
                            }
                            else if (ia->id() == 0x0031) // sensitivitymax
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.clusterId(), ia->id(), ia->numericValue());
                                }

                                quint8 sensitivitymax = ia->numericValue().u8;
                                ResourceItem *item = i->item(RConfigSensitivityMax);

                                if (item && item->toNumber() != sensitivitymax)
                                {
                                    item->setValue(sensitivitymax);
                                    i->setNeedSaveDatabase(true);
                                    Event e(RSensors, RConfigSensitivityMax, i->id(), item);
                                    enqueueEvent(e);
                                }

                                updateSensorEtag(&*i);
                            }
                        }
                    }
                    else if (event.clusterId() == ONOFF_CLUSTER_ID)
                    {
                        for (;ia != enda; ++ia)
                        {
                            if (ia->id() == 0x0000) // onoff
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.clusterId(), ia->id(), ia->numericValue());
                                    pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().u8);
                                }

                                ResourceItem *item = i->item(RStateOpen);

                                if (item)
                                {
                                    bool open = ia->numericValue().u8 == 1;
                                    item->setValue(open);

                                    if (item->lastSet() == item->lastChanged())
                                    {
                                        Event e(RSensors, item->descriptor().suffix, i->id(), item);
                                        enqueueEvent(e);
                                    }
                                    i->setNeedSaveDatabase(true);
                                    i->updateStateTimestamp();
                                    enqueueEvent(Event(RSensors, RStateLastUpdated, i->id()));
                                }

                                item = i->item(RStateButtonEvent);

                                if (item && !i->buttonMap() &&
                                    event.event() == deCONZ::NodeEvent::UpdatedClusterDataZclReport)
                                {
                                    quint32 button = 0;

                                    if (i->modelId().startsWith(QLatin1String("lumi.sensor_86sw")))
                                    {
                                        button = (S_BUTTON_1 * event.endpoint()) + S_BUTTON_ACTION_SHORT_RELEASED;
                                    }
                                    else if (i->modelId().startsWith(QLatin1String("lumi.ctrl_neutral")))
                                    {
                                        switch (event.endpoint())
                                        {
                                        case 4: button = S_BUTTON_1 + S_BUTTON_ACTION_SHORT_RELEASED; break;
                                        case 5: button = S_BUTTON_2 + S_BUTTON_ACTION_SHORT_RELEASED; break;
                                        case 6: button = S_BUTTON_3 + S_BUTTON_ACTION_SHORT_RELEASED; break;
                                        default: // should not happen
                                            // button = (S_BUTTON_1 * event.endpoint()) + S_BUTTON_ACTION_SHORT_RELEASED;
                                            break;
                                        }
                                    }
                                    else
                                    {
                                        if (ia->numericValue().u8 == 0) { button = S_BUTTON_1 + S_BUTTON_ACTION_INITIAL_PRESS; }
                                        else                            { button = S_BUTTON_1 + S_BUTTON_ACTION_SHORT_RELEASED; }
                                    }

                                    if (button)
                                    {
                                        item->setValue(button);

                                        Event e(RSensors, item->descriptor().suffix, i->id(), item);
                                        enqueueEvent(e);
                                        i->setNeedSaveDatabase(true);
                                        i->updateStateTimestamp();
                                        enqueueEvent(Event(RSensors, RStateLastUpdated, i->id()));
                                    }
                                }

                                updateSensorEtag(&*i);
                            }
                        }
                    }
                    else if (event.clusterId() == BASIC_CLUSTER_ID)
                    {
                        DBG_Printf(DBG_INFO_L2, "Update Sensor 0x%016llX Basic Cluster\n", event.node()->address().ext());
                        for (;ia != enda; ++ia)
                        {
                            if (ia->id() == 0x0005) // Model identifier
                            {
                                if (i->mustRead(READ_MODEL_ID))
                                {
                                    i->clearRead(READ_MODEL_ID);
                                }

                                QString str = ia->toString().simplified();
                                if (!str.isEmpty())
                                {
                                    if (i->modelId() != str)
                                    {
                                        i->setModelId(str);
                                        i->setNeedSaveDatabase(true);
                                        checkInstaModelId(&*i);
                                        updateSensorEtag(&*i);
                                        pushSensorInfoToCore(&*i);
                                        queSaveDb(DB_SENSORS, DB_LONG_SAVE_DELAY);
                                    }

                                    if (i->name() == QString("Switch %1").arg(i->id()))
                                    {
                                        QString name = QString("%1 %2").arg(str).arg(i->id());
                                        if (i->name() != name)
                                        {
                                            i->setName(name);
                                            i->setNeedSaveDatabase(true);
                                            updateSensorEtag(&*i);
                                        }
                                    }
                                }
                            }
                            else if (ia->id() == 0x0004) // Manufacturer Name
                            {
                                if (i->mustRead(READ_VENDOR_NAME))
                                {
                                    i->clearRead(READ_VENDOR_NAME);
                                }

                                QString str = ia->toString().simplified();
                                if (!str.isEmpty())
                                {
                                    if (i->manufacturer() != str)
                                    {
                                        updateSensorEtag(&*i);
                                        i->setManufacturer(str);
                                        i->setNeedSaveDatabase(true);
                                        pushSensorInfoToCore(&*i);
                                        queSaveDb(DB_SENSORS, DB_LONG_SAVE_DELAY);
                                    }
                                }
                            }
                            else if (ia->id() == 0x0006) // Date code as fallback for sw build id
                            {
                                QString str = ia->toString().simplified();
                                if (!i->swVersion().isEmpty() && !i->modelId().startsWith(QLatin1String("lumi.")))
                                {
                                    // check
                                }
                                else if (!str.isEmpty() && str != i->swVersion())
                                {
                                    i->setSwVersion(str);
                                    i->setNeedSaveDatabase(true);
                                    pushSensorInfoToCore(&*i);
                                    queSaveDb(DB_SENSORS, DB_LONG_SAVE_DELAY);
                                    updateSensorEtag(&*i);
                                }
                            }
                            else if (ia->id() == 0x4000) // Software build identifier
                            {
                                if (i->mustRead(READ_SWBUILD_ID))
                                {
                                    i->clearRead(READ_SWBUILD_ID);
                                }
                                QString str = ia->toString().simplified();
                                if (!str.isEmpty())
                                {
                                    if (str != i->swVersion())
                                    {
                                        i->setSwVersion(str);
                                        i->setNeedSaveDatabase(true);
                                        pushSensorInfoToCore(&*i);
                                        queSaveDb(DB_SENSORS, DB_LONG_SAVE_DELAY);
                                        updateSensorEtag(&*i);
                                    }
                                }
                            }
                            else if (ia->id() == 0x0032) // usertest
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.clusterId(), ia->id(), ia->numericValue());
                                }

                                bool usertest = ia->numericValue().u8 == 1;
                                ResourceItem *item = i->item(RConfigUsertest);

                                if (item && item->toNumber() != usertest)
                                {
                                    item->setValue(usertest);
                                    i->setNeedSaveDatabase(true);
                                    Event e(RSensors, RConfigUsertest, i->id(), item);
                                    enqueueEvent(e);
                                }

                                updateSensorEtag(&*i);
                            }
                            else if (ia->id() == 0x0033) // ledindication
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.clusterId(), ia->id(), ia->numericValue());
                                }

                                bool ledindication = ia->numericValue().u8 == 1;
                                ResourceItem *item = i->item(RConfigLedIndication);

                                if (item && item->toNumber() != ledindication)
                                {
                                    item->setValue(ledindication);
                                    i->setNeedSaveDatabase(true);
                                    Event e(RSensors, RConfigLedIndication, i->id(), item);
                                    enqueueEvent(e);
                                }

                                updateSensorEtag(&*i);
                            }
                        }
                    }
                    else if (event.clusterId() == ANALOG_INPUT_CLUSTER_ID)
                    {
                        for (;ia != enda; ++ia)
                        {
                            if (ia->id() == 0x0055) // present value
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.clusterId(), ia->id(), ia->numericValue());
                                }

                                if (i->modelId().startsWith(QLatin1String("lumi.sensor_cube")))
                                {
                                    qint32 buttonevent = ia->numericValue().real * 100;
                                    ResourceItem *item = i->item(RStateButtonEvent);

                                    if (item)
                                    {
                                        item->setValue(buttonevent);
                                        i->updateStateTimestamp();
                                        i->setNeedSaveDatabase(true);
                                        Event e(RSensors, RStateButtonEvent, i->id(), item);
                                        enqueueEvent(e);
                                        enqueueEvent(Event(RSensors, RStateLastUpdated, i->id()));
                                    }

                                    updateSensorEtag(&*i);
                                }
                                else if (i->modelId() == QLatin1String("lumi.plug") ||
                                         i->modelId().startsWith(QLatin1String("lumi.ctrl_")))
                                {
                                    if (i->type() == QLatin1String("ZHAPower"))
                                    {
                                        qint16 power = ia->numericValue().real;
                                        ResourceItem *item = i->item(RStatePower);

                                        if (item)
                                        {
                                            item->setValue(power); // in W
                                            i->updateStateTimestamp();
                                            i->setNeedSaveDatabase(true);
                                            enqueueEvent(Event(RSensors, RStatePower, i->id(), item));
                                            enqueueEvent(Event(RSensors, RStateLastUpdated, i->id()));
                                        }
                                        updateSensorEtag(&*i);
                                    }
                                    else if (i->type() == QLatin1String("ZHAConsumption"))
                                    {
                                        qint64 consumption = ia->numericValue().real * 1000;
                                        ResourceItem *item = i->item(RStateConsumption);

                                        if (item)
                                        {
                                            item->setValue(consumption); // in 0.001 kWh
                                            i->updateStateTimestamp();
                                            i->setNeedSaveDatabase(true);
                                            enqueueEvent(Event(RSensors, RStateConsumption, i->id(), item));
                                            enqueueEvent(Event(RSensors, RStateLastUpdated, i->id()));
                                        }
                                        updateSensorEtag(&*i);
                                    }
                                }
                            }
                        }
                    }
                    else if (event.clusterId() == MULTISTATE_INPUT_CLUSTER_ID)
                    {
                        for (;ia != enda; ++ia)
                        {
                            if (ia->id() == 0x0055) // present value
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.clusterId(), ia->id(), ia->numericValue());
                                    pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().u16);
                                }

                                qint32 buttonevent = -1;
                                ResourceItem *item = i->item(RStateButtonEvent);
                                int rawValue = ia->numericValue().u16;

                                if (i->modelId().startsWith(QLatin1String("lumi.sensor_cube")))
                                {
                                    // Map Xiaomi Mi smart cube raw values to buttonevent values
                                    static const int sideMap[] = {1, 3, 5, 6, 4, 2};
                                    int side = sideMap[rawValue & 0x0007];
                                    int previousSide = sideMap[(rawValue & 0x0038) >> 3];
                                         if (rawValue == 0x0002) { buttonevent = 7000; }                       // wakeup
                                    else if (rawValue == 0x0000) { buttonevent = 7007; }                       // shake
                                    else if (rawValue == 0x0003) { buttonevent = 7008; }                       // drop
                                    else if (rawValue & 0x0040)  { buttonevent = side * 1000 + previousSide; } // flip 90°
                                    else if (rawValue & 0x0080)  { buttonevent = side * 1000 + 7 - side; }     // flip 180°
                                    else if (rawValue & 0x0100)  { buttonevent = side * 1000; }                // push
                                    else if (rawValue & 0x0200)  { buttonevent = side * 1000 + side; }         // double tap
                                }
                                else if (i->modelId() == QLatin1String("lumi.sensor_switch.aq3"))
                                {
                                    switch (rawValue)
                                    {
                                        case  1: buttonevent = S_BUTTON_1 + S_BUTTON_ACTION_SHORT_RELEASED; break;
                                        case  2: buttonevent = S_BUTTON_1 + S_BUTTON_ACTION_DOUBLE_PRESS;   break;
                                        case 16: buttonevent = S_BUTTON_1 + S_BUTTON_ACTION_HOLD;           break;
                                        case 17: buttonevent = S_BUTTON_1 + S_BUTTON_ACTION_LONG_RELEASED;  break;
                                        case 18: buttonevent = S_BUTTON_1 + S_BUTTON_ACTION_SHAKE;          break;
                                        default: break;
                                    }
                                }
                                else if (i->modelId() == QLatin1String("lumi.remote.b1acn01") ||
                                         i->modelId() == QLatin1String("lumi.remote.b186acn01") ||
                                         i->modelId() == QLatin1String("lumi.remote.b286acn01"))
                                {
                                    buttonevent = S_BUTTON_1 * event.endpoint();
                                    switch (rawValue)
                                    {
                                        case   0: buttonevent += S_BUTTON_ACTION_HOLD;           break;
                                        case   1: buttonevent += S_BUTTON_ACTION_SHORT_RELEASED; break;
                                        case   2: buttonevent += S_BUTTON_ACTION_DOUBLE_PRESS;   break;
                                        case 255: buttonevent += S_BUTTON_ACTION_LONG_RELEASED;  break;
                                        default:
                                        {
                                            DBG_Printf(DBG_INFO, "unsupported button rawValue 0x%04X\n", rawValue);
                                            buttonevent = -1;
                                        }
                                            break;
                                    }
                                }
                                else if (i->modelId().startsWith(QLatin1String("lumi.ctrl_ln")))
                                {
                                    switch (event.endpoint())
                                    {
                                        case 0x05: buttonevent = S_BUTTON_1; break;
                                        case 0x06: buttonevent = S_BUTTON_2; break;
                                        case 0x07: buttonevent = S_BUTTON_3; break;
                                        default: break;
                                    }
                                    if (buttonevent != -1)
                                    {
                                        switch (rawValue) {
                                            case 1: buttonevent += S_BUTTON_ACTION_SHORT_RELEASED; break;
                                            case 2: buttonevent += S_BUTTON_ACTION_DOUBLE_PRESS;   break;
                                            default: buttonevent = -1; break;
                                        }
                                    }
                                }
                                if (item && buttonevent != -1)
                                {
                                    item->setValue(buttonevent);
                                    i->updateStateTimestamp();
                                    i->setNeedSaveDatabase(true);
                                    Event e(RSensors, RStateButtonEvent, i->id(), item);
                                    enqueueEvent(e);
                                    enqueueEvent(Event(RSensors, RStateLastUpdated, i->id()));
                                }

                                updateSensorEtag(&*i);
                            }
                        }
                    }
                    else if (event.clusterId() == BINARY_INPUT_CLUSTER_ID)
                    {
                        for (;ia != enda; ++ia)
                        {
                            if (ia->id() == 0x0055) // present value
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.clusterId(), ia->id(), ia->numericValue());
                                }

                                const NodeValue &val = i->getZclValue(event.clusterId(), 0x0055);

                                ResourceItem *item = i->item(RStatePresence);

                                if (item)
                                {
                                    item->setValue(true);
                                    i->updateStateTimestamp();
                                    i->setNeedSaveDatabase(true);
                                    Event e(RSensors, RStatePresence, i->id(), item);
                                    enqueueEvent(e);
                                    enqueueEvent(Event(RSensors, RStateLastUpdated, i->id()));

                                    // prepare to automatically set presence to false
                                    if (item->toBool())
                                    {
                                        if (val.clusterId == event.clusterId() && val.maxInterval > 0 &&
                                            updateType == NodeValue::UpdateByZclReport)
                                        {
                                            // prevent setting presence back to false, when report.maxInterval > config.duration
                                            i->durationDue = item->lastSet().addSecs(val.maxInterval);
                                        }
                                        else
                                        {
                                            ResourceItem *item2 = i->item(RConfigDuration);
                                            if (item2 && item2->toNumber() > 0)
                                            {
                                                i->durationDue = item->lastSet().addSecs(item2->toNumber());
                                            }
                                        }
                                    }
                                }
                                updateSensorEtag(&*i);

                            }
                        }
                    }
                    else if (event.clusterId() == METERING_CLUSTER_ID)
                    {
                        bool updated = false;
                        for (;ia != enda; ++ia)
                        {
                            if (std::find(event.attributeIds().begin(),
                                          event.attributeIds().end(),
                                          ia->id()) == event.attributeIds().end())
                            {
                                continue;
                            }

                            if (ia->id() == 0x0000) // Current Summation Delivered
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.clusterId(), ia->id(), ia->numericValue());
                                    pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().u64);
                                }

                                quint64 consumption = ia->numericValue().u64;
                                ResourceItem *item = i->item(RStateConsumption);

                                if (i->modelId() == QLatin1String("SmartPlug")) // Heiman
                                {
                                    consumption += 5; consumption /= 10; // 0.1 Wh -> Wh
                                }
                                else if (i->modelId() == QLatin1String("SP 120")) // innr
                                {
                                    consumption *= 10; // 0.01 kWh = 10 Wh -> Wh
                                }

                                if (item)
                                {
                                    item->setValue(consumption); // in Wh (0.001 kWh)
                                    enqueueEvent(Event(RSensors, RStateConsumption, i->id(), item));
                                    updated = true;
                                }
                            }
                            else if (ia->id() == 0x0400) // Instantaneous Demand
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.clusterId(), ia->id(), ia->numericValue());
                                    pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().s32);
                                }

                                qint32 power = ia->numericValue().s32;
                                ResourceItem *item = i->item(RStatePower);

                                if (i->modelId() == QLatin1String("SmartPlug") || // Heiman
                                    i->modelId() == QLatin1String("902010/25")) // Bitron
                                {
                                    power += 5; power /= 10; // 0.1 W -> W
                                }

                                if (item)
                                {
                                    item->setValue((qint16) power); // in W
                                    enqueueEvent(Event(RSensors, RStatePower, i->id(), item));
                                    updated = true;
                                }
                            }
                        }
                        if (updated)
                        {
                            i->updateStateTimestamp();
                            i->setNeedSaveDatabase(true);
                            enqueueEvent(Event(RSensors, RStateLastUpdated, i->id()));
                            updateSensorEtag(&*i);
                        }
                    }
                    else if (event.clusterId() == ELECTRICAL_MEASUREMENT_CLUSTER_ID)
                    {
                        bool updated = false;
                        for (;ia != enda; ++ia)
                        {
                            if (std::find(event.attributeIds().begin(),
                                          event.attributeIds().end(),
                                          ia->id()) == event.attributeIds().end())
                            {
                                continue;
                            }

                            if (ia->id() == 0x050B) // Active power
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.clusterId(), ia->id(), ia->numericValue());
                                    pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().s16);
                                }

                                qint16 power = ia->numericValue().s16;
                                ResourceItem *item = i->item(RStatePower);

                                if (item && power != -32768)
                                {
                                    if (i->modelId() == QLatin1String("SmartPlug")) // Heiman
                                    {
                                        power += 5; power /= 10; // 0.1W -> W
                                    }
                                    else if (i->modelId().startsWith(QLatin1String("Plug"))) // OSRAM
                                    {
                                        power = power == 28000 ? 0 : power / 10;
                                    }
                                    item->setValue(power); // in W
                                    enqueueEvent(Event(RSensors, RStatePower, i->id(), item));
                                    updated = true;
                                }
                            }
                            else if (ia->id() == 0x0505) // RMS Voltage
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.clusterId(), ia->id(), ia->numericValue());
                                    pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().u16);
                                }

                                quint16 voltage = ia->numericValue().u16;
                                ResourceItem *item = i->item(RStateVoltage);

                                if (item && voltage != 65535)
                                {
                                    if (i->modelId() == QLatin1String("SmartPlug")) // Heiman
                                    {
                                        voltage += 50; voltage /= 100; // 0.01V -> V
                                    }
                                    item->setValue(voltage); // in V
                                    enqueueEvent(Event(RSensors, RStateVoltage, i->id(), item));
                                    updated = true;
                                }
                            }
                            else if (ia->id() == 0x0508) // RMS Current
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.clusterId(), ia->id(), ia->numericValue());
                                    pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().u16);
                                }

                                quint16 current = ia->numericValue().u16;
                                ResourceItem *item = i->item(RStateCurrent);

                                if (item && current != 65535)
                                {
                                    if (i->modelId() == QLatin1String("SP 120")) // innr
                                    {
                                        // already in mA
                                    }
                                    else if (i->modelId() == QLatin1String("SmartPlug")) // Heiman
                                    {
                                        current *= 100; // 0.01A -> mA
                                    }
                                    else
                                    {
                                        current *= 1000; // A -> mA
                                    }
                                    item->setValue(current); // in mA
                                    enqueueEvent(Event(RSensors, RStateCurrent, i->id(), item));
                                    updated = true;
                                }
                            }
                        }
                        if (updated)
                        {
                            i->updateStateTimestamp();
                            i->setNeedSaveDatabase(true);
                            enqueueEvent(Event(RSensors, RStateLastUpdated, i->id()));
                            updateSensorEtag(&*i);
                        }
                    }
                    else if (event.clusterId() == UBISYS_DEVICE_SETUP_CLUSTER_ID && event.endpoint() == 0xE8 &&
                             (event.node()->address().ext() & macPrefixMask) == ubisysMacPrefix) // ubisys device management
                    {
//                        bool updated = false;
                        for (;ia != enda; ++ia)
                        {
                            if (std::find(event.attributeIds().begin(),
                                          event.attributeIds().end(),
                                          ia->id()) == event.attributeIds().end())
                            {
                                continue;
                            }

                            if (ia->id() == 0x0000 && ia->dataType() == deCONZ::ZclArray) // Input configurations
                            {
                                QByteArray arr = ia->toVariant().toByteArray();
                                qDebug() << arr.toHex();
                            }
                            else if (ia->id() == 0x0001 && ia->dataType() == deCONZ::ZclArray) // Input actions
                            {
                                QByteArray arr = ia->toVariant().toByteArray();
                                qDebug() << arr.toHex();
                            }

                            if (i->modelId().startsWith(QLatin1String("C4")))
                            {
                                processUbisysC4Configuration(&*i);
                            }
                        }
                    }
                }
            }
        }

        if (i->needSaveDatabase())
        {
            saveDatabaseItems |= DB_SENSORS;
        }
    }
}

/*! Returns true if the device is supported.
 */
bool DeRestPluginPrivate::isDeviceSupported(const deCONZ::Node *node, const QString &modelId)
{
    if (!node || modelId.isEmpty())
    {
        return false;
    }

    const SupportedDevice *s = supportedDevices;
    while (s->modelId)
    {
        if ((!node->nodeDescriptor().isNull() && node->nodeDescriptor().manufacturerCode() == s->vendorId) ||
            ((node->address().ext() & macPrefixMask) == s->mac))
        {
            if (modelId.startsWith(QLatin1String(s->modelId)))
            {
                return true;
            }
        }
        s++;
    }

    return false;
}

/*! Returns the first Sensor for its given \p id or 0 if not found.
    \note There might be more sensors with the same extAddr.
 */
Sensor *DeRestPluginPrivate::getSensorNodeForAddress(quint64 extAddr)
{
    std::vector<Sensor>::iterator i = sensors.begin();
    std::vector<Sensor>::iterator end = sensors.end();

    for (; i != end; ++i)
    {
        if (i->address().ext() == extAddr && i->deletedState() != Sensor::StateDeleted)
        {
            return &(*i);
        }
    }

    end = sensors.end();

    for (i = sensors.begin(); i != end; ++i)
    {
        if (i->address().ext() == extAddr)
        {
            return &(*i);
        }
    }

    return 0;

}

/*! Returns the first Sensor for its given \p addr or 0 if not found.
    \note There might be more sensors with the same address.
 */
Sensor *DeRestPluginPrivate::getSensorNodeForAddress(const deCONZ::Address &addr)
{
    std::vector<Sensor>::iterator i = sensors.begin();
    std::vector<Sensor>::iterator end = sensors.end();

    if (addr.hasExt())
    {
        for (; i != end; ++i)
        {
            if (i->address().ext() == addr.ext() && i->deletedState() != Sensor::StateDeleted)
            {
                return &(*i);
            }
        }

        for (i = sensors.begin(); i != end; ++i)
        {
            if (i->address().ext() == addr.ext())
            {
                return &(*i);
            }
        }
    }
    else if (addr.hasNwk())
    {
        for (; i != end; ++i)
        {
            if (i->address().nwk() == addr.nwk() && i->deletedState() != Sensor::StateDeleted)
            {
                return &(*i);
            }
        }

        for (i = sensors.begin(); i != end; ++i)
        {
            if (i->address().nwk() == addr.nwk())
            {
                return &(*i);
            }
        }
    }

    return 0;
}

/*! Returns the first Sensor for its given \p Address and \p Endpoint or 0 if not found.
 */
Sensor *DeRestPluginPrivate::getSensorNodeForAddressAndEndpoint(const deCONZ::Address &addr, quint8 ep)
{
    std::vector<Sensor>::iterator i = sensors.begin();
    std::vector<Sensor>::iterator end = sensors.end();

    if (addr.hasExt())
    {
        for (; i != end; ++i)
        {
            if (i->address().ext() == addr.ext() && ep == i->fingerPrint().endpoint && i->deletedState() != Sensor::StateDeleted)
            {
                return &(*i);
            }
        }
    }
    else if (addr.hasNwk())
    {
        for (i = sensors.begin(); i != end; ++i)
        {
            if (i->address().nwk() == addr.nwk() && ep == i->fingerPrint().endpoint && i->deletedState() != Sensor::StateDeleted)
            {
                return &(*i);
            }
        }
    }

    return 0;

}

/*! Returns the first Sensor which matches a fingerprint.
    \note There might be more sensors with the same fingerprint.
 */
Sensor *DeRestPluginPrivate::getSensorNodeForFingerPrint(quint64 extAddr, const SensorFingerprint &fingerPrint, const QString &type)
{
    std::vector<Sensor>::iterator i = sensors.begin();
    std::vector<Sensor>::iterator end = sensors.end();

    for (; i != end; ++i)
    {
        if (i->address().ext() == extAddr && i->deletedState() != Sensor::StateDeleted)
        {
            if (i->type() == type && i->fingerPrint().endpoint == fingerPrint.endpoint)
            {
                if (!(i->fingerPrint() == fingerPrint))
                {
                    DBG_Printf(DBG_INFO, "updated fingerprint for sensor %s\n", qPrintable(i->name()));
                    i->fingerPrint() = fingerPrint;
                    i->setNeedSaveDatabase(true);
                    updateEtag(i->etag);
                    queSaveDb(DB_SENSORS , DB_SHORT_SAVE_DELAY);
                }
                return &(*i);
            }
        }
    }

    end = sensors.end();

    for (i = sensors.begin(); i != end; ++i)
    {
        if (i->address().ext() == extAddr)
        {
            if (i->type() == type && i->fingerPrint().endpoint == fingerPrint.endpoint)
            {
                if (!(i->fingerPrint() == fingerPrint))
                {
                    DBG_Printf(DBG_INFO, "updated fingerprint for sensor %s\n", qPrintable(i->name()));
                    i->fingerPrint() = fingerPrint;
                    i->setNeedSaveDatabase(true);
                    updateEtag(i->etag);
                    queSaveDb(DB_SENSORS , DB_SHORT_SAVE_DELAY);
                }
                return &(*i);
            }
        }
    }

    return 0;
}

/*! Returns a Sensor for its given \p unique id or 0 if not found.
 */
Sensor *DeRestPluginPrivate::getSensorNodeForUniqueId(const QString &uniqueId)
{
    std::vector<Sensor>::iterator i;
    std::vector<Sensor>::iterator end = sensors.end();

    for (i = sensors.begin(); i != end; ++i)
    {
        if (i->uniqueId() == uniqueId)
        {
            return &(*i);
        }
    }

    return 0;
}

/*! Returns a Sensor for its given \p id or 0 if not found.
 */
Sensor *DeRestPluginPrivate::getSensorNodeForId(const QString &id)
{
    std::vector<Sensor>::iterator i;
    std::vector<Sensor>::iterator end = sensors.end();

    for (i = sensors.begin(); i != end; ++i)
    {
        if (i->id() == id)
        {
            return &(*i);
        }
    }

    return 0;
}

/*! Returns a Group for a given group id or 0 if not found.
 */
Group *DeRestPluginPrivate::getGroupForId(uint16_t id)
{
    std::vector<Group>::iterator i = groups.begin();
    std::vector<Group>::iterator end = groups.end();

    for (; i != end; ++i)
    {
        if (i->address() == id)
        {
            return &(*i);
        }
    }

    return 0;
}

/*! Returns a Scene for a given group id and Scene id or 0 if not found.
 */
Scene *DeRestPluginPrivate::getSceneForId(uint16_t gid, uint8_t sid)
{
    Group *group = getGroupForId(gid);

    if (group)
    {
        std::vector<Scene>::iterator i = group->scenes.begin();
        std::vector<Scene>::iterator end = group->scenes.end();

        for (; i != end; ++i)
        {
            if (i->id == sid)
            {
                return &(*i);
            }
        }
    }

    return 0;
}

/*! Returns a Group for a given group name or 0 if not found.
 */
Group *DeRestPluginPrivate::getGroupForName(const QString &name)
{
    DBG_Assert(name.isEmpty() == false);
    if (name.isEmpty())
    {
        return 0;
    }

    std::vector<Group>::iterator i = groups.begin();
    std::vector<Group>::iterator end = groups.end();

    for (; i != end; ++i)
    {
        if (i->name() == name)
        {
            return &(*i);
        }
    }

    return 0;
}

/*! Returns a Group for a given group id or 0 if not found.
 */
Group *DeRestPluginPrivate::getGroupForId(const QString &id)
{
    DBG_Assert(id.isEmpty() == false);
    if (id.isEmpty())
    {
        return 0;
    }

    // check valid 16-bit group id 0..0xFFFF
    bool ok;
    uint gid = id.toUInt(&ok, 10);
    if (!ok || (gid > 0xFFFFUL))
    {
        DBG_Printf(DBG_INFO, "Get group for id error: invalid group id %s\n", qPrintable(id));
        return 0;
    }

    std::vector<Group>::iterator i = groups.begin();
    std::vector<Group>::iterator end = groups.end();

    for (; i != end; ++i)
    {
        if (i->id() == id)
        {
            return &(*i);
        }
    }

    return 0;
}

/*! Delete a group of a switch from database permanently.
 */
bool DeRestPluginPrivate::deleteOldGroupOfSwitch(Sensor *sensor, quint16 newGroupId)
{
    DBG_Assert(sensor && !sensor->id().isEmpty());
    if (!sensor || sensor->id().isEmpty())
    {
        return false;
    }

    std::vector<Group>::iterator i = groups.begin();
    std::vector<Group>::iterator end = groups.end();

    for (; i != end; ++i)
    {
        if (i->address() == newGroupId)
        {
            continue;
        }

        if (i->state() != Group::StateNormal)
        {
            continue;
        }

        if (i->m_deviceMemberships.end() != std::find(i->m_deviceMemberships.begin(),
                                                      i->m_deviceMemberships.end(),
                                                      sensor->id()))
        {
            DBG_Printf(DBG_INFO, "delete old switch group 0x%04X of sensor %s\n", i->address(), qPrintable(sensor->name()));
            //found
            i->setState(Group::StateDeleted);
        }
    }
    return true;
}


/*! Returns GroupInfo in a LightNode for a given group id or 0 if not found.
 */
GroupInfo *DeRestPluginPrivate::getGroupInfo(LightNode *lightNode, uint16_t id)
{
    DBG_Assert(lightNode != 0);

    if (lightNode)
    {
        std::vector<GroupInfo>::iterator i = lightNode->groups().begin();
        std::vector<GroupInfo>::iterator end = lightNode->groups().end();

        for (; i != end; ++i)
        {
            if (i->id == id)
            {
                return &(*i);
            }
        }
    }

    return 0;
}

GroupInfo *DeRestPluginPrivate::createGroupInfo(LightNode *lightNode, uint16_t id)
{
    DBG_Assert(lightNode != 0);

    // dont create a duplicate
    GroupInfo *g = getGroupInfo(lightNode, id);
    if (g)
    {
        return g;
    }

    // not found .. create
    GroupInfo groupInfo;
    groupInfo.id = id;
    lightNode->groups().push_back(groupInfo);

    return &lightNode->groups().back();
}

/*! Returns a deCONZ::Node for a given MAC address or 0 if not found.
 */
deCONZ::Node *DeRestPluginPrivate::getNodeForAddress(uint64_t extAddr)
{
    int i = 0;
    const deCONZ::Node *node;

    DBG_Assert(apsCtrl != 0);

    if (apsCtrl == 0)
    {
        return 0;
    }

    while (apsCtrl->getNode(i, &node) == 0)
    {
        if (node->address().ext() == extAddr)
        {
            return const_cast<deCONZ::Node*>(node); // FIXME: use const
        }
        i++;
    }

    return 0;
}

/*! Returns the cluster descriptor for given cluster id.
    \return the cluster or 0 if not found
 */
deCONZ::ZclCluster *DeRestPluginPrivate::getInCluster(deCONZ::Node *node, uint8_t endpoint, uint16_t clusterId)
{
    if (DBG_Assert(node != 0) == false)
    {
        return 0;
    }

    deCONZ::SimpleDescriptor *sd = node->getSimpleDescriptor(endpoint);

    if (sd)
    {
        QList<deCONZ::ZclCluster>::iterator i = sd->inClusters().begin();
        QList<deCONZ::ZclCluster>::iterator end = sd->inClusters().end();

        for (; i != end; ++i)
        {
            if (i->id() == clusterId)
            {
                return &(*i);
            }
        }
    }

    return 0;
}

/*! Get proper src endpoint for outgoing requests.
    \param req - the profileId() must be specified in the request.
    \return a endpoint number
 */
uint8_t DeRestPluginPrivate::getSrcEndpoint(RestNodeBase *restNode, const deCONZ::ApsDataRequest &req)
{
    Q_UNUSED(restNode);
    if (req.profileId() == HA_PROFILE_ID || req.profileId() == ZLL_PROFILE_ID)
    {
        return endpoint();
    }
    return 0x01;
}

/*! Check and process queued attributes marked for read.
    \return true - if at least one attribute was processed
 */
bool DeRestPluginPrivate::processZclAttributes(LightNode *lightNode)
{
    DBG_Assert(lightNode != 0);

    if (!lightNode)
    {
        return false;
    }

    // check if read should happen now
//    if (lightNode->nextReadTime() > QTime::currentTime())
//    {
//        return false;
//    }

    if (!lightNode->isAvailable() || !lightNode->lastRx().isValid())
    {
        return false;
    }

    deCONZ::ApsController *apsCtrl = deCONZ::ApsController::instance();
    DBG_Assert(apsCtrl != 0);
    if (apsCtrl && (apsCtrl->getParameter(deCONZ::ParamAutoPollingActive) == 0))
    {
        return false;
    }

    int processed = 0;

    if (lightNode->haEndpoint().profileId() == ZLL_PROFILE_ID)
    {
        switch(lightNode->haEndpoint().deviceId())
        {
        case DEV_ID_ZLL_COLOR_LIGHT:
        case DEV_ID_ZLL_EXTENDED_COLOR_LIGHT:
        case DEV_ID_Z30_EXTENDED_COLOR_LIGHT:
        case DEV_ID_ZLL_COLOR_TEMPERATURE_LIGHT:
        case DEV_ID_Z30_COLOR_TEMPERATURE_LIGHT:
            //fall through

        case DEV_ID_ZLL_DIMMABLE_LIGHT:
        case DEV_ID_ZLL_DIMMABLE_PLUGIN_UNIT:
        case DEV_ID_Z30_DIMMABLE_PLUGIN_UNIT:
            //fall through

        case DEV_ID_ZLL_ONOFF_LIGHT:
        case DEV_ID_ZLL_ONOFF_PLUGIN_UNIT:
        case DEV_ID_Z30_ONOFF_PLUGIN_UNIT:
        case DEV_ID_ZLL_ONOFF_SENSOR:
            //readOnOff = true;
            break;

        default:
            break;
        }
    }
    else if (lightNode->haEndpoint().profileId() == HA_PROFILE_ID)
    {
        switch(lightNode->haEndpoint().deviceId())
        {
        case DEV_ID_HA_COLOR_DIMMABLE_LIGHT:
        case DEV_ID_ZLL_COLOR_LIGHT:
        case DEV_ID_ZLL_EXTENDED_COLOR_LIGHT:
        case DEV_ID_Z30_EXTENDED_COLOR_LIGHT:
        case DEV_ID_ZLL_COLOR_TEMPERATURE_LIGHT:
        case DEV_ID_Z30_COLOR_TEMPERATURE_LIGHT:
            //fall through

        case DEV_ID_HA_DIMMABLE_LIGHT:
        //case DEV_ID_ZLL_DIMMABLE_LIGHT: // same as DEV_ID_HA_ONOFF_LIGHT
        case DEV_ID_ZLL_DIMMABLE_PLUGIN_UNIT:
        case DEV_ID_Z30_DIMMABLE_PLUGIN_UNIT:
            //fall through

        case DEV_ID_MAINS_POWER_OUTLET:
        case DEV_ID_SMART_PLUG:
        case DEV_ID_HA_ONOFF_LIGHT:
        case DEV_ID_ZLL_ONOFF_LIGHT:
        case DEV_ID_ZLL_ONOFF_PLUGIN_UNIT:
        case DEV_ID_Z30_ONOFF_PLUGIN_UNIT:
        case DEV_ID_ZLL_ONOFF_SENSOR:
        case DEV_ID_HA_WINDOW_COVERING_DEVICE:
            break;

        default:
            break;
        }
    }

    QTime tNow = QTime::currentTime();

    if (lightNode->mustRead(READ_BINDING_TABLE) && tNow > lightNode->nextReadTime(READ_BINDING_TABLE))
    {
        if (readBindingTable(lightNode, 0))
        {
            // only read binding table once per node even if multiple devices/sensors are implemented
            std::vector<LightNode>::iterator i = nodes.begin();
            std::vector<LightNode>::iterator end = nodes.end();

            for (; i != end; ++i)
            {
                if (i->address().ext() == lightNode->address().ext())
                {
                    i->clearRead(READ_BINDING_TABLE);
                }
            }
            processed++;
        }
    }

    if (lightNode->mustRead(READ_VENDOR_NAME) && tNow > lightNode->nextReadTime(READ_VENDOR_NAME))
    {
        if (!lightNode->manufacturer().isEmpty() && lightNode->manufacturer() != QLatin1String("Unknown"))
        {
            lightNode->clearRead(READ_VENDOR_NAME);
            processed++;
        }
        else
        {
            std::vector<uint16_t> attributes;
            attributes.push_back(0x0004); // Manufacturer name

            if (readAttributes(lightNode, lightNode->haEndpoint().endpoint(), BASIC_CLUSTER_ID, attributes))
            {
                lightNode->clearRead(READ_VENDOR_NAME);
                processed++;
            }
        }
    }

    if ((processed < 2) && lightNode->mustRead(READ_MODEL_ID) && tNow > lightNode->nextReadTime(READ_MODEL_ID))
    {
        if (!lightNode->modelId().isEmpty())
        {
            lightNode->clearRead(READ_MODEL_ID);
            processed++;
        }
        else
        {
            std::vector<uint16_t> attributes;
            attributes.push_back(0x0005); // Model identifier

            if (readAttributes(lightNode, lightNode->haEndpoint().endpoint(), BASIC_CLUSTER_ID, attributes))
            {
                lightNode->clearRead(READ_MODEL_ID);
                processed++;
            }
        }
    }

    if ((processed < 2) && lightNode->mustRead(READ_GROUPS) && tNow > lightNode->nextReadTime(READ_GROUPS))
    {
        std::vector<uint16_t> groups; // empty meaning read all groups
        if (readGroupMembership(lightNode, groups))
        {
            lightNode->clearRead(READ_GROUPS);
            processed++;
        }
    }

#if 0 // TODO add this to poll manager
      // this is very problematic and causes queues to fill up extremely
    if ((processed < 2) && lightNode->mustRead(READ_SCENES) && !lightNode->groups().empty()&& tNow > lightNode->nextReadTime(READ_SCENES))
    {
        std::vector<GroupInfo>::iterator i = lightNode->groups().begin();
        std::vector<GroupInfo>::iterator end = lightNode->groups().end();

        int rd = 0;

        for (; i != end; ++i)
        {
            Group *group = getGroupForId(i->id);

            if (group && group->state() != Group::StateDeleted && group->state() != Group::StateDeleteFromDB)
            {
                // NOTE: this may cause problems if we have a lot of nodes + groups
                // proposal mark groups for which scenes where discovered
                if (readSceneMembership(lightNode, group))
                {
                    processed++;
                    rd++;
                }
                else
                {
                    // print but don't take action
                    DBG_Printf(DBG_INFO_L2, "read scenes membership for group: 0x%04X rejected\n", i->id);
                }
            }
        }

        if (!lightNode->groups().empty())
        {
            if (rd > 0)
            {
                lightNode->clearRead(READ_SCENES);
            }
        }
        else
        {
            lightNode->clearRead(READ_SCENES);
        }

    }

    if ((processed < 2) && lightNode->mustRead(READ_SCENE_DETAILS) && tNow > lightNode->nextReadTime(READ_SCENE_DETAILS))
    {
        std::vector<GroupInfo>::iterator g = lightNode->groups().begin();
        std::vector<GroupInfo>::iterator gend = lightNode->groups().end();

        int rd = 0;

        for (; g != gend; ++g)
        {
            Group *group = getGroupForId(g->id);

            if (group  && group->state() != Group::StateDeleted && group->state() != Group::StateDeleteFromDB)
            {
                std::vector<Scene>::iterator s = group->scenes.begin();
                std::vector<Scene>::iterator send = group->scenes.end();

                for (; s != send; ++s)
                {
                    if (readSceneAttributes(lightNode, g->id, s->id))
                    {
                        processed++;
                        rd++;
                    }
                    else
                    {
                        // print but don't take action
                        DBG_Printf(DBG_INFO_L2, "read scene Attributes for group: 0x%04X rejected\n", g->id);
                    }
                }
            }
        }

        if (!lightNode->groups().empty())
        {
            if (rd > 0)
            {
                lightNode->clearRead(READ_SCENE_DETAILS);
            }
        }
        else
        {
            lightNode->clearRead(READ_SCENE_DETAILS);
        }

    }
#endif

    return (processed > 0);
}

/*! Check and process queued attributes marked for read and write.
    \return true - if at least one attribute was processed
 */
bool DeRestPluginPrivate::processZclAttributes(Sensor *sensorNode)
{
    int processed = 0;

    DBG_Assert(sensorNode != 0);

    if (!sensorNode)
    {
        return false;
    }

    if (!sensorNode->isAvailable())
    {
        return false;
    }

    if (!sensorNode->type().startsWith('Z')) // CLIP & Daylight sensors
    {
        return false;
    }

    if (!sensorNode->node())
    {
        deCONZ::Node *node = getNodeForAddress(sensorNode->address().ext());
        if (node)
        {
            sensorNode->setNode(node);
            sensorNode->fingerPrint().checkCounter = SENSOR_CHECK_COUNTER_INIT; // force check
        }
    }

    if (sensorNode->node() && sensorNode->node()->simpleDescriptors().isEmpty())
    {
        return false;
    }

//    deCONZ::ApsController *apsCtrl = deCONZ::ApsController::instance();
//    DBG_Assert(apsCtrl != 0);
//    if (apsCtrl && (apsCtrl->getParameter(deCONZ::ParamAutoPollingActive) == 0))
//    {
//        return false;
//    }

    QTime tNow = QTime::currentTime();

    if (sensorNode->mustRead(READ_BINDING_TABLE) && tNow > sensorNode->nextReadTime(READ_BINDING_TABLE))
    {
        bool ok = false;
        // only read binding table of chosen sensors
        // whitelist by Model ID
        if (sensorNode->modelId().startsWith(QLatin1String("FLS-NB")) ||
            sensorNode->modelId().startsWith(QLatin1String("D1")) || sensorNode->modelId().startsWith(QLatin1String("S1")) ||
            sensorNode->modelId().startsWith(QLatin1String("S2")) || sensorNode->manufacturer().startsWith(QLatin1String("BEGA")) ||
            sensorNode->modelId().startsWith(QLatin1String("C4")))
        {
            ok = true;
        }

        if (!ok)
        {
            sensorNode->clearRead(READ_BINDING_TABLE);
        }

        if (ok && readBindingTable(sensorNode, 0))
        {
            // only read binding table once per node even if multiple devices/sensors are implemented
            std::vector<Sensor>::iterator i = sensors.begin();
            std::vector<Sensor>::iterator end = sensors.end();

            for (; i != end; ++i)
            {
                if (i->address().ext() == sensorNode->address().ext())
                {
                    i->clearRead(READ_BINDING_TABLE);
                }
            }
            processed++;
        }
    }

    if (sensorNode->mustRead(READ_VENDOR_NAME) && tNow > sensorNode->nextReadTime(READ_VENDOR_NAME))
    {
        std::vector<uint16_t> attributes;
        attributes.push_back(0x0004); // Manufacturer name

        if (readAttributes(sensorNode, sensorNode->fingerPrint().endpoint, BASIC_CLUSTER_ID, attributes))
        {
            sensorNode->clearRead(READ_VENDOR_NAME);
            processed++;
        }
    }

    if (sensorNode->mustRead(READ_MODEL_ID) && tNow > sensorNode->nextReadTime(READ_MODEL_ID))
    {
        std::vector<uint16_t> attributes;
        attributes.push_back(0x0005); // Model identifier

        if (readAttributes(sensorNode, sensorNode->fingerPrint().endpoint, BASIC_CLUSTER_ID, attributes))
        {
            sensorNode->clearRead(READ_MODEL_ID);
            processed++;
        }
    }

    if (sensorNode->mustRead(READ_SWBUILD_ID) && tNow > sensorNode->nextReadTime(READ_SWBUILD_ID))
    {
        std::vector<uint16_t> attributes;
        attributes.push_back(0x4000); // Software build identifier

        if (readAttributes(sensorNode, sensorNode->fingerPrint().endpoint, BASIC_CLUSTER_ID, attributes))
        {
            sensorNode->clearRead(READ_SWBUILD_ID);
            processed++;
        }
    }


    if (sensorNode->mustRead(READ_OCCUPANCY_CONFIG) && tNow > sensorNode->nextReadTime(READ_OCCUPANCY_CONFIG))
    {
        if (sensorNode->modelId().startsWith(QLatin1String("lumi.sensor_motion")))
        {
            sensorNode->clearRead(READ_OCCUPANCY_CONFIG);
        }
        else
        {
            std::vector<uint16_t> attributes;
            attributes.push_back(0x0010); // occupied to unoccupied delay

            if (readAttributes(sensorNode, sensorNode->fingerPrint().endpoint, OCCUPANCY_SENSING_CLUSTER_ID, attributes))
            {
                sensorNode->clearRead(READ_OCCUPANCY_CONFIG);
                processed++;
            }
        }
    }

    if (sensorNode->mustRead(WRITE_OCCUPANCY_CONFIG) && tNow > sensorNode->nextReadTime(READ_OCCUPANCY_CONFIG))
    {
        // only valid bounds
        int duration = sensorNode->item(RConfigDuration)->toNumber();

        if (duration >= 0 && duration <= 65535)
        {
            // occupied to unoccupied delay
            deCONZ::ZclAttribute attr(0x0010, deCONZ::Zcl16BitUint, "occ", deCONZ::ZclReadWrite, true);
            attr.setValue((quint64)duration);

            if (writeAttribute(sensorNode, sensorNode->fingerPrint().endpoint, OCCUPANCY_SENSING_CLUSTER_ID, attr))
            {
                sensorNode->clearRead(WRITE_OCCUPANCY_CONFIG);
                processed++;
            }
        }
        else
        {
            sensorNode->clearRead(WRITE_OCCUPANCY_CONFIG);
        }
    }

    if (sensorNode->mustRead(WRITE_DELAY) && tNow > sensorNode->nextReadTime(WRITE_DELAY))
    {
        ResourceItem *item = sensorNode->item(RConfigDelay);

        DBG_Printf(DBG_INFO_L2, "handle pending delay for 0x%016llX\n", sensorNode->address().ext());
        if (item)
        {
            quint64 delay = item->toNumber();
            // occupied to unoccupied delay
            deCONZ::ZclAttribute attr(0x0010, deCONZ::Zcl16BitUint, "occ", deCONZ::ZclReadWrite, true);
            attr.setValue(delay);

            if (writeAttribute(sensorNode, sensorNode->fingerPrint().endpoint, OCCUPANCY_SENSING_CLUSTER_ID, attr))
            {
                ResourceItem *item = sensorNode->item(RConfigPending);
                uint8_t mask = item->toNumber();
                mask &= ~R_PENDING_DELAY;
                item->setValue(mask);
                sensorNode->clearRead(WRITE_DELAY);
                processed++;
            }
        }
        else
        {
            sensorNode->clearRead(WRITE_DELAY);
        }
    }

    if (sensorNode->mustRead(WRITE_LEDINDICATION) && tNow > sensorNode->nextReadTime(WRITE_LEDINDICATION))
    {
        ResourceItem *item = sensorNode->item(RConfigLedIndication);

        DBG_Printf(DBG_INFO_L2, "handle pending ledindication for 0x%016llX\n", sensorNode->address().ext());
        if (item)
        {
            bool ledindication = (item->toNumber() != 0);
            // ledindication
            deCONZ::ZclAttribute attr(0x0033, deCONZ::ZclBoolean, "ledindication", deCONZ::ZclReadWrite, true);
            attr.setValue(ledindication);

            if (writeAttribute(sensorNode, sensorNode->fingerPrint().endpoint, BASIC_CLUSTER_ID, attr, VENDOR_PHILIPS))
            {
                ResourceItem *item = sensorNode->item(RConfigPending);
                uint8_t mask = item->toNumber();
                mask &= ~R_PENDING_LEDINDICATION;
                item->setValue(mask);
                sensorNode->clearRead(WRITE_LEDINDICATION);
                processed++;
            }
        }
        else
        {
            sensorNode->clearRead(WRITE_LEDINDICATION);
        }
    }

    if (sensorNode->mustRead(WRITE_SENSITIVITY) && tNow > sensorNode->nextReadTime(WRITE_SENSITIVITY))
    {
        ResourceItem *item = sensorNode->item(RConfigSensitivity);

        DBG_Printf(DBG_INFO_L2, "handle pending sensitivity for 0x%016llX\n", sensorNode->address().ext());
        if (item)
        {
            quint64 sensitivity = item->toNumber();
            // sensitivity
            deCONZ::ZclAttribute attr(0x0030, deCONZ::Zcl8BitUint, "sensitivity", deCONZ::ZclReadWrite, true);
            attr.setValue(sensitivity);

            if (writeAttribute(sensorNode, sensorNode->fingerPrint().endpoint, OCCUPANCY_SENSING_CLUSTER_ID, attr, VENDOR_PHILIPS))
            {
                ResourceItem *item = sensorNode->item(RConfigPending);
                uint8_t mask = item->toNumber();
                mask &= ~R_PENDING_SENSITIVITY;
                item->setValue(mask);
                sensorNode->clearRead(WRITE_SENSITIVITY);
                processed++;
            }
        }
        else
        {
            sensorNode->clearRead(WRITE_SENSITIVITY);
        }
    }

    if (sensorNode->mustRead(WRITE_USERTEST) && tNow > sensorNode->nextReadTime(WRITE_USERTEST))
    {
        ResourceItem *item = sensorNode->item(RConfigUsertest);

        DBG_Printf(DBG_INFO_L2, "handle pending usertest for 0x%016llX\n", sensorNode->address().ext());
        if (item)
        {
            bool usertest = (item->toNumber() != 0);
            // usertest
            deCONZ::ZclAttribute attr(0x0032, deCONZ::ZclBoolean, "usertest", deCONZ::ZclReadWrite, true);
            attr.setValue(usertest);

            if (writeAttribute(sensorNode, sensorNode->fingerPrint().endpoint, BASIC_CLUSTER_ID, attr, VENDOR_PHILIPS))
            {
                ResourceItem *item = sensorNode->item(RConfigPending);
                uint8_t mask = item->toNumber();
                mask &= ~R_PENDING_USERTEST;
                item->setValue(mask);
                sensorNode->clearRead(WRITE_USERTEST);
                processed++;
            }
        }
        else
        {
            sensorNode->clearRead(WRITE_USERTEST);
        }
    }

    if (sensorNode->mustRead(READ_THERMOSTAT_STATE) && tNow > sensorNode->nextReadTime(READ_THERMOSTAT_STATE))
    {
        std::vector<uint16_t> attributes;
        attributes.push_back(0x0000); // temperature
        attributes.push_back(0x0012); // heating setpoint
        attributes.push_back(0x0025); // scheduler state
        attributes.push_back(0x0029); // heating operation state

        if (readAttributes(sensorNode, sensorNode->fingerPrint().endpoint, THERMOSTAT_CLUSTER_ID, attributes))
        {
            sensorNode->clearRead(READ_THERMOSTAT_STATE);
            processed++;
        }
    }

    return (processed > 0);
}

/*! Queue reading ZCL attributes of a node.
    \param restNode the node from which the attributes shall be read
    \param endpoint the destination endpoint
    \param clusterId the cluster id related to the attributes
    \param attributes a list of attribute ids which shall be read
    \param manufacturerCode (optional) manufacturerCode for manufacturer-specific attribute
    \return true if the request is queued
 */
bool DeRestPluginPrivate::readAttributes(RestNodeBase *restNode, quint8 endpoint, uint16_t clusterId, const std::vector<uint16_t> &attributes, uint16_t manufacturerCode)
{
    DBG_Assert(restNode != 0);
    DBG_Assert(!attributes.empty());

    if (!restNode || !restNode->node() || attributes.empty() || !restNode->isAvailable())
    {
        return false;
    }

    if (!restNode->node()->nodeDescriptor().receiverOnWhenIdle())
    {
        QDateTime now = QDateTime::currentDateTime();
        if (!restNode->lastRx().isValid() || (restNode->lastRx().secsTo(now) > 3))
        {
            return false;
        }
    }


    if (taskCountForAddress(restNode->address()) >= MAX_TASKS_PER_NODE)
    {
        return false;
    }

    if ((runningTasks.size() + tasks.size()) > MAX_BACKGROUND_TASKS)
    {
        return false;
    }

    TaskItem task;
    task.taskType = TaskReadAttributes;

//    task.req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
    task.req.setDstEndpoint(endpoint);
    task.req.setDstAddressMode(deCONZ::ApsExtAddress);
    task.req.dstAddress() = restNode->address();
    task.req.setClusterId(clusterId);
    task.req.setProfileId(HA_PROFILE_ID);
    task.req.setSrcEndpoint(getSrcEndpoint(restNode, task.req));

    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(deCONZ::ZclReadAttributesId);

    if (manufacturerCode)
    {
        task.zclFrame.setFrameControl(deCONZ::ZclFCProfileCommand |
                                      deCONZ::ZclFCManufacturerSpecific |
                                      deCONZ::ZclFCDirectionClientToServer |
                                      deCONZ::ZclFCDisableDefaultResponse);
        task.zclFrame.setManufacturerCode(manufacturerCode);
        DBG_Printf(DBG_INFO_L2, "read manufacturer specific attributes of 0x%016llX cluster: 0x%04X: [ ", restNode->address().ext(), clusterId);
    }
    else
    {
        task.zclFrame.setFrameControl(deCONZ::ZclFCProfileCommand |
                                      deCONZ::ZclFCDirectionClientToServer |
                                      deCONZ::ZclFCDisableDefaultResponse);

        DBG_Printf(DBG_INFO_L2, "read attributes of 0x%016llX cluster: 0x%04X: [ ", restNode->address().ext(), clusterId);
    }

    { // payload
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        for (uint i = 0; i < attributes.size(); i++)
        {
            stream << attributes[i];
            DBG_Printf(DBG_INFO_L2, "0x%04X ", attributes[i]);
        }
    }
    DBG_Printf(DBG_INFO_L2, "]\n");

    // check duplicates
    for (const TaskItem &t0 : tasks)
    {
        if (t0.taskType != task.taskType ||
            t0.req.dstAddress() != task.req.dstAddress() ||
            t0.req.clusterId() != task.req.clusterId() ||
            t0.req.dstEndpoint() != task.req.dstEndpoint() ||
            t0.zclFrame.commandId() != task.zclFrame.commandId() ||
            t0.zclFrame.manufacturerCode() != task.zclFrame.manufacturerCode())
        {
            continue;
        }

        if (t0.zclFrame.payload() == task.zclFrame.payload())
        {
            DBG_Printf(DBG_INFO, "discard read attributes of 0x%016llX cluster: 0x%04X (already in queue)\n", restNode->address().ext(), clusterId);
            return false;
        }
    }

    { // ZCL frame
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    return addTask(task);
}

/*! Queue reading Group Identifiers.
    \param node the node from which the group identifiers shall be read
    \param startIndex the index to start the reading
    \return true if the request is queued
 */
bool DeRestPluginPrivate::getGroupIdentifiers(RestNodeBase *node, quint8 endpoint, quint8 startIndex)
{
    DBG_Assert(node != 0);

    if (!node || !node->isAvailable())
    {
        return false;
    }

    TaskItem task;
    task.taskType = TaskGetGroupIdentifiers;

    task.req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
    task.req.setDstEndpoint(endpoint);
    task.req.setDstAddressMode(deCONZ::ApsExtAddress);
    task.req.dstAddress() = node->address();
    task.req.setClusterId(COMMISSIONING_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID); // utility commands (ref.: zll spec. 7.1.1)
    task.req.setSrcEndpoint(getSrcEndpoint(node, task.req));

    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(0x41); // get group identifiers cmd
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCDisableDefaultResponse);

    { // payload
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        stream << startIndex;
    }

    { // ZCL frame
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    DBG_Printf(DBG_INFO, "Send get group identifiers for node 0%04X \n", node->address().ext());

    return addTask(task);
}

/*! Queue writing a ZCL attribute of a node.
    \param restNode the node from which the attributes shall be read
    \param endpoint the destination endpoint
    \param clusterId the cluster id related to the attributes
    \param attribute the attribute to write
    \param manufacturerCode (optional) manufacturerCode for manufacturer-specific attribute
    \return true if the request is queued
 */
bool DeRestPluginPrivate::writeAttribute(RestNodeBase *restNode, quint8 endpoint, uint16_t clusterId, const deCONZ::ZclAttribute &attribute, uint16_t manufacturerCode)
{
    DBG_Assert(restNode != 0);

    if (!restNode || !restNode->isAvailable())
    {
        return false;
    }

    TaskItem task;
    task.taskType = TaskWriteAttribute;

    task.req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
    task.req.setDstEndpoint(endpoint);
    task.req.setDstAddressMode(deCONZ::ApsExtAddress);
    task.req.dstAddress() = restNode->address();
    task.req.setClusterId(clusterId);
    task.req.setProfileId(HA_PROFILE_ID);
    task.req.setSrcEndpoint(getSrcEndpoint(restNode, task.req));

    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(deCONZ::ZclWriteAttributesId);

    if (manufacturerCode)
    {
        task.zclFrame.setFrameControl(deCONZ::ZclFCProfileCommand |
                                      deCONZ::ZclFCManufacturerSpecific |
                                      deCONZ::ZclFCDirectionClientToServer |
                                      deCONZ::ZclFCDisableDefaultResponse);
        task.zclFrame.setManufacturerCode(manufacturerCode);
        DBG_Printf(DBG_INFO_L2, "write manufacturer specific attribute of 0x%016llX cluster: 0x%04X: 0x%04X\n", restNode->address().ext(), clusterId, attribute.id());
    }
    else
    {
        task.zclFrame.setFrameControl(deCONZ::ZclFCProfileCommand |
                                      deCONZ::ZclFCDirectionClientToServer |
                                      deCONZ::ZclFCDisableDefaultResponse);

        DBG_Printf(DBG_INFO_L2, "write attribute of 0x%016llX cluster: 0x%04X: 0x%04X\n", restNode->address().ext(), clusterId, attribute.id());
    }

    { // payload
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        stream << attribute.id();
        stream << attribute.dataType();

        if (!attribute.writeToStream(stream))
        {
            return false;
        }
    }

    // check duplicates
    for (const TaskItem &t0 : tasks)
    {
        if (t0.taskType != task.taskType ||
            t0.req.dstAddress() != task.req.dstAddress() ||
            t0.req.clusterId() != task.req.clusterId() ||
            t0.req.dstEndpoint() != task.req.dstEndpoint() ||
            t0.zclFrame.commandId() != task.zclFrame.commandId() ||
            t0.zclFrame.manufacturerCode() != task.zclFrame.manufacturerCode())
        {
            continue;
        }

        if (t0.zclFrame.payload() == task.zclFrame.payload())
        {
            DBG_Printf(DBG_INFO, "discard write attribute of 0x%016llX cluster: 0x%04X: 0x%04X (already in queue)\n", restNode->address().ext(), clusterId, attribute.id());
            return false;
        }
    }


    { // ZCL frame
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    return addTask(task);
}

/*! Queue reading details of a scene from a node.
    \param restNode the node from which the scene details shall be read
    \param groupId the group Id of the scene
    \param sceneId the scene Id
    \return true if the request is queued
 */
bool DeRestPluginPrivate::readSceneAttributes(LightNode *lightNode, uint16_t groupId, uint8_t sceneId )
{
    DBG_Assert(lightNode != 0);

    if (!lightNode || !lightNode->isAvailable())
    {
        return false;
    }

    TaskItem task;
    task.taskType = TaskViewScene;
    task.lightNode = lightNode;

    task.req.setSendDelay(3); // delay a bit to let store scene finish
//    task.req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
    task.req.setDstEndpoint(lightNode->haEndpoint().endpoint());
    task.req.setDstAddressMode(deCONZ::ApsExtAddress);
    task.req.dstAddress() = lightNode->address();
    task.req.setClusterId(SCENE_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);
    task.req.setSrcEndpoint(getSrcEndpoint(lightNode, task.req));

    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(0x41); // Enhanced view scene
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCDisableDefaultResponse);

    { // payload
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        stream << groupId;
        stream << sceneId;
    }

    { // ZCL frame
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    return addTask(task);
}

/*! Get group membership of a node.
    \param lightNode the node from which the groups shall be discovered
    \param groups - 0 or more group ids
 */
bool DeRestPluginPrivate::readGroupMembership(LightNode *lightNode, const std::vector<uint16_t> &groups)
{
    DBG_Assert(lightNode != 0);

    if (!lightNode || !lightNode->isAvailable() || !lightNode->lastRx().isValid())
    {
        return false;
    }

    TaskItem task;
    task.taskType = TaskGetGroupMembership;

//    task.req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
    task.req.setDstEndpoint(lightNode->haEndpoint().endpoint());
    task.req.setDstAddressMode(deCONZ::ApsExtAddress);
    task.req.dstAddress() = lightNode->address();
    task.req.setClusterId(GROUP_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);
    task.req.setSrcEndpoint(getSrcEndpoint(lightNode, task.req));

    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(0x02); // get group membership
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCDisableDefaultResponse);

    { // payload
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        stream << (uint8_t)groups.size();

        for (uint i = 0; i < groups.size(); i++)
        {
            stream << groups[i];
        }
    }

    { // ZCL frame
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    return addTask(task);
}

/*! Checks if a group membership is already known.
    If not the group will be added and node gets marked for update.
 */
void DeRestPluginPrivate::foundGroupMembership(LightNode *lightNode, uint16_t groupId)
{
    DBG_Assert(lightNode != 0);

    if (!lightNode)
    {
        return;
    }

    Group *group = getGroupForId(groupId);

    // check if the group is known in the node
    std::vector<GroupInfo>::iterator i = lightNode->groups().begin();
    std::vector<GroupInfo>::iterator end = lightNode->groups().end();

    for (; i != end; ++i)
    {
        if (i->id == groupId)
        {
            if (group && group->state() != Group::StateNormal && group->m_deviceMemberships.size() == 0) // don't touch group of switch
            {
                i->actions &= ~GroupInfo::ActionAddToGroup; // sanity
                i->actions |= GroupInfo::ActionRemoveFromGroup;
                if (i->state != GroupInfo::StateNotInGroup)
                {
                    i->state = GroupInfo::StateNotInGroup;
                    lightNode->setNeedSaveDatabase(true);
                    queSaveDb(DB_LIGHTS, DB_SHORT_SAVE_DELAY);
                }
            }

            return; // ok already known
        }
    }

    updateLightEtag(lightNode);

    GroupInfo groupInfo;
    groupInfo.id = groupId;

    if (group)
    {
        updateGroupEtag(group);

        if (group->state() != Group::StateNormal && group->m_deviceMemberships.size() == 0) // don't touch group of switch
        {
            groupInfo.actions &= ~GroupInfo::ActionAddToGroup; // sanity
            groupInfo.actions |= GroupInfo::ActionRemoveFromGroup;
            groupInfo.state = GroupInfo::StateNotInGroup;
        }
        else
        {
            lightNode->enableRead(READ_SCENES); // force reading of scene membership
        }
    }

    queSaveDb(DB_LIGHTS, DB_SHORT_SAVE_DELAY);
    lightNode->setNeedSaveDatabase(true);
    lightNode->groups().push_back(groupInfo);
}

/*! Checks if the group is known in the global cache.
    If not it will be added.
 */
void DeRestPluginPrivate::foundGroup(uint16_t groupId)
{
    // check if group is known global
    std::vector<Group>::iterator i = groups.begin();
    std::vector<Group>::iterator end = groups.end();

    for (; i != end; ++i)
    {
        if (i->address() == groupId)
        {
            return; // ok already known
        }
    }

    Group group;
    group.setAddress(groupId);
    group.colorX = 0;
    group.colorY = 0;
    group.setIsOn(false);
    group.level = 128;
    group.hue = 0;
    group.hueReal = 0.0f;
    group.sat = 128;
    group.setName(QString());
    updateEtag(group.etag);
    openDb();
    loadGroupFromDb(&group);
    closeDb();
    if (group.name().isEmpty()) {
        group.setName(QString("Group %1").arg(group.id()));
        queSaveDb(DB_GROUPS, DB_SHORT_SAVE_DELAY);
    }
    groups.push_back(group);
    updateEtag(gwConfigEtag);
}

/*! Returns true if the \p lightNode is member of the group with the \p groupId.
 */
bool DeRestPluginPrivate::isLightNodeInGroup(const LightNode *lightNode, uint16_t groupId) const
{
    DBG_Assert(lightNode != 0);

    if (lightNode)
    {
        if (groupId == 0)
        {
            return true; // global group
        }
        std::vector<GroupInfo>::const_iterator i = lightNode->groups().begin();
        std::vector<GroupInfo>::const_iterator end = lightNode->groups().end();

        for (; i != end; ++i)
        {
            if (i->id == groupId && i->state == GroupInfo::StateInGroup)
            {
                return true;
            }
        }
    }

    return false;
}

/*! Delete the light with the \p lightId from all Scenes of the Group with the given \p groupId.
    Also remove these scenes from the Device.
 */
void DeRestPluginPrivate::deleteLightFromScenes(QString lightId, uint16_t groupId)
{
    Group *group = getGroupForId(groupId);
    LightNode *lightNode = getLightNodeForId(lightId);

    if (group)
    {
        std::vector<Scene>::iterator i = group->scenes.begin();
        std::vector<Scene>::iterator end = group->scenes.end();

        for (; i != end; ++i)
        {
            i->deleteLight(lightId);

            // send remove scene request to lightNode
            if (isLightNodeInGroup(lightNode, group->address()))
            {
                GroupInfo *groupInfo = getGroupInfo(lightNode, group->address());

                std::vector<uint8_t> &v = groupInfo->removeScenes;

                if (std::find(v.begin(), v.end(), i->id) == v.end())
                {
                    groupInfo->removeScenes.push_back(i->id);
                }
            }
        }
    }
}

#if 0
/*! Force reading attributes of all nodes in a group.
 */
void DeRestPluginPrivate::readAllInGroup(Group *group)
{
    DBG_Assert(group != 0);

    if (!group)
    {
        return;
    }

    QTime t = QTime::currentTime().addMSecs(ReadAttributesLongerDelay);
    std::vector<LightNode>::iterator i = nodes.begin();
    std::vector<LightNode>::iterator end = nodes.end();

    for (; i != end; ++i)
    {
        LightNode *lightNode = &(*i);
        if (isLightNodeInGroup(lightNode, group->address()))
        {
            // force reading attributes

            const NodeValue &onOff = lightNode->getZclValue(ONOFF_CLUSTER_ID, 0x0000);
            const NodeValue &level = lightNode->getZclValue(LEVEL_CLUSTER_ID, 0x0000);

            if (onOff.updateType != NodeValue::UpdateByZclReport)
            {
                lightNode->setNextReadTime(READ_ON_OFF, t);
                lightNode->enableRead(READ_ON_OFF);
            }

            if (level.updateType != NodeValue::UpdateByZclReport)
            {
                lightNode->setNextReadTime(READ_LEVEL, t);
                lightNode->enableRead(READ_LEVEL);
            }

            if (lightNode->hasColor())
            {
                lightNode->setNextReadTime(READ_COLOR, t);
                lightNode->enableRead(READ_COLOR);
            }
        }
    }
}
#endif

/*! Set on/off attribute for all nodes in a group.
 */
void DeRestPluginPrivate::setAttributeOnOffGroup(Group *group, uint8_t onOff)
{
    DBG_Assert(group != 0);

    if (!group)
    {
        return;
    }

    bool changed = false;
    bool on = (onOff == 0x01);
    if (on != group->isOn())
    {
        group->setIsOn(on);
        updateGroupEtag(group);
        changed = true;
    }

    std::vector<LightNode>::iterator i = nodes.begin();
    std::vector<LightNode>::iterator end = nodes.end();

    for (; i != end; ++i)
    {
        LightNode *lightNode = &*i;
        if (isLightNodeInGroup(lightNode, group->address()))
        {
            ResourceItem *item = lightNode->item(RStateOn);
            if (item->toBool() != on)
            {
                item->setValue(on);
                Event e(RLights, RStateOn, lightNode->id(), item);
                enqueueEvent(e);
                updateLightEtag(lightNode);
            }
            setAttributeOnOff(lightNode);
        }
    }

    if (changed)
    {
        updateEtag(gwConfigEtag);
    }
}

/*! Get scene membership of a node for a group.
    \param group - the group of interrest
 */
bool DeRestPluginPrivate::readSceneMembership(LightNode *lightNode, Group *group)
{
    DBG_Assert(lightNode != 0);
    DBG_Assert(group != 0);

    if (!lightNode || !group || !lightNode->isAvailable())
    {
        return false;
    }

    TaskItem task;
    task.taskType = TaskGetSceneMembership;

//    task.req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
    task.req.setDstEndpoint(lightNode->haEndpoint().endpoint());
    task.req.setDstAddressMode(deCONZ::ApsExtAddress);
    task.req.dstAddress() = lightNode->address();
    task.req.setClusterId(SCENE_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);
    task.req.setSrcEndpoint(getSrcEndpoint(lightNode, task.req));

    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(0x06); // get scene membership
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCDisableDefaultResponse);

    { // payload
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        stream << group->address();
    }

    { // ZCL frame
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    return addTask(task);
}

/*! Checks if the scene membership is known to the group.
    If the scene is not known it will be added.
 */
void DeRestPluginPrivate::foundScene(LightNode *lightNode, Group *group, uint8_t sceneId)
{
    DBG_Assert(group != 0);

    if (!group)
    {
        return;
    }

    std::vector<Scene>::iterator i = group->scenes.begin();
    std::vector<Scene>::iterator end = group->scenes.end();

    for (; i != end; ++i)
    {
        if (i->id == sceneId)
        {
            if (i->state == Scene::StateDeleted && group->m_deviceMemberships.size() == 0) // don't touch scenes from switch
            {
                GroupInfo *groupInfo = getGroupInfo(lightNode, group->address());

                if (groupInfo)
                {
                    std::vector<uint8_t> &v = groupInfo->removeScenes;

                    if (std::find(v.begin(), v.end(), sceneId) == v.end())
                    {
                        DBG_Printf(DBG_INFO, "Found Scene %u which was deleted before, delete again\n", sceneId);
                        groupInfo->removeScenes.push_back(sceneId);
                    }
                }
            }
            return; // already known
        }
    }

    DBG_Printf(DBG_INFO, "0x%016llX found scene 0x%02X for group 0x%04X\n", lightNode->address().ext(), sceneId, group->address());

    Scene scene;
    scene.groupAddress = group->address();
    scene.id = sceneId;
    openDb();
    loadSceneFromDb(&scene);
    closeDb();
    if (scene.name.isEmpty())
    {
        scene.name.sprintf("Scene %u", sceneId);
    }
    group->scenes.push_back(scene);
    updateGroupEtag(group);
    updateEtag(gwConfigEtag);
    queSaveDb(DB_SCENES, DB_SHORT_SAVE_DELAY);
}

/*! Sets the name of a scene which will be saved in the database.
 */
void DeRestPluginPrivate::setSceneName(Group *group, uint8_t sceneId, const QString &name)
{
    DBG_Assert(group != 0);
    DBG_Assert(name.size() != 0);

    if(!group || name.isEmpty())
    {
        return;
    }

    std::vector<Scene>::iterator i = group->scenes.begin();
    std::vector<Scene>::iterator end = group->scenes.end();

    for (; i != end; ++i)
    {
        if (i->id == sceneId)
        {
            i->name = name;
            queSaveDb(DB_SCENES, DB_SHORT_SAVE_DELAY);
            updateEtag(group->etag);
            break;
        }
    }
}

/*! Sends a store scene request to a group.
 */
bool DeRestPluginPrivate::storeScene(Group *group, uint8_t sceneId)
{
    DBG_Assert(group != 0);

    if (!group)
    {
        return false;
    }

    Scene *scene = group->getScene(sceneId);
    if (!scene)
    {
        return false;
    }

    {
        TaskItem task;
        task.ordered = true;
        task.req.setDstAddressMode(deCONZ::ApsGroupAddress);
        task.req.dstAddress().setGroup(group->address());
        task.req.setDstEndpoint(0xff);
        task.req.setSrcEndpoint(0x01);

        // add or replace empty scene, needed to set transition time
        if (!addTaskAddEmptyScene(task, group->address(), scene->id, scene->transitiontime()))
        {
            return false;
        }
    }

    {
        TaskItem task;
        task.ordered = true;
        task.req.setDstAddressMode(deCONZ::ApsGroupAddress);
        task.req.dstAddress().setGroup(group->address());
        task.req.setDstEndpoint(0xff);
        task.req.setSrcEndpoint(0x01);

        if (!addTaskStoreScene(task, group->address(), scene->id))
        {
            return false;
        }
    }
#if 0
    std::vector<LightNode>::iterator i = nodes.begin();
    std::vector<LightNode>::iterator end = nodes.end();
    for (; i != end; ++i)
    {
        LightNode *lightNode = &(*i);
        if (lightNode->isAvailable() && // note: we only create/store the scene if node is available
            isLightNodeInGroup(lightNode, group->address()) )
        {
            GroupInfo *groupInfo = getGroupInfo(lightNode, group->address());

            if (!groupInfo)
            {
                continue;
            }

            //if (lightNode->sceneCapacity() != 0 || groupInfo->sceneCount() != 0) //xxx workaround
            {
                std::vector<uint8_t> &v = groupInfo->modifyScenes;

                if (std::find(v.begin(), v.end(), sceneId) == v.end())
                {
                    groupInfo->modifyScenes.push_back(sceneId);
                }
            }

            /*if (lightNode->manufacturerCode() == VENDOR_OSRAM ||
                lightNode->manufacturerCode() == VENDOR_OSRAM_STACK)
            {
                // quirks mode: need extra store scene command (color temperature issue)
                std::vector<uint8_t> &v = groupInfo->addScenes;

                if (std::find(v.begin(), v.end(), sceneId) == v.end())
                {
                    groupInfo->addScenes.push_back(sceneId);
                }
            }*/
        }
    }
#endif

    return true;
}

/*! Sends a modify scene request to a group.
 */
bool DeRestPluginPrivate::modifyScene(Group *group, uint8_t sceneId)
{
    DBG_Assert(group != 0);

    if (!group)
    {
        return false;
    }

    std::vector<LightNode>::iterator i = nodes.begin();
    std::vector<LightNode>::iterator end = nodes.end();
    for (; i != end; ++i)
    {
        LightNode *lightNode = &(*i);
        if (lightNode->isAvailable() && // note: we only modify the scene if node is available
            isLightNodeInGroup(lightNode, group->address()))
        {
            GroupInfo *groupInfo = getGroupInfo(lightNode, group->address());

            std::vector<uint8_t> &v = groupInfo->modifyScenes;

            if (std::find(v.begin(), v.end(), sceneId) == v.end())
            {
                DBG_Printf(DBG_INFO, "Start modify scene for 0x%016llX, groupId 0x%04X, scene 0x%02X\n", i->address().ext(), groupInfo->id, sceneId);
                groupInfo->modifyScenes.push_back(sceneId);
            }
        }
    }

    return true;
}

/*! Sends a remove scene request to a group.
 */
bool DeRestPluginPrivate::removeScene(Group *group, uint8_t sceneId)
{
    DBG_Assert(group != 0);

    if (!group)
    {
        return false;
    }

    {
        std::vector<Scene>::iterator i = group->scenes.begin();
        std::vector<Scene>::iterator end = group->scenes.end();

        for (; i != end; ++i)
        {
            if (i->id == sceneId)
            {
                i->state = Scene::StateDeleted;
                updateEtag(group->etag);
                updateEtag(gwConfigEtag);
                break;
            }
        }
    }

    std::vector<LightNode>::iterator i = nodes.begin();
    std::vector<LightNode>::iterator end = nodes.end();
    for (; i != end; ++i)
    {
        LightNode *lightNode = &(*i);
        // note: we queue removing of scene even if node is not available
        if (isLightNodeInGroup(lightNode, group->address()))
        {
            GroupInfo *groupInfo = getGroupInfo(lightNode, group->address());

            std::vector<uint8_t> &v = groupInfo->removeScenes;

            if (std::find(v.begin(), v.end(), sceneId) == v.end())
            {
                groupInfo->removeScenes.push_back(sceneId);
            }
        }
    }

    return true;
}

/*! Sends a call scene request to a group.
 */
bool DeRestPluginPrivate::callScene(Group *group, uint8_t sceneId)
{
    DBG_Assert(group != 0);

    if (!group)
    {
        return false;
    }

    TaskItem task;
    task.taskType = TaskCallScene;

    task.req.setTxOptions(0);
    task.req.setDstEndpoint(0xFF);
    task.req.setDstAddressMode(deCONZ::ApsGroupAddress);
    task.req.dstAddress().setGroup(group->address());
    task.req.setClusterId(SCENE_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);
    task.req.setSrcEndpoint(getSrcEndpoint(0, task.req));

    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(0x05); // recall scene
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCDisableDefaultResponse);

    { // payload
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        stream << group->address();
        stream << sceneId;
    }

    { // ZCL frame
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    if (addTask(task))
    {
        return true;
    }

    return false;
}

/*! Handle incoming DE cluster commands.
 */
void DeRestPluginPrivate::handleDEClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    LightNode *lightNode = getLightNodeForAddress(ind.srcAddress(), ind.srcEndpoint());

    if (!lightNode)
    {
        return;
    }

    if (zclFrame.isClusterCommand() && zclFrame.commandId() == 0x03)
    {
    }

    if (zclFrame.isDefaultResponse())
    {
        DBG_Printf(DBG_INFO, "DE cluster default response cmd 0x%02X, status 0x%02X\n", zclFrame.defaultResponseCommandId(), zclFrame.defaultResponseStatus());
    }
}

/*! Handle incoming XAL cluster commands.
 */
void DeRestPluginPrivate::handleXalClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    LightNode *lightNode = getLightNodeForAddress(ind.srcAddress(), ind.srcEndpoint());

    if (!lightNode)
    {
        return;
    }

    if ((lightNode->address().ext() & macPrefixMask) != xalMacPrefix)
    {
        return;
    }

    bool updated = false;

    if (zclFrame.frameControl() & deCONZ::ZclFCDirectionServerToClient && zclFrame.isClusterCommand())
    {
        QDataStream stream(zclFrame.payload());
        stream.setByteOrder(QDataStream::LittleEndian);
        quint8 status;

        stream >> status;

        if (zclFrame.commandId() == 0x05) // light id
        {
            quint8 id;
            stream >> id;
            ResourceItem *item = lightNode->addItem(DataTypeUInt32, RConfigId);
            if (!item->lastSet().isValid() || item->toNumber() != id)
            {
                item->setValue(id);
                enqueueEvent(Event(RLights, item->descriptor().suffix, lightNode->id(), item));
                updated = true;
            }
        }
        else if (zclFrame.commandId() == 0x07) // min. level
        {
            quint8 minLevel;
            stream >> minLevel;
            ResourceItem *item = lightNode->addItem(DataTypeUInt8, RConfigLevelMin);
            if (!item->lastSet().isValid() || item->toNumber() != minLevel)
            {
                item->setValue(minLevel);
                enqueueEvent(Event(RLights, item->descriptor().suffix, lightNode->id(), item));
                updated = true;
            }
        }
        else if (zclFrame.commandId() == 0x09) // power on level
        {
            quint8 powerOnLevel;
            stream >> powerOnLevel;
            ResourceItem *item = lightNode->addItem(DataTypeUInt8, RConfigPowerOnLevel);
            if (!item->lastSet().isValid() || item->toNumber() != powerOnLevel)
            {
                item->setValue(powerOnLevel);
                enqueueEvent(Event(RLights, item->descriptor().suffix, lightNode->id(), item));
                updated = true;
            }
        }
        else if (zclFrame.commandId() == 0x0d) // power on temperature
        {
            quint16 powerOnTemp;
            stream >> powerOnTemp;
            ResourceItem *item = lightNode->addItem(DataTypeUInt16, RConfigPowerOnCt);
            if (!item->lastSet().isValid() || item->toNumber() != powerOnTemp)
            {
                item->setValue(powerOnTemp);
                enqueueEvent(Event(RLights, item->descriptor().suffix, lightNode->id(), item));
                updated = true;
            }
        }
    }

    if (updated)
    {
        updateLightEtag(lightNode);
    }

    if (zclFrame.isDefaultResponse())
    {
        DBG_Printf(DBG_INFO, "XAL cluster default response cmd 0x%02X, status 0x%02X\n", zclFrame.defaultResponseCommandId(), zclFrame.defaultResponseStatus());
    }
}

/*! Handle incoming ZCL attribute report commands.
 */
void DeRestPluginPrivate::handleZclAttributeReportIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    Q_UNUSED(zclFrame);

    bool checkReporting = false;
    const quint64 macPrefix = ind.srcAddress().ext() & macPrefixMask;

    DBG_Printf(DBG_INFO, "ZCL attribute report 0x%016llX for cluster 0x%04X, ep 0x%02X\n", ind.srcAddress().ext(), ind.clusterId(), ind.srcEndpoint());

    if (DBG_IsEnabled(DBG_INFO_L2))
    {
        DBG_Printf(DBG_INFO_L2, "\tpayload: %s\n", qPrintable(zclFrame.payload().toHex()));
    }

    if (!(zclFrame.frameControl() & deCONZ::ZclFCDisableDefaultResponse))
    {
        checkReporting = true;
        sendZclDefaultResponse(ind, zclFrame, deCONZ::ZclSuccessStatus);
    }
    else if (macPrefix == philipsMacPrefix ||
             macPrefix == tiMacPrefix ||
             macPrefix == ikeaMacPrefix ||
             macPrefix == heimanMacPrefix ||
             macPrefix == jennicMacPrefix ||
             macPrefix == silabsMacPrefix )
    {
        // these sensors tend to mac data poll after report
        checkReporting = true;
    }

    if (checkReporting)
    {
        for (Sensor &sensor : sensors)
        {
            if (sensor.deletedState() != Sensor::StateNormal)
            {
                continue;
            }

            if      (ind.srcAddress().hasExt() && sensor.address().hasExt() &&
                     ind.srcAddress().ext() == sensor.address().ext())
            { }
            else if (ind.srcAddress().hasNwk() && sensor.address().hasNwk() &&
                     ind.srcAddress().nwk() == sensor.address().nwk())
            { }
            else
            {
                continue;
            }

            if (sensor.node() &&
                sensor.lastAttributeReportBind() < (idleTotalCounter - BUTTON_ATTR_REPORT_BIND_LIMIT))
            {
                sensor.setLastAttributeReportBind(idleTotalCounter);
                checkSensorBindingsForAttributeReporting(&sensor);
            }
        }
    }

    if (zclFrame.isProfileWideCommand() && ind.clusterId() == BASIC_CLUSTER_ID)
    {
        handleZclAttributeReportIndicationXiaomiSpecial(ind, zclFrame);
    }

    if (otauLastBusyTimeDelta() < (60 * 60))
    {
        if ((idleTotalCounter - otauUnbindIdleTotalCounter) > 5)
        {
            LightNode *lightNode = getLightNodeForAddress(ind.srcAddress());

            if (lightNode && lightNode->modelId().startsWith(QLatin1String("FLS-")))
            {
                otauUnbindIdleTotalCounter = idleTotalCounter;
                DBG_Printf(DBG_INFO, "ZCL attribute report 0x%016llX for cluster 0x%04X --> unbind (otau busy)\n", ind.srcAddress().ext(), ind.clusterId());

                BindingTask bindingTask;
                Binding &bnd = bindingTask.binding;

                bindingTask.action = BindingTask::ActionUnbind;
                bindingTask.state = BindingTask::StateIdle;

                bnd.srcAddress = lightNode->address().ext();
                bnd.srcEndpoint = ind.srcEndpoint();
                bnd.clusterId = ind.clusterId();
                bnd.dstAddress.ext = apsCtrl->getParameter(deCONZ::ParamMacAddress);
                bnd.dstAddrMode = deCONZ::ApsExtAddress;
                bnd.dstEndpoint = endpoint();

                queueBindingTask(bindingTask);
            }
        }
    }
}

/*! Handle manufacturer specific Xiaomi ZCL attribute report commands to basic cluster.
 */
void DeRestPluginPrivate::handleZclAttributeReportIndicationXiaomiSpecial(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    quint16 attrId = 0;
    quint8 dataType = 0;
    quint8 length = 0;

    QDataStream stream(zclFrame.payload());
    stream.setByteOrder(QDataStream::LittleEndian);

    while (attrId != 0xff01)
    {
        if (stream.atEnd())
        {
            break;
        }

        stream >> attrId;
        stream >> dataType;
        stream >> length;

        if (dataType == deCONZ::ZclCharacterString && attrId != 0xff01)
        {
            DBG_Printf(DBG_INFO, "0x%016llX skip Xiaomi attribute 0x%04X\n", ind.srcAddress().ext(), attrId);
            for (; length > 0; length--) // skip
            {
                quint8 dummy;
                stream >> dummy;
            }
        }
    }

    if (stream.atEnd() || attrId != 0xff01 || dataType != deCONZ::ZclCharacterString)
    {
        return;
    }

    quint16 battery = 0;
    quint32 lightlevel = UINT32_MAX; // use 32-bit to mark invalid and support 0xffff value
    qint16 temperature = INT16_MIN;
    quint16 humidity = UINT16_MAX;
    qint16 pressure = INT16_MIN;
    quint8 onOff = UINT8_MAX;
    quint8 onOff2 = UINT8_MAX;
    quint8 currentPositionLift = UINT8_MAX;

    DBG_Printf(DBG_INFO, "0x%016llX extract Xiaomi special\n", ind.srcAddress().ext());

    QString dateCode;

    while (!stream.atEnd())
    {
        qint8 s8;
        qint16 s16;
        quint8 u8;
        quint16 u16;
        qint32 s32;
        quint32 u32;
        quint64 u64;


        quint8 tag;
        stream >> tag;
        stream >> dataType;

        switch (dataType)
        {
        case deCONZ::ZclBoolean: stream >> u8; break;
        case deCONZ::Zcl8BitInt: stream >> s8; break;
        case deCONZ::Zcl8BitUint: stream >> u8; break;
        case deCONZ::Zcl16BitInt: stream >> s16; break;
        case deCONZ::Zcl16BitUint: stream >> u16; break;
        case deCONZ::Zcl32BitInt: stream >> s32; break;
        case deCONZ::Zcl32BitUint: stream >> u32; break;
        case deCONZ::Zcl40BitUint:
            u64 = 0;
            for (int i = 0; i < 5; i++)
            {
                u64 <<= 8;
                stream >> u8;
                u64 |= u8;
            }
            break;
        case deCONZ::Zcl48BitUint:
            u64 = 0;
            for (int i = 0; i < 6; i++)
            {
                u64 <<= 8;
                stream >> u8;
                u64 |= u8;
            }
            break;
        case deCONZ::Zcl64BitUint: stream >> u64; break;
        case deCONZ::ZclSingleFloat: stream >> u32; break;  // FIXME: use 4-byte float data type
        default:
        {
            DBG_Printf(DBG_INFO, "\tUnsupported datatype 0x%02X (tag 0x%02X)\n", dataType, tag);
        }
            return;
        }

        if (tag == 0x01 && dataType == deCONZ::Zcl16BitUint)
        {
            DBG_Printf(DBG_INFO, "\t01 battery %u (0x%04X)\n", u16, u16);
            battery = u16;
        }
        else if (tag == 0x03 && dataType == deCONZ::Zcl8BitInt)
        {
            DBG_Printf(DBG_INFO, "\t03 temperature %d °C\n", int(s8));
            temperature = qint16(s8) * 100;
        }
        else if (tag == 0x04 && dataType == deCONZ::Zcl16BitUint)
        {
            DBG_Printf(DBG_INFO, "\t04 unknown %d (0x%04X)\n", u16, u16);
        }
        else if (tag == 0x05 && dataType == deCONZ::Zcl16BitUint)
        {
            DBG_Printf(DBG_INFO, "\t05 RSSI dB (?) %d (0x%04X)\n", u16, u16);
        }
        else if (tag == 0x06 && dataType == deCONZ::Zcl40BitUint)
        {
            DBG_Printf(DBG_INFO, "\t06 LQI (?) %lld (0x%010llX)\n", u64, u64);
        }
        else if (tag == 0x07 && dataType == deCONZ::Zcl64BitUint) // lumi.ctrl_ln2
        {
            DBG_Printf(DBG_INFO, "\t07 unknown %lld (0x%016llX)\n", u64, u64);
        }
        else if (tag == 0x08 && dataType == deCONZ::Zcl16BitUint) // lumi.ctrl_ln2
        {
            DBG_Printf(DBG_INFO, "\t08 unknown %d (0x%04X)\n", u16, u16);
        }
        else if (tag == 0x09 && dataType == deCONZ::Zcl16BitUint) // lumi.ctrl_ln2
        {
            DBG_Printf(DBG_INFO, "\t09 unknown %d (0x%04X)\n", u16, u16);
        }
        else if (tag == 0x0a && dataType == deCONZ::Zcl16BitUint) // lumi.vibration.aq1
        {
            DBG_Printf(DBG_INFO, "\t0a unknown %d (0x%04X)\n", u16, u16);
        }
        else if (tag == 0x0b && dataType == deCONZ::Zcl16BitUint)
        {
            DBG_Printf(DBG_INFO, "\t0b lightlevel %u (0x%04X)\n", u16, u16);
            lightlevel = u16;
        }
        else if (tag == 0x64 && dataType == deCONZ::ZclBoolean) // lumi.ctrl_ln2 endpoint 01
        {
            DBG_Printf(DBG_INFO, "\t64 on/off %d\n", u8);
            onOff = u8;
        }
        else if (tag == 0x64 && dataType == deCONZ::Zcl8BitUint) // lumi.curtain
        {
            DBG_Printf(DBG_INFO, "\t64 current position lift %d%%\n", u8);
            if (u8 <= 100)
            {
                currentPositionLift = 100 - u8;
            }
        }
        else if (tag == 0x64 && dataType == deCONZ::Zcl16BitInt)
        {
            DBG_Printf(DBG_INFO, "\t64 temperature %d\n", int(s16));
            temperature = s16;
        }
        else if (tag == 0x65 && dataType == deCONZ::ZclBoolean) // lumi.ctrl_ln2 endpoint 02
        {
            DBG_Printf(DBG_INFO, "\t65 on/off %d\n", u8);
            onOff2 = u8;
        }
        else if (tag == 0x65 && dataType == deCONZ::Zcl16BitUint)
        {
            DBG_Printf(DBG_INFO, "\t65 humidity %u\n", u16); // Mi
            humidity = u16;
        }
        else if (tag == 0x66 && dataType == deCONZ::Zcl32BitInt) // lumi.weather
        {
            pressure = (s32 + 50) / 100;
            DBG_Printf(DBG_INFO, "\t66 pressure %d\n", pressure);
        }
        else if (tag == 0x6e && dataType == deCONZ::Zcl8BitUint) // lumi.ctrl_neutral2
        {
            DBG_Printf(DBG_INFO, "\t6e unknown %d (0x%02X)\n", u8, u8);
        }
        else if (tag == 0x6f && dataType == deCONZ::Zcl8BitUint) // lumi.ctrl_neutral2
        {
            DBG_Printf(DBG_INFO, "\t6f unknown %d (0x%02X)\n", u8, u8);
        }
        else if (tag == 0x95 && dataType == deCONZ::ZclSingleFloat) // lumi.ctrl_ln2
        {
            DBG_Printf(DBG_INFO, "\t95 consumption (?) 0x%08X\n", u32);
        }
        else if (tag == 0x97 && dataType == deCONZ::Zcl16BitUint) // lumi.sensor_cube
        {
            DBG_Printf(DBG_INFO, "\t97 unknown %d (0x%04X)\n", u16, u16);
        }
        else if (tag == 0x98 && dataType == deCONZ::Zcl16BitUint) // lumi.sensor_cube
        {
            DBG_Printf(DBG_INFO, "\t98 unknown %d (0x%04X)\n", u16, u16);
        }
        else if (tag == 0x98 && dataType == deCONZ::ZclSingleFloat) // lumi.ctrl_ln2
        {
            DBG_Printf(DBG_INFO, "\t98 power (?) 0x%08X\n", u32);
        }
        else if (tag == 0x99 && dataType == deCONZ::Zcl16BitUint) // lumi.sensor_cube
        {
            DBG_Printf(DBG_INFO, "\t99 unknown %d (0x%04X)\n", u16, u16);
        }
        else if (tag == 0x99 && dataType == deCONZ::Zcl32BitUint) // lumi.ctrl_neutral2
        {
            DBG_Printf(DBG_INFO, "\t99 unknown %d (0x%08X)\n", u32, u32);
        }
        else if (tag == 0x9a && dataType == deCONZ::Zcl8BitUint) // lumi.ctrl_ln2
        {
            DBG_Printf(DBG_INFO, "\t9a unknown %d (0x%02X)\n", u8, u8);
        }
        else if (tag == 0x9a && dataType == deCONZ::Zcl16BitUint) // lumi.sensor_cube
        {
            DBG_Printf(DBG_INFO, "\t9a unknown %d (0x%04X)\n", u16, u16);
        }
        else if (tag == 0x9a && dataType == deCONZ::Zcl48BitUint) // lumi.vibration.aq1
        {
            DBG_Printf(DBG_INFO, "\t9a unknown %lld (0x%012llX)\n", u64, u64);
        }
        else if (tag == 0x9b && dataType == deCONZ::Zcl16BitUint) // lumi.ctrl_neutral2
        {
            DBG_Printf(DBG_INFO, "\t9b unknown %d (0x%04X)\n", u16, u16);
        }
        else
        {
            DBG_Printf(DBG_INFO, "\t%02X unsupported tag (data type 0x%02X)\n", tag, dataType);
        }
    }

    RestNodeBase *restNodePending = nullptr;

    for (LightNode &lightNode: nodes)
    {
        if      (ind.srcAddress().hasExt() && lightNode.address().hasExt() &&
                 ind.srcAddress().ext() == lightNode.address().ext())
        { }
        else if (ind.srcAddress().hasNwk() && lightNode.address().hasNwk() &&
                 ind.srcAddress().nwk() == lightNode.address().nwk())
        { }
        else
        {
            continue;
        }

        quint8 value = UINT8_MAX;
        ResourceItem *item;

        if (lightNode.modelId().startsWith(QLatin1String("lumi.ctrl_neutral")))
        {
            if (lightNode.haEndpoint().endpoint() == 0x02 && onOff != UINT8_MAX)
            {
                value = onOff;

            }
            else if (lightNode.haEndpoint().endpoint() == 0x03 && onOff2 != UINT8_MAX)
            {
                value = onOff2;
            }
            else
            {
                continue;
            }
        }
        else if (lightNode.modelId().startsWith(QLatin1String("lumi.ctrl_ln")))
        {
            if (lightNode.haEndpoint().endpoint() == 0x01 && onOff != UINT8_MAX)
            {
                value = onOff;
            }
            else if (lightNode.haEndpoint().endpoint() == 0x02 && onOff2 != UINT8_MAX)
            {
                value = onOff2;
            }
            else
            {
                continue;
            }
        }
        else if (lightNode.modelId().startsWith(QLatin1String("lumi.curtain")) && currentPositionLift != UINT8_MAX)
        {
            item = lightNode.item(RStateBri);
            if (item)
            {
                const uint bri = currentPositionLift * 255 / 100;
                item->setValue(bri);
                enqueueEvent(Event(RLights, item->descriptor().suffix, lightNode.id(), item));
                value = bri != 0;
            }
        }
        else
        {
            continue;
        }

        lightNode.rx();
        item = lightNode.item(RStateReachable);
        if (item && !item->toBool())
        {
            item->setValue(true);
            enqueueEvent(Event(RLights, item->descriptor().suffix, lightNode.id(), item));
        }
        item = lightNode.item(RStateOn);
        if (item)
        {
            item->setValue(value);
            enqueueEvent(Event(RLights, item->descriptor().suffix, lightNode.id(), item));
        }
        updateLightEtag(&lightNode);
        lightNode.setNeedSaveDatabase(true);
        saveDatabaseItems |= DB_LIGHTS;
    }

    for (Sensor &sensor : sensors)
    {
        if (!sensor.modelId().startsWith(QLatin1String("lumi.")))
        {
            continue;
        }

        if      (ind.srcAddress().hasExt() && sensor.address().hasExt() &&
                 ind.srcAddress().ext() == sensor.address().ext())
        { }
        else if (ind.srcAddress().hasNwk() && sensor.address().hasNwk() &&
                 ind.srcAddress().nwk() == sensor.address().nwk())
        { }
        else
        {
            continue;
        }

        sensor.rx();
        restNodePending = &sensor; // remember one sensor for pending tasks

        {
            ResourceItem *item = sensor.item(RConfigReachable);
            if (item && !item->toBool())
            {
                item->setValue(true);
                Event e(RSensors, RConfigReachable, sensor.id(), item);
                enqueueEvent(e);
            }
        }

        bool updated = false;
        if (battery != 0)
        {
            ResourceItem *item = sensor.item(RConfigBattery);
            // DBG_Assert(item != 0); // expected - no, lumi.ctrl_neutral2
            if (item)
            {
                // 2.7-3.0V taken from:
                // https://github.com/snalee/Xiaomi/blob/master/devicetypes/a4refillpad/xiaomi-zigbee-button.src/xiaomi-zigbee-button.groovy
                const float vmin = 2700;
                const float vmax = 3000;
                float bat = battery;

                if      (bat > vmax) { bat = vmax; }
                else if (bat < vmin) { bat = vmin; }

                bat = ((bat - vmin) /(vmax - vmin)) * 100;

                if      (bat > 100) { bat = 100; }
                else if (bat <= 0)  { bat = 1; } // ?

                item->setValue(quint8(bat));

                enqueueEvent(Event(RSensors, RConfigBattery, sensor.id(), item));
                updated = true;
            }
        }

        if (temperature != INT16_MIN)
        {
            ResourceItem *item = sensor.item(RConfigTemperature);
            item = item ? item : sensor.item(RStateTemperature);
            if (item)
            {
                item->setValue(temperature);
                enqueueEvent(Event(RSensors, item->descriptor().suffix, sensor.id(), item));
                updated = true;

                if (item->descriptor().suffix == RStateTemperature)
                {
                    sensor.updateStateTimestamp();
                }
            }
        }

        if (humidity != UINT16_MAX)
        {
            ResourceItem *item = sensor.item(RStateHumidity);
            if (item)
            {
                item->setValue(humidity);
                enqueueEvent(Event(RSensors, item->descriptor().suffix, sensor.id(), item));
                updated = true;
                sensor.updateStateTimestamp();
            }
        }

        if (pressure != INT16_MIN)
        {
          ResourceItem *item = sensor.item(RStatePressure);
          if (item)
          {
              item->setValue(pressure);
              enqueueEvent(Event(RSensors, item->descriptor().suffix, sensor.id(), item));
              updated = true;
              sensor.updateStateTimestamp();
          }
        }

        if (lightlevel != UINT32_MAX &&
            sensor.type() == QLatin1String("ZHALightLevel") &&
            sensor.modelId().startsWith(QLatin1String("lumi.sensor_motion")))
        {
            updateSensorLightLevel(sensor, lightlevel);
            updated = true;
        }

        if (onOff != UINT8_MAX)
        {   // don't add, just update, useful since door/window and presence sensors otherwise only report on activation
            ResourceItem *item = sensor.item(RStateOpen);
            item = item ? item : sensor.item(RStatePresence);
            item = item ? item : sensor.item(RStateWater);      // lumi.sensor_wleak.aq1
            if (item)
            {
                item->setValue(onOff);
                enqueueEvent(Event(RSensors, item->descriptor().suffix, sensor.id(), item));
                sensor.updateStateTimestamp();
                updated = true;
            }
        }

        ResourceItem *item = sensor.item(RAttrSwVersion);
        if (item && dateCode.isEmpty() && !item->toString().isEmpty() && !item->toString().startsWith("3000"))
        {
            dateCode = item->toString();
        }

        if (updated)
        {
            updateSensorEtag(&sensor);
            sensor.setNeedSaveDatabase(true);
            saveDatabaseItems |= DB_SENSORS;
        }
    }

    if (dateCode.isEmpty() && restNodePending)
    {
        // read datecode, will be applied to all sensors of this device
        readAttributes(restNodePending, ind.srcEndpoint(), BASIC_CLUSTER_ID, { 0x0006 });
    }
}

void DeRestPluginPrivate::queuePollNode(RestNodeBase *node)
{
    if (!node || !node->node())
    {
        return;
    }

    if (!node->node()->nodeDescriptor().receiverOnWhenIdle())
    {
        return; // only support non sleeping devices for now
    }

    if (std::find(pollNodes.begin(), pollNodes.end(), node) != pollNodes.end())
    {
        return; // already in queue
    }

    pollNodes.push_back(node);
}

void DeRestPluginPrivate::sendZclDefaultResponse(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame, quint8 status)
{
   deCONZ::ApsDataRequest apsReq;

    // ZDP Header
    apsReq.dstAddress() = ind.srcAddress();
    apsReq.setDstAddressMode(ind.srcAddressMode());
    apsReq.setDstEndpoint(ind.srcEndpoint());
    apsReq.setSrcEndpoint(ind.dstEndpoint());
    apsReq.setProfileId(ind.profileId());
    apsReq.setRadius(0);
    apsReq.setClusterId(ind.clusterId());
    //apsReq.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);

    deCONZ::ZclFrame outZclFrame;
    outZclFrame.setSequenceNumber(zclFrame.sequenceNumber());
    outZclFrame.setCommandId(deCONZ::ZclDefaultResponseId);
    outZclFrame.setFrameControl(deCONZ::ZclFCProfileCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCDisableDefaultResponse);

    { // ZCL payload
        QDataStream stream(&outZclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream << zclFrame.commandId();
        stream << status;
    }

    { // ZCL frame
        QDataStream stream(&apsReq.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        outZclFrame.writeToStream(stream);
    }

    if (apsCtrl && apsCtrl->apsdeDataRequest(apsReq) == deCONZ::Success)
    {
        queryTime = queryTime.addSecs(1);
    }
}

/*! Stores on/off and bri of a light so that the state can be recovered after powercycle/powerloss.
    \param lightNode - the related light
 */
void DeRestPluginPrivate::storeRecoverOnOffBri(LightNode *lightNode)
{
    if (!lightNode || !lightNode->address().hasNwk())
    {
        return;
    }

    ResourceItem *onOff = lightNode->item(RStateOn);
    ResourceItem *bri = lightNode->item(RStateBri);
    std::vector<RecoverOnOff>::iterator i = recoverOnOff.begin();
    std::vector<RecoverOnOff>::iterator end = recoverOnOff.end();

    for (; i != end; ++i)
    {
        if (i->address.hasNwk() && lightNode->address().hasNwk() &&
            i->address.nwk() == lightNode->address().nwk())
        {
            // update entry
            i->onOff = onOff ? onOff->toBool() : false;
            if (bri && bri->lastSet().isValid()) { i->bri = bri->toNumber(); }
            else                                 { i->bri = 0; }

            i->idleTotalCounterCopy = idleTotalCounter;
            return;
        }
    }

    // create new entry
    DBG_Printf(DBG_INFO, "New recover onOff entry 0x%016llX\n", lightNode->address().ext());
    RecoverOnOff rc;
    rc.address = lightNode->address();
    rc.onOff = onOff ? onOff->toBool() : false;
    rc.bri = bri ? bri->toNumber() : 0;
    rc.idleTotalCounterCopy = idleTotalCounter;
    recoverOnOff.push_back(rc);
}

/*! Temporary FLS-NB maintenance. */
bool DeRestPluginPrivate::flsNbMaintenance(LightNode *lightNode)
{
    ResourceItem *item = 0;
    item = lightNode->item(RStateReachable);
    DBG_Assert(item != 0);
    if (!item || !item->lastSet().isValid() || !item->toBool())
    {
        return false;
    }

    QDateTime now = QDateTime::currentDateTime();
    QSettings config(deCONZ::getStorageLocation(deCONZ::ConfigLocation), QSettings::IniFormat);

    int resetDelay = config.value("fls-nb/resetdelay", 0).toInt(); // default to disabled
    int resetPhase = config.value("fls-nb/resetphase", 100).toInt(); // DL_NADIR
    int noPirDelay = config.value("fls-nb/nopirdelay", 60 * 30).toInt(); // 30 minutes

    if (resetDelay == 0)
    {
        return false; // disabled
    }

    int uptime = item->lastSet().secsTo(now);
    DBG_Printf(DBG_INFO, "0x%016llx uptime %d\n", lightNode->address().ext(), uptime);

    if (uptime < resetDelay)
    {
        return false;
    }

    item = lightNode->item(RConfigPowerup);
    quint32 powerup = item ? item->toNumber() : 0;

    if ((powerup & R_POWERUP_RESTORE) == 0)
    {
        return false;
    }

    // check for specific phase
    Sensor *daylight = getSensorNodeForId(daylightSensorId);
    item = daylight ? daylight->item(RConfigConfigured) : 0;
    if (!item)
    {
        return false;
    }

    item = daylight->item(RStateStatus);
    if (resetPhase == 0) // 0 = disabled (for testing)
    {}
    else if (!item || item->toNumber() != resetPhase)
    {
        return false;
    }

    // wait until no motion was detected for configured time
    if (globalLastMotion.isValid() && globalLastMotion.secsTo(now) < noPirDelay)
    {
        return false;
    }

    DBG_Printf(DBG_INFO, "0x%016llx start powercycle\n", lightNode->address().ext());

    deCONZ::ApsDataRequest req;
    req.setProfileId(HA_PROFILE_ID);
    req.setDstEndpoint(0x0A);
    req.setClusterId(OTAU_CLUSTER_ID);
    req.dstAddress() = lightNode->address();
    req.setDstAddressMode(deCONZ::ApsExtAddress);
    req.setSrcEndpoint(endpoint());
    req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
    req.setRadius(0);

    deCONZ::ZclFrame zclFrame;
    zclFrame.setSequenceNumber(zclSeq++);
    zclFrame.setCommandId(0x07); // OTAU_UPGRADE_END_RESPONSE_CMD_ID

    zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                             deCONZ::ZclFCDirectionServerToClient |
                             deCONZ::ZclFCDisableDefaultResponse);

    { // ZCL payload
        QDataStream stream(&zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        stream << (quint16)VENDOR_DDEL;
        stream << (quint16)0x0002;
        stream << (quint32)0; // file version

        stream << (quint32)0; // current time
        stream << (quint32)0; // upgrade time
    }

    { // ZCL frame
        QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        zclFrame.writeToStream(stream);
    }

    storeRecoverOnOffBri(lightNode);

    if (deCONZ::ApsController::instance()->apsdeDataRequest(req) == deCONZ::Success)
    {
        return true;
    }

    return false;
}

/*! Queues a client for closing the connection.
    \param sock the client socket
    \param closeTimeout timeout in seconds then the socket should be closed
 */
void DeRestPluginPrivate::pushClientForClose(QTcpSocket *sock, int closeTimeout, const QHttpRequestHeader &hdr)
{
    std::vector<TcpClient>::iterator i = openClients.begin();
    std::vector<TcpClient>::iterator end = openClients.end();

    for ( ;i != end; ++i)
    {
        if (i->sock == sock)
        {
            // update
            i->hdr = hdr;
            if (i->closeTimeout < closeTimeout)
            {
                i->closeTimeout = closeTimeout;
            }
            //DBG_Printf(DBG_INFO, "refresh socket %s : %u %s\n", qPrintable(sock->peerAddress().toString()), sock->peerPort(), qPrintable(hdr.path()));
            return;
        }
    }

    TcpClient client;
    client.hdr = hdr;
    client.created = QDateTime::currentDateTime();
    client.sock = sock;
    client.closeTimeout = closeTimeout;

    connect(sock, SIGNAL(destroyed()),
            this, SLOT(clientSocketDestroyed()));

    openClients.push_back(client);
}

/*! Adds a task to the queue.
    \return true - on success
 */
bool DeRestPluginPrivate::addTask(const TaskItem &task)
{
    if (!isInNetwork())
    {
        return false;
    }

    if (channelChangeState != CC_Idle)
    {
        return false;
    }

    if (DBG_IsEnabled(DBG_INFO))
    {
        if (task.req.dstAddress().hasExt())
        {
            DBG_Printf(DBG_INFO_L2, "add task %d type %d to 0x%016llX cluster 0x%04X req.id %u\n", task.taskId, task.taskType, task.req.dstAddress().ext(), task.req.clusterId(), task.req.id());
        }
        else if (task.req.dstAddress().hasGroup())
        {
            DBG_Printf(DBG_INFO_L2, "add task %d type %d to group 0x%04X cluster 0x%04X req.id %u\n", task.taskId, task.taskType, task.req.dstAddress().group(), task.req.clusterId(), task.req.id());
        }
    }

    const uint MaxTasks = 20;

    std::list<TaskItem>::iterator i = tasks.begin();
    std::list<TaskItem>::iterator end = tasks.end();

    if ((task.taskType != TaskGetSceneMembership) &&
        (task.taskType != TaskGetGroupMembership) &&
        (task.taskType != TaskGetGroupIdentifiers) &&
        (task.taskType != TaskStoreScene) &&
        (task.taskType != TaskRemoveScene) &&
        (task.taskType != TaskRemoveAllScenes) &&
        (task.taskType != TaskReadAttributes) &&
        (task.taskType != TaskWriteAttribute) &&
        (task.taskType != TaskViewScene) &&
        (task.taskType != TaskAddScene))
    {
        for (; i != end; ++i)
        {
            if (i->taskType == task.taskType)
            {
                if ((i->req.dstAddress() ==  task.req.dstAddress()) &&
                    (i->req.dstEndpoint() ==  task.req.dstEndpoint()) &&
                    (i->req.srcEndpoint() ==  task.req.srcEndpoint()) &&
                    (i->req.profileId() ==  task.req.profileId()) &&
                    (i->req.clusterId() ==  task.req.clusterId()) &&
                    (i->req.txOptions() ==  task.req.txOptions()) &&
                    (i->req.asdu().size() ==  task.req.asdu().size()))

                {
                    DBG_Printf(DBG_INFO, "Replace task %d type %d in queue cluster 0x%04X with newer task of same type. %u runnig tasks\n", task.taskId, task.taskType, task.req.clusterId(), runningTasks.size());
                    *i = task;
                    return true;
                }
            }
        }
    }

    if (tasks.size() < MaxTasks) {
        tasks.push_back(task);
        return true;
    }

    DBG_Printf(DBG_INFO, "failed to add task %d type: %d, too many tasks\n", task.taskId, task.taskType);

    return false;
}

/*! Fires the next APS-DATA.request.
 */
void DeRestPluginPrivate::processTasks()
{
    if (!apsCtrl)
    {
        return;
    }

    if (tasks.empty())
    {
        return;
    }

    if (!isInNetwork())
    {
        DBG_Printf(DBG_INFO, "Not in network cleanup %d tasks\n", (runningTasks.size() + tasks.size()));
        runningTasks.clear();
        tasks.clear();
        return;
    }

    if (channelChangeState != CC_Idle)
    {
        return;
    }

    if (runningTasks.size() >= MAX_BACKGROUND_TASKS)
    {
        std::list<TaskItem>::iterator j = runningTasks.begin();
        std::list<TaskItem>::iterator jend = runningTasks.end();

        for (; j != jend; ++j)
        {
            int dt = idleTotalCounter - j->sendTime;

            if (dt > 120)
            {
                DBG_Printf(DBG_INFO, "drop request %u send time %d, cluster 0x%04X, after %d seconds\n", j->req.id(), j->sendTime, j->req.clusterId(), dt);
                runningTasks.erase(j);
                return;
            }

        }

        DBG_Printf(DBG_INFO, "%d running tasks, wait\n", runningTasks.size());
        return;
    }

    QTime now = QTime::currentTime();
    std::list<TaskItem>::iterator i = tasks.begin();
    std::list<TaskItem>::iterator end = tasks.end();

    for (; i != end; ++i)
    {
        if (i->lightNode)
        {
            // drop dead unicasts
            if (!i->lightNode->isAvailable() || !i->lightNode->lastRx().isValid())
            {
                DBG_Printf(DBG_INFO, "drop request to zombie (rx = %u)\n", (uint)i->lightNode->lastRx().isValid());
                tasks.erase(i);
                return;
            }
        }

        // send only few requests to a destination at a time
        int onAir = 0;
        const int maxOnAir = i->req.dstAddressMode() == deCONZ::ApsGroupAddress ? 6 : 2;
        std::list<TaskItem>::iterator j = runningTasks.begin();
        std::list<TaskItem>::iterator jend = runningTasks.end();

        bool ok = true;
        if (i->ordered && std::distance(tasks.begin(), i) > 0) // previous not processed yet
        {
            ok = false;
        }

        for (; ok && j != jend; ++j)
        {
            if (i->ordered && i->taskId == (j->taskId + 1)) // previous running
            {
                ok = false;
                break;
            }

            if (i->req.dstAddressMode() == deCONZ::ApsGroupAddress &&
                j->req.dstAddressMode() == deCONZ::ApsGroupAddress)
            {
                onAir++;

                if (onAir >= maxOnAir)
                {
                    ok = false;
                    break;
                }
            }
            else if (i->req.dstAddress() == j->req.dstAddress())
            {
                onAir++;
                int dt = idleTotalCounter - j->sendTime;
                if (dt < 5 || onAir >= maxOnAir)
                {
                    if (dt > 120)
                    {
                        DBG_Printf(DBG_INFO, "drop request %u send time %d, cluster 0x%04X, onAir %d after %d seconds\n", j->req.id(), j->sendTime, j->req.clusterId(), onAir, dt);
                        runningTasks.erase(j);
                        return;
                    }
                    else
                    {
                        //DBG_Printf(DBG_INFO, "request %u send time %d, cluster 0x%04X, onAir %d\n", i->req.id(), j->sendTime, j->req.clusterId(), onAir);
                        DBG_Printf(DBG_INFO, "delay sending request %u dt %d ms to 0x%016llX, cluster 0x%04X\n", i->req.id(), dt, i->req.dstAddress().ext(), i->req.clusterId());
                        ok = false;
                    }
                    break;
                }
            }
        }

        if (!ok) // destination already busy
        {
            if (i->req.dstAddressMode() == deCONZ::ApsExtAddress)
            {
                DBG_Printf(DBG_INFO_L2, "delay sending request %u cluster 0x%04X to %s\n", i->req.id(), i->req.clusterId(), qPrintable(i->req.dstAddress().toStringExt()));
            }
            else if (i->req.dstAddressMode() == deCONZ::ApsGroupAddress)
            {
                DBG_Printf(DBG_INFO, "delay sending request %u - type: %d to group 0x%04X\n", i->req.id(), i->taskType, i->req.dstAddress().group());
            }
        }
        else
        {
            bool pushRunning = (i->req.state() != deCONZ::FireAndForgetState);

            // groupcast tasks
            if (i->req.dstAddressMode() == deCONZ::ApsGroupAddress)
            {
                Group *group = getGroupForId(i->req.dstAddress().group());

                if (group)
                {
                    int diff = group->sendTime.msecsTo(now);

                    if (!group->sendTime.isValid() || (diff <= 0) || (diff > gwGroupSendDelay))
                    {
                        i->sendTime = idleTotalCounter;
                        if (apsCtrl->apsdeDataRequest(i->req) == deCONZ::Success)
                        {
                            group->sendTime = now;
                            if (pushRunning)
                            {
                                runningTasks.push_back(*i);
                            }
                            tasks.erase(i);
                            return;
                        }
                    }
                    else
                    {
                        DBG_Printf(DBG_INFO, "delayed group sending\n");
                    }
                }
                else
                {
                    DBG_Printf(DBG_INFO, "drop request to unknown group\n");
                    tasks.erase(i);
                    return;
                }
            }
            // unicast/broadcast tasks
            else
            {
                if (i->lightNode && !i->lightNode->isAvailable())
                {
                    DBG_Printf(DBG_INFO, "drop request to zombie\n");
                    tasks.erase(i);
                    return;
                }
                else
                {

                    i->sendTime = idleTotalCounter;
                    int ret = apsCtrl->apsdeDataRequest(i->req);

                    if (ret == deCONZ::Success)
                    {
                        if (pushRunning)
                        {
                            runningTasks.push_back(*i);
                        }
                        tasks.erase(i);
                        return;
                    }
                    else if (ret == deCONZ::ErrorNodeIsZombie)
                    {
                        DBG_Printf(DBG_INFO, "drop request to zombie\n");
                        tasks.erase(i);
                        return;
                    }
                    else
                    {
                        DBG_Printf(DBG_INFO, "enqueue APS request failed with error %d, drop\n", ret);
                        tasks.erase(i);
                        return;
                    }
                }
            }
        }
    }
}

/*! Handler for node events.
    \param event the event which occured
 */
void DeRestPluginPrivate::nodeEvent(const deCONZ::NodeEvent &event)
{
    if (event.event() != deCONZ::NodeEvent::NodeDeselected)
    {
        if (!event.node())
        {
            return;
        }
    }

    switch (event.event())
    {
    case deCONZ::NodeEvent::NodeSelected:
        break;

    case deCONZ::NodeEvent::NodeDeselected:
        break;

    case deCONZ::NodeEvent::NodeRemoved:
    {
        std::vector<LightNode>::iterator i = nodes.begin();
        std::vector<LightNode>::iterator end = nodes.end();

        for (; i != end; ++i)
        {
            if (i->address().ext() == event.node()->address().ext())
            {
                DBG_Printf(DBG_INFO, "LightNode removed %s\n", qPrintable(event.node()->address().toStringExt()));
                nodeZombieStateChanged(event.node());
            }
        }
    }
        break;

    case deCONZ::NodeEvent::NodeAdded:
    {
        QTime now = QTime::currentTime();
        if (queryTime.secsTo(now) < 20)
        {
            queryTime = now.addSecs(20);
        }
        if (event.node())
        {
            refreshDeviceDb(event.node()->address());
        }
        addLightNode(event.node());
        addSensorNode(event.node());
    }
        break;

#if DECONZ_LIB_VERSION >= 0x010900
    case deCONZ::NodeEvent::NodeMacDataRequest:
    {
        handleMacDataRequest(event);
    }
        break;
#endif

    case deCONZ::NodeEvent::NodeZombieChanged:
    {
        DBG_Printf(DBG_INFO, "Node zombie state changed %s\n", qPrintable(event.node()->address().toStringExt()));
        nodeZombieStateChanged(event.node());
    }
        break;

    case deCONZ::NodeEvent::UpdatedNodeAddress:
    {
        if (event.node())
        {
            refreshDeviceDb(event.node()->address());
        }
        break;
    }

    case deCONZ::NodeEvent::UpdatedSimpleDescriptor:
    {
        addLightNode(event.node());
        updatedLightNodeEndpoint(event);
        addSensorNode(event.node());
        checkUpdatedFingerPrint(event.node(), event.endpoint(), nullptr);
        if (!event.node())
        {
            return;
        }
        deCONZ::SimpleDescriptor sd;
        if (event.node()->copySimpleDescriptor(event.endpoint(), &sd) != 0)
        {
            return;
        }

        QByteArray data;
        QDataStream stream(&data, QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        sd.writeToStream(stream);
        if (!data.isEmpty())
        {
            pushZdpDescriptorDb(event.node()->address().ext(), sd.endpoint(), ZDP_SIMPLE_DESCRIPTOR_CLID, data);
        }
    }
        break;

    case deCONZ::NodeEvent::UpdatedNodeDescriptor:
    {
        if (event.node() && !event.node()->nodeDescriptor().isNull())
        {
            pushZdpDescriptorDb(event.node()->address().ext(), ZDO_ENDPOINT, ZDP_NODE_DESCRIPTOR_CLID, event.node()->nodeDescriptor().toByteArray());
        }
    }
        break;

    case deCONZ::NodeEvent::UpdatedPowerDescriptor:
    {
        updateSensorNode(event);
    }
        break;

    case deCONZ::NodeEvent::UpdatedClusterData:
    case deCONZ::NodeEvent::UpdatedClusterDataZclRead:
    case deCONZ::NodeEvent::UpdatedClusterDataZclReport:
    {
        if (event.profileId() == ZDP_PROFILE_ID && event.clusterId() == ZDP_ACTIVE_ENDPOINTS_RSP_CLID)
        {
            updateSensorNode(event);
            return;
        }

        if (event.profileId() != HA_PROFILE_ID && event.profileId() != ZLL_PROFILE_ID)
        {
            return;
        }

        DBG_Printf(DBG_INFO_L2, "Node data %s profileId: 0x%04X, clusterId: 0x%04X\n", qPrintable(event.node()->address().toStringExt()), event.profileId(), event.clusterId());

        // filter for supported sensor clusters
        switch (event.clusterId())
        {
        // sensor node?
        case POWER_CONFIGURATION_CLUSTER_ID:
        case ONOFF_CLUSTER_ID:
        case ONOFF_SWITCH_CONFIGURATION_CLUSTER_ID:
        case ILLUMINANCE_MEASUREMENT_CLUSTER_ID:
        case ILLUMINANCE_LEVEL_SENSING_CLUSTER_ID:
        case TEMPERATURE_MEASUREMENT_CLUSTER_ID:
        case RELATIVE_HUMIDITY_CLUSTER_ID:
        case PRESSURE_MEASUREMENT_CLUSTER_ID:
        case OCCUPANCY_SENSING_CLUSTER_ID:
        case IAS_ZONE_CLUSTER_ID:
        case BASIC_CLUSTER_ID:
        case ANALOG_INPUT_CLUSTER_ID:
        case MULTISTATE_INPUT_CLUSTER_ID:
        case BINARY_INPUT_CLUSTER_ID:
        case METERING_CLUSTER_ID:
        case ELECTRICAL_MEASUREMENT_CLUSTER_ID:
        case VENDOR_CLUSTER_ID:
        case WINDOW_COVERING_CLUSTER_ID:
            {
                addSensorNode(event.node(), &event);
                updateSensorNode(event);
            }
            break;

        default:
            break;
        }

        // filter for supported light clusters
        switch (event.clusterId())
        {
        // sensor node?
        case BASIC_CLUSTER_ID:
        case IDENTIFY_CLUSTER_ID:
        case ONOFF_CLUSTER_ID:
        case LEVEL_CLUSTER_ID:
        case GROUP_CLUSTER_ID:
        case SCENE_CLUSTER_ID:
        case COLOR_CLUSTER_ID:
        case WINDOW_COVERING_CLUSTER_ID:  // FIXME ubisys J1 is not a light
            {
                updateLightNode(event);
            }
            break;

        default:
            break;
        }
    }
        break;

    default:
        break;
    }
}

/*! Process task like add to group and remove from group.
 */
void DeRestPluginPrivate::processGroupTasks()
{
    if (nodes.empty())
    {
        return;
    }

    if (!isInNetwork())
    {
        return;
    }

    if (tasks.size() > MaxGroupTasks)
    {
        return;
    }

    if (groupTaskNodeIter >= nodes.size())
    {
        groupTaskNodeIter = 0;
    }

    TaskItem task;

    task.lightNode = &nodes[groupTaskNodeIter];
    groupTaskNodeIter++;

    if (!task.lightNode->isAvailable())
    {
        return;
    }

    // set destination parameters
    task.req.dstAddress() = task.lightNode->address();
//    task.req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
    task.req.setDstEndpoint(task.lightNode->haEndpoint().endpoint());
    task.req.setSrcEndpoint(getSrcEndpoint(task.lightNode, task.req));
    task.req.setDstAddressMode(deCONZ::ApsExtAddress);

    std::vector<GroupInfo>::iterator i = task.lightNode->groups().begin();
    std::vector<GroupInfo>::iterator end = task.lightNode->groups().end();

    for (; i != end; ++i)
    {
        if (i->actions & GroupInfo::ActionAddToGroup)
        {
            if (addTaskAddToGroup(task, i->id))
            {
                i->actions &= ~GroupInfo::ActionAddToGroup;
            }
            return;
        }

        if (i->actions & GroupInfo::ActionRemoveFromGroup)
        {
            if (addTaskRemoveFromGroup(task, i->id))
            {
                i->actions &= ~GroupInfo::ActionRemoveFromGroup;
            }
            return;
        }

        if (!i->addScenes.empty())
        {
            if (addTaskStoreScene(task, i->id, i->addScenes[0]))
            {
                processTasks();
            }
            return;
        }

        if (!i->removeScenes.empty())
        {
            if (addTaskRemoveScene(task, i->id, i->removeScenes[0]))
            {
                processTasks();
            }
            return;
        }

        for (const TaskItem &task : tasks)
        {
            if (task.taskType == TaskAddScene || task.taskType == TaskStoreScene)
            {
                // wait till tasks are processed
                return;
            }
        }

        for (const TaskItem &task : runningTasks)
        {
            if (task.taskType == TaskAddScene || task.taskType == TaskStoreScene)
            {
                // wait till tasks are processed
                return;
            }
        }

        if (!i->modifyScenes.empty())
        {
            if (i->modifyScenesRetries < GroupInfo::MaxActionRetries)
            {
                i->modifyScenesRetries++;

                bool needRead = false;
                Scene *scene = getSceneForId(i->id, i->modifyScenes[0]);

                if (scene)
                {
                    //const std::vector<LightState> &lights() const;
                    std::vector<LightState>::const_iterator ls = scene->lights().begin();
                    std::vector<LightState>::const_iterator lsend = scene->lights().end();

                    for (; ls != lsend; ++ls)
                    {
                        if (!ls->needRead())
                        {
                            continue;
                        }

                        if (ls->lid() == task.lightNode->id())
                        {
                            needRead = true;
                            if (readSceneAttributes(task.lightNode, i->id, scene->id))
                            {
                                return;
                            }
                        }
                    }
                }


                if (!needRead && addTaskAddScene(task, i->id, i->modifyScenes[0], task.lightNode->id()))
                {
                    processTasks();
                    return;
                }
            }
            else
            {
                i->modifyScenes.front() = i->modifyScenes.back();
                i->modifyScenes.pop_back();
                i->modifyScenesRetries = 0;
            }
        }
    }
}

/*! Handle packets related to the ZCL group cluster.
    \param task the task which belongs to this response
    \param ind the APS level data indication containing the ZCL packet
    \param zclFrame the actual ZCL frame which holds the groups cluster reponse
 */
void DeRestPluginPrivate::handleGroupClusterIndication(TaskItem &task, const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    Q_UNUSED(task);

    LightNode *lightNode = getLightNodeForAddress(ind.srcAddress(), ind.srcEndpoint());

    if (!lightNode)
    {
        return;
    }

    int endpointCount = getNumberOfEndpoints(lightNode->address().ext());

    if (zclFrame.isDefaultResponse())
    {
    }
    else if (zclFrame.commandId() == 0x02) // Get group membership response
    {
        DBG_Assert(zclFrame.payload().size() >= 2);

        QDataStream stream(zclFrame.payload());
        stream.setByteOrder(QDataStream::LittleEndian);

        uint8_t capacity;
        uint8_t count;

        stream >> capacity;
        stream >> count;

        lightNode->setGroupCapacity(capacity);
        lightNode->setGroupCount(count);

        DBG_Printf(DBG_INFO, "verified group capacity: %u and group count: %u of LightNode %s\n", capacity, count, qPrintable(lightNode->address().toStringExt()));

        QVector<quint16> responseGroups;
        for (uint i = 0; i < count; i++)
        {
            if (!stream.atEnd())
            {
                uint16_t groupId;
                stream >> groupId;

                responseGroups.push_back(groupId);

                DBG_Printf(DBG_INFO, "%s found group 0x%04X\n", qPrintable(lightNode->address().toStringExt()), groupId);

                foundGroup(groupId);
                foundGroupMembership(lightNode, groupId);
            }
        }

        std::vector<GroupInfo>::iterator i = lightNode->groups().begin();
        std::vector<GroupInfo>::iterator end = lightNode->groups().end();

        for (; i != end; ++i)
        {
            Group *group = getGroupForId(i->id);

            if (group && group->state() == Group::StateNormal
                && group->m_deviceMemberships.size() == 0 //no switch group
                && !responseGroups.contains(i->id)
                && i->state == GroupInfo::StateInGroup)
            {
                    DBG_Printf(DBG_INFO, "restore group  0x%04X for lightNode %s\n", i->id, qPrintable(lightNode->address().toStringExt()));
                    i->actions &= ~GroupInfo::ActionRemoveFromGroup; // sanity
                    i->actions |= GroupInfo::ActionAddToGroup;
                    i->state = GroupInfo::StateInGroup;
                    updateEtag(group->etag);
                    updateEtag(gwConfigEtag);
                    lightNode->setNeedSaveDatabase(true);
                    queSaveDb(DB_LIGHTS, DB_SHORT_SAVE_DELAY);
            }
            else if (group && group->state() == Group::StateNormal
                && group->m_deviceMemberships.size() > 0) //a switch group
            {
                if (responseGroups.contains(i->id)
                    && i->state == GroupInfo::StateNotInGroup) // light was added by a switch -> add it to deCONZ group)
                {
                    i->state = GroupInfo::StateInGroup;
                    std::vector<QString> &v = group->m_multiDeviceIds;
                    std::vector<QString>::iterator fi = std::find(v.begin(), v.end(), lightNode->id());
                    if (fi != v.end())
                    {
                        group->m_multiDeviceIds.erase(fi);
                        queSaveDb(DB_GROUPS, DB_SHORT_SAVE_DELAY);
                    }
                    updateEtag(group->etag);
                    updateEtag(gwConfigEtag);
                    lightNode->setNeedSaveDatabase(true);
                    queSaveDb(DB_LIGHTS, DB_SHORT_SAVE_DELAY);
                }
                else if (!responseGroups.contains(i->id)
                    && i->state == GroupInfo::StateInGroup) // light was removed from group by switch -> remove it from deCONZ group)
                {
                    i->state = GroupInfo::StateNotInGroup;
                    updateEtag(group->etag);
                    updateEtag(gwConfigEtag);
                    lightNode->setNeedSaveDatabase(true);
                    queSaveDb(DB_LIGHTS, DB_SHORT_SAVE_DELAY);
                }
            }
        }
    }
    else if (zclFrame.commandId() == 0x00) // Add group response
    {
        DBG_Assert(zclFrame.payload().size() >= 2);

        QDataStream stream(zclFrame.payload());
        stream.setByteOrder(QDataStream::LittleEndian);

        uint8_t status;
        uint16_t groupId;

        stream >> status;
        stream >> groupId;

        if (status == 0x00)
        {
            uint8_t capacity = lightNode->groupCapacity();
            if (capacity >= endpointCount)
            {
                capacity = capacity - endpointCount;
            }
            lightNode->setGroupCapacity(capacity);

            uint8_t count = lightNode->groupCount();
            if (count < 255)
            {
                count++;
            }
            lightNode->setGroupCount(count);
        }

        DBG_Printf(DBG_INFO, "Add to group response for light %s. Status:0x%02X, capacity: %u\n", qPrintable(lightNode->id()), status, lightNode->groupCapacity());

    }
    else if (zclFrame.commandId() == 0x03) // Remove group response
    {
        DBG_Assert(zclFrame.payload().size() >= 2);

        QDataStream stream(zclFrame.payload());
        stream.setByteOrder(QDataStream::LittleEndian);

        uint8_t status;
        uint16_t groupId;

        stream >> status;
        stream >> groupId;

        if (status == 0x00)
        {
            GroupInfo *groupInfo = getGroupInfo(lightNode, groupId);
            DBG_Assert(groupInfo != 0);

            if (groupInfo)
            {
                uint8_t sceneCount = groupInfo->sceneCount();
                uint8_t sceneCapacity = lightNode->sceneCapacity();

                if ((sceneCapacity + sceneCount) <= 255)
                {
                    sceneCapacity = sceneCapacity + sceneCount;
                }
                else
                {
                    sceneCapacity = 255;
                }
                lightNode->setSceneCapacity(sceneCapacity);

                uint8_t capacity = lightNode->groupCapacity();
                if ((capacity + endpointCount) <= 255)
                {
                    capacity = capacity + endpointCount;
                }
                lightNode->setGroupCapacity(capacity);

                uint8_t count = lightNode->groupCount();
                if (count > 0)
                {
                    count--;
                }
                lightNode->setGroupCount(count);
            }
        }

        DBG_Printf(DBG_INFO, "Remove from group response for light %s. Status: 0x%02X, capacity: %u\n", qPrintable(lightNode->id()), status, lightNode->groupCapacity());
    }
}

/*! Handle packets related to the ZCL scene cluster.
    \param task the task which belongs to this response
    \param ind the APS level data indication containing the ZCL packet
    \param zclFrame the actual ZCL frame which holds the scene cluster reponse
 */
void DeRestPluginPrivate::handleSceneClusterIndication(TaskItem &task, const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    Q_UNUSED(task);

    if (zclFrame.isDefaultResponse())
    {
    }
    else if (zclFrame.commandId() == 0x06) // Get scene membership response
    {
        if (zclFrame.payload().size() < 4)
        {
            DBG_Printf(DBG_INFO, "get scene membership response payload size too small %d\n", zclFrame.payload().size());
            return;
        }

        QDataStream stream(zclFrame.payload());
        stream.setByteOrder(QDataStream::LittleEndian);

        uint8_t status;
        stream >> status;

        if (status == deCONZ::ZclSuccessStatus && !stream.atEnd())
        {
            uint8_t capacity;
            uint16_t groupId;
            uint8_t count;

            stream >> capacity;
            stream >> groupId;
            stream >> count;

            DBG_Printf(DBG_INFO, "0x%016llX get scene membership response capacity %u, groupId 0x%04X, count %u\n", ind.srcAddress().ext(), capacity, groupId, count);

            Group *group = getGroupForId(groupId);
            LightNode *lightNode = getLightNodeForAddress(ind.srcAddress(), ind.srcEndpoint());
            GroupInfo *groupInfo = getGroupInfo(lightNode, group->address());

            if (group && lightNode && groupInfo && stream.status() != QDataStream::ReadPastEnd)
            {
                lightNode->setSceneCapacity(capacity);
                groupInfo->setSceneCount(count);

                std::vector<quint8> scenes;
                for (uint i = 0; i < count; i++)
                {
                    if (!stream.atEnd())
                    {
                        uint8_t sceneId;
                        stream >> sceneId;
                        scenes.push_back(sceneId);
                        foundScene(lightNode, group, sceneId);
                    }
                }

                std::vector<Scene>::iterator i = group->scenes.begin();
                std::vector<Scene>::iterator end = group->scenes.end();

                for (; i != end; ++i)
                {
                    if (i->state != Scene::StateNormal)
                    {
                        continue;
                    }

                    if (std::find(scenes.begin(), scenes.end(), i->id) != scenes.end())
                    {
                        continue; // exists
                    }

                    std::vector<LightState>::iterator st = i->lights().begin();
                    std::vector<LightState>::iterator stend = i->lights().end();

                    for (; st != stend; ++st)
                    {
                        if (st->lid() == lightNode->id())
                        {
                            DBG_Printf(DBG_INFO, "0x%016llX restore scene 0x%02X in group 0x%04X\n", lightNode->address().ext(), i->id, groupId);

                            std::vector<uint8_t> &v = groupInfo->modifyScenes;

                            if (std::find(v.begin(), v.end(), i->id) == v.end())
                            {
                                DBG_Printf(DBG_INFO, "0x%016llX start modify scene, groupId 0x%04X, scene 0x%02X\n", lightNode->address().ext(), groupInfo->id, i->id);
                                groupInfo->modifyScenes.push_back(i->id);
                            }
                        }
                    }
                }

                if (count > 0)
                {
                    lightNode->enableRead(READ_SCENE_DETAILS);
                }

                Q_Q(DeRestPlugin);
                q->startZclAttributeTimer(checkZclAttributesDelay);
            }
        }
    }
    else if (zclFrame.commandId() == 0x04 && (zclFrame.frameControl() & deCONZ::ZclFCDirectionServerToClient)) // Store scene response
    {
        if (zclFrame.payload().size() < 4)
        {
            DBG_Printf(DBG_INFO, "store scene response payload size too small %d\n", zclFrame.payload().size());
            return;
        }

        QDataStream stream(zclFrame.payload());
        stream.setByteOrder(QDataStream::LittleEndian);

        uint8_t status;
        uint16_t groupId;
        uint8_t sceneId;

        stream >> status;
        stream >> groupId;
        stream >> sceneId;

        LightNode *lightNode = getLightNodeForAddress(ind.srcAddress(), ind.srcEndpoint());

        if (lightNode)
        {
            GroupInfo *groupInfo = getGroupInfo(lightNode, groupId);

            if (groupInfo)
            {
                std::vector<uint8_t> &v = groupInfo->addScenes;
                std::vector<uint8_t>::iterator i = std::find(v.begin(), v.end(), sceneId);

                if (i != v.end())
                {
                    DBG_Printf(DBG_INFO, "Added/stored scene %u in node %s Response. Status: 0x%02X\n", sceneId, qPrintable(lightNode->address().toStringExt()), status);
                    groupInfo->addScenes.erase(i);

                    if (status == 0x00)
                    {
                        Scene *scene = getSceneForId(groupId, sceneId);

                        if (scene)
                        {
                            bool foundLightstate = false;

                            std::vector<LightState>::iterator li = scene->lights().begin();
                            std::vector<LightState>::iterator lend = scene->lights().end();
                            for (; li != lend; ++li)
                            {
                                if (li->lid() == lightNode->id())
                                {
                                    ResourceItem *item = lightNode->item(RStateOn);
                                    DBG_Assert(item != 0);
                                    if (item)
                                    {
                                        li->setOn(item->toBool());
                                    }
                                    item = lightNode->item(RStateBri);
                                    if (item)
                                    {
                                        li->setBri(item->toNumber());
                                    }
                                    item = lightNode->item(RStateColorMode);
                                    if (item)
                                    {
                                        li->setColorMode(item->toString());
                                        if (item->toString() == QLatin1String("xy") || item->toString() == QLatin1String("hs"))
                                        {
                                            item = lightNode->item(RStateX);
                                            if (item)
                                            {
                                                li->setX(item->toNumber());
                                            }
                                            item = lightNode->item(RStateY);
                                            if (item)
                                            {
                                                li->setY(item->toNumber());
                                            }
                                            item = lightNode->item(RStateHue);
                                            if (item)
                                            {
                                                li->setEnhancedHue(item->toNumber());
                                            }
                                            item = lightNode->item(RStateSat);
                                            if (item)
                                            {
                                                li->setSaturation(item->toNumber());
                                            }
                                        }
                                        else if (item->toString() == QLatin1String("ct"))
                                        {
                                            item = lightNode->item(RStateCt);
                                            DBG_Assert(item != 0);
                                            if (item)
                                            {
                                                li->setColorTemperature(item->toNumber());
                                            }
                                        }
                                        li->setColorloopActive(lightNode->isColorLoopActive());
                                        li->setColorloopTime(lightNode->colorLoopSpeed());
                                    }
                                    foundLightstate = true;
                                    break;
                                }
                            }

                            if (!foundLightstate)
                            {
                                LightState state;
                                state.setLightId(lightNode->id());
                                ResourceItem *item = lightNode->item(RStateOn);
                                DBG_Assert(item != 0);
                                if (item)
                                {
                                    state.setOn(item->toBool());
                                }
                                item = lightNode->item(RStateBri);
                                if (item)
                                {
                                    state.setBri(item->toNumber());
                                }
                                item = lightNode->item(RStateColorMode);
                                if (item)
                                {
                                    state.setColorMode(item->toString());
                                    if (item->toString() == QLatin1String("xy") || item->toString() == QLatin1String("hs"))
                                    {
                                        item = lightNode->item(RStateX);
                                        if (item)
                                        {
                                            state.setX(item->toNumber());
                                        }
                                        item = lightNode->item(RStateY);
                                        if (item)
                                        {
                                            state.setY(item->toNumber());
                                        }
                                        item = lightNode->item(RStateHue);
                                        if (item)
                                        {
                                            state.setEnhancedHue(item->toNumber());
                                        }
                                        item = lightNode->item(RStateSat);
                                        if (item)
                                        {
                                            state.setSaturation(item->toNumber());
                                        }
                                    }
                                    else if (item->toString() == QLatin1String("ct"))
                                    {
                                        item = lightNode->item(RStateCt);
                                        DBG_Assert(item != 0);
                                        if (item)
                                        {
                                            state.setColorTemperature(item->toNumber());
                                        }
                                    }
                                    state.setColorloopActive(lightNode->isColorLoopActive());
                                    state.setColorloopTime(lightNode->colorLoopSpeed());
                                }
                                scene->addLightState(state);

                                // only change capacity and count when creating a new scene
                                uint8_t sceneCapacity = lightNode->sceneCapacity();
                                if (sceneCapacity > 0)
                                {
                                    sceneCapacity--;
                                }
                                lightNode->setSceneCapacity(sceneCapacity);

                                uint8_t sceneCount = groupInfo->sceneCount();
                                if (sceneCount < 255)
                                {
                                    sceneCount++;
                                }
                                groupInfo->setSceneCount(sceneCount);

                                DBG_Printf(DBG_INFO, "scene capacity: %u\n", sceneCapacity);
                            }

                            queSaveDb(DB_SCENES, DB_SHORT_SAVE_DELAY);
                        }
                    }
                }
            }
        }
    }
    else if (zclFrame.commandId() == 0x02) // Remove scene response
    {
        if (zclFrame.payload().size() < 4)
        {
            DBG_Printf(DBG_INFO, "remove scene response payload size too small %d\n", zclFrame.payload().size());
            return;
        }

        QDataStream stream(zclFrame.payload());
        stream.setByteOrder(QDataStream::LittleEndian);

        uint8_t status;
        uint16_t groupId;
        uint8_t sceneId;

        stream >> status;
        stream >> groupId;
        stream >> sceneId;

        LightNode *lightNode = getLightNodeForAddress(ind.srcAddress(), ind.srcEndpoint());

        if (lightNode)
        {
            GroupInfo *groupInfo = getGroupInfo(lightNode, groupId);

            if (groupInfo)
            {
                std::vector<uint8_t> &v = groupInfo->removeScenes;
                std::vector<uint8_t>::iterator i = std::find(v.begin(), v.end(), sceneId);

                if (i != v.end())
                {
                    DBG_Printf(DBG_INFO, "Removed scene %u from node %s status 0x%02X\n", sceneId, qPrintable(lightNode->id()), status);
                    groupInfo->removeScenes.erase(i);

                    if (status == 0x00)
                    {
                        Scene *scene = getSceneForId(groupId, sceneId);

                        if (scene)
                        {
                            std::vector<LightState>::const_iterator li = scene->lights().begin();
                            std::vector<LightState>::const_iterator lend = scene->lights().end();
                            for (; li != lend; ++li)
                            {
                                if (li->lid() == lightNode->id())
                                {
                                    scene->deleteLight(lightNode->id());
                                    break;
                                }
                            }

                            queSaveDb(DB_SCENES,DB_SHORT_SAVE_DELAY);

                            uint8_t sceneCapacity = lightNode->sceneCapacity();
                            if (sceneCapacity < 255)
                            {
                                sceneCapacity++;
                            }
                            lightNode->setSceneCapacity(sceneCapacity);

                            uint8_t sceneCount = groupInfo->sceneCount();
                            if (sceneCount > 0)
                            {
                                sceneCount--;
                            }
                            groupInfo->setSceneCount(sceneCount);

                            DBG_Printf(DBG_INFO, "scene capacity: %u\n", sceneCapacity);
                        }
                    }
                }
            }
        }
    }
    else if (zclFrame.commandId() == 0x00 || zclFrame.commandId() == 0x40) // Add scene response | Enhanced add scene response
    {
        if (zclFrame.payload().size() < 4)
        {
            DBG_Printf(DBG_INFO, "add scene response payload size too small %d\n", zclFrame.payload().size());
            return;
        }

        QDataStream stream(zclFrame.payload());
        stream.setByteOrder(QDataStream::LittleEndian);

        uint8_t status;
        uint16_t groupId;
        uint8_t sceneId;

        stream >> status;
        stream >> groupId;
        stream >> sceneId;

        LightNode *lightNode = getLightNodeForAddress(ind.srcAddress(), ind.srcEndpoint());

        if (lightNode)
        {
            GroupInfo *groupInfo = getGroupInfo(lightNode, groupId);

            if (groupInfo)
            {
                std::vector<uint8_t> &v = groupInfo->modifyScenes;
                std::vector<uint8_t>::iterator i = std::find(v.begin(), v.end(), sceneId);

                if (i != v.end())
                {
                    DBG_Printf(DBG_INFO, "Modified scene %u in node %s status 0x%02X\n", sceneId, qPrintable(lightNode->address().toStringExt()), status);

                    if (status == deCONZ::ZclSuccessStatus)
                    {
                        groupInfo->modifyScenesRetries = 0;
                        groupInfo->modifyScenes.erase(i);
                    }
                    else if (status == deCONZ::ZclInsufficientSpaceStatus)
                    {
                    }
                }
            }
        }
    }
    else if (zclFrame.commandId() == 0x01 || zclFrame.commandId() == 0x41) // View scene response || Enhanced view scene response
    {
        if (zclFrame.payload().size() < 4)
        {
            DBG_Printf(DBG_INFO, "view scene response payload size too small %d\n", zclFrame.payload().size());
            return;
        }

        LightNode *lightNode = getLightNodeForAddress(ind.srcAddress(), ind.srcEndpoint());

        if (!lightNode)
        {
            return;
        }

        QDataStream stream(zclFrame.payload());
        stream.setByteOrder(QDataStream::LittleEndian);

        uint8_t status;

        stream >> status;
        if (status == 0x00 && !stream.atEnd())
        {
            uint16_t groupId;
            uint8_t sceneId;
            uint16_t transitionTime;
            uint8_t nameLength;

            stream >> groupId;
            stream >> sceneId;
            stream >> transitionTime;
            stream >> nameLength;

            Group *group = getGroupForId(groupId);
            Scene *scene = group->getScene(sceneId);

            if (!group)
            {
                return;
            }

            // discard scene name
            for (int i = 0; i < nameLength && !stream.atEnd(); i++)
            {
                quint8 c;
                stream >> c;
            }

            bool hasOnOff = false;
            bool hasBri = false;
            bool hasXY = false;
            bool hasHueSat = false;
            quint8 onOff;
            quint8 bri;
            quint16 x;
            quint16 y;
            quint16 ehue;
            quint8 sat;

            DBG_Printf(DBG_INFO_L2, "View scene rsp 0x%016llX group 0x%04X scene 0x%02X\n", lightNode->address().ext(), groupId, sceneId);

            while (!stream.atEnd())
            {
                uint16_t clusterId;
                uint8_t extLength; // extension

                stream >> clusterId;
                stream >> extLength;

                if (clusterId == 0x0006 && extLength >= 1)
                {
                    stream >> onOff;
                    extLength -= 1;
                    if ((onOff == 0x00 || onOff == 0x01) && stream.status() != QDataStream::ReadPastEnd)
                    {
                        hasOnOff = true;
                    }
                }
                else if (clusterId == 0x0008 && extLength >= 1)
                {
                    stream >> bri;
                    extLength -= 1;
                    if (stream.status() != QDataStream::ReadPastEnd)
                    {
                        hasBri = true;
                    }
                }
                else if (clusterId == 0x0300 && extLength >= 4)
                {
                    stream >> x;
                    stream >> y;
                    extLength -= 4;

                    if (x != 0 && y != 0 && stream.status() != QDataStream::ReadPastEnd)
                    {
                        hasXY = true;
                    }

                    if (extLength >= 3)
                    {
                        stream >> ehue;
                        stream >> sat;
                        extLength -= 4;

                        if (stream.status() != QDataStream::ReadPastEnd)
                        {
                            hasHueSat = true;
                        }
                    }
                }

                // discard unknown data
                while (extLength > 0)
                {
                    extLength--;
                    quint8 c;
                    stream >> c;
                }
            }

            DBG_Printf(DBG_INFO_L2, "\t t=%u, on=%u, bri=%u, x=%u, y=%u\n", transitionTime, onOff, bri, x, y);

            if (scene)
            {
                LightState *lightState = 0;
                std::vector<LightState>::iterator i = scene->lights().begin();
                std::vector<LightState>::iterator end = scene->lights().end();

                for (; i != end; ++i)
                {
                    if (i->lid() == lightNode->id())
                    {
                        lightState = &*i;
                        break;
                    }
                }

                if (scene->state == Scene::StateDeleted)
                {
                    // TODO
                }

                if (lightState)
                {
                    bool needModify = false;

                    // validate
                    if (hasOnOff && lightState->on() != onOff)
                    {
                        needModify = true;
                    }

                    if (hasBri && lightState->bri() != bri)
                    {
                        needModify = true;
                    }

                    if (hasXY && (lightState->x() != x || lightState->y() != y))
                    {
                        needModify = true;
                    }

                    if (hasHueSat && (lightState->enhancedHue() != ehue || lightState->saturation() != sat))
                    {
                        needModify = true;
                    }

                    if (lightState->needRead())
                    {
                        needModify = false;
                        lightState->setNeedRead(false);

                        if (hasOnOff)  { lightState->setOn(onOff); }
                        if (hasBri)    { lightState->setBri(bri); }
                        if (hasXY)
                        {
                            if (lightNode->modelId().startsWith(QLatin1String("FLS-H")) ||
                                lightNode->modelId().startsWith(QLatin1String("FLS-CT")) ||
                                lightNode->modelId().startsWith(QLatin1String("Ribag Air O")))
                            {
                                lightState->setColorTemperature(x);
                            }

                            lightState->setX(x);
                            lightState->setY(y);
                        }
                        if (hasHueSat) { lightState->setEnhancedHue(ehue); lightState->setSaturation(sat); }
                        lightState->tVerified.start();
                        queSaveDb(DB_SCENES, DB_LONG_SAVE_DELAY);

                        DBG_Printf(DBG_INFO_L2, "done reading scene scid=%u for %s\n", scene->id, qPrintable(lightNode->name()));
                    }

                    if (needModify)
                    {
                        if (!scene->externalMaster)
                        {
                            // TODO trigger add scene command to update scene
                        }
                        else // a switch might have changed settings
                        {
                            if (hasOnOff) { lightState->setOn(onOff); }
                            if (hasBri)   { lightState->setBri(bri); }
                            if (hasXY)    { lightState->setX(x); lightState->setY(y); }
                            if (hasHueSat) { lightState->setEnhancedHue(ehue); lightState->setSaturation(sat); }
                            lightState->tVerified.start();
                            queSaveDb(DB_SCENES, DB_LONG_SAVE_DELAY);
                        }
                    }
                    else
                    {
                        lightState->tVerified.start();
                    }
                }
                else
                {
                    LightState newLightState;
                    newLightState.setLightId(lightNode->id());
                    newLightState.setTransitionTime(transitionTime * 10);
                    newLightState.tVerified.start();
                    if (hasOnOff) { newLightState.setOn(onOff); }
                    if (hasBri)   { newLightState.setBri(bri); }
                    if (hasXY)
                    {
                        newLightState.setX(x);
                        newLightState.setY(y);

                        if (lightNode->modelId().startsWith(QLatin1String("FLS-H")) ||
                            lightNode->modelId().startsWith(QLatin1String("FLS-CT")) ||
                            lightNode->modelId().startsWith(QLatin1String("Ribag Air O")))
                        {
                            newLightState.setColorMode(QLatin1String("ct"));
                            newLightState.setColorTemperature(x);
                        }
                        else
                        {
                            newLightState.setColorMode(QLatin1String("xy"));
                        }
                    }
                    if (hasHueSat)
                    {
                        newLightState.setEnhancedHue(ehue);
                        newLightState.setSaturation(sat);
                    }
                    scene->addLightState(newLightState);
                    queSaveDb(DB_SCENES, DB_LONG_SAVE_DELAY);
                }
            }

            if (hasOnOff || hasBri || hasXY)
            {
                DBG_Printf(DBG_INFO_L2, "Validaded Scene (gid: %u, sid: %u) for Light %s\n", groupId, sceneId, qPrintable(lightNode->id()));
                DBG_Printf(DBG_INFO_L2, "On: %u, Bri: %u, X: %u, Y: %u, Transitiontime: %u\n",
                        onOff, bri, x, y, transitionTime);
            }
        }
    }
    else if (zclFrame.commandId() == 0x05 && !(zclFrame.frameControl() & deCONZ::ZclFCDirectionServerToClient)) // Recall scene command
    {
        // update Nodes and Groups state if Recall scene Command was send by a switch
        Sensor *sensorNode = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), ind.srcEndpoint());

        DBG_Assert(zclFrame.payload().size() >= 3);
        QDateTime now = QDateTime::currentDateTime();

        QDataStream stream(zclFrame.payload());
        stream.setByteOrder(QDataStream::LittleEndian);

        uint16_t groupId;
        uint8_t sceneId;

        stream >> groupId;
        stream >> sceneId;

        // notify via event
        QVariantMap map;
        map["t"] = QLatin1String("event");
        map["e"] = QLatin1String("scene-called");
        map["r"] = QLatin1String("scenes");
        map["gid"] = QString::number(groupId);
        map["scid"] = QString::number(sceneId);
        webSocketServer->broadcastTextMessage(Json::serialize(map));

        // check if scene exists

        bool colorloopDeactivated = false;
        Group *group = getGroupForId(groupId);
        Scene *scene = group ? group->getScene(sceneId) : 0;

        if (sensorNode && sensorNode->deletedState() == Sensor::StateNormal)
        {
            checkSensorNodeReachable(sensorNode);

            if (!scene && group && group->state() == Group::StateNormal)
            {
                Scene s;
                s.groupAddress = groupId;
                s.id = sceneId;
                s.externalMaster = true;
                s.name.sprintf("Scene %u", sceneId);
                group->scenes.push_back(s);
                updateGroupEtag(group);
                queSaveDb(DB_SCENES, DB_SHORT_SAVE_DELAY);
                DBG_Printf(DBG_INFO, "create scene %u from rx-command\n", sceneId);
            }
        }

        if (group && (group->state() == Group::StateNormal) && scene)
        {
            std::vector<LightState>::const_iterator ls = scene->lights().begin();
            std::vector<LightState>::const_iterator lsend = scene->lights().end();

            pollManager->delay(1500);
            for (; ls != lsend; ++ls)
            {
                LightNode *lightNode = getLightNodeForId(ls->lid());
                if (lightNode && lightNode->isAvailable() && lightNode->state() == LightNode::StateNormal)
                {
                    queuePollNode(lightNode);

                    bool changed = false;
                    if (lightNode->hasColor())
                    {
                        if (!ls->colorloopActive() && lightNode->isColorLoopActive() != ls->colorloopActive())
                        {
                            //stop colorloop if scene was saved without colorloop (Osram don't stop colorloop if another scene is called)
                            TaskItem task2;
                            task2.lightNode = lightNode;
                            task2.req.dstAddress() = task2.lightNode->address();
                            task2.req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
                            task2.req.setDstEndpoint(task2.lightNode->haEndpoint().endpoint());
                            task2.req.setSrcEndpoint(getSrcEndpoint(task2.lightNode, task2.req));
                            task2.req.setDstAddressMode(deCONZ::ApsExtAddress);

                            lightNode->setColorLoopActive(false);
                            addTaskSetColorLoop(task2, false, 15);

                            changed = true;
                            colorloopDeactivated = true;
                        }
                        //turn on colorloop if scene was saved with colorloop (FLS don't save colorloop at device)
                        else if (ls->colorloopActive() && lightNode->isColorLoopActive() != ls->colorloopActive())
                        {
                            TaskItem task2;
                            task2.lightNode = lightNode;
                            task2.req.dstAddress() = task2.lightNode->address();
                            task2.req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
                            task2.req.setDstEndpoint(task2.lightNode->haEndpoint().endpoint());
                            task2.req.setSrcEndpoint(getSrcEndpoint(task2.lightNode, task2.req));
                            task2.req.setDstAddressMode(deCONZ::ApsExtAddress);

                            lightNode->setColorLoopActive(true);
                            lightNode->setColorLoopSpeed(ls->colorloopTime());
                            addTaskSetColorLoop(task2, true, ls->colorloopTime());

                            changed = true;
                        }
                    }

                    // TODO let updates be handled to pollManager?
#if 0
                    ResourceItem *item = lightNode->item(RStateOn);
                    if (item && item->toBool() != ls->on())
                    {
                        item->setValue(ls->on());
                        Event e(RLights, RStateOn, lightNode->id(), item);
                        enqueueEvent(e);
                        changed = true;
                    }

                    item = lightNode->item(RStateBri);
                    if (item && ls->bri() != item->toNumber())
                    {
                        item->setValue(ls->bri());
                        Event e(RLights, RStateBri, lightNode->id(), item);
                        enqueueEvent(e);
                        changed = true;
                    }

                    item = lightNode->item(RStateColorMode);
                    if (item)
                    {
                        if (ls->colorMode() != item->toString())
                        {
                            item->setValue(ls->colorMode());
                            Event e(RLights, RStateColorMode, lightNode->id());
                            enqueueEvent(e);
                            changed = true;
                        }

                        if (ls->colorMode() == QLatin1String("xy"))
                        {
                            item = lightNode->item(RStateX);
                            if (item && ls->x() != item->toNumber())
                            {
                                item->setValue(ls->x());
                                Event e(RLights, RStateX, lightNode->id(), item);
                                enqueueEvent(e);
                                changed = true;
                            }
                            item = lightNode->item(RStateY);
                            if (item && ls->y() != item->toNumber())
                            {
                                item->setValue(ls->y());
                                Event e(RLights, RStateY, lightNode->id(), item);
                                enqueueEvent(e);
                                changed = true;
                            }
                        }
                        else if (ls->colorMode() == QLatin1String("ct"))
                        {
                            item = lightNode->item(RStateCt);
                            if (item && ls->colorTemperature() != item->toNumber())
                            {
                                item->setValue(ls->colorTemperature());
                                Event e(RLights, RStateCt, lightNode->id(), item);
                                enqueueEvent(e);
                                changed = true;
                            }
                        }
                        else if (ls->colorMode() == QLatin1String("hs"))
                        {
                            item = lightNode->item(RStateHue);
                            if (item && ls->enhancedHue() != item->toNumber())
                            {
                                item->setValue(ls->enhancedHue());
                                Event e(RLights, RStateHue, lightNode->id(), item);
                                enqueueEvent(e);
                                changed = true;
                            }

                            item = lightNode->item(RStateSat);
                            if (item && ls->saturation() != item->toNumber())
                            {
                                item->setValue(ls->saturation());
                                Event e(RLights, RStateSat, lightNode->id(), item);
                                enqueueEvent(e);
                                changed = true;
                            }
                        }
                    }
#endif
                    if (changed)
                    {
                        updateLightEtag(lightNode);
                    }
                }
            }

            //recall scene again
            if (colorloopDeactivated)
            {
                callScene(group, sceneId);
            }
        }
        // turning 'on' the group is also a assumtion but a very likely one
        if (group && !group->isOn())
        {
            group->setIsOn(true);
            updateGroupEtag(group);
        }

        updateEtag(gwConfigEtag);
        processTasks();
    }
}

/*! Handle packets related to the ZCL On/Off cluster.
    \param task the task which belongs to this response
    \param ind the APS level data indication containing the ZCL packet
    \param zclFrame the actual ZCL frame which holds the scene cluster reponse
 */
void DeRestPluginPrivate::handleOnOffClusterIndication(TaskItem &task, const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    Q_UNUSED(task);

    if (zclFrame.isDefaultResponse())
    {
        return;
    }

    bool dark = true;
    Group *group = 0;

    if (ind.dstAddressMode() == deCONZ::ApsGroupAddress)
    {
        group = getGroupForId(ind.dstAddress().group());
    }

    if (zclFrame.commandId() == 0x42) // on with timed off
    {
        for (Sensor &s : sensors)
        {
            if ((s.address().hasExt() && s.address().ext() == ind.srcAddress().ext()) ||
                (s.address().hasNwk() && s.address().nwk() == ind.srcAddress().nwk()))
            {
                if (!s.type().endsWith(QLatin1String("Presence")))
                {
                     continue;
                }
                ResourceItem *item;
                quint64 delay = 0;

                if (s.modelId() == QLatin1String("TRADFRI motion sensor") && zclFrame.payload().size() >= 3)
                {
                    // Set ikea motion sensor config.delay and state.dark from the ZigBee command parameters
                    dark = zclFrame.payload().at(0) == 0x00;
                    quint16 timeOn = (zclFrame.payload().at(2) << 8) + zclFrame.payload().at(1);
                    delay = (timeOn + 5) / 10;

                    item = s.item(RConfigDelay);
                    if (!item)
                    {
                        item = s.addItem(DataTypeUInt16, RConfigDelay);
                    }
                    if (item)
                    {
                        item->setValue(delay);
                        Event e(RSensors, RConfigDelay, s.id(), item);
                        enqueueEvent(e);
                    }

                    item = s.item(RStateDark);
                    if (!item)
                    {
                        item = s.addItem(DataTypeBool, RStateDark);
                    }
                    if (item)
                    {
                        item->setValue(dark);
                        Event e(RSensors, RStateDark, s.id(), item);
                        enqueueEvent(e);
                    }
                }

                if (!s.isAvailable())
                {
                    checkSensorNodeReachable(&s);
                }

                s.incrementRxCounter();
                item = s.item(RStatePresence);
                if (item)
                {
                    item->setValue(true);
                    s.updateStateTimestamp();
                    updateSensorEtag(&s);
                    Event e(RSensors, RStatePresence, s.id(), item);
                    enqueueEvent(e);
                    enqueueEvent(Event(RSensors, RStateLastUpdated, s.id()));
                }
                item = s.item(RConfigDuration);
                if (item && item->toNumber() > 0)
                {
                    s.durationDue = QDateTime::currentDateTime().addSecs(item->toNumber());
                }
                else if (delay > 0)
                {
                    s.durationDue = QDateTime::currentDateTime().addSecs(delay);
                }
            }
        }
    }

    // update Nodes and Groups state if On/Off Command was send by a sensor
    if (dark && group &&
        group->state() != Group::StateDeleted &&
        group->state() != Group::StateDeleteFromDB)
    {
        //found
        if (zclFrame.commandId() == 0x00 || zclFrame.commandId() == 0x40) // Off || Off with effect
        {
            group->setIsOn(false);
        }
        else if (zclFrame.commandId() == 0x01 || zclFrame.commandId() == 0x42) // On || On with timed off
        {
            group->setIsOn(true);
            if (group->isColorLoopActive())
            {
                TaskItem task1;
                task1.req.dstAddress().setGroup(group->address());
                task1.req.setDstAddressMode(deCONZ::ApsGroupAddress);
                task1.req.setDstEndpoint(0xFF); // broadcast endpoint
                task1.req.setSrcEndpoint(getSrcEndpoint(0, task1.req));

                addTaskSetColorLoop(task1, false, 15);
                group->setColorLoopActive(false);
            }
        }
        updateGroupEtag(group);

        // check each light if colorloop needs to be disabled
        std::vector<LightNode>::iterator l = nodes.begin();
        std::vector<LightNode>::iterator lend = nodes.end();

        for (; l != lend; ++l)
        {
            if ((zclFrame.frameControl() & deCONZ::ZclFCClusterCommand) &&
                 isLightNodeInGroup(&*l, group->address()))
            {
                bool updated = false;
                if (zclFrame.commandId() == 0x00 || zclFrame.commandId() == 0x40) // Off || Off with effect
                {
                    ResourceItem *item = l->item(RStateOn);
                    if (item && item->toBool())
                    {
                        item->setValue(false);
                        Event e(RLights, RStateOn, l->id(), item);
                        enqueueEvent(e);
                        updated = true;
                    }
                }
                else if (zclFrame.commandId() == 0x01 || zclFrame.commandId() == 0x42) // On || On with timed off
                {
                    ResourceItem *item = l->item(RStateOn);
                    if (item && !item->toBool())
                    {
                        item->setValue(true);
                        Event e(RLights, RStateOn, l->id(), item);
                        enqueueEvent(e);
                        updated = true;
                    }

                    if (l->isAvailable() && l->hasColor() && l->state() != LightNode::StateDeleted && l->isColorLoopActive())
                    {
                        TaskItem task2;
                        task2.lightNode = &(*l);
                        task2.req.dstAddress() = task2.lightNode->address();
                        task2.req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
                        task2.req.setDstEndpoint(task2.lightNode->haEndpoint().endpoint());
                        task2.req.setSrcEndpoint(getSrcEndpoint(task2.lightNode, task2.req));
                        task2.req.setDstAddressMode(deCONZ::ApsExtAddress);

                        addTaskSetColorLoop(task2, false, 15);
                        l->setColorLoopActive(false);
                        updated = true;
                    }
                }

                if (updated)
                {
                    updateLightEtag(&*l);
                }
            }
        }

        updateEtag(gwConfigEtag);
    }
}

/*! Handle packets related to the ZCL Commissioning cluster.
    \param task the task which belongs to this response
    \param ind the APS level data indication containing the ZCL packet
    \param zclFrame the actual ZCL frame which holds the Commissioning cluster reponse
 */
void DeRestPluginPrivate::handleCommissioningClusterIndication(TaskItem &task, const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    Q_UNUSED(task);

    uint8_t ep = ind.srcEndpoint();
    Sensor *sensorNode = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), ind.srcEndpoint());
    int epIter = 0;

    if (!sensorNode)
    {
        for (Sensor &s : sensors)
        {
            if (s.deletedState() != Sensor::StateNormal)
            {
                continue;
            }

            if ((ind.srcAddress().hasExt() && ind.srcAddress().ext() == s.address().ext()) ||
                (ind.srcAddress().hasNwk() && ind.srcAddress().nwk() == s.address().nwk()))
            {
                if (s.modelId().startsWith(QLatin1String("RWL02")))
                {
                    sensorNode = &s;
                }
            }

            if (sensorNode)
            {
                break;
            }
        }
    }

    if (!sensorNode)
    {
        return;
    }

    if (zclFrame.isDefaultResponse())
    {
    }
    else if (zclFrame.commandId() == 0x41) // Get group identifiers response
    {
        DBG_Assert(zclFrame.payload().size() >= 4);

        QDataStream stream(zclFrame.payload());
        stream.setByteOrder(QDataStream::LittleEndian);

        uint8_t total;
        uint8_t startIndex;
        uint8_t count;
        uint16_t groupId;
        uint8_t type;

        stream >> total;
        stream >> startIndex;
        stream >> count;

        DBG_Printf(DBG_INFO, "Get group identifiers response of sensor %s. Count: %u\n", qPrintable(sensorNode->address().toStringExt()), count);

        while (!stream.atEnd() && epIter < count)
        {
            stream >> groupId;
            stream >> type;

            if (stream.status() == QDataStream::ReadPastEnd)
            {
                break;
            }

            DBG_Printf(DBG_INFO, "\tgroup: 0x%04X, type: %u\n", groupId, type);

            if (epIter < count && ep != ind.srcEndpoint())
            {
                sensorNode = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), ep);
                if (!sensorNode)
                {
                    sensorNode = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), ind.srcEndpoint());
                }
            }
            epIter++;
            // assumption: different groups from consecutive endpoints
            ep++;

            if (sensorNode && sensorNode->deletedState() == Sensor::StateNormal)
            {
                sensorNode->clearRead(READ_GROUP_IDENTIFIERS);
                Group *group1 = getGroupForId(groupId);

                if (!group1)
                {
                    foundGroup(groupId);
                    group1 = getGroupForId(groupId);

                    if (group1)
                    {
                        group1->setName(QString("%1 %2").arg(sensorNode->modelId()).arg(groups.size()));
                    }
                }

                if (group1)
                {
                    //not found?
                    if (group1->addDeviceMembership(sensorNode->id()) || group1->state() == Group::StateDeleted)
                    {
                        group1->setState(Group::StateNormal);
                        queSaveDb(DB_GROUPS, DB_SHORT_SAVE_DELAY);
                        updateGroupEtag(group1);
                    }
                }

                ResourceItem *item = sensorNode->addItem(DataTypeString, RConfigGroup);
                QString gid = QString::number(groupId);

                if (item->toString() != gid)
                {
                    DBG_Printf(DBG_INFO, "\tupdate group item: 0x%04X\n", groupId);
                    item->setValue(gid);
                    sensorNode->setNeedSaveDatabase(true);
                    queSaveDb(DB_GROUPS | DB_SENSORS, DB_SHORT_SAVE_DELAY);
                }

                Event e(RSensors, REventValidGroup, sensorNode->id());
                enqueueEvent(e);
                enqueueEvent(Event(RSensors, RConfigGroup, sensorNode->id(), item));
            }
        }
    }
}

/*! Handle the case that a node send a ZDP command.
    \param ind a ZDP command
 */
void DeRestPluginPrivate::handleZdpIndication(const deCONZ::ApsDataIndication &ind)
{
    for (LightNode &lightNode: nodes)
    {
        if (ind.srcAddress().hasExt() && ind.srcAddress().ext() != lightNode.address().ext())
        {
            continue;
        }

        if (ind.srcAddress().hasNwk() && ind.srcAddress().nwk() != lightNode.address().nwk())
        {
            continue;
        }

        lightNode.rx();

        if (lightNode.modelId().isEmpty() && lightNode.haEndpoint().isValid())
        {
            std::vector<uint16_t> attributes;
            attributes.push_back(0x0005); // Model identifier

            if (readAttributes(&lightNode, lightNode.haEndpoint().endpoint(), BASIC_CLUSTER_ID, attributes))
            {
                lightNode.clearRead(READ_MODEL_ID);
            }
        }

        if (lightNode.modelId().startsWith(QLatin1String("FLS-NB")))
        {
            for (Sensor &s: sensors)
            {
                if (s.address().ext() != lightNode.address().ext())
                {
                    continue;
                }

                if (!s.node() && lightNode.node())
                {
                    s.setNode(lightNode.node());
                }

                if (s.isAvailable())
                {
                    continue;
                }

                checkSensorNodeReachable(&s);
            }
        }
    }
}

/*! Handle the case that a node (re)joins the network.
    \param ind a ZDP DeviceAnnce_req
 */
void DeRestPluginPrivate::handleDeviceAnnceIndication(const deCONZ::ApsDataIndication &ind)
{
    std::vector<LightNode>::iterator i = nodes.begin();
    std::vector<LightNode>::iterator end = nodes.end();

    quint16 nwk;
    quint64 ext;
    quint8 macCapabilities;

    {
        QDataStream stream(ind.asdu());
        stream.setByteOrder(QDataStream::LittleEndian);

        quint8 seq;

        stream >> seq;
        stream >> nwk;
        stream >> ext;
        stream >> macCapabilities;
    }

    for (; i != end; ++i)
    {
        deCONZ::Node *node = i->node();
        if (node && (i->address().ext() == ext))
        {
            i->rx();

            // clear to speedup polling
            for (NodeValue &val : i->zclValues())
            {
                val.timestamp = QDateTime();
                val.timestampLastReport = QDateTime();
                val.timestampLastConfigured = QDateTime();
            }

            std::vector<RecoverOnOff>::iterator rc = recoverOnOff.begin();
            std::vector<RecoverOnOff>::iterator rcend = recoverOnOff.end();
            for (; rc != rcend; ++rc)
            {
                if (rc->address.ext() == ext || rc->address.nwk() == nwk)
                {
                    rc->idleTotalCounterCopy -= 60; // speedup release
                    // light was off before, turn off again
                    if (!rc->onOff)
                    {
                        DBG_Printf(DBG_INFO, "Turn off light 0x%016llX again after powercycle\n", rc->address.ext());
                        TaskItem task;
                        task.lightNode = &*i;
                        task.req.dstAddress().setNwk(nwk);
                        task.req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
                        task.req.setDstEndpoint(task.lightNode->haEndpoint().endpoint());
                        task.req.setSrcEndpoint(getSrcEndpoint(task.lightNode, task.req));
                        task.req.setDstAddressMode(deCONZ::ApsNwkAddress);
                        task.req.setSendDelay(1000);
                        queryTime = queryTime.addSecs(5);
                        addTaskSetOnOff(task, ONOFF_COMMAND_OFF, 0);
                    }
                    else if (rc->bri > 0 && rc->bri < 256)
                    {
                        DBG_Printf(DBG_INFO, "Turn on light 0x%016llX on again with former brightness after powercycle\n", rc->address.ext());
                        TaskItem task;
                        task.lightNode = &*i;
                        task.req.dstAddress().setNwk(nwk);
                        task.req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
                        task.req.setDstEndpoint(task.lightNode->haEndpoint().endpoint());
                        task.req.setSrcEndpoint(getSrcEndpoint(task.lightNode, task.req));
                        task.req.setDstAddressMode(deCONZ::ApsNwkAddress);
                        task.req.setSendDelay(1000);
                        queryTime = queryTime.addSecs(5);
                        addTaskSetBrightness(task, rc->bri, true);
                    }
                    break;
                }
            }

            if (node->endpoints().end() == std::find(node->endpoints().begin(),
                                                     node->endpoints().end(),
                                                     i->haEndpoint().endpoint()))
            {
                continue; // not a active endpoint
            }

            ResourceItem *item = i->item(RStateReachable);

            if (item)
            {
                if (gwPermitJoinDuration > 0)
                {
                    if (i->state() == LightNode::StateDeleted)
                    {
                        i->setState(LightNode::StateNormal);
                        i->setNeedSaveDatabase(true);
                        queSaveDb(DB_LIGHTS,DB_SHORT_SAVE_DELAY);
                    }
                }

                item->setValue(true); // refresh timestamp after device announce
                if (i->state() == LightNode::StateNormal)
                {
                    Event e(i->prefix(), RStateReachable, i->id(), item);
                    enqueueEvent(e);
                }

                updateEtag(gwConfigEtag);
            }

            DBG_Printf(DBG_INFO, "DeviceAnnce of LightNode: %s Permit Join: %i\n", qPrintable(i->address().toStringExt()), gwPermitJoinDuration);

            if (i->state() == LightNode::StateNormal)
            {
                // force reading attributes
                i->enableRead(READ_GROUPS | READ_SCENES);

                queuePollNode(&*i);

                // reorder, bring to back to force next polling
                auto n = std::find(pollNodes.begin(), pollNodes.end(), &*i);
                if (n != pollNodes.end())
                {
                    *n = pollNodes.back();
                    pollNodes.back() = &*i;
                }

                for (uint32_t ii = 0; ii < 32; ii++)
                {
                    uint32_t item = 1 << ii;
                    if (i->mustRead(item))
                    {
                        i->setNextReadTime(item, queryTime);
                        i->setLastRead(item, idleTotalCounter);
                    }
                }

                queryTime = queryTime.addSecs(1);
                updateEtag(i->etag);
            }
        }
    }

    int found = 0;
    std::vector<Sensor>::iterator si = sensors.begin();
    std::vector<Sensor>::iterator send = sensors.end();

    for (; si != send; ++si)
    {
        if (si->deletedState() != Sensor::StateNormal)
        {
            continue;
        }

        if (si->address().ext() == ext)
        {
            si->rx();
            found++;
            DBG_Printf(DBG_INFO, "DeviceAnnce of SensorNode: 0x%016llX [1]\n", si->address().ext());

            ResourceItem *item = si->item(RConfigReachable);
            if (item)
            {
                item->setValue(true); // refresh timestamp after device announce
                Event e(si->prefix(), RConfigReachable, si->id(), item);
                enqueueEvent(e);
            }
            checkSensorGroup(&*si);
            checkSensorBindingsForAttributeReporting(&*si);
            checkSensorBindingsForClientClusters(&*si);
            updateSensorEtag(&*si);

            if (searchSensorsState == SearchSensorsActive && si->node())
            {
                // address changed?
                if (si->address().nwk() != nwk)
                {
                    DBG_Printf(DBG_INFO, "\tnwk address changed 0x%04X -> 0x%04X [2]\n", si->address().nwk(), nwk);
                    // indicator that the device was resettet
                    si->address().setNwk(nwk);

                    if (searchSensorsState == SearchSensorsActive &&
                        si->deletedState() == Sensor::StateNormal)
                    {
                        updateSensorEtag(&*si);
                        Event e(RSensors, REventAdded, si->id());
                        enqueueEvent(e);
                    }
                }

                addSensorNode(si->node()); // check if somethings needs to be updated
            }
        }
    }

    if (searchSensorsState == SearchSensorsActive)
    {
        if (!found && apsCtrl)
        {
            int i = 0;
            const deCONZ::Node *node;

            // try to add sensor nodes even if they existed in deCONZ bevor and therefore
            // no node added event will be triggert in this phase
            while (apsCtrl->getNode(i, &node) == 0)
            {
                if (ext == node->address().ext())
                {
                    addSensorNode(node);
                    break;
                }
                i++;
            }
        }

        deCONZ::ZclFrame zclFrame; // dummy
        handleIndicationSearchSensors(ind, zclFrame);
    }
}

/*! Handle mgmt lqi response.
    \param ind a ZDP MgmtLqi_rsp
 */
void DeRestPluginPrivate::handleMgmtLqiRspIndication(const deCONZ::ApsDataIndication &ind)
{
    quint8 zdpSeq;
    quint8 zdpStatus;
    quint8 neighEntries;
    quint8 startIndex;
    quint8 listCount;

    QDataStream stream(ind.asdu());
    stream.setByteOrder(QDataStream::LittleEndian);

    stream >> zdpSeq;
    stream >> zdpStatus;
    stream >> neighEntries;
    stream >> startIndex;
    stream >> listCount;

    if (stream.status() == QDataStream::ReadPastEnd)
    {
        return;
    }

    if ((startIndex + listCount) >= neighEntries || listCount == 0)
    {
        // finish
        for (LightNode &l : nodes)
        {
            if (l.address().ext() == ind.srcAddress().ext())
            {
                l.rx();
            }
        }
    }
}

/*! Handle IEEE address request indication.
    \param ind a ZDP IeeeAddress_req
 */
void DeRestPluginPrivate::handleIeeeAddressReqIndication(const deCONZ::ApsDataIndication &ind)
{
    if (!apsCtrl)
    {
        return;
    }

    quint8 seq;
    quint64 extAddr;
    quint16 nwkAddr;
    quint8 reqType;
    quint8 startIndex;

    {
        QDataStream stream(ind.asdu());
        stream.setByteOrder(QDataStream::LittleEndian);

        stream >> seq;
        stream >> nwkAddr;
        stream >> reqType;
        stream >> startIndex;
    }

    if (nwkAddr != apsCtrl->getParameter(deCONZ::ParamNwkAddress))
    {
        return;
    }

    deCONZ::ApsDataRequest req;

    req.setProfileId(ZDP_PROFILE_ID);
    req.setSrcEndpoint(ZDO_ENDPOINT);
    req.setDstEndpoint(ZDO_ENDPOINT);
    req.setClusterId(ZDP_IEEE_ADDR_RSP_CLID);
    req.setDstAddressMode(deCONZ::ApsNwkAddress);
    req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
    req.dstAddress() = ind.srcAddress();

    QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    extAddr = apsCtrl->getParameter(deCONZ::ParamMacAddress);
    // trick OTA client of busch jaeger switch to think this is a bj dongle
    if ((ind.srcAddress().ext() & macPrefixMask) == bjeMacPrefix)
    {
        extAddr &= ~macPrefixMask;
        extAddr |= bjeMacPrefix;
    }

    quint8 status = ZDP_SUCCESS;
    stream << seq;
    stream << status;
    stream << extAddr;
    stream << nwkAddr;

    if (reqType == 0x01) // extended request type
    {
        stream << (quint8)0; // num of assoc devices
        stream << (quint8)0; // start index
    }

    if (apsCtrl->apsdeDataRequest(req) == deCONZ::Success)
    {

    }
}

/*! Handle NWK address request indication.
    \param ind a ZDP NwkAddress_req
 */
void DeRestPluginPrivate::handleNwkAddressReqIndication(const deCONZ::ApsDataIndication &ind)
{
    if (!apsCtrl)
    {
        return;
    }

    quint8 seq;
    quint16 nwkAddr;
    quint64 extAddr;
    quint8 reqType;
    quint8 startIndex;

    {
        QDataStream stream(ind.asdu());
        stream.setByteOrder(QDataStream::LittleEndian);

        stream >> seq;
        stream >> extAddr;
        stream >> reqType;
        stream >> startIndex;
    }

    if (extAddr != apsCtrl->getParameter(deCONZ::ParamMacAddress))
    {
        return;
    }

    deCONZ::ApsDataRequest req;

    req.setProfileId(ZDP_PROFILE_ID);
    req.setSrcEndpoint(ZDO_ENDPOINT);
    req.setDstEndpoint(ZDO_ENDPOINT);
    req.setClusterId(ZDP_NWK_ADDR_RSP_CLID);
    req.setDstAddressMode(deCONZ::ApsNwkAddress);
    req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
    req.dstAddress() = ind.srcAddress();

    QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    nwkAddr = apsCtrl->getParameter(deCONZ::ParamNwkAddress);
    quint8 status = ZDP_SUCCESS;
    stream << seq;
    stream << status;
    stream << extAddr;
    stream << nwkAddr;

    if (reqType == 0x01) // extended request type
    {
        stream << (quint8)0; // num of assoc devices
        stream << (quint8)0; // start index
    }

    if (apsCtrl->apsdeDataRequest(req) == deCONZ::Success)
    {

    }
}

/*! Push data from a task into all LightNodes of a group or single LightNode.
 */
void DeRestPluginPrivate::taskToLocalData(const TaskItem &task)
{
    Group *group;
    Group dummyGroup;
    std::vector<LightNode*> pushNodes;

    if (task.req.clusterId() == 0xffff)
    {
        return;
    }

    if (task.req.dstAddress().hasGroup() || task.req.dstAddress().isNwkBroadcast())
    {
        group = getGroupForId(task.req.dstAddress().group());

        DBG_Assert(group != 0);

        if (!group)
        {
            group = &dummyGroup;
        }

        std::vector<LightNode>::iterator i = nodes.begin();
        std::vector<LightNode>::iterator end = nodes.end();

        for (; i != end; ++i)
        {
            LightNode *lightNode = &(*i);
            if (isLightNodeInGroup(lightNode, task.req.dstAddress().group()) || group->id() == "0")
            {
                pushNodes.push_back(lightNode);
            }
        }
    }
    else if (task.lightNode)
    {
        group = &dummyGroup; // never mind
        pushNodes.push_back(task.lightNode);
    }
    else if (task.req.dstAddress().hasExt())
    {
        group = &dummyGroup; // never mind
        LightNode *lightNode = getLightNodeForAddress(task.req.dstAddress(), task.req.dstEndpoint());
        if (lightNode)
        {
            pushNodes.push_back(lightNode);
        }
    }
    else
    {
        return;
    }

    std::vector<LightNode*>::iterator i = pushNodes.begin();
    std::vector<LightNode*>::iterator end = pushNodes.end();

    switch (task.taskType)
    {
    case TaskSendOnOffToggle:
        updateEtag(group->etag);
        group->setIsOn(task.onOff);

        if (!task.lightNode && group->id() == "0")
        {
            std::vector<Group>::iterator g = groups.begin();
            std::vector<Group>::iterator gend = groups.end();

            for (; g != gend; ++g)
            {
                if (g->state() != Group::StateDeleted && g->state() != Group::StateDeleteFromDB)
                {
                    updateEtag(g->etag);
                    g->setIsOn(task.onOff);
                }
            }
        }

        break;

    case TaskSetLevel:
        if (task.level > 0)
        {
            group->setIsOn(true);
        }
        else
        {
            group->setIsOn(false);
        }
        updateGroupEtag(group);
        group->level = task.level;
        break;

    case TaskSetSat:
        updateGroupEtag(group);
        group->sat = task.sat;
        break;

    case TaskSetEnhancedHue:
        updateGroupEtag(group);
        group->hue = task.hue;
        group->hueReal = task.hueReal;
        break;

    case TaskSetHueAndSaturation:
        updateGroupEtag(group);
        group->sat = task.sat;
        group->hue = task.hue;
        group->hueReal = task.hueReal;
        break;

    case TaskSetXyColor:
        updateGroupEtag(group);
        group->colorX = task.colorX;
        group->colorY = task.colorY;
        break;

    case TaskIncColorTemperature:
    {
        qint32 modCt = group->colorTemperature + task.inc;
        // clip, TODO use phys. min. max. values from lights
        if (modCt < 153) { modCt = 153; }
        else if (modCt > 500) { modCt = 500; }
        if (group->colorTemperature == modCt)
        {
            group->colorTemperature = modCt;
            updateGroupEtag(group);
        }
    }
        break;

    case TaskIncBrightness:
        break;

    case TaskSetColorTemperature:
        updateGroupEtag(group);
        group->colorTemperature = task.colorTemperature;
        break;

    case TaskSetColorLoop:
        updateGroupEtag(group);
        group->setColorLoopActive(task.colorLoop);
        break;

    default:
        break;
    }

    for (; i != end; ++i)
    {
        LightNode *lightNode = *i;

        if (!lightNode->isAvailable())
        {
            continue;
        }

        switch (task.taskType)
        {
        case TaskSendOnOffToggle:
        {
            ResourceItem *item = lightNode->item(RStateOn);
            if (item && item->toBool() != task.onOff)
            {
                updateLightEtag(lightNode);
                item->setValue(task.onOff);
                Event e(RLights, RStateOn, lightNode->id(), item);
                enqueueEvent(e);
            }
            setAttributeOnOff(lightNode);
        }
            break;

        case TaskSetLevel:
        {
            ResourceItem *item = lightNode->item(RStateOn);
            if (item && item->toBool() != (task.level > 0))
            {
                updateLightEtag(lightNode);
                item->setValue(task.level > 0);
                Event e(RLights, RStateOn, lightNode->id(), item);
                enqueueEvent(e);
            }

            item = lightNode->item(RStateBri);
            if (item && item->toNumber() != task.level)
            {
                updateLightEtag(lightNode);
                item->setValue(task.level);
                Event e(RLights, RStateBri, lightNode->id(), item);
                enqueueEvent(e);
            }

            setAttributeLevel(lightNode);
            setAttributeOnOff(lightNode);
        }
            break;

        case TaskStopLevel:
            updateEtag(lightNode->etag);
            lightNode->enableRead(READ_LEVEL);
            lightNode->mustRead(READ_LEVEL);
            break;

        case TaskSetSat:
        {
            ResourceItem *item = lightNode->item(RStateSat);
            if (item && item->toNumber() != task.sat)
            {
                updateLightEtag(lightNode);
                item->setValue(task.sat);
                Event e(RLights, RStateSat, lightNode->id(), item);
                enqueueEvent(e);
            }

            item = item ? lightNode->item(RStateColorMode) : 0; // depend on sat
            if (item && item->toString() != QLatin1String("hs"))
            {
                item->setValue(QVariant(QLatin1String("hs")));
                Event e(RLights, RStateColorMode, lightNode->id());
                enqueueEvent(e);
            }

            setAttributeSaturation(lightNode);
        }
            break;

        case TaskSetEnhancedHue:
        {
            lightNode->setEnhancedHue(task.enhancedHue);

            ResourceItem *item = lightNode->item(RStateHue);
            if (item && item->toNumber() != task.enhancedHue)
            {
                updateLightEtag(lightNode);
                item->setValue(task.enhancedHue);
                Event e(RLights, RStateHue, lightNode->id(), item);
                enqueueEvent(e);
            }

            item = item ? lightNode->item(RStateColorMode) : 0; // depend on hue
            if (item && item->toString() != QLatin1String("hs"))
            {
                item->setValue(QVariant(QLatin1String("hs")));
                Event e(RLights, RStateColorMode, lightNode->id());
                enqueueEvent(e);
            }

            setAttributeEnhancedHue(lightNode);
        }
            break;

        case TaskSetHueAndSaturation:
        {
            lightNode->setEnhancedHue(task.enhancedHue);

            ResourceItem *item = lightNode->item(RStateHue);
            if (item && item->toNumber() != task.enhancedHue)
            {
                updateLightEtag(lightNode);
                item->setValue(task.enhancedHue);
                Event e(RLights, RStateHue, lightNode->id(), item);
                enqueueEvent(e);
            }

            item = lightNode->item(RStateSat);
            if (item && item->toNumber() != task.sat)
            {
                updateLightEtag(lightNode);
                item->setValue(task.sat);
                Event e(RLights, RStateSat, lightNode->id(), item);
                enqueueEvent(e);
            }

            item = item ? lightNode->item(RStateColorMode) : 0; // depend on hue,sat
            if (item && item->toString() != QLatin1String("hs"))
            {
                item->setValue(QVariant(QLatin1String("hs")));
                Event e(RLights, RStateColorMode, lightNode->id());
                enqueueEvent(e);
            }

            setAttributeSaturation(lightNode);
            setAttributeEnhancedHue(lightNode);
        }
            break;

        case TaskSetXyColor:
        {
            ResourceItem *item = lightNode->item(RStateX);
            if (item && item->toNumber() != task.colorX)
            {
                updateLightEtag(lightNode);
                item->setValue(task.colorX);
                Event e(RLights, RStateX, lightNode->id(), item);
                enqueueEvent(e);
            }

            item = lightNode->item(RStateY);
            if (item && item->toNumber() != task.colorY)
            {
                updateLightEtag(lightNode);
                item->setValue(task.colorY);
                Event e(RLights, RStateY, lightNode->id(), item);
                enqueueEvent(e);
            }

            item = item ? lightNode->item(RStateColorMode) : 0; // depend on xy
            if (item && item->toString() != QLatin1String("xy"))
            {
                item->setValue(QVariant(QLatin1String("xy")));
                Event e(RLights, RStateColorMode, lightNode->id());
                enqueueEvent(e);
            }

            setAttributeColorXy(lightNode);
        }
            break;

        case TaskSetColorTemperature:
        {
            ResourceItem *item = lightNode->item(RStateCt);
            if (item && item->toNumber() != task.colorTemperature)
            {
                updateLightEtag(lightNode);
                item->setValue(task.colorTemperature);
                Event e(RLights, RStateCt, lightNode->id(), item);
                enqueueEvent(e);
            }

            item = item ? lightNode->item(RStateColorMode) : 0; // depend on ct
            if (item && item->toString() != QLatin1String("ct"))
            {
                item->setValue(QVariant(QLatin1String("ct")));
                Event e(RLights, RStateColorMode, lightNode->id());
                enqueueEvent(e);
            }

            setAttributeColorTemperature(lightNode);
        }
            break;

        case TaskIncColorTemperature:
        {
            ResourceItem *item = lightNode->item(RStateCt);
            if (!item)
            {
                break;
            }
            qint32 modCt = item->toNumber() + task.inc;
            // clip, TODO use phys. min. max. values from light
            if (modCt < 153) { modCt = 153; }
            else if (modCt > 500) { modCt = 500; }
            if (item && item->toNumber() != modCt)
            {
                updateLightEtag(lightNode);
                item->setValue(modCt);
                Event e(RLights, RStateCt, lightNode->id(), item);
                enqueueEvent(e);
            }

            item = item ? lightNode->item(RStateColorMode) : 0; // depend on ct
            if (item && item->toString() != QLatin1String("ct"))
            {
                item->setValue(QVariant(QLatin1String("ct")));
                Event e(RLights, RStateColorMode, lightNode->id());
                enqueueEvent(e);
            }

            setAttributeColorTemperature(lightNode);
        }
            break;

        case TaskIncBrightness:
        {
            ResourceItem *item = lightNode->item(RStateOn);
            if (!item || !item->toBool())
            {
                break;
            }

            item = lightNode->item(RStateBri);
            if (!item)
            {
                break;
            }
            qint32 modBri = item->toNumber() + task.inc;

            if (modBri < 1) { modBri = 1; }
            else if (modBri > 254) { modBri = 254; }
            if (item && item->toNumber() != modBri)
            {
                updateLightEtag(lightNode);
                item->setValue(modBri);
                Event e(RLights, RStateBri, lightNode->id(), item);
                enqueueEvent(e);
            }

            setAttributeLevel(lightNode);
        }
            break;

        case TaskSetColorLoop:
            if (lightNode->colorMode() == QLatin1String("ct") || (lightNode->colorX() == 0 && lightNode->colorY() == 0 && lightNode->hue() == 0 && lightNode->enhancedHue() == 0))
            {
                //do nothing
            }
            else
            {
                updateEtag(lightNode->etag);
                lightNode->setColorLoopActive(task.colorLoop);
                setAttributeColorLoopActive(lightNode);
            }
            break;

        default:
            break;
        }
    }
}

/*! Speed up discovery of end devices.
 */
void DeRestPluginPrivate::delayedFastEnddeviceProbe(const deCONZ::NodeEvent *event)
{
    if (/*getUptime() < WARMUP_TIME &&*/ searchSensorsState != SearchSensorsActive)
    {
        return;
    }

    SensorCandidate *sc = nullptr;
    {
        std::vector<SensorCandidate>::iterator i = searchSensorsCandidates.begin();
        std::vector<SensorCandidate>::iterator end = searchSensorsCandidates.end();

        for (; i != end; ++i)
        {
            if (i->address.ext() == fastProbeAddr.ext())
            {
                sc = &*i;
                break;
            }
        }
    }

    if (!sc)
    {
        return;
    }

#if DECONZ_LIB_VERSION >= 0x010900
    // when macPoll = true core will handle ZDP descriptor queries
    bool macPoll = event && event->event() == deCONZ::NodeEvent::NodeMacDataRequest;
#else
    bool macPoll = false;
#endif

    if (macPoll && fastProbeTimer->isActive())
    {
        fastProbeTimer->stop();
    }

    {
        Sensor *sensor = getSensorNodeForAddress(sc->address);
        const deCONZ::Node *node = sensor ? sensor->node() : nullptr;

        if (sensor && sensor->deletedState() != Sensor::StateNormal)
        {
            DBG_Printf(DBG_INFO, "don't use deleted sensor 0x%016llX as candidate\n", sc->address.ext());
            sensor = nullptr;
        }

        if (!node)
        {
            int i = 0;
            const deCONZ::Node *n;

            while (apsCtrl->getNode(i, &n) == 0)
            {
                if (fastProbeAddr.ext() == n->address().ext())
                {
                    node = n;
                    break;
                }
                i++;
            }
        }

        if (!node)
        {
            return;
        }

        if (!macPoll && node->nodeDescriptor().isNull())
        {
            DBG_Printf(DBG_INFO, "[1] get node descriptor for 0x%016llx\n", sc->address.ext());
            deCONZ::ApsDataRequest apsReq;

            // ZDP Header
            apsReq.dstAddress() = sc->address;
            apsReq.setDstAddressMode(deCONZ::ApsNwkAddress);
            apsReq.setDstEndpoint(ZDO_ENDPOINT);
            apsReq.setSrcEndpoint(ZDO_ENDPOINT);
            apsReq.setProfileId(ZDP_PROFILE_ID);
            apsReq.setRadius(0);
            apsReq.setClusterId(ZDP_NODE_DESCRIPTOR_CLID);
            //apsReq.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);

            QDataStream stream(&apsReq.asdu(), QIODevice::WriteOnly);
            stream.setByteOrder(QDataStream::LittleEndian);

            stream << zclSeq++;
            stream << sc->address.nwk();

            deCONZ::ApsController *apsCtrl = deCONZ::ApsController::instance();

            if (apsCtrl && apsCtrl->apsdeDataRequest(apsReq) == deCONZ::Success)
            {
                queryTime = queryTime.addSecs(5);
            }
            return;
        }

        if (sc->indClusterId == ZDP_ACTIVE_ENDPOINTS_RSP_CLID)
        {
            sc->endpoints = node->endpoints();
        }

        if (!macPoll && sc->endpoints.empty())
        {
            DBG_Printf(DBG_INFO, "[2] get active endpoints for 0x%016llx\n", sc->address.ext());
            deCONZ::ApsDataRequest apsReq;

            // ZDP Header
            apsReq.dstAddress() = sc->address;
            apsReq.setDstAddressMode(deCONZ::ApsNwkAddress);
            apsReq.setDstEndpoint(ZDO_ENDPOINT);
            apsReq.setSrcEndpoint(ZDO_ENDPOINT);
            apsReq.setProfileId(ZDP_PROFILE_ID);
            apsReq.setRadius(0);
            apsReq.setClusterId(ZDP_ACTIVE_ENDPOINTS_CLID);
            //apsReq.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);

            QDataStream stream(&apsReq.asdu(), QIODevice::WriteOnly);
            stream.setByteOrder(QDataStream::LittleEndian);

            stream << zclSeq++;
            stream << sc->address.nwk();

            deCONZ::ApsController *apsCtrl = deCONZ::ApsController::instance();

            if (apsCtrl && apsCtrl->apsdeDataRequest(apsReq) == deCONZ::Success)
            {
                queryTime = queryTime.addSecs(5);
            }
            return;
        }

        // simple descriptor for endpoint 0x01
        if (!macPoll && node->simpleDescriptors().size() != (int)node->endpoints().size())
        {
            quint8 ep = 0;

            for (size_t i = 0; i < node->endpoints().size(); i++)
            {
                ep = node->endpoints()[i]; // search

                for (int j = 0; j < node->simpleDescriptors().size(); j++)
                {
                    if (node->simpleDescriptors()[j].endpoint() == ep)
                    {
                        ep = 0;
                    }
                }

                if (ep) // fetch this
                {
                    DBG_Printf(DBG_INFO, "[3] get simple descriptor 0x%02X for 0x%016llx\n", ep, sc->address.ext());
                    deCONZ::ApsDataRequest apsReq;

                    // ZDP Header
                    apsReq.dstAddress() = sc->address;
                    apsReq.setDstAddressMode(deCONZ::ApsNwkAddress);
                    apsReq.setDstEndpoint(ZDO_ENDPOINT);
                    apsReq.setSrcEndpoint(ZDO_ENDPOINT);
                    apsReq.setProfileId(ZDP_PROFILE_ID);
                    apsReq.setRadius(0);
                    apsReq.setClusterId(ZDP_SIMPLE_DESCRIPTOR_CLID);
                    //apsReq.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);

                    QDataStream stream(&apsReq.asdu(), QIODevice::WriteOnly);
                    stream.setByteOrder(QDataStream::LittleEndian);

                    stream << zclSeq++;
                    stream << sc->address.nwk();
                    stream << ep;

                    deCONZ::ApsController *apsCtrl = deCONZ::ApsController::instance();

                    if (apsCtrl && apsCtrl->apsdeDataRequest(apsReq) == deCONZ::Success)
                    {
                        queryTime = queryTime.addSecs(1);
                    }

                    return;
                }
            }
        }

        QString manufacturer;
        QString modelId;
        QString swBuildId;
        QString dateCode;
        quint16 iasZoneType = 0;
        bool swBuildIdAvailable = false;

        if (sensor)
        {
            manufacturer = sensor->manufacturer();
            modelId = sensor->modelId();
            swBuildId = sensor->swVersion();
        }

        quint8 basicClusterEndpoint  = 0;
        std::vector<quint16> unavailBasicAttr;

        for (const deCONZ::SimpleDescriptor &sd : node->simpleDescriptors())
        {
            for (const deCONZ::ZclCluster &cl : sd.inClusters())
            {
                for (const deCONZ::ZclAttribute &attr : cl.attributes())
                {
                    if (cl.id() == BASIC_CLUSTER_ID)
                    {
                        if (basicClusterEndpoint == 0)
                        {
                            basicClusterEndpoint = sd.endpoint();
                        }

                        if (attr.id() == 0x0004 && manufacturer.isEmpty())
                        {
                            manufacturer = attr.toString();
                        }
                        else if (attr.id() == 0x0005 && modelId.isEmpty())
                        {
                            modelId = attr.toString();
                        }
                        else if (attr.id() == 0x0006 && dateCode.isEmpty())
                        {
                            dateCode = attr.toString();
                        }
                        else if (attr.id() == 0x4000 && swBuildId.isEmpty())
                        {
                            swBuildId = attr.toString();
                            swBuildIdAvailable = attr.isAvailable(); // might become false after first read
                        }
                        else
                        {
                            continue;
                        }

                        if (!attr.isAvailable())
                        {
                            unavailBasicAttr.push_back(attr.id());
                        }
                    }
                    else if (cl.id() == IAS_ZONE_CLUSTER_ID)
                    {
                        if (attr.id() == 0x0001 && attr.numericValue().u64 != 0) // Zone type
                        {
                            iasZoneType = attr.numericValue().u64;
                        }
                    }
                }
            }

            if (sd.deviceId() == DEV_ID_IAS_ZONE && iasZoneType == 0)
            {
                deCONZ::ApsDataRequest apsReq;

                DBG_Printf(DBG_INFO, "[3.1] get IAS Zone type for 0x%016llx\n", sc->address.ext());

                // ZDP Header
                apsReq.dstAddress() = sc->address;
                apsReq.setDstAddressMode(deCONZ::ApsNwkAddress);
                apsReq.setDstEndpoint(sd.endpoint());
                apsReq.setSrcEndpoint(endpoint());
                apsReq.setProfileId(HA_PROFILE_ID);
                apsReq.setRadius(0);
                apsReq.setClusterId(IAS_ZONE_CLUSTER_ID);
                //apsReq.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);

                deCONZ::ZclFrame zclFrame;
                zclFrame.setSequenceNumber(zclSeq++);
                zclFrame.setCommandId(deCONZ::ZclReadAttributesId);
                zclFrame.setFrameControl(deCONZ::ZclFCProfileCommand |
                                         deCONZ::ZclFCDirectionClientToServer |
                                         deCONZ::ZclFCDisableDefaultResponse);

                { // payload
                    QDataStream stream(&zclFrame.payload(), QIODevice::WriteOnly);
                    stream.setByteOrder(QDataStream::LittleEndian);

                    stream << (quint16)0x0001; // IAS Zone type
                }

                { // ZCL frame
                    QDataStream stream(&apsReq.asdu(), QIODevice::WriteOnly);
                    stream.setByteOrder(QDataStream::LittleEndian);
                    zclFrame.writeToStream(stream);
                }

                deCONZ::ApsController *apsCtrl = deCONZ::ApsController::instance();

                if (apsCtrl && apsCtrl->apsdeDataRequest(apsReq) == deCONZ::Success)
                {
                    queryTime = queryTime.addSecs(1);
                }
                return;
            }
        }

        if (sensor && sensor->deletedState() != Sensor::StateNormal)
        {
            sensor = nullptr; // force query
        }

        // manufacturer, model id, sw build id
        if (!sensor || modelId.isEmpty() || manufacturer.isEmpty() || (swBuildId.isEmpty() && dateCode.isEmpty()))
        {
            if (!modelId.isEmpty() && !isDeviceSupported(node, modelId))
            {
                return;
            }

            if (modelId.startsWith(QLatin1String("lumi.")))
            {
                return; // Xiaomi devices won't respond to ZCL read
            }

            if (basicClusterEndpoint == 0)
            {
                return;
            }

            deCONZ::ApsDataRequest apsReq;
            std::vector<quint16> attributes;

            // ZDP Header
            apsReq.dstAddress() = sc->address;
            apsReq.setDstAddressMode(deCONZ::ApsNwkAddress);
            apsReq.setDstEndpoint(basicClusterEndpoint);
            apsReq.setSrcEndpoint(endpoint());
            apsReq.setProfileId(HA_PROFILE_ID);
            apsReq.setRadius(0);
            apsReq.setClusterId(BASIC_CLUSTER_ID);
            //apsReq.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);

            deCONZ::ZclFrame zclFrame;
            zclFrame.setSequenceNumber(zclSeq++);
            zclFrame.setCommandId(deCONZ::ZclReadAttributesId);
            zclFrame.setFrameControl(deCONZ::ZclFCProfileCommand |
                                     deCONZ::ZclFCDirectionClientToServer |
                                     deCONZ::ZclFCDisableDefaultResponse);

            if ((sc->address.ext() & macPrefixMask) == jennicMacPrefix)
            {
                // don't read these (Xiaomi, Trust, ...)
                // response is empty or no response at all
            }
            else if (manufacturer.isEmpty()) { attributes.push_back(0x0004); }// manufacturer
            else if (modelId.isEmpty()) { attributes.push_back(0x0005); } // model id
            else if (swBuildId.isEmpty() && dateCode.isEmpty())
            {
                if ((sc->address.ext() & macPrefixMask) == tiMacPrefix ||
                    (sc->address.ext() & macPrefixMask) == ubisysMacPrefix ||
                    modelId == QLatin1String("Motion Sensor-A") || // OSRAM motion sensor
                    manufacturer.startsWith(QLatin1String("Climax")) ||
                    !swBuildIdAvailable)
                {
                    attributes.push_back(0x0006); // date code
                }
                else
                {
                    attributes.push_back(0x4000); // sw build id
                }
            }

            { // filter for available basic cluster attributes
                std::vector<quint16> tmp = attributes;
                attributes.clear();
                for (auto id: tmp)
                {
                    if (std::find(unavailBasicAttr.begin(), unavailBasicAttr.end(), id) == unavailBasicAttr.end())
                    {
                        attributes.push_back(id);
                    }
                }
            }

            if (!attributes.empty())
            {
                // payload
                QDataStream stream(&zclFrame.payload(), QIODevice::WriteOnly);
                stream.setByteOrder(QDataStream::LittleEndian);

                for (quint16 attrId : attributes)
                {
                    stream << attrId;
                    DBG_Printf(DBG_INFO, "[4] get basic cluster attr 0x%04X for 0x%016llx\n", attrId, sc->address.ext());
                }

                { // ZCL frame
                    QDataStream stream(&apsReq.asdu(), QIODevice::WriteOnly);
                    stream.setByteOrder(QDataStream::LittleEndian);
                    zclFrame.writeToStream(stream);
                }

                deCONZ::ApsController *apsCtrl = deCONZ::ApsController::instance();

                if (!zclFrame.payload().isEmpty() &&
                        apsCtrl && apsCtrl->apsdeDataRequest(apsReq) == deCONZ::Success)
                {
                    queryTime = queryTime.addSecs(1);
                }
            }
            else if (!sensor)
            {
                addSensorNode(node);
            }
            return;
        }

        if (!sensor || searchSensorsState != SearchSensorsActive)
        {
            // do nothing
        }
        else if (sensor->modelId().startsWith(QLatin1String("RWL02"))) // Hue dimmer switch
        {
            // Stop the Hue dimmer from touchlinking when holding the On button.
            deCONZ::ZclAttribute attr(0x0031, deCONZ::Zcl16BitBitMap, "mode", deCONZ::ZclReadWrite, false);
            attr.setBitmap(0x000b);

            NodeValue val = sensor->getZclValue(BASIC_CLUSTER_ID, 0x0031);

            if (val.isValid()) // already done
            {
            }
            else if (writeAttribute(sensor, sensor->fingerPrint().endpoint, BASIC_CLUSTER_ID, attr, VENDOR_PHILIPS))
            {
                queryTime = queryTime.addSecs(1);

                // mark done
                deCONZ::NumericUnion touchLink;
                touchLink.u64 = 0x000b;
                sensor->setZclValue(NodeValue::UpdateByZclRead, BASIC_CLUSTER_ID, 0x0031, touchLink);
                return;
            }

            val = sensor->getZclValue(VENDOR_CLUSTER_ID, 0x0000);
            if (!val.isValid())
            {
                if (checkSensorBindingsForAttributeReporting(sensor))
                {
                    return;
                }
            }

            ResourceItem *item = sensor->item(RConfigGroup);
            if (!item || !item->lastSet().isValid())
            {
                getGroupIdentifiers(sensor, 0x01, 0x00);
                return;
            }
        }
        else if (sensor->modelId() == QLatin1String("SML001")) // Hue motion sensor
        {
            std::vector<uint16_t> attributes;
            const NodeValue &sensitivity = sensor->getZclValue(OCCUPANCY_SENSING_CLUSTER_ID, 0x0030);
            if (!sensitivity.timestamp.isValid())
            {
                attributes.push_back(0x0030); // sensitivity
            }

            const NodeValue &sensitivitymax = sensor->getZclValue(OCCUPANCY_SENSING_CLUSTER_ID, 0x0031);
            if (!sensitivitymax.timestamp.isValid())
            {
                attributes.push_back(0x0031); // sensitivitymax
            }

            if (!attributes.empty() &&
                 readAttributes(sensor, sensor->fingerPrint().endpoint, OCCUPANCY_SENSING_CLUSTER_ID, attributes, VENDOR_PHILIPS))
            {
                queryTime = queryTime.addSecs(1);
            }

            attributes = {};
            const NodeValue &usertest = sensor->getZclValue(BASIC_CLUSTER_ID, 0x0032);
            if (!usertest.timestamp.isValid())
            {
                attributes.push_back(0x0032); // usertest
            }

            const NodeValue &ledindication = sensor->getZclValue(BASIC_CLUSTER_ID, 0x0033);
            if (!ledindication.timestamp.isValid())
            {
                attributes.push_back(0x0033); // ledindication
            }

            if (!attributes.empty() &&
                readAttributes(sensor, sensor->fingerPrint().endpoint, BASIC_CLUSTER_ID, attributes, VENDOR_PHILIPS))
            {
                queryTime = queryTime.addSecs(1);
            }
        }
        else if (sensor->modelId() == QLatin1String("TRADFRI wireless dimmer")) // IKEA dimmer
        {
            ResourceItem *item = sensor->item(RConfigGroup);

            if (!item || !item->lastSet().isValid())
            {
                if (getGroupIdentifiers(sensor, sensor->fingerPrint().endpoint, 0))
                {
                    queryTime = queryTime.addSecs(1);
                }
            }

            item = sensor->item(RStateButtonEvent);

            if (!item || !item->lastSet().isValid())
            {
                BindingTask bindingTask;

                bindingTask.state = BindingTask::StateIdle;
                bindingTask.action = BindingTask::ActionBind;
                bindingTask.restNode = sensor;
                Binding &bnd = bindingTask.binding;
                bnd.srcAddress = sensor->address().ext();
                bnd.dstAddrMode = deCONZ::ApsExtAddress;
                bnd.srcEndpoint = sensor->fingerPrint().endpoint;
                bnd.clusterId = LEVEL_CLUSTER_ID;
                bnd.dstAddress.ext = apsCtrl->getParameter(deCONZ::ParamMacAddress);
                bnd.dstEndpoint = endpoint();

                if (bnd.dstEndpoint > 0) // valid gateway endpoint?
                {
                    queueBindingTask(bindingTask);
                }
            }
        }

        for (auto &s : sensors)
        {
            if (s.address().ext() != sc->address.ext())
            {
                continue;
            }

            if (s.deletedState() != Sensor::StateNormal)
            {
                continue;
            }

            if (s.lastAttributeReportBind() < (idleTotalCounter - IDLE_ATTR_REPORT_BIND_LIMIT_SHORT))
            {
                if (checkSensorBindingsForAttributeReporting(&s))
                {
                    s.setLastAttributeReportBind(idleTotalCounter);
                }
            }
        }
    }
}

/*! Updates the onOff attribute in the local node cache.
 */
void DeRestPluginPrivate::setAttributeOnOff(LightNode *lightNode)
{
    DBG_Assert(lightNode != 0);

    if (!lightNode || !lightNode->node())
    {
        return;
    }

    ResourceItem *item = lightNode->item(RStateOn);

    if (!item)
    {
        return;
    }

    deCONZ::ZclCluster *cl = getInCluster(lightNode->node(), lightNode->haEndpoint().endpoint(), ONOFF_CLUSTER_ID);

    if (cl && cl->attributes().size() > 0)
    {
        deCONZ::ZclAttribute &attr = cl->attributes()[0];

        DBG_Assert(attr.id() == 0x0000);

        if (attr.id() == 0x0000)
        {
            attr.setValue(item->toBool());
        }
    }
}

/*! Updates the level attribute in the local node cache.
 */
void DeRestPluginPrivate::setAttributeLevel(LightNode *lightNode)
{
    DBG_Assert(lightNode != 0);

    if (!lightNode || !lightNode->node())
    {
        return;
    }

    ResourceItem *item = lightNode->item(RStateBri);

    if (!item)
    {
        return;
    }

    deCONZ::ZclCluster *cl = getInCluster(lightNode->node(), lightNode->haEndpoint().endpoint(), LEVEL_CLUSTER_ID);

    if (cl && cl->attributes().size() > 0)
    {
        deCONZ::ZclAttribute &attr = cl->attributes()[0];
        if (attr.id() == 0x0000)
        {
            attr.setValue((quint64)item->toNumber());
        }
    }
}

/*! Updates the saturation attribute in the local node cache.
 */
void DeRestPluginPrivate::setAttributeSaturation(LightNode *lightNode)
{
    DBG_Assert(lightNode != 0);

    if (!lightNode || !lightNode->node())
    {
        return;
    }

    ResourceItem *item = lightNode->item(RStateSat);

    if (!item)
    {
        return;
    }

    deCONZ::ZclCluster *cl = getInCluster(lightNode->node(), lightNode->haEndpoint().endpoint(), COLOR_CLUSTER_ID);

    if (cl)
    {
        std::vector<deCONZ::ZclAttribute>::iterator i = cl->attributes().begin();
        std::vector<deCONZ::ZclAttribute>::iterator end = cl->attributes().end();

        for (; i != end; ++i)
        {
            if (i->id() == 0x0001) // Current saturation
            {
                i->setValue((quint64)item->toNumber());
                break;
            }

        }
    }
}

/*! Updates the color xy attribute in the local node cache.
 */
void DeRestPluginPrivate::setAttributeColorXy(LightNode *lightNode)
{
    DBG_Assert(lightNode != 0);

    if (!lightNode || !lightNode->node())
    {
        return;
    }

    ResourceItem *ix = lightNode->item(RStateX);
    ResourceItem *iy = lightNode->item(RStateY);

    if (!ix || !iy)
    {
        return;
    }

    deCONZ::ZclCluster *cl = getInCluster(lightNode->node(), lightNode->haEndpoint().endpoint(), COLOR_CLUSTER_ID);

    if (cl)
    {
        std::vector<deCONZ::ZclAttribute>::iterator i = cl->attributes().begin();
        std::vector<deCONZ::ZclAttribute>::iterator end = cl->attributes().end();

        for (; i != end; ++i)
        {
            if (i->id() == 0x0003) // Current color x
            {
                i->setValue((quint64)ix->toNumber());
            }
            else if (i->id() == 0x0004) // Current color y
            {
                i->setValue((quint64)iy->toNumber());
                break;
            }
        }
    }
}

/*! Updates the color temperature attribute in the local node cache.
 */
void DeRestPluginPrivate::setAttributeColorTemperature(LightNode *lightNode)
{
    DBG_Assert(lightNode != 0);

    if (!lightNode || !lightNode->node())
    {
        return;
    }

    ResourceItem *item = lightNode->item(RStateCt);

    if (!item)
    {
        return;
    }

    deCONZ::ZclCluster *cl = getInCluster(lightNode->node(), lightNode->haEndpoint().endpoint(), COLOR_CLUSTER_ID);

    if (cl)
    {
        std::vector<deCONZ::ZclAttribute>::iterator i = cl->attributes().begin();
        std::vector<deCONZ::ZclAttribute>::iterator end = cl->attributes().end();

        for (; i != end; ++i)
        {
            if (i->id() == 0x0007) // Current color temperature
            {
                i->setValue((quint64)item->toNumber());
                break;
            }
        }
    }
}

/*! Updates the color loop active attribute in the local node cache.
 */
void DeRestPluginPrivate::setAttributeColorLoopActive(LightNode *lightNode)
{
    DBG_Assert(lightNode != 0);

    if (!lightNode || !lightNode->node())
    {
        return;
    }

    deCONZ::ZclCluster *cl = getInCluster(lightNode->node(), lightNode->haEndpoint().endpoint(), COLOR_CLUSTER_ID);

    if (cl)
    {
        std::vector<deCONZ::ZclAttribute>::iterator i = cl->attributes().begin();
        std::vector<deCONZ::ZclAttribute>::iterator end = cl->attributes().end();

        for (; i != end; ++i)
        {
            if (i->id() == 0x4002) // Color loop active
            {
                i->setValue(lightNode->isColorLoopActive());
                break;
            }
        }
    }
}

/*! Shall be called whenever the sensor changed.
 */
void DeRestPluginPrivate::updateSensorEtag(Sensor *sensorNode)
{
    if (sensorNode)
    {
        updateEtag(sensorNode->etag);
        gwSensorsEtag = sensorNode->etag;
        gwConfigEtag = sensorNode->etag;
    }
}

/*! Shall be called whenever the light changed.
 */
void DeRestPluginPrivate::updateLightEtag(LightNode *lightNode)
{
    if (lightNode)
    {
        updateEtag(lightNode->etag);
        gwLightsEtag = lightNode->etag;
        gwConfigEtag = lightNode->etag;
    }
}

/*! Shall be called whenever the group changed.
 */
void DeRestPluginPrivate::updateGroupEtag(Group *group)
{
    if (group)
    {
        updateEtag(group->etag);
        gwGroupsEtag = group->etag;
        gwConfigEtag = group->etag;
    }
}

/*! Shall be called whenever the user did something which resulted in a over the air request.
 */
void DeRestPluginPrivate::userActivity()
{
    idleLastActivity = 0;
}

/*! Updates the enhanced hue attribute in the local node cache.
 */
void DeRestPluginPrivate::setAttributeEnhancedHue(LightNode *lightNode)
{
    DBG_Assert(lightNode != 0);

    if (!lightNode || !lightNode->node())
    {
        return;
    }

    ResourceItem *item = lightNode->item(RStateHue);

    if (!item)
    {
        return;
    }

    deCONZ::ZclCluster *cl = getInCluster(lightNode->node(), lightNode->haEndpoint().endpoint(), COLOR_CLUSTER_ID);

    if (cl)
    {
        std::vector<deCONZ::ZclAttribute>::iterator i = cl->attributes().begin();
        std::vector<deCONZ::ZclAttribute>::iterator end = cl->attributes().end();

        for (; i != end; ++i)
        {
            if (i->id() == 0x4000) // Enhanced hue
            {
                i->setValue((quint64)item->toNumber());
                break;
            }

        }
    }
}

/*! Main plugin constructor.
    \param parent - parent object
 */
DeRestPlugin::DeRestPlugin(QObject *parent) :
    QObject(parent)
{
    d = new DeRestPluginPrivate(this);
    d->q_ptr = this;
    m_state = StateOff;
    m_w = 0;
    m_idleTimer = new QTimer(this);
    m_idleTimer->setSingleShot(false);

    connect(QCoreApplication::instance(), SIGNAL(aboutToQuit()),
            this, SLOT(appAboutToQuit()));

    connect(m_idleTimer, SIGNAL(timeout()),
            this, SLOT(idleTimerFired()));

    m_readAttributesTimer = new QTimer(this);
    m_readAttributesTimer->setSingleShot(true);

    connect(m_readAttributesTimer, SIGNAL(timeout()),
            this, SLOT(checkZclAttributeTimerFired()));

    m_idleTimer->start(1000);
}

/*! The plugin deconstructor.
 */
DeRestPlugin::~DeRestPlugin()
{
    d = 0;
}

/*! Handle idle states.

    After IDLE_LIMIT seconds user inactivity this timer
    checks if nodes need to be refreshed. This is the case
    if a node was not refreshed for IDLE_READ_LIMIT seconds.
 */
void DeRestPlugin::idleTimerFired()
{
    d->idleTotalCounter++;
    d->idleLastActivity++;

    if (d->idleTotalCounter < 0) // overflow
    {
        d->idleTotalCounter = 0;
        d->otauIdleTotalCounter = 0;
        d->otauUnbindIdleTotalCounter = 0;
        d->saveDatabaseIdleTotalCounter = 0;
        d->recoverOnOff.clear();
    }

    if (d->idleLastActivity < 0) // overflow
    {
        d->idleLastActivity = 0;
    }

    if (d->idleLimit > 0)
    {
        d->idleLimit--;
    }

    ResourceItem *localTime = d->config.item(RConfigLocalTime);
    if (localTime)
    {
        localTime->setValue(QDateTime::currentDateTime());
        d->enqueueEvent(Event(RConfig, RConfigLocalTime, 0));
    }

    if (d->idleLastActivity < IDLE_USER_LIMIT)
    {
        return;
    }

    if (!d->gwDeviceAddress.hasExt() && d->apsCtrl)
    {
        const quint64 macAddress = d->apsCtrl->getParameter(deCONZ::ParamMacAddress);
        if (macAddress != 0)
        {
            d->gwDeviceAddress.setExt(macAddress);
            d->gwDeviceAddress.setNwk(d->apsCtrl->getParameter(deCONZ::ParamNwkAddress));
        }
        if (!(d->gwLANBridgeId) && d->gwDeviceAddress.hasExt())
        {
            d->gwBridgeId.sprintf("%016llX", (quint64)d->gwDeviceAddress.ext());
            if (!d->gwConfig.contains("bridgeid") || d->gwConfig["bridgeid"] != d->gwBridgeId)
            {
                DBG_Printf(DBG_INFO, "Set bridgeid to %s\n", qPrintable(d->gwBridgeId));
                d->gwConfig["bridgeid"] = d->gwBridgeId;
                d->queSaveDb(DB_CONFIG, DB_SHORT_SAVE_DELAY);
                d->initDescriptionXml();
            }
        }
    }

    if (!pluginActive())
    {
        return;
    }

    if (!d->isInNetwork())
    {
        return;
    }

    if (d->channelChangeState != DeRestPluginPrivate::CC_Idle)
    {
        return;
    }

    int tSpacing = 2;

    // slow down query if otau was busy recently
    if (d->otauLastBusyTimeDelta() < OTA_LOW_PRIORITY_TIME)
    {
        tSpacing = 60;
    }

    if (!d->recoverOnOff.empty())
    {
        DeRestPluginPrivate::RecoverOnOff &rc = d->recoverOnOff.back();
        if ((d->idleTotalCounter - rc.idleTotalCounterCopy) > MAX_RECOVER_ENTRY_AGE)
        {
            DBG_Printf(DBG_INFO, "Pop recover info for 0x%016llX\n", rc.address.ext());
            d->recoverOnOff.pop_back();
        }
    }

    bool processLights = false;

    if (d->idleLimit <= 0)
    {
        QTime t = QTime::currentTime();

        if (d->idleUpdateZigBeeConf < d->idleTotalCounter)
        {
            d->idleUpdateZigBeeConf = d->idleTotalCounter + CHECK_ZB_GOOD_INTERVAL;
            d->updateZigBeeConfigDb();
        }

        if (d->queryTime > t)
        {
            if (t.secsTo(d->queryTime) < (60 * 30)) // prevent stallation
            {
                DBG_Printf(DBG_INFO_L2, "Wait %ds till query finished\n", t.secsTo(d->queryTime));
                return; // wait finish
            }
        }

        if (!d->pollManager->hasItems())
        {
            d->pollNextDevice();
        }

        QDateTime now = QDateTime::currentDateTime();
        d->queryTime = t;

        DBG_Printf(DBG_INFO_L2, "Idle timer triggered\n");

        if (!d->nodes.empty())
        {
            if (d->lightIter >= d->nodes.size())
            {
                d->lightIter = 0;
            }

            while (d->lightIter < d->nodes.size())
            {
                LightNode *lightNode = &d->nodes[d->lightIter];
                d->lightIter++;

                if (!lightNode->isAvailable() || !lightNode->lastRx().isValid() || !lightNode->node())
                {
                    continue;
                }

                if (lightNode->node()->isZombie())
                {   // handle here if not detected earlier TODO merge
                    d->nodeZombieStateChanged(lightNode->node());
                    if (!lightNode->isAvailable())
                    {
                        continue;
                    }
                }

                if (lightNode->lastRx().secsTo(now) > (5 * 60))
                {
                    // let poll manager detect if node is available
                    d->queuePollNode(lightNode);
                    continue;
                }

                if (processLights)
                {
                    break;
                }

                // workaround for Xiaomi lights and smart plugs with multiple endpoints but only one basic cluster
                if (lightNode->manufacturerCode() == VENDOR_115F && (lightNode->modelId().isEmpty() || lightNode->item(RAttrSwVersion)->toString().isEmpty()))
                {
                    for (const auto &l : d->nodes)
                    {
                        if (l.address().ext() != lightNode->address().ext() || (l.haEndpoint().endpoint() == lightNode->haEndpoint().endpoint()))
                        {
                            continue;
                        }

                        if (lightNode->modelId().isEmpty() && !l.modelId().isEmpty())
                        {
                            lightNode->setModelId(l.modelId());
                            lightNode->setNeedSaveDatabase(true);
                            d->queSaveDb(DB_LIGHTS, DB_SHORT_SAVE_DELAY);
                        }

                        if (lightNode->item(RAttrSwVersion)->toString().isEmpty() && !l.item(RAttrSwVersion)->toString().isEmpty())
                        {
                            lightNode->item(RAttrSwVersion)->setValue(l.item(RAttrSwVersion)->toString());
                            lightNode->setNeedSaveDatabase(true);
                            d->queSaveDb(DB_LIGHTS, DB_SHORT_SAVE_DELAY);
                        }
                        break;
                    }
                }

                if (lightNode->modelId().startsWith(QLatin1String("FLS-NB")))
                {
                    // temporary activate sensor search
                    DeRestPluginPrivate::SearchSensorsState fss = d->searchSensorsState; // remember
                    d->searchSensorsState = DeRestPluginPrivate::SearchSensorsActive;
                    d->addSensorNode(lightNode->node());
                    d->searchSensorsState = fss;

                    // temporary,
                    if (d->flsNbMaintenance(lightNode))
                    {
                        d->queryTime = d->queryTime.addSecs(10);
                        processLights = true;
                    }
                }

                const uint32_t items[]   = { READ_GROUPS, READ_SCENES, 0 };
                const int tRead[]        = {        1800,        3600, 0 };

                for (size_t i = 0; items[i] != 0; i++)
                {
                    if (lightNode->mustRead(items[i]))
                    {
                        continue;
                    }

                    if (items[i] == READ_GROUPS || items[i] == READ_SCENES)
                    {
                        // don't query low priority items when OTA is busy
                        if (d->otauLastBusyTimeDelta() < OTA_LOW_PRIORITY_TIME)
                        {
                            continue;
                        }
                    }

                    if (lightNode->lastRead(items[i]) < (d->idleTotalCounter - tRead[i]))
                    {
                        lightNode->setNextReadTime(items[i], d->queryTime);
                        lightNode->setLastRead(items[i], d->idleTotalCounter);
                        lightNode->enableRead(items[i]);
                        d->queryTime = d->queryTime.addSecs(tSpacing);
                        processLights = true;
                    }
                }

                if (!lightNode->mustRead(READ_SWBUILD_ID) && (lightNode->swBuildId().isEmpty() || lightNode->lastRead(READ_SWBUILD_ID) < d->idleTotalCounter - READ_SWBUILD_ID_INTERVAL))
                {
                    lightNode->setLastRead(READ_SWBUILD_ID, d->idleTotalCounter);
                    lightNode->enableRead(READ_SWBUILD_ID);
                    lightNode->setNextReadTime(READ_SWBUILD_ID, d->queryTime);
                    d->queryTime = d->queryTime.addSecs(tSpacing);
                    processLights = true;
                }

                if (lightNode->manufacturer().isEmpty() || (lightNode->manufacturer() == QLatin1String("Unknown")))
                {
                    lightNode->setLastRead(READ_VENDOR_NAME, d->idleTotalCounter);
                    lightNode->enableRead(READ_VENDOR_NAME);
                    lightNode->setNextReadTime(READ_VENDOR_NAME, d->queryTime);
                    d->queryTime = d->queryTime.addSecs(tSpacing);
                    processLights = true;
                }

                if (processLights)
                {
                    DBG_Printf(DBG_INFO_L2, "Force read attributes for node %s\n", qPrintable(lightNode->name()));
                }

                // don't query low priority items when OTA is busy or sensor search is active
                if (d->otauLastBusyTimeDelta() > OTA_LOW_PRIORITY_TIME || d->permitJoinFlag)
                {
                    if (lightNode->lastAttributeReportBind() < (d->idleTotalCounter - IDLE_ATTR_REPORT_BIND_LIMIT))
                    {
                        d->checkLightBindingsForAttributeReporting(lightNode);
                        if (lightNode->mustRead(READ_BINDING_TABLE))
                        {
                            lightNode->setLastRead(READ_BINDING_TABLE, d->idleTotalCounter);
                            lightNode->setNextReadTime(READ_BINDING_TABLE, d->queryTime);
                            d->queryTime = d->queryTime.addSecs(tSpacing);
                        }
                        lightNode->setLastAttributeReportBind(d->idleTotalCounter);
                        DBG_Printf(DBG_INFO_L2, "Force binding of attribute reporting for node %s\n", qPrintable(lightNode->name()));
                        processLights = true;
                    }
                }
            }
        }

        bool processSensors = false;

        if (!d->sensors.empty())
        {
            if (d->sensorIter >= d->sensors.size())
            {
                d->sensorIter = 0;
            }

            while (d->sensorIter < d->sensors.size())
            {
                Sensor *sensorNode = &d->sensors[d->sensorIter];
                d->sensorIter++;

                if (!sensorNode->node())
                {
                    deCONZ::Node *node = d->getNodeForAddress(sensorNode->address().ext());
                    if (node)
                    {
                        sensorNode->setNode(node);
                        sensorNode->fingerPrint().checkCounter = SENSOR_CHECK_COUNTER_INIT; // force check
                    }
                }

                if (sensorNode->modelId().startsWith(QLatin1String("FLS-NB"))) // sync names
                {
                    LightNode *lightNode = d->getLightNodeForAddress(sensorNode->address());

                    bool updated = false;
                    if (lightNode && sensorNode->name() != lightNode->name())
                    {
                        sensorNode->setName(lightNode->name());
                        updated = true;
                    }

                    if (sensorNode->manufacturer() != QLatin1String("nimbus group"))
                    {
                        sensorNode->setManufacturer(QLatin1String("nimbus group"));
                        updated = true;
                    }

                    if (updated)
                    {
                        sensorNode->setNeedSaveDatabase(true);
                        d->updateSensorEtag(sensorNode);
                        d->queSaveDb(DB_SENSORS, DB_SHORT_SAVE_DELAY);
                    }
                }

                if (sensorNode->node())
                {
                    sensorNode->fingerPrint().checkCounter++;
                    if (sensorNode->fingerPrint().checkCounter > SENSOR_CHECK_COUNTER_INIT)
                    {
                        sensorNode->fingerPrint().checkCounter = 0;
                        for (quint8 ep : sensorNode->node()->endpoints())
                        {
                            d->checkUpdatedFingerPrint(sensorNode->node(), ep, sensorNode);
                        }
                        d->checkSensorNodeReachable(sensorNode);
                    }
                }

                if (!sensorNode->isAvailable())
                {
                    continue;
                }

                if (!sensorNode->type().startsWith(QLatin1String("Z"))) // Exclude CLIP and Daylight sensors
                {
                    continue;
                }

                if (sensorNode->lastRx().secsTo(now) > (5 * 60))
                {
                    // let poll manager detect if node is available
                    d->queuePollNode(sensorNode);
                    continue;
                }

                if (processSensors)
                {
                    break;
                }

                if (sensorNode->modelId().isEmpty())
                {
                    LightNode *lightNode = d->getLightNodeForAddress(sensorNode->address());
                    if (lightNode && !lightNode->modelId().isEmpty())
                    {
                        sensorNode->setModelId(lightNode->modelId());
                    }
                    else if (!sensorNode->mustRead(READ_MODEL_ID))
                    {
                        sensorNode->setLastRead(READ_MODEL_ID, d->idleTotalCounter);
                        sensorNode->setNextReadTime(READ_MODEL_ID, d->queryTime);
                        sensorNode->enableRead(READ_MODEL_ID);
                        d->queryTime = d->queryTime.addSecs(tSpacing);
                        processSensors = true;
                    }
                }

                if (!sensorNode->mustRead(READ_VENDOR_NAME) &&
                   (sensorNode->manufacturer().isEmpty() ||
                    sensorNode->manufacturer() == QLatin1String("unknown")))
                {
                    sensorNode->setLastRead(READ_VENDOR_NAME, d->idleTotalCounter);
                    sensorNode->setNextReadTime(READ_VENDOR_NAME, d->queryTime);
                    sensorNode->enableRead(READ_VENDOR_NAME);
                    d->queryTime = d->queryTime.addSecs(tSpacing);
                    processSensors = true;
                }

                if (processSensors)
                {
                    DBG_Printf(DBG_INFO_L2, "Force read attributes for node %s\n", qPrintable(sensorNode->name()));
                }
                else
                {
                    d->queuePollNode(sensorNode);
                }

                if ((d->otauLastBusyTimeDelta() > OTA_LOW_PRIORITY_TIME) && (sensorNode->lastRead(READ_BINDING_TABLE) < (d->idleTotalCounter - IDLE_READ_LIMIT)))
                {
                    std::vector<quint16>::const_iterator ci = sensorNode->fingerPrint().inClusters.begin();
                    std::vector<quint16>::const_iterator cend = sensorNode->fingerPrint().inClusters.end();
                    for (;ci != cend; ++ci)
                    {
                        NodeValue val;

                        if (*ci == ILLUMINANCE_MEASUREMENT_CLUSTER_ID)
                        {
                            val = sensorNode->getZclValue(*ci, 0x0000); // measured value
                        }
                        else if (*ci == OCCUPANCY_SENSING_CLUSTER_ID)
                        {
                            val = sensorNode->getZclValue(*ci, 0x0000); // occupied state
                        }

                        if (val.timestampLastReport.isValid() &&
                            val.timestampLastReport.secsTo(now) < (60 * 45)) // got update in timely manner
                        {
                            DBG_Printf(DBG_INFO_L2, "binding for attribute reporting SensorNode %s of cluster 0x%04X seems to be active\n", qPrintable(sensorNode->name()), *ci);
                        }
                        else if (!sensorNode->mustRead(READ_BINDING_TABLE))
                        {
                            sensorNode->enableRead(READ_BINDING_TABLE);
                            sensorNode->setLastRead(READ_BINDING_TABLE, d->idleTotalCounter);
                            sensorNode->setNextReadTime(READ_BINDING_TABLE, d->queryTime);
                            d->queryTime = d->queryTime.addSecs(tSpacing);
                            processSensors = true;
                        }

                        if (*ci == OCCUPANCY_SENSING_CLUSTER_ID)
                        {
                            if (!sensorNode->mustRead(READ_OCCUPANCY_CONFIG))
                            {
                                val = sensorNode->getZclValue(*ci, 0x0010); // PIR occupied to unoccupied delay

                                if (!val.timestamp.isValid() || val.timestamp.secsTo(now) > 1800)
                                {
                                    sensorNode->enableRead(READ_OCCUPANCY_CONFIG);
                                    sensorNode->setLastRead(READ_OCCUPANCY_CONFIG, d->idleTotalCounter);
                                    sensorNode->setNextReadTime(READ_OCCUPANCY_CONFIG, d->queryTime);
                                    d->queryTime = d->queryTime.addSecs(tSpacing);
                                    processSensors = true;
                                }
                            }
                        }

                        if (*ci == THERMOSTAT_CLUSTER_ID)
                        {
                            val = sensorNode->getZclValue(*ci, 0x0029); // heating state

                            if (!val.timestamp.isValid() || val.timestamp.secsTo(now) > 600)
                            {
                                sensorNode->enableRead(READ_THERMOSTAT_STATE);
                                sensorNode->setLastRead(READ_THERMOSTAT_STATE, d->idleTotalCounter);
                                sensorNode->setNextReadTime(READ_THERMOSTAT_STATE, d->queryTime);
                                d->queryTime = d->queryTime.addSecs(tSpacing);
                                processSensors = true;
                            }
                        }
                    }

                    DBG_Printf(DBG_INFO_L2, "Force read attributes for SensorNode %s\n", qPrintable(sensorNode->name()));
                    //break;
                }

                if ((d->otauLastBusyTimeDelta() > OTA_LOW_PRIORITY_TIME) && (sensorNode->lastAttributeReportBind() < (d->idleTotalCounter - IDLE_ATTR_REPORT_BIND_LIMIT)))
                {
                    d->checkSensorBindingsForAttributeReporting(sensorNode);
                    sensorNode->setLastAttributeReportBind(d->idleTotalCounter);
                    if (sensorNode->mustRead(READ_BINDING_TABLE))
                    {
                        sensorNode->setNextReadTime(READ_BINDING_TABLE, d->queryTime);
                        d->queryTime = d->queryTime.addSecs(tSpacing);
                    }
                    DBG_Printf(DBG_INFO_L2, "Force binding of attribute reporting for node %s\n", qPrintable(sensorNode->name()));
                    processSensors = true;
                }
            }
        }

        {
            std::vector<LightNode>::iterator i = d->nodes.begin();
            std::vector<LightNode>::iterator end = d->nodes.end();

            int countNoColorXySupport = 0;

            for (; i != end; ++i)
            {
                // older FLS which do not have correct support for color mode xy has atmel vendor id
                if (i->isAvailable() && (i->manufacturerCode() == VENDOR_ATMEL))
                {
                    countNoColorXySupport++;
                }
            }

            if ((countNoColorXySupport > 0) && d->supportColorModeXyForGroups)
            {
                DBG_Printf(DBG_INFO_L2, "disable support for CIE 1931 XY color mode for groups\n");
                d->supportColorModeXyForGroups = false;
            }
            else if ((countNoColorXySupport == 0) && !d->supportColorModeXyForGroups)
            {
                DBG_Printf(DBG_INFO_L2, "enable support for CIE 1931 XY color mode for groups\n");
                d->supportColorModeXyForGroups = true;
            }
            else
            {
    //            DBG_Printf(DBG_INFO_L2, "support for CIE 1931 XY color mode for groups %u\n", d->supportColorModeXyForGroups);
            }
        }

        startZclAttributeTimer(checkZclAttributesDelay);

        if (d->otauLastBusyTimeDelta() < OTA_LOW_PRIORITY_TIME)
        {
            d->idleLimit = 60;
        }
        else if (processLights || processSensors)
        {
            if      (d->nodes.size() < 10)  { d->idleLimit = 1; }
            else if (d->nodes.size() < 20)  { d->idleLimit = 2; }
            else if (d->nodes.size() < 50)  { d->idleLimit = 5; }
            else if (d->nodes.size() < 100) { d->idleLimit = 7; }
            else if (d->nodes.size() < 150) { d->idleLimit = 8; }
        }
        else
        {
            d->idleLimit = IDLE_LIMIT;
        }
    }
}

/*! Refresh all nodes by forcing the idle timer to trigger.
 */
void DeRestPlugin::refreshAll()
{
//    std::vector<LightNode>::iterator i = d->nodes.begin();
//    std::vector<LightNode>::iterator end = d->nodes.end();

//    for (; i != end; ++i)
//    {
//        // force refresh on next idle timer timeout
//        i->setLastRead(d->idleTotalCounter - (IDLE_READ_LIMIT + 1));
//    }

    d->idleLimit = 0;
    d->idleLastActivity = IDLE_USER_LIMIT;
//    d->runningTasks.clear();
//    d->tasks.clear();
}

/*! Starts the read attributes timer with a given \p delay.
 */
void DeRestPlugin::startZclAttributeTimer(int delay)
{
    if (!m_readAttributesTimer->isActive())
    {
        m_readAttributesTimer->start(delay);
    }
}

/*! Stops the read attributes timer.
 */
void DeRestPlugin::stopZclAttributeTimer()
{
    m_readAttributesTimer->stop();
}

/*! Checks if attributes of any nodes shall be queried or written.
 */
void DeRestPlugin::checkZclAttributeTimerFired()
{
    if (!pluginActive())
    {
        return;
    }

    stopZclAttributeTimer();

    if (d->tasks.size() > MAX_BACKGROUND_TASKS)
    {
        startZclAttributeTimer(1000);
        return;
    }

    if (d->lightAttrIter >= d->nodes.size())
    {
        d->lightAttrIter = 0;
    }

    while (d->lightAttrIter < d->nodes.size())
    {
        LightNode *lightNode = &d->nodes[d->lightAttrIter];
        d->lightAttrIter++;

        if (d->getUptime() < WARMUP_TIME)
        {
            // warmup phase
        }
        else if (d->processZclAttributes(lightNode))
        {
            // read next later
            startZclAttributeTimer(checkZclAttributesDelay);
            d->processTasks();
            break;
        }
    }

    if (d->sensorAttrIter >= d->sensors.size())
    {
        d->sensorAttrIter = 0;
    }

    while (d->sensorAttrIter < d->sensors.size())
    {
        Sensor *sensorNode = &d->sensors[d->sensorAttrIter];
        d->sensorAttrIter++;

        if (d->processZclAttributes(sensorNode))
        {
            // read next later
            startZclAttributeTimer(checkZclAttributesDelay);
            d->processTasks();
            break;
        }
    }

    startZclAttributeTimer(checkZclAttributesDelay);
}

/*! Handler called before the application will be closed.
 */
void DeRestPlugin::appAboutToQuit()
{
    DBG_Printf(DBG_INFO, "REST API plugin shutting down\n");

    if (d)
    {
        d->ttlDataBaseConnection = 0;
        d->saveDatabaseItems |= (DB_SENSORS | DB_RULES | DB_LIGHTS);
        d->openDb();
        d->saveDb();
        d->closeDb();

        d->apsCtrl = 0;
    }
}

/*! Helper to start firmware update from main application.
 */
bool DeRestPlugin::startUpdateFirmware()
{
    return d->startUpdateFirmware();
}

const QString &DeRestPlugin::getNodeName(quint64 extAddress) const
{
    deCONZ::Address addr;
    addr.setExt(extAddress);
    LightNode *lightNode = d->getLightNodeForAddress(addr);

    if (lightNode)
    {
        return lightNode->name();
    }

    Sensor *sensor = d->getSensorNodeForAddress(addr);
    if (sensor)
    {
        return sensor->name();
    }

    return d->emptyString;
}

/*! Query this plugin which features are supported.
    \param feature - feature to be checked
    \return true if supported
 */
bool DeRestPlugin::hasFeature(Features feature)
{
    switch (feature)
    {
    case DialogFeature:
    case HttpClientHandlerFeature:
        return true;

    default:
        break;
    }

    return false;
}

/*! Creates a control widget for this plugin.
    \return 0 - not implemented
 */
QWidget *DeRestPlugin::createWidget()
{
    return 0;
}

/*! Creates a control dialog for this plugin.
    \return the dialog
 */
QDialog *DeRestPlugin::createDialog()
{
    if (!m_w)
    {
        m_w = new DeRestWidget(0);
    }

    return m_w;
}

/*! Checks if a request is addressed to this plugin.
    \param hdr - the http header of the request
    \return true - if the request could be processed
 */
bool DeRestPlugin::isHttpTarget(const QHttpRequestHeader &hdr)
{
    if (hdr.path().startsWith(QLatin1String("/api")))
    {
        return true;
    }
    else if (hdr.path().startsWith(QLatin1String("/description.xml")))
    {
        if (!d->descriptionXml.isEmpty())
        {
            return true;
        }
    }

    return false;
}

/*! Broker for any incoming REST API request.
    \param hdr - http request header
    \param sock - the client socket
    \return 0 - on success
           -1 - on error
 */
int DeRestPlugin::handleHttpRequest(const QHttpRequestHeader &hdr, QTcpSocket *sock)
{
    QString content;
    QTextStream stream(sock);
    QHttpRequestHeader hdrmod(hdr);

    stream.setCodec(QTextCodec::codecForName("UTF-8"));
    d->pushClientForClose(sock, 10, hdr);

    if (m_state == StateOff)
    {
        if (d->apsCtrl && (d->apsCtrl->networkState() == deCONZ::InNetwork))
        {
            m_state = StateIdle;
        }
    }

    if (hdrmod.path().startsWith(QLatin1String("/api")))
    {
        // some clients send /api123 instead of /api/123
        // correct the path here
        if (hdrmod.path().length() > 4 && hdrmod.path().at(4) != '/')
        {
            QString urlpath = hdrmod.url().toString();
            urlpath.insert(4, '/');
            hdrmod.setRequest(hdrmod.method(), urlpath);
        }
    }

    if (DBG_IsEnabled(DBG_HTTP))
    {
        DBG_Printf(DBG_HTTP, "HTTP API %s %s - %s\n", qPrintable(hdr.method()), qPrintable(hdrmod.url().toString()), qPrintable(sock->peerAddress().toString()));
    }

    if(hdr.hasKey(QLatin1String("Content-Type")) &&
       hdr.value(QLatin1String("Content-Type")).startsWith(QLatin1String("multipart/form-data")))
    {
        if (DBG_IsEnabled(DBG_HTTP))
        {
            DBG_Printf(DBG_HTTP, "Binary Data: \t%s\n", qPrintable(content));
        }
    }
    else if (!stream.atEnd())
    {
        content = stream.readAll();
        if (DBG_IsEnabled(DBG_HTTP))
        {
            DBG_Printf(DBG_HTTP, "Text Data: \t%s\n", qPrintable(content));
        }
    }

    // we might be behind a proxy, do simple check
    if (d->gwAnnounceVital < 0 && d->gwProxyPort == 0)
    {
        if (hdr.hasKey(QLatin1String("Via")))
        {
            d->inetProxyCheckHttpVia(hdr.value(QLatin1String("Via")));
        }
    }

    QStringList path = hdrmod.path().split(QLatin1String("/"), QString::SkipEmptyParts);
    ApiRequest req(hdrmod, path, sock, content);
    ApiResponse rsp;

    rsp.httpStatus = HttpStatusNotFound;
    rsp.contentType = HttpContentHtml;

    int ret = REQ_NOT_HANDLED;

     // general response to a OPTIONS HTTP method
    if (req.hdr.method() == QLatin1String("OPTIONS"))
    {
        QString origin('*');
        if (hdr.hasKey(QLatin1String("Origin")))
        {
            origin = hdr.value(QLatin1String("Origin"));
        }

        stream << "HTTP/1.1 200 OK\r\n";
        stream << "Cache-Control: no-store, no-cache, must-revalidate, post-check=0, pre-check=0\r\n";
        stream << "Pragma: no-cache\r\n";
        stream << "Connection: close\r\n";
        stream << "Access-Control-Max-Age: 0\r\n";
        stream << "Access-Control-Allow-Origin: " << origin << " \r\n";
        stream << "Access-Control-Allow-Credentials: true\r\n";
        stream << "Access-Control-Allow-Methods: POST, GET, OPTIONS, PUT, DELETE\r\n";
        stream << "Access-Control-Allow-Headers: Authorization, Access-Control-Allow-Origin, Content-Type\r\n";
        stream << "Access-Control-Expose-Headers: Gateway-Name, Gateway-Uuid\r\n";
        stream << "Content-Type: text/html\r\n";
        stream << "Content-Length: 0\r\n";
        stream << "Gateway-Name: " << d->gwName << "\r\n";
        stream << "Gateway-Uuid: " << d->gwUuid << "\r\n";
        stream << "\r\n";
        req.sock->flush();
        return 0;
    }

    else if (req.hdr.method() == QLatin1String("POST") && path.size() == 2 && path[1] == QLatin1String("fileupload"))
    {
        QString path = deCONZ::getStorageLocation(deCONZ::ApplicationsDataLocation);
        QString filename = path + "/deCONZ.tar.gz";

        QFile file(filename);
        if (file.exists())
        {
            file.remove();
        }
        if ( file.open(QIODevice::ReadWrite) )
        {
            QByteArray data;
            while (sock->bytesAvailable())
            {
                data = sock->readAll();
            }
            //
            // cut off header of data
            // first 4 lines and last 2 lines of data are header-data
            QList<QByteArray> list = data.split('\n');
            for (int i = 4; i < list.size()-2; i++)
            {
                file.write(list[i]+"\n");
            }
            file.close();
        }

        stream << "HTTP/1.1 200 OK\r\n";
        stream << "Content-type: text/html\r\n";
        stream << "Content-Length: 0\r\n";
        stream << "Access-Control-Max-Age: 0\r\n";
        stream << "Access-Control-Allow-Origin: *\r\n";
        stream << "Access-Control-Allow-Methods: POST, GET, OPTIONS, PUT, DELETE\r\n";
        stream << "Access-Control-Allow-Headers: Authorization, Access-Control-Allow-Origin, Content-Type\r\n";
        stream << "\r\n";
        stream.flush();
        return 0;
    }

    else if (hdr.path().startsWith(QLatin1String("/description.xml")) && (hdr.method() == QLatin1String("GET")))
    {
        rsp.httpStatus = HttpStatusOk;
        rsp.contentType = HttpContentHtml;
        rsp.str = d->descriptionXml;

        if (d->descriptionXml.isEmpty())
        {
            return -1;
        }
        stream << "HTTP/1.1 " << HttpStatusOk << "\r\n";
        stream << "Content-Type: application/xml\r\n";
        stream << "Content-Length:" << QString::number(d->descriptionXml.size()) << "\r\n";
        stream << "Connection: close\r\n";
        stream << "\r\n";
        stream << d->descriptionXml.constData();
        stream.flush();
        return 0;
    }

    else if (req.path[0] == QLatin1String("api"))
    {
        // POST /api
        if ((req.path.size() == 1) && (req.hdr.method() == QLatin1String("POST")))
        {
            ret = d->createUser(req, rsp);
        }
        // GET /api/challenge
        else if ((req.path.size() == 2) && (req.hdr.method() == QLatin1String("GET")) && (req.path[1] == QLatin1String("challenge")))
        {
            ret = d->getChallenge(req, rsp);
        }
        // GET /api/config
        else if ((req.path.size() == 2) && (req.hdr.method() == QLatin1String("GET")) && (req.path[1] == QLatin1String("config")))
        {
            ret = d->getBasicConfig(req, rsp);
        }
        // PUT /api/<apikey>/config/wifi/updated
        else if ((req.path.size() == 5) && (req.hdr.method() == QLatin1String("PUT")) && (req.path[2] == QLatin1String("config")) && (req.path[3] == QLatin1String("wifi")) && (req.path[4] == QLatin1String("updated")))
        {
            ret = d->putWifiUpdated(req, rsp);
        }
        // PUT /api/<apikey>/config/wifi/scanresult
        else if ((req.path.size() == 5) && (req.hdr.method() == QLatin1String("PUT")) && (req.path[2] == QLatin1String("config")) && (req.path[3] == QLatin1String("wifi")) && (req.path[4] == QLatin1String("scanresult")))
        {
            ret = d->putWifiScanResult(req, rsp);
        }
        // DELETE /api/config/password
        else if ((req.path.size() == 3) && (req.hdr.method() == QLatin1String("DELETE")) && (req.path[1] == QLatin1String("config")) && (req.path[2] == QLatin1String("password")))
        {
            ret = d->deletePassword(req, rsp);
        }
        else if ((req.path.size() >= 2) && !(d->checkApikeyAuthentification(req, rsp)))
        {
            // GET /api/<nouser>/config
            if ((req.path.size() == 3) && (req.path[2] == QLatin1String("config")))
            {
                ret = d->getBasicConfig(req, rsp);
            }
            else
            {
                ret = REQ_READY_SEND;
            }
        }
        else if (req.path.size() >= 2) // && checkApikeyAuthentification(req, rsp)
        {
            bool resourceExist = true;

            // GET /api/<apikey>
            if ((req.path.size() == 2) && (req.hdr.method() == QLatin1String("GET")))
            {
                ret = d->getFullState(req, rsp);
            }
            else if (path[2] == QLatin1String("lights"))
            {
                ret = d->handleLightsApi(req, rsp);
            }
            else if (path[2] == QLatin1String("groups"))
            {
                ret = d->handleGroupsApi(req, rsp);
            }
            else if (path[2] == QLatin1String("schedules"))
            {
                ret = d->handleSchedulesApi(req, rsp);
            }
            else if (path[2] == QLatin1String("scenes"))
            {
                ret = d->handleScenesApi(req, rsp);
            }
            else if (path[2] == QLatin1String("sensors"))
            {
                ret = d->handleSensorsApi(req, rsp);
            }
            else if (path[2] == QLatin1String("rules"))
            {
                ret = d->handleRulesApi(req, rsp);
            }
            else if (path[2] == QLatin1String("config"))
            {
                ret = d->handleConfigurationApi(req, rsp);
            }
            else if (path[2] == QLatin1String("info"))
            {
                ret = d->handleInfoApi(req, rsp);
            }
            else if (path[2] == QLatin1String("resourcelinks"))
            {
                ret = d->handleResourcelinksApi(req, rsp);
            }
            else if (path[2] == QLatin1String("capabilities"))
            {
                ret = d->handleCapabilitiesApi(req, rsp);
            }
            else if (path[2] == QLatin1String("touchlink"))
            {
                ret = d->handleTouchlinkApi(req, rsp);
            }
            else if (path[2] == QLatin1String("userparameter"))
            {
                ret = d->handleUserparameterApi(req, rsp);
            }
            else if (path[2] == QLatin1String("gateways"))
            {
                ret = d->handleGatewaysApi(req, rsp);
            }
            else
            {
                resourceExist = false;
            }

            if (ret == REQ_NOT_HANDLED)
            {
                const QStringList ls = req.path.mid(2);
                const QString resource = "/" + ls.join("/");
                if (resourceExist && req.hdr.method() == QLatin1String("GET"))
                {
                    rsp.list.append(d->errorToMap(ERR_RESOURCE_NOT_AVAILABLE, resource, "resource, " + resource + ", not available"));
                }
                else
                {
                    rsp.list.append(d->errorToMap(ERR_METHOD_NOT_AVAILABLE, resource, "method, " + req.hdr.method() + ", not available for resource, " + resource));
                }
                rsp.httpStatus = HttpStatusNotFound;
                ret = REQ_READY_SEND;
            }
        }
    }

    if (ret == REQ_NOT_HANDLED)
    {
        DBG_Printf(DBG_HTTP, "%s unknown request: %s\n", Q_FUNC_INFO, qPrintable(hdr.path()));
    }

    QString str;

    if (!rsp.map.isEmpty())
    {
        rsp.contentType = HttpContentJson;
        str.append(Json::serialize(rsp.map));
    }
    else if (!rsp.list.isEmpty())
    {
        rsp.contentType = HttpContentJson;
        str.append(Json::serialize(rsp.list));
    }
    else if (!rsp.str.isEmpty())
    {
        rsp.contentType = HttpContentJson;
        str = rsp.str;
    }

    // some client may not be prepared for http return codes other than 200 OK
    if (rsp.httpStatus != HttpStatusOk && req.strict)
    {
        rsp.httpStatus = HttpStatusOk;
    }

    stream << "HTTP/1.1 " << rsp.httpStatus << "\r\n";
    stream << "Access-Control-Allow-Origin: *\r\n";
    stream << "Content-Type: " << rsp.contentType << "\r\n";
    stream << "Content-Length:" << QString::number(str.toUtf8().size()) << "\r\n";

    if (!rsp.hdrFields.empty())
    {
        QList<QPair<QString, QString> >::iterator i = rsp.hdrFields.begin();
        QList<QPair<QString, QString> >::iterator end = rsp.hdrFields.end();

        for (; i != end; ++i)
        {
            stream << i->first << ": " <<  i->second << "\r\n";
        }
    }

    if (!rsp.etag.isEmpty())
    {
        stream << "ETag:" << rsp.etag  << "\r\n";
    }
    stream << "\r\n";

    if (!str.isEmpty())
    {
        stream << str;
    }

    stream.flush();
    if (!str.isEmpty())
    {
        DBG_Printf(DBG_HTTP, "%s\n", qPrintable(str));
    }

    return 0;
}

/*! A client socket was disconnected cleanup here.
    \param sock - the client
 */
void DeRestPlugin::clientGone(QTcpSocket *sock)
{
    d->eventListeners.remove(sock);
}

bool DeRestPlugin::pluginActive() const
{
    if (m_w)
    {
        return m_w->pluginActive();
    }
    return true;
}

/*! Checks if some tcp connections could be closed.
 */
void DeRestPluginPrivate::openClientTimerFired()
{
    std::vector<TcpClient>::iterator i = openClients.begin();
    std::vector<TcpClient>::iterator end = openClients.end();

    for ( ; i != end; ++i)
    {
        i->closeTimeout--;

        if (i->closeTimeout == 0)
        {
            i->closeTimeout = -1;

            DBG_Assert(i->sock != 0);

            if (i->sock)
            {
                QTcpSocket *sock = i->sock;

                if (sock->state() == QTcpSocket::ConnectedState)
                {
                    DBG_Printf(DBG_INFO_L2, "Close socket port: %u\n", sock->peerPort());
                    sock->close();
                }
                else
                {
                    DBG_Printf(DBG_INFO_L2, "Close socket state = %d\n", sock->state());
                }

                sock->deleteLater();
                return;
            }
        }
        else if (i->closeTimeout < -120)
        {
            // cleanup here if not already deleted by socket destroyed slot
            *i = openClients.back();
            openClients.pop_back();
            return;
        }
    }
}

/*! Is called before the client socket will be deleted.
 */
void DeRestPluginPrivate::clientSocketDestroyed()
{
    QObject *obj = sender();
    QTcpSocket *sock = static_cast<QTcpSocket *>(obj);

    std::vector<TcpClient>::iterator i = openClients.begin();
    std::vector<TcpClient>::iterator end = openClients.end();

    for ( ; i != end; ++i)
    {
        if (i->sock == sock)
        {
            //int dt = i->created.secsTo(QDateTime::currentDateTime());
            //DBG_Printf(DBG_INFO, "remove socket %s : %u after %d s, %s\n", qPrintable(sock->peerAddress().toString()), sock->peerPort(), dt, qPrintable(i->hdr.path()));
            *i = openClients.back();
            openClients.pop_back();
            return;
        }
    }
}

/*! Returns the endpoint number of the HA endpoint.
    \return 1..254 - on success
            1 - if not found as default
 */
uint8_t DeRestPluginPrivate::endpoint()
{
    if (haEndpoint != 0)
    {
        return haEndpoint;
    }

    const deCONZ::Node *node;

    if (apsCtrl && apsCtrl->getNode(0, &node) == 0)
    {
        std::vector<uint8_t> eps = node->endpoints();

        std::vector<uint8_t>::const_iterator i = eps.begin();
        std::vector<uint8_t>::const_iterator end = eps.end();

        for (; i != end; ++i)
        {
            deCONZ::SimpleDescriptor sd;
            if (node->copySimpleDescriptor(*i, &sd) == 0)
            {
                if (sd.profileId() == HA_PROFILE_ID)
                {
                    haEndpoint = sd.endpoint();
                    return haEndpoint;
                }
            }
        }
    }

    return 1;
}

QString DeRestPluginPrivate::generateUniqueId(quint64 extAddress, quint8 endpoint, quint16 clusterId)
{
    QString uid;
    union _a
    {
        quint8 bytes[8];
        quint64 mac;
    } a;
    a.mac = extAddress;

    if (clusterId != 0)
    {
        uid.sprintf("%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x-%02x-%04x",
                    a.bytes[7], a.bytes[6], a.bytes[5], a.bytes[4],
                    a.bytes[3], a.bytes[2], a.bytes[1], a.bytes[0],
                    endpoint, clusterId);
    }
    else if (endpoint != 0)
    {
        uid.sprintf("%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x-%02x",
                    a.bytes[7], a.bytes[6], a.bytes[5], a.bytes[4],
                    a.bytes[3], a.bytes[2], a.bytes[1], a.bytes[0],
                    endpoint);
    }
    else
    {
        uid.sprintf("%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
                    a.bytes[7], a.bytes[6], a.bytes[5], a.bytes[4],
                    a.bytes[3], a.bytes[2], a.bytes[1], a.bytes[0]);
    }
    return uid;
}

/*! Returns the name of this plugin.
 */
const char *DeRestPlugin::name()
{
    return "REST API Plugin";
}

/*! Export the deCONZ network settings to a file.
 */
bool DeRestPluginPrivate::exportConfiguration()
{
    if (apsCtrl)
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

        QVariantMap map;
        map["deviceType"] = deviceType;
        map["panId"] = QString("0x%1").arg(QString::number(panId,16));
        map["extPanId"] = QString("0x%1").arg(QString::number(extPanId,16));
        map["apsUseExtPanId"] = QString("0x%1").arg(QString::number(apsUseExtPanId,16));
        map["macAddress"] = QString("0x%1").arg(QString::number(macAddress,16));
        map["staticNwkAddress"] = (staticNwkAddress == 0) ? false : true;
        map["nwkAddress"] = QString("0x%1").arg(QString::number(nwkAddress,16));
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

        bool success = true;
        QString saveString = Json::serialize(map, success);

        if (success)
        {
            //create config file
            QString path = deCONZ::getStorageLocation(deCONZ::ApplicationsDataLocation);
            QString filename = path + "/deCONZ.conf";

            QFile file(filename);
            if (file.exists())
            {
                file.remove();
            }
            if ( file.open(QIODevice::ReadWrite) )
            {
                QTextStream stream( &file );
                stream << saveString << endl;
                file.close();
            }

            //create .tar
            if (!archProcess)
            {
                archProcess = new QProcess(this);
            }

            ttlDataBaseConnection = 0;
            closeDb();

            //TODO: OS X
#ifdef Q_OS_WIN
            QString appPath = qApp->applicationDirPath();
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
            archProcess->start(cmd, args);
#endif
#ifdef Q_OS_LINUX
            archProcess->start("tar -cf " + path + "/deCONZ.tar -C " + path + " deCONZ.conf zll.db session.default");
#endif
            archProcess->waitForFinished(EXT_PROCESS_TIMEOUT);
            DBG_Printf(DBG_INFO, "%s\n", qPrintable(archProcess->readAllStandardOutput()));
            archProcess->deleteLater();
            archProcess = 0;

            //create .tar.gz
            if (!zipProcess)
            {
                zipProcess = new QProcess(this);
            }
#ifdef Q_OS_WIN

            cmd = appPath + "/7za.exe";
            args.clear();
            args.append("a");
            args.append(path + "/deCONZ.tar.gz");
            args.append(path + "/deCONZ.tar");
            zipProcess->start(cmd, args);
#endif
#ifdef Q_OS_LINUX
            zipProcess->start("gzip -f " + path + "/deCONZ.tar");
#endif
            zipProcess->waitForFinished(EXT_PROCESS_TIMEOUT);
            DBG_Printf(DBG_INFO, "%s\n", qPrintable(zipProcess->readAllStandardOutput()));
            zipProcess->deleteLater();
            zipProcess = 0;

            //delete config file
            if (file.exists())
            {
                file.remove();
            }
            //delete .tar file
            filename = path + "/deCONZ.tar";
            QFile file2(filename);
            if (file2.exists())
            {
                file2.remove();
            }
            return success;
        }
    }
    else
    {
        return false;
    }
    return false;
}

/*! Import the deCONZ network settings from a file.
 */
bool DeRestPluginPrivate::importConfiguration()
{
    if (apsCtrl)
    {
        QString path = deCONZ::getStorageLocation(deCONZ::ApplicationsDataLocation);
        QString filename = path + "/deCONZ.conf";
        QString jsonString = "";

        //decompress .tar.gz
        if (!archProcess)
        {
            archProcess = new QProcess(this);
        }
        //TODO: OS X
#ifdef Q_OS_WIN
        QString appPath = qApp->applicationDirPath();
        QString cmd = appPath + "/7za.exe";
        QStringList args;
        args.append("e");
        args.append("-y");
        args.append(path + "/deCONZ.tar.gz");
        args.append("-o" + path);
        archProcess->start(cmd, args);
#endif
#ifdef Q_OS_LINUX
        archProcess->start("gzip -df " + path + "/deCONZ.tar.gz");
#endif
        archProcess->waitForFinished(EXT_PROCESS_TIMEOUT);
        DBG_Printf(DBG_INFO, "%s\n", qPrintable(archProcess->readAllStandardOutput()));
        archProcess->deleteLater();
        archProcess = 0;

        ttlDataBaseConnection = 0;
        closeDb();

        //unpack .tar
        if (!zipProcess)
        {
            zipProcess = new QProcess(this);
        }
#ifdef Q_OS_WIN
        cmd = appPath + "/7za.exe";
        args.clear();
        args.append("e");
        args.append("-y");
        args.append(path + "/deCONZ.tar");
        args.append("-o" + path);
        zipProcess->start(cmd, args);
#endif
#ifdef Q_OS_LINUX
        zipProcess->start("tar -xf " + path + "/deCONZ.tar -C " + path);
#endif
        zipProcess->waitForFinished(EXT_PROCESS_TIMEOUT);
        DBG_Printf(DBG_INFO, "%s\n", qPrintable(zipProcess->readAllStandardOutput()));
        zipProcess->deleteLater();
        zipProcess = 0;

        QFile file(filename);
        if ( file.open(QIODevice::ReadOnly) )
        {
            QTextStream stream( &file );

            stream >> jsonString;

            bool ok;
            QVariant var = Json::parse(jsonString, ok);
            QVariantMap map = var.toMap();

            if (ok)
            {
                uint8_t deviceType = map["deviceType"].toUInt();
                uint16_t panId =  QString(map["panId"].toString()).toUInt(&ok,16);
                quint64 extPanId =  QString(map["extPanId"].toString()).toULongLong(&ok,16);
                quint64 apsUseExtPanId = QString(map["apsUseExtPanId"].toString()).toULongLong(&ok,16);
                quint64 curMacAddress = apsCtrl->getParameter(deCONZ::ParamMacAddress);
                quint64 macAddress =  QString(map["macAddress"].toString()).toULongLong(&ok,16);
                uint8_t staticNwkAddress = (map["staticNwkAddress"].toBool() == true) ? 1 : 0;
                uint16_t nwkAddress = QString(map["nwkAddress"].toString()).toUInt(&ok,16);
                uint8_t apsAck = (map["apsAck"].toBool() == true) ? 1 : 0;
                //map["channelMask"] = channelMask;
                uint8_t curChannel = map["curChannel"].toUInt();
                if (map.contains("otauactive"))
                {
                    uint8_t otauActive = map["otauactive"].toUInt();
                    apsCtrl->setParameter(deCONZ::ParamOtauActive, otauActive);
                }
                uint8_t securityMode = map["securityMode"].toUInt();
                quint64 tcAddress =  QString(map["tcAddress"].toString()).toULongLong(&ok,16);
                QByteArray nwkKey = QByteArray::fromHex(map["networkKey"].toByteArray());
                QByteArray tcLinkKey = QByteArray::fromHex(map["tcLinkKey"].toByteArray());
                uint8_t currentNwkUpdateId = apsCtrl->getParameter(deCONZ::ParamNetworkUpdateId);
                uint8_t nwkUpdateId = map["nwkUpdateId"].toUInt();
                QVariantMap endpoint1 = map["endpoint1"].toMap();
                QVariantMap endpoint2 = map["endpoint2"].toMap();

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
                apsCtrl->setParameter(deCONZ::ParamHAEndpoint, endpoint1);
                apsCtrl->setParameter(deCONZ::ParamHAEndpoint, endpoint2);
            }

            //cleanup
            if (file.exists())
            {
                file.remove(); //deCONZ.conf
            }
            filename = path + "/deCONZ.tar";
            QFile file2(filename);
            if (file2.exists())
            {
                file2.remove();
            }
            filename = path + "/deCONZ.tar.gz";
            QFile file3(filename);
            if (file3.exists())
            {
                file3.remove();
            }
            return true;
        }
        else
        {
            //cleanup
            filename = path + "/deCONZ.tar";
            QFile file2(filename);
            if (file2.exists())
            {
                file2.remove();
            }
            filename = path + "/deCONZ.tar.gz";
            QFile file3(filename);
            if (file3.exists())
            {
                file3.remove();
            }
            return false;
        }
    }
    else
    {
        return false;
    }
}

/*! Reset the deCONZ network settings and/or delete Database.
 */
bool DeRestPluginPrivate::resetConfiguration(bool resetGW, bool deleteDB)
{
    if (apsCtrl)
    {
        if (resetGW)
        {
            qsrand(QDateTime::currentDateTime().toTime_t());
            uint8_t deviceType = deCONZ::Coordinator;
            uint16_t panId = qrand();
            quint64 apsUseExtPanId = 0x0000000000000000;
            uint16_t nwkAddress = 0x0000;
            //uint32_t channelMask = 33554432; // 25
            uint8_t curChannel = 11;
            gwZigbeeChannel = 11;
            uint8_t securityMode = 3;
            // TODO: original macAddress
            quint64 macAddress = apsCtrl->getParameter(deCONZ::ParamMacAddress);

            QByteArray nwkKey1 = QByteArray::number(qrand(), 16);
            QByteArray nwkKey2 = QByteArray::number(qrand(), 16);
            QByteArray nwkKey3 = QByteArray::number(qrand(), 16);
            QByteArray nwkKey4 = QByteArray::number(qrand(), 16);

            QByteArray nwkKey = nwkKey1.append(nwkKey2).append(nwkKey3).append(nwkKey4);
            nwkKey.resize(16);

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

            //reset Endpoint config
            QVariantMap epData;
            QVariantList inClusters;
            inClusters.append("0x0019");
            inClusters.append("0x000a");

            epData["index"] = 0;
            epData["endpoint"] = "0x1";
            epData["profileId"] = "0x104";
            epData["deviceId"] = "0x5";
            epData["deviceVersion"] = "0x1";
            epData["inClusters"] = inClusters;
            apsCtrl->setParameter(deCONZ::ParamHAEndpoint, epData);

            epData.clear();
            epData["index"] = 1;
            epData["endpoint"] = "0x50";
            epData["profileId"] = "0xde00";
            epData["deviceId"] = "0x1";
            epData["deviceVersion"] = "0x1";
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
                bool ret = QFile::copy(filename, newFilename);
                if (ret)
                {
                 DBG_Printf(DBG_INFO, "db backup success\n");
                }
                else
                {
                 DBG_Printf(DBG_INFO, "db backup failed\n");
                }
            }

            nodes.clear();
            groups.clear();
            sensors.clear();
            schedules.clear();
            apiAuths.clear();
            apiAuthCurrent = 0;

            openDb();
            clearDb();
            closeDb();
            DBG_Printf(DBG_INFO, "all database tables (except auth) cleared.\n");
        }
        return true;
    }
    else
    {
        return false;
    }
}

Resource *DeRestPluginPrivate::getResource(const char *resource, const QString &id)
{
    if (resource == RSensors)
    {
        return id.length() < MIN_UNIQUEID_LENGTH ? getSensorNodeForId(id) : getSensorNodeForUniqueId(id);
    }
    else if (resource == RLights)
    {
        return getLightNodeForId(id);
    }
    else if (resource == RGroups && !id.isEmpty())
    {
        return getGroupForId(id);
    }
    else if (resource == RConfig)
    {
        return &config;
    }

    return 0;
}

void DeRestPluginPrivate::pollSwUpdateStateTimerFired()
{
    if (gwSwUpdateState != swUpdateState.transferring &&
        gwSwUpdateState != swUpdateState.installing)
    {
        pollSwUpdateStateTimer->stop();
    }
    else
    {
        openDb();
        loadSwUpdateStateFromDb();
        closeDb();
    }
}

void DeRestPluginPrivate::pollDatabaseWifiTimerFired()
{
    if (saveDatabaseItems & DB_CONFIG)
    {
        DBG_Printf(DBG_INFO_L2, "Don't read wifi from db. Db save still in progress.\n");
        return;
    }

    openDb();
    loadWifiInformationFromDb();
    closeDb();
}

void DeRestPluginPrivate::restartAppTimerFired()
{
    reconnectTimer = new QTimer(this);
    reconnectTimer->setSingleShot(true);
    connect(reconnectTimer, SIGNAL(timeout()),
            this, SLOT(reconnectTimerFired()));

    //on rpi deCONZ is restarted if reconnect was successfull
    genericDisconnectNetwork();
}

void DeRestPluginPrivate::restartGatewayTimerFired()
{
     //qApp->exit(APP_RET_RESTART_SYS);
#ifdef ARCH_ARM
    if (reboot(RB_AUTOBOOT) == -1)
    {
        DBG_Printf(DBG_INFO, "Reboot failed with errno: %s\n", strerror(errno));
    }
#endif
}

void DeRestPluginPrivate::shutDownGatewayTimerFired()
{
     // qApp->exit(APP_RET_SHUTDOWN_SYS);
#ifdef ARCH_ARM
    if (reboot(RB_POWER_OFF) == -1)
    {
        DBG_Printf(DBG_INFO, "Shutdown failed with errno: %s\n", strerror(errno));
    }
#endif
}

void DeRestPluginPrivate::simpleRestartAppTimerFired()
{
    qApp->exit(APP_RET_RESTART_APP);
}

/*! Set sensor node attributes to deCONZ core (nodes and node list).
 */
void DeRestPluginPrivate::pushSensorInfoToCore(Sensor *sensor)
{
    DBG_Assert(sensor != 0);
    if (!sensor || sensor->deletedState() != Sensor::StateNormal)
    {
        return;
    }

    Q_Q(DeRestPlugin);

    if (sensor->modelId().startsWith(QLatin1String("FLS-NB")))
    { } // use name from light
    else if (sensor->modelId().startsWith(QLatin1String("D1")) || sensor->modelId().startsWith(QLatin1String("S2")) ||
             sensor->modelId().startsWith(QLatin1String("lumi.ctrl_")))
    { } // use name from light
    else if (sensor->type() == QLatin1String("ZHAConsumption") || sensor->type() == QLatin1String("ZHAPower"))
    { } // use name from light
    else if (sensor->modelId() == QLatin1String("SML001") && sensor->type() != QLatin1String("ZHAPresence"))
    { } // use name from ZHAPresence sensor only
    else if (sensor->modelId() == QLatin1String("WarningDevice") && sensor->type() == QLatin1String("ZHAAlarm"))
    { } // use name from light
    else if (!sensor->name().isEmpty())
    {
        q->nodeUpdated(sensor->address().ext(), QLatin1String("name"), sensor->name());
    }

    if (!sensor->modelId().isEmpty())
    {
        q->nodeUpdated(sensor->address().ext(), QLatin1String("modelid"), sensor->modelId());
    }

    if (!sensor->manufacturer().isEmpty())
    {
        q->nodeUpdated(sensor->address().ext(), QLatin1String("vendor"), sensor->manufacturer());
    }

    if (!sensor->swVersion().isEmpty())
    {
        q->nodeUpdated(sensor->address().ext(), QLatin1String("version"), sensor->swVersion());
    }
}

/*! Selects the next device to poll.
 */
void DeRestPluginPrivate::pollNextDevice()
{
    DBG_Assert(apsCtrl != 0);

    if (!apsCtrl)
    {
        return;
    }

    QTime t = QTime::currentTime();
    if (queryTime > t)
    {
        return;
    }

    RestNodeBase *restNode = 0;

    if (pollNodes.empty()) // TODO time based
    {
        for (LightNode &l : nodes)
        {
            if (l.isAvailable())
            {
                pollNodes.push_back(&l);
            }
        }

        for (Sensor &s : sensors)
        {
            if (s.isAvailable() && s.node() && s.node()->nodeDescriptor().receiverOnWhenIdle())
            {
                pollNodes.push_back(&s);
            }
        }
    }

    if (!pollNodes.empty())
    {
        restNode = pollNodes.back();
        pollNodes.pop_back();
    }

    if (restNode && restNode->isAvailable())
    {
        DBG_Printf(DBG_INFO, "poll node %s\n", qPrintable(restNode->uniqueId()));
        pollManager->poll(restNode);
        queryTime = queryTime.addSecs(6);
    }
}

/*! Request to disconnect from network.
 */
void DeRestPluginPrivate::genericDisconnectNetwork()
{
    DBG_Assert(apsCtrl != 0);

    if (!apsCtrl)
    {
        return;
    }

    networkDisconnectAttempts = NETWORK_ATTEMPS;
    networkConnectedBefore = gwRfConnectedExpected;
    networkState = DisconnectingNetwork;
    DBG_Printf(DBG_INFO_L2, "networkState: DisconnectingNetwork\n");

    apsCtrl->setNetworkState(deCONZ::NotInNetwork);

    reconnectTimer->start(DISCONNECT_CHECK_DELAY);
}

/*! Checks if network is disconnected to proceed with further actions.
 */
void DeRestPluginPrivate::checkNetworkDisconnected()
{
    if (networkState != DisconnectingNetwork)
    {
        return;
    }

    if (networkDisconnectAttempts > 0)
    {
        networkDisconnectAttempts--;
    }

    if (isInNetwork())
    {
        if (networkDisconnectAttempts == 0)
        {
            DBG_Printf(DBG_INFO, "disconnect from network failed.\n");

            // even if we seem to be connected force a delayed reconnect attemp to
            // prevent the case that the disconnect happens shortly after here
            startReconnectNetwork(RECONNECT_CHECK_DELAY);
        }
        else
        {
            DBG_Assert(apsCtrl != 0);
            if (apsCtrl)
            {
                DBG_Printf(DBG_INFO, "disconnect from network failed, try again\n");
                apsCtrl->setNetworkState(deCONZ::NotInNetwork);
                reconnectTimer->start(DISCONNECT_CHECK_DELAY);
            }
        }

        return;
    }
    startReconnectNetwork(RECONNECT_NOW);
}

/*! Reconnect to previous network state, trying serveral times if necessary.
    \param delay - the delay after which reconnecting shall be started
 */
void DeRestPluginPrivate::startReconnectNetwork(int delay)
{
    networkState = ReconnectNetwork;
    DBG_Printf(DBG_INFO_L2, "networkState: CC_ReconnectNetwork\n");
    networkReconnectAttempts = NETWORK_ATTEMPS;

    DBG_Printf(DBG_INFO, "start reconnect to network\n");

    reconnectTimer->stop();
    if (delay > 0)
    {
        reconnectTimer->start(delay);
    }
    else
    {
        reconnectNetwork();
    }
}

/*! Helper to reconnect to previous network state, trying serveral times if necessary.
 */
void DeRestPluginPrivate::reconnectNetwork()
{
    if (networkState != ReconnectNetwork)
    {
        return;
    }

    if (isInNetwork())
    {
        DBG_Printf(DBG_INFO, "reconnect network done\n");
        //restart deCONZ on rpi to apply changes to MACAddress
        #ifdef ARCH_ARM
        qApp->exit(APP_RET_RESTART_APP);
        //QProcess restartProcess;
        //QString cmd = "systemctl restart deconz";
        //restartProcess.start(cmd);
        #endif
        return;
    }

    // respect former state
    if (!networkConnectedBefore)
    {
        DBG_Printf(DBG_INFO, "network was not connected before\n");
        return;
    }

    if (networkReconnectAttempts > 0)
    {
        if (apsCtrl->networkState() != deCONZ::Connecting)
        {
           networkReconnectAttempts--;

            if (apsCtrl->setNetworkState(deCONZ::InNetwork) != deCONZ::Success)
            {
                DBG_Printf(DBG_INFO, "failed to reconnect to network try=%d\n", (NETWORK_ATTEMPS - networkReconnectAttempts));
            }
            else
            {
                DBG_Printf(DBG_INFO, "try to reconnect to network try=%d\n", (NETWORK_ATTEMPS - networkReconnectAttempts));
            }
        }

        reconnectTimer->start(RECONNECT_CHECK_DELAY);
    }
    else
    {
        DBG_Printf(DBG_INFO, "reconnect network failed\n");
    }
}

/*! Starts a delayed action based on current networkState.
 */
void DeRestPluginPrivate::reconnectTimerFired()
{
    switch (networkState)
    {
    case ReconnectNetwork:
        reconnectNetwork();
        break;

    case DisconnectingNetwork:
        checkNetworkDisconnected();
        break;

    default:
        DBG_Printf(DBG_INFO, "reconnectTimerFired() unhandled state %d\n", networkState);
        break;
    }
}

#if QT_VERSION < 0x050000
Q_EXPORT_PLUGIN2(de_rest_plugin, DeRestPlugin)
#endif
