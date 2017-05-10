/*
 * Copyright (c) 2017 dresden elektronik ingenieurtechnik gmbh.
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
#include <queue>
#include "colorspace.h"
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "de_web_widget.h"
#include "gateway_scanner.h"
#include "json.h"

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

struct SupportedDevice {
    quint16 vendorId;
    const char *modelId;
};

static const SupportedDevice supportedDevices[] = {
    { VENDOR_BUSCH_JAEGER, "RB01" },
    { VENDOR_BUSCH_JAEGER, "RM01" },
    { VENDOR_CLIMAX, "LM_" },
    { VENDOR_CLIMAX, "LMHT_" },
    { VENDOR_CLIMAX, "IR_" },
    { VENDOR_CLIMAX, "DC_" },
    { VENDOR_CLIMAX, "OJB-IR715-Z" },
    { VENDOR_DDEL, "Lighting Switch" },
    { VENDOR_DDEL, "Scene Switch" },
    { VENDOR_DDEL, "FLS-NB1" },
    { VENDOR_DDEL, "FLS-NB2" },
    { VENDOR_IKEA, "TRADFRI remote control" },
    { VENDOR_IKEA, "TRADFRI motion sensor" },
    { VENDOR_INSTA, "Remote" },
    { VENDOR_INSTA, "HS_4f_GJ_1" },
    { VENDOR_INSTA, "WS_4f_J_1" },
    { VENDOR_INSTA, "WS_3f_G_1" },
    { VENDOR_NYCE, "3011" }, // door/window sensor
    { VENDOR_PHILIPS, "RWL020" },
    { VENDOR_PHILIPS, "RWL021" },
    { VENDOR_PHILIPS, "SML001" },
    { 0, 0 }
};

ApiRequest::ApiRequest(const QHttpRequestHeader &h, const QStringList &p, QTcpSocket *s, const QString &c) :
    hdr(h), path(p), sock(s), content(c), version(ApiVersion_1)
{
    if (hdr.hasKey("Accept"))
    {
        if (hdr.value("Accept").contains("vnd.ddel.v1"))
        {
            version = ApiVersion_1_DDEL;
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

    return QString("");
}

/*! Constructor for pimpl.
    \param parent - the main plugin
 */
DeRestPluginPrivate::DeRestPluginPrivate(QObject *parent) :
    QObject(parent)
{
    databaseTimer = new QTimer(this);
    databaseTimer->setSingleShot(true);

    initEventQueue();
    initResourceDescriptors();

    connect(databaseTimer, SIGNAL(timeout()),
            this, SLOT(saveDatabaseTimerFired()));

    webSocketServer = new WebSocketServer(this);

    gwScanner = new GatewayScanner(this);
    connect(gwScanner, SIGNAL(foundGateway(quint32,quint16,QString,QString)),
            this, SLOT(foundGateway(quint32,quint16,QString,QString)));
    gwScanner->startScan();

    db = 0;
    saveDatabaseItems = 0;
    saveDatabaseIdleTotalCounter = 0;
    sqliteDatabaseName = deCONZ::getStorageLocation(deCONZ::ApplicationsDataLocation) + QLatin1String("/zll.db");

    idleLimit = 0;
    idleTotalCounter = IDLE_READ_LIMIT;
    idleLastActivity = 0;
    sensorIndIdleTotalCounter = 0;
    queryTime = QTime::currentTime();
    udpSock = 0;
    haEndpoint = 0;
    gwGroupSendDelay = deCONZ::appArgumentNumeric("--group-delay", GROUP_SEND_DELAY);
    supportColorModeXyForGroups = false;
    groupDeviceMembershipChecked = false;
    gwLinkButton = false;

    // preallocate memory to get consistent pointers
    nodes.reserve(150);
    sensors.reserve(150);

    // Supported sensor types
    sensorTypes.append("CLIPSwitch");
    sensorTypes.append("CLIPOpenClose");
    sensorTypes.append("CLIPPresence");
    sensorTypes.append("CLIPTemperature");
    sensorTypes.append("CLIPGenericFlag");
    sensorTypes.append("CLIPGenericStatus");
    sensorTypes.append("CLIPHumidity");
    sensorTypes.append("Daylight");
    sensorTypes.append("ZGPSwitch");
    sensorTypes.append("ZHASwitch");
    sensorTypes.append("ZHALight");
    sensorTypes.append("ZHAPresence");
    sensorTypes.append("ZHATemperature");

    fastProbeTimer = new QTimer(this);
    fastProbeTimer->setInterval(500);
    fastProbeTimer->setSingleShot(true);
    connect(fastProbeTimer, SIGNAL(timeout()), this, SLOT(delayedFastEnddeviceProbe()));


    apsCtrl = deCONZ::ApsController::instance();
    DBG_Assert(apsCtrl != 0);

    apsCtrl->setParameter(deCONZ::ParamOtauActive, 0);

    // starttime reference counts from here
    starttimeRef.start();

    // default configuration
    gwRunFromShellScript = false;
    gwDeleteUnknownRules = (deCONZ::appArgumentNumeric("--delete-unknown-rules", 1) == 1) ? true : false;
    gwRfConnected = false; // will be detected later
    gwRfConnectedExpected = (deCONZ::appArgumentNumeric("--auto-connect", 1) == 1) ? true : false;
    gwPermitJoinDuration = 0;
    gwPermitJoinResend = 0;
    gwNetworkOpenDuration = 60;
    gwWifi = "not-configured";
    gwWifiType = "accesspoint";
    gwWifiName = "Not set";
    gwWifiChannel = "1";
    gwWifiIp ="192.168.8.1";
    gwWifiPw = "";
    gwRgbwDisplay = "1";
    gwTimezone = QString::fromStdString(getTimezone());
    gwTimeFormat = "12h";
    gwZigbeeChannel = 0;
    gwName = GW_DEFAULT_NAME;
    gwUpdateVersion = GW_SW_VERSION; // will be replaced by discovery handler
    gwUpdateChannel = "stable";
    gwReportingEnabled = (deCONZ::appArgumentNumeric("--reporting", 1) == 1) ? true : false;
    gwFirmwareNeedUpdate = false;
    gwFirmwareVersion = "0x00000000"; // query later
    gwFirmwareVersionUpdate = "";

    {
        QHttpRequestHeader hdr;
        QStringList path;
        QString content;
        ApiRequest dummyReq(hdr, path, 0, content);
        dummyReq.version = ApiVersion_1_DDEL;
        configToMap(dummyReq, gwConfig);
    }
    updateEtag(gwConfigEtag);
    updateEtag(gwSensorsEtag);
    updateEtag(gwGroupsEtag);
    updateEtag(gwLightsEtag);

    gwProxyPort = 0;
    gwProxyAddress = "none";

    // set some default might be overwritten by database
    gwAnnounceInterval = ANNOUNCE_INTERVAL;
    gwAnnounceUrl = "http://dresden-light.appspot.com/discover";
    inetDiscoveryManager = 0;

    archProcess = 0;
    zipProcess = 0;

    // sensors
    findSensorsState = FindSensorsIdle;
    findSensorsTimeout = 0;

    openDb();
    initDb();
    readDb();
    closeDb();

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

    initUpnpDiscovery();

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

    checkSensorsTimer = new QTimer(this);
    checkSensorsTimer->setSingleShot(false);
    checkSensorsTimer->setInterval(1000);
    connect(checkSensorsTimer, SIGNAL(timeout()),
            this, SLOT(checkSensorStateTimerFired()));
    checkSensorsTimer->start();

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

    saveCurrentRuleInDbTimer = new QTimer(this);
    saveCurrentRuleInDbTimer->setSingleShot(true);
    connect(saveCurrentRuleInDbTimer, SIGNAL(timeout()),
            this, SLOT(saveCurrentRuleInDbTimerFired()));

    resendPermitJoinTimer = new QTimer(this);
    resendPermitJoinTimer->setSingleShot(true);
    connect(resendPermitJoinTimer, SIGNAL(timeout()),
            this, SLOT(resendPermitJoinTimerFired()));

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

        default:
        {
        }
            break;
        }

        handleIndicationFindSensors(ind, zclFrame);

        if (ind.dstAddressMode() == deCONZ::ApsGroupAddress || ind.clusterId() == VENDOR_CLUSTER_ID)
        {
            if (zclFrame.isClusterCommand())
            {
                Sensor *sensorNode = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), ind.srcEndpoint());
                if (!sensorNode && zclFrame.manufacturerCode() == VENDOR_PHILIPS)
                {   // dimmer switch?
                    sensorNode = getSensorNodeForAddress(ind.srcAddress());
                }

                if (sensorNode)
                {
                    ResourceItem *item = sensorNode->item(RConfigReachable);
                    if (item && !item->toBool())
                    {
                        item->setValue(true);
                        Event e(RSensors, RConfigReachable, sensorNode->id());
                        enqueueEvent(e);
                    }
                    checkSensorButtonEvent(sensorNode, ind, zclFrame);
                }
            }
        }

        if (zclFrame.isProfileWideCommand() && zclFrame.commandId() == deCONZ::ZclReportAttributesId)
        {
            handleZclAttributeReportIndication(ind, zclFrame);
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
            handleIndicationFindSensors(ind, zclFrame);
        }
            break;

        case ZDP_DEVICE_ANNCE_CLID:
        {
            handleDeviceAnnceIndication(ind);
            handleIndicationFindSensors(ind, zclFrame);
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
    std::list<TaskItem>::iterator i = runningTasks.begin();
    std::list<TaskItem>::iterator end = runningTasks.end();

    for (;i != end; ++i)
    {
        TaskItem &task = *i;
        if (conf.dstAddressMode() == deCONZ::ApsNwkAddress &&
            conf.dstAddress().hasNwk() && task.req.dstAddress().hasNwk() &&
            conf.dstAddress().nwk() != task.req.dstAddress().nwk())
        {
            continue;
        }

        if (task.req.id() == conf.id())
        {
            if (conf.status() != deCONZ::ApsSuccessStatus)
            {
                DBG_Printf(DBG_INFO, "error APSDE-DATA.confirm: 0x%02X on task\n", conf.status());
            }

            DBG_Printf(DBG_INFO_L2, "Erase task zclSequenceNumber: %u send time %d\n", task.zclFrame.sequenceNumber(), idleTotalCounter - task.sendTime);
            runningTasks.erase(i);
            processTasks();

            return;
        }
    }

    if (handleMgmtBindRspConfirm(conf))
    {
        return;
    }

    if (channelChangeApsRequestId == conf.id())
    {
        channelChangeSendConfirm(conf.status() == deCONZ::ApsSuccessStatus);
    }
    if (resetDeviceApsRequestId == conf.id())
    {
        resetDeviceSendConfirm(conf.status() == deCONZ::ApsSuccessStatus);
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

    Event e(RSensors, RStateButtonEvent, sensor->id());
    enqueueEvent(e);
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
            if (findSensorsState != FindSensorsActive)
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

            DBG_Printf(DBG_INFO, "SensorNode %u: %s added\n", sensorNode.id().toUInt(), qPrintable(sensorNode.name()));
            updateSensorEtag(&sensorNode);

            sensorNode.setNeedSaveDatabase(true);
            sensors.push_back(sensorNode);

            Event e(RSensors, REventAdded, sensorNode.id());
            enqueueEvent(e);
            queSaveDb(DB_SENSORS , DB_SHORT_SAVE_DELAY);
        }
        else if (sensor && sensor->deletedState() == Sensor::StateDeleted)
        {
            if (findSensorsState == FindSensorsActive)
            {
                sensor->setDeletedState(Sensor::StateNormal);
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

    DBG_Printf(DBG_INFO, "API error %d, %s, %s\n", id, qPrintable(ressource), qPrintable(description));

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

/*! Adds new node(s) to node cache.
    Only supported ZLL and HA nodes will be added.
    \param node - the base for the LightNode
 */
void DeRestPluginPrivate::addLightNode(const deCONZ::Node *node)
{
    DBG_Assert(node != 0);
    if (!node || node->isEndDevice())
    {
        return;
    }

    QList<deCONZ::SimpleDescriptor>::const_iterator i = node->simpleDescriptors().constBegin();
    QList<deCONZ::SimpleDescriptor>::const_iterator end = node->simpleDescriptors().constEnd();

    for (;i != end; ++i)
    {
        LightNode lightNode;
        lightNode.setNode(0);
        lightNode.item(RStateReachable)->setValue(true);

        bool hasServerOnOff = false;
        bool hasServerLevel = false;
        bool hasServerColor = false;

        for (int c = 0; c < i->inClusters().size(); c++)
        {
            if      (i->inClusters()[c].id() == ONOFF_CLUSTER_ID) { hasServerOnOff = true; }
            else if (i->inClusters()[c].id() == LEVEL_CLUSTER_ID) { hasServerLevel = true; }
            else if (i->inClusters()[c].id() == COLOR_CLUSTER_ID) { hasServerColor = true; }
        }

        // check if node already exist
        LightNode *lightNode2 = getLightNodeForAddress(node->address(), i->endpoint());

        if (lightNode2)
        {
            if (lightNode2->node() != node)
            {
                lightNode2->setNode(const_cast<deCONZ::Node*>(node));
                DBG_Printf(DBG_INFO, "LightNode %s set node %s\n", qPrintable(lightNode2->id()), qPrintable(node->address().toStringExt()));
            }

            lightNode2->setManufacturerCode(node->nodeDescriptor().manufacturerCode());
            ResourceItem *reachable = lightNode2->item(RStateReachable);

            if (!reachable->toBool())
            {
                // the node existed before
                // refresh all with new values
                DBG_Printf(DBG_INFO, "LightNode %u: %s updated\n", lightNode2->id().toUInt(), qPrintable(lightNode2->name()));
                reachable->setValue(true);
                Event e(RLights, RStateReachable, lightNode2->id());
                enqueueEvent(e);

                lightNode2->enableRead(READ_VENDOR_NAME |
                                       READ_MODEL_ID |
                                       READ_SWBUILD_ID |
                                       READ_COLOR |
                                       READ_LEVEL |
                                       READ_ON_OFF |
                                       READ_GROUPS |
                                       READ_SCENES |
                                       READ_BINDING_TABLE);

                for (uint32_t i = 0; i < 32; i++)
                {
                    uint32_t item = 1 << i;
                    if (lightNode.mustRead(item))
                    {
                        lightNode.setNextReadTime(item, queryTime);
                        lightNode.setLastRead(item, idleTotalCounter);
                    }

                }

                queryTime = queryTime.addSecs(1);

                //lightNode2->setLastRead(idleTotalCounter);
                updateEtag(lightNode2->etag);
            }

            if (lightNode2->uniqueId().isEmpty() || lightNode2->uniqueId().startsWith("0x"))
            {
                QString uid = generateUniqueId(lightNode2->address().ext(), lightNode2->haEndpoint().endpoint(), 0);
                lightNode2->setUniqueId(uid);
                lightNode2->setNeedSaveDatabase(true);
                updateEtag(lightNode2->etag);
            }

            continue;
        }

        if (!i->inClusters().isEmpty())
        {
            if (i->profileId() == HA_PROFILE_ID)
            {
                // filter for supported devices
                switch (i->deviceId())
                {
                case DEV_ID_MAINS_POWER_OUTLET:
                case DEV_ID_HA_ONOFF_LIGHT:
                case DEV_ID_ONOFF_OUTPUT:
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
                case DEV_ID_Z30_COLOR_TEMPERATURE_LIGHT:
                    {
                        if (hasServerOnOff)
                        {
                            lightNode.setHaEndpoint(*i);
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
                case DEV_ID_ZLL_COLOR_TEMPERATURE_LIGHT:
                case DEV_ID_Z30_COLOR_TEMPERATURE_LIGHT:
                case DEV_ID_ZLL_DIMMABLE_LIGHT:
                case DEV_ID_ZLL_DIMMABLE_PLUGIN_UNIT:
                case DEV_ID_ZLL_ONOFF_LIGHT:
                case DEV_ID_ZLL_ONOFF_PLUGIN_UNIT:
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

        if (lightNode.haEndpoint().isValid())
        {
            Q_Q(DeRestPlugin);
            lightNode.setNode(const_cast<deCONZ::Node*>(node));
            lightNode.address() = node->address();
            lightNode.setManufacturerCode(node->nodeDescriptor().manufacturerCode());

            QString uid = generateUniqueId(lightNode.address().ext(), lightNode.haEndpoint().endpoint(), 0);
            lightNode.setUniqueId(uid);

            openDb();
            loadLightNodeFromDb(&lightNode);
            closeDb();

            if (lightNode.id().isEmpty())
            {
                openDb();
                lightNode.setId(QString::number(getFreeLightId()));
                closeDb();
            }

            if (lightNode.name().isEmpty())
            {
                lightNode.setName(QString("Light %1").arg(lightNode.id()));
            }

            if (!lightNode.name().isEmpty())
            { q->nodeUpdated(lightNode.address().ext(), QLatin1String("name"), lightNode.name()); }

            if (!lightNode.swBuildId().isEmpty())
            { q->nodeUpdated(lightNode.address().ext(), QLatin1String("version"), lightNode.swBuildId()); }

            if (!lightNode.manufacturer().isEmpty())
            { q->nodeUpdated(lightNode.address().ext(), QLatin1String("vendor"), lightNode.manufacturer()); }

            if (!lightNode.modelId().isEmpty())
            { q->nodeUpdated(lightNode.address().ext(), QLatin1String("modelid"), lightNode.modelId()); }

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
            for (uint32_t i = 0; i < 32; i++)
            {
                uint32_t item = 1 << i;
                if (lightNode.mustRead(item))
                {
                    lightNode.setNextReadTime(item, queryTime);
                    lightNode.setLastRead(item, idleTotalCounter);
                }
            }
            lightNode.setLastAttributeReportBind(idleTotalCounter);
            queryTime = queryTime.addSecs(1);

            DBG_Printf(DBG_INFO, "LightNode %u: %s added\n", lightNode.id().toUInt(), qPrintable(lightNode.name()));
            lightNode.setNeedSaveDatabase(true);
            nodes.push_back(lightNode);
            lightNode2 = &nodes.back();

            q->startZclAttributeTimer(checkZclAttributesDelay);
            updateLightEtag(lightNode2);

            queSaveDb(DB_LIGHTS, DB_LONG_SAVE_DELAY);
        }
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

                if (i->isAvailable() != available)
                {
                    if (available && node->endpoints().end() == std::find(node->endpoints().begin(),
                                                                          node->endpoints().end(),
                                                                          i->haEndpoint().endpoint()))
                    {
                        available = false;
                    }

                    ResourceItem *item = i->item(RStateReachable);
                    if (item && item->toBool() != available)
                    {
                        item->setValue(available);
                        updateLightEtag(&*i);
                        Event e(RLights, RStateReachable, i->id());
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

    ResourceItem *reachable = lightNode->item(RStateReachable);
    if (reachable->toBool())
    {
        if ((event.node()->state() == deCONZ::FailureState) || event.node()->isZombie())
        {
            reachable->setValue(false);
            Event e(RLights, RStateReachable, lightNode->id());
            enqueueEvent(e);
            updated = true;
        }
    }
    else
    {
        if (event.node()->state() != deCONZ::FailureState)
        {
            reachable->setValue(true);
            Event e(RLights, RStateReachable, lightNode->id());
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
            case DEV_ID_HA_COLOR_DIMMABLE_LIGHT:
            case DEV_ID_ZLL_COLOR_LIGHT:
            case DEV_ID_ZLL_EXTENDED_COLOR_LIGHT:
            case DEV_ID_ZLL_COLOR_TEMPERATURE_LIGHT:
            case DEV_ID_Z30_COLOR_TEMPERATURE_LIGHT:
            case DEV_ID_HA_DIMMABLE_LIGHT:
            //case DEV_ID_ZLL_DIMMABLE_LIGHT: // same as DEV_ID_HA_ONOFF_LIGHT
            case DEV_ID_ZLL_DIMMABLE_PLUGIN_UNIT:
            case DEV_ID_HA_ONOFF_LIGHT:
            case DEV_ID_ONOFF_OUTPUT:
            case DEV_ID_ZLL_ONOFF_LIGHT:
            case DEV_ID_ZLL_ONOFF_PLUGIN_UNIT:
            case DEV_ID_ZLL_ONOFF_SENSOR:
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
            case DEV_ID_ZLL_COLOR_TEMPERATURE_LIGHT:
            case DEV_ID_ZLL_DIMMABLE_LIGHT:
            case DEV_ID_ZLL_DIMMABLE_PLUGIN_UNIT:
            case DEV_ID_ZLL_ONOFF_LIGHT:
            case DEV_ID_ZLL_ONOFF_PLUGIN_UNIT:
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
                                Event e(RLights, RStateHue, lightNode->id());
                                enqueueEvent(e);
                            }

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
                            Event e(RLights, RStateSat, lightNode->id());
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
                            Event e(RLights, RStateX, lightNode->id());
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
                            Event e(RLights, RStateY, lightNode->id());
                            enqueueEvent(e);
                            updated = true;
                        }
                    }
                    else if (ia->id() == 0x0007) // color temperature
                    {
                        uint16_t ct = ia->numericValue().u16;
                        ResourceItem *item = lightNode->item(RStateCt);

                        if (item && item->toNumber() != ct)
                        {
                            item->setValue(ct);
                            Event e(RLights, RStateCt, lightNode->id());
                            enqueueEvent(e);
                            updated = true;
                        }
                    }
                    else if (ia->id() == 0x0008) // color mode
                    {
                        uint8_t cm = ia->numericValue().u8;

                        const char *modes[3] = {"hs", "xy", "ct"};
                        if (cm < 3)
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
                            DBG_Printf(DBG_INFO, "level %u --> %u\n", (uint)item->toNumber(), level);
                            lightNode->clearRead(READ_LEVEL);
                            item->setValue(level);
                            Event e(RLights, RStateBri, lightNode->id());
                            enqueueEvent(e);
                            updated = true;
                        }
                        lightNode->setZclValue(updateType, event.clusterId(), 0x0000, ia->numericValue());
                        break;
                    }
                }
                break;
            }
            else if (ic->id() == ONOFF_CLUSTER_ID && (event.clusterId() == ONOFF_CLUSTER_ID))
            {
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
                            lightNode->clearRead(READ_ON_OFF);
                            item->setValue(on);
                            Event e(RLights, RStateOn, lightNode->id());
                            enqueueEvent(e);
                            updated = true;
                        }
                        lightNode->setZclValue(updateType, event.clusterId(), 0x0000, ia->numericValue());
                        break;
                    }
                    break;
                }
            }
            else if (ic->id() == BASIC_CLUSTER_ID && (event.clusterId() == BASIC_CLUSTER_ID))
            {
                std::vector<deCONZ::ZclAttribute>::const_iterator ia = ic->attributes().begin();
                std::vector<deCONZ::ZclAttribute>::const_iterator enda = ic->attributes().end();
                for (;ia != enda; ++ia)
                {
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
                        QString str = ia->toString();
                        if (!str.isEmpty() && str != lightNode->modelId())
                        {
                            lightNode->setModelId(str);
                            lightNode->setNeedSaveDatabase(true);
                            queSaveDb(DB_LIGHTS, DB_LONG_SAVE_DELAY);
                            updated = true;
                        }
                    }
                    else if (ia->id() == 0x4000) // Software build identifier
                    {
                        QString str = ia->toString();
                        if (!str.isEmpty() && str != lightNode->swBuildId())
                        {
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

/*! Returns a LightNode for its given \p id or 0 if not found.
 */
LightNode *DeRestPluginPrivate::getLightNodeForId(const QString &id)
{
    std::vector<LightNode>::iterator i;
    std::vector<LightNode>::iterator end = nodes.end();

    for (i = nodes.begin(); i != end; ++i)
    {
        if (i->id() == id)
        {
            return &(*i);
        }
    }

    return 0;
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
 */
void DeRestPluginPrivate::checkSensorNodeReachable(Sensor *sensor)
{
    if (!sensor)
    {
        return;
    }

    bool updated = false;
    bool reachable = false;

    if (!sensor->fingerPrint().hasEndpoint())
    {
        reachable = true; // assumption for GP device
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
            reachable = true;
        }
    }

    ResourceItem *item = sensor->item(RConfigReachable);

    if (reachable)
    {
        if (item && !item->toBool())
        {
            // the node existed before
            // refresh all with new values
            DBG_Printf(DBG_INFO, "SensorNode id: %s (%s) available\n", qPrintable(sensor->id()), qPrintable(sensor->name()));
            if (sensor->node() && !sensor->node()->isEndDevice())
            {
                sensor->setNextReadTime(READ_BINDING_TABLE, queryTime);
                sensor->enableRead(READ_BINDING_TABLE/* | READ_MODEL_ID | READ_SWBUILD_ID | READ_VENDOR_NAME*/);
                queryTime = queryTime.addSecs(5);
            }
            //sensor->setLastRead(READ_BINDING_TABLE, idleTotalCounter);
            checkSensorBindingsForAttributeReporting(sensor);
            updated = true;
        }

        if (sensor->deletedState() == Sensor::StateDeleted && findSensorsState == FindSensorsActive)
        {
            DBG_Printf(DBG_INFO, "Rediscovered deleted SensorNode %s set node %s\n", qPrintable(sensor->id()), qPrintable(sensor->address().toStringExt()));
            sensor->setDeletedState(Sensor::StateNormal);
            sensor->enableRead(READ_MODEL_ID | READ_VENDOR_NAME);

            if (sensor->node() && !sensor->node()->isEndDevice())
            {
                sensor->setNextReadTime(READ_BINDING_TABLE, queryTime);
                sensor->enableRead(READ_BINDING_TABLE);
            }
            queryTime = queryTime.addSecs(5);
            //sensor->setLastRead(READ_BINDING_TABLE, idleTotalCounter);
            updated = true;
            Event e(RSensors, REventAdded, sensor->id());
            enqueueEvent(e);
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

    if (item && item->toBool() != reachable)
    {
        item->setValue(reachable);
        Event e(RSensors, RConfigReachable, sensor->id());
        enqueueEvent(e);
    }

    if (updated)
    {
        updateSensorEtag(sensor);
        sensor->setNeedSaveDatabase(true);
        queSaveDb(DB_SENSORS, DB_SHORT_SAVE_DELAY);
    }
}

void DeRestPluginPrivate::checkSensorButtonEvent(Sensor *sensor, const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame)
{
    DBG_Assert(sensor != 0);

    if (!sensor)
    {
        return;
    }

    const Sensor::ButtonMap *buttonMap = sensor->buttonMap();
    if (!buttonMap)
    {
        quint8 pl0 = zclFrame.payload().isEmpty() ? 0 : zclFrame.payload().at(0);
        DBG_Printf(DBG_INFO, "no button map for: %s cl: 0x%04X cmd: 0x%02X pl[0]: 0%02X\n",
                   qPrintable(sensor->modelId()), ind.clusterId(), zclFrame.commandId(), pl0);
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

        if (mode != sensor->mode())
        {
            sensor->setMode(mode);
            updateSensorEtag(sensor);
            sensor->setNeedSaveDatabase(true);
            queSaveDb(DB_SENSORS, DB_SHORT_SAVE_DELAY);

            // set changed mode for sensor endpoints 1 and 2
            Sensor *other = getSensorNodeForAddressAndEndpoint(sensor->address(), (ind.srcEndpoint() == 2) ? 1 : 2);
            if (other)
            {
                other->setMode(mode);
                other->setNeedSaveDatabase(true);
                updateSensorEtag(other);
            }
        }
    }
    else if (ind.dstAddressMode() == deCONZ::ApsGroupAddress)
    {
        if (sensor->mode() != Sensor::ModeScenes) // for now all other switches only support scene mode
        {
            sensor->setMode(Sensor::ModeScenes);
            updateSensorEtag(sensor);
        }
    }

    if (ind.dstAddressMode() == deCONZ::ApsGroupAddress)
    {
        ResourceItem *item = sensor->addItem(DataTypeString, RConfigGroup);
        QString gid = QString::number(ind.dstAddress().group());

        if (item && item->toString() != gid)
        {
            item->setValue(gid);
            sensor->setNeedSaveDatabase(true);
            updateSensorEtag(sensor);
        }

        Event e(RSensors, REventValidGroup, sensor->id());
        enqueueEvent(e);
    }

    while (buttonMap->mode != Sensor::ModeNone)
    {
        if (buttonMap->mode == sensor->mode() &&
            buttonMap->endpoint == ind.srcEndpoint() &&
            buttonMap->clusterId == ind.clusterId() &&
            buttonMap->zclCommandId == zclFrame.commandId())
        {
            bool ok = true;

            if (ind.clusterId() == SCENE_CLUSTER_ID && zclFrame.commandId() == 0x05) // recall scene
            {
                ok = false; // payload: groupId, sceneId
                if (zclFrame.payload().size() >= 3 && buttonMap->zclParam0 == zclFrame.payload().at(2))
                {
                    ok = true;
                }
            }
            else if (ind.clusterId() == SCENE_CLUSTER_ID && zclFrame.commandId() == 0x07) // IKEA non-standard scene
            {
                ok = false;
                if (zclFrame.payload().size() >= 1 && buttonMap->zclParam0 == zclFrame.payload().at(0)) // next, prev scene
                {
                    ok = true;
                }
            }
            else if (ind.clusterId() == VENDOR_CLUSTER_ID && zclFrame.manufacturerCode() == VENDOR_PHILIPS && zclFrame.commandId() == 0x00) // Philips dimmer switch non-standard
            {
                ok = false;
                if (zclFrame.payload().size() >= 8)
                {
                    deCONZ::NumericUnion val;
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
                      zclFrame.commandId() == 0x05 ||  // move (with on/off)
                      zclFrame.commandId() == 0x06))  // step (with on/off)
            {
                ok = false;
                if (zclFrame.payload().size() >= 1 && buttonMap->zclParam0 == zclFrame.payload().at(0)) // direction
                {
                    ok = true;
                }
            }

            if (ok)
            {
                DBG_Printf(DBG_INFO, "button %u %s\n", buttonMap->button, buttonMap->name);
                ResourceItem *item = sensor->item(RStateButtonEvent);
                if (item)
                {
                    item->setValue(buttonMap->button);

                    Event e(RSensors, RStateButtonEvent, sensor->id());
                    enqueueEvent(e);
                    updateSensorEtag(sensor);
                    sensor->updateStateTimestamp();
                }

                item = sensor->item(RStatePresence);
                if (item)
                {
                    item->setValue(true);
                    Event e(RSensors, RStatePresence, sensor->id());
                    enqueueEvent(e);
                    updateSensorEtag(sensor);
                    sensor->updateStateTimestamp();
                }
                return;
            }
        }
        buttonMap++;
    }

    // check if hue dimmer switch is configured
    if (sensor->modelId().startsWith(QLatin1String("RWL02")))
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

    quint8 pl0 = zclFrame.payload().isEmpty() ? 0 : zclFrame.payload().at(0);
    DBG_Printf(DBG_INFO, "no button handler for: %s cl: 0x%04X cmd: 0x%02X pl[0]: 0%02X\n",
                 qPrintable(sensor->modelId()), ind.clusterId(), zclFrame.commandId(), pl0);
}

/*! Adds a new sensor node to node cache.
    Only supported ZLL and HA nodes will be added.
    \param node - the base for the SensorNode
 */
void DeRestPluginPrivate::addSensorNode(const deCONZ::Node *node)
{
    DBG_Assert(node != 0);

    if (!node)
    {
        return;
    }

    Q_Q(DeRestPlugin);

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

                    if (!i->name().isEmpty())
                    { q->nodeUpdated(i->address().ext(), QLatin1String("name"), i->name()); }

                    if (!i->modelId().isEmpty())
                    { q->nodeUpdated(i->address().ext(), QLatin1String("modelid"), i->modelId()); }

                    if (!i->manufacturer().isEmpty())
                    { q->nodeUpdated(i->address().ext(), QLatin1String("vendor"), i->manufacturer()); }

                    if (!i->swVersion().isEmpty())
                    { q->nodeUpdated(i->address().ext(), QLatin1String("version"), i->swVersion()); }
                }

                // address changed?
                if (i->address().nwk() != node->address().nwk())
                {
                    i->address() = node->address();
                }
            }
        }
    }

    if (node->nodeDescriptor().isNull())
    {
        return;
    }

    if (findSensorsState != FindSensorsActive)
    {
        return;
    }

    // check for new sensors
    QString modelId;
    QList<deCONZ::SimpleDescriptor>::const_iterator i = node->simpleDescriptors().constBegin();
    QList<deCONZ::SimpleDescriptor>::const_iterator end = node->simpleDescriptors().constEnd();

    for (;i != end; ++i)
    {
        bool supportedDevice = false;
        SensorFingerprint fpSwitch;
        SensorFingerprint fpLightSensor;
        SensorFingerprint fpPresenceSensor;
        SensorFingerprint fpTemperatureSensor;

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
                {
                    if (i->deviceId() == DEV_ID_ZLL_ONOFF_SENSOR &&
                        node->nodeDescriptor().manufacturerCode() == VENDOR_IKEA)
                    {
                        fpPresenceSensor.outClusters.push_back(ci->id());
                    }
                    else
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

        {   // scan server clusters of endpoint
            QList<deCONZ::ZclCluster>::const_iterator ci = i->inClusters().constBegin();
            QList<deCONZ::ZclCluster>::const_iterator cend = i->inClusters().constEnd();
            for (; ci != cend; ++ci)
            {
                switch (ci->id())
                {
                case BASIC_CLUSTER_ID:
                {
                    std::vector<deCONZ::ZclAttribute>::const_iterator i = ci->attributes().begin();
                    std::vector<deCONZ::ZclAttribute>::const_iterator end = ci->attributes().end();

                    if (modelId.isEmpty())
                    {
                        for (; i != end; ++i)
                        {
                            if (i->id() == 0x0005) // model id
                            {
                                modelId = i->toString().trimmed();
                                break;
                            }
                        }
                    }
                    if (modelId.isEmpty())
                    {
                        Sensor *sensor = getSensorNodeForAddress(node->address()); // extract from others if possible
                        if (sensor && !sensor->modelId().isEmpty())
                        {
                            modelId = sensor->modelId();
                        }
                    }

                    supportedDevice = isDeviceSupported(node, modelId);
                    fpSwitch.inClusters.push_back(ci->id());
                }
                    break;

                case POWER_CONFIGURATION_CLUSTER_ID:
                {
                    if (node->nodeDescriptor().manufacturerCode() == VENDOR_PHILIPS ||
                        node->nodeDescriptor().manufacturerCode() == VENDOR_NYCE)
                    {
                        fpSwitch.inClusters.push_back(ci->id());
                        fpPresenceSensor.inClusters.push_back(ci->id());
                        fpLightSensor.inClusters.push_back(ci->id());
                        fpTemperatureSensor.inClusters.push_back(ci->id());
                    }
                }
                    break;

                case COMMISSIONING_CLUSTER_ID:
                {
                    fpSwitch.inClusters.push_back(ci->id());
                    fpPresenceSensor.inClusters.push_back(ci->id());
                }
                    break;

                case ONOFF_SWITCH_CONFIGURATION_CLUSTER_ID:
                {
                    fpSwitch.inClusters.push_back(ci->id());
                }
                    break;

                case IAS_ZONE_CLUSTER_ID:
                case OCCUPANCY_SENSING_CLUSTER_ID:
                {
                    fpPresenceSensor.inClusters.push_back(ci->id());
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

                default:
                    break;
                }
            }
        }

        if (!supportedDevice)
        {
            return;
        }

        Sensor *sensor = 0;

        // ZHASwitch
        if (fpSwitch.hasInCluster(ONOFF_SWITCH_CONFIGURATION_CLUSTER_ID) || !fpSwitch.outClusters.empty()) // (!fpSwitch.inClusters.empty() || !fpSwitch.outClusters.empty())
        {
            fpSwitch.endpoint = i->endpoint();
            fpSwitch.deviceId = i->deviceId();
            fpSwitch.profileId = i->profileId();

            sensor = getSensorNodeForFingerPrint(node->address().ext(), fpSwitch, "ZHASwitch");

            if (!sensor)
            {
                addSensorNode(node, fpSwitch, "ZHASwitch", modelId);
            }
            else
            {
                checkSensorNodeReachable(sensor);
            }
        }

        // ZHALight
        if (fpLightSensor.hasInCluster(ILLUMINANCE_MEASUREMENT_CLUSTER_ID))
        {
            fpLightSensor.endpoint = i->endpoint();
            fpLightSensor.deviceId = i->deviceId();
            fpLightSensor.profileId = i->profileId();

            sensor = getSensorNodeForFingerPrint(node->address().ext(), fpLightSensor, "ZHALight");
            if (!sensor)
            {
                addSensorNode(node, fpLightSensor, "ZHALight", modelId);
            }
            else
            {
                checkSensorNodeReachable(sensor);
            }
        }

        // ZHAPresence
        if (fpPresenceSensor.hasInCluster(OCCUPANCY_SENSING_CLUSTER_ID) ||
            fpPresenceSensor.hasInCluster(IAS_ZONE_CLUSTER_ID) ||
            fpPresenceSensor.hasOutCluster(ONOFF_CLUSTER_ID))
        {
            fpPresenceSensor.endpoint = i->endpoint();
            fpPresenceSensor.deviceId = i->deviceId();
            fpPresenceSensor.profileId = i->profileId();

            sensor = getSensorNodeForFingerPrint(node->address().ext(), fpPresenceSensor, "ZHAPresence");
            if (!sensor)
            {
                addSensorNode(node, fpPresenceSensor, "ZHAPresence", modelId);
            }
            else if (!node->isEndDevice())
            {
                //sensor->setLastRead(idleTotalCounter);
                sensor->enableRead(READ_OCCUPANCY_CONFIG);
                sensor->setNextReadTime(READ_OCCUPANCY_CONFIG, queryTime);
                queryTime = queryTime.addSecs(5);
                checkSensorNodeReachable(sensor);
                Q_Q(DeRestPlugin);
                q->startZclAttributeTimer(checkZclAttributesDelay);
            }
        }

        // ZBTemperature
        if (fpTemperatureSensor.hasInCluster(TEMPERATURE_MEASUREMENT_CLUSTER_ID))
        {
            fpTemperatureSensor.endpoint = i->endpoint();
            fpTemperatureSensor.deviceId = i->deviceId();
            fpTemperatureSensor.profileId = i->profileId();

            sensor = getSensorNodeForFingerPrint(node->address().ext(), fpTemperatureSensor, "ZHATemperature");
            if (!sensor)
            {
                addSensorNode(node, fpTemperatureSensor, "ZHATemperature", modelId);
            }
            else
            {
                checkSensorNodeReachable(sensor);
            }
        }
    }
}

void DeRestPluginPrivate::addSensorNode(const deCONZ::Node *node, const SensorFingerprint &fingerPrint, const QString &type, const QString &modelId)
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
        sensor2 = getSensorNodeForAddressAndEndpoint(node->address(), fingerPrint.endpoint);

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

    if (node->isEndDevice())
    {
        sensorNode.addItem(DataTypeUInt8, RConfigBattery);
    }

    if (sensorNode.type().endsWith(QLatin1String("Switch")))
    {
        if (sensorNode.fingerPrint().hasInCluster(COMMISSIONING_CLUSTER_ID))
        {
            clusterId = COMMISSIONING_CLUSTER_ID;
        }
        else if (sensorNode.fingerPrint().hasOutCluster(ONOFF_CLUSTER_ID))
        {
            clusterId = ONOFF_CLUSTER_ID;
        }
        sensorNode.addItem(DataTypeInt32, RStateButtonEvent);
    }
    else if (sensorNode.type().endsWith(QLatin1String("Light")))
    {
        if (sensorNode.fingerPrint().hasInCluster(ILLUMINANCE_MEASUREMENT_CLUSTER_ID))
        {
            clusterId = ILLUMINANCE_MEASUREMENT_CLUSTER_ID;
        }
        sensorNode.addItem(DataTypeUInt16, RStateLightLevel);
    }
    else if (sensorNode.type().endsWith(QLatin1String("Temperature")))
    {
        if (sensorNode.fingerPrint().hasInCluster(TEMPERATURE_MEASUREMENT_CLUSTER_ID))
        {
            clusterId = TEMPERATURE_MEASUREMENT_CLUSTER_ID;
        }
        sensorNode.addItem(DataTypeInt32, RStateTemperature);
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
        else if (sensorNode.fingerPrint().hasOutCluster(ONOFF_CLUSTER_ID))
        {
            clusterId = ONOFF_CLUSTER_ID;
        }
        sensorNode.addItem(DataTypeBool, RStatePresence);
    }

    QString uid = generateUniqueId(sensorNode.address().ext(), sensorNode.fingerPrint().endpoint, clusterId);
    sensorNode.setUniqueId(uid);

    if (node->nodeDescriptor().manufacturerCode() == VENDOR_DDEL)
    {
        sensorNode.setManufacturer("dresden elektronik");

        if (modelId == QLatin1String("Lighting Switch"))
        {
            sensorNode.setMode(Sensor::ModeTwoGroups); // inital
        }
    }
    else if ((node->nodeDescriptor().manufacturerCode() == VENDOR_OSRAM_STACK) || (node->nodeDescriptor().manufacturerCode() == VENDOR_OSRAM))
    {
        sensorNode.setManufacturer("OSRAM");
    }
    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_UBISYS)
    {
        sensorNode.setManufacturer("ubisys");
    }
    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_BUSCH_JAEGER)
    {
        sensorNode.setManufacturer("Busch-Jaeger");
    }
    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_PHILIPS)
    {
        sensorNode.setManufacturer("Philips");

        if (modelId.startsWith(QLatin1String("RWL02")))
        {
            if (!sensorNode.fingerPrint().hasInCluster(POWER_CONFIGURATION_CLUSTER_ID))
            {   // this cluster is on endpoint 2 and hence not detected
                sensorNode.fingerPrint().inClusters.push_back(POWER_CONFIGURATION_CLUSTER_ID);
            }

            if (!sensorNode.fingerPrint().hasInCluster(VENDOR_CLUSTER_ID)) // for realtime button feedback
            {   // this cluster is on endpoint 2 and hence not detected
                sensorNode.fingerPrint().inClusters.push_back(VENDOR_CLUSTER_ID);
            }
        }
        else if (modelId == QLatin1String("SML001"))
        {
            if (type == QLatin1String("ZHASwitch"))
            {
                // not supported yet
                return;
            }
        }
    }
    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_BEGA)
    {
        sensorNode.setManufacturer("BEGA Gantenbrink-Leuchten KG");
    }
    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_IKEA)
    {
        sensorNode.setManufacturer("IKEA of Sweden");
    }
    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_INSTA)
    {
        sensorNode.setManufacturer("Insta");
        checkInstaModelId(&sensorNode);
    }

    if (!sensor2 && sensorNode.id().isEmpty())
    {
        openDb();
        sensorNode.setId(QString::number(getFreeSensorId()));
        sensorNode.setNeedSaveDatabase(true);
        closeDb();
    }

    if (sensorNode.name().isEmpty())
    {
        if (type.endsWith(QLatin1String("Switch")))
        {
            sensorNode.setName(QString("Switch %1").arg(sensorNode.id()));
        }
        else
        {
            sensorNode.setName(QString("%1 %2").arg(type).arg(sensorNode.id()));
        }
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
        updateSensorEtag(&sensors.back());
    }

    if (findSensorsState == FindSensorsActive)
    {
        Event e(RSensors, REventAdded, sensorNode.id());
        enqueueEvent(e);
    }


    Q_Q(DeRestPlugin);
    q->startZclAttributeTimer(checkZclAttributesDelay);

    queSaveDb(DB_SENSORS , DB_SHORT_SAVE_DELAY);
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

    bool updated = false;

    std::vector<Sensor>::iterator i = sensors.begin();
    std::vector<Sensor>::iterator end = sensors.end();

    for (; i != end; ++i)
    {
        if (i->address().ext() != event.node()->address().ext())
        {
            continue;
        }

        if (i->node() != event.node())
        {
            i->setNode(const_cast<deCONZ::Node*>(event.node()));
            DBG_Printf(DBG_INFO, "Sensor %s set node %s\n", qPrintable(i->id()), qPrintable(event.node()->address().toStringExt()));
        }

        checkSensorNodeReachable(&(*i));

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
                    Event e(RSensors, RConfigBattery, i->id());
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
            case OCCUPANCY_SENSING_CLUSTER_ID:
            case POWER_CONFIGURATION_CLUSTER_ID:
            case BASIC_CLUSTER_ID:
                break;

            default:
                continue; // don't process further
            }
        }
        else
        {
            continue;
        }


        if (event.clusterId() != BASIC_CLUSTER_ID && event.clusterId() != POWER_CONFIGURATION_CLUSTER_ID)
        {
            // filter endpoint
            if (event.endpoint() != i->fingerPrint().endpoint)
            {
                continue;
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
                            if (ia->id() == 0x0021) // battery percentage remaining
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.clusterId(), ia->id(), ia->numericValue());
                                }

                                ResourceItem *item = i->item(RConfigBattery);

                                // Specifies the remaining battery life as a half integer percentage of the full battery capacity (e.g., 34.5%, 45%,
                                // 68.5%, 90%) with a range between zero and 100%, with 0x00 = 0%, 0x64 = 50%, and 0xC8 = 100%. This is
                                // particularly suited for devices with rechargeable batteries.
                                if (item)
                                {
                                    int bat = ia->numericValue().u8 / 2;

                                    if (item->toNumber() != bat)
                                    {
                                        item->setValue(bat);
                                        i->setNeedSaveDatabase(true);
                                        queSaveDb(DB_SENSORS, DB_LONG_SAVE_DELAY);
                                    }
                                    Event e(RSensors, RConfigBattery, i->id());
                                    enqueueEvent(e);
                                }

                                updateSensorEtag(&*i);
                            }
                        }
                    }
                    else if (event.clusterId() == ILLUMINANCE_MEASUREMENT_CLUSTER_ID)
                    {
                        for (;ia != enda; ++ia)
                        {
                            if (ia->id() == 0x0000) // measured illuminance (lux)
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.clusterId(), 0x0000, ia->numericValue());
                                }

                                ResourceItem *item = i->item(RStateLightLevel);

                                quint16 measuredValue = ia->numericValue().u16; // ZigBee uses a 16-bit value

                                if (item)
                                {
                                    item->setValue(measuredValue);
                                    i->updateStateTimestamp();
                                    Event e(RSensors, RStateLightLevel, i->id());
                                    enqueueEvent(e);
                                }

                                item = i->item(RStateLux);

                                if (item)
                                {
                                    quint32 lux = 0;
                                    if (measuredValue > 0 && measuredValue < 0xffff)
                                    {
                                        lux = measuredValue;
                                        // valid values are 1 - 0xfffe
                                        // 0, too low to measure
                                        // 0xffff invalid value

                                        // ZCL Attribute = 10.000 * log10(Illuminance (lx)) + 1
                                        // lux = 10^(ZCL Attribute/10.000) - 1
                                        qreal exp = lux;
                                        qreal l = qPow(10, exp / 10000.0f);

                                        if (l >= 1)
                                        {
                                            l -= 1;
                                            lux = static_cast<quint32>(l);
                                        }
                                        else
                                        {
                                            DBG_Printf(DBG_INFO, "invalid lux value %u", lux);
                                            lux = 0; // invalid value
                                        }
                                    }
                                    else
                                    {
                                        lux = 0;
                                    }
                                    item->setValue(lux);
                                }

                                updateSensorEtag(&*i);
                            }
                        }
                    }
                    else if (event.clusterId() == TEMPERATURE_MEASUREMENT_CLUSTER_ID)
                    {
                        for (;ia != enda; ++ia)
                        {
                            if (ia->id() == 0x0000) // measured illuminance (lux)
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.clusterId(), 0x0000, ia->numericValue());
                                }

                                int temp = ia->numericValue().s16;
                                ResourceItem *item = i->item(RStateTemperature);

                                if (item)
                                {
                                    item->setValue(temp);
                                    i->updateStateTimestamp();
                                    Event e(RSensors, RStateTemperature, i->id());
                                    enqueueEvent(e);
                                }

                                updateSensorEtag(&*i);
                            }
                        }
                    }
                    else if (event.clusterId() == OCCUPANCY_SENSING_CLUSTER_ID)
                    {
                        for (;ia != enda; ++ia)
                        {
                            if (ia->id() == 0x0000) // occupied state
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.clusterId(), 0x0000, ia->numericValue());
                                }

                                ResourceItem *item = i->item(RStatePresence);

                                if (item)
                                {
                                    item->setValue(ia->numericValue().u8);
                                    i->updateStateTimestamp();
                                    Event e(RSensors, RStatePresence, i->id());
                                    enqueueEvent(e);
                                }
                                updateSensorEtag(&*i);
                            }
                            else if (ia->id() == 0x0010) // occupied to unoccupied delay
                            {
                                quint16 duration = ia->numericValue().u16;
                                ResourceItem *item = i->item(RConfigDuration);

                                if (item && item->toNumber() != duration)
                                {
                                    Event e(RSensors, RConfigDuration, i->id());
                                    enqueueEvent(e);

                                    if (item->toNumber() <= 0)
                                    {
                                        DBG_Printf(DBG_INFO, "got occupied to unoccupied delay %u\n", ia->numericValue().u16);
                                        item->setValue(duration);
                                        updateSensorEtag(&*i);
                                        updated = true;
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
                        }
                    }
                    else if (event.clusterId() == BASIC_CLUSTER_ID)
                    {
                        DBG_Printf(DBG_INFO, "Update Sensor 0x%016llX Basic Cluster\n", event.node()->address().ext());
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
                                        queSaveDb(DB_SENSORS, DB_LONG_SAVE_DELAY);
                                        updated = true;
                                    }

                                    if (i->name() == QString("Switch %1").arg(i->id()))
                                    {
                                        QString name = QString("%1 %2").arg(str).arg(i->id());
                                        if (i->name() != name)
                                        {
                                            i->setName(name);
                                            i->setNeedSaveDatabase(true);
                                            updateSensorEtag(&*i);
                                            updated = true;
                                        }
                                    }
                                }
                            }
                            if (ia->id() == 0x0004) // Manufacturer Name
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
                                        queSaveDb(DB_SENSORS, DB_LONG_SAVE_DELAY);
                                        updated = true;
                                    }
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
                                        queSaveDb(DB_SENSORS, DB_LONG_SAVE_DELAY);
                                        updateSensorEtag(&*i);
                                        updated = true;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (updated)
    {
        queSaveDb(DB_SENSORS , DB_SHORT_SAVE_DELAY);
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
        if (node->nodeDescriptor().manufacturerCode() == s->vendorId)
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
            if (i->address().nwk() == addr.nwk() && ep == i->fingerPrint().endpoint)
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

/*! Change the status of a rule that controls a group with the \p groupId to enabled if \p enabled == true else to disabled.
 */
void DeRestPluginPrivate::changeRuleStatusofGroup(QString groupId, bool enabled)
{
    std::vector<Rule>::iterator ri = rules.begin();
    std::vector<Rule>::iterator rend = rules.end();
    for (; ri != rend; ++ri)
    {
        // find sensor id of that rule
        QString sensorId = "";
        std::vector<RuleCondition>::const_iterator c = ri->conditions().begin();
        std::vector<RuleCondition>::const_iterator cend = ri->conditions().end();
        for (; c != cend; ++c)
        {
            if (c->address().indexOf("sensors/") != -1 && c->address().indexOf("/state") != -1)
            {
                int begin = c->address().indexOf("sensors/")+8;
                int end = c->address().indexOf("/state");
                // assumption: all conditions of that rule use the same sensor
                sensorId = c->address().mid(begin, end-begin);
                break;
            }
        }

        // detect sensor of that rule
        QString sensorModelId = "";
        QString sensorType = "";
        std::vector<Sensor>::iterator si = sensors.begin();
        std::vector<Sensor>::iterator send = sensors.end();
        for (; si != send; ++si)
        {
            if (si->id() == sensorId)
            {
                sensorType = si->type();
                sensorModelId = si->modelId();
                break;
            }
        }

        // disable or enable rule depending of group action
        if (sensorType == "ZHALight" && !sensorModelId.startsWith("FLS-NB") && sensorModelId != "")
        {
            std::vector<RuleAction>::const_iterator a = ri->actions().begin();
            std::vector<RuleAction>::const_iterator aend = ri->actions().end();
            for (; a != aend; ++a)
            {
                if (a->address().indexOf("groups/" + groupId + "/action") != -1)
                {
                    if (enabled && ri->status() == "disabled")
                    {
                        ri->setStatus("enabled");
                        queSaveDb(DB_RULES, DB_SHORT_SAVE_DELAY);
                        break;
                    }
                    else if (!enabled && ri->status() == "enabled")
                    {
                        ri->setStatus("disabled");
                        queSaveDb(DB_RULES, DB_SHORT_SAVE_DELAY);
                        break;
                    }
                }
            }
        }
    }
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

    if (!lightNode->isAvailable())
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
    bool readColor = false;
    bool readLevel = false;
    bool readOnOff = false;

    if (lightNode->haEndpoint().profileId() == ZLL_PROFILE_ID)
    {
        switch(lightNode->haEndpoint().deviceId())
        {
        case DEV_ID_ZLL_COLOR_LIGHT:
        case DEV_ID_ZLL_EXTENDED_COLOR_LIGHT:
        case DEV_ID_ZLL_COLOR_TEMPERATURE_LIGHT:
        case DEV_ID_Z30_COLOR_TEMPERATURE_LIGHT:
            readColor = true;
            //fall through

        case DEV_ID_ZLL_DIMMABLE_LIGHT:
        case DEV_ID_ZLL_DIMMABLE_PLUGIN_UNIT:
            readLevel = true;
            //fall through

        case DEV_ID_ZLL_ONOFF_LIGHT:
        case DEV_ID_ZLL_ONOFF_PLUGIN_UNIT:
        case DEV_ID_ZLL_ONOFF_SENSOR:
            readOnOff = true;
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
        case DEV_ID_ZLL_COLOR_TEMPERATURE_LIGHT:
            readColor = true;
            //fall through

        case DEV_ID_HA_DIMMABLE_LIGHT:
        //case DEV_ID_ZLL_DIMMABLE_LIGHT: // same as DEV_ID_HA_ONOFF_LIGHT
        case DEV_ID_ZLL_DIMMABLE_PLUGIN_UNIT:
            readLevel = true;
            //fall through

        case DEV_ID_MAINS_POWER_OUTLET:
        case DEV_ID_HA_ONOFF_LIGHT:
        case DEV_ID_ZLL_ONOFF_LIGHT:
        case DEV_ID_ZLL_ONOFF_PLUGIN_UNIT:
        case DEV_ID_ZLL_ONOFF_SENSOR:
            readOnOff = true;
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
        if (!lightNode->manufacturer().isEmpty())
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

    if (lightNode->manufacturerCode() == VENDOR_UBISYS)
    {
        lightNode->clearRead(READ_SWBUILD_ID); // Ubisys devices have empty sw build id
    }
    else if ((processed < 2) && lightNode->mustRead(READ_SWBUILD_ID) && tNow > lightNode->nextReadTime(READ_SWBUILD_ID))
    {
        std::vector<uint16_t> attributes;
        attributes.push_back(0x4000); // Software build identifier

        if (readAttributes(lightNode, lightNode->haEndpoint().endpoint(), BASIC_CLUSTER_ID, attributes))
        {
            lightNode->clearRead(READ_SWBUILD_ID);
            processed++;
        }
    }

    if ((processed < 2) && readOnOff && lightNode->mustRead(READ_ON_OFF) && tNow > lightNode->nextReadTime(READ_ON_OFF))
    {
        std::vector<uint16_t> attributes;
        attributes.push_back(0x0000); // OnOff

        if (readAttributes(lightNode, lightNode->haEndpoint().endpoint(), ONOFF_CLUSTER_ID, attributes))
        {
            lightNode->clearRead(READ_ON_OFF);
            processed++;
        }
    }

    if ((processed < 2) && readLevel && lightNode->mustRead(READ_LEVEL) && tNow > lightNode->nextReadTime(READ_LEVEL))
    {
        std::vector<uint16_t> attributes;
        attributes.push_back(0x0000); // Level

        if (readAttributes(lightNode, lightNode->haEndpoint().endpoint(), LEVEL_CLUSTER_ID, attributes))
        {
            lightNode->clearRead(READ_LEVEL);
            processed++;
        }
    }

    if ((processed < 2) && readColor && lightNode->mustRead(READ_COLOR) && lightNode->hasColor() && tNow > lightNode->nextReadTime(READ_COLOR))
    {
        std::vector<uint16_t> attributes;
        attributes.push_back(0x0000); // Current hue
        attributes.push_back(0x0001); // Current saturation
        attributes.push_back(0x0003); // Current x
        attributes.push_back(0x0004); // Current y
        attributes.push_back(0x0007); // Color temperature
        attributes.push_back(0x0008); // Color mode
        attributes.push_back(0x4000); // Enhanced hue
        attributes.push_back(0x4002); // Color loop active

        if (readAttributes(lightNode, lightNode->haEndpoint().endpoint(), COLOR_CLUSTER_ID, attributes))
        {
            lightNode->clearRead(READ_COLOR);
            processed++;
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
            sensorNode->modelId().startsWith(QLatin1String("C4")) ||
            sensorNode->modelId().startsWith(QLatin1String("LM_00.00")))
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
        std::vector<uint16_t> attributes;
        attributes.push_back(0x0010); // occupied to unoccupied delay

        if (readAttributes(sensorNode, sensorNode->fingerPrint().endpoint, OCCUPANCY_SENSING_CLUSTER_ID, attributes))
        {
            sensorNode->clearRead(READ_OCCUPANCY_CONFIG);
            processed++;
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

    return (processed > 0);
}

/*! Queue reading ZCL attributes of a node.
    \param restNode the node from which the attributes shall be read
    \param endpoint the destination endpoint
    \param clusterId the cluster id related to the attributes
    \param attributes a list of attribute ids which shall be read
    \return true if the request is queued
 */
bool DeRestPluginPrivate::readAttributes(RestNodeBase *restNode, quint8 endpoint, uint16_t clusterId, const std::vector<uint16_t> &attributes)
{
    DBG_Assert(restNode != 0);
    DBG_Assert(!attributes.empty());

    if (!restNode || attributes.empty() || !restNode->isAvailable())
    {
        return false;
    }

    if (taskCountForAddress(restNode->address()) > 0)
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
    task.zclFrame.setFrameControl(deCONZ::ZclFCProfileCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCDisableDefaultResponse);

    DBG_Printf(DBG_INFO_L2, "read attributes of 0x%016llX cluster: 0x%04X: [ ", restNode->address().ext(), clusterId);

    { // payload
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        for (uint i = 0; i < attributes.size(); i++)
        {
            stream << attributes[i];
            if (DBG_IsEnabled(DBG_INFO_L2))
            {
                DBG_Printf(DBG_INFO_L2, "0x%04X ", attributes[i]);
            }
        }
    }

    if (DBG_IsEnabled(DBG_INFO_L2))
    {
        DBG_Printf(DBG_INFO_L2, "]\n");
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
    \return true if the request is queued
 */
bool DeRestPluginPrivate::writeAttribute(RestNodeBase *restNode, quint8 endpoint, uint16_t clusterId, const deCONZ::ZclAttribute &attribute)
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
    task.zclFrame.setFrameControl(deCONZ::ZclFCProfileCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCDisableDefaultResponse);

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

//    task.req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
    task.req.setDstEndpoint(lightNode->haEndpoint().endpoint());
    task.req.setDstAddressMode(deCONZ::ApsExtAddress);
    task.req.dstAddress() = lightNode->address();
    task.req.setClusterId(SCENE_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);
    task.req.setSrcEndpoint(getSrcEndpoint(lightNode, task.req));

    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(0x01); // view scene
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

    if (!lightNode || !lightNode->isAvailable())
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
bool DeRestPluginPrivate::isLightNodeInGroup(LightNode *lightNode, uint16_t groupId)
{
    DBG_Assert(lightNode != 0);

    if (lightNode)
    {
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
                Event e(RLights, RStateOn, lightNode->id());
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
    updateEtag(group->etag);
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

            if (lightNode->manufacturerCode() == VENDOR_OSRAM ||
                lightNode->manufacturerCode() == VENDOR_OSRAM_STACK)
            {
                // quirks mode: need extra store scene command (color temperature issue)
                std::vector<uint8_t> &v = groupInfo->addScenes;

                if (std::find(v.begin(), v.end(), sceneId) == v.end())
                {
                    groupInfo->addScenes.push_back(sceneId);
                }
            }
        }
    }

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
        fixSceneTableReadResponse(lightNode, ind, zclFrame);
    }

    if (zclFrame.isDefaultResponse())
    {
        DBG_Printf(DBG_INFO, "DE cluster default response cmd 0x%02X, status 0x%02X\n", zclFrame.defaultResponseCommandId(), zclFrame.defaultResponseStatus());
    }
}

/*! Handle incoming ZCL attribute report commands.
 */
void DeRestPluginPrivate::handleZclAttributeReportIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    Q_UNUSED(zclFrame);

    if (!(zclFrame.frameControl() & deCONZ::ZclFCDisableDefaultResponse))
    {
        sendZclDefaultResponse(ind, zclFrame, deCONZ::ZclSuccessStatus);
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

/*! Attemp to hotfix a broken scene table initialisation (temp, will be removed soon).
 */
void DeRestPluginPrivate::fixSceneTableRead(LightNode *lightNode, quint16 offset)
{
    if (!lightNode)
    {
        return;
    }

    if (lightNode->modelId().startsWith(QLatin1String("FLS-NB")))
    {
        if (lightNode->swBuildId().endsWith(QLatin1String("200000D2")) ||
            lightNode->swBuildId().endsWith(QLatin1String("200000D3")))
        {

            if (offset >= (0x4b16 + (16 * 9)))
            {
                return; // done
            }

            deCONZ::ApsDataRequest req;

            req.setClusterId(DE_CLUSTER_ID);
            req.setProfileId(HA_PROFILE_ID);
            req.dstAddress() = lightNode->address();
            req.setDstAddressMode(deCONZ::ApsExtAddress);
            req.setDstEndpoint(0x0A);
            req.setSrcEndpoint(endpoint());

            deCONZ::ZclFrame zclFrame;

            zclFrame.setSequenceNumber(zclSeq++);
            zclFrame.setCommandId(0x03);
            zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                                     deCONZ::ZclFCDirectionClientToServer |
                                     deCONZ::ZclFCDisableDefaultResponse);

            { // payload
                QDataStream stream(&zclFrame.payload(), QIODevice::WriteOnly);
                stream.setByteOrder(QDataStream::LittleEndian);

                if (offset == 0) // first request
                {
                    offset = 0x4B16 + 9;
                }

                quint8 length = 4;
                stream << offset;
                stream << length;
            }

            { // ZCL frame
                req.asdu().clear(); // cleanup old request data if there is any
                QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
                stream.setByteOrder(QDataStream::LittleEndian);
                zclFrame.writeToStream(stream);
            }

            deCONZ::ApsController *apsCtrl = deCONZ::ApsController::instance();

            if (apsCtrl && apsCtrl->apsdeDataRequest(req) == deCONZ::Success)
            {
                queryTime = queryTime.addSecs(2);
            }
        }
    }
}

/*! Attemp to hotfix a broken scene table initialisation (temp, will be removed soon).
 */
void DeRestPluginPrivate::fixSceneTableReadResponse(LightNode *lightNode, const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    Q_UNUSED(ind);

    if (!lightNode)
    {
        return;
    }

    if (lightNode->modelId().startsWith(QLatin1String("FLS-NB")))
    {
        if (lightNode->swBuildId().endsWith(QLatin1String("200000D2")) ||
            lightNode->swBuildId().endsWith(QLatin1String("200000D3")))
        {

            quint8 status;
            quint16 offset;
            quint8 length;

            QDataStream stream(zclFrame.payload());
            stream.setByteOrder(QDataStream::LittleEndian);

            stream >> status;
            stream >> offset;
            stream >> length;

            if (status != deCONZ::ZclSuccessStatus || length != 4)
            {
                return; // unexpected
            }

            quint8 isFree;
            quint8 sceneId;
            quint16 groupId;

            stream >> isFree;
            stream >> sceneId;
            stream >> groupId;

            DBG_Printf(DBG_INFO, "Fix scene table offset 0x%04X free %u, scene 0x%02X, group 0x%04X\n", offset, isFree, sceneId, groupId);

            if (isFree)
            {
                // stop here
            }
            else if (isFree == 0 && groupId == 0 && sceneId == 0)
            {
                // entry must be fixed
                fixSceneTableWrite(lightNode, offset);
            }
            else if (sceneId > 0 || groupId > 0)
            {
                fixSceneTableRead(lightNode, offset + 9); // process next entry
            }
        }
    }
}

/*! Attemp to hotfix a broken scene table initialisation (temp, will be removed soon).
 */
void DeRestPluginPrivate::fixSceneTableWrite(LightNode *lightNode, quint16 offset)
{
    if (!lightNode)
    {
        return;
    }

    if (lightNode->modelId().startsWith(QLatin1String("FLS-NB")))
    {
        if (lightNode->swBuildId().endsWith(QLatin1String("200000D2")) ||
            lightNode->swBuildId().endsWith(QLatin1String("200000D3")))
        {

            if (offset >= (0x4b16 + (16 * 9)))
            {
                return; // done
            }

            DBG_Printf(DBG_INFO, "Write fix to scene table offset 0x%04X\n", offset);

            deCONZ::ApsDataRequest req;

            req.setClusterId(DE_CLUSTER_ID);
            req.setProfileId(HA_PROFILE_ID);
            req.dstAddress() = lightNode->address();
            req.setDstAddressMode(deCONZ::ApsExtAddress);
            req.setDstEndpoint(0x0A);
            req.setSrcEndpoint(endpoint());

            deCONZ::ZclFrame zclFrame;

            zclFrame.setSequenceNumber(zclSeq++);
            zclFrame.setCommandId(0x04);
            zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                                     deCONZ::ZclFCDirectionClientToServer/* |
                                     deCONZ::ZclFCDisableDefaultResponse*/);

            { // payload
                QDataStream stream(&zclFrame.payload(), QIODevice::WriteOnly);
                stream.setByteOrder(QDataStream::LittleEndian);

                quint8 length = 1;
                quint8 isFree = 0x01;
                quint8 dummy = 0; // at least 4 bytes must be in the request
                stream << offset;
                stream << length;
                stream << isFree;
                stream << dummy;
                stream << dummy;
                stream << dummy;
            }

            { // ZCL frame
                req.asdu().clear(); // cleanup old request data if there is any
                QDataStream stream(&req.asdu(), QIODevice::WriteOnly);
                stream.setByteOrder(QDataStream::LittleEndian);
                zclFrame.writeToStream(stream);
            }

            deCONZ::ApsController *apsCtrl = deCONZ::ApsController::instance();

            req.setSendDelay(1000);
            if (apsCtrl && apsCtrl->apsdeDataRequest(req) == deCONZ::Success)
            {
                queryTime = queryTime.addSecs(2);
            }
        }
    }
}

/*! Queues a client for closing the connection.
    \param sock the client socket
    \param closeTimeout timeout in seconds then the socket should be closed
 */
void DeRestPluginPrivate::pushClientForClose(QTcpSocket *sock, int closeTimeout)
{
    std::list<TcpClient>::iterator i = openClients.begin();
    std::list<TcpClient>::iterator end = openClients.begin();

    for ( ;i != end; ++i)
    {
        if (i->sock == sock)
        {
            i->closeTimeout = closeTimeout;
            return;
        }
        // Other QtcpSocket but same peer
        else if (i->sock->peerPort() == sock->peerPort())
        {
            if (i->sock->peerAddress() == sock->peerAddress())
            {
                i->sock->deleteLater();

                i->sock = sock;
                i->closeTimeout = closeTimeout;
                return;
            }
        }
    }

    TcpClient client;
    client.sock = sock;
    client.closeTimeout = closeTimeout;

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
                    DBG_Printf(DBG_INFO, "Replace task in queue cluster 0x%04X with newer task of same type. %u runnig tasks\n", task.req.clusterId(), runningTasks.size());
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

    if (runningTasks.size() > 4)
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
        // drop dead unicasts
        if (i->lightNode && !i->lightNode->isAvailable())
        {
            DBG_Printf(DBG_INFO, "drop request to zombie\n");
            tasks.erase(i);
            return;
        }

        // send only few requests to a destination at a time
        int onAir = 0;
        const int maxOnAir = 2;
        std::list<TaskItem>::iterator j = runningTasks.begin();
        std::list<TaskItem>::iterator jend = runningTasks.end();

        bool ok = true;
        for (; j != jend; ++j)
        {
            if (i->req.dstAddress() == j->req.dstAddress())
            {
                onAir++;
                int dt = idleTotalCounter - j->sendTime;
                if (dt < 5 || onAir >= maxOnAir)
                {
                    if (dt > 120)
                    {
                        DBG_Printf(DBG_INFO, "drop request %u send time %d, cluster 0x%04X, onAir %d after %d seconds\n", j->req.id(), j->sendTime, j->req.clusterId(), onAir, dt);
                    }
                    else
                    {
                        DBG_Printf(DBG_INFO, "request %u send time %d, cluster 0x%04X, onAir %d\n", i->req.id(), j->sendTime, j->req.clusterId(), onAir);
                        DBG_Printf(DBG_INFO_L2, "delay sending request %u dt %d ms to %s\n", i->req.id(), dt, qPrintable(i->req.dstAddress().toStringExt()));
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
                DBG_Printf(DBG_INFO, "delay sending request %u to group 0x%04X\n", i->req.id(), i->req.dstAddress().group());
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
                        DBG_Printf(DBG_INFO, "enqueue APS request failed with error %d\n", ret);
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
                i->item(RStateReachable)->setValue(false);
                updateLightEtag(&*i);
            }
        }
    }
        break;

    case deCONZ::NodeEvent::NodeAdded:
    {
        addLightNode(event.node());
        addSensorNode(event.node());
    }
        break;

    case deCONZ::NodeEvent::NodeZombieChanged:
    {
        DBG_Printf(DBG_INFO, "Node zombie state changed %s\n", qPrintable(event.node()->address().toStringExt()));
        nodeZombieStateChanged(event.node());
    }
        break;

    case deCONZ::NodeEvent::UpdatedSimpleDescriptor:
    {
        addLightNode(event.node());
        addSensorNode(event.node());
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
        case ONOFF_SWITCH_CONFIGURATION_CLUSTER_ID:
        case ILLUMINANCE_MEASUREMENT_CLUSTER_ID:
        case ILLUMINANCE_LEVEL_SENSING_CLUSTER_ID:
        case TEMPERATURE_MEASUREMENT_CLUSTER_ID:
        case OCCUPANCY_SENSING_CLUSTER_ID:
        case IAS_ZONE_CLUSTER_ID:
        case BASIC_CLUSTER_ID:
            {
                addSensorNode(event.node());
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
                return;
            }
        }

        if (!i->removeScenes.empty())
        {
            if (addTaskRemoveScene(task, i->id, i->removeScenes[0]))
            {
                processTasks();
                return;
            }
        }

        if (!i->modifyScenes.empty())
        {
            if (i->modifyScenesRetries < GroupInfo::MaxActionRetries)
            {
                i->modifyScenesRetries++;
                if (addTaskAddScene(task, i->id, i->modifyScenes[0], task.lightNode->id()))
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

                QVector<quint8> responseScenes;
                for (uint i = 0; i < count; i++)
                {
                    if (!stream.atEnd())
                    {
                        uint8_t sceneId;
                        stream >> sceneId;
                        responseScenes.push_back(sceneId);

                        DBG_Printf(DBG_INFO, "0x%016X found scene 0x%02X for group 0x%04X\n", ind.srcAddress().ext(), sceneId, groupId);

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

                    if (!responseScenes.contains(i->id))
                    {
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
                }

                lightNode->enableRead(READ_SCENE_DETAILS);

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
                                    li->setOn(lightNode->isOn());
                                    li->setBri((uint8_t)lightNode->level());
                                    li->setX(lightNode->colorX());
                                    li->setY(lightNode->colorY());
                                    li->setColorloopActive(lightNode->isColorLoopActive());
                                    li->setColorloopTime(lightNode->colorLoopSpeed());
                                    foundLightstate = true;
                                    break;
                                }
                            }

                            if (!foundLightstate)
                            {
                                LightState state;
                                state.setLightId(lightNode->id());
                                state.setOn(lightNode->isOn());
                                state.setBri((uint8_t)lightNode->level());
                                state.setX(lightNode->colorX());
                                state.setY(lightNode->colorY());
                                state.setColorloopActive(lightNode->isColorLoopActive());
                                state.setColorloopTime(lightNode->colorLoopSpeed());
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
    else if (zclFrame.commandId() == 0x00) // Add scene response
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
                        if (lightNode->modelId().startsWith(QLatin1String("FLS-NB")))
                        {
                            DBG_Printf(DBG_INFO, "Start repair scene table for node %s (%s)\n", qPrintable(lightNode->name()), qPrintable(lightNode->swBuildId()));
                            fixSceneTableRead(lightNode, 0);
                        }
                    }
                }
            }
        }
    }
    else if (zclFrame.commandId() == 0x01) // View scene response
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
            quint8 onOff;
            quint8 bri;
            quint16 x;
            quint16 y;

            DBG_Printf(DBG_INFO, "View scene rsp 0x%016llX group 0x%04X scene 0x%02X\n", lightNode->address().ext(), groupId, sceneId);

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

                    if (stream.status() != QDataStream::ReadPastEnd)
                    {
                        hasXY = true;
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

            DBG_Printf(DBG_INFO, "\t t=%u, on=%u, bri=%u, x=%u, y=%u\n", transitionTime, onOff, bri, x, y);

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
                            lightNode->modelId().startsWith(QLatin1String("FLS-CT")))
                        {
                            newLightState.setColorMode(QLatin1String("ct"));
                            newLightState.setColorTemperature(x);
                        }
                        else
                        {
                            newLightState.setColorMode(QLatin1String("xy"));
                        }
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

        if (sensorNode && sensorNode->deletedState() == Sensor::StateNormal)
        {
            checkSensorNodeReachable(sensorNode);
        }

        DBG_Assert(zclFrame.payload().size() >= 3);

        QDataStream stream(zclFrame.payload());
        stream.setByteOrder(QDataStream::LittleEndian);

        uint16_t groupId;
        uint8_t sceneId;

        stream >> groupId;
        stream >> sceneId;

        // check if scene exists

        bool colorloopDeactivated = false;
        Group *group = getGroupForId(groupId);
        Scene *scene = group ? group->getScene(sceneId) : 0;

        if (group && (group->state() == Group::StateNormal) && scene)
        {
            std::vector<LightState>::const_iterator ls = scene->lights().begin();
            std::vector<LightState>::const_iterator lsend = scene->lights().end();

            for (; ls != lsend; ++ls)
            {
                LightNode *lightNode = getLightNodeForId(ls->lid());
                if (lightNode && lightNode->isAvailable() && lightNode->state() == LightNode::StateNormal)
                {
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

                    ResourceItem *item = lightNode->item(RStateOn);
                    if (item && item->toBool() != ls->on())
                    {
                        item->setValue(ls->on());
                        Event e(RLights, RStateOn, lightNode->id());
                        enqueueEvent(e);
                        changed = true;
                    }

                    item = lightNode->item(RStateBri);
                    if (ls->bri() != item->toNumber())
                    {
                        item->setValue(ls->bri());
                        Event e(RLights, RStateBri, lightNode->id());
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
                                Event e(RLights, RStateX, lightNode->id());
                                enqueueEvent(e);
                                changed = true;
                            }
                            item = lightNode->item(RStateY);
                            if (item && ls->y() != item->toNumber())
                            {
                                item->setValue(ls->y());
                                Event e(RLights, RStateY, lightNode->id());
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
                                Event e(RLights, RStateCt, lightNode->id());
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
                                Event e(RLights, RStateHue, lightNode->id());
                                enqueueEvent(e);
                                changed = true;
                            }

                            item = lightNode->item(RStateSat);
                            if (item && ls->saturation() != item->toNumber())
                            {
                                item->setValue(ls->saturation());
                                Event e(RLights, RStateSat, lightNode->id());
                                enqueueEvent(e);
                                changed = true;
                            }
                        }
                    }

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

    Group *group = 0;

    if (ind.dstAddressMode() == deCONZ::ApsGroupAddress)
    {
        group = getGroupForId(ind.dstAddress().group());
    }

    // update Nodes and Groups state if On/Off Command was send by a sensor
    if (group &&
        group->state() != Group::StateDeleted &&
        group->state() != Group::StateDeleteFromDB)
    {
        //found
        if (zclFrame.commandId() == 0x00 || zclFrame.commandId() == 0x40) // Off || Off with effect
        {
            group->setIsOn(false);
            // Deactivate sensor rules if present
            changeRuleStatusofGroup(group->id(), false);
        }
        else if (zclFrame.commandId() == 0x01 || zclFrame.commandId() == 0x42) // On || On with timed off
        {
            group->setIsOn(true);
            // Activate sensor rules if present
            changeRuleStatusofGroup(group->id(), true);
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
        updateEtag(group->etag);

        // check each light if colorloop needs to be disabled
        std::vector<LightNode>::iterator l = nodes.begin();
        std::vector<LightNode>::iterator lend = nodes.end();

        for (; l != lend; ++l)
        {
            if (isLightNodeInGroup(&*l, group->address()))
            {
                bool updated = false;
                if (zclFrame.commandId() == 0x00 || zclFrame.commandId() == 0x40) // Off || Off with effect
                {
                    if (l->isOn())
                    {
                        l->setIsOn(false);
                        updated = true;
                    }
                }
                else if (zclFrame.commandId() == 0x01 || zclFrame.commandId() == 0x42) // On || On with timed off
                {
                    if (!l->isOn())
                    {
                        l->setIsOn(true);
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
                    updateEtag(l->etag);
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
    Sensor *sensorNode = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), ep);
    //int endpointCount = getNumberOfEndpoints(ind.srcAddress().ext());
    int epIter = 0;

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

        while (!stream.atEnd())
        {
            stream >> groupId;
            stream >> type;
            DBG_Printf(DBG_INFO, " - Id: %u, type: %u\n", groupId, type);

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

            if (sensorNode && sensorNode->deletedState() != Sensor::StateDeleted)
            {
                sensorNode->clearRead(READ_GROUP_IDENTIFIERS);
                Group *group1 = getGroupForId(groupId);

                if (group1)
                {
                    if (group1->state() == Group::StateDeleted)
                    {
                        group1->setState(Group::StateNormal);
                    }

                    //not found
                    group1->addDeviceMembership(sensorNode->id());
                    queSaveDb(DB_GROUPS, DB_SHORT_SAVE_DELAY);
                    updateEtag(group1->etag);
                }

                ResourceItem *item = sensorNode->addItem(DataTypeString, RConfigGroup);
                QString gid = QString::number(groupId);

                if (item->toString() != gid)
                {
                    item->setValue(gid);
                    sensorNode->setNeedSaveDatabase(true);
                    queSaveDb(DB_GROUPS | DB_SENSORS, DB_SHORT_SAVE_DELAY);
                }

                Event e(RSensors, REventValidGroup, sensorNode->id());
                enqueueEvent(e);
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
                        DBG_Printf(DBG_INFO, "Turn off light 0x%016llX again after OTA\n", rc->address.ext());
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
                    break;
                }
            }

            if (node->endpoints().end() == std::find(node->endpoints().begin(),
                                                     node->endpoints().end(),
                                                     i->haEndpoint().endpoint()))
            {
                continue; // not a active endpoint
            }

            ResourceItem *reachable = i->item(RStateReachable);

            if (reachable && !reachable->toBool())
            {
                reachable->setValue(true);
                Event e(RLights, RStateReachable, i->id());
                enqueueEvent(e);

                // TODO only when permit join is active
                if (i->state() == LightNode::StateDeleted)
                {
                    i->setState(LightNode::StateNormal);
                    i->setNeedSaveDatabase(true);
                    queSaveDb(DB_LIGHTS,DB_SHORT_SAVE_DELAY);
                }
                updateEtag(gwConfigEtag);
            }

            DBG_Printf(DBG_INFO, "DeviceAnnce of LightNode: %s Permit Join: %i\n", qPrintable(i->address().toStringExt()), gwPermitJoinDuration);

            // force reading attributes

            i->enableRead(READ_MODEL_ID |
                          READ_SWBUILD_ID |
                          READ_COLOR |
                          READ_LEVEL |
                          READ_ON_OFF |
                          READ_GROUPS |
                          READ_SCENES);

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

    int found = 0;
    std::vector<Sensor>::iterator si = sensors.begin();
    std::vector<Sensor>::iterator send = sensors.end();

    for (; si != send; ++si)
    {
        if ((si->address().ext() == ext) || (si->address().nwk() == nwk))
        {
            found++;
            DBG_Printf(DBG_INFO, "DeviceAnnce of SensorNode: %s\n", qPrintable(si->address().toStringExt()));

            ResourceItem *item = si->item(RConfigReachable);
            if (item && !item->toBool())
            {
                item->setValue(true);
                Event e(RSensors, RConfigReachable, si->id());
                enqueueEvent(e);
            }
            checkSensorGroup(&*si);
            checkSensorBindingsForAttributeReporting(&*si);
            checkSensorBindingsForClientClusters(&*si);
            updateSensorEtag(&*si);

            if (findSensorsState == FindSensorsActive && si->node())
            {
                addSensorNode(si->node()); // check if somethings needs to be updated
            }
        }
    }

    if (findSensorsState == FindSensorsActive)
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
        handleIndicationFindSensors(ind, zclFrame);
    }
}

/*! Push data from a task into all LightNodes of a group or single LightNode.
 */
void DeRestPluginPrivate::taskToLocalData(const TaskItem &task)
{
    Group *group;
    Group dummyGroup;
    std::vector<LightNode*> pushNodes;

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
        updateEtag(group->etag);
        group->level = task.level;
        break;

    case TaskSetSat:
        updateEtag(group->etag);
        group->sat = task.sat;
        break;

    case TaskSetEnhancedHue:
        updateEtag(group->etag);
        group->hue = task.hue;
        group->hueReal = task.hueReal;
        break;

    case TaskSetHueAndSaturation:
        updateEtag(group->etag);
        group->sat = task.sat;
        group->hue = task.hue;
        group->hueReal = task.hueReal;
        break;

    case TaskSetXyColor:
        updateEtag(group->etag);
        group->colorX = task.colorX;
        group->colorY = task.colorY;
        break;

    case TaskSetColorTemperature:
        updateEtag(group->etag);
        group->colorTemperature = task.colorTemperature;
        break;

    case TaskSetColorLoop:
        updateEtag(group->etag);
        group->setColorLoopActive(task.colorLoop);
        break;

    default:
        break;
    }

    for (; i != end; ++i)
    {
        LightNode *lightNode = *i;

        switch (task.taskType)
        {
        case TaskSendOnOffToggle:
        {
            ResourceItem *item = lightNode->item(RStateOn);
            if (item && item->toBool() != task.onOff)
            {
                updateLightEtag(lightNode);
                item->setValue(task.onOff);
                Event e(RLights, RStateOn, lightNode->id());
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
                Event e(RLights, RStateOn, lightNode->id());
                enqueueEvent(e);
            }

            item = lightNode->item(RStateBri);
            if (item && item->toNumber() != task.level)
            {
                updateLightEtag(lightNode);
                item->setValue(task.level);
                Event e(RLights, RStateBri, lightNode->id());
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
            updateEtag(lightNode->etag);
            lightNode->setSaturation(task.sat);
            lightNode->setColorMode(QLatin1String("hs"));
            setAttributeSaturation(lightNode);
            break;

        case TaskSetEnhancedHue:
            updateEtag(lightNode->etag);
            lightNode->setEnhancedHue(task.enhancedHue);
            lightNode->setColorMode(QLatin1String("hs"));
            setAttributeEnhancedHue(lightNode);
            break;

        case TaskSetHueAndSaturation:
            updateEtag(lightNode->etag);
            lightNode->setSaturation(task.sat);
            lightNode->setEnhancedHue(task.enhancedHue);
            lightNode->setColorMode(QLatin1String("hs"));
            setAttributeSaturation(lightNode);
            setAttributeEnhancedHue(lightNode);
            break;

        case TaskSetXyColor:
            updateEtag(lightNode->etag);
            lightNode->setColorXY(task.colorX, task.colorY);
            lightNode->setColorMode(QLatin1String("xy"));
            setAttributeColorXy(lightNode);
            break;

        case TaskSetColorTemperature:
            updateEtag(lightNode->etag);
            lightNode->setColorTemperature(task.colorTemperature);
            lightNode->setColorMode(QLatin1String("ct"));
            setAttributeColorTemperature(lightNode);
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
void DeRestPluginPrivate::delayedFastEnddeviceProbe()
{
    const SensorCandidate *sc = 0;
    {
        std::vector<SensorCandidate>::const_iterator i = findSensorCandidates.begin();
        std::vector<SensorCandidate>::const_iterator end = findSensorCandidates.end();

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

    {
        Sensor *sensor = getSensorNodeForAddress(sc->address);
        const deCONZ::Node *node = sensor ? sensor->node() : 0;

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

        if (node->nodeDescriptor().isNull())
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

        if (node->endpoints().empty())
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
        if (node->simpleDescriptors().size() != (int)node->endpoints().size())
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

        // model id, sw build id
        if (!sensor /*&& (sensor->modelId().isEmpty() || sensor->swVersion().isEmpty())*/)
        {
            deCONZ::ApsDataRequest apsReq;

            DBG_Printf(DBG_INFO, "[4] get model id, sw build id for 0x%016llx\n", sc->address.ext());

            // ZDP Header
            apsReq.dstAddress() = sc->address;
            apsReq.setDstAddressMode(deCONZ::ApsNwkAddress);
            apsReq.setDstEndpoint(node->endpoints()[0]);
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

            { // payload
                QDataStream stream(&zclFrame.payload(), QIODevice::WriteOnly);
                stream.setByteOrder(QDataStream::LittleEndian);

                stream << (quint16)0x0005; // model id
                stream << (quint16)0x4000; // sw build id
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

        if (sensor->modelId() == QLatin1String("RWL020") ||
            sensor->modelId() == QLatin1String("RWL021"))
        {
            ResourceItem *item = sensor->item(RConfigGroup);
            if (!item || !item->lastSet().isValid())
            {
                getGroupIdentifiers(sensor, 0x01, 0x00);
                return;
            }
        }

        std::vector<Sensor>::iterator i = sensors.begin();
        std::vector<Sensor>::iterator end = sensors.end();
        for (; i != end; i++)
        {
            if (i->address().ext() == sc->address.ext())
            {
                if (findSensorsState == FindSensorsActive)
                {
                    if (i->deletedState() == Sensor::StateDeleted)
                    {
                        // reanimate
                        i->setDeletedState(Sensor::StateNormal);
                        i->setNeedSaveDatabase(true);
                        Event e(RSensors, REventAdded, i->id());
                        enqueueEvent(e);
                    }
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

    if (d->idleLastActivity < IDLE_USER_LIMIT)
    {
        return;
    }

    if (!d->gwDeviceAddress.hasExt() && d->apsCtrl)
    {
        d->gwDeviceAddress.setExt(d->apsCtrl->getParameter(deCONZ::ParamMacAddress));
        d->gwDeviceAddress.setNwk(d->apsCtrl->getParameter(deCONZ::ParamNwkAddress));
    }

    if (!pluginActive())
    {
        return;
    }

    if (!d->isInNetwork())
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
        if ((d->idleTotalCounter - rc.idleTotalCounterCopy) > 240)
        {
            DBG_Printf(DBG_INFO, "Pop recover info for 0x%016llX\n", rc.address.ext());
            d->recoverOnOff.pop_back();
        }
    }

    bool processLights = false;

    if (d->idleLimit <= 0)
    {
        QTime t = QTime::currentTime();

        if (d->queryTime > t)
        {
            if (t.secsTo(d->queryTime) < (60 * 30)) // prevent stallation
            {
                DBG_Printf(DBG_INFO_L2, "Wait %ds till query finished\n", t.secsTo(d->queryTime));
                return; // wait finish
            }
        }

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

                if (!lightNode->isAvailable())
                {
                    continue;
                }

                if (processLights)
                {
                    break;
                }

                const uint32_t items[]   = { READ_ON_OFF, READ_LEVEL, READ_COLOR, READ_GROUPS, READ_SCENES, 0 };
                const int tRead[]        = {         120,        120,        240,         600,         600, 0 };
                const quint16 clusters[] = {      0x0006,     0x0008,     0x0300,      0xffff,      0xffff, 0 };
                const quint16 attrs[]    = {      0x0000,     0x0000,     0x0003,           0,           0, 0 };

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
                        if (clusters[i] == COLOR_CLUSTER_ID && !lightNode->hasColor())
                        {
                            continue;
                        }

                        if (clusters[i] != 0xffff)
                        {
                            const NodeValue &val = lightNode->getZclValue(clusters[i], attrs[i]);

                            if (val.updateType == NodeValue::UpdateByZclRead ||
                                val.updateType == NodeValue::UpdateByZclReport)
                            {
                                if (val.timestamp.isValid() && val.timestamp.elapsed() < (tRead[i] * 1000))
                                {
                                    // fresh enough
                                    continue;
                                }
                            }
                        }

                        lightNode->setNextReadTime(items[i], d->queryTime);
                        lightNode->setLastRead(items[i], d->idleTotalCounter);
                        lightNode->enableRead(items[i]);
                        d->queryTime = d->queryTime.addSecs(tSpacing);
                        processLights = true;
                    }
                }

                if (lightNode->modelId().isEmpty())
                {
                    Sensor *sensor = d->getSensorNodeForAddress(lightNode->address());

                    if (sensor && sensor->modelId().startsWith(QLatin1String("FLS-NB")))
                    {
                        // extract model id from sensor node
                        lightNode->setModelId(sensor->modelId());
                        lightNode->setLastRead(READ_MODEL_ID, d->idleTotalCounter);
                    }
                }

                if (!lightNode->mustRead(READ_MODEL_ID) && (lightNode->modelId().isEmpty() || lightNode->lastRead(READ_MODEL_ID) < d->idleTotalCounter - READ_MODEL_ID_INTERVAL))
                {
                    lightNode->setLastRead(READ_MODEL_ID, d->idleTotalCounter);
                    lightNode->enableRead(READ_MODEL_ID);
                    lightNode->setNextReadTime(READ_MODEL_ID, d->queryTime);
                    d->queryTime = d->queryTime.addSecs(tSpacing);
                    processLights = true;
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
                    DBG_Printf(DBG_INFO, "Force read attributes for node %s\n", qPrintable(lightNode->name()));
                }

                // don't query low priority items when OTA is busy
                if (d->otauLastBusyTimeDelta() > OTA_LOW_PRIORITY_TIME)
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
                        DBG_Printf(DBG_INFO, "Force binding of attribute reporting for node %s\n", qPrintable(lightNode->name()));
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

                if (!sensorNode->isAvailable())
                {
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
                            val.timestampLastReport.secsTo(t) < (60 * 45)) // got update in timely manner
                        {
                            DBG_Printf(DBG_INFO, "binding for attribute reporting SensorNode %s of cluster 0x%04X seems to be active\n", qPrintable(sensorNode->name()), *ci);
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

                                if (!val.timestamp.isValid() || val.timestamp.secsTo(t) > 1800)
                                {
                                    sensorNode->enableRead(READ_OCCUPANCY_CONFIG);
                                    sensorNode->setLastRead(READ_OCCUPANCY_CONFIG, d->idleTotalCounter);
                                    sensorNode->setNextReadTime(READ_OCCUPANCY_CONFIG, d->queryTime);
                                    d->queryTime = d->queryTime.addSecs(tSpacing);
                                    processSensors = true;
                                }
                            }
                        }
                    }

                    DBG_Printf(DBG_INFO, "Force read attributes for SensorNode %s\n", qPrintable(sensorNode->name()));
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
                    DBG_Printf(DBG_INFO, "Force binding of attribute reporting for node %s\n", qPrintable(sensorNode->name()));
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

    if (d->lightAttrIter >= d->nodes.size())
    {
        d->lightAttrIter = 0;
    }

    while (d->lightAttrIter < d->nodes.size())
    {
        LightNode *lightNode = &d->nodes[d->lightAttrIter];
        d->lightAttrIter++;

        if (d->processZclAttributes(lightNode))
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

        connect(m_w, SIGNAL(refreshAllClicked()),
                this, SLOT(refreshAll()));

        connect(m_w, SIGNAL(changeChannelClicked(quint8)),
                d, SLOT(changeChannel(quint8)));
    }

    return m_w;
}

/*! Checks if a request is addressed to this plugin.
    \param hdr - the http header of the request
    \return true - if the request could be processed
 */
bool DeRestPlugin::isHttpTarget(const QHttpRequestHeader &hdr)
{
    if (hdr.path().startsWith(QLatin1String("/api/config")))
    {
        return true;
    }
    else if (hdr.path().startsWith(QLatin1String("/api")))
    {
        QString path = hdr.path();
        int quest = path.indexOf('?');

        if (quest > 0)
        {
            path = path.mid(0, quest);
        }

        QStringList ls = path.split(QLatin1String("/"), QString::SkipEmptyParts);

        if (ls.size() > 2)
        {
            if ((ls[2] == QLatin1String("lights")) ||
                (ls[2] == QLatin1String("groups")) ||
                (ls[2] == QLatin1String("config")) ||
                (ls[2] == QLatin1String("schedules")) ||
                (ls[2] == QLatin1String("sensors")) ||
                (ls[2] == QLatin1String("touchlink")) ||
                (ls[2] == QLatin1String("rules")) ||
                (ls[2] == QLatin1String("userparameter")) ||
                (ls[2] == QLatin1String("gateways")) ||
                (hdr.path().at(4) != '/') /* Bug in some clients */)
            {
                return true;
            }
        }
        else // /api, /api/config and /api/287398279837
        {
            return true;
        }
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

    if (m_state == StateOff)
    {
        if (d->apsCtrl && (d->apsCtrl->networkState() == deCONZ::InNetwork))
        {
            m_state = StateIdle;
        }
    }

    QUrl url(hdrmod.path()); // get rid of query string
    QString strpath = url.path();

    if (hdrmod.path().startsWith(QLatin1String("/api")))
    {
        // some clients send /api123 instead of /api/123
        // correct the path here
        if (hdrmod.path().length() > 4 && hdrmod.path().at(4) != '/')
        {
            strpath.insert(4, '/');
        }
    }

    hdrmod.setRequest(hdrmod.method(), strpath);

    DBG_Printf(DBG_HTTP, "HTTP API %s %s - %s\n", qPrintable(hdr.method()), qPrintable(hdrmod.path()), qPrintable(sock->peerAddress().toString()));

    //qDebug() << hdr.toString();

    if(hdr.hasKey(QLatin1String("Content-Type")) &&
       hdr.value(QLatin1String("Content-Type")).startsWith(QLatin1String("multipart/form-data")))
    {
        DBG_Printf(DBG_HTTP, "Binary Data: \t%s\n", qPrintable(content));
    }
    else if (!stream.atEnd())
    {
        content = stream.readAll();
        DBG_Printf(DBG_HTTP, "Text Data: \t%s\n", qPrintable(content));
    }

    connect(sock, SIGNAL(destroyed()),
            d, SLOT(clientSocketDestroyed()));

    QStringList path = hdrmod.path().split(QLatin1String("/"), QString::SkipEmptyParts);
    ApiRequest req(hdrmod, path, sock, content);
    ApiResponse rsp;

    rsp.httpStatus = HttpStatusNotFound;
    rsp.contentType = HttpContentHtml;

    int ret = REQ_NOT_HANDLED;

    // general response to a OPTIONS HTTP method
    if (req.hdr.method() == QLatin1String("OPTIONS"))
    {
        stream << "HTTP/1.1 200 OK\r\n";
        stream << "Cache-Control: no-store, no-cache, must-revalidate, post-check=0, pre-check=0\r\n";
        stream << "Pragma: no-cache\r\n";
        stream << "Connection: close\r\n";
        stream << "Access-Control-Max-Age: 0\r\n";
        stream << "Access-Control-Allow-Origin: *\r\n";
        stream << "Access-Control-Allow-Credentials: true\r\n";
        stream << "Access-Control-Allow-Methods: POST, GET, OPTIONS, PUT, DELETE\r\n";
        stream << "Access-Control-Allow-Headers: Access-Control-Allow-Origin, Content-Type\r\n";
        stream << "Access-Control-Expose-Headers: Gateway-Name, Gateway-Uuid\r\n";
        stream << "Content-Type: text/html\r\n";
        stream << "Content-Length: 0\r\n";
        stream << "Gateway-Name: " << d->gwName << "\r\n";
        stream << "Gateway-Uuid: " << d->gwUuid << "\r\n";
        stream << "\r\n";
        req.sock->flush();
        return 0;
    }

    if (req.hdr.method() == QLatin1String("POST") && path.size() == 2 && path[1] == QLatin1String("fileupload"))
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
        stream << "\r\n";
        stream.flush();
        return 0;
    }

    if (path.size() > 2)
    {
        if (path[2] == QLatin1String("lights"))
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
        else if (path[2] == QLatin1String("touchlink"))
        {
            ret = d->handleTouchlinkApi(req, rsp);
        }
        else if (path[2] == QLatin1String("sensors"))
        {
            ret = d->handleSensorsApi(req, rsp);
        }
        else if (path[2] == QLatin1String("rules"))
        {
            ret = d->handleRulesApi(req, rsp);
        }
        else if (path[2] == QLatin1String("userparameter"))
        {
            ret = d->handleUserparameterApi(req, rsp);
        }
        else if (path[2] == QLatin1String("gateways"))
        {
            ret = d->handleGatewaysApi(req, rsp);
        }
    }

    if (ret == REQ_NOT_HANDLED)
    {
        ret = d->handleConfigurationApi(req, rsp);
    }

    if (ret == REQ_DONE)
    {
        return 0;
    }
    else if (ret == REQ_READY_SEND)
    {
        // new api // TODO cleanup/remove later
        // sending below
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
        d->pushClientForClose(sock, 2);
        stream << "\r\n";
        stream << d->descriptionXml.constData();
        stream.flush();
        return 0;

    }
    else
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

    stream << "HTTP/1.1 " << rsp.httpStatus << "\r\n";
    stream << "Access-Control-Allow-Origin: *\r\n";
    stream << "Content-Type: " << rsp.contentType << "\r\n";
    stream << "Content-Length:" << QString::number(str.toUtf8().size()) << "\r\n";

    bool keepAlive = false;
    if (hdr.hasKey(QLatin1String("Connection")))
    {
        if (hdr.value(QLatin1String("Connection")).toLower() == QLatin1String("keep-alive"))
        {
            keepAlive = true;
            d->pushClientForClose(sock, 3);
        }
    }
    if (!keepAlive)
    {
        stream << "Connection: close\r\n";
        d->pushClientForClose(sock, 2);
    }

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

/*! save Rule State (timesTriggered, lastTriggered) in DB only if
 *  no Button was pressed for 3 seconds.
 */
void DeRestPluginPrivate::saveCurrentRuleInDbTimerFired()
{
    queSaveDb(DB_RULES , DB_SHORT_SAVE_DELAY);
}

/*! Checks if some tcp connections could be closed.
 */
void DeRestPluginPrivate::openClientTimerFired()
{
    std::list<TcpClient>::iterator i = openClients.begin();
    std::list<TcpClient>::iterator end = openClients.end();

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
    }
}

/*! Is called before the client socket will be deleted.
 */
void DeRestPluginPrivate::clientSocketDestroyed()
{
    QObject *obj = sender();
    QTcpSocket *sock = static_cast<QTcpSocket *>(obj);

    std::list<TcpClient>::iterator i = openClients.begin();
    std::list<TcpClient>::iterator end = openClients.end();

    for ( ; i != end; ++i)
    {
        if (i->sock == sock)
        {
            openClients.erase(i);
            return;
        }
    }
}

/*! Returns the endpoint number of the HA endpoint.
    \return 1..254 - on success
            0 - if not found
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

    return 0;
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
    else
    {
        uid.sprintf("%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x-%02x",
                    a.bytes[7], a.bytes[6], a.bytes[5], a.bytes[4],
                    a.bytes[3], a.bytes[2], a.bytes[1], a.bytes[0],
                    endpoint);
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
            //TODO: Win: provide 7zip or other
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
        args.append("-o");
        args.append(path);
        archProcess->start(cmd, args);
#endif
#ifdef Q_OS_LINUX
        archProcess->start("gzip -df " + path + "/deCONZ.tar.gz");
#endif
        archProcess->waitForFinished(EXT_PROCESS_TIMEOUT);
        DBG_Printf(DBG_INFO, "%s\n", qPrintable(archProcess->readAllStandardOutput()));
        archProcess->deleteLater();
        archProcess = 0;

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
        args.append("-o");
        args.append(path);
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

            //reset Endpoint config
            QVariantMap epData;
            QVariantList inClusters;
            inClusters.append("0x19");

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
        return getSensorNodeForId(id);
    }
    else if (resource == RConfig)
    {
        return 0; // TODO
    }

    return 0;
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
        //perhaps remove this function to another location in future
        #ifdef ARCH_ARM
        qApp->exit(APP_RET_RESTART_APP);
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
