/*
 * Copyright (C) 2013 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifndef DE_WEB_PLUGIN_PRIVATE_H
#define DE_WEB_PLUGIN_PRIVATE_H
#include <QtGlobal>
#include <QObject>
#include <QTime>
#include <QTimer>
#include <QElapsedTimer>
#include <stdint.h>
#include "sqlite3.h"
#include <deconz.h>
#include "rest_node_base.h"
#include "light_node.h"
#include "group.h"
#include "group_info.h"
#include "scene.h"

/*! JSON generic error message codes */
#define ERR_UNAUTHORIZED_USER          1
#define ERR_INVALID_JSON               2
#define ERR_RESOURCE_NOT_AVAILABLE     3
#define ERR_METHOD_NOT_AVAILABLE       4
#define ERR_MISSING_PARAMETER          5
#define ERR_PARAMETER_NOT_AVAILABLE    6
#define ERR_INVALID_VALUE              7
#define ERR_PARAMETER_NOT_MODIFIEABLE  8
#define ERR_DUPLICATE_EXIST            100 // de extension
#define ERR_INTERNAL_ERROR             901

#define ERR_NOT_CONNECTED              950 // de extension
#define ERR_BRIDGE_BUSY                951 // de extension

#define ERR_LINK_BUTTON_NOT_PRESSED    101
#define ERR_DEVICE_OFF                 201
#define ERR_BRIDGE_GROUP_TABLE_FULL    301
#define ERR_DEVICE_GROUP_TABLE_FULL    302

#define IDLE_LIMIT 30
#define IDLE_READ_LIMIT 120
#define IDLE_USER_LIMIT 60

#define MAX_UNLOCK_GATEWAY_TIME 600
#define PERMIT_JOIN_SEND_INTERVAL (1000 * 160)

#define DE_PROFILE_ID           0xDE00

// HA lighting devices
#define DEV_ID_HA_ONOFF_LIGHT               0x0100 // On/Off light
#define DEV_ID_HA_DIMMABLE_LIGHT            0x0101 // Dimmable light
#define DEV_ID_HA_COLOR_DIMMABLE_LIGHT      0x0102 // Color dimmable light
// ZLL lighting devices
#define DEV_ID_ZLL_ONOFF_LIGHT              0x0000 // On/Off light
#define DEV_ID_ZLL_ONOFF_PLUGIN_UNIT        0x0010 // On/Off plugin unit
#define DEV_ID_ZLL_DIMMABLE_LIGHT           0x0100 // Dimmable light
#define DEV_ID_ZLL_DIMMABLE_PLUGIN_UNIT     0x0110 // Dimmable plugin unit
#define DEV_ID_ZLL_COLOR_LIGHT              0x0200 // Color light
#define DEV_ID_ZLL_EXTENDED_COLOR_LIGHT     0x0210 // Extended color light
#define DEV_ID_ZLL_COLOR_TEMPERATURE_LIGHT  0x0220 // Color temperature light
// ZLL controller devices
#define DEV_ID_ZLL_COLOR_CONTROLLER         0x0800 // Color controller
#define DEV_ID_ZLL_COLOR_SCENE_CONTROLLER   0x0810 // Color scene controller
#define DEV_ID_ZLL_NON_COLOR_CONTROLLER     0x0820 // Non color controller
#define DEV_ID_ZLL_NON_COLOR_SCENE_CONTROLLER 0x0830 // Non color scene controller
#define DEV_ID_ZLL_CONTROL_BRIDGE           0x0840 // Control bridge
#define DEV_ID_ZLL_ONOFF_SENSOR             0x0850 // On/Off sensor

#define DEFAULT_TRANSITION_TIME 4 // 400ms
#define MAX_ENHANCED_HUE 65536

#define BASIC_CLUSTER_ID 0x0000
#define GROUP_CLUSTER_ID 0x0004
#define SCENE_CLUSTER_ID 0x0005
#define ONOFF_CLUSTER_ID 0x0006
#define LEVEL_CLUSTER_ID 0x0008
#define COLOR_CLUSTER_ID 0x0300

// manufacturer codes
#define VENDOR_DDEL     0x1014
#define VENDOR_PHILIPS  0x100B

#define ANNOUNCE_INTERVAL 10 // minutes default announce interval

#define MAX_GROUP_SEND_DELAY 5000 // ms between to requests to the same group
#define GROUP_SEND_DELAY 500 // default ms between to requests to the same group

// string lengths
#define MAX_GROUP_NAME_LENGTH 32
#define MAX_SCENE_NAME_LENGTH 32

// REST API return codes
#define REQ_READY_SEND   0
#define REQ_DONE         2
#define REQ_NOT_HANDLED -1

// Special application return codes
#define APP_RET_UPDATE 40

// schedules
#define SCHEDULE_CHECK_PERIOD 1000

// save database items
#define DB_LIGHTS      0x00000001
#define DB_GROUPS      0x00000002
#define DB_AUTH        0x00000004
#define DB_CONFIG      0x00000008
#define DB_SCENES      0x00000010
#define DB_SCHEDULES   0x00000020

#define DB_LONG_SAVE_DELAY  (5 * 60 * 1000) // 5 minutes
#define DB_SHORT_SAVE_DELAY (5 *  1 * 1000) // 5 seconds

// internet discovery

// HTTP status codes
extern const char *HttpStatusOk;
extern const char *HttpStatusAccepted;
extern const char *HttpStatusNotModified;
extern const char *HttpStatusUnauthorized;
extern const char *HttpStatusBadRequest;
extern const char *HttpStatusForbidden;
extern const char *HttpStatusNotFound;
extern const char *HttpStatusNotImplemented;
extern const char *HttpStatusServiceUnavailable;
extern const char *HttpContentHtml;
extern const char *HttpContentCss;
extern const char *HttpContentJson;
extern const char *HttpContentJS;
extern const char *HttpContentPNG;
extern const char *HttpContentJPG;
extern const char *HttpContentSVG;

// Forward declarations
class QUdpSocket;
class QTcpSocket;
class DeRestPlugin;
class QNetworkReply;
class QNetworkAccessManager;

struct Schedule
{
    Schedule()
    {
    }

    /*! Numeric identifier as string. */
    QString id;
    /*! Name length 0..32, if 0 default name "schedule" will be used. (Optional) */
    QString name;
    /*! Description length 0..64, default is empty string. (Optional) */
    QString description;
    /*! Command a JSON object with length 0..90. (Required) */
    QString command;
    /*! Time is given in ISO 8601:2004 format: YYYY-MM-DDTHH:mm:ss. (Required) */
    QString time;
    /*! Same as time but as qt object */
    QDateTime datetime;
};

enum TaskType
{
    TaskGetHue,
    TaskSetHue,
    TaskSetEnhancedHue,
    TaskSetHueAndSaturation,
    TaskSetXyColor,
    TaskGetColor,
    TaskGetSat,
    TaskSetSat,
    TaskGetLevel,
    TaskSetLevel,
    TaskSetOnOff,
    TaskGetOnOff,
    TaskReadAttributes,
    TaskGetGroupMembership,
    TaskGetSceneMembership,
    TaskStoreScene,
    TaskCallScene,
    TaskRemoveScene,
    TaskAddToGroup,
    TaskRemoveFromGroup
};

struct TaskItem
{
    TaskItem()
    {
        autoMode = false;
        onOff = false;
        client = 0;
        node = 0;
        lightNode = 0;
        cluster = 0;
        transitionTime = DEFAULT_TRANSITION_TIME;
    }

    TaskType taskType;
    deCONZ::ApsDataRequest req;
    deCONZ::ZclFrame zclFrame;
    uint8_t zclSeq;
    bool confirmed;
    bool onOff;
    qreal hueReal;
    uint8_t hue;
    uint8_t sat;
    uint8_t level;
    uint16_t enhancedHue;
    uint16_t colorX;
    uint16_t colorY;
    uint16_t groupId;
    QString etag;
    uint16_t transitionTime;
    QTcpSocket *client;

    bool autoMode; // true then this is a automode task
    deCONZ::Node *node;
    LightNode *lightNode;
    deCONZ::ZclCluster *cluster;
};

/*! \class ApiAuth

    Helper to combine serval authentification parameters.
 */
class ApiAuth
{
public:
    ApiAuth();

    QString apikey; // also called username (10..32 chars)
    QString devicetype;
    QDateTime createDate;
    QDateTime lastUseDate;
    QString useragent;
};

enum ApiVersion
{
    ApiVersion_1,      //!< common version 1.0
    ApiVersion_1_DDEL  //!< version 1.0, "Accept: application/vnd.ddel.v1"
};

/*! \class ApiRequest

    Helper to simplify HTTP REST request handling.
 */
class ApiRequest
{
public:
    ApiRequest(const QHttpRequestHeader &h, const QStringList &p, QTcpSocket *s, const QString &c);
    QString apikey() const;
    ApiVersion apiVersion() const { return version; }

    const QHttpRequestHeader &hdr;
    const QStringList &path;
    QTcpSocket *sock;
    QString content;
    ApiVersion version;
};

/*! \class ApiResponse

    Helper to simplify HTTP REST request handling.
 */
struct ApiResponse
{
    QString etag;
    const char *httpStatus;
    const char *contentType;
    QList<QPair<QString, QString> > hdrFields; // extra header fields
    QVariantMap map; // json content
    QVariantList list; // json content
    QString str; // json string
};

class TcpClient
{
public:
    int closeTimeout; // close socket in n seconds
    QTcpSocket *sock;
};

/*! \class DeWebPluginPrivate

    Pimpl of DeWebPlugin.
 */
class DeRestPluginPrivate : public QObject
{
    Q_OBJECT

public:
    DeRestPluginPrivate(QObject *parent = 0);
    ~DeRestPluginPrivate();

    // REST API authentification
    void initAuthentification();
    bool allowedToCreateApikey(const ApiRequest &req);
    bool checkApikeyAuthentification(const ApiRequest &req, ApiResponse &rsp);
    QString encryptString(const QString &str);

    // REST API configuration
    int handleConfigurationApi(const ApiRequest &req, ApiResponse &rsp);
    int createUser(const ApiRequest &req, ApiResponse &rsp);
    int getFullState(const ApiRequest &req, ApiResponse &rsp);
    int getConfig(const ApiRequest &req, ApiResponse &rsp);
    int modifyConfig(const ApiRequest &req, ApiResponse &rsp);
    int updateSoftware(const ApiRequest &req, ApiResponse &rsp);
    int changePassword(const ApiRequest &req, ApiResponse &rsp);
    int deletePassword(const ApiRequest &req, ApiResponse &rsp);

    void configToMap(QVariantMap &map);

    // REST API lights
    int handleLightsApi(ApiRequest &req, ApiResponse &rsp);
    int getAllLights(const ApiRequest &req, ApiResponse &rsp);
    int searchLights(const ApiRequest &req, ApiResponse &rsp);
    int getNewLights(const ApiRequest &req, ApiResponse &rsp);
    int getLightState(const ApiRequest &req, ApiResponse &rsp);
    int setLightState(const ApiRequest &req, ApiResponse &rsp);
    int renameLight(const ApiRequest &req, ApiResponse &rsp);

    bool lightToMap(const ApiRequest &req, const LightNode *webNode, QVariantMap &map);

    // REST API groups
    int handleGroupsApi(ApiRequest &req, ApiResponse &rsp);
    int getAllGroups(const ApiRequest &req, ApiResponse &rsp);
    int createGroup(const ApiRequest &req, ApiResponse &rsp);
    int getGroupAttributes(const ApiRequest &req, ApiResponse &rsp);
    int setGroupAttributes(const ApiRequest &req, ApiResponse &rsp);
    int setGroupState(const ApiRequest &req, ApiResponse &rsp);
    int deleteGroup(const ApiRequest &req, ApiResponse &rsp);

    // REST API groups > scenes
    int createScene(const ApiRequest &req, ApiResponse &rsp);
    int getAllScenes(const ApiRequest &req, ApiResponse &rsp);
    int getSceneAttributes(const ApiRequest &req, ApiResponse &rsp);
    int setSceneAttributes(const ApiRequest &req, ApiResponse &rsp);
    int storeScene(const ApiRequest &req, ApiResponse &rsp);
    int recallScene(const ApiRequest &req, ApiResponse &rsp);
    int deleteScene(const ApiRequest &req, ApiResponse &rsp);

    bool groupToMap(const Group *group, QVariantMap &map);

    // REST API schedules
    void initSchedules();
    int handleSchedulesApi(ApiRequest &req, ApiResponse &rsp);
    int getAllSchedules(const ApiRequest &req, ApiResponse &rsp);
    int createSchedule(const ApiRequest &req, ApiResponse &rsp);
    int getScheduleAttributes(const ApiRequest &req, ApiResponse &rsp);
    int setScheduleAttributes(const ApiRequest &req, ApiResponse &rsp);
    int deleteSchedule(const ApiRequest &req, ApiResponse &rsp);

    // REST API touchlink
    void initTouchlinkApi();
    int handleTouchlinkApi(ApiRequest &req, ApiResponse &rsp);
    int touchlinkScan(ApiRequest &req, ApiResponse &rsp);
    int getTouchlinkScanResults(ApiRequest &req, ApiResponse &rsp);
    int identifyLight(ApiRequest &req, ApiResponse &rsp);
    int resetLight(ApiRequest &req, ApiResponse &rsp);

    // REST API common
    QVariantMap errorToMap(int id, const QString &ressource, const QString &description);

    // UPNP discovery
    void initUpnpDiscovery();
    // Internet discovery
    void initInternetDicovery();
    bool setInternetDiscoveryInterval(int minutes);
    // Permit join
    void initPermitJoin();
    bool setPermitJoinDuration(uint8_t duration);

    // Otau
    void initOtau();
    void otauDataIndication(const deCONZ::ApsDataIndication &ind);
    void otauSendNotify(LightNode *node);
    bool isOtauBusy();

public Q_SLOTS:
    void announceUpnp();
    void upnpReadyRead();
    void changeChannel(int channel);
    void apsdeDataIndication(const deCONZ::ApsDataIndication &ind);
    void apsdeDataConfirm(const deCONZ::ApsDataConfirm &conf);
    void processTasks();
    void processGroupTasks();
    void nodeEvent(const deCONZ::NodeEvent &event);
    void internetDiscoveryTimerFired();
    void internetDiscoveryFinishedRequest(QNetworkReply *reply);
    void scheduleTimerFired();
    void permitJoinTimerFired();
    void otauTimerFired();
    void updateSoftwareTimerFired();
    void lockGatewayTimerFired();
    void openClientTimerFired();
    void clientSocketDestroyed();
    void saveDatabaseTimerFired();
    void userActivity();

    // touchlink
    void touchlinkDisconnectNetwork();
    void checkTouchlinkNetworkDisconnected();
    void startTouchlinkMode(uint8_t channel);
    void startTouchlinkModeConfirm(deCONZ::TouchlinkStatus status);
    void sendTouchlinkConfirm(deCONZ::TouchlinkStatus status);
    void sendTouchlinkScanRequest();
    void sendTouchlinkIdentifyRequest();
    void sendTouchlinkResetRequest();
    void touchlinkScanTimeout();
    void interpanDataIndication(const QByteArray &data);
    void touchlinkStartReconnectNetwork(int delay);
    void touchlinkReconnectNetwork();
    bool isTouchlinkActive();

public:
    void checkRfConnectState();
    bool isInNetwork();
    void generateGatewayUuid();
    void updateEtag(QString &etag);
    qint64 getUptime();
    LightNode *addNode(const deCONZ::Node *node);
    LightNode *nodeZombieStateChanged(const deCONZ::Node *node);
    LightNode *updateLightNode(const deCONZ::NodeEvent &event);
    LightNode *getLightNodeForAddress(uint64_t extAddr);
    LightNode *getLightNodeForId(const QString &id);
    Group *getGroupForName(const QString &name);
    Group *getGroupForId(uint16_t id);
    Group *getGroupForId(const QString &id);
    GroupInfo *getGroupInfo(LightNode *lightNode, uint16_t id);
    GroupInfo *createGroupInfo(LightNode *lightNode, uint16_t id);
    deCONZ::Node *getNodeForAddress(uint64_t extAddr);
    deCONZ::ZclCluster *getInCluster(deCONZ::Node *node, uint8_t endpoint, uint16_t clusterId);
    uint8_t getSrcEndpoint(LightNode *lightNode, const deCONZ::ApsDataRequest &req);
    bool processReadAttributes(LightNode *lightNode);
    bool readAttributes(LightNode *lightNode, const deCONZ::SimpleDescriptor *sd, uint16_t clusterId, const std::vector<uint16_t> &attributes);
    bool readGroupMembership(LightNode *lightNode, const std::vector<uint16_t> &groups);
    void foundGroupMembership(LightNode *lightNode, uint16_t groupId);
    void foundGroup(uint16_t groupId);
    bool isLightNodeInGroup(LightNode *lightNode, uint16_t groupId);
    void readAllInGroup(Group *group);
    void setAttributeOnOffGroup(Group *group, uint8_t onOff);
    bool readSceneMembership(LightNode *lightNode, Group *group);
    void foundScene(LightNode *lightNode, Group *group, uint8_t sceneId);
    void setSceneName(Group *group, uint8_t sceneId, const QString &name);
    bool storeScene(Group *group, uint8_t sceneId);
    bool removeScene(Group *group, uint8_t sceneId);
    bool callScene(Group *group, uint8_t sceneId);

    bool pushState(QString json, QTcpSocket *sock);

    void pushClientForClose(QTcpSocket *sock, int closeTimeout);

    // Task interface
    bool addTask(const TaskItem &task);
    bool addTaskSetOnOff(TaskItem &task, bool on);
    bool addTaskSetBrightness(TaskItem &task, uint8_t bri, bool withOnOff);
    bool addTaskSetEnhancedHue(TaskItem &task, uint16_t hue);
    bool addTaskSetSaturation(TaskItem &task, uint8_t sat);
    bool addTaskSetHueAndSaturation(TaskItem &task, uint8_t hue, uint8_t sat);
    bool addTaskSetXyColorAsHueAndSaturation(TaskItem &task, double x, double y);
    bool addTaskSetXyColor(TaskItem &task, double x, double y);
    bool addTaskAddToGroup(TaskItem &task, uint16_t groupId);
    bool addTaskRemoveFromGroup(TaskItem &task, uint16_t groupId);
    bool addTaskAddScene(TaskItem &task, uint16_t groupId, uint8_t sceneId);
    bool addTaskRemoveScene(TaskItem &task, uint16_t groupId, uint8_t sceneId);
    bool obtainTaskCluster(TaskItem &task, const deCONZ::ApsDataIndication &ind);
    void handleGroupClusterIndication(TaskItem &task, const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame);
    void handleSceneClusterIndication(TaskItem &task, const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame);
    void handleDeviceAnnceIndication(const deCONZ::ApsDataIndication &ind);
    void broadCastNodeUpdate(LightNode *webNode);
    void markForPushUpdate(LightNode *lightNode);
    void taskToLocalData(const TaskItem &task);

    // Modify node attributes
    void setAttributeOnOff(LightNode *lightNode);
    void setAttributeLevel(LightNode *lightNode);
    void setAttributeEnhancedHue(LightNode *lightNode);
    void setAttributeSaturation(LightNode *lightNode);
    void setAttributeColorXy(LightNode *lightNode);

    // Database interface
    void initDb();
    void openDb();
    void readDb();
    void loadAuthFromDb();
    void loadConfigFromDb();
    void loadAllGroupsFromDb();
    void loadLightNodeFromDb(LightNode *lightNode);
    void loadGroupFromDb(Group *group);
    void loadSceneFromDb(Scene *scene);
    int getFreeLightId();
    void saveDb();
    void closeDb();
    void queSaveDb(int items, int msec);

    sqlite3 *db;
    int saveDatabaseItems;
    QString sqliteDatabaseName;
    std::vector<int> lightIds;
    QTimer *databaseTimer;

    // authentification
    std::vector<ApiAuth> apiAuths;
    QString gwAdminUserName;
    QString gwAdminPasswordHash;

    // configuration
    bool gwLinkButton;
    bool gwRfConnectedExpected;  // the state which should be hold
    bool gwRfConnected;  // to detect changes
    bool gwOtauActive;
    int gwAnnounceInterval; // used by internet discovery [minutes]
    QString gwAnnounceUrl;
    uint8_t gwPermitJoinDuration; // global permit join state (last set)
    QString gwIpAddress;
    uint16_t gwPort;
    QString gwName;
    QString gwUuid;
    QString gwUpdateVersion;
    int gwGroupSendDelay;
    QVariantMap gwConfig;
    QString gwConfigEtag;

    // upnp
    QByteArray descriptionXml;

    // gateway lock (link button)
    QTimer *lockGatewayTimer;

    // permit join
    // used by searchLights()
    QTimer *permitJoinTimer;
    QTime permitJoinLastSendTime;

    // schedules
    QTimer *scheduleTimer;
    std::vector<Schedule> schedules;

    // internet discovery
    QNetworkAccessManager *inetDiscoveryManager;
    QTimer *inetDiscoveryTimer;
    QNetworkReply *inetDiscoveryResponse;

    // otau
    QTimer *otauTimer;
    int otauIdleTicks;
    int otauBusyTicks;
    uint otauNotifyIter; // iterator over nodes
    int otauNotifyDelay;

    // touchlink

    // touchlink state machine
    enum TouchlinkState
    {
        // general
        TL_Idle,
        TL_DisconnectingNetwork,
        TL_StartingInterpanMode,
        TL_StoppingInterpanMode,
        TL_ReconnectNetwork,
        // scanning
        TL_SendingScanRequest,
        TL_WaitScanResponses,
        // identify
        TL_SendingIdentifyRequest,
        // reset
        TL_SendingResetRequest
    };

    enum TouchlinkAction
    {
        TouchlinkScan,
        TouchlinkIdentify,
        TouchlinkReset
    };

    struct ScanResponse
    {
        QString id;
        deCONZ::Address address;
        bool factoryNew;
        uint8_t channel;
        uint16_t panid;
        uint32_t transactionId;
    };

    int touchlinkNetworkDisconnectAttempts; // disconnect attemps before touchlink
    int touchlinkNetworkReconnectAttempts; // reconnect attemps after touchlink
    bool touchlinkNetworkConnectedBefore;
    uint8_t touchlinkChannel;
    uint8_t touchlinkScanCount;
    deCONZ::TouchlinkController *touchlinkCtrl;
    TouchlinkAction touchlinkAction;
    TouchlinkState touchlinkState;
    deCONZ::TouchlinkRequest touchlinkReq;
    QTimer *touchlinkTimer;
    QDateTime touchlinkScanTime;
    std::vector<ScanResponse> touchlinkScanResponses;
    ScanResponse touchlinkDevice; // device of interrest (identify, reset, ...)

    // general
    deCONZ::ApsController *apsCtrl;
    uint groupTaskNodeIter; // Iterates through nodes array
    int idleTotalCounter; // sys timer
    int idleLimit;
    int idleLastActivity; // delta in seconds
    std::vector<Group> groups;
    std::vector<LightNode> nodes;
    std::list<LightNode*> broadCastUpdateNodes;
    std::list<TaskItem> tasks;
    std::list<TaskItem> runningTasks;
    QTimer *taskTimer;
    QTimer *groupTaskTimer;
    uint8_t zclSeq;
    std::list<QTcpSocket*> eventListeners;
    QUdpSocket *udpSock;
    QUdpSocket *udpSockOut;

    // TCP connection watcher
    QTimer *openClientTimer;
    std::list<TcpClient> openClients;

    // will be set at startup to calculate the uptime
    QElapsedTimer starttimeRef;

    DeRestPlugin *p; // public interface
};

#endif // DE_WEB_PLUGIN_PRIVATE_H
