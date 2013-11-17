/*
 * Copyright (C) 2013 dresden elektronik ingenieurtechnik gmbh.
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
#include <QHttpRequestHeader>
#include <QtPlugin>
#include <QPushButton>
#include <QTextCodec>
#include <QTime>
#include <QTimer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QUrl>
#include <QCryptographicHash>
#include <queue>
#include "colorspace.h"
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "de_web_widget.h"
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

static int ReadAttributesDelay = 750;
static int ReadAttributesLongDelay = 5000;
static int ReadAttributesLongerDelay = 60000;
static uint MaxGroupTasks = 4;

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

    connect(databaseTimer, SIGNAL(timeout()),
            this, SLOT(saveDatabaseTimerFired()));

    db = 0;
    saveDatabaseItems = 0;
    sqliteDatabaseName = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
    sqliteDatabaseName.append("/zll.db");
    idleLimit = 0;
    idleTotalCounter = 0;
    idleLastActivity = 0;
    udpSock = 0;
    gwGroupSendDelay = deCONZ::appArgumentNumeric("--group-delay", GROUP_SEND_DELAY);

    gwLinkButton = false;
    gwOtauActive = false;

    apsCtrl = deCONZ::ApsController::instance();
    DBG_Assert(apsCtrl != 0);

    // starttime reference counts from here
    starttimeRef.start();

    // default configuration
    gwPermitJoinDuration = 0;
    gwName = GW_DEFAULT_NAME;
    gwUpdateVersion = GW_SW_VERSION; // will be replaced by discovery handler
    configToMap(gwConfig);
    updateEtag(gwConfigEtag);

    // set some default might be overwritten by database
    gwAnnounceInterval = ANNOUNCE_INTERVAL;
    gwAnnounceUrl = "http://dresden-light.appspot.com/discover";

    openDb();
    initDb();
    readDb();
    closeDb();

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

    taskTimer = new QTimer(this);
    taskTimer->setSingleShot(false);
    connect(taskTimer, SIGNAL(timeout()),
            this, SLOT(processTasks()));
    taskTimer->start(100);

    groupTaskTimer = new QTimer(this);
    taskTimer->setSingleShot(false);
    connect(taskTimer, SIGNAL(timeout()),
            this, SLOT(processGroupTasks()));
    taskTimer->start(250);

    lockGatewayTimer = new QTimer(this);
    lockGatewayTimer->setSingleShot(true);
    connect(lockGatewayTimer, SIGNAL(timeout()),
            this, SLOT(lockGatewayTimerFired()));

    openClientTimer = new QTimer(this);
    openClientTimer->setSingleShot(false);
    connect(openClientTimer, SIGNAL(timeout()),
            this, SLOT(openClientTimerFired()));
    openClientTimer->start(1000);

    initAuthentification();
    initInternetDicovery();
    initSchedules();
    initPermitJoin();
    initOtau();
}

/*! Deconstructor for pimpl.
 */
DeRestPluginPrivate::~DeRestPluginPrivate()
{
}

/*! APSDE-DATA.indication callback.
    \param ind - the indication primitive
    \note Will be called from the main application for each incoming indication.
    Any filtering for nodes, profiles, clusters must be handled by this plugin.
 */
void DeRestPluginPrivate::apsdeDataIndication(const deCONZ::ApsDataIndication &ind)
{
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
            break;

        default:
            break;
        }
    }
    else if (ind.profileId() == ZDP_PROFILE_ID)
    {
        switch (ind.clusterId())
        {
        case ZDP_DEVICE_ANNCE_CLID:
            handleDeviceAnnceIndication(ind);
            break;

        default:
            break;
        }
    }
    else if (ind.profileId() == DE_PROFILE_ID)
    {
        otauDataIndication(ind);
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
        if (task.req.id() == conf.id())
        {
            DBG_Printf(DBG_INFO_L2, "Erase task zclSequenceNumber: %u\n", task.zclFrame.sequenceNumber());
            runningTasks.erase(i);
            processTasks();

            if (conf.status() != deCONZ::ApsSuccessStatus)
            {
                DBG_Printf(DBG_INFO, "error APSDE-DATA.confirm: 0x%02X on task\n", conf.status());
            }
            // TODO: check if some action shall be done based on confirm status

            return;
        }
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
    etag = QString(QCryptographicHash::hash(time.toString().toAscii(), QCryptographicHash::Md5).toHex());
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

/*! Adds a new node to node cache.
    Only supported ZLL and HA nodes will be added.
    \param node - the base for the LightNode
    \return the new LightNode or 0
 */
LightNode *DeRestPluginPrivate::addNode(const deCONZ::Node *node)
{
    LightNode lightNode;
    lightNode.setNode(0);
    lightNode.setIsAvailable(true);

    // check if node already exist
    LightNode *lightNode2 = getLightNodeForAddress(node->address().ext());

    if (lightNode2)
    {
        if (!lightNode2->isAvailable())
        {
            // the node existed before
            // refresh all with new values
            DBG_Printf(DBG_INFO, "LightNode %u: %s updated\n", lightNode.id().toUInt(), qPrintable(lightNode.name()));
            lightNode2->setIsAvailable(true);
            lightNode2->setNextReadTime(QTime::currentTime().addMSecs(ReadAttributesLongDelay));
            lightNode2->enableRead(READ_MODEL_ID |
                                   READ_SWBUILD_ID |
                                   READ_COLOR |
                                   READ_LEVEL |
                                   READ_ON_OFF |
                                   READ_GROUPS |
                                   READ_SCENES);

            lightNode2->setLastRead(idleTotalCounter);
            updateEtag(lightNode2->etag);
        }
        return lightNode2;
    }

    QList<deCONZ::SimpleDescriptor>::const_iterator i = node->simpleDescriptors().constBegin();
    QList<deCONZ::SimpleDescriptor>::const_iterator end = node->simpleDescriptors().constEnd();

    for (;i != end; ++i)
    {
        if (i->profileId() == HA_PROFILE_ID)
        {
            // filter for supported devices
            switch (i->deviceId())
            {
            case DEV_ID_HA_ONOFF_LIGHT:
            case DEV_ID_HA_DIMMABLE_LIGHT:
            case DEV_ID_HA_COLOR_DIMMABLE_LIGHT:
                {
                    lightNode.setHaEndpoint(*i);
                }
                break;

            default:
                break;
            }
        }
        else if (i->profileId() == ZLL_PROFILE_ID)
        {
            // filter for supported devices
            switch (i->deviceId())
            {
            case DEV_ID_ZLL_ONOFF_LIGHT:
            case DEV_ID_ZLL_ONOFF_PLUGIN_UNIT:
            case DEV_ID_ZLL_DIMMABLE_LIGHT:
            case DEV_ID_ZLL_DIMMABLE_PLUGIN_UNIT:
            case DEV_ID_ZLL_COLOR_LIGHT:
            case DEV_ID_ZLL_EXTENDED_COLOR_LIGHT:
            case DEV_ID_ZLL_COLOR_TEMPERATURE_LIGHT:
                {
                    lightNode.setHaEndpoint(*i);
                }
                break;

            default:
                break;
            }
        }
    }

    if (lightNode.haEndpoint().isValid())
    {
        lightNode.setNode(const_cast<deCONZ::Node*>(node));
        lightNode.address() = node->address();
        lightNode.setManufacturerCode(node->nodeDescriptor().manufacturerCode());

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

        // force reading attributes
        lightNode.setNextReadTime(QTime::currentTime().addMSecs(ReadAttributesLongDelay));
        lightNode.enableRead(READ_MODEL_ID |
                             READ_SWBUILD_ID |
                             READ_COLOR |
                             READ_LEVEL |
                             READ_ON_OFF |
                             READ_GROUPS |
                             READ_SCENES);
        lightNode.setLastRead(idleTotalCounter);

        DBG_Printf(DBG_INFO, "LightNode %u: %s added\n", lightNode.id().toUInt(), qPrintable(lightNode.name()));
        nodes.push_back(lightNode);
        lightNode2 = &nodes.back();

        p->startReadTimer(ReadAttributesDelay);
        updateEtag(lightNode2->etag);
        return lightNode2;
    }

    return 0;
}

/*! Checks if a known node changed its reachable state changed.
    \param node - the base for the LightNode
    \return the related LightNode or 0
 */
LightNode *DeRestPluginPrivate::nodeZombieStateChanged(const deCONZ::Node *node)
{
    if (!node)
    {
        return 0;
    }

    LightNode *lightNode = 0;

    lightNode = getLightNodeForAddress(node->address().ext());

    if (lightNode)
    {
        bool available = !node->isZombie();
        if (lightNode->isAvailable() != available)
        {
            lightNode->setIsAvailable(available);
            updateEtag(lightNode->etag);
            updateEtag(gwConfigEtag);
        }
    }

    return lightNode;
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
    LightNode *lightNode = getLightNodeForAddress(event.node()->address().ext());

    if (!lightNode || !lightNode->isAvailable())
    {
        lightNode = addNode(event.node());

        if (!lightNode)
        {
            // was no relevant node
            return 0;
        }

        updated = true;
    }

    if (lightNode->isAvailable())
    {
        if ((event.node()->state() == deCONZ::FailureState) || event.node()->isZombie())
        {
            lightNode->setIsAvailable(false);
            updated = true;
        }
    }
    else
    {
        if (event.node()->state() != deCONZ::FailureState)
        {
            lightNode->setIsAvailable(true);
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
        if (i->profileId() == HA_PROFILE_ID || i->profileId() == ZLL_PROFILE_ID)
        {
            // copy whole endpoint as reference
            lightNode->setHaEndpoint(*i);

            QList<deCONZ::ZclCluster>::const_iterator ic = lightNode->haEndpoint().inClusters().constBegin();
            QList<deCONZ::ZclCluster>::const_iterator endc = lightNode->haEndpoint().inClusters().constEnd();

            for (; ic != endc; ++ic)
            {
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
                                updated = true;
                            }
                        }
                        else if (ia->id() == 0x0001) // current saturation
                        {
                            uint8_t sat = ia->numericValue().u8;
                            if (lightNode->saturation() != sat)
                            {
                                lightNode->setSaturation(sat);
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
                            if (lightNode->level() != level)
                            {
                                DBG_Printf(DBG_INFO, "level %u --> %u\n", lightNode->level(), level);
                                lightNode->setLevel(level);
                                updated = true;
                            }
                        }
                    }
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
                            if (lightNode->isOn() != on)
                            {
                                lightNode->setIsOn(on);
                                updated = true;
                            }
                        }
                    }
                }
                else if (ic->id() == BASIC_CLUSTER_ID && (event.clusterId() == BASIC_CLUSTER_ID))
                {
                    std::vector<deCONZ::ZclAttribute>::const_iterator ia = ic->attributes().begin();
                    std::vector<deCONZ::ZclAttribute>::const_iterator enda = ic->attributes().end();
                    for (;ia != enda; ++ia)
                    {
                        if (ia->id() == 0x0005) // Model identifier
                        {
                            QString str = ia->toString();
                            if (!str.isEmpty())
                            {
                                lightNode->setModelId(str);
                                updated = true;
                            }
                        }
                        else if (ia->id() == 0x4000) // Software build identifier
                        {
                            QString str = ia->toString();
                            if (!str.isEmpty())
                            {
                                lightNode->setSwBuildId(str);
                                updated = true;
                            }
                        }
                    }
                }
            }

            break;
        }
    }

    if (updated)
    {
        updateEtag(lightNode->etag);
        updateEtag(gwConfigEtag);
    }

    return lightNode;
}

/*! Returns a LightNode for a given MAC address or 0 if not found.
 */
LightNode *DeRestPluginPrivate::getLightNodeForAddress(uint64_t extAddr)
{
    std::vector<LightNode>::iterator i;
    std::vector<LightNode>::iterator end = nodes.end();

    for (i = nodes.begin(); i != end; ++i)
    {
        if (i->address().ext() == extAddr)
        {
            return &(*i);
        }
    }

    return 0;
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

/*! Returns a GroupInfo in a LightNode for a given group (will be created if not exist).
 */
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
uint8_t DeRestPluginPrivate::getSrcEndpoint(LightNode *lightNode, const deCONZ::ApsDataRequest &req)
{
    Q_UNUSED(lightNode);
    Q_UNUSED(req);
    // TODO: query from controller
    return 0x01;
}

/*! Check and process queued attributes marked for read.
    \return true - if at least one attribute was processed
 */
bool DeRestPluginPrivate::processReadAttributes(LightNode *lightNode)
{
    DBG_Assert(lightNode != 0);

    if (!lightNode)
    {
        return false;
    }

    // check if read should happen now
    if (lightNode->nextReadTime() > QTime::currentTime())
    {
        return false;
    }

    if (!lightNode->isAvailable())
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
            readColor = true;
            //fall through

        case DEV_ID_HA_DIMMABLE_LIGHT:
            readLevel = true;
            //fall through

        case DEV_ID_HA_ONOFF_LIGHT:
            readOnOff = true;
            break;

        default:
            break;
        }
    }

    if (lightNode->mustRead(READ_MODEL_ID))
    {
        std::vector<uint16_t> attributes;
        attributes.push_back(0x0005); // Model identifier

        if (readAttributes(lightNode, &lightNode->haEndpoint(), BASIC_CLUSTER_ID, attributes))
        {
            lightNode->clearRead(READ_MODEL_ID);
            processed++;
        }
    }

    if (lightNode->mustRead(READ_SWBUILD_ID))
    {
        std::vector<uint16_t> attributes;
        attributes.push_back(0x4000); // Software build identifier

        if (readAttributes(lightNode, &lightNode->haEndpoint(), BASIC_CLUSTER_ID, attributes))
        {
            lightNode->clearRead(READ_SWBUILD_ID);
            processed++;
        }
    }

    if (readOnOff && lightNode->mustRead(READ_ON_OFF))
    {
        std::vector<uint16_t> attributes;
        attributes.push_back(0x0000); // OnOff

        if (readAttributes(lightNode, &lightNode->haEndpoint(), ONOFF_CLUSTER_ID, attributes))
        {
            lightNode->clearRead(READ_ON_OFF);
            processed++;
        }
    }

    if (readLevel && lightNode->mustRead(READ_LEVEL))
    {
        std::vector<uint16_t> attributes;
        attributes.push_back(0x0000); // Level

        if (readAttributes(lightNode, &lightNode->haEndpoint(), LEVEL_CLUSTER_ID, attributes))
        {
            lightNode->clearRead(READ_LEVEL);
            processed++;
        }
    }

    if (readColor && lightNode->mustRead(READ_COLOR))
    {
        std::vector<uint16_t> attributes;
        attributes.push_back(0x0000); // Current hue
        attributes.push_back(0x0001); // Current saturation
        attributes.push_back(0x0003); // Current x
        attributes.push_back(0x0004); // Current y
        attributes.push_back(0x4000); // Enhanced hue

        if (readAttributes(lightNode, &lightNode->haEndpoint(), COLOR_CLUSTER_ID, attributes))
        {
            lightNode->clearRead(READ_COLOR);
            processed++;
        }
    }

    if (lightNode->mustRead(READ_GROUPS))
    {
        std::vector<uint16_t> groups; // empty meaning read all groups
        if (readGroupMembership(lightNode, groups))
        {
            lightNode->clearRead(READ_GROUPS);
            processed++;
        }
    }

    if (lightNode->mustRead(READ_SCENES) && !lightNode->groups().empty())
    {
        std::vector<GroupInfo>::iterator i = lightNode->groups().begin();
        std::vector<GroupInfo>::iterator end = lightNode->groups().end();

        int rd = 0;

        for (; i != end; ++i)
        {
            Group *group = getGroupForId(i->id);

            DBG_Assert(group != 0);
            if (group)
            {
                // NOTE: this may cause problems if we have a lot of nodes + groups
                // proposal mark groups for which scenes where discovered
                if (readSceneMembership(lightNode, group))
                {
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

        processed++;
    }

    return (processed > 0);
}

/*! Queue reading ZCL attributes of a node.
    \param lightNode the node from whch the attributes shall be read
    \param sd the simple descriptor for the endpoint of interrest
    \param clusterId the cluster id related to the attributes
    \param attributes a list of attribute ids which shall be read
    \return true if the request is queued
 */
bool DeRestPluginPrivate::readAttributes(LightNode *lightNode, const deCONZ::SimpleDescriptor *sd, uint16_t clusterId, const std::vector<uint16_t> &attributes)
{
    DBG_Assert(lightNode != 0);
    DBG_Assert(sd != 0);
    DBG_Assert(!attributes.empty());

    if (!lightNode || !sd || attributes.empty() || !lightNode->isAvailable())
    {
        return false;
    }

    TaskItem task;
    task.taskType = TaskReadAttributes;

    task.req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
    task.req.setDstEndpoint(sd->endpoint());
    task.req.setDstAddressMode(deCONZ::ApsExtAddress);
    task.req.dstAddress() = lightNode->address();
    task.req.setClusterId(clusterId);
    task.req.setProfileId(HA_PROFILE_ID);
    task.req.setSrcEndpoint(getSrcEndpoint(lightNode, task.req));

    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(deCONZ::ZclReadAttributesId);
    task.zclFrame.setFrameControl(deCONZ::ZclFCProfileCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCDisableDefaultResponse);

    { // payload
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        for (uint i = 0; i < attributes.size(); i++)
        {
            stream << attributes[i];
        }
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

    task.req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
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

    // check if the group is known in the node
    std::vector<GroupInfo>::iterator i = lightNode->groups().begin();
    std::vector<GroupInfo>::iterator end = lightNode->groups().end();

    for (; i != end; ++i)
    {
        if (i->id == groupId)
        {
            return; // ok already known
        }
    }

    Group *group = getGroupForId(groupId);

    if (group)
    {
        updateEtag(group->etag);
    }

    updateEtag(lightNode->etag);
    updateEtag(gwConfigEtag);
    lightNode->enableRead(READ_SCENES); // force reading of scene membership

    GroupInfo groupInfo;
    groupInfo.id = groupId;
    lightNode->groups().push_back(groupInfo);
    markForPushUpdate(lightNode);
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
            if (i->id == groupId)
            {
                return true;
            }
        }
    }

    return false;
}

/*! Force reading attributes of all nodes in a group.
 */
void DeRestPluginPrivate::readAllInGroup(Group *group)
{
    DBG_Assert(group != 0);

    if (!group)
    {
        return;
    }

    std::vector<LightNode>::iterator i = nodes.begin();
    std::vector<LightNode>::iterator end = nodes.end();

    for (; i != end; ++i)
    {
        LightNode *lightNode = &(*i);
        if (isLightNodeInGroup(lightNode, group->address()))
        {
            // force reading attributes
            lightNode->setNextReadTime(QTime::currentTime().addMSecs(ReadAttributesLongerDelay));
            lightNode->enableRead(READ_ON_OFF | READ_COLOR | READ_LEVEL);
        }
    }
}

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
        updateEtag(group->etag);
        changed = true;
    }

    std::vector<LightNode>::iterator i = nodes.begin();
    std::vector<LightNode>::iterator end = nodes.end();

    for (; i != end; ++i)
    {
        LightNode *lightNode = &(*i);
        if (isLightNodeInGroup(lightNode, group->address()))
        {
            if (lightNode->isOn() != on)
            {
                lightNode->setIsOn(on);
                updateEtag(lightNode->etag);
                changed = true;
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

    task.req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
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
            if (i->state == Scene::StateDeleted)
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
            isLightNodeInGroup(lightNode, group->address()))
        {
            GroupInfo *groupInfo = getGroupInfo(lightNode, group->address());

            std::vector<uint8_t> &v = groupInfo->addScenes;

            if (std::find(v.begin(), v.end(), sceneId) == v.end())
            {
                groupInfo->addScenes.push_back(sceneId);
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
        (task.taskType != TaskStoreScene) &&
        (task.taskType != TaskRemoveScene) &&
        (task.taskType != TaskReadAttributes))
    {
        for (; i != end; ++i)
        {
            if (i->taskType ==  task.taskType)
            {
                if ((i->req.dstAddress() ==  task.req.dstAddress()) &&
                    (i->req.dstEndpoint() ==  task.req.dstEndpoint()) &&
                    (i->req.srcEndpoint() ==  task.req.srcEndpoint()) &&
                    (i->req.profileId() ==  task.req.profileId()) &&
                    (i->req.clusterId() ==  task.req.clusterId()) &&
                    (i->req.txOptions() ==  task.req.txOptions()) &&
                    (i->req.asdu().size() ==  task.req.asdu().size()))

                {
                    DBG_Printf(DBG_INFO, "Replace task in queue cluster 0x%04X with newer task of same type\n", task.req.clusterId());
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

/*! Fills cluster, lightNode and node fields of \p task based on the information in \p ind.
    \return true - on success
 */
bool DeRestPluginPrivate::obtainTaskCluster(TaskItem &task, const deCONZ::ApsDataIndication &ind)
{
    deCONZ::SimpleDescriptor *sd = 0;

    task.node = 0;
    task.lightNode = 0;
    task.cluster = 0;

    if (task.req.dstAddressMode() == deCONZ::ApsExtAddress)
    {
        quint64 extAddr = task.req.dstAddress().ext();

        task.lightNode = getLightNodeForAddress(extAddr);
        task.node = getNodeForAddress(extAddr);

        if (!task.node)
        {
            return false;
        }

        sd = task.node->getSimpleDescriptor(task.req.dstEndpoint());
        if (!sd)
        {
            return false;
        }

        task.cluster = sd->cluster(ind.clusterId(), deCONZ::ServerCluster);
    }
    else
    {
        // broadcast not supported
        return false;
    }

    if (!task.lightNode || !task.node || !task.cluster)
    {
        return false;
    }

    return true;
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
        DBG_Printf(DBG_INFO, "%d running tasks, wait\n", runningTasks.size());
        return;
    }

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

        // send only one request to a destination at a time
        std::list<TaskItem>::iterator j = runningTasks.begin();
        std::list<TaskItem>::iterator jend = runningTasks.end();

        bool ok = true;
        for (; j != jend; ++j)
        {
            if (i->req.dstAddress() == j->req.dstAddress())
            {
                ok = false;
                break;
            }
        }

        if (!ok) // destination already busy
        {
            if (i->req.dstAddressMode() == deCONZ::ApsExtAddress)
            {
                DBG_Printf(DBG_INFO_L2, "delay sending request %u to %s\n", i->req.id(), qPrintable(i->req.dstAddress().toStringExt()));
            }
            else if (i->req.dstAddressMode() == deCONZ::ApsGroupAddress)
            {
                DBG_Printf(DBG_INFO, "delay sending request %u to group 0x%04X\n", i->req.id(), i->req.dstAddress().group());
            }
        }
        else
        {
            // groupcast tasks
            if (i->req.dstAddressMode() == deCONZ::ApsGroupAddress)
            {
                Group *group = getGroupForId(i->req.dstAddress().group());

                if (group)
                {
                    QTime now = QTime::currentTime();
                    int diff = group->sendTime.msecsTo(now);

                    if (!group->sendTime.isValid() || (diff <= 0) || (diff > gwGroupSendDelay))
                    {
                        if (apsCtrl->apsdeDataRequest(i->req) == deCONZ::Success)
                        {
                            group->sendTime = now;
                            runningTasks.push_back(*i);
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
            // unicast tasks
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
                    int ret = apsCtrl->apsdeDataRequest(i->req);

                    if (ret == deCONZ::Success)
                    {
                        runningTasks.push_back(*i);
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
        DBG_Printf(DBG_INFO, "Node removed %s\n", qPrintable(event.node()->address().toStringExt()));
        LightNode *lightNode = getLightNodeForAddress(event.node()->address().ext());

        if (lightNode)
        {
            lightNode->setIsAvailable(false);
            updateEtag(lightNode->etag);
            updateEtag(gwConfigEtag);
        }
    }
        break;

    case deCONZ::NodeEvent::NodeAdded:
    {
        DBG_Printf(DBG_INFO, "Node added %s\n", qPrintable(event.node()->address().toStringExt()));
        addNode(event.node());
    }
        break;

    case deCONZ::NodeEvent::NodeZombieChanged:
    {
        DBG_Printf(DBG_INFO, "Node zombie state changed %s\n", qPrintable(event.node()->address().toStringExt()));
        nodeZombieStateChanged(event.node());
    }
        break;

    case deCONZ::NodeEvent::UpdatedSimpleDescriptor:
    case deCONZ::NodeEvent::UpdatedClusterData:
    {
        DBG_Printf(DBG_INFO, "Node data %s profileId: 0x%04X, clusterId: 0x%04X\n", qPrintable(event.node()->address().toStringExt()), event.profileId(), event.clusterId());
        updateLightNode(event);
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
    task.req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
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
            if (addTaskAddScene(task, i->id, i->addScenes[0]))
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

    if (!ind.srcAddress().hasExt())
    {
        return;
    }

    LightNode *lightNode = getLightNodeForAddress(ind.srcAddress().ext());

    if (!lightNode)
    {
        return;
    }

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

        for (uint i = 0; i < count; i++)
        {
            if (!stream.atEnd())
            {
                uint16_t groupId;
                stream >> groupId;

                DBG_Printf(DBG_INFO, "%s found group 0x%04X\n", qPrintable(lightNode->address().toStringExt()), groupId);

                foundGroup(groupId);
                foundGroupMembership(lightNode, groupId);
            }
        }
    }
}

/*! Handle packets related to the ZCL scene cluster.
    \param task the task which belongs to this response
    \param ind the APS level data indication containing the ZCL packet
    \param zclFrame the actual ZCL frame which holds the scene cluster reponse
 */
void DeRestPluginPrivate::handleSceneClusterIndication(TaskItem &task, const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    Q_UNUSED(ind);
    Q_UNUSED(task);

    if (zclFrame.isDefaultResponse())
    {
    }
    else if (zclFrame.commandId() == 0x06) // Get scene membership response
    {
        DBG_Assert(zclFrame.payload().size() >= 4);

        QDataStream stream(zclFrame.payload());
        stream.setByteOrder(QDataStream::LittleEndian);

        uint8_t status;
        uint8_t capacity;
        uint16_t groupId;
        uint8_t count;

        stream >> status;
        stream >> capacity;
        stream >> groupId;

        if (status == deCONZ::ZclSuccessStatus)
        {
            Group *group = getGroupForId(groupId);
            LightNode *lightNode = getLightNodeForAddress(ind.srcAddress().ext());

            DBG_Assert(group != 0);
            DBG_Assert(lightNode != 0);
            stream >> count;

            for (uint i = 0; i < count; i++)
            {
                if (!stream.atEnd())
                {
                    uint8_t sceneId;
                    stream >> sceneId;

                    DBG_Printf(DBG_INFO, "found scene 0x%02X for group 0x%04X\n", sceneId, groupId);

                    if (group && lightNode)
                    {
                        foundScene(lightNode, group, sceneId);
                    }
                }
            }
        }
    }
    else if (zclFrame.commandId() == 0x04) // Store scene response
    {
        DBG_Assert(zclFrame.payload().size() >= 4);

        QDataStream stream(zclFrame.payload());
        stream.setByteOrder(QDataStream::LittleEndian);

        uint8_t status;
        uint16_t groupId;
        uint8_t sceneId;

        stream >> status;
        stream >> groupId;
        stream >> sceneId;

        LightNode *lightNode = getLightNodeForAddress(ind.srcAddress().ext());

        if (lightNode)
        {
            GroupInfo *groupInfo = getGroupInfo(lightNode, groupId);

            if (groupInfo)
            {
                std::vector<uint8_t> &v = groupInfo->addScenes;
                std::vector<uint8_t>::iterator i = std::find(v.begin(), v.end(), sceneId);

                if (i != v.end())
                {
                    DBG_Printf(DBG_INFO, "Added/stored scene %u in node %s status 0x%02X\n", sceneId, qPrintable(lightNode->address().toStringExt()), status);
                    groupInfo->addScenes.erase(i);
                }
            }
        }
    }
    else if (zclFrame.commandId() == 0x02) // Remove scene response
    {
        DBG_Assert(zclFrame.payload().size() >= 4);

        QDataStream stream(zclFrame.payload());
        stream.setByteOrder(QDataStream::LittleEndian);

        uint8_t status;
        uint16_t groupId;
        uint8_t sceneId;

        stream >> status;
        stream >> groupId;
        stream >> sceneId;

        LightNode *lightNode = getLightNodeForAddress(ind.srcAddress().ext());

        if (lightNode)
        {
            GroupInfo *groupInfo = getGroupInfo(lightNode, groupId);

            if (groupInfo)
            {
                std::vector<uint8_t> &v = groupInfo->removeScenes;
                std::vector<uint8_t>::iterator i = std::find(v.begin(), v.end(), sceneId);

                if (i != v.end())
                {
                    DBG_Printf(DBG_INFO, "Removed scene %u from node %s status 0x%02X\n", sceneId, qPrintable(lightNode->address().toStringExt()), status);
                    groupInfo->removeScenes.erase(i);
                }
            }
        }
    }
}

/*! Handle the case than a node (re)joins the network.
    \param ind a ZDP DeviceAnnce_req
 */
void DeRestPluginPrivate::handleDeviceAnnceIndication(const deCONZ::ApsDataIndication &ind)
{
    if (!ind.srcAddress().hasExt())
    {
        return;
    }

    LightNode *lightNode = getLightNodeForAddress(ind.srcAddress().ext());

    if (!lightNode)
    {
        return;
    }

    if (!lightNode->isAvailable())
    {
        lightNode->setIsAvailable(true);
        updateEtag(gwConfigEtag);
    }

    DBG_Printf(DBG_INFO, "DeviceAnnce %s\n", qPrintable(lightNode->name()));

    // force reading attributes
    lightNode->setNextReadTime(QTime::currentTime().addMSecs(ReadAttributesLongDelay));
    lightNode->enableRead(READ_MODEL_ID |
                          READ_SWBUILD_ID |
                          READ_COLOR |
                          READ_LEVEL |
                          READ_ON_OFF |
                          READ_GROUPS |
                          READ_SCENES);
    lightNode->setSwBuildId(QString()); // might be changed due otau
    lightNode->setLastRead(idleTotalCounter);
    updateEtag(lightNode->etag);
}

/*! Mark node so current state will be pushed to all clients.
 */
void DeRestPluginPrivate::markForPushUpdate(LightNode *lightNode)
{
    std::list<LightNode*>::iterator i = std::find(broadCastUpdateNodes.begin(), broadCastUpdateNodes.end(), lightNode);

    if (i == broadCastUpdateNodes.end())
    {
        broadCastUpdateNodes.push_back(lightNode);
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
            if (isLightNodeInGroup(lightNode, task.req.dstAddress().group()))
            {
                pushNodes.push_back(lightNode);
            }
        }
    }
    else if (task.req.dstAddress().hasExt())
    {
        group = &dummyGroup; // never mind
        LightNode *lightNode = getLightNodeForAddress(task.req.dstAddress().ext());
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
    case TaskSetOnOff:
        updateEtag(group->etag);
        group->setIsOn(task.onOff);
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

    default:
        break;
    }

    for (; i != end; ++i)
    {
        LightNode *lightNode = *i;

        switch (task.taskType)
        {
        case TaskSetOnOff:
            updateEtag(lightNode->etag);
            lightNode->setIsOn(task.onOff);
            setAttributeOnOff(lightNode);
            break;

        case TaskSetLevel:
            if (task.level > 0)
            {
                lightNode->setIsOn(true);
            }
            else
            {
                lightNode->setIsOn(false);
            }
            updateEtag(lightNode->etag);
            lightNode->setLevel(task.level);
            setAttributeLevel(lightNode);
            setAttributeOnOff(lightNode);
            break;

        case TaskSetSat:
            updateEtag(lightNode->etag);
            lightNode->setSaturation(task.sat);
            setAttributeSaturation(lightNode);
            break;

        case TaskSetEnhancedHue:
            updateEtag(lightNode->etag);
            lightNode->setEnhancedHue(task.enhancedHue);
            setAttributeEnhancedHue(lightNode);
            break;

        case TaskSetHueAndSaturation:
            updateEtag(lightNode->etag);
            lightNode->setSaturation(task.sat);
            lightNode->setEnhancedHue(task.enhancedHue);
            setAttributeSaturation(lightNode);
            setAttributeEnhancedHue(lightNode);
            break;

        case TaskSetXyColor:
            updateEtag(lightNode->etag);
            lightNode->setColorXY(task.colorX, task.colorY);
            setAttributeColorXy(lightNode);
            break;

        default:
            break;
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

    deCONZ::ZclCluster *cl = getInCluster(lightNode->node(), lightNode->haEndpoint().endpoint(), ONOFF_CLUSTER_ID);

    if (cl && cl->attributes().size() > 0)
    {
        deCONZ::ZclAttribute &attr = cl->attributes()[0];

        DBG_Assert(attr.id() == 0x0000);

        if (attr.id() == 0x0000)
        {
            attr.setValue(lightNode->isOn());
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

    deCONZ::ZclCluster *cl = getInCluster(lightNode->node(), lightNode->haEndpoint().endpoint(), LEVEL_CLUSTER_ID);

    if (cl && cl->attributes().size() > 0)
    {
        deCONZ::ZclAttribute &attr = cl->attributes()[0];
        if (attr.id() == 0x0000)
        {
            attr.setValue((quint64)lightNode->level());
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

    deCONZ::ZclCluster *cl = getInCluster(lightNode->node(), lightNode->haEndpoint().endpoint(), COLOR_CLUSTER_ID);

    if (cl)
    {
        std::vector<deCONZ::ZclAttribute>::iterator i = cl->attributes().begin();
        std::vector<deCONZ::ZclAttribute>::iterator end = cl->attributes().end();

        for (; i != end; ++i)
        {
            if (i->id() == 0x0001) // Current saturation
            {
                i->setValue((quint64)lightNode->saturation());
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

    deCONZ::ZclCluster *cl = getInCluster(lightNode->node(), lightNode->haEndpoint().endpoint(), COLOR_CLUSTER_ID);

    if (cl)
    {
        std::vector<deCONZ::ZclAttribute>::iterator i = cl->attributes().begin();
        std::vector<deCONZ::ZclAttribute>::iterator end = cl->attributes().end();

        for (; i != end; ++i)
        {
            if (i->id() == 0x0003) // Current color x
            {
                i->setValue((quint64)lightNode->colorX());
                break;
            }
            else if (i->id() == 0x0004) // Current color x
            {
                i->setValue((quint64)lightNode->colorY());
                break;
            }
        }
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

    deCONZ::ZclCluster *cl = getInCluster(lightNode->node(), lightNode->haEndpoint().endpoint(), COLOR_CLUSTER_ID);

    if (cl)
    {
        std::vector<deCONZ::ZclAttribute>::iterator i = cl->attributes().begin();
        std::vector<deCONZ::ZclAttribute>::iterator end = cl->attributes().end();

        for (; i != end; ++i)
        {
            if (i->id() == 0x4000) // Enhanced hue
            {
                i->setValue((quint64)lightNode->enhancedHue());
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
    d->p = this;
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
            this, SLOT(checkReadTimerFired()));

    m_idleTimer->start(1000);

#if 1
    int dummyCount = deCONZ::appArgumentNumeric("--rest-dummy-lights", 0);

    for (int i = 0; i < dummyCount; i++)
    {
        // create dummy node
        LightNode lightNode;
        d->openDb();
        lightNode.setId(QString::number(d->getFreeLightId()));
        d->closeDb();
        lightNode.setNode(0);
        lightNode.setName(QString("Light %1").arg(lightNode.id()));
        lightNode.setSaturation(0);
        lightNode.setHue(180);
        lightNode.setIsAvailable(true);
        lightNode.address().setExt(0x002 + i);
        lightNode.address().setNwk(0x999 + i);

        deCONZ::SimpleDescriptor haEndpoint;

        haEndpoint.setDeviceId(DEV_ID_ZLL_EXTENDED_COLOR_LIGHT);

        deCONZ::ZclCluster clLevel(LEVEL_CLUSTER_ID, "Level");
        deCONZ::ZclAttribute attrLevel(0x0000, deCONZ::Zcl8BitUint, "Current level", deCONZ::ZclRead, true);
        clLevel.attributes().push_back(attrLevel);
        haEndpoint.inClusters().append(clLevel);

        deCONZ::ZclCluster clOnOff(ONOFF_CLUSTER_ID, "OnOff");
        deCONZ::ZclAttribute attrOnOff(0x0000, deCONZ::Zcl8BitUint, "OnOff", deCONZ::ZclRead, true);
        clOnOff.attributes().push_back(attrOnOff);
        haEndpoint.inClusters().append(clOnOff);

        lightNode.setHaEndpoint(haEndpoint);

        d->nodes.push_back(lightNode);
    }
#endif // dummy node
}

/*! The plugin deconstructor.
 */
DeRestPlugin::~DeRestPlugin()
{
    d = 0;
}

/*! Handle node events which are reported by main application.
    \param event - a deCONZ::NodeEvent value
    \param node - the node for the event
 */
void DeRestPlugin::nodeEvent(int event, const deCONZ::Node *node)
{
    (void)event;
    (void)node;
// TODO: remove
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

    if (d->idleLimit > 0)
    {
        d->idleLimit--;
    }

    if (d->idleLastActivity < IDLE_USER_LIMIT)
    {
        return;
    }

    if (d->idleLimit <= 0)
    {

        DBG_Printf(DBG_INFO, "Idle timer triggered\n");
        std::vector<LightNode>::iterator i = d->nodes.begin();
        std::vector<LightNode>::iterator end = d->nodes.end();

        for (; i != end; ++i)
        {
            if (i->lastRead() < (d->idleTotalCounter - IDLE_READ_LIMIT))
            {
                i->enableRead(READ_ON_OFF | READ_LEVEL | READ_COLOR | READ_GROUPS | READ_SCENES);

                if (i->modelId().isEmpty())
                {
                    i->enableRead(READ_MODEL_ID);
                }
                if (i->swBuildId().isEmpty())
                {
                    i->enableRead(READ_SWBUILD_ID);
                }
                i->setLastRead(d->idleTotalCounter);
                DBG_Printf(DBG_INFO, "Force read attributes for node %s\n", qPrintable(i->name()));
                break;
            }
        }

        startReadTimer(ReadAttributesDelay);

        d->idleLimit = IDLE_LIMIT;
    }
}

/*! Refresh all nodes by forcing the idle timer to trigger.
 */
void DeRestPlugin::refreshAll()
{
    std::vector<LightNode>::iterator i = d->nodes.begin();
    std::vector<LightNode>::iterator end = d->nodes.end();

    for (; i != end; ++i)
    {
        // force refresh on next idle timer timeout
        i->setLastRead(d->idleTotalCounter - (IDLE_READ_LIMIT + 1));
    }

    d->idleLimit = 0;
    d->idleLastActivity = IDLE_USER_LIMIT;
    d->runningTasks.clear();
    d->tasks.clear();
}

/*! Starts the read attributes timer with a given \p delay.
 */
void DeRestPlugin::startReadTimer(int delay)
{
    m_readAttributesTimer->stop();
    m_readAttributesTimer->start(delay);
}

/*! Stops the read attributes timer.
 */
void DeRestPlugin::stopReadTimer()
{
    m_readAttributesTimer->stop();
}

/*! Checks if attributes of any nodes shall be queried.
 */
void DeRestPlugin::checkReadTimerFired()
{
    std::vector<LightNode>::iterator i = d->nodes.begin();
    std::vector<LightNode>::iterator end = d->nodes.end();

    stopReadTimer();

    for (; i != end; ++i)
    {
        if (d->processReadAttributes(&(*i)))
        {
            // read next later
            startReadTimer(ReadAttributesDelay);
            d->processTasks();
            return;
        }
    }

    startReadTimer(ReadAttributesDelay);
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

/*! Main task handler will forward the event to deticated state handlers.
    \param event - the event which shall be handled
 */
void DeRestPlugin::taskHandler(DeRestPlugin::Event event)
{
    switch (m_state)
    {
    case StateOff:
        handleStateOff(event);
        break;

    case StateIdle:
        handleStateIdle(event);
        break;

    default:
        DBG_Printf(DBG_INFO, "web plugin: unknown state %d\n", m_state);
        m_state = StateOff;
        break;
    }
}

// TODO remove handleStateOff()
void DeRestPlugin::handleStateOff(DeRestPlugin::Event event)
{
    switch (event)
    {
    default:
        break;
    }
}

// TODO remove handleStateIdle()
void DeRestPlugin::handleStateIdle(DeRestPlugin::Event event)
{
    switch (event)
    {
    case TaskAdded:
    {
        d->processTasks();
    }
        break;

    default:
        break;
    }
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

        connect(m_w, SIGNAL(changeChannelClicked(int)),
                d, SLOT(changeChannel(int)));
    }

    return m_w;
}

/*! Checks if a request is addressed to this plugin.
    \param hdr - the http header of the request
    \return true - if the request could be processed
 */
bool DeRestPlugin::isHttpTarget(const QHttpRequestHeader &hdr)
{
    if (hdr.path().startsWith("/api/config"))
    {
        return true;
    }
    else if (hdr.path().startsWith("/api"))
    {
        QString path = hdr.path();
        int quest = path.indexOf('?');

        if (quest > 0)
        {
            path = path.mid(0, quest);
        }

        QStringList ls = path.split("/", QString::SkipEmptyParts);

        if (ls.size() > 2)
        {
            if ((ls[2] == "lights") ||
                (ls[2] == "groups") ||
                (ls[2] == "config") ||
                (ls[2] == "schedules") ||
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
    else if (hdr.path().startsWith("/description.xml"))
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

    if (hdrmod.path().startsWith("/api"))
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

    if (!stream.atEnd())
    {
        content = stream.readAll();
        DBG_Printf(DBG_HTTP, "\t%s\n", qPrintable(content));
    }

    connect(sock, SIGNAL(destroyed()),
            d, SLOT(clientSocketDestroyed()));

    QStringList path = hdrmod.path().split("/", QString::SkipEmptyParts);
    ApiRequest req(hdrmod, path, sock, content);
    ApiResponse rsp;

    rsp.httpStatus = HttpStatusNotFound;
    rsp.contentType = HttpContentHtml;

    int ret = REQ_NOT_HANDLED;

    // general response to a OPTIONS HTTP method
    if (req.hdr.method() == "OPTIONS")
    {
        stream << "HTTP/1.1 200 OK\r\n";
        stream << "Cache-Control: no-store, no-cache, must-revalidate, post-check=0, pre-check=0\r\n";
        stream << "Pragma: no-cache\r\n";
        stream << "Connection: close\r\n";
        stream << "Access-Control-Max-Age: 0\r\n";
        stream << "Access-Control-Allow-Origin: *\r\n";
        stream << "Access-Control-Allow-Credentials: true\r\n";
        stream << "Access-Control-Allow-Methods: POST, GET, OPTIONS, PUT, DELETE\r\n";
        stream << "Access-Control-Allow-Headers: Content-Type\r\n";
        stream << "Content-type: text/html\r\n";
        stream << "Content-Length: 0\r\n";
        stream << "\r\n";
        req.sock->flush();
        return 0;
    }

    if (path.size() > 2)
    {
        if (path[2] == "lights")
        {
            ret = d->handleLightsApi(req, rsp);
        }
        else if (path[2] == "groups")
        {
            ret = d->handleGroupsApi(req, rsp);
        }
        else if (path[2] == "schedules")
        {
            ret = d->handleSchedulesApi(req, rsp);
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
    else if (hdr.path().startsWith("/description.xml") && (hdr.method() == "GET"))
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
    stream << "Content-Type: " << rsp.contentType << "\r\n";
    stream << "Content-Length:" << QString::number(str.toUtf8().size()) << "\r\n";

    bool keepAlive = false;
    if (hdr.hasKey("Connection"))
    {
        if (hdr.value("Connection").toLower() == "keep-alive")
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

/*! Returns the name of this plugin.
 */
const char *DeRestPlugin::name()
{
    return "REST API Plugin";
}

Q_EXPORT_PLUGIN2(de_rest_plugin, DeRestPlugin)
