/*
 * Copyright (c) 2017-2020 dresden elektronik ingenieurtechnik gmbh.
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
#include <sqlite3.h>
#include <deconz.h>
#include "resource.h"
#include "daylight.h"
#include "event.h"
#include "green_power.h"
#include "resource.h"
#include "rest_node_base.h"
#include "light_node.h"
#include "group.h"
#include "group_info.h"
#include "scene.h"
#include "sensor.h"
#include "resourcelinks.h"
#include "rule.h"
#include "bindings.h"
#include <math.h>
#include "websocket_server.h"
#include "tuya.h"

#if defined(Q_OS_LINUX) && !defined(Q_PROCESSOR_X86)
  // Workaround to detect ARM and AARCH64 in older Qt versions.
  #define ARCH_ARM
#endif

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
#define IDLE_ATTR_REPORT_BIND_LIMIT 1800
#define IDLE_ATTR_REPORT_BIND_LIMIT_SHORT 5
#define BUTTON_ATTR_REPORT_BIND_LIMIT 120
#define WARMUP_TIME 120

#define MAX_UNLOCK_GATEWAY_TIME 600
#define MAX_RECOVER_ENTRY_AGE 600
#define PERMIT_JOIN_SEND_INTERVAL (1000 * 1800)
#define EXT_PROCESS_TIMEOUT 10000
#define SET_ENDPOINTCONFIG_DURATION (1000 * 16) // time deCONZ needs to update Endpoints
#define OTA_LOW_PRIORITY_TIME (60 * 2)
#define CHECK_SENSOR_FAST_ROUNDS 3
#define CHECK_SENSOR_FAST_INTERVAL 100
#define CHECK_SENSOR_INTERVAL      1000
#define CHECK_SENSORS_MAX          10
#define CHECK_ZB_GOOD_INTERVAL     60

// wifi managed flags
#define WIFI_MGTM_HOSTAPD         0x01  // hostapd (by deCONZ)
#define WIFI_MGTM_WPA_SUPPLICANT  0x02  // wpa_supplicant (by deCONZ)
#define WIFI_MGTM_INTERFACES      0x04  // interfaces (by deCONZ)
#define WIFI_MGMT_ACTIVE          0x08  // 1 when accesspoint or client is active

#define DE_OTAU_ENDPOINT             0x50
#define DE_PROFILE_ID              0xDE00

// Digi Drop-In-Networking (DIN) ZigBee Profile, used by the XBee.
#define DIN_PROFILE_ID                      0xC105 // Digi Drop-In-Networking
#define DIN_DDO_ENDPOINT                    0xE6   // Digi Device Object endpoint
#define DIN_DDM_ENDPOINT                    0xE8   // Digi Data Management endpoint
#define DEV_ID_DIN_XBEE                     0x0001 // Device ID used by the XBee

// Generic devices
#define DEV_ID_ONOFF_SWITCH                 0x0000 // On/Off switch
#define DEV_ID_LEVEL_CONTROL_SWITCH         0x0001 // Level control switch
#define DEV_ID_ONOFF_OUTPUT                 0x0002 // On/Off output
#define DEV_ID_LEVEL_CONTROLLABLE_OUTPUT    0x0003 // Level controllable output
#define DEV_ID_CONFIGURATION_TOOL           0x0005 // Configuration tool
#define DEV_ID_RANGE_EXTENDER               0x0008 // Range extender
#define DEV_ID_MAINS_POWER_OUTLET           0x0009 // Mains power outlet
#define DEV_ID_CONSUMPTION_AWARENESS_DEVICE 0x000d // Consumption awareness device
#define DEV_ID_FAN                          0x000e // Fan (used by Hamption Bay fan module)
#define DEV_ID_SMART_PLUG                   0x0051 // Smart plug
// HA lighting devices
#define DEV_ID_HA_ONOFF_LIGHT               0x0100 // On/Off light
#define DEV_ID_HA_DIMMABLE_LIGHT            0x0101 // Dimmable light
#define DEV_ID_HA_COLOR_DIMMABLE_LIGHT      0x0102 // Color dimmable light
#define DEV_ID_HA_ONOFF_LIGHT_SWITCH        0x0103 // On/Off light switch
#define DEV_ID_HA_DIMMER_SWITCH             0x0104 // Dimmer switch
#define DEV_ID_HA_LIGHT_SENSOR              0x0106 // Light sensor
#define DEV_ID_HA_OCCUPANCY_SENSOR          0x0107 // Occupancy sensor

// Other HA devices
#define DEV_ID_HA_WINDOW_COVERING_DEVICE    0x0202 // Window Covering Device
#define DEV_ID_HA_WINDOW_COVERING_CONTROLLER 0x0203 // Window Covering Controller

// Danalock support
#define DEV_ID_DOOR_LOCK                    0x000a // Door Lock

//
#define DEV_ID_IAS_ZONE                     0x0402 // IAS Zone
#define DEV_ID_IAS_WARNING_DEVICE           0x0403 // IAS Warning Device
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
// ZigBee 3.0 lighting devices
#define DEV_ID_Z30_ONOFF_PLUGIN_UNIT        0x010a // On/Off plugin unit
#define DEV_ID_Z30_DIMMABLE_PLUGIN_UNIT     0x010b // Dimmable plugin unit
#define DEV_ID_Z30_COLOR_TEMPERATURE_LIGHT  0x010c // Color temperature light
#define DEV_ID_Z30_EXTENDED_COLOR_LIGHT     0x010d // Extended color light
// ZLL controller devices
#define DEV_ID_ZLL_COLOR_CONTROLLER         0x0800 // Color controller
#define DEV_ID_ZLL_COLOR_SCENE_CONTROLLER   0x0810 // Color scene controller
#define DEV_ID_ZLL_NON_COLOR_CONTROLLER     0x0820 // Non color controller
#define DEV_ID_ZLL_NON_COLOR_SCENE_CONTROLLER 0x0830 // Non color scene controller
#define DEV_ID_ZLL_CONTROL_BRIDGE           0x0840 // Control bridge
#define DEV_ID_ZLL_ONOFF_SENSOR             0x0850 // On/Off sensor

#define DEV_ID_XIAOMI_SMART_PLUG            0xffff

#define DEFAULT_TRANSITION_TIME 4 // 400ms
#define MAX_ENHANCED_HUE 65535
#define MAX_ENHANCED_HUE_Z 65278 // max supportet ehue of all devices
#define MIN_UNIQUEID_LENGTH 26   // 00:21:2e:ff:ff:00:a6:fd-02

#define BASIC_CLUSTER_ID                      0x0000
#define POWER_CONFIGURATION_CLUSTER_ID        0x0001
#define IDENTIFY_CLUSTER_ID                   0x0003
#define GROUP_CLUSTER_ID                      0x0004
#define SCENE_CLUSTER_ID                      0x0005
#define ONOFF_CLUSTER_ID                      0x0006
#define ONOFF_SWITCH_CONFIGURATION_CLUSTER_ID 0x0007
#define LEVEL_CLUSTER_ID                      0x0008
#define TIME_CLUSTER_ID                       0x000A
#define ANALOG_INPUT_CLUSTER_ID               0x000C
#define ANALOG_OUTPUT_CLUSTER_ID              0x000D
#define BINARY_INPUT_CLUSTER_ID               0x000F
#define MULTISTATE_INPUT_CLUSTER_ID           0x0012
#define OTAU_CLUSTER_ID                       0x0019
#define POLL_CONTROL_CLUSTER_ID               0x0020
#define DOOR_LOCK_CLUSTER_ID                  0x0101
#define WINDOW_COVERING_CLUSTER_ID            0x0102
#define THERMOSTAT_CLUSTER_ID                 0x0201
#define FAN_CONTROL_CLUSTER_ID                0x0202
#define THERMOSTAT_UI_CONFIGURATION_CLUSTER_ID 0x0204
#define COLOR_CLUSTER_ID                      0x0300
#define ILLUMINANCE_MEASUREMENT_CLUSTER_ID    0x0400
#define ILLUMINANCE_LEVEL_SENSING_CLUSTER_ID  0x0401
#define TEMPERATURE_MEASUREMENT_CLUSTER_ID    0x0402
#define PRESSURE_MEASUREMENT_CLUSTER_ID       0x0403
#define RELATIVE_HUMIDITY_CLUSTER_ID          0x0405
#define OCCUPANCY_SENSING_CLUSTER_ID          0x0406
#define IAS_ZONE_CLUSTER_ID                   0x0500
#define IAS_ACE_CLUSTER_ID                    0x0501
#define IAS_WD_CLUSTER_ID                     0x0502
#define METERING_CLUSTER_ID                   0x0702
#define APPLIANCE_EVENTS_AND_ALERTS_CLUSTER_ID 0x0B02
#define ELECTRICAL_MEASUREMENT_CLUSTER_ID     0x0B04
#define DIAGNOSTICS_CLUSTER_ID                0x0B05
#define COMMISSIONING_CLUSTER_ID              0x1000
#define TUYA_CLUSTER_ID                       0xEF00
#define DE_CLUSTER_ID                         0xFC00
#define VENDOR_CLUSTER_ID                     0xFC00
#define UBISYS_DEVICE_SETUP_CLUSTER_ID        0xFC00
#define SAMJIN_CLUSTER_ID                     0xFC02
#define LEGRAND_CONTROL_CLUSTER_ID            0xFC40
#define XAL_CLUSTER_ID                        0xFCCE
#define BOSCH_AIR_QUALITY_CLUSTER_ID          quint16(0xFDEF)

#define IAS_ZONE_CLUSTER_ATTR_ZONE_STATUS_ID  0x0002

#define ONOFF_COMMAND_OFF     0x00
#define ONOFF_COMMAND_ON      0x01
#define ONOFF_COMMAND_TOGGLE  0x02
#define ONOFF_COMMAND_OFF_WITH_EFFECT  0x040
#define ONOFF_COMMAND_ON_WITH_TIMED_OFF  0x42
#define LEVEL_COMMAND_MOVE_TO_LEVEL 0x00
#define LEVEL_COMMAND_MOVE 0x01
#define LEVEL_COMMAND_STEP 0x02
#define LEVEL_COMMAND_STOP 0x03
#define LEVEL_COMMAND_MOVE_TO_LEVEL_WITH_ON_OFF 0x04
#define LEVEL_COMMAND_MOVE_WITH_ON_OFF 0x05
#define LEVEL_COMMAND_STEP_WITH_ON_OFF 0x06
#define LEVEL_COMMAND_STOP_WITH_ON_OFF 0x07
#define SCENE_COMMAND_RECALL_SCENE 0x05
#define SCENE_COMMAND_IKEA_STEP_CT 0x07
#define SCENE_COMMAND_IKEA_MOVE_CT 0x08
#define SCENE_COMMAND_IKEA_STOP_CT 0x09
#define WINDOW_COVERING_COMMAND_OPEN          0x00
#define WINDOW_COVERING_COMMAND_CLOSE         0x01
#define WINDOW_COVERING_COMMAND_STOP          0x02
#define WINDOW_COVERING_COMMAND_GOTO_LIFT_PCT 0x05
#define WINDOW_COVERING_COMMAND_GOTO_TILT_PCT 0x08

#define MULTI_STATE_INPUT_PRESENT_VALUE_ATTRIBUTE_ID quint16(0x0055)


// IAS Zone Types
#define IAS_ZONE_TYPE_STANDARD_CIE            0x0000
#define IAS_ZONE_TYPE_MOTION_SENSOR           0x000d
#define IAS_ZONE_TYPE_CONTACT_SWITCH          0x0015
#define IAS_ZONE_TYPE_FIRE_SENSOR             0x0028
#define IAS_ZONE_TYPE_WATER_SENSOR            0x002a
#define IAS_ZONE_TYPE_CARBON_MONOXIDE_SENSOR  0x002b
#define IAS_ZONE_TYPE_VIBRATION_SENSOR        0x002d
#define IAS_ZONE_TYPE_WARNING_DEVICE          0x0225

// read and write flags
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
#define WRITE_OCCUPANCY_CONFIG (1 << 11)
#define READ_GROUP_IDENTIFIERS (1 << 12)
#define WRITE_DELAY            (1 << 13)
#define WRITE_LEDINDICATION    (1 << 14)
#define WRITE_SENSITIVITY      (1 << 15)
#define WRITE_USERTEST         (1 << 16)
#define READ_THERMOSTAT_STATE  (1 << 17)
#define READ_BATTERY           (1 << 18)
#define READ_TIME              (1 << 19)
#define WRITE_TIME             (1 << 20)
#define READ_THERMOSTAT_SCHEDULE (1 << 21)

#define READ_MODEL_ID_INTERVAL   (60 * 60) // s
#define READ_SWBUILD_ID_INTERVAL (60 * 60) // s

// manufacturer codes
// https://github.com/wireshark/wireshark/blob/master/epan/dissectors/packet-zbee.h
#define VENDOR_NONE                 0x0000
#define VENDOR_EMBER                0x1002
#define VENDOR_PHILIPS              0x100B // Also used by iCasa routers
#define VENDOR_VISONIC              0x1011
#define VENDOR_ATMEL                0x1014
#define VENDOR_DEVELCO              0x1015
#define VENDOR_MAXSTREAM            0x101E // Used by Digi
#define VENDOR_VANTAGE              0x1021
#define VENDOR_LEGRAND              0x1021 // wrong name?
#define VENDOR_LGE                  0x102E
#define VENDOR_JENNIC               0x1037 // Used by Xiaomi, Trust, Eurotronic
#define VENDOR_ALERTME              0x1039
#define VENDOR_CLS                  0x104E
#define VENDOR_CENTRALITE           0x104E // wrong name?
#define VENDOR_SI_LABS              0x1049
#define VENDOR_4_NOKS               0x1071
#define VENDOR_BITRON               0x1071 // branded
#define VENDOR_COMPUTIME            0x1078
#define VENDOR_AXIS                 0x1262 // Axis
#define VENDOR_KWIKSET              0x1092
#define VENDOR_MMB                  0x109a
#define VENDOR_NETVOX               0x109F
#define VENDOR_NYCE                 0x10B9
#define VENDOR_UNIVERSAL2           0x10EF
#define VENDOR_UBISYS               0x10F2
#define VENDOR_DANALOCK             0x115C
#define VENDOR_SCHLAGE              0x1236 // Used by Schlage Locks
#define VENDOR_BEGA                 0x1105
#define VENDOR_PHYSICAL             0x110A // Used by SmartThings
#define VENDOR_OSRAM                0x110C
#define VENDOR_PROFALUX             0x1110
#define VENDOR_EMBERTEC             0x1112
#define VENDOR_JASCO                0x1124 // Used by GE
#define VENDOR_BUSCH_JAEGER         0x112E
#define VENDOR_SERCOMM              0x1131
#define VENDOR_BOSCH                0x1133
#define VENDOR_DDEL                 0x1135
#define VENDOR_WAXMAN               0x113B
#define VENDOR_OWON                 0x113C
#define VENDOR_LUTRON               0x1144
#define VENDOR_BOSCH2               0x1155
#define VENDOR_ZEN                  0x1158
#define VENDOR_KEEN_HOME            0x115B
#define VENDOR_XIAOMI               0x115F
#define VENDOR_SENGLED_OPTOELEC     0x1160
#define VENDOR_INNR                 0x1166
#define VENDOR_LDS                  0x1168 // Used by Samsung SmartPlug 2019
#define VENDOR_PLUGWISE_BV          0x1172
#define VENDOR_INSTA                0x117A
#define VENDOR_IKEA                 0x117C
#define VENDOR_3A_SMART_HOME        0x117E
#define VENDOR_STELPRO              0x1185
#define VENDOR_LEDVANCE             0x1189
#define VENDOR_SINOPE               0x119C
#define VENDOR_JIUZHOU              0x119D
#define VENDOR_PAULMANN             0x119D // branded
#define VENDOR_HEIMAN               0x120B
#define VENDOR_CHINA_FIRE_SEC       0x1214
#define VENDOR_MUELLER              0x121B // Used by Mueller Licht
#define VENDOR_AURORA               0x121C // Used by Aurora Aone
#define VENDOR_SUNRICHER            0x1224 // white label used by iCasa, Illuminize, Namron ...
#define VENDOR_XAL                  0x122A
#define VENDOR_ADUROLIGHT           0x122D
#define VENDOR_THIRD_REALITY        0x1233
#define VENDOR_DSR                  0x1234
#define VENDOR_HANGZHOU_IMAGIC      0x123B
#define VENDOR_SAMJIN               0x1241
#define VENDOR_DANFOSS              0x1246
#define VENDOR_NIKO_NV              0x125F
#define VENDOR_KONKE                0x1268
#define VENDOR_SHYUGJ_TECHNOLOGY    0x126A
#define VENDOR_OSRAM_STACK          0xBBAA
#define VENDOR_C2DF                 0xC2DF
#define VENDOR_PHILIO               0xFFA0

#define ANNOUNCE_INTERVAL 45 // minutes default announce interval

#define MAX_NODES 200
#define MAX_SENSORS 1000
#define MAX_GROUPS 100
#define MAX_SCENES 100
#define MAX_LIGHTSTATES 1000
#define MAX_SCHEDULES 500
#define MAX_RULES 500
#define MAX_CONDITIONS 1000
#define MAX_ACTIONS 1000
#define MAX_RESOURCELINKS 100
#define MAX_STREAMING 0
#define MAX_CHANNELS 50

#define MAX_GROUP_SEND_DELAY 5000 // ms between to requests to the same group
#define GROUP_SEND_DELAY 50 // default ms between to requests to the same group
#define MAX_TASKS_PER_NODE 2
#define MAX_BACKGROUND_TASKS 5

#define MAX_RULE_ILLUMINANCE_VALUE_AGE_MS (1000 * 60 * 20) // 20 minutes

// string lengths
#define MAX_GROUP_NAME_LENGTH 32
#define MAX_SCENE_NAME_LENGTH 32
#define MAX_RULE_NAME_LENGTH 32
#define MAX_SENSOR_NAME_LENGTH 32

// REST API return codes
#define REQ_READY_SEND   0
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
#define FW_PLATFORM_AVR           0x00000500UL
#define FW_PLATFORM_R21           0x00000700UL

// schedules
#define SCHEDULE_CHECK_PERIOD 1000

// save database items
#define DB_LIGHTS         0x00000001
#define DB_GROUPS         0x00000002
#define DB_AUTH           0x00000004
#define DB_CONFIG         0x00000008
#define DB_SCENES         0x00000010
#define DB_SCHEDULES      0x00000020
#define DB_RULES          0x00000040
#define DB_SENSORS        0x00000080
#define DB_USERPARAM      0x00000100
#define DB_GATEWAYS       0x00000200
#define DB_RESOURCELINKS  0x00000400
#define DB_QUERY_QUEUE    0x00000800
#define DB_SYNC           0x00001000
#define DB_NOSAVE         0x00002000

#define DB_HUGE_SAVE_DELAY  (60 * 60 * 1000) // 60 minutes
#define DB_LONG_SAVE_DELAY  (15 * 60 * 1000) // 15 minutes
#define DB_SHORT_SAVE_DELAY (5 *  1 * 1000) // 5 seconds

#define DB_CONNECTION_TTL (60 * 15) // 15 minutes

// internet discovery

// network reconnect
#define DISCONNECT_CHECK_DELAY 100
#define NETWORK_ATTEMPS        10
#define RECONNECT_CHECK_DELAY  5000
#define RECONNECT_NOW          100

extern const quint64 macPrefixMask;

extern const quint64 celMacPrefix;
extern const quint64 bjeMacPrefix;
extern const quint64 davicomMacPrefix;
extern const quint64 deMacPrefix;
extern const quint64 emberMacPrefix;
extern const quint64 embertecMacPrefix;
extern const quint64 energyMiMacPrefix;
extern const quint64 heimanMacPrefix;
extern const quint64 zenMacPrefix;
extern const quint64 ikeaMacPrefix;
extern const quint64 ikea2MacPrefix;
extern const quint64 silabsMacPrefix;
extern const quint64 silabs2MacPrefix;
extern const quint64 silabs3MacPrefix;
extern const quint64 silabs4MacPrefix;
extern const quint64 silabs5MacPrefix;
extern const quint64 silabs6MacPrefix;
extern const quint64 silabs7MacPrefix;
extern const quint64 silabs8MacPrefix;
extern const quint64 silabs9MacPrefix;
extern const quint64 instaMacPrefix;
extern const quint64 boschMacPrefix;
extern const quint64 jennicMacPrefix;
extern const quint64 keenhomeMacPrefix;
extern const quint64 lutronMacPrefix;
extern const quint64 netvoxMacPrefix;
extern const quint64 osramMacPrefix;
extern const quint64 philipsMacPrefix;
extern const quint64 sinopeMacPrefix;
extern const quint64 stMacPrefix;
extern const quint64 samjinMacPrefix;
extern const quint64 tiMacPrefix;
extern const quint64 ubisysMacPrefix;
extern const quint64 xalMacPrefix;
extern const quint64 develcoMacPrefix;
extern const quint64 legrandMacPrefix;
extern const quint64 profaluxMacPrefix;
extern const quint64 xiaomiMacPrefix;
extern const quint64 computimeMacPrefix;
extern const quint64 konkeMacPrefix;
extern const quint64 ecozyMacPrefix;
extern const quint64 zhejiangMacPrefix;
// Danalock support
extern const quint64 danalockMacPrefix;
extern const quint64 schlageMacPrefix;

extern const QDateTime epoch;

inline bool existDevicesWithVendorCodeForMacPrefix(quint64 addr, quint16 vendor)
{
    const quint64 prefix = addr & macPrefixMask;
    switch (vendor) {
        case VENDOR_XIAOMI:
            return prefix == jennicMacPrefix ||
                   prefix == xiaomiMacPrefix;
        case VENDOR_SINOPE:
            return prefix == sinopeMacPrefix;
        case VENDOR_HEIMAN:
            return prefix == emberMacPrefix ||
                   prefix == jennicMacPrefix;
        case VENDOR_SUNRICHER:
            return prefix == emberMacPrefix ||
                   prefix == silabs3MacPrefix ||
                   prefix == silabs6MacPrefix;
        case VENDOR_3A_SMART_HOME:
            return prefix == jennicMacPrefix;
        case VENDOR_ALERTME:
            return prefix == tiMacPrefix ||
                   prefix == computimeMacPrefix;
        case VENDOR_BITRON:
            return prefix == tiMacPrefix;
        case VENDOR_BOSCH:
            return prefix == boschMacPrefix ||
                   prefix == emberMacPrefix;
        case VENDOR_BUSCH_JAEGER:
            return prefix == bjeMacPrefix;
        case VENDOR_C2DF:
            return prefix == emberMacPrefix;
        case VENDOR_CENTRALITE:
            return prefix == emberMacPrefix;
        case VENDOR_CHINA_FIRE_SEC:
            return prefix == jennicMacPrefix;
        case VENDOR_DANFOSS:
            return prefix == silabs2MacPrefix;
        case VENDOR_EMBER:
            return prefix == emberMacPrefix ||
                   prefix == konkeMacPrefix ||
                   prefix == silabs3MacPrefix ||
                   prefix == silabs5MacPrefix ||
                   prefix == silabs7MacPrefix;
        case VENDOR_EMBERTEC:
            return prefix == embertecMacPrefix;
        case VENDOR_DDEL:
            return prefix == deMacPrefix ||
                   prefix == silabs3MacPrefix;
        case VENDOR_IKEA:
            return prefix == ikeaMacPrefix ||
                   prefix == silabsMacPrefix ||
                   prefix == silabs2MacPrefix ||
                   prefix == silabs4MacPrefix ||
                   prefix == energyMiMacPrefix ||
                   prefix == emberMacPrefix;
        case VENDOR_JASCO:
            return prefix == celMacPrefix;
        case VENDOR_INNR:
            return prefix == jennicMacPrefix ||
                   prefix == silabs4MacPrefix;
        case VENDOR_LDS:
            return prefix == jennicMacPrefix ||
                   prefix == silabsMacPrefix ||
                   prefix == silabs2MacPrefix;
        case VENDOR_INSTA:
            return prefix == instaMacPrefix;
        case VENDOR_JENNIC:
            return prefix == jennicMacPrefix;
        case VENDOR_KEEN_HOME:
            return prefix == keenhomeMacPrefix;
        case VENDOR_LGE:
            return prefix == emberMacPrefix;
        case VENDOR_LUTRON:
            return prefix == lutronMacPrefix;
        case VENDOR_NIKO_NV:
            return prefix == konkeMacPrefix;
        case VENDOR_NYCE:
            return prefix == emberMacPrefix;
        case VENDOR_OSRAM:
        case VENDOR_OSRAM_STACK:
            return prefix == osramMacPrefix ||
                   prefix == heimanMacPrefix;
        case VENDOR_OWON:
            return prefix == davicomMacPrefix;
        case VENDOR_PHILIPS:
            return prefix == philipsMacPrefix;
        case VENDOR_PLUGWISE_BV:
            return prefix == emberMacPrefix;
        case VENDOR_PHYSICAL:
            return prefix == stMacPrefix;
        case VENDOR_SENGLED_OPTOELEC:
            return prefix == zhejiangMacPrefix;
        case VENDOR_SERCOMM:
            return prefix == emberMacPrefix ||
                   prefix == energyMiMacPrefix;
        case VENDOR_SI_LABS:
            return prefix == silabsMacPrefix ||
                   prefix == energyMiMacPrefix ||
                   prefix == ikeaMacPrefix; // belongs to SiLabs
        case VENDOR_STELPRO:
            return prefix == xalMacPrefix;
        case VENDOR_UBISYS:
            return prefix == ubisysMacPrefix;
        case VENDOR_UNIVERSAL2:
            return prefix == emberMacPrefix;
        case VENDOR_VISONIC:
            return prefix == emberMacPrefix;
        case VENDOR_XAL:
            return prefix == xalMacPrefix;
        case VENDOR_SAMJIN:
            return prefix == samjinMacPrefix;
        case VENDOR_DEVELCO:
            return prefix == develcoMacPrefix;
        case VENDOR_LEGRAND:
            return prefix == legrandMacPrefix;
        case VENDOR_PROFALUX:
            return prefix == profaluxMacPrefix;
        case VENDOR_NETVOX:
            return prefix == netvoxMacPrefix;
        case VENDOR_AURORA:
            return prefix == jennicMacPrefix;
        case VENDOR_COMPUTIME:
            return prefix == computimeMacPrefix;
        case VENDOR_DANALOCK:
            return prefix == danalockMacPrefix;
        case VENDOR_AXIS:
        case VENDOR_MMB:
            return prefix == zenMacPrefix;
        case VENDOR_SCHLAGE:
            return prefix == schlageMacPrefix;
        case VENDOR_ADUROLIGHT:
	        return prefix == jennicMacPrefix;
        default:
            return false;
    }
}

inline bool existDevicesWithVendorCodeForMacPrefix(const deCONZ::Address &addr, quint16 vendor)
{
    return existDevicesWithVendorCodeForMacPrefix(addr.ext(), vendor);
}

inline bool checkMacAndVendor(const deCONZ::Node *node, quint16 vendor)
{
    return node->nodeDescriptor().manufacturerCode() == vendor && existDevicesWithVendorCodeForMacPrefix(node->address(), vendor);
}

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
class Gateway;
class GatewayScanner;
class QUdpSocket;
class QTcpSocket;
class DeRestPlugin;
class QHostInfo;
class QNetworkReply;
class QNetworkAccessManager;
class QProcess;
class PollManager;
class RestDevices;

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
        status(QLatin1String("enabled")),
        activation(QLatin1String("start")),
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
    /*! Localtime is given in ISO 8601:2004 format: YYYY-MM-DDTHH:mm:ss. (Optional) */
    QString localtime;
    /*! UTC time that the timer was started. Only provided for timers. */
    QString starttime;
    /*! status of schedule (enabled or disabled). */
    QString status;
    /*! should activation of schedule start or end at given time (if a fading time is given) (start or end). */
    QString activation;
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
    QDateTime endtime; /*! Localtime of timeout: for timers only. */
    /*! Timeout in seconds. */
    int timeout;
    /*! Current timeout counting down to ::timeout. */
    int currentTimeout;
};

enum TaskType
{
    TaskIdentify = 0,
    TaskGetHue = 1,
    TaskSetHue = 2,
    TaskSetEnhancedHue = 3,
    TaskSetHueAndSaturation = 4,
    TaskSetXyColor = 5,
    TaskSetColorTemperature = 6,
    TaskGetColor = 7,
    TaskGetSat = 8,
    TaskSetSat = 9,
    TaskGetLevel = 10,
    TaskSetLevel = 11,
    TaskIncColorTemperature = 12,
    TaskStopLevel = 13,
    TaskSendOnOffToggle = 14,
    TaskMoveLevel = 15,
    TaskGetOnOff = 16,
    TaskSetColorLoop = 17,
    TaskGetColorLoop = 18,
    TaskReadAttributes = 19,
    TaskWriteAttribute = 20,
    TaskGetGroupMembership = 21,
    TaskGetGroupIdentifiers = 22,
    TaskGetSceneMembership = 23,
    TaskStoreScene = 24,
    TaskCallScene = 25,
    TaskViewScene = 26,
    TaskAddScene = 27,
    TaskRemoveScene = 28,
    TaskRemoveAllScenes = 29,
    TaskAddToGroup = 30,
    TaskRemoveFromGroup = 31,
    TaskViewGroup = 32,
    TaskTriggerEffect = 33,
    TaskWarning = 34,
    TaskIncBrightness = 35,
    TaskWindowCovering = 36,
    TaskThermostat = 37,
    // Danalock support
    TaskDoorLock = 38,
    TaskDoorUnlock = 39,
    TaskSyncTime = 40,
    TaskTuyaRequest = 41,
    TaskXmasLightStrip = 42
};

enum XmasLightStripMode
{
    ModeWhite = 0,
    ModeColour = 1,
    ModeEffect = 2
};

enum XmasLightStripEffect
{
    EffectSteady = 0x00,
    EffectSnow = 0x01,
    EffectRainbow = 0x02,
    EffectSnake = 0x03,
    EffectTwinkle = 0x04,
    EffectFireworks = 0x05,
    EffectFlag = 0x06,
    EffectWaves = 0x07,
    EffectUpdown = 0x08,
    EffectVintage = 0x09,
    EffectFading = 0x0a,
    EffectCollide = 0x0b,
    EffectStrobe = 0x0c,
    EffectSparkles = 0x0d,
    EffectCarnaval = 0x0e,
    EffectGlow = 0x0f
};

struct TaskItem
{
    TaskItem()
    {
        taskId = _taskCounter++;
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
        onTime = 0;
        sendTime = 0;
        ordered = false;
    }

    TaskType taskType;
    int taskId;
    deCONZ::ApsDataRequest req;
    deCONZ::ZclFrame zclFrame;
    uint8_t zclSeq;
    bool ordered; // won't be send until al prior taskIds are send
    int sendTime; // copy of idleTotalCounter
    bool confirmed;
    bool onOff;
    bool colorLoop;
    qreal hueReal;
    uint16_t identifyTime;
    uint8_t effectIdentifier;
    uint8_t options;
    uint16_t duration;
    uint8_t hue;
    uint8_t sat;
    uint8_t level;
    uint16_t enhancedHue;
    uint16_t colorX;
    uint16_t colorY;
    uint16_t colorTemperature;
    uint16_t groupId;
    uint8_t sceneId;
    qint32 inc; // bri_inc, hue_inc, sat_inc, ct_inc
    QString etag;
    uint16_t transitionTime;
    uint16_t onTime;
    QTcpSocket *client;

    bool autoMode; // true then this is a automode task
    deCONZ::Node *node;
    LightNode *lightNode;
    deCONZ::ZclCluster *cluster;

private:
    static int _taskCounter;
};

/*! \class ApiAuth

    Helper to combine serval authorisation parameters.
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
    void setDeviceType(const QString &devtype);

    bool needSaveDatabase;
    State state;
    QString apikey; // also called username (10..32 chars)
    QString devicetype;
    QDateTime createDate;
    QDateTime lastUseDate;
    QString useragent;
};

enum ApiVersion
{
    ApiVersion_1,        //!< common version 1.0
    ApiVersion_1_DDEL,   //!< version 1.0, "Accept: application/vnd.ddel.v1"
    ApiVersion_1_1_DDEL, //!< version 1.1, "Accept: application/vnd.ddel.v1.1"
    ApiVersion_2_DDEL,   //!< version 2.0, "Accept: application/vnd.ddel.v2"
};

enum ApiAuthorisation
{
    ApiAuthNone,
    ApiAuthLocal,
    ApiAuthInternal,
    ApiAuthFull
};

enum ApiMode
{
    ApiModeNormal,
    ApiModeStrict,
    ApiModeEcho,
    ApiModeHue
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
    ApiAuthorisation auth;
    ApiMode mode;
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

/*! \class ApiConfig

    Provide config to the resource system.
 */
class ApiConfig : public Resource
{
public:
    ApiConfig();
};

class TcpClient
{
public:
    QHttpRequestHeader hdr;
    QDateTime created;
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

    // REST API authorisation
    void initAuthentication();
    bool allowedToCreateApikey(const ApiRequest &req, ApiResponse &rsp, QVariantMap &map);
    void authorise(ApiRequest &req, ApiResponse &rsp);
    QString encryptString(const QString &str);

    // REST API gateways
    int handleGatewaysApi(const ApiRequest &req, ApiResponse &rsp);
    int getAllGateways(const ApiRequest &req, ApiResponse &rsp);
    int getGatewayState(const ApiRequest &req, ApiResponse &rsp);
    int setGatewayState(const ApiRequest &req, ApiResponse &rsp);
    int addCascadeGroup(const ApiRequest &req, ApiResponse &rsp);
    int deleteCascadeGroup(const ApiRequest &req, ApiResponse &rsp);
    void gatewayToMap(const ApiRequest &req, const Gateway *gw, QVariantMap &map);

    // REST API configuration
    void initConfig();
    int handleConfigBasicApi(const ApiRequest &req, ApiResponse &rsp);
    int handleConfigLocalApi(const ApiRequest &req, ApiResponse &rsp);
    int handleConfigFullApi(const ApiRequest &req, ApiResponse &rsp);
    int createUser(const ApiRequest &req, ApiResponse &rsp);
    int getFullState(const ApiRequest &req, ApiResponse &rsp);
    int getConfig(const ApiRequest &req, ApiResponse &rsp);
    int getBasicConfig(const ApiRequest &req, ApiResponse &rsp);
    int getZigbeeConfig(const ApiRequest &req, ApiResponse &rsp);
    int putZigbeeConfig(const ApiRequest &req, ApiResponse &rsp);
    int getChallenge(const ApiRequest &req, ApiResponse &rsp);
    int modifyConfig(const ApiRequest &req, ApiResponse &rsp);
    int deleteUser(const ApiRequest &req, ApiResponse &rsp);
    int updateSoftware(const ApiRequest &req, ApiResponse &rsp);
    int restartGateway(const ApiRequest &req, ApiResponse &rsp);
    int restartApp(const ApiRequest &req, ApiResponse &rsp);
    int shutDownGateway(const ApiRequest &req, ApiResponse &rsp);
    int updateFirmware(const ApiRequest &req, ApiResponse &rsp);
    int exportConfig(const ApiRequest &req, ApiResponse &rsp);
    int importConfig(const ApiRequest &req, ApiResponse &rsp);
    int resetConfig(const ApiRequest &req, ApiResponse &rsp);
    int changePassword(const ApiRequest &req, ApiResponse &rsp);
    int deletePassword(const ApiRequest &req, ApiResponse &rsp);
    int getWifiState(const ApiRequest &req, ApiResponse &rsp);
    int configureWifi(const ApiRequest &req, ApiResponse &rsp);
    int restoreWifiConfig(const ApiRequest &req, ApiResponse &rsp);
    int putWifiScanResult(const ApiRequest &req, ApiResponse &rsp);
    int putWifiUpdated(const ApiRequest &req, ApiResponse &rsp);
    int putHomebridgeUpdated(const ApiRequest &req, ApiResponse &rsp);

    void configToMap(const ApiRequest &req, QVariantMap &map);
    void basicConfigToMap(QVariantMap &map);

    // REST API userparameter
    int handleUserparameterApi(const ApiRequest &req, ApiResponse &rsp);
    int createUserParameter(const ApiRequest &req, ApiResponse &rsp);
    int addUserParameter(const ApiRequest &req, ApiResponse &rsp);
    int modifyUserParameter(const ApiRequest &req, ApiResponse &rsp);
    int getUserParameter(const ApiRequest &req, ApiResponse &rsp);
    int getAllUserParameter(const ApiRequest &req, ApiResponse &rsp);
    int deleteUserParameter(const ApiRequest &req, ApiResponse &rsp);

    // REST API lights
    int handleLightsApi(const ApiRequest &req, ApiResponse &rsp);
    int getAllLights(const ApiRequest &req, ApiResponse &rsp);
    int searchNewLights(const ApiRequest &req, ApiResponse &rsp);
    int getNewLights(const ApiRequest &req, ApiResponse &rsp);
    int getLightData(const ApiRequest &req, ApiResponse &rsp);
    int getLightState(const ApiRequest &req, ApiResponse &rsp);
    int setLightState(const ApiRequest &req, ApiResponse &rsp);
    int setWindowCoveringState(const ApiRequest &req, ApiResponse &rsp, TaskItem &taskRef, QVariantMap &map);
    int setWarningDeviceState(const ApiRequest &req, ApiResponse &rsp, TaskItem &taskRef, QVariantMap &map);
    int setTuyaDeviceState(const ApiRequest &req, ApiResponse &rsp, TaskItem &taskRef, QVariantMap &map);
    int setLightAttributes(const ApiRequest &req, ApiResponse &rsp);
    int deleteLight(const ApiRequest &req, ApiResponse &rsp);
    int removeAllScenes(const ApiRequest &req, ApiResponse &rsp);
    int removeAllGroups(const ApiRequest &req, ApiResponse &rsp);
    int getConnectivity(const ApiRequest &req, ApiResponse &rsp, bool alt);
    void handleLightEvent(const Event &e);

    bool lightToMap(const ApiRequest &req, const LightNode *webNode, QVariantMap &map);

    // REST API groups
    int handleGroupsApi(const ApiRequest &req, ApiResponse &rsp);
    int getAllGroups(const ApiRequest &req, ApiResponse &rsp);
    int createGroup(const ApiRequest &req, ApiResponse &rsp);
    int getGroupAttributes(const ApiRequest &req, ApiResponse &rsp);
    int setGroupAttributes(const ApiRequest &req, ApiResponse &rsp);
    int setGroupState(const ApiRequest &req, ApiResponse &rsp);
    int deleteGroup(const ApiRequest &req, ApiResponse &rsp);
    void handleGroupEvent(const Event &e);
    Group *addGroup();

    // REST API groups > scenes
    int createScene(const ApiRequest &req, ApiResponse &rsp);
    int getAllScenes(const ApiRequest &req, ApiResponse &rsp);
    int getSceneAttributes(const ApiRequest &req, ApiResponse &rsp);
    int setSceneAttributes(const ApiRequest &req, ApiResponse &rsp);
    int storeScene(const ApiRequest &req, ApiResponse &rsp);
    int recallScene(const ApiRequest &req, ApiResponse &rsp);
    int modifyScene(const ApiRequest &req, ApiResponse &rsp);
    int deleteScene(const ApiRequest &req, ApiResponse &rsp);

    bool groupToMap(const ApiRequest &req, const Group *group, QVariantMap &map);

    // REST API schedules
    void initSchedules();
    int handleSchedulesApi(const ApiRequest &req, ApiResponse &rsp);
    int getAllSchedules(const ApiRequest &req, ApiResponse &rsp);
    int createSchedule(const ApiRequest &req, ApiResponse &rsp);
    int getScheduleAttributes(const ApiRequest &req, ApiResponse &rsp);
    int setScheduleAttributes(const ApiRequest &req, ApiResponse &rsp);
    int deleteSchedule(const ApiRequest &req, ApiResponse &rsp);
    bool jsonToSchedule(const QString &jsonString, Schedule &schedule, ApiResponse *rsp);

    // REST API touchlink
    void initTouchlinkApi();
    int handleTouchlinkApi(const ApiRequest &req, ApiResponse &rsp);
    int touchlinkScan(const ApiRequest &req, ApiResponse &rsp);
    int getTouchlinkScanResults(const ApiRequest &req, ApiResponse &rsp);
    int identifyLight(const ApiRequest &req, ApiResponse &rsp);
    int resetLight(const ApiRequest &req, ApiResponse &rsp);

    // REST API sensors
    int handleSensorsApi(const ApiRequest &req, ApiResponse &rsp);
    int getAllSensors(const ApiRequest &req, ApiResponse &rsp);
    int getSensor(const ApiRequest &req, ApiResponse &rsp);
    int getSensorData(const ApiRequest &req, ApiResponse &rsp);
    int searchNewSensors(const ApiRequest &req, ApiResponse &rsp);
    int getNewSensors(const ApiRequest &req, ApiResponse &rsp);
    int updateSensor(const ApiRequest &req, ApiResponse &rsp);
    int deleteSensor(const ApiRequest &req, ApiResponse &rsp);
    int changeSensorConfig(const ApiRequest &req, ApiResponse &rsp);
    int changeSensorState(const ApiRequest &req, ApiResponse &rsp);
    int changeThermostatSchedule(const ApiRequest &req, ApiResponse &rsp);
    int createSensor(const ApiRequest &req, ApiResponse &rsp);
    int getGroupIdentifiers(const ApiRequest &req, ApiResponse &rsp);
    int recoverSensor(const ApiRequest &req, ApiResponse &rsp);
    bool sensorToMap(const Sensor *sensor, QVariantMap &map, const ApiRequest &req);
    void handleSensorEvent(const Event &e);

    // REST API resourcelinks
    int handleResourcelinksApi(const ApiRequest &req, ApiResponse &rsp);
    int getAllResourcelinks(const ApiRequest &req, ApiResponse &rsp);
    int getResourcelinks(const ApiRequest &req, ApiResponse &rsp);
    int createResourcelinks(const ApiRequest &req, ApiResponse &rsp);
    int updateResourcelinks(const ApiRequest &req, ApiResponse &rsp);
    int deleteResourcelinks(const ApiRequest &req, ApiResponse &rsp);

    // REST API rules
    int handleRulesApi(const ApiRequest &req, ApiResponse &rsp);
    int getAllRules(const ApiRequest &req, ApiResponse &rsp);
    int getRule(const ApiRequest &req, ApiResponse &rsp);
    int createRule(const ApiRequest &req, ApiResponse &rsp);
    int updateRule(const ApiRequest &req, ApiResponse &rsp);
    int deleteRule(const ApiRequest &req, ApiResponse &rsp);
    void queueCheckRuleBindings(const Rule &rule);
    bool evaluateRule(Rule &rule, const Event &e, Resource *eResource, ResourceItem *eItem, QDateTime now, QDateTime previousNow);
    void indexRuleTriggers(Rule &rule);
    void triggerRule(Rule &rule);
    bool ruleToMap(const Rule *rule, QVariantMap &map);
    int handleWebHook(const RuleAction &action);

    bool checkActions(QVariantList actionsList, ApiResponse &rsp);
    bool checkConditions(QVariantList conditionsList, ApiResponse &rsp);

    // REST API scenes
    int handleScenesApi(const ApiRequest &req, ApiResponse &rsp);

    // REST API info
    int handleInfoApi(const ApiRequest &req, ApiResponse &rsp);
    int getInfoTimezones(const ApiRequest &req, ApiResponse &rsp);

    // REST API capabilities
    int handleCapabilitiesApi(const ApiRequest &req, ApiResponse &rsp);
    int getCapabilities(const ApiRequest &req, ApiResponse &rsp);

    // REST API common
    QVariantMap errorToMap(int id, const QString &ressource, const QString &description);

    // UPNP discovery
    void initUpnpDiscovery();
    void initDescriptionXml();
    // Internet discovery
    void initInternetDicovery();
    bool setInternetDiscoveryInterval(int minutes);
    // Permit join
    void initPermitJoin();
    bool setPermitJoinDuration(uint8_t duration);

    // Otau
    void initOtau();
    void otauDataIndication(const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame);
    void otauSendStdNotify(LightNode *node);
    bool isOtauBusy();
    bool isOtauActive();
    int otauLastBusyTimeDelta() const;

    //Channel Change
    void initChangeChannelApi();
    bool startChannelChange(quint8 channel);

    //reset Device
    void initResetDeviceApi();

    //Timezone
    // std::string getTimezone();
    QVariantList getTimezones();

    //Export/Import/Reset Configuration
    bool exportConfiguration();
    bool importConfiguration();
    bool resetConfiguration(bool resetGW, bool deleteDB);

public Q_SLOTS:
    Resource *getResource(const char *resource, const QString &id = QString());
    void announceUpnp();
    void upnpReadyRead();
    void apsdeDataIndication(const deCONZ::ApsDataIndication &ind);
    void apsdeDataConfirm(const deCONZ::ApsDataConfirm &conf);
    void gpDataIndication(const deCONZ::GpDataIndication &ind);
    void gpProcessButtonEvent(const deCONZ::GpDataIndication &ind);
    void configurationChanged();
    void networkStateChangeRequest(bool shouldConnect);
    int taskCountForAddress(const deCONZ::Address &address);
    void processTasks();
    void processGroupTasks();
    void nodeEvent(const deCONZ::NodeEvent &event);
    void initTimezone();
    void initNetworkInfo();
    void initWiFi();
    void internetDiscoveryTimerFired();
    void internetDiscoveryFinishedRequest(QNetworkReply *reply);
    void internetDiscoveryExtractVersionInfo(QNetworkReply *reply);
    void internetDiscoveryExtractGeo(QNetworkReply *reply);
    void inetProxyHostLookupDone(const QHostInfo &host);
    void inetProxyCheckHttpVia(const QString &via);
    void scheduleTimerFired();
    void permitJoinTimerFired();
    void resendPermitJoinTimerFired();
    void otauTimerFired();
    void lockGatewayTimerFired();
    void openClientTimerFired();
    void clientSocketDestroyed();
    void saveDatabaseTimerFired();
    void userActivity();
    bool sendBindRequest(BindingTask &bt);
    bool sendConfigureReportingRequest(BindingTask &bt, const std::vector<ConfigureReportingRequest> &requests);
    bool sendConfigureReportingRequest(BindingTask &bt);
    void checkLightBindingsForAttributeReporting(LightNode *lightNode);
    bool checkPollControlClusterTask(Sensor *sensor);
    bool checkSensorBindingsForAttributeReporting(Sensor *sensor);
    bool checkSensorBindingsForClientClusters(Sensor *sensor);
    void checkSensorGroup(Sensor *sensor);
    void checkOldSensorGroups(Sensor *sensor);
    void deleteGroupsWithDeviceMembership(const QString &id);
    void processUbisysBinding(Sensor *sensor, const Binding &bnd);
    void processUbisysC4Configuration(Sensor *sensor);
    void bindingTimerFired();
    void bindingToRuleTimerFired();
    void bindingTableReaderTimerFired();
    void verifyRuleBindingsTimerFired();
    void indexRulesTriggers();
    void fastRuleCheckTimerFired();
    void webhookFinishedRequest(QNetworkReply *reply);
    void daylightTimerFired();
    bool checkDaylightSensorConfiguration(Sensor *sensor, const QString &gwBridgeId, double *lat, double *lng);
    size_t calcDaylightOffsets(Sensor *daylightSensor, size_t iter);
    void handleRuleEvent(const Event &e);
    bool queueBindingTask(const BindingTask &bindingTask);
    void restartAppTimerFired();
    void pollSwUpdateStateTimerFired();
    void pollDatabaseWifiTimerFired();
    void restartGatewayTimerFired();
    void shutDownGatewayTimerFired();
    void simpleRestartAppTimerFired();
    void pushSensorInfoToCore(Sensor *sensor);
    void pollNextDevice();

    // database
#if DECONZ_LIB_VERSION >= 0x010E00
    void storeSourceRoute(const deCONZ::SourceRoute &sourceRoute);
    void deleteSourceRoute(const QString &uuid);
    void restoreSourceRoutes();
#endif

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
    void networkWatchdogTimerFired();

    // generic reconnect network
    void reconnectTimerFired();
    void genericDisconnectNetwork();
    void checkNetworkDisconnected();
    void startReconnectNetwork(int delay);
    void reconnectNetwork();

    //reset device
    void resetDeviceTimerFired();
    void checkResetState();
    void resetDeviceSendConfirm(bool success);

    // lights
    void startSearchLights();
    void searchLightsTimerFired();

    // sensors
    void startSearchSensors();
    void searchSensorsTimerFired();
    void checkInstaModelId(Sensor *sensor);
    void delayedFastEnddeviceProbe(const deCONZ::NodeEvent *event = nullptr);
    void checkSensorStateTimerFired();

    // events
    void initEventQueue();
    void eventQueueTimerFired();
    void enqueueEvent(const Event &event);

    // firmware update
    void initFirmwareUpdate();
    void firmwareUpdateTimerFired();
    void checkFirmwareDevices();
    void queryFirmwareVersion();
    void updateFirmwareDisconnectDevice();
    void updateFirmware();
    void updateFirmwareWaitFinished();
    bool startUpdateFirmware();

    //wifi settings
    int scanWifiNetworks(const ApiRequest &req, ApiResponse &rsp);
    void wifiPageActiveTimerFired();

    //homebridge
    int resetHomebridge(const ApiRequest &req, ApiResponse &rsp);

    // time manager
    void timeManagerTimerFired();
    void ntpqFinished();

    // gateways
    void foundGateway(const QHostAddress &host, quint16 port, const QString &uuid, const QString &name);

    // window covering
    void calibrateWindowCoveringNextStep();

    // thermostat
    void addTaskThermostatGetScheduleTimer();

public:
    void checkRfConnectState();
    bool isInNetwork();
    void generateGatewayUuid();
    void updateEtag(QString &etag);
    qint64 getUptime();
    void handleMacDataRequest(const deCONZ::NodeEvent &event);
    void addLightNode(const deCONZ::Node *node);
    void setLightNodeStaticCapabilities(LightNode *lightNode);
    void updatedLightNodeEndpoint(const deCONZ::NodeEvent &event);
    void nodeZombieStateChanged(const deCONZ::Node *node);
    LightNode *updateLightNode(const deCONZ::NodeEvent &event);
    LightNode *getLightNodeForAddress(const deCONZ::Address &addr, quint8 endpoint = 0);
    int getNumberOfEndpoints(quint64 extAddr);
    LightNode *getLightNodeForId(const QString &id);
    Rule *getRuleForId(const QString &id);
    Rule *getRuleForName(const QString &name);
    void addSensorNode(const deCONZ::Node *node, const deCONZ::NodeEvent *event = 0);
    void addSensorNode(const deCONZ::Node *node, const SensorFingerprint &fingerPrint, const QString &type, const QString &modelId, const QString &manufacturer);
    void checkUpdatedFingerPrint(const deCONZ::Node *node, quint8 endpoint, Sensor *sensorNode);
    void checkSensorNodeReachable(Sensor *sensor, const deCONZ::NodeEvent *event = 0);
    void checkSensorButtonEvent(Sensor *sensor, const deCONZ::ApsDataIndication &ind, const deCONZ::ZclFrame &zclFrame);
    void updateSensorNode(const deCONZ::NodeEvent &event);
    void updateSensorLightLevel(Sensor &sensor, quint16 measuredValue);
    bool isDeviceSupported(const deCONZ::Node *node, const QString &modelId);
    Sensor *getSensorNodeForAddressAndEndpoint(const deCONZ::Address &addr, quint8 ep, const QString &type);
    Sensor *getSensorNodeForAddressAndEndpoint(const deCONZ::Address &addr, quint8 ep);
    Sensor *getSensorNodeForAddress(quint64 extAddr);
    Sensor *getSensorNodeForAddress(const deCONZ::Address &addr);
    Sensor *getSensorNodeForFingerPrint(quint64 extAddr, const SensorFingerprint &fingerPrint, const QString &type);
    Sensor *getSensorNodeForUniqueId(const QString &uniqueId);
    Sensor *getSensorNodeForId(const QString &id);
    Group *getGroupForName(const QString &name);
    Group *getGroupForId(uint16_t id);
    Group *getGroupForId(const QString &id);
    bool deleteOldGroupOfSwitch(Sensor *sensor, quint16 newGroupId);
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
    bool readAttributes(RestNodeBase *restNode, quint8 endpoint, uint16_t clusterId, const std::vector<uint16_t> &attributes, uint16_t manufacturerCode = 0);
    bool writeAttribute(RestNodeBase *restNode, quint8 endpoint, uint16_t clusterId, const deCONZ::ZclAttribute &attribute, uint16_t manufacturerCode = 0);
    bool readSceneAttributes(LightNode *lightNode, uint16_t groupId, uint8_t sceneId);
    bool readGroupMembership(LightNode *lightNode, const std::vector<uint16_t> &groups);
    void foundGroupMembership(LightNode *lightNode, uint16_t groupId);
    void foundGroup(uint16_t groupId);
    bool isLightNodeInGroup(const LightNode *lightNode, uint16_t groupId) const;
    void deleteLightFromScenes(QString lightId, uint16_t groupId);
//    void readAllInGroup(Group *group);
    void setAttributeOnOffGroup(Group *group, uint8_t onOff);
    bool readSceneMembership(LightNode *lightNode, Group *group);
    void foundScene(LightNode *lightNode, Group *group, uint8_t sceneId);
    void setSceneName(Group *group, uint8_t sceneId, const QString &name);
    bool storeScene(Group *group, uint8_t sceneId);
    bool modifyScene(Group *group, uint8_t sceneId);
    bool removeScene(Group *group, uint8_t sceneId);
    bool callScene(Group *group, uint8_t sceneId);
    bool removeAllScenes(Group *group);
    void storeRecoverOnOffBri(LightNode *lightNode);
    bool flsNbMaintenance(LightNode *lightNode);
    bool pushState(QString json, QTcpSocket *sock);
    void patchNodeDescriptor(const deCONZ::ApsDataIndication &ind);
    void writeIasCieAddress(Sensor*);
    void checkIasEnrollmentStatus(Sensor*);

    void pushClientForClose(QTcpSocket *sock, int closeTimeout, const QHttpRequestHeader &hdr);

    uint8_t endpoint();
    QString generateUniqueId(quint64 extAddress, quint8 endpoint, quint16 clusterId);

    // Task interface
    bool addTask(const TaskItem &task);
    bool addTaskMoveLevel(TaskItem &task, bool withOnOff, bool upDirection, quint8 rate);
    bool addTaskSetOnOff(TaskItem &task, quint8 cmd, quint16 ontime, quint8 flags = 0);
    bool addTaskSetBrightness(TaskItem &task, uint8_t bri, bool withOnOff);
    bool addTaskIncColorTemperature(TaskItem &task, int32_t ct);
    bool addTaskIncBrightness(TaskItem &task, int16_t bri);
    bool addTaskStopBrightness(TaskItem &task);
    bool addTaskSetColorTemperature(TaskItem &task, uint16_t ct);
    bool addTaskSetEnhancedHue(TaskItem &task, uint16_t hue);
    bool addTaskSetSaturation(TaskItem &task, uint8_t sat);
    bool addTaskSetHueAndSaturation(TaskItem &task, uint8_t hue, uint8_t sat);
    bool addTaskSetXyColorAsHueAndSaturation(TaskItem &task, double x, double y);
    bool addTaskSetXyColor(TaskItem &task, double x, double y);
    bool addTaskSetColorLoop(TaskItem &task, bool colorLoopActive, uint8_t speed);
    bool addTaskIdentify(TaskItem &task, uint16_t identifyTime);
    bool addTaskTriggerEffect(TaskItem &task, uint8_t effectIdentifier);
    bool addTaskWarning(TaskItem &task, uint8_t options, uint16_t duration);
    // Danalock support. To control the lock from the REST API, you need to create a new routine addTaskDoorLock() in zcl_tasks.cpp, cf. the addTaskWarning() I created to control the Siren.
    bool addTaskDoorLockUnlock(TaskItem &task, uint8_t cmd);
    bool addTaskAddToGroup(TaskItem &task, uint16_t groupId);
    bool addTaskViewGroup(TaskItem &task, uint16_t groupId);
    bool addTaskRemoveFromGroup(TaskItem &task, uint16_t groupId);
    bool addTaskStoreScene(TaskItem &task, uint16_t groupId, uint8_t sceneId);
    bool addTaskAddEmptyScene(TaskItem &task, quint16 groupId, quint8 sceneId, quint16 transitionTime);
    bool addTaskAddScene(TaskItem &task, uint16_t groupId, uint8_t sceneId, const QString &lightId);
    bool addTaskRemoveScene(TaskItem &task, uint16_t groupId, uint8_t sceneId);
    bool addTaskWindowCovering(TaskItem &task, uint8_t cmdId, uint16_t pos, uint8_t pct);
    bool addTaskWindowCoveringSetAttr(TaskItem &task, uint16_t mfrCode, uint16_t attrId, uint8_t attrType, uint16_t attrValue);
    bool addTaskWindowCoveringCalibrate(TaskItem &task, int WindowCoveringType);
    bool addTaskUbisysConfigureSwitch(TaskItem &taskRef);
    bool addTaskThermostatCmd(TaskItem &task, uint16_t mfrCode, uint8_t cmd, int16_t setpoint, uint8_t daysToReturn);
    bool addTaskThermostatGetSchedule(TaskItem &task);
    bool addTaskThermostatSetWeeklySchedule(TaskItem &task, quint8 weekdays, const QString &transitions);
    void updateThermostatSchedule(Sensor *sensor, quint8 newWeekdays, QString &transitions);
    bool addTaskThermostatReadWriteAttribute(TaskItem &task, uint8_t readOrWriteCmd, uint16_t mfrCode, uint16_t attrId, uint8_t attrType, uint32_t attrValue);
    bool addTaskThermostatWriteAttributeList(TaskItem &task, uint16_t mfrCode, QMap<quint16, quint32> &AttributeList );
    bool addTaskControlModeCmd(TaskItem &task, uint8_t cmdId, int8_t mode);
    bool addTaskSyncTime(Sensor *sensor);
    bool addTaskThermostatUiConfigurationReadWriteAttribute(TaskItem &task, uint8_t readOrWriteCmd, uint16_t attrId, uint8_t attrType, uint32_t attrValue, uint16_t mfrCode=0);
    bool addTaskFanControlReadWriteAttribute(TaskItem &task, uint8_t readOrWriteCmd, uint16_t attrId, uint8_t attrType, uint32_t attrValue, uint16_t mfrCode=0);

    // Merry Christmas!
    bool isXmasLightStrip(LightNode *lightNode);
    bool addTaskXmasLightStripOn(TaskItem &task, bool on);
    bool addTaskXmasLightStripMode(TaskItem &task, XmasLightStripMode mode);
    bool addTaskXmasLightStripWhite(TaskItem &task, quint8 bri);
    bool addTaskXmasLightStripColour(TaskItem &task, quint16 hue, quint8 sat, quint8 bri);
    bool addTaskXmasLightStripEffect(TaskItem &task, XmasLightStripEffect effect, quint8 speed, QList<QList<quint8>> &colours);
    int setXmasLightStripState(const ApiRequest &req, ApiResponse &rsp, TaskItem &taskRef, QVariantMap &map);

    void handleGroupClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame);
    void handleSceneClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame);
    void handleOnOffClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame);
    void handleClusterIndicationGateways(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame);
    void handleIasZoneClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame);
    void sendIasZoneEnrollResponse(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame);
    void handleIndicationSearchSensors(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame);
    bool SendTuyaRequest(TaskItem &task, TaskType taskType , qint8 Dp_type, qint8 Dp_identifier , QByteArray data );
    bool SendTuyaCommand( const deCONZ::ApsDataIndication &ind, qint8 command, QByteArray data );
    void handleCommissioningClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame);
    bool SendTuyaRequestThermostatSetWeeklySchedule(TaskItem &taskRef, quint8 weekdays , QString transitions , qint8 Dp_identifier);
    void handleZdpIndication(const deCONZ::ApsDataIndication &ind);
    bool handleMgmtBindRspConfirm(const deCONZ::ApsDataConfirm &conf);
    void handleDeviceAnnceIndication(const deCONZ::ApsDataIndication &ind);
    void handleNodeDescriptorResponseIndication(const deCONZ::ApsDataIndication &ind);
    void handleIeeeAddressReqIndication(const deCONZ::ApsDataIndication &ind);
    void handleNwkAddressReqIndication(const deCONZ::ApsDataIndication &ind);
    void handleMgmtBindRspIndication(const deCONZ::ApsDataIndication &ind);
    void handleBindAndUnbindRspIndication(const deCONZ::ApsDataIndication &ind);
    void handleMgmtLeaveRspIndication(const deCONZ::ApsDataIndication &ind);
    void handleMgmtLqiRspIndication(const deCONZ::ApsDataIndication &ind);
    void handleDEClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame);
    void handleXalClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame);
    void handleWindowCoveringClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame);
    void handlePollControlIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame);
    // Danalock support
    void handleDoorLockClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame);
    void handleThermostatClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame);
    void handleThermostatUiConfigurationClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame);
    void handleAirQualityClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame);
    void handleTimeClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame);
    void handleDiagnosticsClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame);
    void handleFanControlClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame);
    void handleIdentifyClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame);
    void sendTimeClusterResponse(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame);
    void handleBasicClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame);
    void sendBasicClusterResponse(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame);
    void handlePhilipsClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame);
    void handleTuyaClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame);
    void handleZclAttributeReportIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame);
    void handleZclConfigureReportingResponseIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame);
    void sendZclDefaultResponse(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame, quint8 status);
    void taskToLocalData(const TaskItem &task);
    void handleZclAttributeReportIndicationXiaomiSpecial(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame);
    void queuePollNode(RestNodeBase *node);
    void handleApplianceAlertClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame);
    bool serialiseThermostatTransitions(const QVariantList &transitions, QString *s);
    bool deserialiseThermostatTransitions(const QString &s, QVariantList *transitions);
    bool serialiseThermostatSchedule(const QVariantMap &schedule, QString *s);
    bool deserialiseThermostatSchedule(const QString &s, QVariantMap *schedule);

    // Modify node attributes
    void setAttributeOnOff(LightNode *lightNode);
    void setAttributeLevel(LightNode *lightNode);
    void setAttributeEnhancedHue(LightNode *lightNode);
    void setAttributeSaturation(LightNode *lightNode);
    void setAttributeColorXy(LightNode *lightNode);
    void setAttributeColorTemperature(LightNode *lightNode);
    void setAttributeColorLoopActive(LightNode *lightNode);

    // Etag helper
    void updateSensorEtag(Sensor *sensorNode);
    void updateLightEtag(LightNode *lightNode);
    void updateGroupEtag(Group *group);

    // Database interface
    void initDb();
    void checkDbUserVersion();
    void cleanUpDb();
    void createTempViews();
    int getDbPragmaInteger(const char *sql);
    bool setDbUserVersion(int userVersion);
    bool upgradeDbToUserVersion1();
    bool upgradeDbToUserVersion2();
    bool upgradeDbToUserVersion6();
    bool upgradeDbToUserVersion7();
    void refreshDeviceDb(const deCONZ::Address &addr);
    void pushZdpDescriptorDb(quint64 extAddress, quint8 endpoint, quint16 type, const QByteArray &data);
    void pushZclValueDb(quint64 extAddress, quint8 endpoint, quint16 clusterId, quint16 attributeId, qint64 data);
    void openDb();
    void readDb();
    void loadAuthFromDb();
    void loadConfigFromDb();
    void loadUserparameterFromDb();
    void loadAllGroupsFromDb();
    void loadAllResourcelinksFromDb();
    void loadAllScenesFromDb();
    void loadAllSchedulesFromDb();
    void loadLightNodeFromDb(LightNode *lightNode);
    QString loadDataForLightNodeFromDb(QString extAddress);
    void loadGroupFromDb(Group *group);
    void loadSceneFromDb(Scene *scene);
    void loadSwUpdateStateFromDb();
    void loadWifiInformationFromDb();
    void loadAllRulesFromDb();
    void loadAllSensorsFromDb();
    void loadSensorDataFromDb(Sensor *sensor, QVariantList &ls, qint64 fromTime, int max);
    void loadLightDataFromDb(LightNode *lightNode, QVariantList &ls, qint64 fromTime, int max);
    void loadAllGatewaysFromDb();
    int getFreeLightId();
    int getFreeSensorId();
    void saveDb();
    void saveApiKey(QString apikey);
    void closeDb();
    void queSaveDb(int items, int msec);
    void updateZigBeeConfigDb();
    void getLastZigBeeConfigDb(QString &out);
    void getZigbeeConfigDb(QVariantList &out);

    void checkConsistency();

    sqlite3 *db;
    int ttlDataBaseConnection; // when idleTotalCounter becomes greater the DB will be closed
    int saveDatabaseItems;
    int saveDatabaseIdleTotalCounter;
    QString sqliteDatabaseName;
    std::vector<int> lightIds;
    std::vector<int> sensorIds;
    std::vector<QString> dbQueryQueue;
    qint64 dbZclValueMaxAge;
    QTimer *databaseTimer;
    QString emptyString;

    // JSON support
    QMap<QString, std::vector<Sensor::ButtonMap>> buttonMapData;
    QMap<QString, quint16> btnMapClusters;
    QMap<QString, QMap<QString, quint16>> btnMapClusterCommands;
    QMap<QString, QString> buttonMapForModelId;

    // gateways
    std::vector<Gateway*> gateways;
    GatewayScanner *gwScanner;

    // authorisation
    QElapsedTimer apiAuthSaveDatabaseTime;
    size_t apiAuthCurrent;
    std::vector<ApiAuth> apiAuths;
    QString gwAdminUserName;
    QString gwAdminPasswordHash;

    struct SwUpdateState {
     QString noUpdate;
     QString readyToInstall;
     QString transferring;
     QString installing;
    } swUpdateState = {"noupdates","allreadytoinstall","transferring","installing"};

    // configuration
    bool gwLinkButton;
    bool gwWebSocketNotifyAll;  // include all attributes in websocket notification
    bool gwdisablePermitJoinAutoOff; // Stop the periodic verification for closed network
    bool gwRfConnectedExpected;  // the state which should be hold
    bool gwRfConnected;  // to detect changes
    int gwAnnounceInterval; // used by internet discovery [minutes]
    QString gwAnnounceUrl;
    int gwAnnounceVital; // 0 not tried, > 0 success attemps, < 0 failed attemps
    uint8_t gwPermitJoinDuration; // global permit join state (last set)
    int gwPermitJoinResend; // permit join of values > 255
    uint16_t gwNetworkOpenDuration; // user setting how long network remains open
    QString gwWifi;     // configured | not-configured | not-available | new-configured | deactivated
    QString gwWifiActive;
    uint gwWifiLastUpdated;
    QString gwWifiEth0;
    QString gwWifiWlan0;
    QVariantList gwWifiAvailable;
    enum WifiState {
        WifiStateInitMgmt,
        WifiStateIdle
    };
    WifiState gwWifiState;
    QString gwWifiStateString;
    quint32 gwWifiMgmt;
    QString gwWifiType; // accesspoint | ad-hoc | client
    QString gwWifiName;
    QString gwWifiBackupName;
    QString gwWifiWorkingType;
    QString gwWifiWorkingName;
    QString gwWifiWorkingPw;
    QString gwWifiWorkingPwEnc;
    QString gwWifiClientName;
    QString gwWifiChannel;
    QString gwWifiIp;
    QString gwWifiPw;
    QString gwWifiPwEnc;
    QString gwWifiBackupPw;
    QString gwWifiBackupPwEnc;
    QString gwWifiClientPw;
    //QString gwWifiApPw;
    pid_t gwWifiPID;
    QTimer *wifiPageActiveTimer;
    bool gwWifiPageActive;
    QString gwProxyAddress;
    quint16 gwProxyPort;
    QString gwTimezone;
    QString gwTimeFormat;
    QString gwMAC;
    QString gwIPAddress;
    uint16_t gwPort;
    bool gwAllowLocal;
    QString gwNetMask;
    QString gwHomebridge;
    QString gwHomebridgePin;
    QString gwHomebridgeVersion;
    QString gwHomebridgeUpdateVersion;
    bool gwHomebridgeUpdate;
    QString gwName;
    bool gwLANBridgeId;
    QString gwBridgeId;
    QString gwUuid;
    QString gwUpdateVersion;
    QString gwUpdateDate;
    QString gwSwUpdateState;
    QString gwRgbwDisplay;
    QString gwFirmwareVersion;
    QString gwFirmwareVersionUpdate; // for local update of the firmware if it doesn't fit the GW_MIN_<platform>_FW_VERSION
    bool gwFirmwareNeedUpdate;
    QString gwUpdateChannel;
    int gwGroupSendDelay;
    uint gwZigbeeChannel;
    uint16_t gwGroup0;
    QVariantMap gwConfig;
    QString gwSensorsEtag;
    QString gwLightsEtag;
    QString gwGroupsEtag;
    QString gwConfigEtag;
    QByteArray gwChallenge;
    QDateTime gwLastChallenge;
    bool gwRunFromShellScript;
    QString gwRunMode;
    bool gwDeleteUnknownRules;
    bool groupDeviceMembershipChecked;
    QVariantMap gwUserParameter;
    std::vector<QString> gwUserParameterToDelete;
    deCONZ::Address gwDeviceAddress;
    QString gwSdImageVersion;
    QDateTime globalLastMotion; // last time any physical PIR has detected motion
    QDateTime zbConfigGood; // timestamp incoming ZCL reports/read attribute responses are received, indication that network is operational

    // time manager
    enum TimeManagerState {
        TM_Init,
        TM_WaitNtpq,
        TM_NtpRunning
    };
    TimeManagerState timeManagerState;
    QProcess *ntpqProcess;

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
    QTimer *pollSwUpdateStateTimer;
    QTimer *pollDatabaseWifiTimer;
    int fwUpdateIdleTimeout;
    bool fwUpdateStartedByUser;
    FW_UpdateState fwUpdateState;
    QString fwUpdateFile;
    QProcess *fwProcess;
    QProcess *zipProcess;
    QProcess *archProcess;
    QStringList fwProcessArgs;
    QString fwDeviceName;

    // Helper to reference nodes in containers.
    // This is needed since the pointer might change due container resize / item removal.
    struct PollNodeItem
    {
        PollNodeItem(const QString &_uuid, const char *rt) :
        uuid(_uuid),
        resourceType(rt)
        { }
        bool operator==(const PollNodeItem &other) const
        {
            return resourceType == other.resourceType && uuid == other.uuid;
        }
        const QString uuid;
        const char* resourceType = nullptr; // back ref to the container RLights, RSensors
    };

    std::deque<PollNodeItem> pollNodes;
    PollManager *pollManager = nullptr;

    // upnp
    QByteArray descriptionXml;

    // gateway lock (link button)
    QTimer *lockGatewayTimer;

    // permit join
    // used by searchLights()
    QTimer *permitJoinTimer;
    QTime permitJoinLastSendTime;
    bool permitJoinFlag; // indicates that permitJoin changed from greater than 0 to 0
    QTimer *resendPermitJoinTimer;

    // schedules
    QTimer *scheduleTimer;
    std::vector<Schedule> schedules;

    // webhooks
    QNetworkAccessManager *webhookManager = nullptr;

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
    int otauIdleTotalCounter;
    int otauUnbindIdleTotalCounter;
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

    // generic network reconnect state machine
    enum NetworkReconnectState
    {
        DisconnectingNetwork,
        ReconnectNetwork,
        MaintainNetwork
    };

    QTimer *reconnectTimer = nullptr;
    NetworkReconnectState networkState = MaintainNetwork;
    int networkDisconnectAttempts;
    int networkReconnectAttempts;
    bool networkConnectedBefore;
    bool needRestartApp = false;

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

    // lights
    enum SearchLightsState
    {
        SearchLightsIdle,
        SearchLightsActive,
        SearchLightsDone,
    };

    // sensors
    enum SearchSensorsState
    {
        SearchSensorsIdle,
        SearchSensorsActive,
        SearchSensorsDone,
    };

    RestDevices *restDevices;

    int sensorIndIdleTotalCounter;

    class SensorCommand
    {
    public:
        bool operator ==(const SensorCommand &other) const
        {
            return endpoint == other.endpoint &&
                    cluster == other.cluster &&
                    zclCommand == other.zclCommand &&
                    zclCommandParameter == other.zclCommandParameter &&
                    dstGroup == other.dstGroup;
        }
        quint8 endpoint;
        quint16 cluster;
        quint8 zclCommand;
        quint16 dstGroup;
        uint zclCommandParameter;
    };

    class SensorCandidate
    {
    public:
        SensorCandidate() :
            macCapabilities(0),
            waitIndicationClusterId(0)
        {

        }
        deCONZ::Address address;
        quint8 macCapabilities;
        QElapsedTimer timeout;
        quint16 waitIndicationClusterId;
        std::vector<quint8> endpoints;
        std::vector<SensorCommand> rxCommands;
    };

    SearchLightsState searchLightsState;
    QVariantMap searchLightsResult;
    int searchLightsTimeout;
    QString lastLightsScan;

    SearchSensorsState searchSensorsState;
    size_t searchSensorGppPairCounter = 0;
    deCONZ::Address fastProbeAddr;
    std::vector<deCONZ::ApsDataIndication> fastProbeIndications;
    QVariantMap searchSensorsResult;
    QTimer *fastProbeTimer;
    int searchSensorsTimeout;
    QString lastSensorsScan;
    std::vector<SensorCandidate> searchSensorsCandidates;

    class RecoverOnOff
    {
    public:
        deCONZ::Address address;
        bool onOff;
        uint bri;
        int idleTotalCounterCopy;
    };
    std::vector<RecoverOnOff> recoverOnOff;

    // resourcelinks
    std::vector<Resourcelinks> resourcelinks;

    // rules
    std::vector<int> fastRuleCheck;
    QTimer *fastRuleCheckTimer;

    // general
    ApiConfig config;
    QTime queryTime;
    deCONZ::ApsController *apsCtrl;
    uint groupTaskNodeIter; // Iterates through nodes array
    int idleTotalCounter; // sys timer
    int idleLimit;
    int idleUpdateZigBeeConf; //
    int idleLastActivity; // delta in seconds
    bool supportColorModeXyForGroups;
    size_t lightIter;
    size_t sensorIter;
    size_t lightAttrIter;
    size_t sensorAttrIter;
    size_t sensorCheckIter;
    int sensorCheckFast;
    std::vector<Group> groups;
    std::vector<LightNode> nodes;
    std::vector<Rule> rules;
    QString daylightSensorId;
    size_t daylightOffsetIter = 0;
    std::vector<DL_Result> daylightTimes;
    std::vector<Sensor> sensors;
    std::list<TaskItem> tasks;
    std::list<TaskItem> runningTasks;
    QTimer *verifyRulesTimer;
    QTimer *taskTimer;
    QTimer *groupTaskTimer;
    QTimer *checkSensorsTimer;
    uint8_t zclSeq;
    std::list<QTcpSocket*> eventListeners;
    bool joinedMulticastGroup;
    QTimer *upnpTimer;
    QUdpSocket *udpSock;
    QUdpSocket *udpSockOut;
    uint8_t haEndpoint;

    // events
    QTimer *eventTimer;
    std::deque<Event> eventQueue;

    // bindings
    size_t verifyRuleIter;
    bool gwReportingEnabled;
    QTimer *bindingToRuleTimer;
    QTimer *bindingTimer;
    QTimer *bindingTableReaderTimer;
    std::list<Binding> bindingToRuleQueue; // check if rule exists for discovered bindings
    std::list<BindingTask> bindingQueue; // bind/unbind queue
    std::vector<BindingTableReader> bindingTableReaders;

    // TCP connection watcher
    QTimer *openClientTimer;
    std::vector<TcpClient> openClients;

    WebSocketServer *webSocketServer;

    // will be set at startup to calculate the uptime
    QElapsedTimer starttimeRef;

    Q_DECLARE_PUBLIC(DeRestPlugin)
    DeRestPlugin *q_ptr; // public interface

};

extern DeRestPluginPrivate *plugin;

#endif // DE_WEB_PLUGIN_PRIVATE_H
