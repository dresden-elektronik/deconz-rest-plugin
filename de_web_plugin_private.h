/*
 * Copyright (c) 2016 dresden elektronik ingenieurtechnik gmbh.
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
#include <queue>
#if QT_VERSION < 0x050000
#include <QHttpRequestHeader>
#endif
#include "sqlite3.h"
#include <deconz.h>
#include "rest_node_base.h"
#include "light_node.h"
#include "group.h"
#include "group_info.h"
#include "scene.h"
#include "sensor.h"
#include "rule.h"
#include "bindings.h"
#include <math.h>

/*! JSON generic error message codes */
#define ERR_UNAUTHORIZED_USER          1
#define ERR_INVALID_JSON               2
#define ERR_RESOURCE_NOT_AVAILABLE     3
#define ERR_METHOD_NOT_AVAILABLE       4
#define ERR_MISSING_PARAMETER          5
#define ERR_PARAMETER_NOT_AVAILABLE    6
#define ERR_INVALID_VALUE              7
#define ERR_PARAMETER_NOT_MODIFIEABLE  8
#define ERR_TOO_MANY_ITEMS             11
#define ERR_DUPLICATE_EXIST            100 // de extension
#define ERR_NOT_ALLOWED_SENSOR_TYPE    501
#define ERR_SENSOR_LIST_FULL           502
#define ERR_RULE_ENGINE_FULL           601
#define ERR_CONDITION_ERROR            607
#define ERR_ACTION_ERROR               608
#define ERR_INTERNAL_ERROR             901

#define ERR_NOT_CONNECTED              950 // de extension
#define ERR_BRIDGE_BUSY                951 // de extension

#define ERR_LINK_BUTTON_NOT_PRESSED    101
#define ERR_DEVICE_OFF                 201
#define ERR_BRIDGE_GROUP_TABLE_FULL    301
#define ERR_DEVICE_GROUP_TABLE_FULL    302

#define ERR_DEVICE_SCENES_TABLE_FULL   402 // de extension

#define IDLE_LIMIT 30
#define IDLE_READ_LIMIT 120
#define IDLE_USER_LIMIT 20
#define IDLE_ATTR_REPORT_BIND_LIMIT 240

#define MAX_UNLOCK_GATEWAY_TIME 600
#define PERMIT_JOIN_SEND_INTERVAL (1000 * 160)

#define DE_OTAU_ENDPOINT             0x50
#define DE_PROFILE_ID              0xDE00
#define ATMEL_WSNDEMO_PROFILE_ID   0x0001

// Generic devices
#define DEV_ID_ONOFF_SWITCH                 0x0000 // On/Off switch
#define DEV_ID_LEVEL_CONTROL_SWITCH         0x0001 // Level control switch
#define DEV_ID_ONOFF_OUTPUT                 0x0002 // On/Off output
#define DEV_ID_RANGE_EXTENDER               0x0008 // Range extender
#define DEV_ID_MAINS_POWER_OUTLET           0x0009 // Mains power outlet
// HA lighting devices
#define DEV_ID_HA_ONOFF_LIGHT               0x0100 // On/Off light
#define DEV_ID_HA_DIMMABLE_LIGHT            0x0101 // Dimmable light
#define DEV_ID_HA_COLOR_DIMMABLE_LIGHT      0x0102 // Color dimmable light
#define DEV_ID_HA_ONOFF_LIGHT_SWITCH        0x0103 // On/Off light switch
#define DEV_ID_HA_DIMMER_SWITCH             0x0104 // Dimmer switch
#define DEV_ID_HA_LIGHT_SENSOR              0x0106 // Light sensor
#define DEV_ID_HA_OCCUPANCY_SENSOR          0x0107 // Occupancy sensor

// Smart Energy devices
#define DEV_ID_SE_METERING_DEVICE           0x0501 // Smart Energy metering device

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
#define MAX_ENHANCED_HUE 65535

#define BASIC_CLUSTER_ID 0x0000
#define IDENTIFY_CLUSTER_ID 0x0003
#define GROUP_CLUSTER_ID 0x0004
#define SCENE_CLUSTER_ID 0x0005
#define ONOFF_CLUSTER_ID 0x0006
#define ONOFF_SWITCH_CONFIGURATION_CLUSTER_ID 0x0007
#define LEVEL_CLUSTER_ID 0x0008
#define COLOR_CLUSTER_ID 0x0300
#define ILLUMINANCE_MEASUREMENT_CLUSTER_ID   0x0400
#define ILLUMINANCE_LEVEL_SENSING_CLUSTER_ID 0x0401
#define OCCUPANCY_SENSING_CLUSTER_ID         0x0406
#define OTAU_CLUSTER_ID  0x0019
#define GREEN_POWER_CLUSTER_ID 0x0021
#define GREEN_POWER_ENDPOINT 0xf2
#define COMMISSIONING_CLUSTER_ID  0x1000

#define ONOFF_COMMAND_OFF     0x00
#define ONOFF_COMMAND_ON      0x01
#define ONOFF_COMMAND_TOGGLE  0x02
#define ONOFF_COMMAND_ON_WITH_TIMED_OFF  0x42

// read flags
#define READ_MODEL_ID          (1 << 0)
#define READ_SWBUILD_ID        (1 << 1)
#define READ_ON_OFF            (1 << 2)
#define READ_LEVEL             (1 << 3)
#define READ_COLOR             (1 << 4)
#define READ_GROUPS            (1 << 5)
#define READ_SCENES            (1 << 6)
#define READ_SCENE_DETAILS     (1 << 7)
#define READ_VENDOR_NAME       (1 << 8)
#define READ_BINDING_TABLE     (1 << 9)
#define READ_OCCUPANCY_CONFIG  (1 << 10)
#define READ_GROUP_IDENTIFIERS (1 << 12)

// write flags
#define WRITE_OCCUPANCY_CONFIG  (1 << 11)

// manufacturer codes
#define VENDOR_ATMEL    0x1014
#define VENDOR_DDEL     0x1135
#define VENDOR_PHILIPS  0x100B
#define VENDOR_OSRAM_STACK  0xBBAA
#define VENDOR_OSRAM    0x110C
#define VENDOR_UBISYS   0x10F2
#define VENDOR_BUSCH_JAEGER 0x112E
#define VENDOR_BEGA 0x1105

#define ANNOUNCE_INTERVAL 10 // minutes default announce interval

#define MAX_GROUP_SEND_DELAY 5000 // ms between to requests to the same group
#define GROUP_SEND_DELAY 500 // default ms between to requests to the same group

// string lengths
#define MAX_GROUP_NAME_LENGTH 32
#define MAX_SCENE_NAME_LENGTH 32
#define MAX_RULE_NAME_LENGTH 32
#define MAX_SENSOR_NAME_LENGTH 32

// REST API return codes
#define REQ_READY_SEND   0
#define REQ_DONE         2
#define REQ_NOT_HANDLED -1

// Special application return codes
#define APP_RET_UPDATE        40
#define APP_RET_RESTART_APP   41
#define APP_RET_UPDATE_BETA   42
#define APP_RET_RESTART_SYS   43
#define APP_RET_SHUTDOWN_SYS  44
#define APP_RET_UPDATE_ALPHA  45
#define APP_RET_UPDATE_FW     46

// Firmware version related (32-bit field)
#define FW_PLATFORM_MASK          0x0000FF00UL
#define FW_PLATFORM_DERFUSB23E0X  0x00000300UL
#define FW_PLATFORM_RPI           0x00000500UL

// schedules
#define SCHEDULE_CHECK_PERIOD 1000

// save database items
#define DB_LIGHTS      0x00000001
#define DB_GROUPS      0x00000002
#define DB_AUTH        0x00000004
#define DB_CONFIG      0x00000008
#define DB_SCENES      0x00000010
#define DB_SCHEDULES   0x00000020
#define DB_RULES       0x00000040
#define DB_SENSORS     0x00000080

#define DB_LONG_SAVE_DELAY  (15 * 60 * 1000) // 15 minutes
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
class QProcess;

struct Schedule
{
    enum Week
    {
        Monday    = 0x01,
        Tuesday   = 0x02,
        Wednesday = 0x04,
        Thursday  = 0x08,
        Friday    = 0x10,
        Saturday  = 0x20,
        Sunday    = 0x40,
    };

    enum Type
    {
        TypeInvalid,
        TypeAbsoluteTime,
        TypeRecurringTime,
        TypeTimer
    };

    enum State
    {
        StateNormal,
        StateDeleted
    };

    Schedule() :
        type(TypeInvalid),
        state(StateNormal),
        status("enabled"),
        autodelete(true),
        weekBitmap(0),
        recurring(0),
        timeout(0),
        currentTimeout(0)
    {
    }

    Type type;
    State state;
    /*! Numeric identifier as string. */
    QString id;
    /*! etag of Schedule. */
    QString etag;
    /*! Name length 0..32, if 0 default name "schedule" will be used. (Optional) */
    QString name;
    /*! Description length 0..64, default is empty string. (Optional) */
    QString description;
    /*! Command a JSON object with length 0..90. (Required) */
    QString command;
    /*! Time is given in ISO 8601:2004 format: YYYY-MM-DDTHH:mm:ss. (Required) */
    QString time;
    /*! UTC time that the timer was started. Only provided for timers. */
    QString starttime;
    /*! status of schedule (enabled or disabled). */
    QString status;
    /*! If set to true, the schedule will be removed automatically if expired, if set to false it will be disabled. */
    bool autodelete;
    /*! Same as time but as qt object. */
    QDateTime datetime;
    /*! Date time of last schedule activation. */
    QDateTime lastTriggerDatetime;
    /*! Whole JSON schedule as received from API as string. */
    QString jsonString;
    /*! Whole JSON schedule as received from API as map. */
    QVariantMap jsonMap;
    /*! Bitmap for recurring schedule. */
    quint8 weekBitmap;
    /*! R[nn], the recurring part, 0 means forever. */
    uint recurring;
    /*! Timeout in seconds. */
    int timeout;
    /*! Current timeout counting down to ::timeout. */
    int currentTimeout;
};

enum TaskType
{
    TaskIdentify,
    TaskGetHue,
    TaskSetHue,
    TaskSetEnhancedHue,
    TaskSetHueAndSaturation,
    TaskSetXyColor,
    TaskSetColorTemperature,
    TaskGetColor,
    TaskGetSat,
    TaskSetSat,
    TaskGetLevel,
    TaskSetLevel,
    TaskStopLevel,
    TaskSendOnOffToggle,
    TaskMoveLevel,
    TaskGetOnOff,
    TaskSetColorLoop,
    TaskGetColorLoop,
    TaskReadAttributes,
    TaskWriteAttribute,
    TaskGetGroupMembership,
    TaskGetGroupIdentifiers,
    TaskGetSceneMembership,
    TaskStoreScene,
    TaskCallScene,
    TaskViewScene,
    TaskAddScene,
    TaskRemoveScene,
    TaskRemoveAllScenes,
    TaskAddToGroup,
    TaskRemoveFromGroup,
    TaskViewGroup
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
        colorX = 0;
        colorY = 0;
        colorTemperature = 0;
        transitionTime = DEFAULT_TRANSITION_TIME;
    }

    TaskType taskType;
    deCONZ::ApsDataRequest req;
    deCONZ::ZclFrame zclFrame;
    uint8_t zclSeq;
    bool confirmed;
    bool onOff;
    bool colorLoop;
    qreal hueReal;
    uint16_t identifyTime;
    uint8_t hue;
    uint8_t sat;
    uint8_t level;
    uint16_t enhancedHue;
    uint16_t colorX;
    uint16_t colorY;
    uint16_t colorTemperature;
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
    enum State
    {
        StateNormal,
        StateDeleted
    };

    ApiAuth();

    State state;
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

    struct nodeVisited {
        const deCONZ::Node* node;
        bool visited;
    };

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
    int deleteUser(const ApiRequest &req, ApiResponse &rsp);
    int updateSoftware(const ApiRequest &req, ApiResponse &rsp);
    int updateFirmware(const ApiRequest &req, ApiResponse &rsp);
    int changePassword(const ApiRequest &req, ApiResponse &rsp);
    int deletePassword(const ApiRequest &req, ApiResponse &rsp);

    void configToMap(const ApiRequest &req, QVariantMap &map);

    // REST API lights
    int handleLightsApi(ApiRequest &req, ApiResponse &rsp);
    int getAllLights(const ApiRequest &req, ApiResponse &rsp);
    int searchLights(const ApiRequest &req, ApiResponse &rsp);
    int getNewLights(const ApiRequest &req, ApiResponse &rsp);
    int getLightState(const ApiRequest &req, ApiResponse &rsp);
    int setLightState(const ApiRequest &req, ApiResponse &rsp);
    int renameLight(const ApiRequest &req, ApiResponse &rsp);
    int deleteLight(const ApiRequest &req, ApiResponse &rsp);
    int removeAllScenes(const ApiRequest &req, ApiResponse &rsp);
    int removeAllGroups(const ApiRequest &req, ApiResponse &rsp);
    int getConnectivity(const ApiRequest &req, ApiResponse &rsp, bool alt);

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
    int modifyScene(const ApiRequest &req, ApiResponse &rsp);
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
    bool jsonToSchedule(const QString &jsonString, Schedule &schedule, ApiResponse *rsp);

    // REST API touchlink
    void initTouchlinkApi();
    int handleTouchlinkApi(ApiRequest &req, ApiResponse &rsp);
    int touchlinkScan(ApiRequest &req, ApiResponse &rsp);
    int getTouchlinkScanResults(ApiRequest &req, ApiResponse &rsp);
    int identifyLight(ApiRequest &req, ApiResponse &rsp);
    int resetLight(ApiRequest &req, ApiResponse &rsp);

    // REST API sensors
    int handleSensorsApi(ApiRequest &req, ApiResponse &rsp);
    int getAllSensors(const ApiRequest &req, ApiResponse &rsp);
    int getSensor(const ApiRequest &req, ApiResponse &rsp);
    int findNewSensors(const ApiRequest &req, ApiResponse &rsp);
    int getNewSensors(const ApiRequest &req, ApiResponse &rsp);
    int updateSensor(const ApiRequest &req, ApiResponse &rsp);
    int deleteSensor(const ApiRequest &req, ApiResponse &rsp);
    int changeSensorConfig(const ApiRequest &req, ApiResponse &rsp);
    int changeSensorState(const ApiRequest &req, ApiResponse &rsp);
    int createSensor(const ApiRequest &req, ApiResponse &rsp);
    int getGroupIdentifiers(const ApiRequest &req, ApiResponse &rsp);
    int recoverSensor(const ApiRequest &req, ApiResponse &rsp);
    bool sensorToMap(const Sensor *sensor, QVariantMap &map);

    // REST API rules
    int handleRulesApi(const ApiRequest &req, ApiResponse &rsp);
    int getAllRules(const ApiRequest &req, ApiResponse &rsp);
    int getRule(const ApiRequest &req, ApiResponse &rsp);
    int createRule(const ApiRequest &req, ApiResponse &rsp);
    int updateRule(const ApiRequest &req, ApiResponse &rsp);
    int deleteRule(const ApiRequest &req, ApiResponse &rsp);
    void queueCheckRuleBindings(const Rule &rule);

    bool checkActions(QVariantList actionsList, ApiResponse &rsp);
    bool checkConditions(QVariantList conditionsList, ApiResponse &rsp);

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
    void otauDataIndication(const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame);
    void otauSendNotify(LightNode *node);
    void otauSendStdNotify(LightNode *node);
    bool isOtauBusy();
    bool isOtauActive();

    // WSNDemo sensor
    void wsnDemoDataIndication(const deCONZ::ApsDataIndication &ind);

    //Channel Change
    void initChangeChannelApi();
    bool startChannelChange(quint8 channel);

    //reset Device
    void initResetDeviceApi();

    //Timezone
    std::string getTimezone();


public Q_SLOTS:
    void announceUpnp();
    void upnpReadyRead();
    void apsdeDataIndication(const deCONZ::ApsDataIndication &ind);
    void apsdeDataConfirm(const deCONZ::ApsDataConfirm &conf);
    void gpDataIndication(const deCONZ::GpDataIndication &ind);
    void gpProcessButtonEvent(const deCONZ::GpDataIndication &ind);
    int taskCountForAddress(const deCONZ::Address &address);
    void processTasks();
    void processGroupTasks();
    void nodeEvent(const deCONZ::NodeEvent &event);
    void internetDiscoveryTimerFired();
    void internetDiscoveryFinishedRequest(QNetworkReply *reply);
    void internetDiscoveryExtractVersionInfo(QNetworkReply *reply);
    void scheduleTimerFired();
    void permitJoinTimerFired();
    void otauTimerFired();
    void updateSoftwareTimerFired();
    void lockGatewayTimerFired();
    void saveCurrentRuleInDbTimerFired();
    void openClientTimerFired();
    void clientSocketDestroyed();
    void saveDatabaseTimerFired();
    void userActivity();
    bool sendBindRequest(BindingTask &bt);
    void checkLightBindingsForAttributeReporting(LightNode *lightNode);
    void checkSensorBindingsForAttributeReporting(Sensor *sensor);
    void bindingTimerFired();
    void bindingToRuleTimerFired();
    void verifyRuleBindingsTimerFired();
    void queueBindingTask(const BindingTask &bindingTask);

    // touchlink
    void touchlinkDisconnectNetwork();
    void checkTouchlinkNetworkDisconnected();
    void startTouchlinkMode(uint8_t channel);
    void startTouchlinkModeConfirm(deCONZ::TouchlinkStatus status);
    void sendTouchlinkConfirm(deCONZ::TouchlinkStatus status);
    void sendTouchlinkScanRequest();
    void sendTouchlinkIdentifyRequest();
    void sendTouchlinkResetRequest();
    void touchlinkTimerFired();
    void touchlinkScanTimeout();
    void interpanDataIndication(const QByteArray &data);
    void touchlinkStartReconnectNetwork(int delay);
    void touchlinkReconnectNetwork();
    bool isTouchlinkActive();

    // channel change
    void channelchangeTimerFired();
    void changeChannel(quint8 channel);
    bool verifyChannel(quint8 channel);
    void channelChangeSendConfirm(bool success);
    void channelChangeDisconnectNetwork();
    void checkChannelChangeNetworkDisconnected();
    void channelChangeStartReconnectNetwork(int delay);
    void channelChangeReconnectNetwork();

    //reset device
    void resetDeviceTimerFired();
    void checkResetState();
    void resetDeviceSendConfirm(bool success);

    // firmware update
    void initFirmwareUpdate();
    void firmwareUpdateTimerFired();
    void checkFirmwareDevices();
    void queryFirmwareVersion();
    void updateFirmwareDisconnectDevice();
    void updateFirmware();
    void updateFirmwareWaitFinished();
    bool startUpdateFirmware();

public:
    void checkRfConnectState();
    bool isInNetwork();
    void generateGatewayUuid();
    void updateEtag(QString &etag);
    qint64 getUptime();
    void addLightNode(const deCONZ::Node *node);
    void nodeZombieStateChanged(const deCONZ::Node *node);
    LightNode *updateLightNode(const deCONZ::NodeEvent &event);
    LightNode *getLightNodeForAddress(quint64 extAddr, quint8 endpoint = 0);
    int getNumberOfEndpoints(quint64 extAddr);
    LightNode *getLightNodeForId(const QString &id);
    Rule *getRuleForId(const QString &id);
    Rule *getRuleForName(const QString &name);
    void addSensorNode(const deCONZ::Node *node);
    void addSensorNode(const deCONZ::Node *node, const SensorFingerprint &fingerPrint, const QString &type);
    void checkSensorNodeReachable(Sensor *sensor);
    void updateSensorNode(const deCONZ::NodeEvent &event);
    void checkAllSensorsAvailable();
    Sensor *getSensorNodeForAddressAndEndpoint(quint64 extAddr, quint8 ep);
    Sensor *getSensorNodeForAddress(quint64 extAddr);
    Sensor *getSensorNodeForFingerPrint(quint64 extAddr, const SensorFingerprint &fingerPrint, const QString &type);
    Sensor *getSensorNodeForUniqueId(const QString &uniqueId);
    Sensor *getSensorNodeForId(const QString &id);
    Group *getGroupForName(const QString &name);
    Group *getGroupForId(uint16_t id);
    Group *getGroupForId(const QString &id);
    Scene *getSceneForId(uint16_t gid, uint8_t sid);
    GroupInfo *getGroupInfo(LightNode *lightNode, uint16_t id);
    GroupInfo *createGroupInfo(LightNode *lightNode, uint16_t id);
    deCONZ::Node *getNodeForAddress(uint64_t extAddr);
    deCONZ::ZclCluster *getInCluster(deCONZ::Node *node, uint8_t endpoint, uint16_t clusterId);
    uint8_t getSrcEndpoint(RestNodeBase *restNode, const deCONZ::ApsDataRequest &req);
    bool processZclAttributes(LightNode *lightNode);
    bool processZclAttributes(Sensor *sensorNode);
    bool readBindingTable(RestNodeBase *node, quint8 startIndex);
    bool getGroupIdentifiers(RestNodeBase *node, quint8 endpoint, quint8 startIndex);
    bool readAttributes(RestNodeBase *restNode, quint8 endpoint, uint16_t clusterId, const std::vector<uint16_t> &attributes);
    bool writeAttribute(RestNodeBase *restNode, quint8 endpoint, uint16_t clusterId, const deCONZ::ZclAttribute &attribute);
    bool readSceneAttributes(LightNode *lightNode, uint16_t groupId, uint8_t sceneId);
    bool readGroupMembership(LightNode *lightNode, const std::vector<uint16_t> &groups);
    void foundGroupMembership(LightNode *lightNode, uint16_t groupId);
    void foundGroup(uint16_t groupId);
    bool isLightNodeInGroup(LightNode *lightNode, uint16_t groupId);
    void deleteLightFromScenes(QString lightId, uint16_t groupId);
    void readAllInGroup(Group *group);
    void setAttributeOnOffGroup(Group *group, uint8_t onOff);
    bool readSceneMembership(LightNode *lightNode, Group *group);
    void foundScene(LightNode *lightNode, Group *group, uint8_t sceneId);
    void setSceneName(Group *group, uint8_t sceneId, const QString &name);
    bool storeScene(Group *group, uint8_t sceneId);
    bool modifyScene(Group *group, uint8_t sceneId);
    bool removeScene(Group *group, uint8_t sceneId);
    bool callScene(Group *group, uint8_t sceneId);
    bool removeAllScenes(Group *group);

    bool pushState(QString json, QTcpSocket *sock);

    void pushClientForClose(QTcpSocket *sock, int closeTimeout);

    uint8_t endpoint();

    // Task interface
    bool addTask(const TaskItem &task);
    bool addTaskMoveLevel(TaskItem &task, bool withOnOff, bool upDirection, quint8 rate);
    bool addTaskSetOnOff(TaskItem &task, quint8 cmd, quint16 ontime);
    bool addTaskSetBrightness(TaskItem &task, uint8_t bri, bool withOnOff);
    bool addTaskStopBrightness(TaskItem &task);
    bool addTaskSetColorTemperature(TaskItem &task, uint16_t ct);
    bool addTaskSetEnhancedHue(TaskItem &task, uint16_t hue);
    bool addTaskSetSaturation(TaskItem &task, uint8_t sat);
    bool addTaskSetHueAndSaturation(TaskItem &task, uint8_t hue, uint8_t sat);
    bool addTaskSetXyColorAsHueAndSaturation(TaskItem &task, double x, double y);
    bool addTaskSetXyColor(TaskItem &task, double x, double y);
    bool addTaskSetColorLoop(TaskItem &task, bool colorLoopActive, uint8_t speed);
    bool addTaskIdentify(TaskItem &task, uint16_t identifyTime);
    bool addTaskAddToGroup(TaskItem &task, uint16_t groupId);
    bool addTaskViewGroup(TaskItem &task, uint16_t groupId);
    bool addTaskRemoveFromGroup(TaskItem &task, uint16_t groupId);
    bool addTaskStoreScene(TaskItem &task, uint16_t groupId, uint8_t sceneId);
    bool addTaskAddScene(TaskItem &task, uint16_t groupId, uint8_t sceneId, QString lightId);
    bool addTaskRemoveScene(TaskItem &task, uint16_t groupId, uint8_t sceneId);
    bool obtainTaskCluster(TaskItem &task, const deCONZ::ApsDataIndication &ind);
    void handleGroupClusterIndication(TaskItem &task, const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame);
    void handleSceneClusterIndication(TaskItem &task, const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame);
    void handleOnOffClusterIndication(TaskItem &task, const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame);
    void handleCommissioningClusterIndication(TaskItem &task, const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame);
    void handleDeviceAnnceIndication(const deCONZ::ApsDataIndication &ind);
    void handleMgmtBindRspIndication(const deCONZ::ApsDataIndication &ind);
    void handleBindAndUnbindRspIndication(const deCONZ::ApsDataIndication &ind);
    void handleMgmtLeaveRspIndication(const deCONZ::ApsDataIndication &ind);
    void broadCastNodeUpdate(LightNode *webNode);
    void markForPushUpdate(LightNode *lightNode);
    void taskToLocalData(const TaskItem &task);

    // Modify node attributes
    void setAttributeOnOff(LightNode *lightNode);
    void setAttributeLevel(LightNode *lightNode);
    void setAttributeEnhancedHue(LightNode *lightNode);
    void setAttributeSaturation(LightNode *lightNode);
    void setAttributeColorXy(LightNode *lightNode);
    void setAttributeColorTemperature(LightNode *lightNode);
    void setAttributeColorLoopActive(LightNode *lightNode);

    // Database interface
    void initDb();
    void openDb();
    void readDb();
    void loadAuthFromDb();
    void loadConfigFromDb();
    void loadAllGroupsFromDb();
    void loadAllSchedulesFromDb();
    void loadLightNodeFromDb(LightNode *lightNode);
    void loadSensorNodeFromDb(Sensor *sensorNode);
    void loadGroupFromDb(Group *group);
    void loadSceneFromDb(Scene *scene);
    void loadAllRulesFromDb();
    void loadAllSensorsFromDb();
    int getFreeLightId();
    int getFreeSensorId();
    void saveDb();
    void closeDb();
    void queSaveDb(int items, int msec);

    sqlite3 *db;
    int saveDatabaseItems;
    QString sqliteDatabaseName;
    std::vector<int> lightIds;
    std::vector<int> sensorIds;
    QTimer *databaseTimer;

    // authentification
    std::vector<ApiAuth> apiAuths;
    QString gwAdminUserName;
    QString gwAdminPasswordHash;

    // configuration
    bool gwLinkButton;
    bool gwRfConnectedExpected;  // the state which should be hold
    bool gwRfConnected;  // to detect changes
    int gwAnnounceInterval; // used by internet discovery [minutes]
    QString gwAnnounceUrl;
    uint8_t gwPermitJoinDuration; // global permit join state (last set)
    uint16_t gwNetworkOpenDuration; // user setting how long network remains open
    QString gwTimezone;
    QString gwTimeFormat;
    QString gwIpAddress;
    uint16_t gwPort;
    QString gwName;
    QString gwUuid;
    QString gwUpdateVersion;
    QString gwRgbwDisplay;
    QString gwFirmwareVersion;
    QString gwFirmwareVersionUpdate; // for local update of the firmware if it doesn't fit the GW_MIN_<platform>_FW_VERSION
    bool gwFirmwareNeedUpdate;
    QString gwUpdateChannel;
    int gwGroupSendDelay;
    uint gwZigbeeChannel;
    QVariantMap gwConfig;
    QString gwConfigEtag;
    bool gwRunFromShellScript;
    bool gwDeleteUnknownRules;
    bool groupDeviceMembershipChecked;

    // firmware update
    enum FW_UpdateState {
        FW_Idle,
        FW_CheckVersion,
        FW_CheckDevices,
        FW_WaitUserConfirm,
        FW_DisconnectDevice,
        FW_Update,
        FW_UpdateWaitFinished
    };
    QTimer *fwUpdateTimer;
    int fwUpdateIdleTimeout;
    FW_UpdateState fwUpdateState;
    QString fwUpdateFile;
    QProcess *fwProcess;
    QStringList fwProcessArgs;

    // upnp
    QByteArray descriptionXml;

    // gateway lock (link button)
    QTimer *lockGatewayTimer;

    // permit join
    // used by searchLights()
    QTimer *permitJoinTimer;
    QTime permitJoinLastSendTime;
    bool permitJoinFlag; // indicates that permitJoin changed from greater than 0 to 0

    // schedules
    QTimer *scheduleTimer;
    std::vector<Schedule> schedules;

    // internet discovery
    QNetworkAccessManager *inetDiscoveryManager;
    QTimer *inetDiscoveryTimer;
    QNetworkReply *inetDiscoveryResponse;
    QString osPrettyName;
    QString piRevision;

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
        int8_t rssi;
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

    // channel change state machine
    enum ChannelChangeState
    {
        CC_Idle,
        CC_Verify_Channel,
        CC_WaitConfirm,
        CC_Change_Channel,
        CC_DisconnectingNetwork,
        CC_ReconnectNetwork
    };

    ChannelChangeState channelChangeState;
    QTimer *channelchangeTimer;
    quint8 ccRetries;
    int ccNetworkDisconnectAttempts; // disconnect attemps before chanelchange
    int ccNetworkReconnectAttempts; // reconnect attemps after channelchange
    bool ccNetworkConnectedBefore;
    uint8_t channelChangeApsRequestId;

    // delete device state machine
    enum ResetDeviceState
    {
        ResetIdle,
        ResetWaitConfirm,
        ResetWaitIndication
    };

    QTimer *resetDeviceTimer;
    ResetDeviceState resetDeviceState;
    uint8_t zdpResetSeq;
    uint64_t lastNodeAddressExt;
    uint8_t resetDeviceApsRequestId;

    // sensors
    QString lastscan;

    // rules

    QTimer *saveCurrentRuleInDbTimer;
    // general
    deCONZ::ApsController *apsCtrl;
    uint groupTaskNodeIter; // Iterates through nodes array
    int idleTotalCounter; // sys timer
    int idleLimit;
    int idleLastActivity; // delta in seconds
    bool supportColorModeXyForGroups;
    size_t lightIter;
    size_t sensorIter;
    size_t lightAttrIter;
    size_t sensorAttrIter;
    std::vector<Group> groups;
    std::vector<LightNode> nodes;
    std::vector<Rule> rules;
    std::vector<Sensor> sensors;
    std::list<LightNode*> broadCastUpdateNodes;
    std::list<TaskItem> tasks;
    std::list<TaskItem> runningTasks;
    QTimer *verifyRulesTimer;
    QTimer *taskTimer;
    QTimer *groupTaskTimer;
    uint8_t zclSeq;
    std::list<QTcpSocket*> eventListeners;
    QUdpSocket *udpSock;
    QUdpSocket *udpSockOut;
    uint8_t haEndpoint;

    // bindings
    size_t verifyRuleIter;
    bool gwReportingEnabled;
    QTimer *bindingToRuleTimer;
    QTimer *bindingTimer;
    std::list<Binding> bindingToRuleQueue; // check if rule exists for discovered bindings
    std::list<BindingTask> bindingQueue; // bind/unbind queue

    // TCP connection watcher
    QTimer *openClientTimer;
    std::list<TcpClient> openClients;

    // will be set at startup to calculate the uptime
    QElapsedTimer starttimeRef;

    Q_DECLARE_PUBLIC(DeRestPlugin)
    DeRestPlugin *q_ptr; // public interface

};

#endif // DE_WEB_PLUGIN_PRIVATE_H
