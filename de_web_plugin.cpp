 /*
 * Copyright (c) 2017-2020 dresden elektronik ingenieurtechnik gmbh.
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
#include <QDir>
#include <QProcess>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonParseError>
#include <queue>
#include <cmath>
#include "colorspace.h"
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
#include "de_web_widget.h"
#include "gateway_scanner.h"
#include "json.h"
#include "poll_control.h"
#include "poll_manager.h"
#include "rest_devices.h"
#include "read_files.h"
#ifdef ARCH_ARM
  #include <unistd.h>
  #include <sys/reboot.h>
  #include <errno.h>
#endif

DeRestPluginPrivate *plugin;

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
static uint MaxGroupTasks = 4;

const quint64 macPrefixMask       = 0xffffff0000000000ULL;

// New mac prefixes can be checked here: https://wintelguy.com/index.pl
const quint64 legrandMacPrefix    = 0x0004740000000000ULL;
const quint64 dishMacPrefix       = 0x0008890000000000ULL;
const quint64 ikeaMacPrefix       = 0x000b570000000000ULL;
const quint64 emberMacPrefix      = 0x000d6f0000000000ULL;
const quint64 instaMacPrefix      = 0x000f170000000000ULL;
const quint64 tiMacPrefix         = 0x00124b0000000000ULL;
const quint64 netvoxMacPrefix     = 0x00137a0000000000ULL;
const quint64 boschMacPrefix      = 0x00155f0000000000ULL;
const quint64 jennicMacPrefix     = 0x00158d0000000000ULL;
const quint64 develcoMacPrefix    = 0x0015bc0000000000ULL;
const quint64 philipsMacPrefix    = 0x0017880000000000ULL;
const quint64 computimeMacPrefix  = 0x001e5e0000000000ULL;
const quint64 celMacPrefix        = 0x0022a30000000000ULL; // California Eastern Laboratories
const quint64 ubisysMacPrefix     = 0x001fee0000000000ULL;
const quint64 deMacPrefix         = 0x00212e0000000000ULL;
const quint64 keenhomeMacPrefix   = 0x0022a30000000000ULL;
const quint64 zenMacPrefix        = 0x0024460000000000ULL;
const quint64 heimanMacPrefix     = 0x0050430000000000ULL;
const quint64 davicomMacPrefix    = 0x00606e0000000000ULL;
const quint64 xiaomiMacPrefix     = 0x04cf8c0000000000ULL;
const quint64 konkeMacPrefix      = 0x086bd70000000000ULL;
const quint64 ikea2MacPrefix      = 0x14b4570000000000ULL;
const quint64 profaluxMacPrefix   = 0x20918a0000000000ULL;
const quint64 stMacPrefix         = 0x24fd5b0000000000ULL;
const quint64 samjinMacPrefix     = 0x286d970000000000ULL;
const quint64 sinopeMacPrefix     = 0x500b910000000000ULL;
const quint64 silabs9MacPrefix    = 0x5c02720000000000ULL;
const quint64 silabs6MacPrefix    = 0x588e810000000000ULL;
const quint64 silabs8MacPrefix    = 0x60a4230000000000ULL;
const quint64 silabs4MacPrefix    = 0x680ae20000000000ULL;
const quint64 ecozyMacPrefix      = 0x70b3d50000000000ULL;
const quint64 osramMacPrefix      = 0x8418260000000000ULL;
const quint64 silabs5MacPrefix    = 0x842e140000000000ULL;
const quint64 embertecMacPrefix   = 0x848e960000000000ULL;
const quint64 silabsMacPrefix     = 0x90fd9f0000000000ULL;
const quint64 zhejiangMacPrefix   = 0xb0ce180000000000ULL;
const quint64 silabs7MacPrefix    = 0xbc33ac0000000000ULL;
const quint64 silabs2MacPrefix    = 0xcccccc0000000000ULL;
const quint64 silabs3MacPrefix    = 0xec1bbd0000000000ULL;
const quint64 energyMiMacPrefix   = 0xd0cf5e0000000000ULL;
const quint64 bjeMacPrefix        = 0xd85def0000000000ULL;
const quint64 xalMacPrefix        = 0xf8f0050000000000ULL;
const quint64 lutronMacPrefix     = 0xffff000000000000ULL;
// Danalock support
const quint64 danalockMacPrefix   = 0x000b570000000000ULL; // note: same as ikeaMacPrefix
const quint64 schlageMacPrefix    = 0xd0cf5e0000000000ULL;

struct SupportedDevice {
    quint16 vendorId;
    const char *modelId;
    quint64 mac;
};

static const SupportedDevice supportedDevices[] = {
    { VENDOR_3A_SMART_HOME, "FNB56-GAS", jennicMacPrefix }, // Feibit FNB56-GAS05FB1.4 gas leak detector
    { VENDOR_3A_SMART_HOME, "FNB56-COS", jennicMacPrefix }, // Feibit FNB56-COS06FB1.7 Carb. Mon. detector
    { VENDOR_3A_SMART_HOME, "FNB56-SMF", jennicMacPrefix }, // Feibit FNB56-SMF06FB1.6 smoke detector
    { VENDOR_BUSCH_JAEGER, "RB01", bjeMacPrefix },
    { VENDOR_BUSCH_JAEGER, "RM01", bjeMacPrefix },
    { VENDOR_BOSCH, "ISW-ZDL1-WP11G", boschMacPrefix },
    { VENDOR_BOSCH, "ISW-ZPR1-WP13", boschMacPrefix },
    { VENDOR_BOSCH, "RFDL-ZB-MS", emberMacPrefix }, // Bosch motion sensor
    { VENDOR_BOSCH2, "AIR", tiMacPrefix }, // Bosch Air quality sensor
    { VENDOR_CENTRALITE, "Motion Sensor-A", emberMacPrefix },
    { VENDOR_CENTRALITE, "3321-S", emberMacPrefix }, // Centralite multipurpose sensor
    { VENDOR_CENTRALITE, "3325-S", emberMacPrefix }, // Centralite motion sensor
    { VENDOR_CENTRALITE, "3305-S", emberMacPrefix }, // Centralite motion sensor
    { VENDOR_CLS, "3200-Sgb", emberMacPrefix }, // Centralite smart plug / Samsung smart outlet
    { VENDOR_CLS, "3200-de", emberMacPrefix }, // Centralite smart plug / Samsung smart outlet
    { VENDOR_C2DF, "3300", emberMacPrefix }, // Centralite contact sensor
    { VENDOR_C2DF, "3320-L", emberMacPrefix }, // Centralite contact sensor
    { VENDOR_C2DF, "3315", emberMacPrefix }, // Centralite water sensor
    { VENDOR_NONE, "SD8SC_00.00.03.09TC", tiMacPrefix }, // Centralite smoke sensor
    { VENDOR_CENTRALITE, "3326-L", emberMacPrefix }, // Iris motion sensor v2
    { VENDOR_C2DF, "3326-L", emberMacPrefix }, // Iris motion sensor v2
    { VENDOR_CENTRALITE, "3328-G", emberMacPrefix }, // Centralite micro motion sensor
    { VENDOR_CENTRALITE, "3323", emberMacPrefix }, // Centralite contact sensor
    { VENDOR_DDEL, "de_spect", silabs3MacPrefix }, // dresden elektronic spectral sensor
    { VENDOR_JASCO, "45856", celMacPrefix },
    { VENDOR_NONE, "LM_",  tiMacPrefix },
    { VENDOR_NONE, "LMHT_", tiMacPrefix },
    { VENDOR_NONE, "IR_", tiMacPrefix },
    { VENDOR_NONE, "DC_", tiMacPrefix },
    { VENDOR_NONE, "BX_", tiMacPrefix }, // Climax siren
    { VENDOR_NONE, "PSMD_", tiMacPrefix }, // Climax smart plug
    { VENDOR_NONE, "PSMP5_", tiMacPrefix }, // Climax smart plug
    { VENDOR_NONE, "PCM_", tiMacPrefix }, // Climax power meter
    { VENDOR_NONE, "OJB-IR715-Z", tiMacPrefix },
    { VENDOR_NONE, "902010/21", tiMacPrefix }, // Bitron: door/window sensor
    { VENDOR_NONE, "902010/22", tiMacPrefix }, // Bitron: motion sensor
    { VENDOR_NONE, "902010/23", tiMacPrefix }, // Bitron: remote control
    { VENDOR_NONE, "902010/24", tiMacPrefix }, // Bitron: smoke detector
    { VENDOR_NONE, "902010/25", tiMacPrefix }, // Bitron: smart plug
    { VENDOR_NONE, "902010/29", tiMacPrefix }, // Bitron: Outdoor siren
    { VENDOR_NONE, "SPW35Z", tiMacPrefix }, // RT-RK OBLO SPW35ZD0 smart plug
    { VENDOR_BITRON, "902010/32", emberMacPrefix }, // Bitron: thermostat
    { VENDOR_DDEL, "Lighting Switch", deMacPrefix },
    { VENDOR_DDEL, "Scene Switch", deMacPrefix },
    { VENDOR_DDEL, "FLS-NB1", deMacPrefix },
    { VENDOR_DDEL, "FLS-NB2", deMacPrefix },
    { VENDOR_IKEA, "TRADFRI remote control", ikeaMacPrefix },
    { VENDOR_IKEA, "TRADFRI remote control", silabsMacPrefix },
    { VENDOR_IKEA, "TRADFRI remote control", silabs2MacPrefix },
    { VENDOR_IKEA, "TRADFRI motion sensor", ikeaMacPrefix },
    { VENDOR_IKEA, "TRADFRI wireless dimmer", ikeaMacPrefix },
    { VENDOR_IKEA, "TRADFRI on/off switch", ikeaMacPrefix },
    { VENDOR_IKEA, "TRADFRI SHORTCUT Button", silabs4MacPrefix },
    { VENDOR_IKEA, "TRADFRI open/close remote", ikeaMacPrefix },
    { VENDOR_IKEA, "FYRTUR", ikeaMacPrefix }, // smart blind
    { VENDOR_IKEA, "KADRILJ", ikeaMacPrefix }, // smart blind
    { VENDOR_IKEA, "KADRILJ", silabs4MacPrefix }, // smart blind
    { VENDOR_IKEA, "SYMFONISK", ikea2MacPrefix }, // sound controller
    { VENDOR_INSTA, "Remote", instaMacPrefix },
    { VENDOR_INSTA, "HS_4f_GJ_1", instaMacPrefix },
    { VENDOR_INSTA, "WS_4f_J_1", instaMacPrefix },
    { VENDOR_INSTA, "WS_3f_G_1", instaMacPrefix },
    { VENDOR_AXIS, "Gear", zenMacPrefix },
    { VENDOR_MMB, "Gear", zenMacPrefix },
    { VENDOR_NYCE, "3011", emberMacPrefix }, // NYCE door/window sensor
    { VENDOR_NYCE, "3014", emberMacPrefix }, // NYCE garage door/tilt sensor
    { VENDOR_NYCE, "3041", emberMacPrefix }, // NYCE motion sensor
    { VENDOR_NYCE, "3043", emberMacPrefix }, // NYCE ceiling motion sensor
    { VENDOR_PHILIPS, "RWL02", philipsMacPrefix }, // Hue dimmer switch
    { VENDOR_PHILIPS, "ROM00", philipsMacPrefix }, // Hue smart button
    { VENDOR_PHILIPS, "RDM00", philipsMacPrefix }, // Hue wall switch module
    { VENDOR_PHILIPS, "SML00", philipsMacPrefix }, // Hue motion sensor
    { VENDOR_PHYSICAL, "tagv4", stMacPrefix}, // SmartThings Arrival sensor
    { VENDOR_PHYSICAL, "motionv4", stMacPrefix}, // SmartThings motion sensor
    { VENDOR_PHYSICAL, "moisturev4", stMacPrefix}, // SmartThings water leak sensor
    { VENDOR_PHYSICAL, "multiv4", stMacPrefix}, // SmartThings multi sensor 2016
    { VENDOR_SAMJIN, "motion", samjinMacPrefix }, // Smarthings GP-U999SJVLBAA (Samjin) Motion Sensor
    { VENDOR_SAMJIN, "multi", samjinMacPrefix }, // Smarthings (Samjin) Multipurpose Sensor
    { VENDOR_SAMJIN, "water", samjinMacPrefix }, // Smarthings (Samjin) Water Sensor
    { VENDOR_SAMJIN, "button", samjinMacPrefix }, // Smarthings (Samjin) Button
    { VENDOR_SAMJIN, "outlet", samjinMacPrefix }, // Smarthings (Samjin) Outlet
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
    { VENDOR_JENNIC, "lumi.remote.b186acn02", jennicMacPrefix },  // Xiaomi single button wall switch WXKG02LM 2020
    { VENDOR_JENNIC, "lumi.sensor_86sw2", jennicMacPrefix },      // Xiaomi dual button wall switch WXKG02LM 2016
    { VENDOR_JENNIC, "lumi.remote.b286acn01", jennicMacPrefix },  // Xiaomi dual button wall switch WXKG02LM 2018
    { VENDOR_JENNIC, "lumi.remote.b286acn02", jennicMacPrefix },  // Xiaomi dual button wall switch WXKG02LM 2020
    { VENDOR_JENNIC, "lumi.sensor_switch", jennicMacPrefix },     // Xiaomi WXKG01LM, WXKG11LM and WXKG12LM (fallback)
    { VENDOR_JENNIC, "lumi.ctrl_neutral", jennicMacPrefix }, // Xiaomi Wall Switch (end-device)
    { VENDOR_JENNIC, "lumi.vibration", jennicMacPrefix }, // Xiaomi Aqara vibration/shock sensor
    { VENDOR_JENNIC, "lumi.sensor_wleak", jennicMacPrefix },
    { VENDOR_JENNIC, "lumi.sensor_smoke", jennicMacPrefix },
    { VENDOR_JENNIC, "lumi.sensor_natgas", jennicMacPrefix }, // Xiaomi Mija (HonneyWell) gas detector
    { VENDOR_JENNIC, "lumi.relay.c", jennicMacPrefix }, // Xiaomi Aqara LLKZMK11LM
    { VENDOR_XIAOMI, "lumi.plug", jennicMacPrefix }, // Xiaomi smart plug (router)
    { VENDOR_XIAOMI, "lumi.ctrl_ln", jennicMacPrefix}, // Xiaomi Wall Switch (router)
    { VENDOR_XIAOMI, "lumi.plug.maeu01", xiaomiMacPrefix}, // Xiaomi Aqara outlet
    { VENDOR_XIAOMI, "lumi.remote.b286opcn01", xiaomiMacPrefix }, // Xiaomi Aqara Opple WXCJKG11LM
    { VENDOR_XIAOMI, "lumi.remote.b486opcn01", xiaomiMacPrefix }, // Xiaomi Aqara Opple WXCJKG12LM
    { VENDOR_XIAOMI, "lumi.remote.b686opcn01", xiaomiMacPrefix }, // Xiaomi Aqara Opple WXCJKG13LM
    { VENDOR_XIAOMI, "lumi.sen_ill.mgl01", xiaomiMacPrefix }, // Xiaomi ZB3.0 light sensor
    { VENDOR_XIAOMI, "lumi.plug", xiaomiMacPrefix }, // Xiaomi smart plugs (router)
    { VENDOR_XIAOMI, "lumi.switch.b1naus01", xiaomiMacPrefix }, // Xiaomi Aqara ZB3.0 Smart Wall Switch Single Rocker WS-USC03
    // { VENDOR_XIAOMI, "lumi.curtain", jennicMacPrefix}, // Xiaomi curtain controller (router) - exposed only as light
    { VENDOR_XIAOMI, "lumi.curtain.hagl04", xiaomiMacPrefix}, // Xiaomi B1 curtain controller
    { VENDOR_XIAOMI, "lumi.sensor_magnet.agl02", xiaomiMacPrefix}, // Xiaomi Aqara T1 open/close sensor MCCGQ12LM
    { VENDOR_XIAOMI, "lumi.flood.agl02", xiaomiMacPrefix}, // Xiaomi Aqara T1 water leak sensor SJCGQ12LM
    { VENDOR_UBISYS, "C4", ubisysMacPrefix },
    { VENDOR_UBISYS, "D1", ubisysMacPrefix },
    { VENDOR_UBISYS, "J1", ubisysMacPrefix },
    { VENDOR_UBISYS, "S1", ubisysMacPrefix },
    { VENDOR_UBISYS, "S2", ubisysMacPrefix },
    { VENDOR_NONE, "Z716A", netvoxMacPrefix },
    // { VENDOR_OSRAM_STACK, "Plug", osramMacPrefix }, // OSRAM plug - exposed only as light
    { VENDOR_OSRAM, "Lightify Switch Mini", emberMacPrefix }, // Osram 3 button remote
    { VENDOR_OSRAM, "Switch 4x EU-LIGHTIFY", emberMacPrefix }, // Osram 4 button remote
    { VENDOR_OSRAM, "Switch 4x-LIGHTIFY", emberMacPrefix }, // Osram 4 button remote
    { VENDOR_OSRAM, "Switch-LIGHTIFY", emberMacPrefix }, // Osram 4 button remote
    { VENDOR_OSRAM_STACK, "CO_", heimanMacPrefix }, // Heiman CO sensor
    { VENDOR_OSRAM_STACK, "DOOR_", heimanMacPrefix }, // Heiman door/window sensor - older model
    { VENDOR_OSRAM_STACK, "PIR_", heimanMacPrefix }, // Heiman motion sensor
    { VENDOR_OSRAM_STACK, "GAS_", heimanMacPrefix }, // Heiman gas sensor - older model
    { VENDOR_OSRAM_STACK, "TH-", heimanMacPrefix }, // Heiman temperature/humidity sensor
    { VENDOR_OSRAM_STACK, "SMOK_", heimanMacPrefix }, // Heiman fire sensor - older model
    { VENDOR_OSRAM_STACK, "WATER_", heimanMacPrefix }, // Heiman water sensor
    { VENDOR_OSRAM_STACK, "RC_V14", heimanMacPrefix }, // Heiman HS1RC-M remote control
    { VENDOR_LGE, "LG IP65 HMS", emberMacPrefix },
    { VENDOR_EMBER, "SmartPlug", emberMacPrefix }, // Heiman smart plug
    { VENDOR_HEIMAN, "SmartPlug", emberMacPrefix }, // Heiman smart plug
    { VENDOR_HEIMAN, "GAS", emberMacPrefix }, // Heiman gas sensor - newer model
    { VENDOR_HEIMAN, "Smoke", emberMacPrefix }, // Heiman fire sensor - newer model
    { VENDOR_HEIMAN, "COSensor", emberMacPrefix }, // Heiman CO sensor - newer model
    { VENDOR_HEIMAN, "TH-", emberMacPrefix }, // Heiman temperature/humidity sensor - newer model
    { VENDOR_HEIMAN, "HT-", emberMacPrefix }, // Heiman temperature/humidity sensor - newer model
    { VENDOR_HEIMAN, "Water", emberMacPrefix }, // Heiman water sensor - newer model
    { VENDOR_HEIMAN, "Door", emberMacPrefix }, // Heiman door/window sensor - newer model
    { VENDOR_HEIMAN, "WarningDevice", emberMacPrefix }, // Heiman siren
    { VENDOR_HEIMAN, "Smoke", jennicMacPrefix }, // Heiman fire sensor - newer model
    { VENDOR_HEIMAN, "PIRSensor-EM", jennicMacPrefix }, // Heiman motion sensor - newer model
    { VENDOR_HEIMAN, "SKHMP30", jennicMacPrefix }, // GS (Heiman) smart plug
    { VENDOR_HEIMAN, "RC-EM", emberMacPrefix }, // Heiman IAS ACE remote control
    { VENDOR_HEIMAN, "RC-EF-3.0", silabs5MacPrefix }, // Heiman IAS ACE remote control
    { VENDOR_LUTRON, "LZL4BWHL01", lutronMacPrefix }, // Lutron LZL-4B-WH-L01 Connected Bulb Remote
    { VENDOR_LUTRON, "Z3-1BRL", lutronMacPrefix }, // Lutron Aurora Friends-of-Hue dimmer
    { VENDOR_KEEN_HOME , "SV01-", keenhomeMacPrefix}, // Keen Home Vent
    { VENDOR_KEEN_HOME , "SV02-", keenhomeMacPrefix}, // Keen Home Vent
    { VENDOR_INNR, "SP 120", jennicMacPrefix}, // innr smart plug
    { VENDOR_JENNIC, "VMS_ADUROLIGHT", jennicMacPrefix }, // Trust motion sensor ZPIR-8000
    { VENDOR_JENNIC, "CSW_ADUROLIGHT", jennicMacPrefix }, // Trust contact sensor ZMST-808
    { VENDOR_JENNIC, "ZYCT-202", jennicMacPrefix }, // Trust remote control ZYCT-202 (older model)
    { VENDOR_ADUROLIGHT, "ZLL-NonColorController", jennicMacPrefix }, // Trust remote control ZYCT-202 (newer model)
    { VENDOR_INNR, "RC 110", jennicMacPrefix }, // innr remote RC 110
    { VENDOR_VISONIC, "MCT-340", emberMacPrefix }, // Visonic MCT-340 E temperature/motion
    { VENDOR_SUNRICHER, "ED-1001", silabs2MacPrefix }, // EcoDim wireless switches
    { VENDOR_SUNRICHER, "ZGR904-S", emberMacPrefix }, // Envilar remote
    { VENDOR_SUNRICHER, "ICZB-KPD1", emberMacPrefix }, // iCasa keypad
    { VENDOR_SUNRICHER, "ICZB-RM", silabs2MacPrefix }, // iCasa remote
    { VENDOR_SUNRICHER, "ZGRC-KEY", emberMacPrefix }, // Sunricher wireless CCT remote
    { VENDOR_SUNRICHER, "ZG2833K", emberMacPrefix }, // Sunricher remote controller
    { VENDOR_SUNRICHER, "RGBgenie ZB-5", emberMacPrefix }, // RGBgenie remote control
    { VENDOR_SUNRICHER, "ROB_200", silabs3MacPrefix }, // Sunricher SR-ZG9040A built-in dimmer, whitelabeled by Robbshop
    { VENDOR_SUNRICHER, "Micro Smart Dimmer", silabs3MacPrefix }, // Sunricher SR-ZG9040A built-in dimmer
    { VENDOR_SUNRICHER, "ZG2835", silabs6MacPrefix }, // SR-ZG2835 Zigbee Rotary Switch
    { VENDOR_SUNRICHER, "ZGRC-TEUR-", emberMacPrefix }, // iluminize wall switch 511.524
    { VENDOR_JENNIC, "SPZB0001", jennicMacPrefix }, // Eurotronic thermostat
    { VENDOR_NONE, "RES001", tiMacPrefix }, // Hubitat environment sensor, see #1308
    { VENDOR_SINOPE, "WL4200S", sinopeMacPrefix}, // Sinope water sensor with wired remote sensor
    { VENDOR_SINOPE, "WL4200", sinopeMacPrefix}, // Sinope water sensor
    { VENDOR_SINOPE, "TH1300ZB", sinopeMacPrefix }, // Sinope Thermostat
    { VENDOR_SINOPE, "TH112", sinopeMacPrefix }, // Sinope Thermostat TH1123ZB & TH1124ZB
    { VENDOR_ZEN, "Zen-01", zenMacPrefix }, // Zen Thermostat
    { VENDOR_C2DF, "3157100", emberMacPrefix }, // Centralite Thermostat
    { VENDOR_EMBER, "Super TR", emberMacPrefix }, // Elko Thermostat
    { VENDOR_DATEK_WIRLESS, "Super TR", silabs4MacPrefix }, // Elko Thermostat
    { VENDOR_EMBER, "ElkoDimmer", emberMacPrefix }, // Elko dimmer
    { VENDOR_ATMEL, "Thermostat", ecozyMacPrefix }, // eCozy Thermostat
    { VENDOR_OWON, "AC201", davicomMacPrefix }, // OWON AC201 Thermostat
    { VENDOR_OWON, "PR412C", emberMacPrefix }, // OWON PCT502 Thermostat
    { VENDOR_STELPRO, "ST218", xalMacPrefix }, // Stelpro Thermostat
    { VENDOR_STELPRO, "STZB402", xalMacPrefix }, // Stelpro baseboard thermostat
    { VENDOR_STELPRO, "SORB", xalMacPrefix }, // Stelpro Orleans Fan
    { VENDOR_DEVELCO, "AQSZB-1", develcoMacPrefix }, // Develco air quality sensor
    { VENDOR_DEVELCO, "SMSZB-1", develcoMacPrefix }, // Develco smoke sensor
    { VENDOR_DEVELCO, "HESZB-1", develcoMacPrefix }, // Develco heat sensor
    { VENDOR_DEVELCO, "SPLZB-1", develcoMacPrefix }, // Develco smart plug
    { VENDOR_DEVELCO, "WISZB-1", develcoMacPrefix }, // Develco window sensor
    { VENDOR_DEVELCO, "MOSZB-1", develcoMacPrefix }, // Develco motion sensor
    { VENDOR_DEVELCO, "FLSZB-1", develcoMacPrefix }, // Develco water leak sensor
    { VENDOR_DEVELCO, "EMIZB-1", develcoMacPrefix }, // Develco EMI Norwegian HAN
    { VENDOR_DEVELCO, "SMRZB-3", develcoMacPrefix }, // Develco Smart Relay DIN
    { VENDOR_DEVELCO, "SMRZB-1", develcoMacPrefix }, // Develco Smart Cable
    { VENDOR_DEVELCO, "SIRZB-1", develcoMacPrefix }, // Develco siren
    { VENDOR_DEVELCO, "HMSZB-1", develcoMacPrefix }, // Develco temp/hum sensor
    { VENDOR_DEVELCO, "ZHMS101", develcoMacPrefix }, // Wattle (Develco) magnetic sensor
    { VENDOR_DEVELCO, "MotionSensor51AU", develcoMacPrefix }, // Aurora (Develco) motion sensor
    { VENDOR_DATEK_WIRLESS, "PoP", konkeMacPrefix }, // Apex Smart Plug
    { VENDOR_EMBER, "3AFE14010402000D", konkeMacPrefix }, // Konke Kit Pro-BS Motion Sensor
    { VENDOR_KONKE, "3AFE28010402000D", ikea2MacPrefix }, // Konke Kit Pro-BS Motion Sensor ver.2
    { VENDOR_EMBER, "3AFE140103020000", konkeMacPrefix }, // Konke Kit Pro-FT Temp Humidity Sensor
    { VENDOR_KONKE, "3AFE220103020000", ikea2MacPrefix }, // Konke Kit Pro-BS Temp Humidity Sensor ver.2
    { VENDOR_KONKE, "3AFE220103020000", konkeMacPrefix }, // Konke Kit Pro-BS Temp Humidity Sensor ver ???
    { VENDOR_EMBER, "3AFE130104020015", konkeMacPrefix }, // Konke Kit Pro-Door Entry Sensor
    { VENDOR_NONE, "RICI01", tiMacPrefix}, // LifeControl smart plug
    { VENDOR_JENNIC, "Adurolight_NCC", jennicMacPrefix}, // Eria Adurosmart Wireless Dimming Switch
    { VENDOR_JENNIC, "VOC_Sensor", jennicMacPrefix}, // LifeControl Enviroment sensor
    { VENDOR_JENNIC, "SN10ZW", jennicMacPrefix }, // ORVIBO motion sensor
    { VENDOR_OSRAM_STACK, "SF20", heimanMacPrefix }, // ORVIBO SF20 smoke sensor
    // Danalock support
    { VENDOR_DANALOCK, "V3", danalockMacPrefix}, // Danalock Smart Lock
    // Schlage support
    { VENDOR_SCHLAGE, "BE468", schlageMacPrefix}, // Schlage BE468 Smart Lock
    { VENDOR_HEIMAN, "SF21", emberMacPrefix }, // ORVIBO SF21 smoke sensor
    { VENDOR_3A_SMART_HOME, "ST30 Temperature Sensor", jennicMacPrefix }, // Orvibo ST30 Temp/Humidity Sensor with diplay
    { VENDOR_HEIMAN, "358e4e3e03c644709905034dae81433e", emberMacPrefix }, // Orvibo Combustible Gas Sensor
    { VENDOR_LEGRAND, "Dimmer switch w/o neutral", legrandMacPrefix }, // Legrand Dimmer switch wired
    { VENDOR_LEGRAND, "Connected outlet", legrandMacPrefix }, // Legrand Plug
    { VENDOR_LEGRAND, "Shutter switch with neutral", legrandMacPrefix }, // Legrand Shutter switch
    { VENDOR_LEGRAND, "Remote toggle switch", legrandMacPrefix }, // Legrand switch module
    { VENDOR_LEGRAND, "Cable outlet", legrandMacPrefix }, // Legrand Cable outlet
    { VENDOR_LEGRAND, "Remote switch", legrandMacPrefix }, // Legrand wireless switch
    { VENDOR_LEGRAND, "Double gangs remote switch", legrandMacPrefix }, // Legrand wireless double switch
    { VENDOR_LEGRAND, "Remote motion sensor", legrandMacPrefix }, // Legrand motion sensor
    { VENDOR_LEGRAND, "Shutters central remote switch", legrandMacPrefix }, // Legrand wireless shutter switch (battery)
    { VENDOR_LEGRAND, "DIN power consumption module", legrandMacPrefix }, // Legrand DIN power consumption module
    { VENDOR_LEGRAND, "Teleruptor", legrandMacPrefix }, // Legrand Teleruptor
    { VENDOR_LEGRAND, "Contactor", legrandMacPrefix }, // Legrand Contactor
    { VENDOR_NETVOX, "Z809AE3R", netvoxMacPrefix }, // Netvox smartplug
    { VENDOR_LDS, "ZB-ONOFFPlug-D0005", silabs2MacPrefix }, // Samsung SmartPlug 2019 (7A-PL-Z-J3)
    { VENDOR_LDS, "ZBT-DIMSwitch", silabs2MacPrefix }, // Linkind 1 key Remote Control / ZS23000178
    { VENDOR_LDS, "ZB-MotionSensor-D0003", silabsMacPrefix }, // Linkind motion sensor / ZS110040078
    { VENDOR_PHYSICAL, "outletv4", stMacPrefix }, // Samsung SmartThings plug (IM6001-OTP)
    { VENDOR_EMBER, "RH3040", konkeMacPrefix }, // Tuyatec motion sensor
    { VENDOR_NONE, "RH3001", ikea2MacPrefix }, // Tuyatec door/window sensor
    { VENDOR_EMBER, "RH3001", silabs3MacPrefix }, // Tuya/Blitzwolf BW-IS2 door/window sensor
    { VENDOR_NONE, "RH3052", emberMacPrefix }, // Tuyatec temperature sensor
    { VENDOR_NONE, "RH3052", konkeMacPrefix }, // Tuyatec/Lupus temperature sensor
    { VENDOR_EMBER, "TS0201", silabs3MacPrefix }, // Tuya/Blitzwolf temperature and humidity sensor
    { VENDOR_NONE, "TS0204", silabs3MacPrefix }, // Tuya gas sensor
    { VENDOR_NONE, "TS0205", silabs3MacPrefix }, // Tuya smoke sensor
    { VENDOR_NONE, "TS0121", silabs3MacPrefix }, // Tuya/Blitzwolf smart plug
    { VENDOR_EMBER, "TS0121", silabs3MacPrefix }, // Tuya/Blitzwolf smart plug
    { VENDOR_EMBER, "TS0302", silabs3MacPrefix }, // Tuya curtain switch
    { VENDOR_EMBER, "TS0041", silabs3MacPrefix }, // Tuya wireless switch
    { VENDOR_EMBER, "TS0041", silabs5MacPrefix }, // Tuya wireless switch
    { VENDOR_EMBER, "TS0042", silabs3MacPrefix }, // Tuya wireless switch
    { VENDOR_EMBER, "TS0043", silabs3MacPrefix }, // Tuya wireless switch
    { VENDOR_EMBER, "TS0043", silabs8MacPrefix }, // Tuya wireless switch
    { VENDOR_EMBER, "TS0043", silabs7MacPrefix }, // Tuya wireless switch
    { VENDOR_EMBER, "TS0044", silabs9MacPrefix }, // Tuya wireless switch
    { VENDOR_NONE, "kud7u2l", silabs3MacPrefix }, // Tuya Smart TRV HY369 Thermostatic Radiator Valve
    { VENDOR_NONE, "GbxAXL2", silabs3MacPrefix }, // Another Tuya Smart TRV Thermostatic Radiator Valve
    { VENDOR_NONE, "w7cahqs", silabs8MacPrefix }, // hama Smart Radiator Thermostat
    { VENDOR_EMBER, "TS0601", silabs7MacPrefix }, // Tuya Smart TRV HY369 Thermostatic Radiator Valve
    { VENDOR_EMBER, "TS0601", silabs5MacPrefix }, // MOES Zigbee Radiator Actuator HY368 / Moes Tuya Thermostat BTH-002
    { VENDOR_EMBER, "TS0207", silabs3MacPrefix }, // Tuya water leak sensor
    { VENDOR_NONE, "TS0202", silabs4MacPrefix }, // Tuya presence sensor
    { VENDOR_NONE, "0yu2xgi", silabs5MacPrefix }, // Tuya siren
    { VENDOR_EMBER, "TS0601", silabs9MacPrefix }, // Tuya siren
    { VENDOR_NONE, "eaxp72v", ikea2MacPrefix }, // Tuya TRV Wesmartify Thermostat Essentials Premium
    { VENDOR_NONE, "88teujp", silabs8MacPrefix }, // SEA802-Zigbee
    { VENDOR_NONE, "uhszj9s", silabs8MacPrefix }, // HiHome WZB-TRVL
    { VENDOR_NONE, "fvq6avy", silabs7MacPrefix }, // Revolt NX-4911-675 Thermostat
    { VENDOR_NONE, "GMB-HAS-WL-B01", tiMacPrefix }, // GamaBit Ltd. water leak Sensor
    { VENDOR_NONE, "GMB-HAS-DW-B01", tiMacPrefix }, // GamaBit Ltd. Window/Door Sensor
    { VENDOR_NONE, "GMB-HAS-VB-B01", tiMacPrefix }, // GamaBit Ltd. Vibration Sensor
    { VENDOR_HEIMAN, "TY0203", silabs3MacPrefix }, // Lidl/Silvercrest Smart Window or Door Sensor
    { VENDOR_HEIMAN, "TY0203", silabs7MacPrefix }, // Lidl/Silvercrest Smart Window or Door Sensor
    { VENDOR_HEIMAN, "TY0202", silabs3MacPrefix }, // Lidl/Silvercrest Smart Motion Sensor
    { VENDOR_HEIMAN, "TY0202", silabs7MacPrefix }, // Lidl/Silvercrest Smart Motion Sensor
    { VENDOR_HEIMAN, "TS0211", silabs3MacPrefix }, // Lidl/Silvercrest Smart Wireless Door Bell
    { VENDOR_HEIMAN, "TS0211", silabs5MacPrefix }, // Lidl/Silvercrest Smart Wireless Door Bell
    { VENDOR_HEIMAN, "TS0211", silabs7MacPrefix }, // Lidl/Silvercrest Smart Wireless Door Bell
    { VENDOR_HEIMAN, "TS0215", silabs3MacPrefix }, // Tuya IAS ACE remote control
    { VENDOR_AURORA, "DoubleSocket50AU", jennicMacPrefix }, // Aurora AOne Double Socket UK
    { VENDOR_COMPUTIME, "SP600", computimeMacPrefix }, // Salus smart plug
    { VENDOR_COMPUTIME, "SPE600", computimeMacPrefix }, // Salus smart plug
    { VENDOR_HANGZHOU_IMAGIC, "1116-S", energyMiMacPrefix }, // iris contact sensor v3
    { VENDOR_HANGZHOU_IMAGIC, "1117-S", energyMiMacPrefix }, // iris motion sensor v3
    { VENDOR_JENNIC, "113D", jennicMacPrefix }, // iHorn (Huawei) temperature and humidity sensor
    { VENDOR_CHINA_FIRE_SEC, "LH05121", jennicMacPrefix }, // iHorn (Huawei) smoke detector
    { VENDOR_SERCOMM, "SZ-ESW01", emberMacPrefix }, // Sercomm / Telstra smart plug
    { VENDOR_SERCOMM, "SZ-SRN12N", emberMacPrefix }, // Sercomm siren
    { VENDOR_SERCOMM, "SZ-SRN12N", energyMiMacPrefix }, // Sercomm siren
    { VENDOR_SERCOMM, "SZ-DWS04", emberMacPrefix }, // Sercomm open/close sensor
    { VENDOR_SERCOMM, "SZ-WTD02N_CAR", emberMacPrefix }, // Sercomm water sensor
    { VENDOR_SERCOMM, "GZ-PIR02", emberMacPrefix }, // Sercomm motion sensor
    { VENDOR_SERCOMM, "Tripper", emberMacPrefix }, // Quirky Tripper (Sercomm) open/close sensor
    { VENDOR_ALERTME, "MOT003", tiMacPrefix }, // Hive Motion Sensor
    { VENDOR_ALERTME, "DWS003", tiMacPrefix }, // Hive Door sensor
    { VENDOR_ALERTME, "SLP2", computimeMacPrefix }, // Hive  plug
    { VENDOR_ALERTME, "SLP2b", computimeMacPrefix }, // Hive  plug
    { VENDOR_ALERTME, "SLR1b", computimeMacPrefix }, // Hive   Heating Receiver 1 channel
    { VENDOR_ALERTME, "SLR2", computimeMacPrefix }, // Hive   Heating Receiver 2 channel
    { VENDOR_ALERTME, "SLR2b", computimeMacPrefix }, // Hive   Heating Receiver 2 channel second version
    { VENDOR_ALERTME, "SLT2", computimeMacPrefix }, // Hive thermostat
    { VENDOR_DANFOSS, "TRV001", silabs2MacPrefix }, // Hive thermostat (From Danfoss)
    { VENDOR_SUNRICHER, "45127", silabs2MacPrefix }, // Namron 1/2/4-ch remote controller
    { VENDOR_SENGLED_OPTOELEC, "E13-", zhejiangMacPrefix }, // Sengled PAR38 Bulbs
    { VENDOR_SENGLED_OPTOELEC, "E1D-", zhejiangMacPrefix }, // Sengled contact sensor
    { VENDOR_SENGLED_OPTOELEC, "E1E-", zhejiangMacPrefix }, // Sengled Smart Light Switch
    { VENDOR_JENNIC, "Plug-230V-ZB3.0", silabs2MacPrefix }, // Immax NEO ZB3.0 smart plug
    { VENDOR_JENNIC, "4in1-Sensor-ZB3.0", emberMacPrefix }, // Immax NEO ZB3.0 4 in 1 sensor
    { VENDOR_JENNIC, "Keyfob-ZB3.0", emberMacPrefix }, // Immax Keyfob
    { VENDOR_WAXMAN, "leakSMART Water Sensor V2", celMacPrefix }, // WAXMAN LeakSMART v2
    { VENDOR_PHILIO, "PST03A-v2.2.5", emberMacPrefix }, // Philio pst03-a
    { VENDOR_EMBERTEC, "BQZ10-AU", embertecMacPrefix }, // Embertec smart plug
    { VENDOR_MUELLER, "ZBT-Remote-ALL-RGBW", jennicMacPrefix }, // Tint remote control
    { VENDOR_PLUGWISE_BV, "160-01", emberMacPrefix }, // Plugwise smart plug
    { VENDOR_NIKO_NV, "Connected socket outlet", konkeMacPrefix }, // Niko smart socket 170-33505
    { VENDOR_ATMEL, "Bell", dishMacPrefix }, // Sage doorbell sensor
    { VENDOR_UNIVERSAL2, "4655BC0", emberMacPrefix }, // Ecolink contact sensor
    { VENDOR_NONE, "ZB-SmartPlug-1.0.0", tiMacPrefix }, // edp re:dy plug
    { VENDOR_NONE, "WB01", tiMacPrefix }, // Sonoff SNZB-01
    { VENDOR_NONE, "WB-01", tiMacPrefix }, // Sonoff SNZB-01
    { VENDOR_NONE, "MS01", tiMacPrefix }, // Sonoff SNZB-03
    { VENDOR_NONE, "MSO1", tiMacPrefix }, // Sonoff SNZB-03
    { VENDOR_NONE, "ms01", tiMacPrefix }, // Sonoff SNZB-03
    { VENDOR_NONE, "66666", tiMacPrefix }, // Sonoff SNZB-03
    { VENDOR_NONE, "TH01", tiMacPrefix }, // Sonoff SNZB-02
    { VENDOR_NONE, "DS01", tiMacPrefix }, // Sonoff SNZB-04
    { VENDOR_DANFOSS, "eTRV0100", silabs2MacPrefix }, // Danfoss Ally thermostat
    { VENDOR_DANFOSS, "0x8020", silabs6MacPrefix }, // Danfoss RT24V Display thermostat
    { VENDOR_DANFOSS, "0x8021", silabs6MacPrefix }, // Danfoss RT24V Display thermostat with floor sensor
    { VENDOR_DANFOSS, "0x8030", silabs6MacPrefix }, // Danfoss RTbattery Display thermostat
    { VENDOR_DANFOSS, "0x8031", silabs6MacPrefix }, // Danfoss RTbattery Display thermostat with infrared
    { VENDOR_DANFOSS, "0x8034", silabs6MacPrefix }, // Danfoss RTbattery Dial thermostat
    { VENDOR_DANFOSS, "0x8035", silabs6MacPrefix }, // Danfoss RTbattery Dial thermostat with infrared
    { VENDOR_LDS, "ZBT-CCTSwitch-D0001", silabs2MacPrefix }, // Leedarson remote control
    { VENDOR_KWIKSET, "SMARTCODE_CONVERT_GEN1", zenMacPrefix }, // Kwikset 914 ZigBee smart lock
    { VENDOR_EMBER, "TS1001", silabs5MacPrefix }, // LIDL Livarno Lux Remote Control HG06323
    { VENDOR_EMBER, "TS1001", silabs7MacPrefix }, // LIDL Livarno Lux Remote Control HG06323

    { 0, nullptr, 0 }
};

struct lidlDevice {
    const char *zigbeeManufacturerName;
    const char *zigbeeModelIdentifier;
    const char *manufacturername;
    const char *modelid;
};

static const lidlDevice lidlDevices[] = { // Sorted by zigbeeManufacturerName
    { "_TYZB01_bngwdjsr", "TS1001",  "LIDL Livarno Lux", "HG06323" }, // Remote Control
    { "_TZ3000_1obwwnmq", "TS011F",  "LIDL Silvercrest", "HG06338" }, // Smart USB Extension Lead (EU)
    { "_TZ3000_49qchf10", "TS0502A", "LIDL Livarno Lux", "HG06492C" }, // CT Light (E27)
    { "_TZ3000_9cpuaca6", "TS0505A", "LIDL Livarno Lux", "14148906L" }, // Stimmungsleuchte
    { "_TZ3000_dbou1ap4", "TS0505A", "LIDL Livarno Lux", "HG06106C" }, // RGB Light (E27)
    { "_TZ3000_el5kt5im", "TS0502A", "LIDL Livarno Lux", "HG06492A" }, // CT Light (GU10)
    { "_TZ3000_gek6snaj", "TS0505A", "LIDL Livarno Lux", "14149506L" }, // Lichtleiste
    { "_TZ3000_kdi2o9m6", "TS011F",  "LIDL Silvercrest", "HG06337" }, // Smart plug (EU)
    { "_TZ3000_kdpxju99", "TS0505A", "LIDL Livarno Lux", "HG06106A" }, // RGB Light (GU10)
    { "_TZ3000_oborybow", "TS0502A", "LIDL Livarno Lux", "HG06492B" }, // CT Light (E14)
    { "_TZ3000_odygigth", "TS0505A", "LIDL Livarno Lux", "HG06106B" }, // RGB Light (E14)
    { "_TZ3000_riwp3k79", "TS0505A", "LIDL Livarno Lux", "HG06104A" }, // LED Light Strip
    { "_TZE200_s8gkrkxk", "TS0601",  "LIDL Livarno Lux", "HG06467" }, // Smart LED String Lights (EU)
    { nullptr, nullptr, nullptr, nullptr }
};

static const lidlDevice *getLidlDevice(const QString &zigbeeManufacturerName)
{
    const lidlDevice *device = lidlDevices;

    while (device->zigbeeManufacturerName != nullptr)
    {
        if (zigbeeManufacturerName == QLatin1String(device->zigbeeManufacturerName))
        {
            return device;
        }
        device++;
    }
    return nullptr;
}

static bool isLidlDevice(const QString &zigbeeModelIdentifier, const QString &manufacturername)
{
    const lidlDevice *device = lidlDevices;

    while (device->zigbeeManufacturerName != nullptr)
    {
        if (zigbeeModelIdentifier == QLatin1String(device->zigbeeModelIdentifier) &&
            manufacturername == QLatin1String(device->manufacturername))
        {
            return true;
        }
        device++;
    }
    return false;
}

int TaskItem::_taskCounter = 1; // static rolling taskcounter

/*! Returns the largest supported API version.

    There might be one, none or multiple versions listed in \p hdrValue:

        Accept: nothing, relevant, here    --> ApiVersion_1
        Accept: vnd.ddel.v1                --> ApiVersion_1_DDEL
        Accept: vnd.ddel.v1,vnd.ddel.v1.1  --> ApiVersion_1_1_DDEL
        Accept: vnd.ddel.v2                --> ApiVersion_2_DDEL
        Accept: vnd.ddel.v1,vnd.ddel.v2    --> ApiVersion_2_DDEL
 */
static ApiVersion getAcceptHeaderApiVersion(const QString &hdrValue)
{
    ApiVersion result = { ApiVersion_1 };

    static const struct {
        ApiVersion version;
        const char *str;
    } versions[] = {
        // ordered by largest version
        {ApiVersion_2_DDEL,   "application/vnd.ddel.v2"},
        {ApiVersion_1_1_DDEL, "application/vnd.ddel.v1.1"},
        {ApiVersion_1_1_DDEL, "vnd.ddel.v1.1"}, // backward compatibility
        {ApiVersion_1_DDEL,   "application/vnd.ddel.v1"},
        {ApiVersion_1_DDEL,   "vnd.ddel.v1"},   // backward compatibility
        {ApiVersion_1, nullptr}
    };

    const auto ls = hdrValue.split(QLatin1Char(','), QString::SkipEmptyParts);

    for (int i = 0; versions[i].str != nullptr; i++)
    {
        if (ls.contains(QLatin1String(versions[i].str)))
        {
            result = versions[i].version;
            break;
        }
    }

    return result;
}

ApiRequest::ApiRequest(const QHttpRequestHeader &h, const QStringList &p, QTcpSocket *s, const QString &c) :
    hdr(h), path(p), sock(s), content(c), version(ApiVersion_1), auth(ApiAuthNone), mode(ApiModeNormal)
{
    if (hdr.hasKey(QLatin1String("Accept")) && hdr.value(QLatin1String("Accept")).contains(QLatin1String("vnd.ddel")))
    {
        version = getAcceptHeaderApiVersion(hdr.value(QLatin1String("Accept")));
    }
}

/*! Returns the apikey of a request or a empty string if not available
 */
QString ApiRequest::apikey() const
{
    if (path.length() > 1 && path[0] == QLatin1String("api"))
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
    plugin = this;

    pollManager = new PollManager(this);
    restDevices = new RestDevices(this);

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
//    gwScanner->startScan();

    QString dataPath = deCONZ::getStorageLocation(deCONZ::ApplicationsDataLocation);
    db = 0;
    saveDatabaseItems = 0;
    saveDatabaseIdleTotalCounter = 0;
    dbZclValueMaxAge = 0; // default disable
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
    supportColorModeXyForGroups = true;
    groupDeviceMembershipChecked = false;
    gwLinkButton = false;
    gwWebSocketNotifyAll = true;
    gwdisablePermitJoinAutoOff = false;
    gwLightLastSeenInterval = 60;

    // preallocate memory to get consistent pointers
    nodes.reserve(300);
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
    gwAnnounceUrl = "https://phoscon.de/discover";
    inetDiscoveryManager = nullptr;

    webhookManager = new QNetworkAccessManager(this);
    connect(webhookManager, SIGNAL(finished(QNetworkReply*)),
            this, SLOT(webhookFinishedRequest(QNetworkReply*)));

    // lights
    searchLightsState = SearchLightsIdle;
    searchLightsTimeout = 0;

    // sensors
    sensorCheckIter = 0;
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
    // get new id
    if (gwGroup0 == 0)
    {
        for (uint16_t i = 0xFFF0; i > 0; i--) // 0 and larger than 0xfff7 is not valid for Osram Lightify
        {
            Group* group = getGroupForId(i);
            if (!group)
            {
                gwGroup0 = i;
                break;
            }
        }
    }

    // delete old group 0
    if (gwGroup0 != 0)
    {
        for (Group& group : groups)
        {
            if (group.address() == 0 && !(group.state() == Group::StateDeleted || group.state() == Group::StateDeleteFromDB))
            {
                group.setState(Group::StateDeleted);
                queSaveDb(DB_CONFIG | DB_GROUPS, DB_LONG_SAVE_DELAY);
                break;
            }
        }
    }

    // create new group 0
    Group* group = getGroupForId(gwGroup0);
    if (!group)
    {
        Group group;
        group.setAddress(gwGroup0);
        group.setName("All");
        groups.push_back(group);
        queSaveDb(DB_GROUPS, DB_LONG_SAVE_DELAY);
    }

    connect(apsCtrl, SIGNAL(apsdeDataConfirm(const deCONZ::ApsDataConfirm&)),
            this, SLOT(apsdeDataConfirm(const deCONZ::ApsDataConfirm&)));

    connect(apsCtrl, SIGNAL(apsdeDataIndication(const deCONZ::ApsDataIndication&)),
            this, SLOT(apsdeDataIndication(const deCONZ::ApsDataIndication&)));

    connect(apsCtrl, SIGNAL(nodeEvent(deCONZ::NodeEvent)),
            this, SLOT(nodeEvent(deCONZ::NodeEvent)));

#if DECONZ_LIB_VERSION >= 0x010E00
    connect(apsCtrl, &deCONZ::ApsController::sourceRouteCreated, this, &DeRestPluginPrivate::storeSourceRoute);
    connect(apsCtrl, &deCONZ::ApsController::sourceRouteDeleted, this, &DeRestPluginPrivate::deleteSourceRoute);
    connect(apsCtrl, &deCONZ::ApsController::nodesRestored, this, &DeRestPluginPrivate::restoreSourceRoutes, Qt::QueuedConnection);
#endif

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

    initAuthentication();
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

    connect(pollManager, &PollManager::done, this, &DeRestPluginPrivate::pollNextDevice);

    const deCONZ::Node *node;
    if (apsCtrl && apsCtrl->getNode(0, &node) == 0)
    {
        addLightNode(node);
    }

    const QStringList buttonMapLocations = {
        deCONZ::getStorageLocation(deCONZ::ApplicationsDataLocation) + QLatin1String("/devices/button_maps.json")
#ifdef Q_OS_LINUX
        , "/usr/share/deCONZ/devices/button_maps.json"
#endif
#ifdef Q_OS_WIN
        , qApp->applicationDirPath() + QLatin1String("/../devices/button_maps.json")
#endif
    };

    for (const auto &path : buttonMapLocations)
    {
        if (!QFile::exists(path))
        {
            continue;
        }

        QStringList requiredJsonObjects = {"buttons", "buttonActions", "clusters", "commands", "maps"};
        QJsonDocument buttonMaps = readButtonMapJson(path);

        if (checkRootLevelObjectsJson(buttonMaps, requiredJsonObjects))
        {
            btnMapClusters = loadButtonMapClustersJson(buttonMaps);
            btnMapClusterCommands = loadButtonMapCommadsJson(buttonMaps);
            buttonMapForModelId = loadButtonMapModelIdsJson(buttonMaps);
            buttonMapData = loadButtonMapsJson(buttonMaps, btnMapClusters, btnMapClusterCommands);
            break; // only load once
        }
    }
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

        switch (ind.clusterId())
        {
        case GROUP_CLUSTER_ID:
            handleGroupClusterIndication(ind, zclFrame);
            break;

        case SCENE_CLUSTER_ID:
            handleSceneClusterIndication(ind, zclFrame);
            handleClusterIndicationGateways(ind, zclFrame);
            break;

        case OTAU_CLUSTER_ID:
            otauDataIndication(ind, zclFrame);
            break;

        case COMMISSIONING_CLUSTER_ID:
            handleCommissioningClusterIndication(ind, zclFrame);
            break;

        case LEVEL_CLUSTER_ID:
            handleClusterIndicationGateways(ind, zclFrame);
            break;

        case ONOFF_CLUSTER_ID:
            handleOnOffClusterIndication(ind, zclFrame);
            handleClusterIndicationGateways(ind, zclFrame);
            break;

        case IAS_ZONE_CLUSTER_ID:
            handleIasZoneClusterIndication(ind, zclFrame);
            break;

        case IAS_ACE_CLUSTER_ID:
            handleIasAceClusterIndication(ind, zclFrame);
            break;

        case VENDOR_CLUSTER_ID:
        // case DE_CLUSTER_ID:
            if (zclFrame.manufacturerCode() == VENDOR_PHILIPS)
            {
                handlePhilipsClusterIndication(ind, zclFrame);
            }
            else // Shouldn't we check for DE manufacturer code?
            {
                handleDEClusterIndication(ind, zclFrame);
            }
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

        case TUYA_CLUSTER_ID:
            // Tuya manfacture cluster:
            handleTuyaClusterIndication(ind, zclFrame);
            break;

        case THERMOSTAT_CLUSTER_ID:
            handleThermostatClusterIndication(ind, zclFrame);
            break;

        case BASIC_CLUSTER_ID:
            handleBasicClusterIndication(ind, zclFrame);
            break;

        case APPLIANCE_EVENTS_AND_ALERTS_CLUSTER_ID:
            handleApplianceAlertClusterIndication(ind, zclFrame);
            break;

        case THERMOSTAT_UI_CONFIGURATION_CLUSTER_ID:
            handleThermostatUiConfigurationClusterIndication(ind, zclFrame);
            break;

        case DIAGNOSTICS_CLUSTER_ID:
            handleDiagnosticsClusterIndication(ind, zclFrame);
            break;

        case IDENTIFY_CLUSTER_ID:
            handleIdentifyClusterIndication(ind, zclFrame);
            break;

        case DEVELCO_AIR_QUALITY_CLUSTER_ID: // Develco specific -> VOC Management
        case BOSCH_AIR_QUALITY_CLUSTER_ID: // Bosch Air quality sensor
            handleAirQualityClusterIndication(ind, zclFrame);
            break;

        case POLL_CONTROL_CLUSTER_ID:
            handlePollControlIndication(ind, zclFrame);
            break;

        case FAN_CONTROL_CLUSTER_ID:
            handleFanControlClusterIndication(ind, zclFrame);
            break;

        default:
        {
        }
            break;
        }

        handleIndicationSearchSensors(ind, zclFrame);

        if (ind.dstAddressMode() == deCONZ::ApsGroupAddress || ind.clusterId() == VENDOR_CLUSTER_ID || ind.clusterId() == IAS_ZONE_CLUSTER_ID ||
            !(zclFrame.frameControl() & deCONZ::ZclFCDirectionServerToClient) ||
            (zclFrame.isProfileWideCommand() && zclFrame.commandId() == deCONZ::ZclReportAttributesId))
        {
            Sensor *sensorNode = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), ind.srcEndpoint());

            if (sensorNode && ind.clusterId() == IAS_ZONE_CLUSTER_ID && sensorNode->type() != QLatin1String("ZHASwitch"))
            {
                sensorNode = nullptr;
                auto it = std::find_if(sensors.begin(), sensors.end(), [&ind](const Sensor &s) {
                    return s.address().ext() == ind.srcAddress().ext() && s.type() == QLatin1String("ZHASwitch");
                });

                if (it != sensors.end())
                {
                    sensorNode = &*it;
                }
            }

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
                    else if (sensorNode->modelId().startsWith(QLatin1String("C4")) || // ubisys
                             sensorNode->modelId().startsWith(QLatin1String("RC 110")) || // innr RC 110
                             sensorNode->modelId().startsWith(QLatin1String("ICZB-RM")) || // icasa remote
                             sensorNode->modelId().startsWith(QLatin1String("ZGR904-S")) || // Envilar remote
                             sensorNode->modelId().startsWith(QLatin1String("ZGRC-KEY")) || // Sunricher remote
                             sensorNode->modelId().startsWith(QLatin1String("ED-1001")) || // EcoDim switches
                             sensorNode->modelId().startsWith(QLatin1String("45127")) || // Namron switches
                             sensorNode->modelId().startsWith(QLatin1String("Lightify Switch Mini")) ||  // Osram 3 button remote
                             sensorNode->modelId().startsWith(QLatin1String("Switch 4x EU-LIGHTIFY")) || // Osram 4 button remote
                             sensorNode->modelId().startsWith(QLatin1String("Switch 4x-LIGHTIFY")) || // Osram 4 button remote
                             sensorNode->modelId().startsWith(QLatin1String("Switch-LIGHTIFY")) ) // Osram 4 button remote
                    {
                        sensorNode = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), 0x01);
                    }
                    else if (sensorNode->modelId().startsWith(QLatin1String("D1")) || // ubisys
                             sensorNode->modelId().startsWith(QLatin1String("S1")))   // ubisys
                    {
                        sensorNode = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), 0x02);
                    }
                    else if (sensorNode->modelId().startsWith(QLatin1String("S2")))
                    {
                        sensorNode = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), 0x03);
                    }
                    else if (sensorNode->modelId().endsWith(QLatin1String("86opcn01"))) //Aqara Opple enable events from all multistate clusters
                    {
                        sensorNode = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), 0x01);
                    }
                    else if ((sensorNode->manufacturer() == QLatin1String("_TZ3000_bi6lpsew")) ||
                        (sensorNode->manufacturer() == QLatin1String("_TZ3400_keyjhapk")) ||
                        (sensorNode->manufacturer() == QLatin1String("_TYZB02_key8kk7r")) ||
                        (sensorNode->manufacturer() == QLatin1String("_TZ3400_keyjqthh")) ||
                        (sensorNode->manufacturer() == QLatin1String("_TZ3400_key8kk7r")) ||
                        (sensorNode->manufacturer() == QLatin1String("_TZ3000_vp6clf9d")) ||
                        (sensorNode->manufacturer() == QLatin1String("_TZ3000_peszejy7")) ||
                        (sensorNode->manufacturer() == QLatin1String("_TZ3000_qzjcsmar")) ||
                        (sensorNode->manufacturer() == QLatin1String("_TZ3000_owgcnkrh")) ||
                        (sensorNode->manufacturer() == QLatin1String("_TZ3000_adkvzooy")) ||
                        (sensorNode->manufacturer() == QLatin1String("_TZ3000_arfwfgoa")) ||
                        (sensorNode->manufacturer() == QLatin1String("_TYZB02_keyjqthh")))
                    {
                        sensorNode = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), 0x01);
                    }
                    else if ((ind.srcEndpoint() == 0x06 || ind.srcEndpoint() == 0x07) && sensorNode->modelId() == QLatin1String("lumi.ctrl_ln2.aq1"))
                    {
                        // TODO Button maps should express one ZHASwitch is related to multiple endpoints.
                        //      Or search for one ZHASwitch resource inside sensors.
                        sensorNode = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), 0x05);
                    }
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
        {
            // Safeguard to issue a 2nd active endpoint request in case the first one got MIA
            // Temporary workaround till state machine is available (request is fired ruthless)
            deCONZ::ApsDataRequest apsReq;

            // ZDP Header
            apsReq.dstAddress() = ind.srcAddress();
            apsReq.setDstAddressMode(deCONZ::ApsNwkAddress);
            apsReq.setDstEndpoint(ZDO_ENDPOINT);
            apsReq.setSrcEndpoint(ZDO_ENDPOINT);
            apsReq.setProfileId(ZDP_PROFILE_ID);
            apsReq.setRadius(0);
            apsReq.setClusterId(ZDP_ACTIVE_ENDPOINTS_CLID);

            QDataStream stream(&apsReq.asdu(), QIODevice::WriteOnly);
            stream.setByteOrder(QDataStream::LittleEndian);

            stream << zclSeq++;
            stream << ind.srcAddress().nwk();

            apsCtrl->apsdeDataRequest(apsReq);  // Fire and forget

            handleNodeDescriptorResponseIndication(ind);
            handleIndicationSearchSensors(ind, zclFrame);
        }
            break;

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
                if (gwPermitJoinDuration > 0)
                {
                    break;
                }

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
        else if (task.lightNode && gwPermitJoinDuration == 0)
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

    /*
        Friends of Hue switch

        A0 B0
        A1 B1

        DeviceId 0x02 (On/Off Switch)


             A0,B0 Press    0x64 Press   2 of 2
             A0,B0 Release  0x65 Release 2 of 2

        A0 0x10 Press   Scene0      B0 0x13 Press   Scene 3
        A0 0x14 Release Scene4      B0 0x17 Release Scene 7

        A1 0x11 Press   Scene1      B1 0x12 Press   Scene 2
        A1 0x15 Release Scene5      B1 0x16 Release Scene 6

             A1,B1 Press    0x62 Press   1 of 2
             A1,B1 Release  0x63 Release 1 of 2

     */

    Sensor *sensor = getSensorNodeForAddress(ind.gpdSrcId());
    ResourceItem *item = sensor ? sensor->item(RStateButtonEvent) : nullptr;

    if (!sensor || !item || sensor->deletedState() == Sensor::StateDeleted)
    {
        return;
    }
    sensor->rx();

    quint32 btn = ind.gpdCommandId();
    if (sensor->modelId() == QLatin1String("FOHSWITCH"))
    {
        // Map the command to the mapped button and action.
        // PTM215ZE Friends of Hue switch.
        quint32 buttonMapPTM215ZE[] = {
            0x12, S_BUTTON_1, S_BUTTON_ACTION_INITIAL_PRESS,
            0x13, S_BUTTON_1, S_BUTTON_ACTION_SHORT_RELEASED,
            0x14, S_BUTTON_2, S_BUTTON_ACTION_INITIAL_PRESS,
            0x15, S_BUTTON_2, S_BUTTON_ACTION_SHORT_RELEASED,
            0x18, S_BUTTON_3, S_BUTTON_ACTION_INITIAL_PRESS,
            0x19, S_BUTTON_3, S_BUTTON_ACTION_SHORT_RELEASED,
            0x22, S_BUTTON_4, S_BUTTON_ACTION_INITIAL_PRESS,
            0x23, S_BUTTON_4, S_BUTTON_ACTION_SHORT_RELEASED,
            0
        };
        // Generic Friends of Hue switch.
        quint32 buttonMapFOHSWITCH[] = {
            0x10, S_BUTTON_1, S_BUTTON_ACTION_INITIAL_PRESS,
            0x14, S_BUTTON_1, S_BUTTON_ACTION_SHORT_RELEASED,
            0x11, S_BUTTON_2, S_BUTTON_ACTION_INITIAL_PRESS,
            0x15, S_BUTTON_2, S_BUTTON_ACTION_SHORT_RELEASED,
            0x13, S_BUTTON_3, S_BUTTON_ACTION_INITIAL_PRESS,
            0x17, S_BUTTON_3, S_BUTTON_ACTION_SHORT_RELEASED,
            0x12, S_BUTTON_4, S_BUTTON_ACTION_INITIAL_PRESS,
            0x16, S_BUTTON_4, S_BUTTON_ACTION_SHORT_RELEASED,
            0x64, S_BUTTON_5, S_BUTTON_ACTION_INITIAL_PRESS,
            0x65, S_BUTTON_5, S_BUTTON_ACTION_SHORT_RELEASED,
            0x62, S_BUTTON_6, S_BUTTON_ACTION_INITIAL_PRESS,
            0x63, S_BUTTON_6, S_BUTTON_ACTION_SHORT_RELEASED,
            0x68, S_BUTTON_7, S_BUTTON_ACTION_INITIAL_PRESS,
            0xe0, S_BUTTON_7, S_BUTTON_ACTION_SHORT_RELEASED,
            0
        };
        quint32* buttonMap = 0;
        // Determine which button map to use.
        if (sensor->swVersion() == QLatin1String("PTM215ZE"))
        {
            buttonMap = buttonMapPTM215ZE;
        }
        else {
            buttonMap = buttonMapFOHSWITCH;
        }

        quint32 btnMapped = 0;
        quint32 btnAction = 0;
        for (int i = 0; buttonMap[i] != 0; i += 3)
        {
            if (buttonMap[i] == btn)
            {
                btnMapped = buttonMap[i + 1];
                btnAction = buttonMap[i + 2];
                break;
            }
        }

        const QDateTime now = QDateTime::currentDateTime();
        if (btnMapped == 0)
        {
            // not found
        }
        else if (btnAction == S_BUTTON_ACTION_INITIAL_PRESS)
        {
            sensor->durationDue = now.addMSecs(500); // enable generation of x001 (hold)
            checkSensorsTimer->start(CHECK_SENSOR_FAST_INTERVAL);
            btn = btnMapped + S_BUTTON_ACTION_INITIAL_PRESS;
        }
        else if (btnAction == S_BUTTON_ACTION_SHORT_RELEASED)
        {
            sensor->durationDue = QDateTime(); // disable generation of x001 (hold)
            btn = buttonMap[btn & 0x0f];
            const quint32 action = item->toNumber() & 0x03; // last action

            if (action == S_BUTTON_ACTION_HOLD || // hold already triggered -> long release
                item->lastSet().msecsTo(now) > 400) // over 400 ms since initial press? -> long release
            {
                btn = btnMapped + S_BUTTON_ACTION_LONG_RELEASED;
            }
            else
            {
                btn = btnMapped + S_BUTTON_ACTION_SHORT_RELEASED;
            }
        }
        else if (btn == 0x68) // aka ShortPress2Of2
        {
            // finish commissioning by pressing button 2000 and 3000 simultaneously
            btn = btnMapped + S_BUTTON_ACTION_SHORT_RELEASED;
        }
        else if (btn == 0xe0) // aka commissioning
        {
            btn = btnMapped + S_BUTTON_ACTION_LONG_RELEASED;
        }
    }

    updateSensorEtag(sensor);
    sensor->updateStateTimestamp();
    item->setValue(btn);
    DBG_Printf(DBG_ZGP, "ZGP button %u %s\n", item->toNumber(), qPrintable(sensor->modelId()));
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
    case deCONZ::GpCommandIdShortPress1Of1:
    case deCONZ::GpCommandIdShortPress1Of2:
    case deCONZ::GpCommandIdShortPress2Of2:
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

        // Philips Hue Tap
        // 0x02               GPD DeviceID
        // 0x81               Options (MAC Sequence number, extended options field)
        // 0xF2               Extended Options Field: (Key Type: Individual, out of the box GPD key, GPD Key Present, GPD Key Encryption, GPD Outgoing present)
        // 16 Security Key  GPD Key
        // 4  u32           GPD Key MIC
        // 4  u32           GPD outgoing counter

        // Vimar (Friends of Hue) 4 button 03906
        // Note 1: Niko and Busch-Jaeger Friends of Hue switches use the same module.
        // Note 2: Hue bridge uses modelid FOHSWITCH for Friends of Hue switches.
        // 0x02               GPD DeviceID
        // 0xC5               Options (MAC Sequence number, application information present, fixed location, extended options field)
        // 0xF2               Extended Options Field: (Key Type: Individual, out of the box GPD key, GPD Key Present, GPD Key Encryption, GPD Outgoing present)
        // 16   Security Key  GPD Key
        // 4    u32           GPD Key MIC
        // 4    u32           GPD outgoing counter
        // 0x04               ApplicationInformation (GPD commands are present)
        // ManufacturerSpecific (18 byte)
        // Number of GPD commands (1 byte): 0x11 (17)
        // GPD CommandID list (17 byte): 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x22, 0x60, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68

        // Illumra (ZBT-S1AWH and ZBT-S2AWH)
        // Uses the more advanced PTM 215ZE module
        // 0x02               GPD DeviceID
        // 0x81               Options (MAC Sequence number, application information present, extended options field)
        // 0xF2               Extended Options Field: (Key Type: Individual, out of the box GPD key, GPD Key Present, GPD Key Encryption, GPD Outgoing present)
        // 16   Security Key  GPD Key
        // 4    u32           GPD Key MIC
        // 4    u32           GPD outgoing counter
        // Commissioning package is identical to Hue tab, however PTM 215ZE has a very different command set to above two modules

        // TODO: Capture all received commands and store them in ordered string.
        //       This way we can distinguish between the modules.
        // Note: It would be better to map things to the usual 1002, 1001, 1003, ... buttonevents
        //       Maybe except for Hue Tap to keep compatibility? -- Maybe.

        quint8 gpdDeviceId;
        GpKey_t gpdKey = { 0 };
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
                stream >> gpdKey.at(i);
            }

            if (extOptions.bits.gpdKeyEncryption)
            {
                // When GPDkeyPresent sub-field is set
                // to 0b1 and the GPDkeyEncryption sub-field is set to 0b1, both fields GPDkey and GPDkeyMIC are
                // present; the field GPDkey contains the gpdSecurityKey, of the type as indicated in the gpdSecurityKey-
                // Type, encrypted with the default TC-LK (see A.3.3.3.3) as described inA.3.7.1.2.3; and the GPDk-
                // eyMIC field contains the MIC for the encrypted GPD key, calculated as described in A.3.7.1.2.3.
                if (stream.atEnd()) { return; }

                gpdKey = GP_DecryptSecurityKey(ind.gpdSrcId(), gpdKey);

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

        Sensor *sensor = getSensorNodeForFingerPrint(ind.gpdSrcId(), fp, QLatin1String("ZGPSwitch"));

#if DECONZ_LIB_VERSION >= 0x011000
        if (searchSensorsState == SearchSensorsActive && extOptions.bits.gpdKeyEncryption && ind.gppShortAddress() != 0)
        {
            if (searchSensorGppPairCounter < GP_MAX_PROXY_PAIRINGS && ind.gppLqi() >= deCONZ::GppGpdLqiModerate)
            {
                if (GP_SendPairing(ind.gpdSrcId(), GP_DEFAULT_PROXY_GROUP, gpdDeviceId, gpdOutgoingCounter, gpdKey, apsCtrl, zclSeq++, ind.gppShortAddress()))
                {
                    searchSensorGppPairCounter++;
                }
            }
        }
#endif

        if (!sensor || sensor->deletedState() == Sensor::StateDeleted)
        {
            if (searchSensorsState != SearchSensorsActive)
            {
                return;
            }

            // create new sensor
            Sensor sensorNode;
            sensorNode.setType("ZGPSwitch");

            // https://github.com/dresden-elektronik/deconz-rest-plugin/pull/3285
            // Illumra Dual Rocker Switch PTM215ZE
            if (gpdDeviceId == deCONZ::GpDeviceIdOnOffSwitch && options.byte == 0x81 && ind.payload().size() == 27 && (ind.gpdSrcId() & 0x01500000) == 0x01500000)
            {
                sensorNode.setModelId("FOHSWITCH");
                sensorNode.setManufacturer("PhilipsFoH");
                sensorNode.setSwVersion("PTM215ZE");
            }
            else if (gpdDeviceId == deCONZ::GpDeviceIdOnOffSwitch && options.byte == 0x81)
            {
                sensorNode.setModelId("ZGPSWITCH");
                sensorNode.setManufacturer("Philips");
                sensorNode.setSwVersion("1.0");
            }
            else if (gpdDeviceId == deCONZ::GpDeviceIdOnOffSwitch && options.byte == 0xc5 && ind.payload().size() == 46)
            {
                sensorNode.setModelId("FOHSWITCH");
                sensorNode.setManufacturer("PhilipsFoH");
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
            sensorNode.rx();

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
            gpProcessButtonEvent(ind);
        }
        else if (sensor && sensor->deletedState() == Sensor::StateNormal)
        {
            if (searchSensorsState == SearchSensorsActive)
            {
                gpProcessButtonEvent(ind);
            }
        }
        else
        {
            DBG_Printf(DBG_INFO, "SensorNode %s already known\n", qPrintable(sensor->name()));
        }
    }
        break;

    default:
    {
        DBG_Printf(DBG_ZGP, "ZGP unhandled command gpdsrcid %u: gpdcmdid: 0x%02X\n", ind.gpdSrcId(), ind.gpdCommandId());
    }
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
    error["address"] = ressource.toHtmlEscaped();
    error["description"] = description.toHtmlEscaped();
    map["error"] = error;

    DBG_Printf(DBG_INFO_L2, "API error %d, %s, %s\n", id, qPrintable(ressource), qPrintable(description));

    return map;
}

/*! Creates a new unique ETag for a resource.
 */
void DeRestPluginPrivate::updateEtag(QString &etag)
{
    QDateTime time = QDateTime::currentDateTime();
#if QT_VERSION < 0x050000
    etag = QString(QCryptographicHash::hash(time.toString("yyyy-MM-ddThh:mm:ss.zzz").toAscii(), QCryptographicHash::Md5).toHex());
#else
    etag = QString(QCryptographicHash::hash(time.toString("yyyy-MM-ddThh:mm:ss.zzz").toLatin1(), QCryptographicHash::Md5).toHex());
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

        checkIasEnrollmentStatus(&s);

        if (s.lastAttributeReportBind() < (idleTotalCounter - IDLE_ATTR_REPORT_BIND_LIMIT))
        {
            if (checkSensorBindingsForAttributeReporting(&s))
            {
                s.setLastAttributeReportBind(idleTotalCounter);
            }
        }
    }

    for (auto &l : nodes)
    {
        if (l.state() != LightNode::StateNormal)
        {
            continue;
        }

        if (l.address().ext() != event.node()->address().ext())
        {
            continue;
        }

        l.rx();

        // FIXME: probably need to do some more light stuff here.
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
        node->nodeDescriptor().manufacturerCode() == VENDOR_XIAOMI || // Xiaomi lumi.curtain.hagl04
        node->nodeDescriptor().manufacturerCode() == VENDOR_EMBER || // atsmart Z6-03 switch + Heiman plug + Tuya stuff
        (!node->nodeDescriptor().isNull() && node->nodeDescriptor().manufacturerCode() == VENDOR_NONE) || // Climax Siren
        node->nodeDescriptor().manufacturerCode() == VENDOR_DEVELCO || // Develco Smoke sensor with siren
        node->nodeDescriptor().manufacturerCode() == VENDOR_LDS || // Samsung SmartPlug 2019
        node->nodeDescriptor().manufacturerCode() == VENDOR_IKEA || // IKEA FYRTUR and KADRILJ smart binds
        node->nodeDescriptor().manufacturerCode() == VENDOR_THIRD_REALITY || // Third Reality smart light switch
        node->nodeDescriptor().manufacturerCode() == VENDOR_AXIS || // Axis shade
        node->nodeDescriptor().manufacturerCode() == VENDOR_MMB || // Axis shade
        // Danalock support. The vendor ID (0x115c) needs to defined and whitelisted, as it's battery operated
        node->nodeDescriptor().manufacturerCode() == VENDOR_DANALOCK || // Danalock Door Lock
        node->nodeDescriptor().manufacturerCode() == VENDOR_KWIKSET || // Kwikset 914 ZigBee smart lock
        // Schlage support. The vendor ID (0x1236) needs to defined and whitelisted, as it's battery operated
        node->nodeDescriptor().manufacturerCode() == VENDOR_SCHLAGE)
    {
        // whitelist
    }
    else if (!node->nodeDescriptor().receiverOnWhenIdle())
    {
        return;
    }

    bool hasTuyaCluster = false;
    QString manufacturer;

    //Make 2 fakes device for tuya switches
    if (node->nodeDescriptor().manufacturerCode() == VENDOR_EMBER && !node->simpleDescriptors().isEmpty())
    {
        const deCONZ::SimpleDescriptor *sd = &node->simpleDescriptors()[0];
        bool hasColorCluster = false;

        if (sd && (sd->deviceId() == DEV_ID_SMART_PLUG) && (node->simpleDescriptors().size() < 2) &&
        (((node->address().ext() & 0xffffff0000000000ULL ) == silabs3MacPrefix) ||
         ((node->address().ext() & 0xffffff0000000000ULL ) == silabs4MacPrefix))
        )
        {

            for (int c = 0; c < sd->inClusters().size(); c++)
            {
                if (sd->inClusters()[c].id() == TUYA_CLUSTER_ID) { hasTuyaCluster = true; }
                if (sd->inClusters()[c].id() == COLOR_CLUSTER_ID) { hasColorCluster = true; }
            }

            if  (hasTuyaCluster && !hasColorCluster)
            {
                DBG_Printf(DBG_INFO, "Tuya : Creating 2 Fake Endpoints\n");

                //Ok it's the good device, make 2 clones with differents endpoints

                //node is not modifiable (WHY ?) so use an ugly way
                deCONZ::Node *NodePachable = const_cast<deCONZ::Node*>(&*node);

                deCONZ::SimpleDescriptor sd1;
                deCONZ::SimpleDescriptor sd2;

                const deCONZ::SimpleDescriptor &csd1 = sd1;
                const deCONZ::SimpleDescriptor &csd2 = sd2;

                node->copySimpleDescriptor(0x01, &sd1);
                node->copySimpleDescriptor(0x01, &sd2);

                sd1.setEndpoint(0x02);
                sd2.setEndpoint(0x03);

                //remove useless cluster
                if (false)
                {
					QList<deCONZ::ZclCluster> &cl = sd1.inClusters();
					cl.clear();

					for (const deCONZ::ZclCluster &cl2 : sd2.inClusters())
					{
						if (cl2.id() == TUYA_CLUSTER_ID)
						{
							cl.append(cl2);
						}
					}
			    }

                NodePachable->setSimpleDescriptor(csd1);
                NodePachable->setSimpleDescriptor(csd2);

                // Update node
                apsCtrl->updateNode(*NodePachable);

            }
        }
    }

    QList<deCONZ::SimpleDescriptor>::const_iterator i = node->simpleDescriptors().constBegin();
    QList<deCONZ::SimpleDescriptor>::const_iterator end = node->simpleDescriptors().constEnd();

    for (;i != end; ++i)
    {
        bool hasServerOnOff = false;
        bool hasServerLevel = false;
        bool hasServerColor = false;
        bool hasIASWDCluster = false;

        for (int c = 0; c < i->inClusters().size(); c++)
        {
            if      (i->inClusters()[c].id() == ONOFF_CLUSTER_ID) { hasServerOnOff = true; }
            else if (i->inClusters()[c].id() == LEVEL_CLUSTER_ID) { hasServerLevel = true; }
            else if (i->inClusters()[c].id() == COLOR_CLUSTER_ID) { hasServerColor = true; }
            else if (i->inClusters()[c].id() == WINDOW_COVERING_CLUSTER_ID) { hasServerOnOff = true; }
            else if (i->inClusters()[c].id() == IAS_WD_CLUSTER_ID) { hasIASWDCluster = true; }
            else if ((i->inClusters()[c].id() == TUYA_CLUSTER_ID) && (node->macCapabilities() & deCONZ::MacDeviceIsFFD) ) { hasServerOnOff = true; }
            // Danalock support. The cluster needs to be defined and whitelisted by setting hasServerOnOff
            else if (node->nodeDescriptor().manufacturerCode() == VENDOR_DANALOCK && i->inClusters()[c].id() == DOOR_LOCK_CLUSTER_ID) { hasServerOnOff = true; }
            else if (node->nodeDescriptor().manufacturerCode() == VENDOR_SCHLAGE && i->inClusters()[c].id() == DOOR_LOCK_CLUSTER_ID) { hasServerOnOff = true; } //Schlage Connect Smart Deadbolt B3468
            else if (node->nodeDescriptor().manufacturerCode() == VENDOR_KWIKSET && i->inClusters()[c].id() == DOOR_LOCK_CLUSTER_ID) { hasServerOnOff = true; } //Kwikset 914 ZigBee smart lock
            else if (i->inClusters()[c].id() == BASIC_CLUSTER_ID)
            {
                std::vector<deCONZ::ZclAttribute>::const_iterator j = i->inClusters()[c].attributes().begin();
                std::vector<deCONZ::ZclAttribute>::const_iterator jend = i->inClusters()[c].attributes().end();

                for (; j != jend; ++j)
                {
                    if (manufacturer.isEmpty() && j->id() == 0x0004) // manufacturer id
                    {
                        manufacturer = j->toString().trimmed();
                    }
                }

            }
        }

        // check if node already exist
        LightNode *lightNode2 = nullptr;

        for (auto &l : nodes)
        {
            if (l.state() != LightNode::StateNormal)
            {
                continue;
            }

            if (!node->address().hasExt() || node->address().ext() != l.address().ext())
            {
                continue;
            }

            if (l.haEndpoint().endpoint() != i->endpoint())
            {
                continue;
            }

            lightNode2 = &l;
            break;
        }

        if (lightNode2)
        {
            DBG_Assert(lightNode2->state() != LightNode::StateDeleted);

            if (lightNode2->node() != node)
            {
                lightNode2->setNode(const_cast<deCONZ::Node*>(node));
                DBG_Printf(DBG_INFO, "LightNode %s set node %s\n", qPrintable(lightNode2->id()), qPrintable(node->address().toStringExt()));
            }

            lightNode2->setManufacturerCode(node->nodeDescriptor().manufacturerCode());
            ResourceItem *reachable = lightNode2->item(RStateReachable);

            DBG_Assert(reachable);
            bool avail = !node->isZombie() && lightNode2->lastRx().isValid();
            if (!reachable->toBool() && avail)
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

                    lightNode2->setNeedSaveDatabase(true);
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

            continue;
        }

        LightNode lightNode;
        lightNode.setNode(nullptr);
        lightNode.item(RStateReachable)->setValue(true);

        Q_Q(DeRestPlugin);
        lightNode.setNode(const_cast<deCONZ::Node*>(node));
        lightNode.address() = node->address();
        lightNode.setManufacturerCode(node->nodeDescriptor().manufacturerCode());

        // For Tuya, we realy need manufacture Name, but can't use it to compare because of fonction setManufacturerCode() that put "Heiman",
        if (node->nodeDescriptor().isNull() || node->simpleDescriptors().empty())
        { }
        else if (node->nodeDescriptor().manufacturerCode() == VENDOR_NONE || (node->nodeDescriptor().manufacturerCode() == VENDOR_EMBER))
        {
            if (manufacturer.isEmpty())
            {
                DBG_Printf(DBG_INFO_L2, "Tuya debug 7 : Missing manufacture name for 0x%016llx\n", node->address().ext());

                //searching in DB
                openDb();
                manufacturer = loadDataForLightNodeFromDb(generateUniqueId(node->address().ext(),0,0));
                closeDb();

                if (manufacturer.isEmpty())
                {
                    // extract from sensor if possible
                    Sensor *sensor = getSensorNodeForAddress(node->address());
                    if (sensor && !sensor->manufacturer().isEmpty())
                    {
                        manufacturer = sensor->manufacturer();
                    }
                }

				if (manufacturer.isEmpty())
				{
					DBG_Printf(DBG_INFO_L2, "Tuya debug 7 : Missing manufacture name, till missing in DB.\n");
				}
            }
            if (!manufacturer.isEmpty())
            {
                lightNode.setManufacturerName(manufacturer);
                lightNode.setNeedSaveDatabase(true);
            }
        }

        //VENDOR_NONE only use device with 2 cluster ? or perhaps VENDOR_EMBER too
        if (!node->nodeDescriptor().isNull() && node->nodeDescriptor().manufacturerCode() == VENDOR_NONE)
        {
            //General method to detect tuya cluster
            if ((i->inClusters().size() == 2) && (i->endpoint() == 0x01) )
            {
                hasServerOnOff = true;
            }
            //Tuya white list
            // _TYST11_wmcdj3aq is covering with cluster 0x0006
            // _TYST11_xu1rkty3 is covering with only 2 clusters
            // _TYST11_d0yu2xgi siren with only 2 clusters
            if (lightNode.manufacturer() == QLatin1String("_TYST11_xu1rkty3") ||
                R_GetProductId(&lightNode) == QLatin1String("NAS-AB02B0 Siren"))
            {
                hasServerOnOff = true;
            }
            //Tuya black list
            // _TYST11_ckud7u2l is valve with 2 cluster
            // _TYST11_jeaxp72v same
            // _TYST11_kfvq6avy same, _TZE200_kfvq6avy
            // _TYST11_c88teujp same
            // _TYST11_zuhszj9s same
            // _TYST11_yw7cahqs same
            // _TZE200_ywdxldoj same
            if (R_GetProductId(&lightNode).startsWith(QLatin1String("Tuya_THD")))
            {
                hasServerOnOff = false;
            }
        }
        if (node->nodeDescriptor().manufacturerCode() == VENDOR_EMBER)
        {
            //Tuya black list
            //_TZE200_aoclfnxz is a thermostat
            //_TZE200_c88teujp same
            if (R_GetProductId(&lightNode).startsWith(QLatin1String("Tuya_THD")))
            {
                hasServerOnOff = false;
            }
            //Battery covering
            if (lightNode.manufacturer() == QLatin1String("_TZE200_wmcdj3aq") ||
                lightNode.manufacturer() == QLatin1String("_TZE200_xuzcvlku") || // Zemismart Chain Roller Shades Driver M515EGB (TS0601)
                lightNode.manufacturer() == QLatin1String("_TZE200_zah67ekd")) // MoesHouse / Livolo Roller Blinds
            {
                hasServerOnOff = true;
            }
            //wireless switch
            if (lightNode.manufacturer() == QLatin1String("_TZ3000_bi6lpsew") ||
                lightNode.manufacturer() == QLatin1String("_TZ3400_keyjhapk") ||
                lightNode.manufacturer() == QLatin1String("_TYZB02_key8kk7r") ||
                lightNode.manufacturer() == QLatin1String("_TZ3400_keyjqthh") ||
                lightNode.manufacturer() == QLatin1String("_TZ3400_key8kk7r") ||
                lightNode.manufacturer() == QLatin1String("_TZ3000_vp6clf9d") ||
                lightNode.manufacturer() == QLatin1String("_TZ3000_peszejy7") ||
                lightNode.manufacturer() == QLatin1String("_TZ3000_qzjcsmar") ||
                lightNode.manufacturer() == QLatin1String("_TZ3000_owgcnkrh") ||
                lightNode.manufacturer() == QLatin1String("_TZ3000_arfwfgoa") ||
                lightNode.manufacturer() == QLatin1String("_TYZB02_keyjqthh"))
            {
                hasServerOnOff = false;
            }
        }

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
                case DEV_ID_HA_WINDOW_COVERING_CONTROLLER:
                // Danalock support. The device id (0x000a) needs to be defined and whitelisted
                case DEV_ID_DOOR_LOCK:
                {
                    if (hasServerOnOff)
                    {
                        lightNode.setHaEndpoint(*i);
                    }
                }
                break;
                case DEV_ID_DOOR_LOCK_UNIT: // ORVIBO Zigbee Dry Contact CM10ZW
                {
                    if (!node->nodeDescriptor().isNull() && node->nodeDescriptor().manufacturerCode() == VENDOR_NONE)
                    {
                        if (hasServerOnOff)
                        {
                            lightNode.setHaEndpoint(*i);
                        }
                    }
                }
                break;

                case DEV_ID_FAN:
                {
                    if (hasServerOnOff)
                    {
                        lightNode.setHaEndpoint(*i);
                    }
                }
                break;

                case DEV_ID_MAINS_POWER_OUTLET:
                case DEV_ID_HA_ONOFF_LIGHT:
                case DEV_ID_LEVEL_CONTROL_SWITCH:
                case DEV_ID_ONOFF_OUTPUT:
                case DEV_ID_LEVEL_CONTROLLABLE_OUTPUT:
                case DEV_ID_HA_DIMMABLE_LIGHT:
                case DEV_ID_HA_ONOFF_LIGHT_SWITCH:
                case DEV_ID_HA_DIMMER_SWITCH:
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
                            if (checkMacAndVendor(node, VENDOR_JENNIC) &&
                                // prevent false positives like Immax IM-Z3.0-DIM which has only two endpoints (0x01)
                                // lumi.ctrl_neutral1 and lumi.ctrl_neutral2 have more 5 endpoints
                                node->simpleDescriptors().size() > 5  &&
                                i->endpoint() != 0x02 && i->endpoint() != 0x03)
                            {
                                // TODO better filter for lumi. devices (i->deviceId(), modelid?)
                                // blacklist switch endpoints for lumi.ctrl_neutral1 and lumi.ctrl_neutral2
                                DBG_Printf(DBG_INFO, "Skip load endpoint 0x%02X for 0x%016llX (expect: lumi.ctrl_neutral1 / lumi.ctrl_neutral2)\n", i->endpoint(), node->address().ext());
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

                case DEV_ID_RANGE_EXTENDER:
                    {
                        if (node->nodeDescriptor().manufacturerCode() == VENDOR_IKEA)
                        {
                            lightNode.setHaEndpoint(*i);
                        }
                    }
                    break;

                case DEV_ID_CONFIGURATION_TOOL:
                    {
                        if (node->nodeDescriptor().manufacturerCode() == VENDOR_DDEL)
                        {
                            lightNode.setHaEndpoint(*i);
                        }
                    }
                    break;

                case DEV_ID_XIAOMI_SMART_PLUG:
                    {
                        if (node->nodeDescriptor().manufacturerCode() == VENDOR_XIAOMI &&
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

                case DEV_ID_IAS_ZONE:
                    {
                        if (hasIASWDCluster)
                        {
                            lightNode.setHaEndpoint(*i);
                        }
                    }
                    break;

                case DEV_ID_IAS_WARNING_DEVICE:
                    {
                        lightNode.setHaEndpoint(*i);
                    }
                    break;

                case DEV_ID_CONSUMPTION_AWARENESS_DEVICE:
                    {
                        if (node->nodeDescriptor().manufacturerCode() == VENDOR_LEGRAND)
                        {
                            lightNode.setHaEndpoint(*i);
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
        else if (i->profileId() == DIN_PROFILE_ID)
        {
            switch (i->deviceId())
            {
            case DEV_ID_DIN_XBEE:
            {
                if (i->endpoint() == DIN_DDO_ENDPOINT)
                {
                    lightNode.setHaEndpoint(*i);
                }
            }
                break;
            default:
                break;
            }
        }

        if (!lightNode.haEndpoint().isValid())
        {
            continue;
        }

        QString uid = generateUniqueId(lightNode.address().ext(), lightNode.haEndpoint().endpoint(), 0);
        lightNode.setUniqueId(uid);

        if (existDevicesWithVendorCodeForMacPrefix(node->address(), VENDOR_DDEL) && i->deviceId() != DEV_ID_CONFIGURATION_TOOL && node->nodeDescriptor().manufacturerCode() == VENDOR_DDEL)
        {
            ResourceItem *item = lightNode.addItem(DataTypeUInt32, RConfigPowerup);
            DBG_Assert(item != 0);
            item->setValue(R_POWERUP_RESTORE | R_POWERUP_RESTORE_AT_DAYLIGHT | R_POWERUP_RESTORE_AT_NO_DAYLIGHT);
        }

        openDb();
        loadLightNodeFromDb(&lightNode);
        closeDb();

        setLightNodeStaticCapabilities(&lightNode);

        DBG_Assert(lightNode.state() != LightNode::StateDeleted);

        if (lightNode.manufacturerCode() == VENDOR_XIAOMI)
        {
            if (lightNode.manufacturer() != QLatin1String("LUMI"))
            {
                lightNode.setManufacturerName(QLatin1String("LUMI"));
                lightNode.setNeedSaveDatabase(true);
            }
        }

        if (lightNode.manufacturerCode() == VENDOR_MAXSTREAM)
        {
            lightNode.setManufacturerName(QLatin1String("Digi"));
            lightNode.setModelId(QLatin1String("XBee"));
            lightNode.setNeedSaveDatabase(true);
        }

        ResourceItem *reachable = lightNode.item(RStateReachable);
        DBG_Assert(reachable);
        if (reachable) //  might have been set to false after reload
        {
            if (!reachable->toBool())
            {
                reachable->setValue(!node->isZombie() && lightNode.lastRx().isValid());
            }
        }

        if (lightNode.id().isEmpty())
        {
            if (deCONZ::appArgumentNumeric("--always-add-lights", 0) == 1)
            {
            }
            else if (!(searchLightsState == SearchLightsActive || permitJoinFlag))
            {
                // don't add new light node when search is not active
                return;
            }

            openDb();
            lightNode.setId(QString::number(getFreeLightId()));
            closeDb();
            lightNode.setNeedSaveDatabase(true);
        }

        if (checkMacAndVendor(node, VENDOR_OSRAM) || checkMacAndVendor(node, VENDOR_OSRAM_STACK))
        {
            if (lightNode.manufacturer() != QLatin1String("OSRAM"))
            {
                lightNode.setManufacturerName(QLatin1String("OSRAM"));
                lightNode.setNeedSaveDatabase(true);
            }
        }

        if (existDevicesWithVendorCodeForMacPrefix(node->address(), VENDOR_PHILIPS))
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
                lightNode.setName(QString("%1 %2").arg(lightNode.type()).arg(lightNode.id()));

            if (!lightNode.name().isEmpty())
            { q->nodeUpdated(lightNode.address().ext(), QLatin1String("name"), lightNode.name()); }

            if (!lightNode.swBuildId().isEmpty())
            { q->nodeUpdated(lightNode.address().ext(), QLatin1String("version"), lightNode.swBuildId()); }

            if (!lightNode.manufacturer().isEmpty())
            { q->nodeUpdated(lightNode.address().ext(), QLatin1String("vendor"), lightNode.manufacturer()); }

            if (!lightNode.modelId().isEmpty())
            { q->nodeUpdated(lightNode.address().ext(), QLatin1String("modelid"), lightNode.modelId()); }
        }

        //Add missing values for Profalux device
        if (existDevicesWithVendorCodeForMacPrefix(node->address(), VENDOR_PROFALUX))
        {
            //Shutter ?
            if (i->deviceId() == DEV_ID_ZLL_COLOR_LIGHT)
            {
                lightNode.setManufacturerName(QLatin1String("Profalux"));
                lightNode.setModelId(QLatin1String("PFLX Shutter"));
                lightNode.setNeedSaveDatabase(true);
            }
        }

        //Add missing field for Tuya Device with tuya cluster
        // Window covering
        if (lightNode.manufacturer() == QString("_TYST11_xu1rkty3") ||
            lightNode.manufacturer() == QString("_TZE200_xuzcvlku") ||
            lightNode.manufacturer() == QString("_TZE200_wmcdj3aq") ||
            lightNode.manufacturer() == QString("_TZE200_nogaemzt") ||
            lightNode.manufacturer() == QString("_TZE200_zah67ekd") || // MoesHouse / Livolo Roller Blinds
            lightNode.manufacturer() == QString("_TZE200_fzo2pocs") ||
            lightNode.manufacturer() == QString("_TYST11_wmcdj3aq"))
        {
            lightNode.addItem(DataTypeBool, RStateOpen);
            lightNode.addItem(DataTypeUInt8, RStateLift);
            lightNode.addItem(DataTypeUInt8, RStateBri);

            ResourceItem *type = lightNode.item(RAttrType);
            DBG_Assert(type);
            if (type)
            {
                type->setValue(QString("Window covering device"));
            }
            lightNode.setNeedSaveDatabase(true);
        }

        //Siren
        if (R_GetProductId(&lightNode) == QLatin1String("NAS-AB02B0 Siren"))
        {
            lightNode.removeItem(RStateOn);
            ResourceItem *type = lightNode.item(RAttrType);
            DBG_Assert(type);
            if (type)
            {
                type->setValue(QString("Warning device"));
            }
            lightNode.setNeedSaveDatabase(true);
        }

        // Tanslate Tuya ManufacturerName
        const lidlDevice *device = getLidlDevice(lightNode.manufacturer());
        if (device != nullptr)
        {
            lightNode.setManufacturerName(QLatin1String(device->manufacturername));
            lightNode.setModelId(QLatin1String(device->modelid));
            lightNode.setNeedSaveDatabase(true);
        }

        // "translate" ORVIBO vendor name
        if (lightNode.manufacturer() == QString("欧瑞博"))
        {
            lightNode.setManufacturerName(QLatin1String("ORVIBO"));
            lightNode.setNeedSaveDatabase(true);
        }
        // replace ORVIBO model IDs
        if (lightNode.modelId() == QLatin1String("abb71ca5fe1846f185cfbda554046cce"))
        {
            lightNode.setModelId(QLatin1String("T10D1ZW dimmer"));
            lightNode.setNeedSaveDatabase(true);
        }
        else if (lightNode.modelId() == QLatin1String("545df2981b704114945f6df1c780515a"))
        {
            lightNode.setModelId(QLatin1String("T10W1ZW switch"));
            lightNode.setNeedSaveDatabase(true);
        }
        else if (lightNode.modelId() == QLatin1String("82c167c95ed746cdbd21d6817f72c593"))
        {
            lightNode.setModelId(QLatin1String("CM10ZW"));
            lightNode.setNeedSaveDatabase(true);
        }

        // add light node to default group
        GroupInfo *groupInfo = getGroupInfo(&lightNode, gwGroup0);
        if (!groupInfo)
        {
            groupInfo = createGroupInfo(&lightNode, gwGroup0);
            lightNode.setNeedSaveDatabase(true);
            groupInfo->actions &= ~GroupInfo::ActionRemoveFromGroup; // sanity
            groupInfo->actions |= GroupInfo::ActionAddToGroup;
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
        lightNode.setLastAttributeReportBind(0);
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

/*! Adds known static values to a lightnode.
    \param lightNode - the LightNode to update
 */
void DeRestPluginPrivate::setLightNodeStaticCapabilities(LightNode *lightNode)
{
    DBG_Assert(lightNode);
    if (!lightNode)
    {
        return;
    }

    ResourceItem *item = nullptr;

    if (lightNode->manufacturerCode() == VENDOR_LEDVANCE &&
            (lightNode->modelId() == QLatin1String("BR30 RGBW") ||
            lightNode->modelId() == QLatin1String("RT RGBW") ||
            lightNode->modelId() == QLatin1String("A19 RGBW")))
    {
        item = lightNode->item(RAttrType);
        if (item)
        {
            item->setValue(QVariant("Color temperature light"));
        }
        if (lightNode->item(RConfigColorCapabilities) != nullptr)
        {
            return; // already initialized
        }
        lightNode->addItem(DataTypeUInt16, RStateCt);
        lightNode->addItem(DataTypeUInt16, RConfigCtMin)->setValue(142);
        lightNode->addItem(DataTypeUInt16, RConfigCtMax)->setValue(666);
        lightNode->addItem(DataTypeUInt16, RConfigColorCapabilities)->setValue(0x0001 | 0x0008 | 0x0010);
        lightNode->addItem(DataTypeString, RStateColorMode)->setValue(QVariant("ct"));
    }
    else if (lightNode->modelId() == QLatin1String("LIGHTIFY A19 RGBW"))
    {
        if (lightNode->item(RConfigColorCapabilities) != nullptr)
        {
            return; // already initialized
        }
        lightNode->addItem(DataTypeUInt16, RStateCt);
        // the light doesn't provide ctmin, ctmax and color capabilities attributes
        // however it supports the 'Move To Color Temperature' command and Color Temperature attribute
        lightNode->addItem(DataTypeUInt16, RConfigCtMin)->setValue(152);
        lightNode->addItem(DataTypeUInt16, RConfigCtMax)->setValue(689);
        // hue, saturation, color mode, xy, ct
        lightNode->addItem(DataTypeUInt16, RConfigColorCapabilities)->setValue(0x0001 | 0x0008 | 0x0010);
    }
    else if (lightNode->modelId() == QLatin1String("LIGHTIFY A19 Tunable White") ||
             lightNode->modelId() == QLatin1String("LIGHTIFY Conv Under Cabinet TW") ||
             lightNode->modelId() == QLatin1String("LIGHTIFY Under Cabinet TW") ||
             lightNode->modelId() == QLatin1String("LIGHTIFY BR Tunable White") ||
             lightNode->modelId() == QLatin1String("LIGHTIFY RT Tunable White") ||
             lightNode->modelId() == QLatin1String("LIGHTIFY Edge-lit Flushmount TW") ||
             lightNode->modelId() == QLatin1String("LIGHTIFY Surface TW") ||
             lightNode->modelId() == QLatin1String("A19 TW 10 year") ||
             lightNode->modelId() == QLatin1String("Classic B40 TW - LIGHTIFY") ||
             lightNode->modelId() == QLatin1String("Classic A60 TW") ||
             lightNode->modelId() == QLatin1String("Classic A60 TW") ||
             lightNode->modelId() == QLatin1String("Zigbee CCT Downlight") ||
             lightNode->modelId() == QLatin1String("Halo_RL5601") ||
             (lightNode->manufacturerCode() == VENDOR_LEDVANCE && lightNode->modelId() == QLatin1String("Down Light TW")) ||
             (lightNode->manufacturerCode() == VENDOR_LEDVANCE && lightNode->modelId() == QLatin1String("BR30 TW")) ||
             (lightNode->manufacturerCode() == VENDOR_LEDVANCE && lightNode->modelId() == QLatin1String("MR16 TW")) ||
             (lightNode->manufacturerCode() == VENDOR_LEDVANCE && lightNode->modelId() == QLatin1String("RT TW")))
    {
        item = lightNode->item(RAttrType);
        if (item && item->toString() == QLatin1String("Color dimmable light")) // some TW lights wrongly report as dimmable
        {
            item->setValue(QVariant("Color temperature light"));
        }

        if (lightNode->item(RConfigColorCapabilities) != nullptr)
        {
            return; // already initialized
        }
        lightNode->addItem(DataTypeUInt16, RStateCt);
        // these lights don't provide ctmin, ctmax and color capabilities attributes
        // however they support the 'Move To Color Temperature' command and Color Temperature attribute
        lightNode->addItem(DataTypeUInt16, RConfigCtMin)->setValue(153); // 6500K
        lightNode->addItem(DataTypeUInt16, RConfigCtMax)->setValue(370); // 2700K
        // color mode, xy, ct
        lightNode->addItem(DataTypeUInt16, RConfigColorCapabilities)->setValue(0x0008 | 0x0010);
        lightNode->addItem(DataTypeString, RStateColorMode)->setValue(QVariant("ct"));
        lightNode->removeItem(RStateHue);
        lightNode->removeItem(RStateSat);

        item = lightNode->item(RStateX);
        if (item) { item->setIsPublic(false); }
        item = lightNode->item(RStateY);
        if (item) { item->setIsPublic(false); }
    }
    else if (isXmasLightStrip(lightNode))
    {
        lightNode->removeItem(RStateAlert);
        lightNode->removeItem(RStateX);
        lightNode->removeItem(RStateY);
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
            if (i->state() != LightNode::StateNormal)
            {
                continue;
            }

            if (i->address().ext() != node->address().ext())
            {
                continue;
            }

            if (i->node() != node)
            {
                i->setNode(const_cast<deCONZ::Node*>(node));
                DBG_Printf(DBG_INFO, "LightNode %s set node %s\n", qPrintable(i->id()), qPrintable(node->address().toStringExt()));
            }

            ResourceItem *item = i->item(RStateReachable);
            DBG_Assert(item);
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
                    i->setNeedSaveDatabase(true);
                    item->setValue(available);
                    updateLightEtag(&*i);
                    Event e(RLights, RStateReachable, i->id(), item);
                    enqueueEvent(e);
                }
            }
        }
    }

    { // sensors
        std::vector<Sensor>::iterator i = sensors.begin();
        std::vector<Sensor>::iterator end = sensors.end();

        for (; i != end; ++i)
        {
            if (i->deletedState() != Sensor::StateNormal)
            {
                continue;
            }

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
        return nullptr;
    }

    bool updated = false;
    LightNode *lightNode = getLightNodeForAddress(event.node()->address(), event.endpoint());

    if (!lightNode)
    {
        // was no relevant node
        return nullptr;
    }

    if (lightNode->node() != event.node())
    {
        lightNode->setNode(const_cast<deCONZ::Node*>(event.node()));
        DBG_Printf(DBG_INFO, "LightNode %s set node %s\n", qPrintable(lightNode->id()), qPrintable(event.node()->address().toStringExt()));
    }

    if (lightNode->toBool(RStateReachable))
    {
        if ((event.node()->state() == deCONZ::FailureState) || event.node()->isZombie())
        {
            lightNode->setValue(RStateReachable, false);
        }
    }
    else
    {
        if (event.node()->state() != deCONZ::FailureState)
        {
            lightNode->setValue(RStateReachable, true);
        }
    }

    if (lightNode->isAvailable())
    {
        lightNode->rx();
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
            case DEV_ID_HA_ONOFF_LIGHT_SWITCH:
            case DEV_ID_HA_DIMMER_SWITCH:
            case DEV_ID_RANGE_EXTENDER:
            case DEV_ID_CONFIGURATION_TOOL:
            //case DEV_ID_ZLL_DIMMABLE_LIGHT: // same as DEV_ID_HA_ONOFF_LIGHT
            case DEV_ID_ZLL_DIMMABLE_PLUGIN_UNIT:
            case DEV_ID_Z30_DIMMABLE_PLUGIN_UNIT:
            case DEV_ID_HA_ONOFF_LIGHT:
            case DEV_ID_LEVEL_CONTROL_SWITCH:
            case DEV_ID_ONOFF_OUTPUT:
            case DEV_ID_LEVEL_CONTROLLABLE_OUTPUT:
            case DEV_ID_ZLL_ONOFF_LIGHT:
            case DEV_ID_ZLL_ONOFF_PLUGIN_UNIT:
            case DEV_ID_Z30_ONOFF_PLUGIN_UNIT:
            case DEV_ID_HA_WINDOW_COVERING_DEVICE:
            case DEV_ID_HA_WINDOW_COVERING_CONTROLLER:
            // Danalock support. The device id (0x000a) needs to be defined and whitelisted
            case DEV_ID_DOOR_LOCK:
            case DEV_ID_ZLL_ONOFF_SENSOR:
            case DEV_ID_XIAOMI_SMART_PLUG:
            case DEV_ID_IAS_ZONE:
            case DEV_ID_IAS_WARNING_DEVICE:
            case DEV_ID_FAN:
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
                if (isXmasLightStrip(lightNode))
                {
                    continue;
                }
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

                    lightNode->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());

                    if (ia->id() == 0x0000) // current hue
                    {
                        uint8_t hue = ia->numericValue().u8;

                        if (hue > 254) // FIXME: Hue lights accept and report Hue of 255 ?
                        {
                            hue = 254;
                        }

                        qreal normHue = (static_cast<double>(hue) * 360 / 254) / 360; // FIXME: Hue lights enhanced hue = 256 * hue.
                        if      (normHue < 0) { normHue = 0; }
                        else if (normHue > 1) { normHue = 1; }

                        const quint16 ehue = static_cast<quint16>(normHue * 65535);

                        lightNode->setValue(RStateHue, ehue);
                    }
                    else if (ia->id() == 0x4000 && lightNode->manufacturerCode() != VENDOR_MUELLER) // enhanced current hue
                    {
                        quint16 hue = ia->numericValue().u16;
                        lightNode->setValue(RStateHue, hue);
                    }
                    else if (ia->id() == 0x0001) // current saturation
                    {
                        uint8_t sat = ia->numericValue().u8;
                        lightNode->setValue(RStateSat, sat);
                    }
                    else if (ia->id() == 0x0003) // current x
                    {
                        uint16_t colorX = ia->numericValue().u16;

                        // sanity for colorX
                        if (colorX > 0xFEFF)
                        {
                            colorX = 0xFEFF;
                        }
                        lightNode->setValue(RStateX, colorX);
                    }
                    else if (ia->id() == 0x0004) // current y
                    {
                        uint16_t colorY = ia->numericValue().u16;
                        // sanity for colorY
                        if (colorY > 0xFEFF)
                        {
                            colorY = 0xFEFF;
                        }
                        lightNode->setValue(RStateY, colorY);
                    }
                    else if (ia->id() == 0x0007) // color temperature
                    {
                        uint16_t ct = ia->numericValue().u16;
                        lightNode->setValue(RStateCt, ct);
                    }
                    else if (ia->id() == 0x0008 || ia->id() == 0x4001) // color mode | enhanced color mode
                    {
                        if (ia->id() == 0x0008 && lightNode->manufacturerCode() == VENDOR_IKEA)
                        {
                            //DBG_Printf(DBG_INFO, "Ignore color mode (0x0008) in favor for enhanced color mode (0x4001) for Ikea light 0x%016llx\n", lightNode->address().ext());
                            continue;
                        }
                        if (ia->id() == 0x4001 && lightNode->manufacturer() == QLatin1String("GLEDOPTO"))
                        {
                            //DBG_Printf(DBG_INFO, "Ignore enhanced color mode (0x4001) in favor for color mode (0x0008) for GLEDOPTO light 0x%016llx\n", lightNode->address().ext());
                            continue;
                        }

                        uint8_t cm = ia->numericValue().u8;

                        if (lightNode->item(RStateHue) == nullptr && lightNode->item(RStateCt) != nullptr)
                        {
                            // OSRAM/LEDVANCE tunable white lights sometimes report hue and saturation, but only ct makes sense
                            cm = 2;
                        }

                        {
                            ResourceItem *item = lightNode->item(RConfigColorCapabilities);
                            if (item && item->toNumber() > 0)
                            {
                                const auto cap = item->toNumber();
                                if (cap == 0x0010 && cm != 2) // color temperature only light
                                {
                                    cm = 2; // fix unsupported color modes (IKEA ct light)
                                }
                            }
                        }

                        const char *modes[4] = {"hs", "xy", "ct", "hs"};

                        if (cm < 4)
                        {
                            lightNode->setValue(RStateColorMode, QString(modes[cm]));
                        }
                    }
                    else if (ia->id() == 0x4002) // color loop active
                    {
                        if (RStateEffectValuesMueller.indexOf(lightNode->toString(RStateEffect), 0) <= 1)
                        {
                            lightNode->setValue(RStateEffect, RStateEffectValues[ia->numericValue().u8]);
                        }
                    }
                    else if (ia->id() == 0x4004) // color loop time
                    {
                        uint8_t clTime = ia->numericValue().u8;

                        if (lightNode->colorLoopSpeed() != clTime)
                        {
                            lightNode->setColorLoopSpeed(clTime);
                        }
                    }
                    else if (ia->id() == 0x400a) // color capabilities
                    {
                        quint16 cap = ia->numericValue().u16;
                        lightNode->setValue(RConfigColorCapabilities, cap);
                    }
                    else if (ia->id() == 0x400b) // color temperature min
                    {
                        quint16 cap = ia->numericValue().u16;
                        lightNode->setValue(RConfigCtMin, cap);
                    }
                    else if (ia->id() == 0x400c) // color temperature max
                    {
                        quint16 cap = ia->numericValue().u16;
                        lightNode->setValue(RConfigCtMax, cap);
                    }
                }
            }
            else if (ic->id() == LEVEL_CLUSTER_ID && (event.clusterId() == LEVEL_CLUSTER_ID))
            {
                if (isXmasLightStrip(lightNode))
                {
                    continue;
                }
                std::vector<deCONZ::ZclAttribute>::const_iterator ia = ic->attributes().begin();
                std::vector<deCONZ::ZclAttribute>::const_iterator enda = ic->attributes().end();
                for (;ia != enda; ++ia)
                {
                    if (ia->id() == 0x0000) // current level
                    {
                        lightNode->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());

                        uint8_t level = ia->numericValue().u8;
                        if (lightNode->setValue(RStateBri, level))
                        {
                            lightNode->clearRead(READ_LEVEL);
                            pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().u8);
                        }
                        break;
                    }
                }
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
                        lightNode->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());

                        bool on = ia->numericValue().u8;
                        if (lightNode->setValue(RStateOn, on))
                        {
                            pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().u8);
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
                        break;
                    }
                }
            }
            // Danalock support. In updateLightNode(), whitelist the same cluster and add a handler for ic->id() == DOOR_LOCK_CLUSTER_ID, similar to ONOFF_CLUSTER_ID, but obviously checking for attribute 0x0101/0x0000.
            else if (ic->id() == DOOR_LOCK_CLUSTER_ID && (event.clusterId() == DOOR_LOCK_CLUSTER_ID))
            {
                std::vector<deCONZ::ZclAttribute>::const_iterator ia = ic->attributes().begin();
                std::vector<deCONZ::ZclAttribute>::const_iterator enda = ic->attributes().end();
                for (;ia != enda; ++ia)
                {
                    if (ia->id() == 0x0000) // Lock state
                    {
                        bool on = ia->numericValue().u8 == 1;
                        DBG_Printf(DBG_INFO, "Status dørlås: %u\n", (uint)ia->numericValue().u8);
                        ResourceItem *item = lightNode->item(RStateOn);
                        if (item && item->toBool() != on)
                        {
                            DBG_Printf(DBG_INFO, "0x%016llX onOff %u --> %u\n", lightNode->address().ext(), (uint)item->toNumber(), on);
                            item->setValue(on);
                            Event e(RLights, RStateOn, lightNode->id(), item);
                            enqueueEvent(e);
                            updated = true;
                            pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().u8);
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
                        lightNode->setZclValue(updateType, event.endpoint(), event.clusterId(), 0x0000, ia->numericValue());
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

                    if (ia->id() == 0x0001 && lightNode->modelId() == QLatin1String("TS011F")) // Application version
                    {
                        // For Lidl plugs (TS011F) date code is empty, use this attribute instead.
                        // _TZ3000_1obwwnmq    3x plug
                        // _TZ3000_kdi2o9m6    1x plug
                        const auto str = QString::number(static_cast<int>(ia->numericValue().u8));
                        ResourceItem *item = lightNode->item(RAttrSwVersion);

                        if (!str.isEmpty() && item)
                        {
                            if (item->toString() != str)
                            {
                                lightNode->setNeedSaveDatabase(true);
                                queSaveDb(DB_LIGHTS, DB_LONG_SAVE_DELAY);
                                updated = true;
                            }
                            item->setValue(str); // always needed to refresh set timestamp
                        }
                    }
                    else if (ia->id() == 0x0004) // Manufacturer name
                    {
                        QString str = ia->toString().trimmed();
                        const lidlDevice *device = getLidlDevice(str);

                        if (device != nullptr)
                        {
                            ResourceItem *item = lightNode->item(RAttrModelId);
                            QString str2 = QLatin1String(device->modelid);

                            if (item && !str2.isEmpty() && str2 != item->toString())
                            {
                                lightNode->setModelId(str2);
                                item->setValue(str2);
                                lightNode->setNeedSaveDatabase(true);
                                queSaveDb(DB_LIGHTS, DB_LONG_SAVE_DELAY);
                                updated = true;
                                setLightNodeStaticCapabilities(lightNode);
                            }
                            str = QLatin1String(device->manufacturername);
                        }
                        if (str == QString("欧瑞博"))
                        {
                            str = QLatin1String("ORVIBO");
                        }

                        if (!str.isEmpty() && str != lightNode->manufacturer())
                        {
                            lightNode->setManufacturerName(str);
                            lightNode->setNeedSaveDatabase(true);
                            queSaveDb(DB_LIGHTS, DB_LONG_SAVE_DELAY);
                            updated = true;
                            setLightNodeStaticCapabilities(lightNode);
                        }
                    }
                    else if (ia->id() == 0x0005) // Model identifier
                    {
                        QString str = ia->toString().trimmed();
                        ResourceItem *item = lightNode->item(RAttrModelId);

                        if (isLidlDevice(str, lightNode->manufacturer()))
                        {
                            // Ignore non-unique ModelIdentifier; modelid set from unqiue ManufacturerName.
                            continue;
                        }
                        if (str == QLatin1String("abb71ca5fe1846f185cfbda554046cce"))
                        {
                            str = QLatin1String("T10D1ZW dimmer");
                        }
                        else if (str == QLatin1String("545df2981b704114945f6df1c780515a"))
                        {
                            str = QLatin1String("T10W1ZW switch");
                        }
                        else if (str == QLatin1String("82c167c95ed746cdbd21d6817f72c593"))
                        {
                            str = QLatin1String("CM10ZW");
                        }

                        if (item && !str.isEmpty() && str != item->toString())
                        {
                            lightNode->setModelId(str);
                            item->setValue(str);
                            lightNode->setNeedSaveDatabase(true);
                            queSaveDb(DB_LIGHTS, DB_LONG_SAVE_DELAY);
                            updated = true;
                            setLightNodeStaticCapabilities(lightNode);
                        }
                    }
                    else if (ia->id() == 0x0006) // Date code
                    {
                        const QString str = ia->toString().trimmed();
                        ResourceItem *item = lightNode->item(RAttrSwVersion);

                        if (item && !str.isEmpty())
                        {
                            if (str != item->toString())
                            {
                                lightNode->setNeedSaveDatabase(true);
                                queSaveDb(DB_LIGHTS, DB_LONG_SAVE_DELAY);
                                updated = true;
                            }
                            item->setValue(str); // always needed to refresh set timestamp
                        }
                    }
                    else if (ia->id() == 0x4000) // Software build identifier
                    {
                        const QString str = ia->toString().trimmed();
                        ResourceItem *item = lightNode->item(RAttrSwVersion);

                        deCONZ::NumericUnion dummy;
                        lightNode->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), dummy);

                        if (item && !str.isEmpty())
                        {
                            if (str != item->toString())
                            {
                                lightNode->setNeedSaveDatabase(true);
                                queSaveDb(DB_LIGHTS, DB_LONG_SAVE_DELAY);
                                updated = true;
                            }
                            item->setValue(str); // always needed to refresh set timestamp
                        }
                    }
                    else if (ia->id() == 0x4005 && lightNode->manufacturerCode() == VENDOR_MUELLER)
                    {
                        lightNode->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());

                        quint8 scene = ia->numericValue().u8;
                        if (scene >= 1 && scene <= 6)
                        {
                            lightNode->setValue(RStateEffect, RStateEffectValuesMueller[scene + 1]);
                        }
                    }
                }
            }
            else if (ic->id() == ANALOG_OUTPUT_CLUSTER_ID && (event.clusterId() == ANALOG_OUTPUT_CLUSTER_ID))
            {
                if (!(lightNode->modelId().startsWith(QLatin1String("lumi.curtain"))))
                {
                    continue; // ignore except for lumi.curtain
                }

                std::vector<deCONZ::ZclAttribute>::const_iterator ia = ic->attributes().begin();
                std::vector<deCONZ::ZclAttribute>::const_iterator enda = ic->attributes().end();
                for (;ia != enda; ++ia)
                {
                    if (ia->id() == 0x0055) // Present Value
                    {
                        lightNode->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());

                        quint8 lift = 100 - ia->numericValue().real;
                        bool open = lift < 100;
                        if (lightNode->setValue(RStateLift, lift))
                        {
                            pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().u8);
                        }
                        lightNode->setValue(RStateOpen, open);

                        // FIXME: deprecate
                        quint8 level = 254 * lift / 100;
                        bool on = level > 0;
                        lightNode->setValue(RStateBri, level);
                        lightNode->setValue(RStateOn, on);
                        // END FIXME: deprecate

                        break;
                    }
                }
            }
            // This code can potentially be removed here and covered by fan_control.cpp
            else if (ic->id() == FAN_CONTROL_CLUSTER_ID && (event.clusterId() == FAN_CONTROL_CLUSTER_ID))
            {
                std::vector<deCONZ::ZclAttribute>::const_iterator ia = ic->attributes().begin();
                std::vector<deCONZ::ZclAttribute>::const_iterator enda = ic->attributes().end();
                for (;ia != enda; ++ia)
                {
                    if (ia->id() == 0x0000) // Fan Mode
                    {
                        lightNode->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());

                        uint8_t mode = ia->numericValue().u8;
                        lightNode->setValue(RStateSpeed, mode);
                    }
                }
            }
        }

        break;
    }

    if (updated)
    {
        updateLightEtag(lightNode);
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
            if (i->state() != LightNode::StateNormal)
            {
                continue;
            }

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
            if (i->state() != LightNode::StateNormal)
            {
                continue;
            }

            if (i->address().nwk() == addr.nwk())
            {
                if ((endpoint == 0) || (endpoint == i->haEndpoint().endpoint()))
                {
                    return &(*i);
                }
            }
        }
    }

    return nullptr;
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
            if (i->id() == id && i->state() == LightNode::StateNormal)
            {
                return &*i;
            }
        }
    }
    else
    {
        for (i = nodes.begin(); i != end; ++i)
        {
            if (i->uniqueId() == id && i->state() == LightNode::StateNormal)
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
        if (sensor->type() == QLatin1String("ZHATime") && !sensor->mustRead(READ_TIME))
        {
            std::vector<quint16>::const_iterator ci = sensor->fingerPrint().inClusters.begin();
            std::vector<quint16>::const_iterator cend = sensor->fingerPrint().inClusters.end();
            for (;ci != cend; ++ci)
            {
                if (*ci == TIME_CLUSTER_ID)
                {
                    NodeValue val = sensor->getZclValue(*ci, 0x0000); // Time
                    if (!val.timestamp.isValid() || val.timestamp.secsTo(now) >= 6 * 3600)
                    {
                        DBG_Printf(DBG_INFO, "  >>> %s sensor %s: set READ_TIME from checkSensorNodeReachable()\n", qPrintable(sensor->type()), qPrintable(sensor->name()));
                        sensor->setNextReadTime(READ_TIME, queryTime);
                        sensor->setLastRead(READ_TIME, idleTotalCounter);
                        sensor->enableRead(READ_TIME);
                        queryTime = queryTime.addSecs(1);
                    }
                }
            }
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
    DBG_Assert(sensor != nullptr);

    if (!sensor)
    {
        return;
    }

    if (ind.clusterId() == OTAU_CLUSTER_ID)
    {
        return;
    }

    bool checkReporting = false;
    bool checkClientCluster = false;
    const std::vector<Sensor::ButtonMap> buttonMapVec = sensor->buttonMap(buttonMapData, buttonMapForModelId);
    QString cluster = "0x" + QString("%1").arg(ind.clusterId(), 4, 16, QLatin1Char('0')).toUpper();
    QString cmd = "0x" + QString("%1").arg(zclFrame.commandId(), 2, 16, QLatin1Char('0')).toUpper();
    QString addressMode;
    QString zclPayload = zclFrame.payload().isEmpty() ? "None" : qPrintable(zclFrame.payload().toHex().toUpper());
    quint8 pl0 = zclFrame.payload().isEmpty() ? 0 : zclFrame.payload().at(0);

    if (ind.dstAddress().isNwkUnicast()) { addressMode = ", unicast to: 0x" + QString("%1").arg(ind.dstAddress().nwk(), 4, 16, QLatin1Char('0')).toUpper(); }
    else if (ind.dstAddressMode() == deCONZ::ApsGroupAddress) { addressMode = ", broadcast to: 0x" + QString("%1").arg(ind.dstAddress().group(), 4, 16, QLatin1Char('0')).toUpper(); }
    else { addressMode = ", unknown"; }

    if (!btnMapClusters.key(ind.clusterId()).isEmpty())
    {
        QString val = btnMapClusters.key(ind.clusterId());
        QMap<QString, quint16> temp = btnMapClusterCommands.value(val);
        cluster = val + " (" + cluster + ")";

        if (!temp.empty() && !temp.key(zclFrame.commandId()).isEmpty()) { cmd = temp.key(zclFrame.commandId()) + " (" + cmd + ")"; }
    }

    if (buttonMapVec.empty())
    {
        DBG_Printf(DBG_INFO, "[INFO] - No button map for: %s%s, endpoint: 0x%02X, cluster: %s, command: %s, payload: %s, zclSeq: %u\n",
            qPrintable(sensor->modelId()), qPrintable(addressMode), ind.srcEndpoint(), qPrintable(cluster), qPrintable(cmd), qPrintable(zclPayload), zclFrame.sequenceNumber());
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

        if (sensor->fingerPrint().profileId == HA_PROFILE_ID) // new ZB3 firmware
        {
            if (ind.dstAddressMode() == deCONZ::ApsGroupAddress && ind.dstAddress().group() == 0)
            {
                checkClientCluster = true;
                ResourceItem *item = sensor->item(RConfigGroup);
                if (!item || (item && (item->toString() == QLatin1String("0") || item->toString().isEmpty())))
                {
                    // still default group, create unique group and binding
                    checkSensorGroup(sensor);
                }
            }
        }
    }
    else if (sensor->modelId() == QLatin1String("TRADFRI wireless dimmer"))
    {
        if (sensor->mode() != Sensor::ModeDimmer)
        {
            sensor->setMode(Sensor::ModeDimmer);
        }

        if (sensor->fingerPrint().profileId == HA_PROFILE_ID) // new ZB3 firmware
        {
            checkReporting = true;
            if (ind.dstAddressMode() == deCONZ::ApsGroupAddress && ind.dstAddress().group() == 0)
            {
                checkClientCluster = true;
                ResourceItem *item = sensor->item(RConfigGroup);
                if (!item || (item && (item->toString() == QLatin1String("0") || item->toString().isEmpty())))
                {
                    // still default group, create unique group and binding
                    checkSensorGroup(sensor);
                }
            }
        }
    }
    else if (sensor->modelId().startsWith(QLatin1String("TRADFRI on/off switch")) ||
             sensor->modelId().startsWith(QLatin1String("TRADFRI SHORTCUT Button")) ||
             sensor->modelId().startsWith(QLatin1String("TRADFRI open/close remote")) ||
             sensor->modelId().startsWith(QLatin1String("TRADFRI motion sensor")))
    {
        if (ind.dstAddressMode() == deCONZ::ApsGroupAddress && ind.dstAddress().group() == 0)
        {
            checkClientCluster = true;
            ResourceItem *item = sensor->item(RConfigGroup);
            if (!item || (item && (item->toString() == QLatin1String("0") || item->toString().isEmpty())))
            {
                // still default group, create unique group and binding
                checkSensorGroup(sensor);
            }
        }
    }
    else if (sensor->modelId().startsWith(QLatin1String("SYMFONISK")))
    {
        if (zclFrame.sequenceNumber() == sensor->previousSequenceNumber)
        {
            return;
        }
        sensor->previousSequenceNumber = zclFrame.sequenceNumber();
        checkReporting = true;
    }
    else if (sensor->modelId() == QLatin1String("Remote switch") || //legrand switch
             sensor->modelId() == QLatin1String("Double gangs remote switch") || //Legrand micro module
             sensor->modelId() == QLatin1String("Shutters central remote switch") || // legrand shutter switch
             sensor->modelId() == QLatin1String("Remote motion sensor") || // legrand motion sensor
             sensor->modelId() == QLatin1String("Remote toggle switch")) // legrand switch simple and double
    {
        checkReporting = true;
        checkClientCluster = true;
    }
    else if (sensor->modelId().startsWith(QLatin1String("RWL02")) || // Hue dimmer switch
             sensor->modelId().startsWith(QLatin1String("ROM00")) || // Hue smart button
             sensor->modelId().startsWith(QLatin1String("RDM00")) || // Hue wall switch module
             sensor->modelId().startsWith(QLatin1String("Z3-1BRL"))) // Lutron Aurora Friends-of-Hue dimmer switch
    {
        checkReporting = true;
    }
    else if (sensor->modelId().startsWith(QLatin1String("Lightify Switch Mini")) ||  // Osram 3 button remote
             sensor->modelId().startsWith(QLatin1String("Switch 4x EU-LIGHTIFY")) || // Osram 4 button remote
             sensor->modelId().startsWith(QLatin1String("Switch 4x-LIGHTIFY")) || // Osram 4 button remote
             sensor->modelId().startsWith(QLatin1String("Switch-LIGHTIFY"))) // Osram 4 button remote
    {
        checkReporting = true;
        checkClientCluster = true;
    }
    else if (sensor->modelId().startsWith(QLatin1String("ZBT-CCTSwitch-D0001"))) // LDS remote
    {
        checkReporting = true;
        checkClientCluster = true;
    }
    else if (sensor->modelId().startsWith(QLatin1String("ICZB-RM")) || // icasa remote
             sensor->modelId().startsWith(QLatin1String("ZGR904-S")) || // Envilar remote
             sensor->modelId().startsWith(QLatin1String("RGBGenie ZB-5")) || // RGBGenie remote control
             sensor->modelId().startsWith(QLatin1String("ZGRC-KEY")))        // RGBGenie ZB-5001
    {
        checkReporting = true;
        checkClientCluster = true;
    }
    else if (sensor->modelId().startsWith(QLatin1String("TS0215")) || // Tuya remote
             sensor->modelId().startsWith(QLatin1String("RC 110")) || // innr remote
             sensor->modelId().startsWith(QLatin1String("RC_V14")) || // Heiman remote
             sensor->modelId().startsWith(QLatin1String("RC-EM")))   // Heiman remote
    {
        checkClientCluster = true;
    }
    else if (ind.dstAddressMode() == deCONZ::ApsGroupAddress)
    {
        if (sensor->mode() == Sensor::ModeTwoGroups) // only supported for DE Lighting Switch
        {
            sensor->setMode(Sensor::ModeScenes);
            updateSensorEtag(sensor);
        }
    }
    else if (sensor->modelId().endsWith(QLatin1String("86opcn01"))) // Aqara Opple
    {
        checkReporting = true;
    }
    else if (sensor->modelId() == QLatin1String("Bell")) //Sage doorbell sensor
    {
        if (ind.dstAddressMode() == deCONZ::ApsGroupAddress && ind.dstAddress().group() == 0)
        {
            checkClientCluster = true;
            checkReporting = true;
            ResourceItem *item = sensor->item(RConfigGroup);
            if (!item || (item && (item->toString() == QLatin1String("0") || item->toString().isEmpty())))
            {
                checkSensorGroup(sensor); // still default group, create unique group and binding
            }
        }
    }
    else if (sensor->modelId() == QLatin1String("HG06323")) // LIDL Remote Control
    {
        // Probably needed because deCONZ doesn't send Default Response to unicast command.
        if (zclFrame.sequenceNumber() == sensor->previousSequenceNumber)
        {
            return;
        }
        sensor->previousSequenceNumber = zclFrame.sequenceNumber();
        checkReporting = true;
    }

    if (ind.dstAddressMode() == deCONZ::ApsGroupAddress && ind.dstAddress().group() != 0)
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
        if (sensor->modelId().startsWith(QLatin1String("RC 110"))) // innr remote
        {
            // 7 controller endpoints: 0x01, 0x03, 0x04, ..., 0x08
            if (gids.length() != 7)
            {
                // initialise list of groups: one for each endpoint
                gids = QStringList();
                gids << "0" << "0" << "0" << "0" << "0" << "0" << "0";
            }

            // check group corresponding to source endpoint
            int i = ind.srcEndpoint();
            i -= i == 1 ? 1 : 2;
            if (gids.value(i) != gid)
            {
                // replace group corresponding to source endpoint
                gids.replace(i, gid);
                item->setValue(gids.join(','));
                sensor->setNeedSaveDatabase(true);
                updateSensorEtag(sensor);
                enqueueEvent(Event(RSensors, RConfigGroup, sensor->id(), item));
            }

            Event e(RSensors, REventValidGroup, sensor->id());
            enqueueEvent(e);
        }
        else if (sensor->modelId().startsWith(QLatin1String("ICZB-RM")) ||          // icasa remote
                 sensor->modelId().startsWith(QLatin1String("ZGR904-S")) ||         // Envilar remote
                 sensor->modelId().startsWith(QLatin1String("ZGRC-KEY")) ||         // Sunricher remote
                 sensor->modelId().startsWith(QLatin1String("ED-1001")) ||          // EcoDim switches
                 sensor->modelId().startsWith(QLatin1String("45127")) ||            // Namron switches
                 sensor->modelId().startsWith(QLatin1String("RGBgenie ZB-5001")) || // RGBGenie remote
                 sensor->modelId().startsWith(QLatin1String("ZGRC-TEUR-")))         // iluminize wall switch 511.524
        {
            if (gids.length() != 5 && sensor->modelId().startsWith(QLatin1String("ZGRC-KEY-012"))) // 5 controller endpoints: 0x01, 0x02, 0x03, 0x04, 0x05
            {
                // initialise list of groups: one for each endpoint
                gids = QStringList();
                gids << "0" << "0" << "0" << "0" << "0";
            }
            else if (gids.length() != 4) // 4 controller endpoints: 0x01, 0x02, 0x03, 0x04
            {
                // initialise list of groups: one for each endpoint
                gids = QStringList();
                gids << "0" << "0" << "0" << "0";
            }

            // check group corresponding to source endpoint
            int i = ind.srcEndpoint();
            i -= 1;
            if (gids.value(i) != gid)
            {
                // replace group corresponding to source endpoint
                gids.replace(i, gid);
                item->setValue(gids.join(','));
                sensor->setNeedSaveDatabase(true);
                updateSensorEtag(sensor);
                enqueueEvent(Event(RSensors, RConfigGroup, sensor->id(), item));
            }

            Event e(RSensors, REventValidGroup, sensor->id());
            enqueueEvent(e);
        }
        else if (sensor->modelId().startsWith(QLatin1String("ZBT-Remote-ALL-RGBW")))
        {
            bool changed = true;
            if (gids.length() != 3)
            {
                gids = QStringList();
                gids << "0" << "0" << "0";
            }

            if (gids.contains(gid))
            {
                changed = false;
            }
            else if (gids.value(1) == "0" && gids.value(0) == QString::number(groupId - 2))
            {
                gids.replace(1, QString::number(groupId - 1));
                gids.replace(2, gid);
            }
            else if (gids.value(1) == "0" && gids.value(0) == QString::number(groupId - 1))
            {
                gids.replace(1, gid);
            }
            else if (gids.value(1) == "0" && gids.value(0) == QString::number(groupId + 2))
            {
                gids.replace(0, gid);
                gids.replace(1, QString::number(groupId + 1));
                gids.replace(2, QString::number(groupId + 2));
            }
            else if (gids.value(1) == "0" && gids.value(0) == QString::number(groupId + 1))
            {
                gids.replace(0, gid);
                gids.replace(1, QString::number(groupId + 1));
            }
            else if (gids.value(2) == "0" && gids.value(1) == QString::number(groupId - 1))
            {
                gids.replace(2, gid);
            }
            else if (gids.value(2) == "0" && gids.value(0) == QString::number(groupId + 1))
            {
                gids.replace(0, gid);
                gids.replace(1, QString::number(groupId + 1));
                gids.replace(2, QString::number(groupId + 1));
            }
            else
            {
                gids.replace(0, gid);
                gids.replace(1, "0");
                gids.replace(2, "0");
            }

            if (changed)
            {
                item->setValue(gids.join(','));
                sensor->setNeedSaveDatabase(true);
                updateSensorEtag(sensor);
                enqueueEvent(Event(RSensors, RConfigGroup, sensor->id(), item));
            }

            Event e(RSensors, REventValidGroup, sensor->id());
            enqueueEvent(e);
        }
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
    for (const auto &buttonMap : buttonMapVec)
    {
        if (buttonMap.mode != Sensor::ModeNone && !ok)
        {
            if (buttonMap.mode == sensor->mode() &&
                buttonMap.endpoint == ind.srcEndpoint() &&
                buttonMap.clusterId == ind.clusterId() &&
                buttonMap.zclCommandId == zclFrame.commandId())
            {
                ok = true;

                //Tuya
                if (ind.clusterId() == ONOFF_CLUSTER_ID && zclFrame.commandId() == 0xFD)
                {
                    ok = false;
                    if (zclFrame.payload().size() >= 1)
                    {
                        quint8 level = zclFrame.payload().at(0);
                        ok = buttonMap.zclParam0 == level;
                    }
                }
                else if (zclFrame.isProfileWideCommand() &&
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
                        quint8 value;
                        stream >> value;
                        ok = false;
                        // payload: u16 attrId, u8 datatype, u8 data
                        if (attrId == 0x0000 && dataType == deCONZ::ZclBoolean && // onoff attribute
                            buttonMap.zclParam0 == value)
                        {
                            ok = true;
                        }
                        else if (attrId == 0x8000 && dataType == deCONZ::Zcl8BitUint && // custom attribute for multi press
                            buttonMap.zclParam0 == value)
                        {
                            ok = true;
                        }

                        // the round button (lumi.sensor_switch) sends a release command regardless if it is a short press or a long release
                        // figure it out here to decide if it is a short release (1002) or a long release (1003)
                        if (ok && sensor->modelId() == QLatin1String("lumi.sensor_switch"))
                        {
                            const QDateTime now = QDateTime::currentDateTime();

                            if (buttonMap.button == (S_BUTTON_1 + S_BUTTON_ACTION_INITIAL_PRESS))
                            {
                                sensor->durationDue = now.addMSecs(500); // enable generation of 1001 (hold)
                                checkSensorsTimer->start(CHECK_SENSOR_FAST_INTERVAL);
                            }
                            else if (buttonMap.button == (S_BUTTON_1 + S_BUTTON_ACTION_SHORT_RELEASED))
                            {
                                sensor->durationDue = QDateTime(); // disable generation of 1001 (hold)

                                ResourceItem *item = sensor->item(RStateButtonEvent);
                                if (item && (item->toNumber() == (S_BUTTON_1 + S_BUTTON_ACTION_INITIAL_PRESS) ||
                                             item->toNumber() == (S_BUTTON_1 + S_BUTTON_ACTION_HOLD)))
                                {
                                    if (item->toNumber() == (S_BUTTON_1 + S_BUTTON_ACTION_HOLD) || // hold already triggered -> long release
                                        item->lastSet().msecsTo(now) > 400) // over 400 ms since initial press? -> long release
                                    {
                                        ok = false; // force long release button event
                                    }
                                }
                            }
                        }
                    }
                    else if ((ind.clusterId() == DOOR_LOCK_CLUSTER_ID && sensor->manufacturer() == QLatin1String("LUMI")) ||
                             ind.clusterId() == MULTISTATE_INPUT_CLUSTER_ID)
                    {
                        ok = false;
                        if (attrId == MULTI_STATE_INPUT_PRESENT_VALUE_ATTRIBUTE_ID &&
                            dataType == deCONZ::Zcl16BitUint)
                        {
                            quint16 value;
                            stream >> value;
                            ok = buttonMap.zclParam0 == value;
                        }
                    }
                }
                else if (zclFrame.isProfileWideCommand() &&
                         zclFrame.commandId() == deCONZ::ZclWriteAttributesId &&
                         zclFrame.payload().size() >= 4)
                {
                    QDataStream stream(zclFrame.payload());
                    stream.setByteOrder(QDataStream::LittleEndian);
                    quint16 attrId;
                    quint8 dataType;
                    quint8 pl3;
                    stream >> attrId;
                    stream >> dataType;
                    stream >> pl3;

                    if (ind.clusterId() == BASIC_CLUSTER_ID && sensor->modelId().startsWith(QLatin1String("ZBT-Remote-ALL-RGBW")))
                    {
                        ok = attrId == 0x4005 && dataType == deCONZ::Zcl8BitUint && buttonMap.zclParam0 == pl3;
                    }
                }
                else if (zclFrame.isProfileWideCommand())
                {
                }
                else if (ind.clusterId() == SCENE_CLUSTER_ID && zclFrame.commandId() == 0x05) // recall scene
                {
                    ok = false; // payload: groupId, sceneId
                    if (zclFrame.payload().size() >= 3 && buttonMap.zclParam0 == zclFrame.payload().at(2))
                    {
                        ok = true;
                    }
                }
                else if (ind.clusterId() == SCENE_CLUSTER_ID && zclFrame.commandId() == 0x04) // store scene
                {
                    ok = false; // payload: groupId, sceneId
                    if (zclFrame.payload().size() >= 3 && buttonMap.zclParam0 == zclFrame.payload().at(2))
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
                        if (zclFrame.payload().size() >= 1 && buttonMap.zclParam0 == zclFrame.payload().at(0)) // next, prev scene
                        {
                            sensor->previousDirection = buttonMap.zclParam0;
                            ok = true;
                        }
                    }
                    else if (zclFrame.commandId() == 0x09) // long release
                    {
                        if (buttonMap.zclParam0 == sensor->previousDirection)
                        {
                            sensor->previousDirection = 0xFF;
                            ok = true;
                        }
                    }
                }
                else if (ind.clusterId() == LEVEL_CLUSTER_ID && zclFrame.commandId() == 0x04 && // move to level (with on/off)
                         sensor->modelId().startsWith(QLatin1String("Z3-1BRL"))) // Lutron Aurora Friends-of-Hue dimmer
                {
                    // This code is for handling the button map for the Aurora, until we figure out how to activate the 0xFC00 cluster.
                    ok = false;
                    if (zclFrame.payload().size() >= 2)
                    {
                        quint8 level = zclFrame.payload().at(0);
                        quint8 tt = zclFrame.payload().at(1);
                        if (tt == 7) // button pressed
                        {
                            ok = buttonMap.zclParam0 == 0; // Toggle
                        }
                        else if (tt == 2) // dial turned
                        {
                            if      (sensor->previousDirection < level) ok = buttonMap.zclParam0 == 1; // DimUp
                            else if (sensor->previousDirection > level) ok = buttonMap.zclParam0 == 2; // DimDown
                            else if (level == 0xFF)                     ok = buttonMap.zclParam0 == 1; // DimUp
                            else if (level == 2)                        ok = buttonMap.zclParam0 == 2; // DimDown
                        }
                        if (ok)
                        {
                            sensor->previousDirection = level;
                        }
                    }
                }
                else if (ind.clusterId() == LEVEL_CLUSTER_ID && zclFrame.commandId() == 0x04) // move to level (with on/off)
                {
                    ok = false;
                    if (zclFrame.payload().size() >= 1)
                    {
                        quint8 level = zclFrame.payload().at(0);
                        ok = buttonMap.zclParam0 == level;

                    }
                }
                else if (ind.clusterId() == LEVEL_CLUSTER_ID &&
                         (zclFrame.commandId() == 0x01 ||  // move
                          zclFrame.commandId() == 0x02 ||  // step
                          zclFrame.commandId() == 0x05 ||  // move (with on/off)
                          zclFrame.commandId() == 0x06))   // step (with on/off)
                {
                    ok = false;
                    if (zclFrame.payload().size() >= 1 && buttonMap.zclParam0 == zclFrame.payload().at(0)) // direction
                    {
                        sensor->previousDirection = zclFrame.payload().at(0);
                        ok = true;
                    }
                }
                else if (ind.clusterId() == LEVEL_CLUSTER_ID &&
                           (zclFrame.commandId() == 0x03 ||  // stop
                            zclFrame.commandId() == 0x07))  // stop (with on/off)
                {
                    ok = false;
                    if (buttonMap.zclParam0 == sensor->previousDirection) // direction of previous move/step
                    {
                        sensor->previousDirection = 0xFF;
                        ok = true;
                    }
                    if (buttonMap.zclParam0 != sensor->previousDirection && // direction of previous move/step
                        (sensor->modelId().startsWith(QLatin1String("RGBgenie ZB-5121")) || // Device sends cmd = 7 + param = 0 for dim up/down
                        sensor->modelId().startsWith(QLatin1String("ZBT-DIMSwitch-D0001")) ||
                        sensor->modelId().startsWith(QLatin1String("ZGRC-TEUR-003"))))
                    {
                        sensor->previousDirection = 0xFF;
                        ok = true;
                    }
                }
                else if (ind.clusterId() == WINDOW_COVERING_CLUSTER_ID)
                {
                    ok = false;
                    if (zclFrame.commandId() == 0x00 || zclFrame.commandId() == 0x01) // Open, Close
                    {
                        sensor->previousDirection = zclFrame.commandId();
                        ok = true;
                    }
                    else if (zclFrame.commandId() == 0x02) // Stop
                    {
                        if (buttonMap.zclParam0 == sensor->previousDirection)
                        {
                            sensor->previousDirection = 0xFF;
                            ok = true;
                        }
                    }
                }
                else if (ind.clusterId() == IAS_ZONE_CLUSTER_ID)
                {
                    ok = false;
                    // following works for Samjin button
                    if (zclFrame.payload().size() == 6 && buttonMap.zclParam0 == zclFrame.payload().at(0))
                    {
                        ok = true;
                    }
                }
                else if (ind.clusterId() == IAS_ACE_CLUSTER_ID)
                {
                    ok = false;
                    if (zclFrame.commandId() == 0x00 && zclFrame.payload().size() == 3 && buttonMap.zclParam0 == zclFrame.payload().at(0))
                    {
                        ok = true;
                    }
                    else if (zclFrame.commandId() == 0x02 && zclFrame.payload().isEmpty())
                    {
                        ok = true;
                    }
                }
                else if (ind.clusterId() == COLOR_CLUSTER_ID &&
                         zclFrame.commandId() == 0x07 && zclFrame.payload().size() >= 4 && // Move to Color
                         sensor->modelId().startsWith(QLatin1String("ZBT-Remote-ALL-RGBW")))
                {
                    quint16 x;
                    quint16 y;
                    quint16 a = 0xFFFF;
                    QDataStream stream(zclFrame.payload());
                    stream.setByteOrder(QDataStream::LittleEndian);
                    stream >> x;
                    stream >> y;

                    switch (x) {
                        case 19727: a =   0; break; // North
                        case 22156: a =  10; break;
                        case 24216: a =  20; break;
                        case 25909: a =  30; break;
                        case 27663: a =  40; break;
                        case 29739: a =  50; break;
                        case 32302: a =  60; break;
                        case 35633: a =  70; break;
                        case 39898: a =  80; break;
                        case 45875: a =  90; break; // East
                        case 42184: a = 100; break;
                        case 39202: a = 110; break;
                        case 36633: a = 120; break;
                        case 34493: a = 130; break;
                        case 32602: a = 140; break;
                        case 30993: a = 150; break;
                        case 29270: a = 160; break;
                        case 27154: a = 170; break;
                        case 24420: a = 180; break; // South
                        case 20648: a = 190; break;
                        case 11111: a = 200; break;
                        case  7208: a = 210; break;
                        case  7356: a = 220; break;
                        case  7451: a = 230; break;
                        case  7517: a = 240; break;
                        case  7563: a = 250; break;
                        case  7599: a = 260; break;
                        case  7627: a = 270; break; // West
                        case  7654: a = 280; break;
                        case  7684: a = 290; break;
                        case  7719: a = 300; break;
                        case  7760: a = 310; break;
                        case  7808: a = 320; break;
                        case  7864: a = 330; break;
                        case 12789: a = 340; break;
                        case 16664: a = 350; break;
                        default: ok = false; break;
                    }

                    if (ok)
                    {
                        ResourceItem *item = sensor->item(RStateX);
                        if (item)
                        {
                            item->setValue(x);
                            enqueueEvent(Event(RSensors, RStateX, sensor->id(), item));
                        }
                        item = sensor->item(RStateY);
                        if (item)
                        {
                            item->setValue(y);
                            enqueueEvent(Event(RSensors, RStateY, sensor->id(), item));
                        }
                        item = sensor->item(RStateAngle);
                        if (item)
                        {
                            item->setValue(a);
                            enqueueEvent(Event(RSensors, RStateAngle, sensor->id(), item));
                        }
                    }
                    else
                    {
                        DBG_Printf(DBG_INFO, "unknown xy values for: %s ep: 0x%02X cl: 0x%04X cmd: 0x%02X xy: (%u, %u)\n",
                                   qPrintable(sensor->modelId()), ind.srcEndpoint(), ind.clusterId(), zclFrame.commandId(), x, y);
                    }
                }
                else if (ind.clusterId() == COLOR_CLUSTER_ID &&
                         zclFrame.commandId() == 0x0a && zclFrame.payload().size() >= 2 && // Move to Color Temperature
                         sensor->modelId().startsWith(QLatin1String("ZBT-Remote-ALL-RGBW")))
                {
                    quint16 ct;
                    QDataStream stream(zclFrame.payload());
                    stream.setByteOrder(QDataStream::LittleEndian);
                    stream >> ct;

                    if      (sensor->previousCt < ct) ok = buttonMap.zclParam0 == 0; // CtUp
                    else if (sensor->previousCt > ct) ok = buttonMap.zclParam0 == 1; // CtDown
                    else if (ct == 370)               ok = buttonMap.zclParam0 == 0; // DimUp
                    else if (ct == 153)               ok = buttonMap.zclParam0 == 1; // DimDown
                    if (ok)
                    {
                        sensor->previousCt = ct;
                    }

                }
                else if (ind.clusterId() == COLOR_CLUSTER_ID &&
                         zclFrame.commandId() == 0x0a && zclFrame.payload().size() >= 2 && // Move to Color Temperature
                         sensor->modelId().startsWith(QLatin1String("ZBT-CCTSwitch-D0001")))
                {
                        if (buttonMap.zclParam0 != pl0)
                        {
                            ok = false;
                            pl0 = zclFrame.payload().isEmpty() ? 0 : zclFrame.payload().at(1);
                        }
                        //ignore the command if previous was button4
                        if (sensor->previousCommandId == 0x04)
                        {
                            ok = false;
                        }
                }
                else if ((ind.clusterId() == COLOR_CLUSTER_ID) &&
                         (zclFrame.commandId() == 0x4b) &&
                         sensor->modelId().startsWith(QLatin1String("ZBT-CCTSwitch-D0001")))
                {
                    quint8 pl0 = zclFrame.payload().isEmpty() ? 0 : zclFrame.payload().at(0);
                    if (buttonMap.zclParam0 != pl0)
                    {
                        ok = false;
                    }
                }
                else if (ind.clusterId() == COLOR_CLUSTER_ID &&
                         (zclFrame.commandId() == 0x4b && zclFrame.payload().size() >= 7))  // move color temperature
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
                    if (buttonMap.zclParam0 == param)
                    {
                        if (moveMode == 0x00)
                        {
                            sensor->previousDirection = 0xFF;
                        }
                        ok = true;
                    }
                }
                else if (ind.clusterId() == COLOR_CLUSTER_ID && (zclFrame.commandId() == 0x01 ))  // Move hue command
                {
                    // Only used by Osram devices currently
                    if (sensor->modelId().startsWith(QLatin1String("Lightify Switch Mini")) ||  // Osram 3 button remote
                        sensor->modelId().startsWith(QLatin1String("Switch 4x EU-LIGHTIFY")) || // Osram 4 button remote
                        sensor->modelId().startsWith(QLatin1String("Switch 4x-LIGHTIFY")) || // Osram 4 button remote
                        sensor->modelId().startsWith(QLatin1String("Switch-LIGHTIFY"))) // Osram 4 button remote
                    {
                        if (buttonMap.zclParam0 != pl0)
                        {
                            ok = false;
                        }
                    }

                }
                else if (ind.clusterId() == SENGLED_CLUSTER_ID)
                {
                    ok = false;

                    if (buttonMap.zclParam0 == pl0)
                    {
                        ok = true;
                    }
                }
                else if (ind.clusterId() == ADUROLIGHT_CLUSTER_ID)
                {
                    ok = false;
                    
                    if (buttonMap.zclParam0 == zclFrame.payload().at(1))
                    {
                        ok = true;
                    }
                }

                if (ok && buttonMap.button != 0)
                {
                    if (!buttonMap.name.isEmpty()) { cmd = buttonMap.name; }

                    DBG_Printf(DBG_INFO, "[INFO] - Button %u - %s%s, endpoint: 0x%02X, cluster: %s, action: %s, payload: %s, zclSeq: %u\n",
                        buttonMap.button, qPrintable(sensor->modelId()), qPrintable(addressMode), ind.srcEndpoint(), qPrintable(cluster), qPrintable(cmd), qPrintable(zclPayload), zclFrame.sequenceNumber());

                    ResourceItem *item = sensor->item(RStateButtonEvent);
                    if (item)
                    {
                        if (item->toNumber() == buttonMap.button && ind.dstAddressMode() == deCONZ::ApsGroupAddress)
                        {
                            QDateTime now = QDateTime::currentDateTime();
                            const auto dt = item->lastSet().msecsTo(now);

                            if (dt > 0 && dt < 500)
                            {
                                DBG_Printf(DBG_INFO, "[INFO] - Button %u %s, discard too fast event (dt = %d) %s\n", buttonMap.button, qPrintable(cmd), static_cast<int>(dt), qPrintable(sensor->modelId()));
                                break;
                            }
                        }

                        item->setValue(buttonMap.button);

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
        }
    }

    //Remember last command id
    if (sensor->modelId().startsWith(QLatin1String("ZBT-CCTSwitch-D0001"))) // LDS remote
    {
        sensor->previousCommandId = zclFrame.commandId();
    }

    if (checkReporting && sensor->node() &&
        sensor->lastAttributeReportBind() < (idleTotalCounter - BUTTON_ATTR_REPORT_BIND_LIMIT))
    {
        if (checkSensorBindingsForAttributeReporting(sensor))
        {
            sensor->setLastAttributeReportBind(idleTotalCounter);
        }
        if (sensor->mustRead(READ_BINDING_TABLE))
        {
            sensor->setNextReadTime(READ_BINDING_TABLE, queryTime);
            queryTime = queryTime.addSecs(1);
        }
        DBG_Printf(DBG_INFO_L2, "Force binding of attribute reporting for sensor %s\n", qPrintable(sensor->name()));
    }

    if (checkClientCluster && sensor->node())
    {
        checkSensorBindingsForClientClusters(sensor);
    }

    if (ok)
    {
        return;
    }

    if (sensor->item(RStateButtonEvent))
    {
        DBG_Printf(DBG_INFO, "[INFO] - No button handler for: %s%s, endpoint: 0x%02X, cluster: %s, command: %s, payload: %s, zclSeq: %u\n",
            qPrintable(sensor->modelId()), qPrintable(addressMode), ind.srcEndpoint(), qPrintable(cluster), qPrintable(cmd), qPrintable(zclPayload), zclFrame.sequenceNumber());
    }
}

/*! Adds a new sensor node to node cache.
    Only supported ZLL and HA nodes will be added.
    \param node - the base for the SensorNode
    \param event - the related NodeEvent (optional)
 */
void DeRestPluginPrivate::addSensorNode(const deCONZ::Node *node, const deCONZ::NodeEvent *event)
{
    DBG_Assert(node);

    if (!node)
    {
        return;
    }

    { // check existing sensors
        std::vector<Sensor>::iterator i = sensors.begin();
        std::vector<Sensor>::iterator end = sensors.end();

        bool pollControlInitialized = false;

        for (; i != end; ++i)
        {
            if (i->address().ext() == node->address().ext())
            {
                if (i->deletedState() != Sensor::StateNormal)
                {
                    continue;
                }

                if (i->node() != node)
                {
                    i->setNode(const_cast<deCONZ::Node*>(node));
                    DBG_Printf(DBG_INFO, "SensorNode %s set node %s\n", qPrintable(i->id()), qPrintable(node->address().toStringExt()));

                    pushSensorInfoToCore(&*i);

                    // If device has Poll Control cluster, configure it via the first Sensor.
                    if (!pollControlInitialized && PC_GetPollControlEndpoint(node) > 0)
                    {
                        auto *itemPending = i->item(RConfigPending);
                        if (itemPending)
                        {
                            DBG_Printf(DBG_INFO, "Init Poll Control for %s\n", qPrintable(node->address().toStringExt()));
                            pollControlInitialized = true;
                            itemPending->setValue(itemPending->toNumber() | R_PENDING_WRITE_POLL_CHECKIN_INTERVAL | R_PENDING_SET_LONG_POLL_INTERVAL);
                        }
                    }
                }

                auto *item = i->item(RStateBattery);
                if (!item)
                {
                    item = i->item(RConfigBattery);
                }

                if (item && item->toNumber() > 0)
                {
                    q_ptr->nodeUpdated(i->address().ext(), QLatin1String(item->descriptor().suffix), QString::number(item->toNumber()));
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

    // Trust and iHorn specific
    if (node->nodeDescriptor().manufacturerCode() == VENDOR_JENNIC && modelId.isEmpty() && i != end)
    {
        int inClusterCount = i->inClusters().size();
        int outClusterCount = i->outClusters().size();

        // check Trust remote control ZYCT-202 or ZLL-NonColorController
        if (node->simpleDescriptors().size() == 2 &&
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
        // check iHorn temperature and humidity sensor 113D
        if (node->simpleDescriptors().size() == 1 &&
            node->simpleDescriptors()[0].endpoint() == 0x01 &&
            node->simpleDescriptors()[0].profileId() == HA_PROFILE_ID &&
            node->simpleDescriptors()[0].deviceId() == 0x0302 &&
            inClusterCount == 8 && outClusterCount == 11)
        {
            modelId = QLatin1String("113D"); //  the modelid returned by device is empty
            manufacturer = QLatin1String("iHorn");
        }
    }

    for (;i != end; ++i)
    {
        SensorFingerprint fpAirQualitySensor;
        SensorFingerprint fpAlarmSensor;
        SensorFingerprint fpBatterySensor;
        SensorFingerprint fpCarbonMonoxideSensor;
        SensorFingerprint fpConsumptionSensor;
        SensorFingerprint fpFireSensor;
        SensorFingerprint fpHumiditySensor;
        SensorFingerprint fpLightSensor;
        SensorFingerprint fpOpenCloseSensor;
        SensorFingerprint fpPowerSensor;
        SensorFingerprint fpPresenceSensor;
        SensorFingerprint fpPressureSensor;
        SensorFingerprint fpSpectralSensor;
        SensorFingerprint fpSwitch;
        SensorFingerprint fpTemperatureSensor;
        SensorFingerprint fpThermostatSensor;
        SensorFingerprint fpTimeSensor;
        SensorFingerprint fpVibrationSensor;
        SensorFingerprint fpWaterSensor;

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
                                // replace ORVIBO model ID
                                if (modelId == QLatin1String("895a2d80097f4ae2b2d40500d5e03dcc"))
                                {
                                    modelId = QLatin1String("SN10ZW motion sensor");
                                }
                                else if (modelId == QLatin1String("b5db59bfd81e4f1f95dc57fdbba17931"))
                                {
                                    modelId = QLatin1String("SF20 smoke sensor");
                                }
                                else if (modelId == QLatin1String("98293058552c49f38ad0748541ee96ba"))
                                {
                                    modelId = QLatin1String("SF21 smoke sensor");
                                }
                                else if (modelId == QLatin1String("898ca74409a740b28d5841661e72268d"))
                                {
                                    modelId = QLatin1String("ST30 Temperature Sensor");
                                }
                                //This device have model ID but not manufacture name
                                if (modelId == QLatin1String("PST03A-v2.2.5"))
                                {
                                    manufacturer = QLatin1String("Philio");
                                }
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
                    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_SINOPE &&
                             modelId.startsWith(QLatin1String("WL4200")))
                    {
                        fpWaterSensor.inClusters.push_back(IAS_ZONE_CLUSTER_ID);
                    }
                    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_XIAOMI &&
                             modelId.endsWith(QLatin1String("86opcn01"))) // Aqara Opple switches
                    {
                        fpSwitch.inClusters.push_back(MULTISTATE_INPUT_CLUSTER_ID);
                    }
                    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_JENNIC &&
                             modelId.startsWith(QLatin1String("lumi.sensor_natgas")))
                    {
                        fpCarbonMonoxideSensor.inClusters.push_back(IAS_ZONE_CLUSTER_ID);
                    }
                    else if (!node->nodeDescriptor().isNull() && node->nodeDescriptor().manufacturerCode() == VENDOR_NONE)
                    {
                        // For some device the Tuya cluster is sometime Invisible, so force device detection
                        if (manufacturer.endsWith(QLatin1String("kud7u2l"))  ||
                            manufacturer.endsWith(QLatin1String("eaxp72v")) ||
                            manufacturer.endsWith(QLatin1String("88teujp")) ||
                            manufacturer.endsWith(QLatin1String("uhszj9s")) ||
                            manufacturer.endsWith(QLatin1String("fvq6avy")) ||
                            manufacturer.endsWith(QLatin1String("w7cahqs")) ||
                            manufacturer.endsWith(QLatin1String("wdxldoj")) ||
                            manufacturer.endsWith(QLatin1String("GbxAXL2")))
                        {
                            fpThermostatSensor.inClusters.push_back(TUYA_CLUSTER_ID);
                        }
                        if (manufacturer.endsWith(QLatin1String("0yu2xgi")))
                        {
                            fpTemperatureSensor.inClusters.push_back(TUYA_CLUSTER_ID);
                            fpTemperatureSensor.inClusters.push_back(TEMPERATURE_MEASUREMENT_CLUSTER_ID);
                            fpHumiditySensor.inClusters.push_back(TUYA_CLUSTER_ID);
                            fpHumiditySensor.inClusters.push_back(RELATIVE_HUMIDITY_CLUSTER_ID);
                            fpAlarmSensor.inClusters.push_back(TUYA_CLUSTER_ID);
                            fpAlarmSensor.inClusters.push_back(IAS_ZONE_CLUSTER_ID);
                        }
                    }
                    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_EMBER &&
                             (manufacturer.endsWith(QLatin1String("oclfnxz")) ||
                              manufacturer.endsWith(QLatin1String("88teujp"))))
                    {
                        fpThermostatSensor.inClusters.push_back(TUYA_CLUSTER_ID);
                    }
                }
                    break;

                case IDENTIFY_CLUSTER_ID:
                {
                    if (manufacturer == QLatin1String("_TYST11_xu1rkty3"))
                    {
                        fpBatterySensor.inClusters.push_back(TUYA_CLUSTER_ID);
                    }
                }
                    break;

                case POWER_CONFIGURATION_CLUSTER_ID:
                {
                    fpAlarmSensor.inClusters.push_back(ci->id());
                    if (node->nodeDescriptor().manufacturerCode() == VENDOR_IKEA &&
                        (modelId.startsWith(QLatin1String("FYRTUR")) || modelId.startsWith(QLatin1String("KADRILJ"))))
                    {
                        fpBatterySensor.inClusters.push_back(ci->id());
                    }
                    if (node->nodeDescriptor().manufacturerCode() == VENDOR_XIAOMI &&
                        modelId.startsWith(QLatin1String("lumi.curtain.hagl04")))
                    {
                        fpBatterySensor.inClusters.push_back(ci->id());
                    }
                    if ((node->nodeDescriptor().manufacturerCode() == VENDOR_AXIS || node->nodeDescriptor().manufacturerCode() == VENDOR_MMB) &&
                        (modelId == QLatin1String("Gear")) && (i->endpoint() == 0x01))
                    {
                        fpBatterySensor.inClusters.push_back(ci->id());
                    }
                    fpCarbonMonoxideSensor.inClusters.push_back(ci->id());
                    fpFireSensor.inClusters.push_back(ci->id());
                    fpHumiditySensor.inClusters.push_back(ci->id());
                    fpLightSensor.inClusters.push_back(ci->id());
                    fpOpenCloseSensor.inClusters.push_back(ci->id());
                    fpPresenceSensor.inClusters.push_back(ci->id());
                    fpPressureSensor.inClusters.push_back(ci->id());
                    fpSwitch.inClusters.push_back(ci->id());
                    fpTemperatureSensor.inClusters.push_back(ci->id());
                    fpThermostatSensor.inClusters.push_back(ci->id());
                    fpTimeSensor.inClusters.push_back(ci->id());
                    fpVibrationSensor.inClusters.push_back(ci->id());
                    fpWaterSensor.inClusters.push_back(ci->id());
                }
                    break;

                case COMMISSIONING_CLUSTER_ID:
                {
                    if ((modelId == QLatin1String("ZYCT-202") || modelId == QLatin1String("ZLL-NonColorController")) && i->endpoint() != 0x01)
                    {
                        // ignore second endpoint
                    }
                    else if (modelId.startsWith(QLatin1String("Lightify Switch Mini")) ||  // Osram 3 button remote
                             modelId.startsWith(QLatin1String("Switch 4x EU-LIGHTIFY")) || // Osram 4 button remote
                             modelId.startsWith(QLatin1String("Switch 4x-LIGHTIFY")) || // Osram 4 button remote
                             modelId.startsWith(QLatin1String("Switch-LIGHTIFY"))) // Osram 4 button remote
                    {
                        // Don't create entry for this cluster
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
                    else if (manufacturer == QLatin1String("_TZ3000_bi6lpsew") ||
                             manufacturer == QLatin1String("_TZ3400_keyjhapk") ||
                             manufacturer == QLatin1String("_TYZB02_key8kk7r") ||
                             manufacturer == QLatin1String("_TZ3400_keyjqthh") ||
                             manufacturer == QLatin1String("_TZ3000_vp6clf9d") ||
                             manufacturer == QLatin1String("_TZ3400_key8kk7r") ||
                             manufacturer == QLatin1String("_TZ3000_peszejy7") ||
                             manufacturer == QLatin1String("_TZ3000_qzjcsmar") ||
                             manufacturer == QLatin1String("_TZ3000_owgcnkrh") ||
                             manufacturer == QLatin1String("_TZ3000_adkvzooy") ||
                             manufacturer == QLatin1String("_TZ3000_arfwfgoa") ||
                             manufacturer == QLatin1String("_TYZB02_keyjqthh"))
                    {
                        //Making the device only for endpoint 0x01
                        if (i->endpoint() == 0x01)
                        {
                            fpSwitch.inClusters.push_back(ci->id());
                        }
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
                    // Don't create ZHAalarm for this device using this cluster
                    if (manufacturer.endsWith(QLatin1String("0yu2xgi")))
                    {
                    }
                    else if (modelId.startsWith(QLatin1String("CO_")) ||                   // Heiman CO sensor
                        modelId.startsWith(QLatin1String("COSensor")) ||              // Heiman CO sensor (newer model)
                        modelId.startsWith(QLatin1String("lumi.sensor_natgas")))      // Xiaomi Mi gas detector
                    {
                        fpCarbonMonoxideSensor.inClusters.push_back(ci->id());
                    }
                    else if (modelId.startsWith(QLatin1String("DOOR_")) ||            // Heiman door/window sensor
                             modelId.startsWith(QLatin1String("Door")) ||             // Heiman door/window sensor (newer model)
                             modelId == QLatin1String("3AFE130104020015") ||          // Konke door/window sensor
                             modelId.startsWith(QLatin1String("902010/21")) ||        // Bitron door/window sensor
                             modelId.startsWith(QLatin1String("WISZB-1")) ||          // Develco door/window sensor
                             modelId.startsWith(QLatin1String("ZHMS101")) ||          // Wattle (Develco) door/window sensor
                             modelId.startsWith(QLatin1String("4655BC0")) ||          // Ecolink contact sensor
                             modelId.startsWith(QLatin1String("3300")) ||             // Centralite contact sensor
                             modelId == QLatin1String("lumi.sensor_magnet.agl02") ||  // Xiaomi Aqara T1 open/close sensor MCCGQ12LM
                             modelId == QLatin1String("E1D-G73") ||                   // Sengled contact sensor
                             modelId == QLatin1String("DS01") ||                      // Sonoff SNZB-04
                             modelId == QLatin1String("GMB-HAS-DW-B01") ||            // GamaBit Ltd. Window/Door Sensor
                             modelId == QLatin1String("TY0203") ||                    // lidl / SilverCrest
                             modelId == QLatin1String("RH3001"))                      // Tuya/Blitzwolf BW-IS2 door/window sensor
                    {
                        fpOpenCloseSensor.inClusters.push_back(ci->id());
                    }
                    else if (modelId.startsWith(QLatin1String("PIR_")) ||             // Heiman motion sensor
                             modelId.startsWith(QLatin1String("PIRS")) ||             // Heiman motion sensor (newer model)
                             modelId == QLatin1String("3AFE14010402000D") ||          // Konke motion sensor
                             modelId == QLatin1String("3AFE28010402000D") ||          // Konke motion sensor ver.2
                             modelId == QLatin1String("motion") ||                    // Samjin motion sensor
                             modelId == QLatin1String("ZB-MotionSensor-D0003") ||     // Linkind motion sensor
                             modelId == QLatin1String("3041") ||                      // NYCE motion sensor
                             modelId.startsWith(QLatin1String("902010/22")) ||        // Bitron motion sensor
                             modelId.startsWith(QLatin1String("SN10ZW")) ||           // ORVIBO motion sensor
                             modelId.startsWith(QLatin1String("MOSZB-1")) ||          // Develco motion sensor
                             modelId.startsWith(QLatin1String("MotionSensor51AU")) || // Aurora (Develco) motion sensor
                             modelId.startsWith(QLatin1String("MOT003")) ||           // Hive motion sensor
                             modelId == QLatin1String("4in1-Sensor-ZB3.0") ||         // Immax NEO ZB3.0 4 in 1 sensor E13-A21
                             modelId == QLatin1String("GZ-PIR02") ||                  // Sercomm motion sensor
                             modelId == QLatin1String("E13-A21") ||                   // Sengled E13-A21 PAR38 bulp with motion sensor
                             modelId == QLatin1String("TS0202") ||                    // Tuya generic motion sensor
                             modelId == QLatin1String("TY0202") ||                    // Lidl/Silvercrest Smart Motion Sensor
                             modelId == QLatin1String("66666") ||                     // Sonoff SNZB-03
                             modelId == QLatin1String("MS01") ||                      // Sonoff SNZB-03
                             modelId == QLatin1String("MSO1") ||                      // Sonoff SNZB-03
                             modelId == QLatin1String("ms01"))                        // Sonoff SNZB-03
                    {
                        fpPresenceSensor.inClusters.push_back(ci->id());
                    }
                    else if (modelId.startsWith(QLatin1String("GAS")) ||                     // Heiman gas sensor (old and newer model)
                             modelId.startsWith(QLatin1String("SMOK_")) ||                   // Heiman fire sensor
                             modelId.startsWith(QLatin1String("Smoke")) ||                   // Heiman fire sensor (newer model)
                             modelId.startsWith(QLatin1String("902010/24")) ||               // Bitron smoke detector
                             modelId.startsWith(QLatin1String("SMSZB-1")) ||                 // Develco smoke detector
                             modelId.startsWith(QLatin1String("HESZB-1")) ||                 // Develco heat detector
                             modelId.startsWith(QLatin1String("SF2")) ||                     // ORVIBO (Heiman) smoke sensor
                             modelId == QLatin1String("358e4e3e03c644709905034dae81433e") || // Orvibo Combustible Gas Sensor
                             modelId.startsWith(QLatin1String("LH05121")) ||                 // iHorn smoke detector
                             modelId.startsWith(QLatin1String("lumi.sensor_smoke")) ||       // Xiaomi Mi smoke sensor
                             modelId.startsWith(QLatin1String("TS0204")) ||                  // Tuya gas sensor
                             modelId.startsWith(QLatin1String("TS0205")) ||                  // Tuya smoke sensor
                             modelId == QLatin1String("SD8SC_00.00.03.09TC") ||              // Centralite smoke sensor
                             modelId.startsWith(QLatin1String("FNB56-SMF")) ||               // Feibit smoke detector
                             modelId.startsWith(QLatin1String("FNB56-COS")) ||               // Feibit FNB56-COS06FB1.7 Carb. Mon. detector
                             modelId.startsWith(QLatin1String("FNB56-GAS")))                 // Feibit gas sensor
                    {
                        // Gas sensor detects combustable gas, so fire is more appropriate than CO.
                        fpFireSensor.inClusters.push_back(ci->id());
                    }
                    else if (modelId.startsWith(QLatin1String("WATER_")) ||           // Heiman water sensor
                             modelId.startsWith(QLatin1String("Water")) ||            // Heiman water sensor (newer model)
                             modelId.startsWith(QLatin1String("lumi.sensor_wleak")) || // Xiaomi Aqara flood sensor
                             modelId == QLatin1String("lumi.flood.agl02") ||          // Xiaomi Aqara T1 water leak sensor SJCGQ12LM
                             modelId.startsWith(QLatin1String("moisturev4")) ||       // SmartThings water leak sensor
                             modelId.startsWith(QLatin1String("WL4200")) ||           // Sinope Water Leak detector
                             modelId.startsWith(QLatin1String("3315")) ||             // Centralite water sensor
                             modelId.startsWith(QLatin1String("SZ-WTD02N_CAR")) ||    // Sercomm water sensor
                             modelId.startsWith(QLatin1String("FLSZB-1")) ||          // Develco Water Leak detector
                             modelId == QLatin1String("GMB-HAS-WL-B01") ||            // GamaBit Ltd. water leak Sensor
                             modelId.startsWith(QLatin1String("TS0207")))             // Tuya water leak sensor
                    {
                        fpWaterSensor.inClusters.push_back(ci->id());
                    }
                    else if (modelId == QLatin1String("WarningDevice") ||               // Heiman siren
                             modelId == QLatin1String("SZ-SRN12N") ||                   // Sercomm siren
                             modelId == QLatin1String("SIRZB-1") ||                     // Develco siren
                             modelId == QLatin1String("902010/29"))                     // Bitron outdoor siren
                    {
                        fpAlarmSensor.inClusters.push_back(ci->id());
                    }
                    else if (modelId == QLatin1String("GMB-HAS-VB-B01"))                // GamaBit Ltd. Vibration Sensor
                    {
                        fpVibrationSensor.inClusters.push_back(ci->id());
                    }
                    else if ((manufacturer == QLatin1String("Samjin") && modelId == QLatin1String("button")) ||
                              modelId == QLatin1String("Keyfob-ZB3.0") || modelId == QLatin1String("TS0211"))
                    {
                        fpSwitch.inClusters.push_back(ci->id());
                    }
                    else if (!modelId.isEmpty())
                    {
                        for (const deCONZ::ZclAttribute &attr : ci->attributes())
                        {
                            if (attr.id() == 0x0001 && attr.lastRead() != static_cast<time_t>(-1)) // IAS Zone type
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
                                        else if (modelId.startsWith(QLatin1String("lumi.sensor_motion")))
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
                    if (node->nodeDescriptor().manufacturerCode() == VENDOR_CENTRALITE ||
                        node->nodeDescriptor().manufacturerCode() == VENDOR_C2DF)
                    {
                        // only use IAS Zone cluster on endpoint 0x01 for Centralite motion sensors
                    }
                    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_NYCE)
                    {
                        // only use IAS Zone cluster on endpoint 0x01 for NYCE motion sensors
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

                    if (modelId == QLatin1String("VOC_Sensor"))
                    {
                        fpHumiditySensor.inClusters.push_back(ci->id());
                    }

                    // Hive devices, don't show model id faster enought
                    if ((node->nodeDescriptor().manufacturerCode() == VENDOR_ALERTME) && (modelId.isEmpty()))
                    {
                    }
                    // Don't create entry for the plug
                    else if (modelId == QLatin1String("SLP2b"))
                    {
                    }
                    // Don't create entry for cluster 0x07 and 0x08 for Hive thermostat
                    else if ((modelId.startsWith(QLatin1String("SLR2")) || modelId == QLatin1String("SLR1b")) && i->endpoint() > 0x06)
                    {
                    }
                    // Don't create entry for the door lock
                    //else if (modelId == QLatin1String("SMARTCODE_CONVERT_GEN1"))
                    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_KWIKSET)
                    {
                    }
                    // Don't create entries for the following Danfoss in-room thermostats
                    else if(modelId == QLatin1String("0x8020") ||
                            modelId == QLatin1String("0x8030") ||
                            modelId == QLatin1String("0x8034"))
                    {
                    }
                    else
                    {
                        fpTemperatureSensor.inClusters.push_back(ci->id());
                    }
                }
                    break;

                case RELATIVE_HUMIDITY_CLUSTER_ID:
                {
                    if (modelId == QLatin1String("AIR") && node->nodeDescriptor().manufacturerCode() == VENDOR_BOSCH2)
                    {
                       // use manufacturer specific cluster instead
                    }
                    // Don't create sensor for first endpoint
                    else if (modelId == QLatin1String("ST30 Temperature Sensor") && i->endpoint() != 0x02)
                    {
                    }
                    else if (modelId != QLatin1String("VOC_Sensor"))
                    {
                        fpHumiditySensor.inClusters.push_back(ci->id());
                    }
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
                    else if (modelId.startsWith(QLatin1String("lumi.plug")) || modelId.startsWith(QLatin1String("lumi.ctrl_ln1")))
                    {
                        if (i->endpoint() == 0x02 || i->endpoint() == 0x15)
                        {
                            fpPowerSensor.inClusters.push_back(ci->id());
                        }
                        else if (i->endpoint() == 0x03 || i->endpoint() == 0x16)
                        {
                            fpConsumptionSensor.inClusters.push_back(ci->id());
                        }
                    }
                    else if (modelId.startsWith(QLatin1String("lumi.ctrl_ln2")))
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
                    else if (modelId.startsWith(QLatin1String("lumi.relay.c")))
                    {
                        fpConsumptionSensor.inClusters.push_back(ci->id());
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
                    else if ((modelId == QLatin1String("lumi.remote.b186acn01") ||
                              modelId == QLatin1String("lumi.remote.b186acn02"))
                             && i->endpoint() == 0x01)
                    {
                        fpSwitch.inClusters.push_back(ci->id());
                    }
                    else if ((modelId == QLatin1String("lumi.remote.b286acn01") ||
                              modelId == QLatin1String("lumi.remote.b286acn02"))
                             && i->endpoint() == 0x01)
                    {
                        fpSwitch.inClusters.push_back(ci->id());
                    }
                }
                    break;

                case BINARY_INPUT_CLUSTER_ID:
                {
                    if (modelId.startsWith(QLatin1String("tagv4")) || // SmartThings Arrival sensor
                        modelId == QLatin1String("Remote motion sensor")) // Legrand motion sensor
                    {
                        fpPresenceSensor.inClusters.push_back(ci->id());
                    }

                }
                    break;

                case DOOR_LOCK_CLUSTER_ID:
                {
                    if (modelId.startsWith(QLatin1String("lumi.vibration"))) // lumi.vibration
                    {
                        // fpSwitch.inClusters.push_back(DOOR_LOCK_CLUSTER_ID);
                        fpVibrationSensor.inClusters.push_back(DOOR_LOCK_CLUSTER_ID);
                    }
                }
                    break;

                case LEGRAND_CONTROL_CLUSTER_ID:
                {
                    if (modelId == QLatin1String("Cable outlet"))
                    {
                        fpThermostatSensor.inClusters.push_back(LEGRAND_CONTROL_CLUSTER_ID);
                    }
                }
                    break;

                case TUYA_CLUSTER_ID:
                {
                    if (manufacturer.endsWith(QLatin1String("kud7u2l")) ||
                        manufacturer.endsWith(QLatin1String("GbxAXL2")) ||
                        manufacturer.endsWith(QLatin1String("eaxp72v")) ||
                        manufacturer.endsWith(QLatin1String("fvq6avy")) ||
                        manufacturer.endsWith(QLatin1String("uhszj9s")) ||
                        manufacturer.endsWith(QLatin1String("oclfnxz")) ||
                        manufacturer.endsWith(QLatin1String("w7cahqs")) ||
                        manufacturer.endsWith(QLatin1String("wdxldoj")) ||
                        manufacturer.endsWith(QLatin1String("88teujp")))
                    {
                        fpThermostatSensor.inClusters.push_back(TUYA_CLUSTER_ID);
                    }
                }
                    break;

                case SAMJIN_CLUSTER_ID:
                {
                    if (modelId.startsWith(QLatin1String("multi"))) // Samjin Multipurpose sensor
                    {
                        fpVibrationSensor.inClusters.push_back(SAMJIN_CLUSTER_ID);
                    }
                }
                    break;

                case METERING_CLUSTER_ID:
                {
                    if(modelId != QLatin1String("lumi.plug.mmeu01"))
                    {
                        fpConsumptionSensor.inClusters.push_back(ci->id());
                    }
                }
                    break;

                case ELECTRICAL_MEASUREMENT_CLUSTER_ID:
                {
                    if(modelId != QLatin1String("lumi.plug.mmeu01") && modelId != QLatin1String("160-01"))
                    {
                        fpPowerSensor.inClusters.push_back(ci->id());
                    }
                }
                    break;

                case THERMOSTAT_CLUSTER_ID:
                {
                    if(modelId != QLatin1String("VOC_Sensor"))
                    {
                        fpThermostatSensor.inClusters.push_back(ci->id());
                    }
                }
                    break;

                case VENDOR_CLUSTER_ID:
                {
                    if (modelId.startsWith(QLatin1String("RWL02")) || // Hue dimmer switch
                        modelId.startsWith(QLatin1String("ROM00")) || // Hue smart button
                        modelId.startsWith(QLatin1String("RDM00")) || // Hue wall switch module
                        modelId.startsWith(QLatin1String("Z3-1BRL"))) // Lutron Aurora FoH dimmer switch
                    {
                        fpSwitch.inClusters.push_back(ci->id());
                    }
                    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_DDEL && modelId == QLatin1String("de_spect"))
                    {
                        if (i->endpoint() <= 0x03)
                        {
                            fpSpectralSensor.inClusters.push_back(ci->id());
                        }
                    }
                }
                    break;

                case APPLIANCE_EVENTS_AND_ALERTS_CLUSTER_ID:
                {
                    if (modelId == QLatin1String("leakSMART Water Sensor V2"))
                    {
                        fpWaterSensor.inClusters.push_back(ci->id());
                    }
                }
                    break;

                case TIME_CLUSTER_ID:
                {
                    // Many Xiaomi devices advertise non-functional Time cluster, so better use whitelist.
                    if (modelId == QLatin1String("Thermostat") || // eCozy
                        modelId == QLatin1String("eTRV0100")) // Danfoss
                    {
                        fpTimeSensor.inClusters.push_back(ci->id());
                    }
                }
                    break;

                case DEVELCO_AIR_QUALITY_CLUSTER_ID:
                {
                    if (modelId == QLatin1String("AQSZB-110"))  // Develco air quality sensor
                    {
                        fpAirQualitySensor.inClusters.push_back(ci->id());
                    }
                }
                    break;

                case BOSCH_AIR_QUALITY_CLUSTER_ID:
                {
                    if (node->nodeDescriptor().manufacturerCode() == VENDOR_BOSCH2 && modelId == QLatin1String("AIR"))
                    {
                        fpAirQualitySensor.inClusters.push_back(ci->id());
                        // fake proper clusterIds
                        fpLightSensor.inClusters.push_back(ILLUMINANCE_MEASUREMENT_CLUSTER_ID);
                        fpLightSensor.inClusters.push_back(ci->id());
                        fpPressureSensor.inClusters.push_back(PRESSURE_MEASUREMENT_CLUSTER_ID);
                        fpPressureSensor.inClusters.push_back(ci->id());
                        fpHumiditySensor.inClusters.push_back(RELATIVE_HUMIDITY_CLUSTER_ID);
                        fpHumiditySensor.inClusters.push_back(ci->id());
                    }
                }
                    break;

                case THERMOSTAT_UI_CONFIGURATION_CLUSTER_ID:
                case DIAGNOSTICS_CLUSTER_ID:
                case FAN_CONTROL_CLUSTER_ID:
                {
                    fpThermostatSensor.inClusters.push_back(ci->id());
                }
                    break;

                case XIAOMI_CLUSTER_ID:    // Xiaomi specific
                {
                    if (modelId.startsWith(QLatin1String("lumi.")))
                    {
                        fpConsumptionSensor.inClusters.push_back(ci->id());
                        fpHumiditySensor.inClusters.push_back(ci->id());
                        fpLightSensor.inClusters.push_back(ci->id());
                        fpOpenCloseSensor.inClusters.push_back(ci->id());
                        fpPowerSensor.inClusters.push_back(ci->id());
                        fpPresenceSensor.inClusters.push_back(ci->id());
                        fpPressureSensor.inClusters.push_back(ci->id());
                        fpSwitch.inClusters.push_back(ci->id());
                        fpTemperatureSensor.inClusters.push_back(ci->id());
                        fpThermostatSensor.inClusters.push_back(ci->id());
                    }
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
                    if (modelId == QLatin1String("ZYCT-202") || modelId == QLatin1String("ZLL-NonColorController") || modelId == QLatin1String("Adurolight_NCC"))
                    {
                        fpSwitch.outClusters.push_back(ci->id());
                    }
                    else if (modelId.startsWith(QLatin1String("RC 110")) ||   // innr RC 110
                             modelId.startsWith(QLatin1String("ICZB-RM")) ||  // icasa remote
                             modelId.startsWith(QLatin1String("ZGR904-S")) || // Envilar remote
                             modelId.startsWith(QLatin1String("ZGRC-KEY")) || // Sunricher remote
                             modelId.startsWith(QLatin1String("ED-1001")) ||  // EcoDim switches
                             modelId.startsWith(QLatin1String("45127")))      // Namron switches
                    {
                        if (i->endpoint() == 0x01) // create sensor only for first endpoint
                        {
                            fpSwitch.outClusters.push_back(ci->id());
                        }
                    }
                    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_JENNIC ||
                             modelId.endsWith(QLatin1String("86opcn01"))) // Aqara Opple prevent client clusters creation, client clusters aren't used, fpSwitch is created for 0xfc00 cluster.
                    {
                        // prevent creation of ZHASwitch, till supported
                    }
                    else if (i->deviceId() == DEV_ID_ZLL_ONOFF_SENSOR &&
                        node->nodeDescriptor().manufacturerCode() == VENDOR_IKEA)
                    {
                        fpPresenceSensor.outClusters.push_back(ci->id());
                    }
                    else if (modelId.startsWith(QLatin1String("Lightify Switch Mini")) ||  // Osram 3 button remote
                             modelId.startsWith(QLatin1String("Switch 4x EU-LIGHTIFY")) || // Osram 4 button remote
                             modelId.startsWith(QLatin1String("Switch 4x-LIGHTIFY")) || // Osram 4 button remote
                             modelId.startsWith(QLatin1String("Switch-LIGHTIFY"))) // Osram 4 button remote
                    {
                        // Only create entry for endpoint 0x01
                        fpSwitch.outClusters.push_back(ci->id());
                    }
                    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_UBISYS)
                    {
                        if ((modelId.startsWith(QLatin1String("D1")) && i->endpoint() == 0x02) ||
                            (modelId.startsWith(QLatin1String("J1")) && i->endpoint() == 0x02) ||
                            (modelId.startsWith(QLatin1String("C4")) && i->endpoint() == 0x01) ||
                            (modelId.startsWith(QLatin1String("S1")) && i->endpoint() == 0x02) ||
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

                case IAS_ACE_CLUSTER_ID:
                {
                    if (modelId == QLatin1String("Keyfob-ZB3.0") || modelId == QLatin1String("TS0215") || modelId == QLatin1String("RC_V14") ||
                        modelId == QLatin1String("RC-EM") || modelId == QLatin1String("RC-EF-3.0"))
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
            fpSwitch.hasInCluster(IAS_ZONE_CLUSTER_ID) ||
            fpSwitch.hasOutCluster(IAS_ACE_CLUSTER_ID) ||
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
                if (modelId != QLatin1String("RWL022")) // new model with one endpoint
                {
                    fpSwitch.endpoint = 2;
                }
            }

            if (modelId.startsWith(QLatin1String("Lightify Switch Mini")) ||  // Osram 3 button remote
                modelId.startsWith(QLatin1String("Switch 4x EU-LIGHTIFY")) || // Osram 4 button remote
                modelId.startsWith(QLatin1String("Switch 4x-LIGHTIFY")) || // Osram 4 button remote
                modelId.startsWith(QLatin1String("Switch-LIGHTIFY"))) // Osram 4 button remote
            {
                sensor = getSensorNodeForAddress(node->address().ext());
                if (sensor && sensor->deletedState() != Sensor::StateNormal)
                {
                    sensor = nullptr;
                }
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
                checkIasEnrollmentStatus(sensor);
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
                checkIasEnrollmentStatus(sensor);
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
                checkIasEnrollmentStatus(sensor);
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
                checkIasEnrollmentStatus(sensor);
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
                checkIasEnrollmentStatus(sensor);
            }
        }

        // ZHAVibration
        if (fpVibrationSensor.hasInCluster(IAS_ZONE_CLUSTER_ID) ||
            fpVibrationSensor.hasInCluster(SAMJIN_CLUSTER_ID) ||
            fpVibrationSensor.hasInCluster(DOOR_LOCK_CLUSTER_ID))
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
                checkIasEnrollmentStatus(sensor);
            }
        }

        // ZHAWater
        if (fpWaterSensor.hasInCluster(IAS_ZONE_CLUSTER_ID) ||
            fpWaterSensor.hasInCluster(APPLIANCE_EVENTS_AND_ALERTS_CLUSTER_ID))
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
                checkIasEnrollmentStatus(sensor);
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

        // ZHASpectral
        if (fpSpectralSensor.hasInCluster(VENDOR_CLUSTER_ID))
        {
            fpSpectralSensor.endpoint = i->endpoint();
            fpSpectralSensor.deviceId = i->deviceId();
            fpSpectralSensor.profileId = i->profileId();

            sensor = getSensorNodeForFingerPrint(node->address().ext(), fpTemperatureSensor, "ZHASpectral");
            if (!sensor || sensor->deletedState() != Sensor::StateNormal)
            {
                addSensorNode(node, fpSpectralSensor, "ZHASpectral", modelId, manufacturer);
            }
            else
            {
                checkSensorNodeReachable(sensor);
            }
        }

        // ZHAThermostat
        if (fpThermostatSensor.hasInCluster(THERMOSTAT_CLUSTER_ID) ||
           (fpThermostatSensor.hasInCluster(LEGRAND_CONTROL_CLUSTER_ID) && modelId == QLatin1String("Cable outlet")) ||
           (fpThermostatSensor.hasInCluster(TUYA_CLUSTER_ID)))
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

        // ZHABattery
        if (fpBatterySensor.hasInCluster(POWER_CONFIGURATION_CLUSTER_ID) ||
            fpBatterySensor.hasInCluster(TUYA_CLUSTER_ID))
        {
            fpBatterySensor.endpoint = i->endpoint();
            fpBatterySensor.deviceId = i->deviceId();
            fpBatterySensor.profileId = i->profileId();

            sensor = getSensorNodeForFingerPrint(node->address().ext(), fpBatterySensor, "ZHABattery");
            if (!sensor || sensor->deletedState() != Sensor::StateNormal)
            {
                addSensorNode(node, fpBatterySensor, "ZHABattery", modelId, manufacturer);
            }
            else
            {
                checkSensorNodeReachable(sensor);
            }
        }

        // ZHATime
        if (fpTimeSensor.hasInCluster(TIME_CLUSTER_ID))
        {
            fpTimeSensor.endpoint = i->endpoint();
            fpTimeSensor.deviceId = i->deviceId();
            fpTimeSensor.profileId = i->profileId();

            sensor = getSensorNodeForFingerPrint(node->address().ext(), fpTimeSensor, "ZHATime");
            if (!sensor || sensor->deletedState() != Sensor::StateNormal)
            {
                addSensorNode(node, fpTimeSensor, "ZHATime", modelId, manufacturer);
            }
            else
            {
                checkSensorNodeReachable(sensor);
            }
        }

        // ZHAAirQuality
        if (fpAirQualitySensor.hasInCluster(DEVELCO_AIR_QUALITY_CLUSTER_ID)   // Develco specific -> VOC Management
            || fpAirQualitySensor.hasInCluster(BOSCH_AIR_QUALITY_CLUSTER_ID)) // Bosch Air quality sensor
        {
            fpAirQualitySensor.endpoint = i->endpoint();
            fpAirQualitySensor.deviceId = i->deviceId();
            fpAirQualitySensor.profileId = i->profileId();

            sensor = getSensorNodeForFingerPrint(node->address().ext(), fpAirQualitySensor, QLatin1String("ZHAAirQuality"));
            if (!sensor || sensor->deletedState() != Sensor::StateNormal)
            {
                addSensorNode(node, fpAirQualitySensor, QLatin1String("ZHAAirQuality"), modelId, manufacturer);
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
    DBG_Assert(node);
    if (!node)
    {
        return;
    }

    // Xiaomi plug might contain invalid sensor clusters
    // prevent creation of related sensors for clusters like 0x0400, 0x0402, 0x0403, 0x0405, 0x0406
    // https://github.com/dresden-elektronik/deconz-rest-plugin/issues/1094
    if (modelId.startsWith(QLatin1String("lumi.plug")) &&
       !(type == QLatin1String("ZHAConsumption") || type == QLatin1String("ZHAPower")))
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

    if (!manufacturer.isEmpty())
    {
        sensorNode.setManufacturer(manufacturer);
    }

    // simple check if existing device needs to be updated
    Sensor *sensor2 = nullptr;
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
        if (manufacturer.startsWith(QLatin1String("Climax")) ||
            sensorNode.modelId().startsWith(QLatin1String("902010/23")))
        {
            // climax non IAS reports state/lowbattery via battery alarm mask attribute
            sensorNode.addItem(DataTypeBool, RStateLowBattery);
            // don't set value -> null until reported
        }
        else if (sensorNode.modelId() == QLatin1String("lumi.sensor_natgas") ||
                 sensorNode.modelId() == QLatin1String("Bell"))
        {
            // Don't expose battery resource item for this device
        }
        else if (!sensorNode.type().endsWith(QLatin1String("Battery")))
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
        else if (sensorNode.fingerPrint().hasInCluster(WINDOW_COVERING_CLUSTER_ID) ||
                 sensorNode.fingerPrint().hasOutCluster(WINDOW_COVERING_CLUSTER_ID))
        {
            clusterId = WINDOW_COVERING_CLUSTER_ID;
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
        else if (sensorNode.fingerPrint().hasInCluster(IAS_ZONE_CLUSTER_ID))
        {
            clusterId = IAS_ZONE_CLUSTER_ID;
        }
        else if (sensorNode.fingerPrint().hasOutCluster(IAS_ACE_CLUSTER_ID))
        {
            clusterId = IAS_ACE_CLUSTER_ID;
        }
        sensorNode.addItem(DataTypeInt32, RStateButtonEvent);

        if (modelId.startsWith(QLatin1String("lumi.sensor_cube")))
        {
            sensorNode.addItem(DataTypeInt32, RStateGesture);
        }
        else if (modelId.startsWith(QLatin1String("RWL02")) || // Hue dimmer switch
                 modelId.startsWith(QLatin1String("ROM00")) || // Hue smart button
                 modelId.startsWith(QLatin1String("RDM00")) || // Hue wall switch module
                 modelId.startsWith(QLatin1String("Z3-1BRL"))) // Lutron Aurora Firends-of-Hue dimmer switch
        {
            clusterId = VENDOR_CLUSTER_ID;
            sensorNode.addItem(DataTypeUInt16, RStateEventDuration);
        }
        else if (modelId.startsWith(QLatin1String("ZBT-Remote-ALL-RGBW")))
        {
            sensorNode.addItem(DataTypeUInt16, RStateX);
            sensorNode.addItem(DataTypeUInt16, RStateY);
            sensorNode.addItem(DataTypeUInt16, RStateAngle);
        }
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
    else if (sensorNode.type().endsWith(QLatin1String("Spectral")))
    {
        if (sensorNode.fingerPrint().hasInCluster(VENDOR_CLUSTER_ID))
        {
            clusterId = VENDOR_CLUSTER_ID;
        }

        if (modelId == QLatin1String("de_spect"))
        {
            item = sensorNode.item(RConfigOn); // default off
            item->setValue(false);
        }

        sensorNode.addItem(DataTypeUInt16, RStateSpectralX);
        sensorNode.addItem(DataTypeUInt16, RStateSpectralY);
        sensorNode.addItem(DataTypeUInt16, RStateSpectralZ);
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
        else if (modelId.startsWith(QLatin1String("lumi.sensor_motion"))) // reporting under motion varies between 60 - 90 seconds
        {
            item->setValue(90);
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

        if (R_GetProductId(&sensorNode) == QLatin1String("NAS-AB02B0 Siren"))
        {
            sensorNode.addItem(DataTypeUInt8, RConfigMelody);
            sensorNode.addItem(DataTypeString, RConfigPreset);
            sensorNode.addItem(DataTypeUInt8, RConfigVolume);
            sensorNode.addItem(DataTypeString, RConfigTempThreshold);
            sensorNode.addItem(DataTypeString, RConfigHumiThreshold);
        }
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
        else if (sensorNode.fingerPrint().hasInCluster(DOOR_LOCK_CLUSTER_ID))
        {
            clusterId = DOOR_LOCK_CLUSTER_ID;
        }
        else if (sensorNode.fingerPrint().hasInCluster(SAMJIN_CLUSTER_ID))
        {
            clusterId = SAMJIN_CLUSTER_ID;
            item = sensorNode.addItem(DataTypeInt16, RStateOrientationX);
            item = sensorNode.addItem(DataTypeInt16, RStateOrientationY);
            item = sensorNode.addItem(DataTypeInt16, RStateOrientationZ);
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
        else if (sensorNode.fingerPrint().hasInCluster(APPLIANCE_EVENTS_AND_ALERTS_CLUSTER_ID))
        {
            clusterId = APPLIANCE_EVENTS_AND_ALERTS_CLUSTER_ID;
        }
        item = sensorNode.addItem(DataTypeBool, RStateWater);
        item->setValue(false);
    }
    else if (sensorNode.type().endsWith(QLatin1String("Consumption")))
    {
        if (sensorNode.fingerPrint().hasInCluster(METERING_CLUSTER_ID))
        {
            clusterId = METERING_CLUSTER_ID;
            if (modelId != QLatin1String("160-01"))
            {
                item = sensorNode.addItem(DataTypeUInt64, RStateConsumption);
            }
            if ((modelId != QLatin1String("SP 120")) &&
                (modelId != QLatin1String("ZB-ONOFFPlug-D0005")) &&
                (modelId != QLatin1String("TS0121")) &&
                (!modelId.startsWith(QLatin1String("BQZ10-AU"))) &&
                (!modelId.startsWith(QLatin1String("ROB_200"))) &&
                (!modelId.startsWith(QLatin1String("lumi.plug.ma"))) &&
                (modelId != QLatin1String("Plug-230V-ZB3.0")) &&
                (modelId != QLatin1String("lumi.switch.b1naus01")) &&
                (modelId != QLatin1String("Connected socket outlet")) &&
                (!modelId.startsWith(QLatin1String("SPW35Z"))))
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
            if ( (!modelId.startsWith(QLatin1String("Plug"))) &&
                 (!modelId.startsWith(QLatin1String("ZB-ONOFFPlug-D0005"))) &&
                 (modelId != QLatin1String("Plug-230V-ZB3.0")) &&
                 (modelId != QLatin1String("lumi.switch.b1naus01")) &&
                 (node->nodeDescriptor().manufacturerCode() != VENDOR_LEGRAND) ) // OSRAM and Legrand plug don't have theses options
            {
                item = sensorNode.addItem(DataTypeUInt16, RStateVoltage);
                item = sensorNode.addItem(DataTypeUInt16, RStateCurrent);
            }
        }
        else if (sensorNode.fingerPrint().hasInCluster(ANALOG_INPUT_CLUSTER_ID))
        {
            clusterId = ANALOG_INPUT_CLUSTER_ID;
            item = sensorNode.addItem(DataTypeInt16, RStatePower);
            if (modelId.startsWith(QLatin1String("lumi.plug.mm"))) // Only available for new ZB3.0 Mi smart plugs?
            {
                item = sensorNode.addItem(DataTypeUInt16, RStateVoltage);
                item = sensorNode.addItem(DataTypeUInt16, RStateCurrent);
            }
        }
    }
    else if (sensorNode.type().endsWith(QLatin1String("Thermostat")))
    {
        if (sensorNode.fingerPrint().hasInCluster(THERMOSTAT_CLUSTER_ID) || sensorNode.fingerPrint().hasInCluster(TUYA_CLUSTER_ID))
        {
            clusterId = THERMOSTAT_CLUSTER_ID;
        }
        //Only for legrand cluster, add only mode field.
        if ((sensorNode.fingerPrint().hasInCluster(LEGRAND_CONTROL_CLUSTER_ID)) && (sensorNode.modelId() == QLatin1String("Cable outlet") ) )
        {
            clusterId = LEGRAND_CONTROL_CLUSTER_ID;
            sensorNode.addItem(DataTypeString, RConfigMode);
        }
        else
        {
            sensorNode.addItem(DataTypeInt16, RStateTemperature);
            item = sensorNode.addItem(DataTypeInt16, RConfigOffset);
            item->setValue(0);
            sensorNode.addItem(DataTypeInt16, RConfigHeatSetpoint);    // Heating set point
            sensorNode.addItem(DataTypeBool, RStateOn)->setValue(false);           // Heating on/off

            if (sensorNode.modelId().startsWith(QLatin1String("SLR2")) ||   // Hive
                sensorNode.modelId() == QLatin1String("SLR1b") ||           // Hive
                sensorNode.modelId().startsWith(QLatin1String("TH112")) ||  // Sinope
                sensorNode.modelId() == QLatin1String("902010/32") ||       // Bitron
                R_GetProductId(&sensorNode) == QLatin1String("Tuya_THD SEA801-ZIGBEE TRV") ||
                R_GetProductId(&sensorNode) == QLatin1String("Tuya_THD WZB-TRVL TRV") ||
                R_GetProductId(&sensorNode) == QLatin1String("Tuya_THD Smart radiator TRV") ||
                R_GetProductId(&sensorNode) == QLatin1String("Tuya_THD HY368 TRV") ||
                R_GetProductId(&sensorNode) == QLatin1String("Tuya_THD HY369 TRV"))
            {
                sensorNode.addItem(DataTypeString, RConfigMode);
            }

            if (R_GetProductId(&sensorNode) == QLatin1String("Tuya_THD HY369 TRV") ||
                R_GetProductId(&sensorNode) == QLatin1String("Tuya_THD HY368 TRV") ||
                R_GetProductId(&sensorNode) == QLatin1String("Tuya_THD WZB-TRVL TRV") ||
                R_GetProductId(&sensorNode) == QLatin1String("Tuya_THD Smart radiator TRV") ||
                R_GetProductId(&sensorNode) == QLatin1String("Tuya_THD SEA801-ZIGBEE TRV"))
            {
                sensorNode.addItem(DataTypeUInt8, RStateValve);
                sensorNode.addItem(DataTypeBool, RStateLowBattery)->setValue(false);
            }

            if (R_GetProductId(&sensorNode) == QLatin1String("Tuya_THD HY369 TRV") ||
                R_GetProductId(&sensorNode) == QLatin1String("Tuya_THD HY368 TRV") ||
                R_GetProductId(&sensorNode) == QLatin1String("Tuya_THD Essentials TRV") ||
                R_GetProductId(&sensorNode) == QLatin1String("Tuya_THD SEA801-ZIGBEE TRV") ||
                R_GetProductId(&sensorNode) == QLatin1String("Tuya_THD NX-4911-675 TRV") ||
                R_GetProductId(&sensorNode) == QLatin1String("Tuya_THD WZB-TRVL TRV") ||
                R_GetProductId(&sensorNode) == QLatin1String("Tuya_THD Smart radiator TRV") ||
                R_GetProductId(&sensorNode) == QLatin1String("Tuya_THD BTH-002 Thermostat"))
            {
                sensorNode.addItem(DataTypeBool, RConfigLocked)->setValue(false);
            }

            if (R_GetProductId(&sensorNode) == QLatin1String("Tuya_THD HY369 TRV") ||
                R_GetProductId(&sensorNode) == QLatin1String("Tuya_THD HY368 TRV") ||
                R_GetProductId(&sensorNode) == QLatin1String("Tuya_THD Essentials TRV") ||
                R_GetProductId(&sensorNode) == QLatin1String("Tuya_THD SEA801-ZIGBEE TRV") ||
                R_GetProductId(&sensorNode) == QLatin1String("Tuya_THD NX-4911-675 TRV") ||
                R_GetProductId(&sensorNode) == QLatin1String("Tuya_THD Smart radiator TRV") ||
                R_GetProductId(&sensorNode) == QLatin1String("Tuya_THD BTH-002 Thermostat"))
            {
                sensorNode.addItem(DataTypeString, RConfigPreset);
                sensorNode.addItem(DataTypeBool, RConfigSetValve)->setValue(false);
            }

            if (R_GetProductId(&sensorNode) == QLatin1String("Tuya_THD HY369 TRV") ||
                R_GetProductId(&sensorNode) == QLatin1String("Tuya_THD HY368 TRV") ||
                R_GetProductId(&sensorNode) == QLatin1String("Tuya_THD SEA801-ZIGBEE TRV") ||
                R_GetProductId(&sensorNode) == QLatin1String("Tuya_THD NX-4911-675 TRV") ||
                R_GetProductId(&sensorNode) == QLatin1String("Tuya_THD WZB-TRVL TRV") ||
                R_GetProductId(&sensorNode) == QLatin1String("Tuya_THD Smart radiator TRV") ||
                R_GetProductId(&sensorNode) == QLatin1String("Tuya_THD BTH-002 Thermostat"))
            {
                sensorNode.addItem(DataTypeString, RConfigSchedule);
            }

            if (R_GetProductId(&sensorNode) == QLatin1String("Tuya_THD HY369 TRV") ||
                R_GetProductId(&sensorNode) == QLatin1String("Tuya_THD HY368 TRV") ||
                R_GetProductId(&sensorNode) == QLatin1String("Tuya_THD Essentials TRV") ||
                R_GetProductId(&sensorNode) == QLatin1String("Tuya_THD NX-4911-675 TRV") ||
                R_GetProductId(&sensorNode) == QLatin1String("Tuya_THD Smart radiator TRV") ||
                R_GetProductId(&sensorNode) == QLatin1String("Tuya_THD WZB-TRVL TRV") ||
                R_GetProductId(&sensorNode) == QLatin1String("Tuya_THD SEA801-ZIGBEE TRV"))
            {
                sensorNode.addItem(DataTypeBool, RConfigWindowOpen)->setValue(false);
            }

            if (modelId.startsWith(QLatin1String("SPZB"))) // Eurotronic Spirit
            {
                sensorNode.addItem(DataTypeUInt8, RStateValve);
                sensorNode.addItem(DataTypeUInt32, RConfigHostFlags); // hidden
                sensorNode.addItem(DataTypeBool, RConfigDisplayFlipped)->setValue(false);
                sensorNode.addItem(DataTypeBool, RConfigLocked)->setValue(false);
                sensorNode.addItem(DataTypeString, RConfigMode);
            }
            else if (sensorNode.modelId() == QLatin1String("Super TR"))   // ELKO
            {
                sensorNode.addItem(DataTypeString, RConfigTemperatureMeasurement);
                sensorNode.addItem(DataTypeInt16, RStateFloorTemperature);
                sensorNode.addItem(DataTypeBool, RStateHeating)->setValue(false);
                sensorNode.addItem(DataTypeBool, RConfigLocked)->setValue(false);
                sensorNode.addItem(DataTypeString, RConfigMode);
            }
            else if (modelId == QLatin1String("Thermostat")) // ecozy
            {
                sensorNode.addItem(DataTypeUInt8, RStateValve);
                sensorNode.addItem(DataTypeString, RConfigSchedule);
                sensorNode.addItem(DataTypeBool, RConfigScheduleOn)->setValue(false);
                sensorNode.addItem(DataTypeInt16, RConfigLastChangeAmount);
                sensorNode.addItem(DataTypeUInt8, RConfigLastChangeSource);
                sensorNode.addItem(DataTypeTime, RConfigLastChangeTime);
            }
            else if (modelId == QLatin1String("SORB")) // Stelpro Orleans Fan
            {
                sensorNode.addItem(DataTypeInt16, RConfigCoolSetpoint);
                sensorNode.addItem(DataTypeUInt8, RStateValve);
                sensorNode.addItem(DataTypeBool, RConfigLocked)->setValue(false);
                sensorNode.addItem(DataTypeString, RConfigMode);
            }
            else if (modelId.startsWith(QLatin1String("STZB402"))) // Stelpro baseboard thermostat
            {
                sensorNode.addItem(DataTypeUInt8, RStateValve);
                sensorNode.addItem(DataTypeBool, RConfigLocked)->setValue(false);
                sensorNode.addItem(DataTypeString, RConfigMode);
            }
            else if (modelId == QLatin1String("Zen-01"))
            {
                sensorNode.addItem(DataTypeInt16, RConfigCoolSetpoint);
                sensorNode.addItem(DataTypeString, RConfigMode);
                sensorNode.addItem(DataTypeString, RConfigFanMode);
            }
            else if (modelId == QLatin1String("3157100"))
            {
                sensorNode.addItem(DataTypeInt16, RConfigCoolSetpoint);
                sensorNode.addItem(DataTypeBool, RConfigLocked)->setValue(false);
                sensorNode.addItem(DataTypeString, RConfigMode);
                sensorNode.addItem(DataTypeString, RConfigFanMode);
            }
            else if ((modelId == QLatin1String("eTRV0100")) || // Danfoss Ally
                     (modelId == QLatin1String("TRV001")) )    // Hive TRV
            {
                sensorNode.addItem(DataTypeUInt8, RStateValve);
                sensorNode.addItem(DataTypeString, RStateWindowOpen);
                sensorNode.addItem(DataTypeBool, RStateMountingModeActive)->setValue(false);
                sensorNode.addItem(DataTypeString, RStateErrorCode);
                sensorNode.addItem(DataTypeBool, RConfigDisplayFlipped)->setValue(false);
                sensorNode.addItem(DataTypeBool, RConfigLocked)->setValue(false);
                sensorNode.addItem(DataTypeBool, RConfigMountingMode)->setValue(false);
                // Supported with Danfoss firmware version 1.08
                sensorNode.addItem(DataTypeBool, RConfigScheduleOn)->setValue(false);
                sensorNode.addItem(DataTypeString, RConfigSchedule);
                sensorNode.addItem(DataTypeInt16, RConfigExternalTemperatureSensor);
                sensorNode.addItem(DataTypeBool, RConfigExternalWindowOpen)->setValue(false);
            }
            else if (modelId == QLatin1String("AC201")) // OWON AC201 Thermostat
            {
                sensorNode.addItem(DataTypeInt16, RConfigCoolSetpoint);
                sensorNode.addItem(DataTypeString, RConfigMode);
                sensorNode.addItem(DataTypeString, RConfigFanMode);
                sensorNode.addItem(DataTypeString, RConfigSwingMode);
            }
            else if (modelId == QLatin1String("PR412C")) // OWON PCT502 Thermostat
            {
                sensorNode.addItem(DataTypeBool, RConfigLocked)->setValue(false);
            }
            else if (modelId == QLatin1String("TH1300ZB")) // sinope thermostat
            {
                sensorNode.addItem(DataTypeUInt8, RStateValve);
                sensorNode.addItem(DataTypeBool, RConfigLocked)->setValue(false);
                sensorNode.addItem(DataTypeString, RConfigMode);
            }
            else
            {
                if (!modelId.isEmpty())
                {
                    sensorNode.addItem(DataTypeBool, RConfigScheduleOn)->setValue(false);
                    sensorNode.addItem(DataTypeString, RConfigSchedule);
                }
            }
        }
    }
    else if (sensorNode.type().endsWith(QLatin1String("Battery")))
    {
        if (sensorNode.fingerPrint().hasInCluster(POWER_CONFIGURATION_CLUSTER_ID))
        {
            clusterId = POWER_CONFIGURATION_CLUSTER_ID;
        }
        if (sensorNode.manufacturer() == QLatin1String("_TYST11_xu1rkty3"))
        {
            clusterId = TUYA_CLUSTER_ID;
        }
        sensorNode.addItem(DataTypeUInt8, RStateBattery);
    }
    else if (sensorNode.type().endsWith(QLatin1String("Time")))
    {
        if (sensorNode.fingerPrint().hasInCluster(TIME_CLUSTER_ID))
        {
            clusterId = TIME_CLUSTER_ID;
        }
        item = sensorNode.addItem(DataTypeTime, RStateUtc);
        item = sensorNode.addItem(DataTypeTime, RStateLocaltime);
        item = sensorNode.addItem(DataTypeTime, RStateLastSet);
    }
    else if (sensorNode.type().endsWith(QLatin1String("AirQuality")))
    {
        if (sensorNode.fingerPrint().hasInCluster(DEVELCO_AIR_QUALITY_CLUSTER_ID))
        {
            clusterId = DEVELCO_AIR_QUALITY_CLUSTER_ID;
        }
        else if (sensorNode.fingerPrint().hasInCluster(BOSCH_AIR_QUALITY_CLUSTER_ID))
        {
            clusterId = BOSCH_AIR_QUALITY_CLUSTER_ID;

            // Init Poll control
            item = sensorNode.addItem(DataTypeUInt16, RConfigPending);
            item->setValue(item->toNumber() | R_PENDING_WRITE_POLL_CHECKIN_INTERVAL | R_PENDING_SET_LONG_POLL_INTERVAL);
        }

        if (modelId == QLatin1String("AQSZB-110")  // Develco air quality sensor
            || (node->nodeDescriptor().manufacturerCode() == VENDOR_BOSCH2 && modelId == QLatin1String("AIR")))  // Bosch air quality sensor
        {
            item = sensorNode.addItem(DataTypeString, RStateAirQuality);
            item = sensorNode.addItem(DataTypeUInt16, RStateAirQualityPpb);
        }
    }

    const lidlDevice *device = getLidlDevice(manufacturer);
    if (device != nullptr)
    {
        sensorNode.setManufacturer(QLatin1String(device->manufacturername));
        sensorNode.setModelId(QLatin1String(device->modelid));
    }
    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_DDEL)
    {
        sensorNode.setManufacturer("dresden elektronik");

        if (modelId == QLatin1String("Lighting Switch"))
        {
            sensorNode.setMode(Sensor::ModeTwoGroups); // inital
        }
        else if (modelId.startsWith(QLatin1String("FLS-NB")))
        {
            sensorNode.setManufacturer("nimbus group");
        }
    }
    else if ((node->nodeDescriptor().manufacturerCode() == VENDOR_OSRAM_STACK) || (node->nodeDescriptor().manufacturerCode() == VENDOR_OSRAM))
    {
        if (modelId.startsWith(QLatin1String("CO_")) ||   // Heiman CO sensor
            modelId.startsWith(QLatin1String("DOOR_")) || // Heiman door/window sensor
            modelId.startsWith(QLatin1String("PIR_")) ||  // Heiman motion sensor
            modelId.startsWith(QLatin1String("GAS_")) ||  // Heiman conbustable gas sensor
            modelId.startsWith(QLatin1String("TH-")) || // Heiman temperature/humidity sensor
            modelId.startsWith(QLatin1String("SMOK_")) || // Heiman fire sensor
            modelId.startsWith(QLatin1String("WATER_")) || // Heiman water sensor
            modelId.startsWith(QLatin1String("RC_V14")))   // Heiman remote
        {
            sensorNode.setManufacturer("Heiman");
        }
        else
        {
            sensorNode.setManufacturer("OSRAM");
        }
    }
    else if ((node->nodeDescriptor().manufacturerCode() == VENDOR_SINOPE))
    {
        sensorNode.setManufacturer("Sinope");
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
        if (modelId == QLatin1String("RWL022") || modelId.startsWith(QLatin1String("RDM00")))
        {
            sensorNode.setManufacturer(QLatin1String("Signify Netherlands B.V."));
        }
        else
        {
            sensorNode.setManufacturer(QLatin1String("Philips"));
        }

        if (modelId.startsWith(QLatin1String("RWL02"))) // Hue dimmer switch
        {
            if (modelId != QLatin1String("RWL022")) // new model with one endpoint
            {
                sensorNode.fingerPrint().endpoint = 2;
            }
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
        else if (modelId.startsWith(QLatin1String("SML00"))) // Hue motion sensor
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
            item = sensorNode.addItem(DataTypeUInt16, RConfigPending);
            item->setValue(0);
            item = sensorNode.addItem(DataTypeBool, RConfigUsertest);
            item->setValue(false);
        }
        else if (modelId.startsWith(QLatin1String("RDM00"))) // Hue wall switch module
        {
            item = sensorNode.addItem(DataTypeString, RConfigDeviceMode);
            item->setValue(RConfigDeviceModeValues[0]);
            item = sensorNode.addItem(DataTypeUInt16, RConfigPending);
            item->setValue(0);
        }
    }
    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_BEGA)
    {
        sensorNode.setManufacturer("BEGA Gantenbrink-Leuchten KG");
    }
    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_BOSCH || node->nodeDescriptor().manufacturerCode() == VENDOR_BOSCH2)
    {
        sensorNode.setManufacturer(QLatin1String("BOSCH"));
    }
    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_IKEA || modelId.startsWith(QLatin1String("TRADFRI")))
    {
        sensorNode.setManufacturer(QLatin1String("IKEA of Sweden"));

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
    else if (sensorNode.modelId() == QLatin1String("lumi.sensor_magnet.agl02") || // skip
             sensorNode.modelId() == QLatin1String("lumi.flood.agl02"))
    {
    }
    else if (modelId.startsWith(QLatin1String("lumi")))
    {
        sensorNode.setManufacturer("LUMI");
        if (!sensorNode.modelId().startsWith(QLatin1String("lumi.ctrl_")) &&
            !sensorNode.modelId().startsWith(QLatin1String("lumi.plug")) &&
            !sensorNode.modelId().startsWith(QLatin1String("lumi.relay.c")) &&
            sensorNode.modelId() != QLatin1String("lumi.curtain") &&
            sensorNode.modelId() != QLatin1String("lumi.sensor_natgas") &&
            sensorNode.modelId() != QLatin1String("lumi.switch.b1naus01") &&
            !sensorNode.type().endsWith(QLatin1String("Battery")))
        {
            sensorNode.addItem(DataTypeUInt8, RConfigBattery);
        }

        if (!sensorNode.item(RStateTemperature) &&
            sensorNode.modelId() != QLatin1String("lumi.sensor_switch") &&
            !sensorNode.modelId().contains(QLatin1String("weather")) &&
            !sensorNode.modelId().startsWith(QLatin1String("lumi.sensor_ht")) &&
            !sensorNode.modelId().endsWith(QLatin1String("86opcn01"))) // exclude Aqara Opple
        {
            sensorNode.addItem(DataTypeInt16, RConfigTemperature);
            //sensorNode.addItem(DataTypeInt16, RConfigOffset);
        }

        if (sensorNode.modelId().endsWith(QLatin1String("86opcn01")))
        {
            // Aqara Opple switches need to be configured to send proper button events
            // write basic cluster attribute 0x0009 value 1
            item = sensorNode.addItem(DataTypeUInt16, RConfigPending);
            item->setValue(item->toNumber() | R_PENDING_MODE);
        }

        if (sensorNode.modelId().startsWith(QLatin1String("lumi.vibration")))
        {
            ResourceItem *item = nullptr;
            if (sensorNode.type() == QLatin1String("ZHAVibration"))
            {
                item = sensorNode.addItem(DataTypeInt16, RStateOrientationX);
                item = sensorNode.addItem(DataTypeInt16, RStateOrientationY);
                item = sensorNode.addItem(DataTypeInt16, RStateOrientationZ);
                item = sensorNode.addItem(DataTypeUInt16, RStateTiltAngle);
                item = sensorNode.addItem(DataTypeUInt16, RStateVibrationStrength);
            }
            // low: 0x15, medium: 0x0B, high: 0x01
            item = sensorNode.addItem(DataTypeUInt8, RConfigSensitivity);
            item = sensorNode.addItem(DataTypeUInt8, RConfigSensitivityMax);
            item->setValue(0x15); // low
            item = sensorNode.addItem(DataTypeUInt16, RConfigPending);
        }
    }
    else if (modelId.startsWith(QLatin1String("Super TR")) ||
             modelId.startsWith(QLatin1String("ElkoDimmer")))
    {
        sensorNode.setManufacturer("ELKO");
    }
    else if ((modelId == QLatin1String("TY0202") || modelId == QLatin1String("TY0203") || modelId == QLatin1String("TS0211")) && node->nodeDescriptor().manufacturerCode() == VENDOR_HEIMAN)
    {
        sensorNode.setManufacturer(QLatin1String("SILVERCREST"));
    }
    else if ( //node->nodeDescriptor().manufacturerCode() == VENDOR_EMBER ||
             node->nodeDescriptor().manufacturerCode() == VENDOR_HEIMAN)
    {
        sensorNode.setManufacturer("Heiman");
    }
    else if (modelId.startsWith(QLatin1String("45127")))
    {
        sensorNode.setManufacturer("Namron AS");
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
    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_SAMJIN)
    {
        sensorNode.setManufacturer("Samjin");

        if (PC_GetPollControlEndpoint(node) > 0)
        {
            item = sensorNode.addItem(DataTypeUInt16, RConfigPending);
            item->setValue(item->toNumber() | R_PENDING_WRITE_POLL_CHECKIN_INTERVAL | R_PENDING_SET_LONG_POLL_INTERVAL);
        }
    }
    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_INNR)
    {
        sensorNode.setManufacturer("innr");
    }
    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_VISONIC)
    {
        sensorNode.setManufacturer("Visonic");
    }
    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_CENTRALITE ||
             node->nodeDescriptor().manufacturerCode() == VENDOR_C2DF)
    {
        sensorNode.setManufacturer("CentraLite");
    }
    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_NYCE)
    {
        sensorNode.setManufacturer("NYCE");
    }
    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_DEVELCO)
    {
        sensorNode.setManufacturer("Develco Products A/S");
    }
    else if (manufacturer.startsWith(QLatin1String("_TYZB01")))
    {
        sensorNode.setManufacturer("Tuya");
    }
    else if (sensorNode.manufacturer().startsWith(QLatin1String("TUYATEC")))
    {
        sensorNode.setManufacturer("Tuyatec");
    }
    else if (node->nodeDescriptor().manufacturerCode() == VENDOR_SERCOMM)
    {
        sensorNode.setManufacturer("Sercomm Corp.");
    }

    if (sensorNode.manufacturer().isEmpty())
    {
        return; // required
    }

    if (clusterId == IAS_ZONE_CLUSTER_ID)
    {
        if (modelId == QLatin1String("button") ||
            modelId.startsWith(QLatin1String("multi")) ||
            modelId == QLatin1String("water") ||
            R_GetProductId(&sensorNode) == QLatin1String("NAS-AB02B0 Siren") ||
            modelId == QLatin1String("Motion Sensor-A"))
        {
            // no support for some IAS flags
        }
        else if (modelId == QLatin1String("Keyfob-ZB3.0"))
        {
            sensorNode.addItem(DataTypeBool, RStateLowBattery)->setValue(false);
        }
        else
        {
            item = sensorNode.addItem(DataTypeBool, RStateLowBattery);
            item->setValue(false);
            if (modelId.startsWith(QLatin1String("SMSZB-1"))) // Develco smoke detector
            {
                item = sensorNode.addItem(DataTypeBool, RStateTest);
                item->setValue(false);
            }
            else
            {
                item = sensorNode.addItem(DataTypeBool, RStateTampered);
                item->setValue(false);
            }
        }
        item = sensorNode.addItem(DataTypeUInt16, RConfigPending);
        sensorNode.addItem(DataTypeUInt32, RConfigEnrolled)->setValue(IAS_STATE_INIT);
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
            else if (*ci == POWER_CONFIGURATION_CLUSTER_ID)
            {
                //This device make a Rejoin every time, you trigger it, it's the only moment where you can read attribute.
                if (sensorNode.modelId() == QLatin1String("Remote switch") ||
                    sensorNode.modelId() == QLatin1String("Shutters central remote switch") ||
                    sensorNode.modelId() == QLatin1String("Double gangs remote switch") )
                {
                    //Ask for battery but only every day max
                    //int diff = idleTotalCounter - sensorNode.lastRead(READ_BATTERY);
                    //if (diff > 24 * 3600)
                    {
                        sensorNode.setNextReadTime(READ_BATTERY, queryTime);
                        sensorNode.setLastRead(READ_BATTERY, idleTotalCounter);
                        sensorNode.enableRead(READ_BATTERY);
                        queryTime = queryTime.addSecs(1);
                    }
                }
            }
            else if (*ci == TIME_CLUSTER_ID)
            {
                if (sensorNode.modelId() == QLatin1String("Thermostat")) // eCozy
                {
                    DBG_Printf(DBG_INFO, "  >>> %s sensor %s: set READ_TIME from addSensorNode()\n", qPrintable(sensorNode.type()), qPrintable(sensorNode.name()));
                    sensorNode.setNextReadTime(READ_TIME, queryTime);
                    sensorNode.setLastRead(READ_TIME, idleTotalCounter);
                    sensorNode.enableRead(READ_TIME);
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
        indexRulesTriggers();
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

        if (modelId.startsWith(QLatin1String("lumi.")))
        {
            for (const auto &ind : fastProbeIndications)
            {
                if ((ind.clusterId() == BASIC_CLUSTER_ID || ind.clusterId() == XIAOMI_CLUSTER_ID) && ind.profileId() != ZDP_PROFILE_ID)
                {
                    deCONZ::ZclFrame zclFrame;

                    {
                        QDataStream stream(ind.asdu());
                        stream.setByteOrder(QDataStream::LittleEndian);
                        zclFrame.readFromStream(stream);
                    }

                    // replay Xiaomi special report
                    handleZclAttributeReportIndicationXiaomiSpecial(ind, zclFrame);
                }
            }
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
            case DOOR_LOCK_CLUSTER_ID:
            case SAMJIN_CLUSTER_ID:
            case TIME_CLUSTER_ID:
                break;

            case VENDOR_CLUSTER_ID:
            {
                // ubisys device management (UBISYS_DEVICE_SETUP_CLUSTER_ID)
                if (event.endpoint() == 0xE8 && existDevicesWithVendorCodeForMacPrefix(event.node()->address(), VENDOR_UBISYS))
                {
                    break;
                }
                // dresden elektronik spectral sensor
                else if (i->modelId() == QLatin1String("de_spect") && existDevicesWithVendorCodeForMacPrefix(event.node()->address(), VENDOR_DDEL))
                {
                    break;
                }
            }
                continue; // ignore

            case BOSCH_AIR_QUALITY_CLUSTER_ID:
                if (i->modelId() == QLatin1String("AIR") && event.node()->nodeDescriptor().manufacturerCode() == VENDOR_BOSCH2)
                {
                    break; // Bosch Air quality sensor
                }
                continue;

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
                if (existDevicesWithVendorCodeForMacPrefix(event.node()->address(), VENDOR_JENNIC))
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

                            if (i->mustRead(READ_BATTERY))
                            {
                                i->clearRead(READ_BATTERY);
                            }

                            if (ia->id() == 0x0021) // battery percentage remaining
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());
                                    pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().u8);
                                }

                                ResourceItem *item = i->item(RStateBattery);

                                if (item) {
                                    int bat = ia->numericValue().u8 / 2;

                                    if (i->modelId().startsWith(QLatin1String("TRADFRI")) || // IKEA
                                        i->modelId().startsWith(QLatin1String("FYRTUR")) || // IKEA
                                        i->modelId().startsWith(QLatin1String("KADRILJ")) || // IKEA
                                        i->modelId().startsWith(QLatin1String("SYMFONISK")) || // IKEA
                                        i->modelId().startsWith(QLatin1String("ICZB-")) || // iCasa keypads and remote
                                        i->modelId().startsWith(QLatin1String("ZGR904-S")) || // Envilar remote
                                        i->modelId().startsWith(QLatin1String("ED-1001")) || // EcoDim wireless switches
                                        i->modelId().startsWith(QLatin1String("ZGRC-KEY")) || //  Sunricher wireless CCT remote
                                        i->modelId().startsWith(QLatin1String("ZG2833K")) || // Sunricher remote controller
                                        i->modelId().startsWith(QLatin1String("SV01-")) || // Keen Home vent
                                        i->modelId().startsWith(QLatin1String("SV02-")) || // Keen Home vent
                                        i->modelId().startsWith(QLatin1String("45127")) || // Namron 1/2/4-ch remote controller
                                        i->modelId().startsWith(QLatin1String("RGBgenie ZB-5")) || // RGBgenie remote control
                                        i->modelId().startsWith(QLatin1String("VOC_Sensor")) || // LifeControl Enviroment sensor
                                        i->modelId().startsWith(QLatin1String("TY0203")) || // SilverCrest / lidl
                                        i->modelId().startsWith(QLatin1String("TY0202")) || // SilverCrest / lidl
                                        i->modelId().startsWith(QLatin1String("ZG2835"))) // SR-ZG2835 Zigbee Rotary Switch
                                    {
                                        bat = ia->numericValue().u8;
                                    }
                                    item->setValue(bat);
                                    i->updateStateTimestamp();
                                    i->setNeedSaveDatabase(true);
                                    queSaveDb(DB_SENSORS, DB_HUGE_SAVE_DELAY);
                                    Event e(RSensors, RStateBattery, i->id(), item);
                                    enqueueEvent(e);
                                    enqueueEvent(Event(RSensors, RStateLastUpdated, i->id()));
                                    updateSensorEtag(&*i);
                                    q_ptr->nodeUpdated(i->address().ext(), QLatin1String(item->descriptor().suffix), QString::number(bat));
                                    continue;
                                }

                                item = i->item(RConfigBattery);

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

                                    if (i->modelId().startsWith(QLatin1String("TRADFRI")) || // IKEA
                                        i->modelId().startsWith(QLatin1String("SYMFONISK")) || // IKEA
                                        i->modelId().startsWith(QLatin1String("ICZB-")) || // iCasa keypads and remote
                                        i->modelId().startsWith(QLatin1String("ZGR904-S")) || // Envilar remote
                                        i->modelId().startsWith(QLatin1String("ED-1001")) || // EcoDim wireless switches
                                        i->modelId().startsWith(QLatin1String("ZGRC-KEY")) || // Sunricher wireless CCT remote
                                        i->modelId().startsWith(QLatin1String("ZG2833K")) || // Sunricher remote controller
                                        i->modelId().startsWith(QLatin1String("SV01-")) || // Keen Home vent
                                        i->modelId().startsWith(QLatin1String("SV02-")) || // Keen Home vent
                                        i->modelId().startsWith(QLatin1String("45127")) || // Namron 1/2/4-ch remote controller
                                        i->modelId().startsWith(QLatin1String("RGBgenie ZB-5")) || // RGBgenie remote control
                                        i->modelId().startsWith(QLatin1String("VOC_Sensor")) || // LifeControl Enviroment sensor
                                        i->modelId().startsWith(QLatin1String("ZG2835"))) // SR-ZG2835 Zigbee Rotary Switch
                                    {
                                        bat = ia->numericValue().u8;
                                    }

                                    if (i->modelId() == QLatin1String("0x8020") || // Danfoss RT24V Display thermostat
                                        i->modelId() == QLatin1String("0x8021") || // Danfoss RT24V Display thermostat with floor sensor
                                        i->modelId() == QLatin1String("0x8030") || // Danfoss RTbattery Display thermostat
                                        i->modelId() == QLatin1String("0x8031") || // Danfoss RTbattery Display thermostat with infrared
                                        i->modelId() == QLatin1String("0x8034") || // Danfoss RTbattery Dial thermostat
                                        i->modelId() == QLatin1String("0x8035"))   // Danfoss RTbattery Dial thermostat with infrared
                                    {
                                        // The Danfoss Icon Zigbee module exposes each in-room thermostat in its controller
                                        // as an endpoint. Each endpoint has the battery measurement for the device it represents.
                                        // This check makes sure none of the other endpoints get their battery value overwritten.
                                        if (event.endpoint() != i->fingerPrint().endpoint)
                                        {
                                            continue;
                                        }
                                    }

                                    if (item->toNumber() != bat)
                                    {
                                        i->setNeedSaveDatabase(true);
                                        queSaveDb(DB_SENSORS, DB_HUGE_SAVE_DELAY);
                                    }
                                    item->setValue(bat);
                                    Event e(RSensors, RConfigBattery, i->id(), item);
                                    enqueueEvent(e);
                                    q_ptr->nodeUpdated(i->address().ext(), QLatin1String(item->descriptor().suffix), QString::number(bat));
                                }

                                updateSensorEtag(&*i);
                            }
                            else if (ia->id() == 0x0020) // battery voltage
                            {
                                if (i->modelId().startsWith(QLatin1String("tagv4")) ||   // SmartThings Arrival sensor
                                    i->modelId().startsWith(QLatin1String("motionv4")) ||// SmartThings motion sensor
                                    i->modelId().startsWith(QLatin1String("moisturev4")) ||// SmartThings water leak sensor
                                    i->modelId().startsWith(QLatin1String("multiv4")) ||// SmartThings multi sensor 2016
                                    i->modelId().startsWith(QLatin1String("3305-S")) ||  // SmartThings 2014 motion sensor
                                    i->modelId() == QLatin1String("Remote switch") ||    // Legrand switch
                                    i->modelId() == QLatin1String("Double gangs remote switch") ||    // Legrand switch double
                                    i->modelId() == QLatin1String("Shutters central remote switch") || // Legrand switch module
                                    i->modelId() == QLatin1String("Remote toggle switch") || // Legrand shutter switch
                                    i->modelId() == QLatin1String("Remote motion sensor") || // Legrand motion sensor
                                    i->modelId() == QLatin1String("lumi.sensor_magnet.agl02") || // Xiaomi Aqara T1 open/close sensor MCCGQ12LM
                                    i->modelId() == QLatin1String("lumi.flood.agl02") ||         // Xiaomi Aqara T1 water leak sensor SJCGQ12LM
                                    i->modelId() == QLatin1String("Zen-01") ||           // Zen thermostat
                                    i->modelId() == QLatin1String("Thermostat") ||       // eCozy thermostat
                                    i->modelId() == QLatin1String("Motion Sensor-A") ||  // Osram motion sensor
                                    i->modelId() == QLatin1String("Bell") ||             // Sage doorbell sensor
                                    i->modelId() == QLatin1String("ISW-ZPR1-WP13") ||    // Bosch motion sensor
                                    i->modelId() == QLatin1String("3AFE14010402000D") ||   // Konke motion sensor
                                    i->modelId() == QLatin1String("3AFE28010402000D") ||   // Konke motion sensor v2
                                    i->modelId().endsWith(QLatin1String("86opcn01")) ||    // Aqara Opple
                                    i->modelId().startsWith(QLatin1String("AQSZB-1")) ||   // Develco air quality sensor
                                    i->modelId().startsWith(QLatin1String("SMSZB-1")) ||   // Develco smoke sensor
                                    i->modelId().startsWith(QLatin1String("HESZB-1")) ||   // Develco heat sensor
                                    i->modelId().startsWith(QLatin1String("MOSZB-1")) ||   // Develco motion sensor
                                    i->modelId().startsWith(QLatin1String("WISZB-1")) ||   // Develco window sensor
                                    i->modelId().startsWith(QLatin1String("FLSZB-1")) ||   // Develco water leak sensor
                                    i->modelId().startsWith(QLatin1String("SIRZB-1")) ||   // Develco siren
                                    i->modelId().startsWith(QLatin1String("HMSZB-1")) ||   // Develco temp/hum sensor
                                    i->modelId().startsWith(QLatin1String("ZHMS101")) ||   // Wattle (Develco) magnetic sensor
                                    i->modelId().startsWith(QLatin1String("MotionSensor51AU")) || // Aurora (Develco) motion sensor
                                    i->modelId().startsWith(QLatin1String("RFDL-ZB-MS")) ||// Bosch motion sensor
                                    i->modelId().startsWith(QLatin1String("1116-S")) ||    // iris contact sensor v3
                                    i->modelId().startsWith(QLatin1String("1117-S")) ||    // iris motion sensor v3
                                    i->modelId().startsWith(QLatin1String("3326-L")) ||    // iris motion sensor v2
                                    i->modelId().startsWith(QLatin1String("3300")) ||      // Centralite contact sensor
                                    i->modelId().startsWith(QLatin1String("3320-L")) ||    // Centralite contact sensor
                                    i->modelId().startsWith(QLatin1String("3323")) ||      // Centralite contact sensor
                                    i->modelId().startsWith(QLatin1String("3315")) ||      // Centralite water sensor
                                    i->modelId().startsWith(QLatin1String("3157100")) ||      // Centralite pearl thermostat
                                    i->modelId().startsWith(QLatin1String("4655BC0")) ||      // Ecolink contact sensor
                                    i->modelId().startsWith(QLatin1String("lumi.sen_ill")) || // Xiaomi ZB3.0 light sensor
                                    i->modelId().startsWith(QLatin1String("SZ-DWS04"))   || // Sercomm open/close sensor
                                    i->modelId().startsWith(QLatin1String("SZ-WTD02N_CAR")) || // Sercomm water sensor
                                    i->modelId().startsWith(QLatin1String("GZ-PIR02"))   || // Sercomm motion sensor
                                    i->modelId().startsWith(QLatin1String("Tripper")) || // Quirky Tripper (Sercomm) open/close
                                    i->modelId().startsWith(QLatin1String("Lightify Switch Mini")) ||  // Osram 3 button remote
                                    i->modelId().startsWith(QLatin1String("Switch 4x EU-LIGHTIFY")) || // Osram 4 button remote
                                    i->modelId().startsWith(QLatin1String("Switch 4x-LIGHTIFY")) || // Osram 4 button remote
                                    i->modelId().startsWith(QLatin1String("Switch-LIGHTIFY")) ) // Osram 4 button remote
                                {  }
                                else
                                {
                                    continue;
                                }

                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());
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
                                    float vmin = 20; // TODO: check - I've seen 24
                                    float vmax = 30; // TODO: check - I've seen 29
                                    float bat = battery;

                                    if (i->modelId() == QLatin1String("Zen-01"))
                                    {
                                        // 4x LR6 AA 1.5 V
                                        vmin = 36; // according to attribute 0x0036
                                        vmax = 60;
                                    }

                                    if      (bat > vmax) { bat = vmax; }
                                    else if (bat < vmin) { bat = vmin; }

                                    bat = ((bat - vmin) / (vmax - vmin)) * 100;

                                    if      (bat > 100) { bat = 100; }
                                    else if (bat <= 0)  { bat = 1; } // ?

                                    if (item->toNumber() != static_cast<quint8>(bat))
                                    {
                                        i->setNeedSaveDatabase(true);
                                        queSaveDb(DB_SENSORS, DB_HUGE_SAVE_DELAY);
                                    }
                                    item->setValue(static_cast<quint8>(bat));
                                    Event e(RSensors, RConfigBattery, i->id(), item);
                                    enqueueEvent(e);
                                    q_ptr->nodeUpdated(i->address().ext(), QLatin1String(item->descriptor().suffix), QString::number(bat));
                                }
                                updateSensorEtag(&*i);
                            }
                            else if (ia->id() == 0x0035) // battery alarm mask
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());
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
                    else if (event.clusterId() == BOSCH_AIR_QUALITY_CLUSTER_ID)
                    {
                        for (;ia != enda; ++ia)
                        {
                            if (std::find(event.attributeIds().begin(),
                                          event.attributeIds().end(),
                                          ia->id()) == event.attributeIds().end())
                            {
                                continue;
                            }

                            if (ia->id() == 0x4001) // air pressure (mBar)
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());
                                    pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().u16);
                                }

                                const auto pressure = ia->numericValue().u16;
                                ResourceItem *item = i->item(RStatePressure);

                                if (item)
                                {
                                    item->setValue(pressure);
                                    i->updateStateTimestamp();
                                    i->setNeedSaveDatabase(true);
                                    enqueueEvent(Event(RSensors, item->descriptor().suffix, i->id(), item));
                                    enqueueEvent(Event(RSensors, RStateLastUpdated, i->id()));
                                }

                                updateSensorEtag(&*i);
                            }
                            else if (ia->id() == 0x4003) // humidity (%)
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());
                                    pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().u16);
                                }

                                ResourceItem *item = i->item(RStateHumidity);

                                if (item)
                                {
                                    item->setValue(static_cast<quint16>(ia->numericValue().u8) * 100);
                                    i->updateStateTimestamp();
                                    i->setNeedSaveDatabase(true);
                                    enqueueEvent(Event(RSensors, item->descriptor().suffix, i->id(), item));
                                    enqueueEvent(Event(RSensors, RStateLastUpdated, i->id()));
                                }

                                updateSensorEtag(&*i);
                            }
                            else if (ia->id() == 0x4005) // measured illuminance (lux)
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());
                                    pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().u16);
                                }

                                updateSensorLightLevel(*i, ia->numericValue().u16); // ZigBee uses a 16-bit measured value
                                updateSensorEtag(&*i);
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
                                    i->setZclValue(updateType, event.endpoint(), event.clusterId(), 0x0000, ia->numericValue());
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
                                    i->setZclValue(updateType, event.endpoint(), event.clusterId(), 0x0000, ia->numericValue());
                                    pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().s16);
                                }

                                int temp = ia->numericValue().s16;
                                ResourceItem *item = i->item(RStateTemperature);

                                if (item)
                                {
                                    if (temp == -10000 && i->modelId().startsWith(QLatin1String("lumi.weather")))
                                    {
                                        continue;
                                    }
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
                            else if (i->modelId().startsWith(QLatin1String("VOC_Sensor")) && ia->id() == 0x0001) // LifeControl MCLH-08 relative humidity
                            {
                                // humidity sensor values are transferred via temperature cluster 0x0001 attribute
                                // see: https://github.com/dresden-elektronik/deconz-rest-plugin/pull/1964

                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.endpoint(), event.clusterId(), 0x0001, ia->numericValue());
                                    pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().u16);
                                }

                                int humidity = ia->numericValue().u16;
                                ResourceItem *item = i->item(RStateHumidity);

                                if (item)
                                {
                                    ResourceItem *item2 = i->item(RConfigOffset);
                                    if (item2 && item2->toNumber() != 0)
                                    {
                                        int _humidity = humidity + static_cast<int>(item2->toNumber());
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
                    else if (event.clusterId() == RELATIVE_HUMIDITY_CLUSTER_ID)
                    {
                        if (i->modelId() == QLatin1String("AIR") && event.node()->nodeDescriptor().manufacturerCode() == VENDOR_BOSCH2)
                        {
                            continue; // Bosch Air quality sensor has invalid value here, it will be taken from BOSCH_AIR_QUALITY_CLUSTER_ID
                        }

                        for (;ia != enda; ++ia)
                        {
                            if (ia->id() == 0x0000) // relative humidity
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.endpoint(), event.clusterId(), 0x0000, ia->numericValue());
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
                                    i->setZclValue(updateType, event.endpoint(), event.clusterId(), 0x0000, ia->numericValue());
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
                                    i->setZclValue(updateType, event.endpoint(), event.clusterId(), 0x0000, ia->numericValue());
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
                                    i->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());
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
                                    i->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());
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

                                if (i->mustRead(WRITE_DELAY))
                                {
                                    ResourceItem *item = i->item(RConfigPending);
                                    if (item)
                                    {
                                        quint16 mask = item->toNumber();
                                        mask &= ~R_PENDING_DELAY;
                                        item->setValue(mask);
                                        Event e(RSensors, RConfigPending, i->id(), item);
                                        enqueueEvent(e);
                                    }
                                    i->clearRead(WRITE_DELAY);
                                }

                                updateSensorEtag(&*i);
                            }
                            else if (ia->id() == 0x0030) // sensitivity
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());
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

                                if (i->mustRead(WRITE_SENSITIVITY))
                                {
                                    ResourceItem *item = i->item(RConfigPending);
                                    if (item)
                                    {
                                        quint16 mask = item->toNumber();
                                        mask &= ~R_PENDING_SENSITIVITY;
                                        item->setValue(mask);
                                        Event e(RSensors, RConfigPending, i->id(), item);
                                        enqueueEvent(e);
                                    }
                                    i->clearRead(WRITE_SENSITIVITY);
                                }

                                updateSensorEtag(&*i);
                            }
                            else if (ia->id() == 0x0031) // sensitivitymax
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());
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
                            if (std::find(event.attributeIds().begin(),
                                          event.attributeIds().end(),
                                          ia->id()) == event.attributeIds().end())
                            {
                                continue;
                            }

                            if (ia->id() == 0x0000) // onoff
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());
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

                                if (item && // i->buttonMap(buttonMapData, buttonMapForModelId).empty() &&
                                    event.event() == deCONZ::NodeEvent::UpdatedClusterDataZclReport)
                                {
                                    quint32 button = 0;

                                    if (i->modelId().startsWith(QLatin1String("lumi.sensor_86sw")))
                                    {
                                        button = (S_BUTTON_1 * event.endpoint()) + S_BUTTON_ACTION_SHORT_RELEASED;
                                    }
                                    else if (i->modelId() == QLatin1String("lumi.sensor_switch"))
                                    { // handeled by button map
                                    }
                                    else if (i->modelId() == QLatin1String("lumi.sensor_switch.aq2"))
                                    { // handeled by button map
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
                    else if (event.clusterId() == SAMJIN_CLUSTER_ID)
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

                            if (ia->id() == 0x0010) // active
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());
                                    pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().u8);
                                }

                                ResourceItem *item = i->item(RStateVibration);
                                if (item)
                                {
                                    const bool vibration = ia->numericValue().u8 == 0x01;
                                    item->setValue(vibration);
                                    updated = true;

                                    if (item->lastSet() == item->lastChanged())
                                    {
                                        Event e(RSensors, item->descriptor().suffix, i->id(), item);
                                        enqueueEvent(e);
                                    }
                                }
                            }
                            else if (ia->id() == 0x0012) // accelerate x
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());
                                    pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().s16);
                                }

                                ResourceItem *item = i->item(RStateOrientationX);

                                if (item)
                                {
                                    item->setValue(ia->numericValue().s16);
                                    updated = true;

                                    if (item->lastSet() == item->lastChanged())
                                    {
                                        Event e(RSensors, item->descriptor().suffix, i->id(), item);
                                        enqueueEvent(e);
                                    }
                                }
                            }
                            else if (ia->id() == 0x0013) // accelerate y
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());
                                    pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().s16);
                                }

                                ResourceItem *item = i->item(RStateOrientationY);

                                if (item)
                                {
                                    item->setValue(ia->numericValue().s16);
                                    updated = true;

                                    if (item->lastSet() == item->lastChanged())
                                    {
                                        Event e(RSensors, item->descriptor().suffix, i->id(), item);
                                        enqueueEvent(e);
                                    }
                                }
                            }
                            else if (ia->id() == 0x0014) // accelerate z
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());
                                    pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().s16);
                                }

                                ResourceItem *item = i->item(RStateOrientationZ);

                                if (item)
                                {
                                    item->setValue(ia->numericValue().s16);
                                    updated = true;

                                    if (item->lastSet() == item->lastChanged())
                                    {
                                        Event e(RSensors, item->descriptor().suffix, i->id(), item);
                                        enqueueEvent(e);
                                    }
                                }
                            }
                        }

                        if (updated)
                        {
                            i->setNeedSaveDatabase(true);
                            i->updateStateTimestamp();
                            enqueueEvent(Event(RSensors, RStateLastUpdated, i->id()));
                        }
                    }
                    else if (event.clusterId() == BASIC_CLUSTER_ID)
                    {
                        DBG_Printf(DBG_INFO_L2, "Update Sensor 0x%016llX Basic Cluster\n", event.node()->address().ext());

                        for (;ia != enda; ++ia)
                        {
                            if (std::find(event.attributeIds().begin(),
                                          event.attributeIds().end(),
                                          ia->id()) == event.attributeIds().end())
                            {
                                continue;
                            }

                            if (ia->id() == 0x0005) // Model identifier
                            {
                                if (i->mustRead(READ_MODEL_ID))
                                {
                                    i->clearRead(READ_MODEL_ID);
                                }

                                QString str = ia->toString().simplified();

                                if (isLidlDevice(str, i->manufacturer()))
                                {
                                    // Ignore non-unique ModelIdentifier; modelid set from unqiue ManufacturerName.
                                    continue;
                                }

                                if (str == QLatin1String("895a2d80097f4ae2b2d40500d5e03dcc"))
                                {
                                    str = QLatin1String("SN10ZW motion sensor");
                                }
                                else if (str == QLatin1String("b5db59bfd81e4f1f95dc57fdbba17931"))
                                {
                                    str = QLatin1String("SF20 smoke sensor");
                                }
                                else if (str == QLatin1String("98293058552c49f38ad0748541ee96ba"))
                                {
                                    str = QLatin1String("SF21 smoke sensor");
                                }
                                else if (str == QLatin1String("898ca74409a740b28d5841661e72268d"))
                                {
                                    str = QLatin1String("ST30 Temperature Sensor");
                                }
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
                                const lidlDevice *device = getLidlDevice(str);

                                if (device != nullptr)
                                {
                                    QString str2 = QLatin1String(device->modelid);

                                    if (!str2.isEmpty() && str2 != i->modelId())
                                    {
                                        i->setModelId(str);
                                        i->setNeedSaveDatabase(true);
                                        checkInstaModelId(&*i);
                                        updateSensorEtag(&*i);
                                        pushSensorInfoToCore(&*i);
                                        queSaveDb(DB_SENSORS, DB_LONG_SAVE_DELAY);
                                    }
                                    str = QLatin1String(device->manufacturername);
                                }

                                if (str.startsWith(QLatin1String("TUYATEC")))
                                {
                                    str = QLatin1String("Tuyatec"); // normalize TUYATEC-xdqihhgb --> Tuyatec
                                }

                                if (i->modelId().startsWith(QLatin1String("TRADFRI")))
                                {
                                    str = QLatin1String("IKEA of Sweden"); // Fix: since some IKEA devices had a bug returning an invalid string
                                }

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
                                    i->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());
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

                                if (i->mustRead(WRITE_USERTEST))
                                {
                                    ResourceItem *item = i->item(RConfigPending);
                                    if (item)
                                    {
                                        quint16 mask = item->toNumber();
                                        mask &= ~R_PENDING_USERTEST;
                                        item->setValue(mask);
                                        Event e(RSensors, RConfigPending, i->id(), item);
                                        enqueueEvent(e);
                                    }
                                    i->clearRead(WRITE_USERTEST);
                                }

                                updateSensorEtag(&*i);
                            }
                            else if (ia->id() == 0x0033) // ledindication
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());
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

                                if (i->mustRead(WRITE_LEDINDICATION))
                                {
                                    ResourceItem *item = i->item(RConfigPending);
                                    if (item)
                                    {
                                        quint16 mask = item->toNumber();
                                        mask &= ~R_PENDING_LEDINDICATION;
                                        item->setValue(mask);
                                        Event e(RSensors, RConfigPending, i->id(), item);
                                        enqueueEvent(e);
                                    }
                                    i->clearRead(WRITE_LEDINDICATION);
                                }

                                updateSensorEtag(&*i);
                            }
                            else if (ia->id() == 0x0034) // devicemode
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());
                                }

                                QString devicemode = RConfigDeviceModeValues[ia->numericValue().u8];
                                ResourceItem *item = i->item(RConfigDeviceMode);

                                if (item && item->toString() != devicemode)
                                {
                                    item->setValue(devicemode);
                                    i->setNeedSaveDatabase(true);
                                    Event e(RSensors, RConfigDeviceMode, i->id(), item);
                                    enqueueEvent(e);
                                }

                                if (i->mustRead(WRITE_DEVICEMODE))
                                {
                                    ResourceItem *item = i->item(RConfigPending);
                                    if (item)
                                    {
                                        quint16 mask = item->toNumber();
                                        mask &= ~R_PENDING_DEVICEMODE;
                                        item->setValue(mask);
                                        Event e(RSensors, RConfigPending, i->id(), item);
                                        enqueueEvent(e);
                                    }
                                    i->clearRead(WRITE_DEVICEMODE);
                                }

                                updateSensorEtag(&*i);
                            }
                            else if (ia->id() == 0xff0d && i->modelId().startsWith(QLatin1String("lumi.vibration"))) // sensitivity
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());
                                }

                                const quint8 sensitivity = ia->numericValue().u8;
                                ResourceItem *item = i->item(RConfigSensitivity);

                                if (item && item->toNumber() != sensitivity)
                                {
                                    if (!item->lastSet().isValid())
                                    {
                                        item->setValue(sensitivity);
                                    }
                                    else
                                    {
                                        // values differs
                                        quint16 pending = R_PENDING_SENSITIVITY;
                                        ResourceItem *item2 = i->item(RConfigPending);
                                        DBG_Assert(item2);
                                        if (item2)
                                        {
                                            if (item2->lastSet().isValid())
                                            {
                                                pending |= item2->toNumber();
                                            }

                                            item2->setValue(pending);
                                        }
                                    }

                                    i->setNeedSaveDatabase(true);
                                    Event e(RSensors, RConfigSensitivity, i->id(), item);
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
                                    i->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());
                                }

                                if (i->modelId().startsWith(QLatin1String("lumi.sensor_cube")))
                                {
                                    const qint32 buttonevent = static_cast<qint32>(ia->numericValue().real * 100);
                                    ResourceItem *item = i->item(RStateButtonEvent);
                                    ResourceItem *item2 = i->item(RStateGesture);

                                    DBG_Assert(item && item2);
                                    if (item && item2)
                                    {
                                        item->setValue(buttonevent);
                                        item2->setValue(buttonevent > 0 ? GESTURE_ROTATE_CLOCKWISE : GESTURE_ROTATE_COUNTER_CLOCKWISE);
                                        i->updateStateTimestamp();
                                        i->setNeedSaveDatabase(true);
                                        enqueueEvent(Event(RSensors, RStateButtonEvent, i->id(), item));
                                        enqueueEvent(Event(RSensors, RStateGesture, i->id(), item2));
                                        enqueueEvent(Event(RSensors, RStateLastUpdated, i->id()));
                                        updateSensorEtag(&*i);
                                    }
                                }
                                else if ((i->modelId() == QLatin1String("lumi.plug.mmeu01") && event.endpoint() == 21) ||
                                         (i->modelId() == QLatin1String("lumi.plug") && event.endpoint() == 2) ||
                                         (i->modelId().startsWith(QLatin1String("lumi.ctrl_")) && event.endpoint() == 2) ||
                                          i->modelId().startsWith(QLatin1String("lumi.relay.c")))
                                {
                                    if (i->type() == QLatin1String("ZHAPower"))
                                    {
                                        qint16 power = static_cast<qint16>(round(ia->numericValue().real));
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
                                }
                                else if ((i->modelId() == QLatin1String("lumi.plug.mmeu01") && event.endpoint() == 22) ||
                                         ((i->modelId() == QLatin1String("lumi.plug") && event.endpoint() == 3)) ||
                                         (i->modelId().startsWith(QLatin1String("lumi.ctrl_")) && event.endpoint() == 3))
                                {
                                    if (i->type() == QLatin1String("ZHAConsumption"))
                                    {
                                        quint64 consumption = round(ia->numericValue().real) * 1000;
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
                            if (ia->id() == MULTI_STATE_INPUT_PRESENT_VALUE_ATTRIBUTE_ID) // present value
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());
                                    pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().u16);
                                }

                                qint32 buttonevent = -1;
                                qint32 gesture = -1; //
                                ResourceItem *item = i->item(RStateButtonEvent);
                                int rawValue = ia->numericValue().u16;

                                DBG_Printf(DBG_INFO, "Multi state present value: 0x%04X (%u), %s\n", rawValue, rawValue, qPrintable(i->modelId()));

                                if (i->modelId().startsWith(QLatin1String("lumi.sensor_cube")))
                                {
                                    // Map Xiaomi Mi smart cube raw values to buttonevent values
                                    static const int sideMap[] = {1, 3, 5, 6, 4, 2};
                                    int side = sideMap[rawValue & 0x0007];
                                    int previousSide = sideMap[(rawValue & 0x0038) >> 3];
                                         if (rawValue == 0x0002) { buttonevent = 7000; gesture = GESTURE_NONE; }  // wakeup
                                    else if (rawValue == 0x0000) { buttonevent = 7007; gesture = GESTURE_SHAKE; } // shake
                                    else if (rawValue == 0x0003) { buttonevent = 7008; gesture = GESTURE_DROP; }  // drop
                                    else if (rawValue & 0x0040)  { buttonevent = side * 1000 + previousSide;      // flip 90°
                                                                   gesture = GESTURE_FLIP_90; }
                                    else if (rawValue & 0x0080)  { buttonevent = side * 1000 + 7 - side;          // flip 180°
                                                                   gesture = GESTURE_FLIP_180; }
                                    else if (rawValue & 0x0100)  { buttonevent = side * 1000;                     // push
                                                                   gesture = GESTURE_PUSH; }
                                    else if (rawValue & 0x0200)  { buttonevent = side * 1000 + side;              // double tap
                                                                   gesture = GESTURE_DOUBLE_TAP; }
                                }
                                else if (i->modelId() == QLatin1String("lumi.remote.b186acn02") ||
                                         i->modelId() == QLatin1String("lumi.remote.b286acn01") ||
                                         i->modelId() == QLatin1String("lumi.remote.b286acn02"))
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
                                else if (i->modelId() == QLatin1String("lumi.ctrl_ln1.aq1"))
                                {
                                    // handeled by button map
                                }
                                else if (i->modelId() == QLatin1String("lumi.ctrl_ln2.aq1"))
                                {
                                    // handeled by button map
                                }
                                else if (i->modelId().startsWith(QLatin1String("lumi.ctrl_ln")))
                                {
                                    // TODO The following can likely be removed sine lumi.ctrl_ln1.aq1 and lumi.ctrl_ln2.aq1 are using button maps now.
                                    //      are there any other lumi.ctrl_ln* devices?
                                    //      The lumi.ctrl_ln1.aq2, lumi.ctrl_ln2.aq2 seem to be a typo, the Internet only knows .aq1 versions
                                    //      and versions without .aq1? https://github.com/Koenkk/zigbee-herdsman-converters/blob/master/devices.js#L716.
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
                                    DBG_Printf(DBG_INFO, "[INFO] - Button %u %s\n", item->toNumber(), qPrintable(i->modelId()));
                                    i->updateStateTimestamp();
                                    i->setNeedSaveDatabase(true);
                                    Event e(RSensors, RStateButtonEvent, i->id(), item);
                                    enqueueEvent(e);
                                }

                                item = (gesture != -1) ? i->item(RStateGesture) : nullptr;
                                if (item && gesture != -1)
                                {
                                    item->setValue(gesture);
                                    i->updateStateTimestamp();
                                    i->setNeedSaveDatabase(true);
                                    Event e(RSensors, RStateGesture, i->id(), item);
                                    enqueueEvent(e);
                                }

                                if (gesture != -1 || buttonevent != -1) // something was updated
                                {
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
                                    i->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());
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
                                    i->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());
                                    pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().u64);
                                }

                                quint64 consumption = ia->numericValue().u64;
                                ResourceItem *item = i->item(RStateConsumption);

                                if (i->modelId() == QLatin1String("SmartPlug") ||        // Heiman
                                    i->modelId().startsWith(QLatin1String("PSMP5_")) ||  // Climax
                                    i->modelId().startsWith(QLatin1String("SKHMP30")) || // GS smart plug
                                    i->modelId().startsWith(QLatin1String("E13-")) ||    // Sengled PAR38 Bulbs
                                    i->modelId() == QLatin1String("Connected socket outlet")) // Niko smart socket
                                {
                                    //consumption += 5; consumption /= 10; // 0.1 Wh -> Wh
                                    consumption = static_cast<quint64>(round((double)consumption / 10.0)); // 0.1 Wh -> Wh
                                }
                                else if (i->modelId() == QLatin1String("SP 120") ||            // innr
                                         i->modelId() == QLatin1String("Plug-230V-ZB3.0") ||   // Immax
                                         i->modelId() == QLatin1String("TS0121"))              // Tuya / Blitzwolf
                                {
                                    consumption *= 10; // 0.01 kWh = 10 Wh -> Wh
                                }
                                else if (i->modelId().startsWith(QLatin1String("SZ-ESW01"))) // Sercomm / Telstra smart plug
                                {
                                    //consumption /= 1000;
                                    consumption = static_cast<quint64>(round((double)consumption / 1000.0)); // -> Wh
                                }
                                else if (i->modelId().startsWith(QLatin1String("ROB_200")) ||            // ROBB Smarrt micro dimmer
                                         i->modelId().startsWith(QLatin1String("Micro Smart Dimmer")) || // Sunricher Micro Smart Dimmer
                                         i->modelId().startsWith(QLatin1String("SPW35Z")))               // RT-RK OBLO SPW35ZD0 smart plug
                                {
                                    //consumption /= 3600;
                                    consumption = static_cast<quint64>(round((double)consumption / 3600.0)); // -> Wh
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
                                    i->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());
                                    pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().s32);
                                }

                                qint32 power = ia->numericValue().s32;
                                ResourceItem *item = i->item(RStatePower);

                                if (i->modelId() == QLatin1String("SmartPlug") ||       // Heiman
                                    i->modelId() == QLatin1String("902010/25") ||       // Bitron
                                    i->modelId().startsWith(QLatin1String("PSMP5_")) || // Climax
                                    i->modelId().startsWith(QLatin1String("SKHMP30")) ||// GS smart plug
                                    i->modelId().startsWith(QLatin1String("160-01")))   // Plugwise smart plug
                                {
                                    //power += 5; power /= 10; // 0.1 W -> W
                                    power = static_cast<qint32>(round((double)power / 10.0)); // 0.1W -> W
                                }
                                else if (i->modelId().startsWith(QLatin1String("SZ-ESW01"))) // Sercomm / Telstra smart plug
                                {
                                    //power /= 1000;
                                    power = static_cast<qint32>(round((double)power / 1000.0)); // -> W
                                }

                                if (item)
                                {
                                    item->setValue(power); // in W
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
                                    i->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());
                                    pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().s16);
                                }

                                qint16 power = ia->numericValue().s16;
                                ResourceItem *item = i->item(RStatePower);

                                if (item && power != -32768)
                                {
                                    if (i->modelId() == QLatin1String("SmartPlug") ||        // Heiman
                                        i->modelId().startsWith(QLatin1String("SKHMP30")) || // GS smart plug
                                        i->modelId().startsWith(QLatin1String("ROB_200")) || // ROBB Smarrt micro dimmer
                                        i->modelId().startsWith(QLatin1String("Micro Smart Dimmer")) || // Sunricher Micro Smart Dimmer
                                        i->modelId().startsWith(QLatin1String("lumi.plug.maeu")) || // Xiaomi Aqara ZB3.0 smart plug
                                        i->modelId() == QLatin1String("RICI01") ||           // LifeControl Smart Plug
                                        i->modelId().startsWith(QLatin1String("outlet")) ||  // Samsung SmartThings IM6001-OTP/IM6001-OTP01
                                        i->modelId() == QLatin1String("3200-Sgb") ||           // Samsung/Centralite smart outlet
                                        i->modelId() == QLatin1String("3200-de") ||            // Samsung/Centralite smart outlet
                                        i->modelId().startsWith(QLatin1String("lumi.switch.b1naus01"))) // Xiaomi ZB3.0 Smart Wall Switch
                                    {
                                        //power += 5; power /= 10; // 0.1W -> W
                                        power = static_cast<qint16>(round((double)power / 10.0)); // 0.1W -> W
                                    }
                                    else if (i->modelId().startsWith(QLatin1String("Plug")) && i->manufacturer() == QLatin1String("OSRAM")) // OSRAM
                                    {
                                        power = power == 28000 ? 0 : power / 10;
                                    }
                                    else if (i->modelId().startsWith(QLatin1String("SZ-ESW01"))) // Sercomm / Telstra smart plug
                                    {
                                        //power *= 128; power /= 1000;
                                        power = static_cast<qint16>(round(((double)power * 128) / 1000.0));
                                    }
                                    else if (i->modelId() == QLatin1String("Connected socket outlet")) // Niko smart socket
                                    {
                                        //power *= 1123; power /= 10000;
                                        power = static_cast<qint16>(round(((double)power * 1123) / 10000.0));
                                    }
                                    else if (i->modelId().startsWith(QLatin1String("lumi.relay.c2acn"))) // Xiaomi relay
                                    {
                                        continue;   // Device seems to always report -1 via this cluster/attribute
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
                                    i->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());
                                    pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().u16);
                                }

                                quint16 voltage = ia->numericValue().u16;
                                ResourceItem *item = i->item(RStateVoltage);

                                if (item && voltage != 65535)
                                {
                                    if (i->modelId() == QLatin1String("SmartPlug") ||         // Heiman
                                        i->modelId().startsWith(QLatin1String("SPLZB-1")) ||  // Develco smart plug
                                        i->modelId().startsWith(QLatin1String("SMRZB-3")) ||  // Develco smart relay
                                        i->modelId().startsWith(QLatin1String("SMRZB-1")) ||  // Develco smart cable
                                        i->modelId().startsWith(QLatin1String("SKHMP30")) ||  // GS smart plug
                                        i->modelId() == QLatin1String("PoP"))           // Apex Smart Plug
                                    {
                                        //voltage += 50; voltage /= 100; // 0.01V -> V
                                        voltage = static_cast<quint16>(round((double)voltage / 100.0)); // 0.01V -> V
                                    }
                                    else if (i->modelId() == QLatin1String("RICI01") ||           // LifeControl Smart Plug
                                             i->modelId().startsWith(QLatin1String("outlet")) ||  // Samsung SmartThings IM6001-OTP/IM6001-OTP01
                                             i->modelId().startsWith(QLatin1String("EMIZB-1")) || // Develco EMI
                                             i->modelId().startsWith(QLatin1String("ROB_200")) || // ROBB Smarrt micro dimmer
                                             i->modelId().startsWith(QLatin1String("Micro Smart Dimmer")) || // Sunricher Micro Smart Dimmer
                                             i->modelId() == QLatin1String("Connected socket outlet") || // Niko smart socket
                                             i->modelId().startsWith(QLatin1String("TH112"))) // Sinope Thermostats
                                    {
                                        //voltage /= 10; // 0.1V -> V
                                        voltage = static_cast<quint16>(round((double)voltage / 10.0)); // 0.1V -> V
                                    }
                                    else if (i->modelId().startsWith(QLatin1String("SZ-ESW01"))) // Sercomm / Telstra smart plug
                                    {
                                        //voltage /= 125; // -> V
                                        voltage = static_cast<quint16>(round((double)voltage / 125.0)); // -> V
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
                                    i->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());
                                    pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().u16);
                                }

                                quint16 current = ia->numericValue().u16;
                                ResourceItem *item = i->item(RStateCurrent);

                                if (item && current != 65535)
                                {
                                    if (i->modelId() == QLatin1String("SP 120") ||            // innr
                                        i->modelId().startsWith(QLatin1String("outlet")) ||   // Samsung SmartThings IM6001-OTP/IM6001-OTP01
                                        i->modelId() == QLatin1String("DoubleSocket50AU") ||  // Aurora
                                        i->modelId().startsWith(QLatin1String("SPLZB-1")) ||  // Develco smart plug
                                        i->modelId() == QLatin1String("RICI01") ||            // LifeControl Smart Plug
                                        i->modelId().startsWith(QLatin1String("SZ-ESW01")) || // Sercomm / Telstra smart plug
                                        i->modelId() == QLatin1String("TS0121") ||            // Tuya smart plug
                                        i->modelId().startsWith(QLatin1String("ROB_200")) ||  // ROBB Smarrt micro dimmer
                                        i->modelId().startsWith(QLatin1String("Micro Smart Dimmer")) || // Sunricher Micro Smart Dimmer
                                        i->modelId() == QLatin1String("Connected socket outlet") || // Niko smart socket
                                        i->modelId() == QLatin1String("SMRZB-1") || // Develco smart cable
                                        i->modelId().startsWith(QLatin1String("S1")) || // Ubisys S1/S1-R
                                        i->modelId().startsWith(QLatin1String("S2")) || // Ubisys S2/S2-R
                                        i->modelId().startsWith(QLatin1String("J1")) || // Ubisys J1/J1-R
                                        i->modelId().startsWith(QLatin1String("D1")))   // Ubisys D1/D1-R
                                    {
                                        // already in mA
                                    }
                                    else if (i->modelId() == QLatin1String("SmartPlug") ||        // Heiman
                                             i->modelId().startsWith(QLatin1String("EMIZB-1")) || // Develco EMI
                                             i->modelId().startsWith(QLatin1String("SKHMP30")) || // GS smart plug
                                             i->modelId() == QLatin1String("3200-Sgb") ||         // Samsung smart outlet
                                             i->modelId() == QLatin1String("3200-de") ||          // Samsung smart outlet
                                             i->modelId().startsWith(QLatin1String("SPW35Z")) ||  // RT-RK OBLO SPW35ZD0 smart plug
                                             i->modelId() == QLatin1String("TH1300ZB"))           // Sinope thermostat
                                    {
                                        current *= 10; // 0.01A -> mA
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
                            else if (ia->id() == 0x050f) // Apparent power
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());
                                    pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().u16);
                                }

                                quint16 power = ia->numericValue().u16;
                                ResourceItem *item = i->item(RStatePower);

                                if (item && power != 65535)
                                {
                                    if (i->modelId() == QLatin1String("TH1300ZB")) // Sinope thermostat
                                    {
                                        power = static_cast<quint16>(round((double)power / 1000.0)); // -> W
                                    }
                                    item->setValue(power); // in W
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
                    else if (event.clusterId() == DOOR_LOCK_CLUSTER_ID) {
                        bool updated = false;
                        for (;ia != enda; ++ia)
                        {
                            if (std::find(event.attributeIds().begin(),
                                          event.attributeIds().end(),
                                          ia->id()) == event.attributeIds().end())
                            {
                                continue;
                            }

                            if (i->modelId().startsWith(QLatin1String("lumi.vibration")) && i->type() == QLatin1String("ZHAVibration"))
                            {
                                if (ia->id() == 0x0055) // u16: event type
                                {
                                    if (updateType != NodeValue::UpdateInvalid)
                                    {
                                        i->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());
                                        pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().u16);
                                    }
                                    const quint16 value = ia->numericValue().u16;
                                    DBG_Printf(DBG_INFO, "0x%016llX: 0x0101/0x0055: event: %d\n", event.node()->address().ext(), value);

                                    if (value == 0x0001) // vibration
                                    {
                                        ResourceItem *item = i->item(RStateVibration);
                                        if (item)
                                        {
                                            item->setValue(true);
                                            enqueueEvent(Event(RSensors, RStateVibration, i->id(), item));
                                            i->durationDue = item->lastSet().addSecs(65);
                                            updated = true;
                                        }
                                    }
                                    else if (value == 0x002) // tilted
                                    {
                                        // tiltangle is set through 0x0503 attribute
                                    }
                                    else if (value == 0x003) // dropped
                                    {
                                        ResourceItem *item = i->item(RStateTiltAngle);
                                        if (item)
                                        {
                                            item->setValue(360);
                                            enqueueEvent(Event(RSensors, RStateTiltAngle, i->id(), item));
                                            updated = true;
                                        }
                                    }
                                }
                                else if (ia->id() == 0x0503) // u16: tilt angle
                                {
                                    if (updateType != NodeValue::UpdateInvalid)
                                    {
                                        i->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());
                                        pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().u16);
                                    }
                                    const quint16 value = ia->numericValue().u16;
                                    DBG_Printf(DBG_INFO, "0x%016llX: 0x0101/0x0503: tilt angle: %d°\n", event.node()->address().ext(), value);
                                    ResourceItem *item = i->item(RStateTiltAngle);
                                    if (item)
                                    {
                                        item->setValue(value);
                                        enqueueEvent(Event(RSensors, RStateTiltAngle, i->id(), item));
                                        updated = true;
                                    }
                                }
                                else if (ia->id() == 0x0505) // u32: vibration strength
                                {
                                    if (updateType != NodeValue::UpdateInvalid)
                                    {
                                        i->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());
                                        pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().u16);
                                    }

                                    const quint32 value = ia->numericValue().u32;
                                    const quint16 strength = (value >> 16) & 0xffff;
                                    DBG_Printf(DBG_INFO, "0x%016llX: 0x0101/0x0505: vibration strength: %d\n", event.node()->address().ext(), strength);
                                    ResourceItem *item = i->item(RStateVibrationStrength);
                                    if (item)
                                    {
                                        item->setValue(strength);
                                        enqueueEvent(Event(RSensors, RStateVibrationStrength, i->id(), item));
                                        updated = true;
                                    }
                                }
                                else if (ia->id() == 0x0508) // u48: orientation
                                {
                                    if (updateType != NodeValue::UpdateInvalid)
                                    {
                                        i->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());
                                        pushZclValueDb(event.node()->address().ext(), event.endpoint(), event.clusterId(), ia->id(), ia->numericValue().u16);
                                    }
                                    const quint64 value = ia->numericValue().u64;
                                    const qint16 x = value & 0xffff;
                                    const qint16 y = (value >> 16) & 0xffff;
                                    const qint16 z = (value >> 32) & 0xffff;
                                    DBG_Printf(DBG_INFO, "0x%016llX: 0x0101/0x0508: raw orientation: 0x%012llx (%d, %d, %d)\n", event.node()->address().ext(), value, x, y, z);
                                    const qreal X = 0.0 + x;
                                    const qreal Y = 0.0 + y;
                                    const qreal Z = 0.0 + z;
                                    const qint16 angleX = round(qAtan(X / qSqrt(Z * Z + Y * Y)) * 180 / M_PI);
                                    const qint16 angleY = round(qAtan(Y / qSqrt(X * X + Z * Z)) * 180 / M_PI);
                                    const qint16 angleZ = round(qAtan(Z / qSqrt(X * X + Y * Y)) * 180 / M_PI);
                                    DBG_Printf(DBG_INFO, "0x%016llX: 0x0101/0x0508: orientation: (%d°, %d°, %d°)\n", event.node()->address().ext(), angleX, angleY, angleZ);
                                    ResourceItem *item = i->item(RStateOrientationX);
                                    if (item)
                                    {
                                        item->setValue(angleX);
                                        enqueueEvent(Event(RSensors, RStateOrientationX, i->id(), item));
                                        updated = true;
                                    }
                                    item = i->item(RStateOrientationY);
                                    if (item)
                                    {
                                        item->setValue(angleY);
                                        enqueueEvent(Event(RSensors, RStateOrientationY, i->id(), item));
                                        updated = true;
                                    }
                                    item = i->item(RStateOrientationZ);
                                    if (item)
                                    {
                                        item->setValue(angleZ);
                                        enqueueEvent(Event(RSensors, RStateOrientationZ, i->id(), item));
                                        updated = true;
                                    }
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
                            existDevicesWithVendorCodeForMacPrefix(event.node()->address(), VENDOR_UBISYS)) // ubisys device management
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
                    else if (event.clusterId() == VENDOR_CLUSTER_ID && i->modelId() == QLatin1String("de_spect"))
                    {
                        bool updated = false;

                        if (event.endpoint() != i->fingerPrint().endpoint)
                        {
                            continue;
                        }

                        for (;ia != enda; ++ia)
                        {
                            if (std::find(event.attributeIds().begin(),
                                          event.attributeIds().end(),
                                          ia->id()) == event.attributeIds().end())
                            {
                                continue;
                            }

                            if (updateType != NodeValue::UpdateInvalid)
                            {
                                i->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());
                            }

                            if (ia->id() == 0x0000 && ia->dataType() == deCONZ::Zcl8BitUint) // sensor enabled / disabled
                            {
                                ResourceItem *item = i->item(RConfigOn);
                                if (item)
                                {
                                    bool enabled = ia->numericValue().u8 > 0 ? true : false;
                                    if (item->toBool() != enabled)
                                    {
                                        DBG_Printf(DBG_INFO, "0x%016llX: enabled state (%u) != config.on (%u)\n", event.node()->address().ext(), enabled, item->toBool());
                                        deCONZ::ZclAttribute attr = *ia;
                                        attr.setValue(static_cast<quint64>(enabled ? 0 : 1)); // toggle
                                        writeAttribute(&*i, i->fingerPrint().endpoint, VENDOR_CLUSTER_ID, attr);
                                        processTasks();
                                    }
                                }
                            }
                            else if (ia->id() >= 0x0001 && ia->id() <= 0x0003 && ia->dataType() == deCONZ::Zcl16BitUint) // Input actions
                            {
                                // 0x0001: X, 0x0002: Y, 0x0003: Z
                                const char *r[] = { RStateSpectralX, RStateSpectralY, RStateSpectralZ };

                                ResourceItem *item = i->item(r[ia->id() - 1]);
                                if (item)
                                {
                                    item->setValue(ia->numericValue().u16);
                                    enqueueEvent(Event(RSensors, r[ia->id() - 1], i->id(), item));
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
                    else if (event.clusterId() == TIME_CLUSTER_ID)
                    {
                        bool updated = false;
                        const QDateTime epoch = QDateTime(QDate(2000, 1, 1), QTime(0, 0), Qt::UTC);

                        for (;ia != enda; ++ia)
                        {
                            if (ia->id() == 0x0000) // Time (utc, in UTC)
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());
                                }
                                QDateTime time = epoch.addSecs(ia->numericValue().u32);
                                ResourceItem *item = i->item(RStateUtc);
                                if (item && item->toVariant().toDateTime().toMSecsSinceEpoch() != time.toMSecsSinceEpoch())
                                {
                                    item->setValue(time);
                                    enqueueEvent(Event(RSensors, RStateUtc, i->id(), item));
                                    updated = true;
                                }
                                const qint32 drift = QDateTime::currentDateTimeUtc().secsTo(time);
                                DBG_Printf(DBG_INFO, "  >>> %s sensor %s: drift %d\n", qPrintable(i->type()), qPrintable(i->name()), drift);

                                if (!i->mustRead(WRITE_TIME))
                                {
                                    if (drift < -10 || drift > 10)
                                    {
                                        DBG_Printf(DBG_INFO, "  >>> %s sensor %s: drift: %d: set WRITE_TIME\n", qPrintable(i->type()), qPrintable(i->name()), drift);
                                        i->setNextReadTime(WRITE_TIME, queryTime);
                                        i->setLastRead(WRITE_TIME, idleTotalCounter);
                                        i->enableRead(WRITE_TIME);
                                        queryTime = queryTime.addSecs(1);
                                    }
                                }
                            }
                            else if (ia->id() == 0x0007) // Local Time (u32, in local time)
                            {
                                if (updateType != NodeValue::UpdateInvalid)
                                {
                                    i->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());
                                }
                                QDateTime time = epoch.addSecs(ia->numericValue().u32 - QDateTime::currentDateTime().offsetFromUtc());
                                ResourceItem *item = i->item(RStateLocaltime);
                                if (item && item->toVariant().toDateTime().toMSecsSinceEpoch() != time.toMSecsSinceEpoch())
                                {
                                    item->setValue(time);
                                    enqueueEvent(Event(RSensors, RStateLocaltime, i->id(), item));
                                    updated = true;
                                }
                            }
                            else if (ia->id() == 0x0008) // Last set time (utc, in UTC)
                            {
                              if (updateType != NodeValue::UpdateInvalid)
                              {
                                  i->setZclValue(updateType, event.endpoint(), event.clusterId(), ia->id(), ia->numericValue());
                              }
                              QDateTime time = epoch.addSecs(ia->numericValue().u32);
                              ResourceItem *item = i->item(RStateLastSet);
                              if (item && item->toVariant().toDateTime().toMSecsSinceEpoch() != time.toMSecsSinceEpoch())
                              {
                                  item->setValue(time);
                                  enqueueEvent(Event(RSensors, RStateLastSet, i->id(), item));
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
            ((node->address().ext() & macPrefixMask) == s->mac) || existDevicesWithVendorCodeForMacPrefix(node->address(), s->vendorId))
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

/*! Returns the first Sensor for its given \p Address and \p Endpoint and \p Type or 0 if not found.
 */
Sensor *DeRestPluginPrivate::getSensorNodeForAddressAndEndpoint(const deCONZ::Address &addr, quint8 ep, const QString &type)
{
    for (Sensor &sensor: sensors)
    {
        if (sensor.deletedState() != Sensor::StateNormal || !sensor.node() ||
            sensor.fingerPrint().endpoint != ep || sensor.type() != type)
        {
            continue;
        }
        if (sensor.address().hasNwk() && addr.hasNwk() &&
            sensor.address().nwk() == addr.nwk())
        {
            return &sensor;
        }
        if (sensor.address().hasExt() && addr.hasExt() &&
            sensor.address().ext() == addr.ext())
        {
            return &sensor;
        }
    }
    return nullptr;
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
    if (uniqueId.isEmpty())
    {
        return nullptr;
    }

    for (Sensor &s : sensors)
    {
        if (s.deletedState() == Sensor::StateNormal && s.uniqueId() == uniqueId)
        {
            return &s;
        }
    }

    return nullptr;
}

/*! Returns a Sensor for its given \p id or 0 if not found.
 */
Sensor *DeRestPluginPrivate::getSensorNodeForId(const QString &id)
{
    for (Sensor &s : sensors)
    {
        if (s.deletedState() == Sensor::StateNormal && s.id() == id)
        {
            return &s;
        }
    }

    return nullptr;
}

/*! Returns a Group for a given group id or 0 if not found.
 */
Group *DeRestPluginPrivate::getGroupForId(uint16_t id)
{
    uint16_t gid = id ? id : gwGroup0;

    std::vector<Group>::iterator i = groups.begin();
    std::vector<Group>::iterator end = groups.end();

    for (; i != end; ++i)
    {
        if (i->address() == gid)
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
    if (gid == 0)
    {
        gid = gwGroup0;
    }

    std::vector<Group>::iterator i = groups.begin();
    std::vector<Group>::iterator end = groups.end();

    for (; i != end; ++i)
    {
        if (i->address() == gid)
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
        case DEV_ID_HA_ONOFF_LIGHT_SWITCH:
        case DEV_ID_HA_DIMMER_SWITCH:
        //case DEV_ID_ZLL_DIMMABLE_LIGHT: // same as DEV_ID_HA_ONOFF_LIGHT
        case DEV_ID_ZLL_DIMMABLE_PLUGIN_UNIT:
        case DEV_ID_Z30_DIMMABLE_PLUGIN_UNIT:
        case DEV_ID_LEVEL_CONTROL_SWITCH:
            //fall through

        case DEV_ID_MAINS_POWER_OUTLET:
        case DEV_ID_SMART_PLUG:
        case DEV_ID_HA_ONOFF_LIGHT:
        case DEV_ID_ZLL_ONOFF_LIGHT:
        case DEV_ID_ZLL_ONOFF_PLUGIN_UNIT:
        case DEV_ID_Z30_ONOFF_PLUGIN_UNIT:
        case DEV_ID_ZLL_ONOFF_SENSOR:
        case DEV_ID_HA_WINDOW_COVERING_DEVICE:
        case DEV_ID_HA_WINDOW_COVERING_CONTROLLER:
        // Danalock support. The device id (0x000a) needs to be defined and whitelisted
        case DEV_ID_DOOR_LOCK:
        case DEV_ID_FAN:
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

    if (!sensorNode->node())
    {
        return false;
    }

    const deCONZ::NodeDescriptor &nd = sensorNode->node()->nodeDescriptor();

    if (nd.isNull())
    {
        return false;
    }

    if (!nd.receiverOnWhenIdle() && (nd.manufacturerCode() == VENDOR_XIAOMI || sensorNode->modelId().startsWith(QLatin1String("lumi."))))
    {
        // don't talk to sleeping Xiaomi devices here
        return false;
    }

    if (sensorNode->node()->simpleDescriptors().isEmpty())
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

    // FIXME: Need check that end device is awake.

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
                // FIXME: The Write Attributes command will not reach deep sleepers.
                //        Unfortuneately, Occupied to Unoccupied Delay is not reportable.
                //        Fortunately, the Hue motion sensor is a light sleeper.
                ResourceItem *item = sensorNode->item(RConfigPending);
                quint16 mask = item->toNumber();
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
                // ResourceItem *item = sensorNode->item(RConfigPending);
                // quint16 mask = item->toNumber();
                // mask &= ~R_PENDING_LEDINDICATION;
                // item->setValue(mask);
                // sensorNode->clearRead(WRITE_LEDINDICATION);
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
                // ResourceItem *item = sensorNode->item(RConfigPending);
                // quint16 mask = item->toNumber();
                // mask &= ~R_PENDING_SENSITIVITY;
                // item->setValue(mask);
                // sensorNode->clearRead(WRITE_SENSITIVITY);
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
                // ResourceItem *item = sensorNode->item(RConfigPending);
                // quint16 mask = item->toNumber();
                // mask &= ~R_PENDING_USERTEST;
                // item->setValue(mask);
                // sensorNode->clearRead(WRITE_USERTEST);
                processed++;
            }
        }
        else
        {
            sensorNode->clearRead(WRITE_USERTEST);
        }
    }

    if (sensorNode->mustRead(WRITE_DEVICEMODE) && tNow > sensorNode->nextReadTime(WRITE_DEVICEMODE))
    {
        ResourceItem *item = sensorNode->item(RConfigDeviceMode);

        DBG_Printf(DBG_INFO_L2, "handle pending devicemode for 0x%016llX\n", sensorNode->address().ext());
        if (item)
        {
            quint64 devicemode = RConfigDeviceModeValues.indexOf(item->toString());
            // devicemode
            deCONZ::ZclAttribute attr(0x0034, deCONZ::Zcl8BitEnum, "devicemode", deCONZ::ZclReadWrite, true);
            attr.setValue(devicemode);

            DBG_Printf(DBG_INFO_L2, "write %lu to devicemode for 0x%016llX\n", devicemode, sensorNode->address().ext());
            if (writeAttribute(sensorNode, sensorNode->fingerPrint().endpoint, BASIC_CLUSTER_ID, attr, VENDOR_PHILIPS))
            {
                // ResourceItem *item = sensorNode->item(RConfigPending);
                // quint16 mask = item->toNumber();
                // mask &= ~R_PENDING_DEVICEMODE;
                // item->setValue(mask);
                // sensorNode->clearRead(WRITE_DEVICEMODE);
                processed++;
            }
        }
        else
        {
            sensorNode->clearRead(WRITE_DEVICEMODE);
        }
    }

    if (sensorNode->mustRead(READ_THERMOSTAT_STATE) && tNow > sensorNode->nextReadTime(READ_THERMOSTAT_STATE))
    {
        std::vector<uint16_t> attributes;

        if (sensorNode->modelId() == QLatin1String("Thermostat")) // eCozy
        {
            // attributes.push_back(0x0000); // Local Temperature - reported
            // attributes.push_back(0x0008); // PI Heating Demand - reported
            attributes.push_back(0x0010); // Local Temperature Calibration
            attributes.push_back(0x0012); // Occupied Heating Setpoint
            attributes.push_back(0x0023); // Temperature Setpoint Hold
            attributes.push_back(0x0030); // Setpoint Change Source
            attributes.push_back(0x0031); // Setpoint Change Amount
            attributes.push_back(0x0032); // Setpoint Change Timestamp
        }
        else
        {
            // TODO use poll manager, only poll when needed
            attributes.push_back(0x0000); // temperature
            attributes.push_back(0x0012); // heating setpoint
            attributes.push_back(0x0025); // scheduler state
            attributes.push_back(0x0029); // heating operation state
        }

        if (readAttributes(sensorNode, sensorNode->fingerPrint().endpoint, THERMOSTAT_CLUSTER_ID, attributes))
        {
            sensorNode->clearRead(READ_THERMOSTAT_STATE);
            processed++;
        }
    }

    if (sensorNode->mustRead(READ_THERMOSTAT_SCHEDULE) && tNow > sensorNode->nextReadTime(READ_THERMOSTAT_SCHEDULE))
    {
        TaskItem task;

        // set destination parameters
        task.req.dstAddress() = sensorNode->address();
        task.req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
        task.req.setDstEndpoint(sensorNode->fingerPrint().endpoint);
        task.req.setSrcEndpoint(getSrcEndpoint(sensorNode, task.req));
        task.req.setDstAddressMode(deCONZ::ApsExtAddress);

        if (addTaskThermostatGetSchedule(task))
        {
            sensorNode->clearRead(READ_THERMOSTAT_SCHEDULE);
            processed++;
        }
    }

    if (sensorNode->mustRead(READ_BATTERY) && tNow > sensorNode->nextReadTime(READ_BATTERY))
    {
        std::vector<uint16_t> attributes;
        attributes.push_back(0x0020); // battery level
        if (readAttributes(sensorNode, sensorNode->fingerPrint().endpoint, POWER_CONFIGURATION_CLUSTER_ID, attributes))
        {
            sensorNode->clearRead(READ_BATTERY);
            processed++;
        }
    }

    if (sensorNode->mustRead(READ_TIME) && tNow > sensorNode->nextReadTime(READ_TIME))
    {
        DBG_Printf(DBG_INFO, "  >>> %s sensor %s: exec READ_TIME\n", qPrintable(sensorNode->type()), qPrintable(sensorNode->name()));
        std::vector<uint16_t> attributes;
        attributes.push_back(0x0000); // Time
        attributes.push_back(0x0007); // Local Time
        attributes.push_back(0x0008); // Last Set
        if (readAttributes(sensorNode, sensorNode->fingerPrint().endpoint, TIME_CLUSTER_ID, attributes))
        {
            DBG_Printf(DBG_INFO, "  >>> %s sensor %s: clear READ_TIME\n", qPrintable(sensorNode->type()), qPrintable(sensorNode->name()));
            sensorNode->clearRead(READ_TIME);
            processed++;
        }
        else
        {
            DBG_Printf(DBG_INFO, "  >>> %s sensor %s: READ_TIME failed\n", qPrintable(sensorNode->type()), qPrintable(sensorNode->name()));
        }
    }

    if (sensorNode->mustRead(WRITE_TIME) && tNow > sensorNode->nextReadTime(WRITE_TIME))
    {
        DBG_Printf(DBG_INFO, "  >>> %s sensor %s: exec WRITE_TIME\n", qPrintable(sensorNode->type()), qPrintable(sensorNode->name()));
        deCONZ::ZclAttribute attr(0x0000, deCONZ::ZclUtcTime, "time", deCONZ::ZclReadWrite, true);
        attr.setValue(QDateTime::currentDateTimeUtc());

        if (addTaskSyncTime(sensorNode))
        {
            DBG_Printf(DBG_INFO, "  >>> %s sensor %s: clear WRITE_TIME\n", qPrintable(sensorNode->type()), qPrintable(sensorNode->name()));
            sensorNode->clearRead(WRITE_TIME);
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

    if (clusterId == TIME_CLUSTER_ID)
    {
        // FIXME: should check for light sleeper instead
    }
    else if (!restNode->node()->nodeDescriptor().receiverOnWhenIdle())
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
    DBG_Assert(restNode != nullptr);

    if (!restNode || !restNode->isAvailable())
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
        DBG_Printf(DBG_INFO_L2, "write manufacturer specific attribute of 0x%016llX ep: 0x%02X cluster: 0x%04X: 0x%04X\n", restNode->address().ext(), endpoint, clusterId, attribute.id());
    }
    else
    {
        task.zclFrame.setFrameControl(deCONZ::ZclFCProfileCommand |
                                      deCONZ::ZclFCDirectionClientToServer |
                                      deCONZ::ZclFCDisableDefaultResponse);

        DBG_Printf(DBG_INFO, "write attribute of 0x%016llX ep: 0x%02X cluster: 0x%04X: 0x%04X\n", restNode->address().ext(), endpoint, clusterId, attribute.id());
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
            DBG_Printf(DBG_INFO, "discard write attribute of 0x%016llX ep: 0x%02X cluster: 0x%04X: 0x%04X (already in queue)\n", restNode->address().ext(), endpoint, clusterId, attribute.id());
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

    bool found = false;
    for (const auto &cl : lightNode->haEndpoint().inClusters())
    {
        if (cl.id() == GROUP_CLUSTER_ID)
        {
            found = true;
            break;
        }
    }

    if (!found) // not all light endpoints have a group cluster (lumi.ctrl_ln2.aq1)
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
        scene.name = tr("Scene %u").arg(sceneId);
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

    if (!existDevicesWithVendorCodeForMacPrefix(lightNode->address(), VENDOR_XAL))
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
    Q_UNUSED(zclFrame)

    bool checkReporting = false;
    const quint64 macPrefix = ind.srcAddress().ext() & macPrefixMask;

    if (DBG_IsEnabled(DBG_INFO))
    {
        DBG_Printf(DBG_INFO, "ZCL attribute report 0x%016llX for cluster: 0x%04X, ep: 0x%02X, frame control: 0x%02X, mfcode: 0x%04X \n", ind.srcAddress().ext(), ind.clusterId(), ind.srcEndpoint(), zclFrame.frameControl(), zclFrame.manufacturerCode());
    }

    if (DBG_IsEnabled(DBG_INFO_L2))
    {
        DBG_Printf(DBG_INFO_L2, "\tpayload: %s\n", qPrintable(zclFrame.payload().toHex()));
    }

    if (!(zclFrame.frameControl() & deCONZ::ZclFCDisableDefaultResponse))
    {
        checkReporting = true;
        sendZclDefaultResponse(ind, zclFrame, deCONZ::ZclSuccessStatus);
    }
    else if (existDevicesWithVendorCodeForMacPrefix(ind.srcAddress(), VENDOR_PHILIPS) ||
            macPrefix == tiMacPrefix ||
            existDevicesWithVendorCodeForMacPrefix(ind.srcAddress(), VENDOR_DDEL) ||
            existDevicesWithVendorCodeForMacPrefix(ind.srcAddress(), VENDOR_IKEA) ||
            existDevicesWithVendorCodeForMacPrefix(ind.srcAddress(), VENDOR_OSRAM_STACK) ||
            existDevicesWithVendorCodeForMacPrefix(ind.srcAddress(), VENDOR_JENNIC) ||
            existDevicesWithVendorCodeForMacPrefix(ind.srcAddress(), VENDOR_SI_LABS) ||
            existDevicesWithVendorCodeForMacPrefix(ind.srcAddress(), VENDOR_CENTRALITE))
    {
        // these sensors tend to mac data poll after report
        checkReporting = true;
    }

    if (checkReporting)
    {
        for (Sensor &sensor : sensors)
        {
            if (sensor.deletedState() != Sensor::StateNormal || !sensor.node())
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
                ((sensor.lastAttributeReportBind() < (idleTotalCounter - BUTTON_ATTR_REPORT_BIND_LIMIT)) || sensor.lastAttributeReportBind() == 0))
            {
                if (checkSensorBindingsForAttributeReporting(&sensor))
                {
                    sensor.setLastAttributeReportBind(idleTotalCounter);
                }
            }

            checkPollControlClusterTask(&sensor);
        }
    }

    if (zclFrame.isProfileWideCommand() && existDevicesWithVendorCodeForMacPrefix(ind.srcAddress().ext(), VENDOR_XIAOMI) && (ind.clusterId() == BASIC_CLUSTER_ID || ind.clusterId() == XIAOMI_CLUSTER_ID))
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
    stream.setFloatingPointPrecision(QDataStream::SinglePrecision);

    while (attrId == 0)
    {
        if (stream.atEnd())
        {
            break;
        }

        quint16 a;
        stream >> a;
        stream >> dataType;

        if (dataType == deCONZ::ZclCharacterString || dataType == deCONZ::ZclOctedString)
        {
            stream >> length;
        }

        if (a == 0xff01 && dataType == deCONZ::ZclCharacterString)
        {
            attrId = a;
        }
        else if (a == 0xff02 && dataType == 0x4c /*deCONZ::ZclStruct*/)
        {
            attrId = a;
        }
        else if (a == 0x00f7 && dataType == deCONZ::ZclOctedString)
        {
            attrId = a;
        }

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

    if (stream.atEnd() || attrId == 0)
    {
        return;
    }

    quint8 structIndex = 0; // only attribute id 0xff02
    quint16 structSize = 0; // only attribute id 0xff02

    quint16 battery = 0;
    quint32 lightlevel = UINT32_MAX; // use 32-bit to mark invalid and support 0xffff value
    qint16 temperature = INT16_MIN;
    quint16 humidity = UINT16_MAX;
    qint16 pressure = INT16_MIN;
    quint8 onOff = UINT8_MAX;
    quint8 onOff2 = UINT8_MAX;
    quint8 lift = UINT8_MAX;
    quint32 power = UINT32_MAX;
    quint32 consumption = UINT32_MAX;
    quint32 current = UINT32_MAX;
    quint32 voltage = UINT32_MAX;

    DBG_Printf(DBG_INFO, "0x%016llX extract Xiaomi special attribute 0x%04X\n", ind.srcAddress().ext(), attrId);

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
        float f;

        quint8 tag = 0;

        if (attrId == 0xff01 || attrId == 0x00f7)
        {
            stream >> tag;
        }
        else if (attrId == 0xff02)
        {
            if (structIndex == 0)
            {
                stream >> structSize; // number of elements
            }
            structIndex++;
        }

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
        case deCONZ::ZclSingleFloat: stream >> f; break;
        default:
        {
            DBG_Printf(DBG_INFO, "\tUnsupported datatype 0x%02X (tag 0x%02X)\n", dataType, tag);
        }
            return;
        }

        if ((tag == 0x01 || structIndex == 0x02) && dataType == deCONZ::Zcl16BitUint)
        {
            DBG_Printf(DBG_INFO, "\t01 battery %u (0x%04X)\n", u16, u16);
            battery = u16;
        }
        else if (tag == 0x03 && dataType == deCONZ::Zcl8BitInt)
        {
            DBG_Printf(DBG_INFO, "\t03 Device temperature %d °C\n", int(s8)); // Device temperature for lumi.plug.mmeu01
            temperature = qint16(s8) * 100;
        }
        else if ((tag == 0x04 || structIndex == 0x03) && dataType == deCONZ::Zcl16BitUint)
        {
            DBG_Printf(DBG_INFO, "\t04 unknown %d (0x%04X)\n", u16, u16);
        }
        else if (tag == 0x05 && dataType == deCONZ::Zcl16BitUint)
        {
            DBG_Printf(DBG_INFO, "\t05 RSSI dB (?) %d (0x%04X)\n", u16, u16); // Power outages for lumi.plug.mmeu01
        }
        else if ((tag == 0x06 || structIndex == 0x04) && dataType == deCONZ::Zcl40BitUint)
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
            DBG_Printf(DBG_INFO, "\t0a Parent NWK %d (0x%04X)\n", u16, u16);
        }
        else if (tag == 0x0b && dataType == deCONZ::Zcl16BitUint)
        {
            DBG_Printf(DBG_INFO, "\t0b lightlevel %u (0x%04X)\n", u16, u16);
            lightlevel = u16;
        }
        else if (tag == 0x0b && dataType == deCONZ::Zcl8BitUint)
        {
            DBG_Printf(DBG_INFO, "\t0b unknown %d (0x%02X)\n", u8, u8);
        }
        else if ((tag == 0x64 || structIndex == 0x01) && dataType == deCONZ::ZclBoolean) // lumi.ctrl_ln2 endpoint 01
        {
            DBG_Printf(DBG_INFO, "\t64 on/off %d\n", u8);
            onOff = u8;
        }
        else if (tag == 0x64 && dataType == deCONZ::Zcl8BitUint) // lumi.curtain
        {
            if (u8 <= 100)
            {
                lift = 100 - u8;
            }
            DBG_Printf(DBG_INFO, "\t64 lift %d (%d%%)\n", u8, lift);
            DBG_Printf(DBG_INFO, "\t64 somke/gas density %d (0x%02X)\n", u8, u8);   // lumi.sensor_smoke/lumi.sensor_natgas
        }
        else if (tag == 0x64 && dataType == deCONZ::Zcl16BitInt)
        {
            if (int(s16) == -10000)
            {
                DBG_Printf(DBG_INFO, "\t64 temperature %d (ignored)\n", int(s16));
            }
            else
            {
                DBG_Printf(DBG_INFO, "\t64 temperature %d\n", int(s16));
                temperature = s16;
            }
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
        else if (tag == 0x65 && dataType == deCONZ::Zcl8BitUint)
        {
            DBG_Printf(DBG_INFO, "\t65 unknown %d (0x%02X)\n", u8, u8);
        }
        else if (tag == 0x66 && dataType == deCONZ::Zcl16BitUint)
        {
            DBG_Printf(DBG_INFO, "\t66 unknown %d (0x%04X)\n", u16, u16);
        }
        else if (tag == 0x66 && dataType == deCONZ::Zcl32BitInt) // lumi.weather
        {
            pressure = (s32 + 50) / 100;
            DBG_Printf(DBG_INFO, "\t66 pressure %d (%d)\n", s32, pressure);
        }
        else if (tag == 0x6e && dataType == deCONZ::Zcl8BitUint) // lumi.ctrl_neutral2
        {
            DBG_Printf(DBG_INFO, "\t6e unknown %d (0x%02X)\n", u8, u8);
        }
        else if (tag == 0x6f && dataType == deCONZ::Zcl8BitUint) // lumi.ctrl_neutral2
        {
            DBG_Printf(DBG_INFO, "\t6f unknown %d (0x%02X)\n", u8, u8);
        }
        else if (tag == 0x94 && dataType == deCONZ::Zcl8BitUint) // lumi.relay.c2acn01
        {
            DBG_Printf(DBG_INFO, "\t6f unknown %d (0x%02X)\n", u8, u8);
        }
        else if (tag == 0x95 && dataType == deCONZ::ZclSingleFloat) // lumi.ctrl_ln2
        {
            consumption = static_cast<qint32>(round(f * 1000)); // convert to Wh
            DBG_Printf(DBG_INFO, "\t95 consumption %f (%d)\n", f, consumption);
        }
        else if (tag == 0x96 && dataType == deCONZ::ZclSingleFloat) // lumi.plug.mmeu01
        {
            voltage = static_cast<qint32>(round(f / 10)); // convert to V
            DBG_Printf(DBG_INFO, "\t96 voltage %f (%d)\n", f, voltage);
        }
        else if (tag == 0x96 && dataType == deCONZ::Zcl32BitUint) // lumi.sensor_smoke
        {
            DBG_Printf(DBG_INFO, "\t96 unknown %d (0x%08X)\n", u32, u32);
        }
        else if (tag == 0x97 && dataType == deCONZ::Zcl16BitUint) // lumi.sensor_cube
        {
            DBG_Printf(DBG_INFO, "\t97 unknown %d (0x%04X)\n", u16, u16);
        }
        else if (tag == 0x97 && dataType == deCONZ::ZclSingleFloat) // lumi.plug.mmeu01
        {
            current = static_cast<qint32>(round(f));  // already in mA
            DBG_Printf(DBG_INFO, "\t97 current %f (%d)\n", f, current);
        }
        else if (tag == 0x98 && dataType == deCONZ::Zcl16BitUint) // lumi.sensor_cube
        {
            DBG_Printf(DBG_INFO, "\t98 unknown %d (0x%04X)\n", u16, u16);
        }
        else if (tag == 0x98 && dataType == deCONZ::ZclSingleFloat) // lumi.ctrl_ln2
        {
            power = static_cast<qint32>(round(f));  // already in W
            DBG_Printf(DBG_INFO, "\t98 power %f (%d)\n", f, power);
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
        else if (tag == 0x9b && dataType == deCONZ::ZclBoolean) // lumi.plug.mmeu01
        {
            DBG_Printf(DBG_INFO, "\t9b Consumer connected (yes/no) %d\n", u8);
        }
        else if (structIndex == 0x05 && dataType == deCONZ::Zcl16BitUint) // lumi.sensor_magnet
        {
            DBG_Printf(DBG_INFO, "\tStruct index 05 unknown (counter?) %d (0x%04X)\n", u16, u16);
        }
        else if (structIndex == 0x06 && dataType == deCONZ::Zcl8BitUint) // lumi.sensor_magnet
        {
            DBG_Printf(DBG_INFO, "\tStruct index 06 unknown (counter?) %d (0x%02X)\n", u8, u8);
        }
        else if (tag)
        {
            DBG_Printf(DBG_INFO, "\t%02X unsupported tag (data type 0x%02X)\n", tag, dataType);
        }
        else if (structIndex)
        {
            DBG_Printf(DBG_INFO, "\t%02X unsupported index (data type 0x%02X)\n", structIndex, dataType);
        }
    }

    RestNodeBase *restNodePending = nullptr;
    QString modelId;

    for (LightNode &lightNode: nodes)
    {
        if (!lightNode.modelId().startsWith(QLatin1String("lumi.")))
        {
            continue;
        }

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

        quint8 stateOnOff = UINT8_MAX;
        ResourceItem *item;

        if (lightNode.modelId().startsWith(QLatin1String("lumi.ctrl_neutral")))
        {
            if (lightNode.haEndpoint().endpoint() == 0x02 && onOff != UINT8_MAX)
            {
                stateOnOff = onOff;

            }
            else if (lightNode.haEndpoint().endpoint() == 0x03 && onOff2 != UINT8_MAX)
            {
                stateOnOff = onOff2;
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
                stateOnOff = onOff;
            }
            else if (lightNode.haEndpoint().endpoint() == 0x02 && onOff2 != UINT8_MAX)
            {
                stateOnOff = onOff2;
            }
            else
            {
                continue;
            }
        }
        else if (lightNode.modelId().startsWith(QLatin1String("lumi.curtain")) && lift != UINT8_MAX)
        {
            item = lightNode.item(RStateLift);
            if (item)
            {
                item->setValue(lift);
                enqueueEvent(Event(RLights, item->descriptor().suffix, lightNode.id(), item));
            }
            item = lightNode.item(RStateOpen);
            bool open = lift < 100;
            if (item)
            {
                item->setValue(open);
                enqueueEvent(Event(RLights, item->descriptor().suffix, lightNode.id(), item));
            }
            // FIXME: deprecate
            item = lightNode.item(RStateBri);
            if (item)
            {
                const uint bri = lift * 254 / 100;
                item->setValue(bri);
                enqueueEvent(Event(RLights, item->descriptor().suffix, lightNode.id(), item));
                stateOnOff = bri != 0;
            }
            // END FIXME: deprecate
        }
        else if (lightNode.modelId() == QLatin1String("lumi.plug.mmeu01"))
        {
            stateOnOff = onOff;
        }

        lightNode.rx();
        item = lightNode.item(RStateReachable);
        if (item && !item->toBool())
        {
            item->setValue(true);
            enqueueEvent(Event(RLights, item->descriptor().suffix, lightNode.id(), item));
        }
        item = lightNode.item(RStateOn);
        if (item && stateOnOff != UINT8_MAX) // updated?
        {
            DBG_Assert(stateOnOff == 0 || stateOnOff == 1);
            item->setValue(stateOnOff);
            enqueueEvent(Event(RLights, item->descriptor().suffix, lightNode.id(), item));
        }
        updateLightEtag(&lightNode);
        lightNode.setNeedSaveDatabase(true);
        saveDatabaseItems |= DB_LIGHTS;
    }

    for (Sensor &sensor : sensors)
    {
        if (sensor.deletedState() != Sensor::StateNormal)
        {
            continue;
        }

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

        if (modelId.isEmpty())
        {
            modelId = sensor.modelId();
        }

        sensor.rx();
        bool updated = false;
        restNodePending = &sensor; // remember one sensor for pending tasks

        {
            ResourceItem *item = sensor.item(RConfigReachable);
            if (item && !item->toBool())
            {
                item->setValue(true);
                enqueueEvent(Event(RSensors, RConfigReachable, sensor.id(), item));
                updated = true;
            }
        }

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
                q_ptr->nodeUpdated(sensor.address().ext(), QLatin1String(item->descriptor().suffix), QString::number(bat));

                if (item->lastSet() == item->lastChanged())
                {
                    updated = true;
                }
            }
        }

        if (temperature != INT16_MIN)
        {
            ResourceItem *item = sensor.item(RStateTemperature);
            if (item)
            {
                ResourceItem *item2 = sensor.item(RConfigOffset);
                if (item2 && item2->toNumber() != 0)
                {
                    temperature += item2->toNumber();
                }
            }
            else
            {
                item = sensor.item(RConfigTemperature);
            }
            if (item)
            {
                item->setValue(temperature);
                enqueueEvent(Event(RSensors, item->descriptor().suffix, sensor.id(), item));

                if (item->lastSet() == item->lastChanged())
                {
                    updated = true;
                }
                if (item->descriptor().suffix == RStateTemperature)
                {
                    sensor.updateStateTimestamp();
                    enqueueEvent(Event(RSensors, RStateLastUpdated, sensor.id()));
                    updated = true;
                }
            }
        }

        if (humidity != UINT16_MAX)
        {
            ResourceItem *item = sensor.item(RStateHumidity);
            if (item)
            {
                ResourceItem *item2 = sensor.item(RConfigOffset);
                if (item2 && item2->toNumber() != 0)
                {
                    humidity += item2->toNumber();
                }
                item->setValue(humidity);
                enqueueEvent(Event(RSensors, item->descriptor().suffix, sensor.id(), item));
                sensor.updateStateTimestamp();
                enqueueEvent(Event(RSensors, RStateLastUpdated, sensor.id()));
                updated = true;
            }
        }

        if (pressure != INT16_MIN)
        {
          ResourceItem *item = sensor.item(RStatePressure);
          if (item)
          {
              item->setValue(pressure);
              enqueueEvent(Event(RSensors, item->descriptor().suffix, sensor.id(), item));
              sensor.updateStateTimestamp();
              enqueueEvent(Event(RSensors, RStateLastUpdated, sensor.id()));
              updated = true;
          }
        }

        if (power != UINT32_MAX)
        {
            ResourceItem *item = sensor.item(RStatePower);
            if (item)
            {
                item->setValue(power);
                enqueueEvent(Event(RSensors, item->descriptor().suffix, sensor.id(), item));
                sensor.updateStateTimestamp();
                enqueueEvent(Event(RSensors, RStateLastUpdated, sensor.id()));
                updated = true;
            }
        }

        if (consumption != UINT32_MAX)
        {
            ResourceItem *item = sensor.item(RStateConsumption);
            if (item)
            {
                item->setValue(consumption);
                enqueueEvent(Event(RSensors, item->descriptor().suffix, sensor.id(), item));
                sensor.updateStateTimestamp();
                enqueueEvent(Event(RSensors, RStateLastUpdated, sensor.id()));
                updated = true;
            }
        }

        if (voltage != UINT32_MAX)
        {
            ResourceItem *item = sensor.item(RStateVoltage);
            if (item)
            {
                item->setValue(voltage);
                enqueueEvent(Event(RSensors, item->descriptor().suffix, sensor.id(), item));
                sensor.updateStateTimestamp();
                enqueueEvent(Event(RSensors, RStateLastUpdated, sensor.id()));
                updated = true;
            }
        }

        if (current != UINT32_MAX)
        {
            ResourceItem *item = sensor.item(RStateCurrent);
            if (item)
            {
                item->setValue(current);
                enqueueEvent(Event(RSensors, item->descriptor().suffix, sensor.id(), item));
                sensor.updateStateTimestamp();
                enqueueEvent(Event(RSensors, RStateLastUpdated, sensor.id()));
                updated = true;
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
            // item = item ? item : sensor.item(RStateWater);  // lumi.sensor_wleak.aq1, ignore, value is not reliable
            if (attrId == 0xff02)
            {
                // don't update Mija devices
                // e.g. lumi.sensor_motion always reports 1
            }
            else if (sensor.modelId().startsWith(QLatin1String("lumi.sensor_motion")))
            {
                // don't update Motion sensor state.
                // Imcompatibility with delay feature, and not really usefull
               sensor.updateStateTimestamp();
               enqueueEvent(Event(RSensors, RStateLastUpdated, sensor.id()));
               updated = true;
            }
            else if (sensor.modelId().startsWith(QLatin1String("lumi.sensor_wleak")))
            {
               // only update state timestamp assuming last known value is valid
                sensor.updateStateTimestamp();
                enqueueEvent(Event(RSensors, RStateLastUpdated, sensor.id()));
                updated = true;
            }
            else if (item)
            {
                item->setValue(onOff);
                enqueueEvent(Event(RSensors, item->descriptor().suffix, sensor.id(), item));
                sensor.updateStateTimestamp();
                enqueueEvent(Event(RSensors, RStateLastUpdated, sensor.id()));
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

    if (searchSensorsState == SearchSensorsActive)
    {
        return;
    }

    if  (!restNodePending)
    {
        return;
    }

    Resource *r = dynamic_cast<Resource*>(restNodePending);
    DBG_Assert(r != nullptr);
    if (!r)
    {
        return;
    }

    if (modelId.endsWith(QLatin1String("86opcn01")))
    {
        auto *item = r->item(RConfigPending);
        if (item && (item->toNumber() & R_PENDING_MODE))
        {
            // Aqara Opple switches need to be configured to send proper button events
            // send the magic word
            DBG_Printf(DBG_INFO, "Write Aqara Opple switch 0x%016llX mode attribute 0x0009 = 1\n", ind.srcAddress().ext());
            deCONZ::ZclAttribute attr(0x0009, deCONZ::Zcl8BitUint, QLatin1String("mode"), deCONZ::ZclReadWrite, false);
            attr.setValue(static_cast<quint64>(1));
            writeAttribute(restNodePending, 0x01, XIAOMI_CLUSTER_ID, attr, VENDOR_XIAOMI);
            item->setValue(item->toNumber() & ~R_PENDING_MODE);
        }
    }

    if (dateCode.isEmpty() && restNodePending)
    {
        // read datecode, will be applied to all sensors of this device
        readAttributes(restNodePending, ind.srcEndpoint(), BASIC_CLUSTER_ID, { 0x0006 });
        return;
    }

    ResourceItem *item = r->item(RAttrModelId);
    if (item && item->toString().startsWith(QLatin1String("lumi.vibration")))
    {
        item = r->item(RConfigSensitivity);
        ResourceItem *item2 = r->item(RConfigPending);
        DBG_Assert(item2);
        DBG_Assert(item);
        if (!item->lastSet().isValid() || item2->toNumber() == 0)
        {
            if (readAttributes(restNodePending, ind.srcEndpoint(), BASIC_CLUSTER_ID, { 0xff0d }, VENDOR_XIAOMI))
            {
                return;
            }
        }
        else
        {
            if (item2 && item2->toNumber() & R_PENDING_SENSITIVITY)
            {
                deCONZ::ZclAttribute attr(0xff0d, deCONZ::Zcl8BitUint, "sensitivity", deCONZ::ZclReadWrite, true);
                attr.setValue(static_cast<quint64>(item->toNumber()));
                if (writeAttribute(restNodePending, ind.srcEndpoint(), BASIC_CLUSTER_ID, attr, VENDOR_XIAOMI))
                {
                    item2->setValue(item2->toNumber() & ~R_PENDING_SENSITIVITY);
                    return;
                }
            }
        }
    }
}

void DeRestPluginPrivate::queuePollNode(RestNodeBase *node)
{
    if (!node || !node->node() || node->address().ext() == gwDeviceAddress.ext())
    {
        return;
    }

    if (!node->node()->nodeDescriptor().receiverOnWhenIdle())
    {
        return; // only support non sleeping devices for now
    }

    auto *resource = dynamic_cast<Resource*>(node);

    if (!resource)
    {
        return;
    }

    const PollNodeItem pollItem(node->uniqueId(), resource->prefix());

    if (std::find(pollNodes.begin(), pollNodes.end(), pollItem) != pollNodes.end())
    {
        return; // already in queue
    }

    pollNodes.push_back(pollItem);
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


    if (zclFrame.manufacturerCode_t() != 0x0000_mfcode)
    {
        outZclFrame.setFrameControl(outZclFrame.frameControl() | deCONZ::ZclFCManufacturerSpecific);
        outZclFrame.setManufacturerCode(zclFrame.manufacturerCode_t());
    }

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
            if (i->closeTimeout > 0)
            {
                i->hdr = hdr;
                if (i->closeTimeout < closeTimeout)
                {
                    i->closeTimeout = closeTimeout;
                    //DBG_Printf(DBG_INFO, "refresh socket %s : %u %s\n", qPrintable(sock->peerAddress().toString()), sock->peerPort(), qPrintable(hdr.path()));
                }
            }
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

    if ((task.taskType != TaskSetLevel) &&
        (task.taskType != TaskGetSceneMembership) &&
        (task.taskType != TaskGetGroupMembership) &&
        (task.taskType != TaskGetGroupIdentifiers) &&
        (task.taskType != TaskStoreScene) &&
        (task.taskType != TaskRemoveScene) &&
        (task.taskType != TaskRemoveAllScenes) &&
        (task.taskType != TaskReadAttributes) &&
        (task.taskType != TaskWriteAttribute) &&
        (task.taskType != TaskViewScene) &&
        (task.taskType != TaskTuyaRequest) &&
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
                    DBG_Printf(DBG_INFO, "Replace task %d type %d in queue cluster 0x%04X with newer task %d of same type. %u runnig tasks\n", i->taskId, task.taskType, task.req.clusterId(), task.taskId, runningTasks.size());
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
                        DBG_Printf(DBG_INFO, "delay sending request %u dt %d ms to 0x%016llX, ep: 0x%02X cluster: 0x%04X onAir: %d\n", i->req.id(), dt, i->req.dstAddress().ext(), i->req.dstEndpoint(), i->req.clusterId(), onAir);
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
                DBG_Printf(DBG_INFO_L2, "delay sending request %u ep: 0x%02X cluster 0x%04X to %s onAir %d\n", i->req.id(), i->req.dstEndpoint(), i->req.clusterId(), qPrintable(i->req.dstAddress().toStringExt()), onAir);
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
        if (event.node()->address().nwk() == 0x0000)
        {
            addLightNode(event.node());
        }

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
                if (i->state() != LightNode::StateNormal)
                {
                    continue;
                }

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
        if (!data.isEmpty() && sd.deviceId() != 0xffff)
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
        case DOOR_LOCK_CLUSTER_ID:
        case SAMJIN_CLUSTER_ID:
        case TIME_CLUSTER_ID:
        case BOSCH_AIR_QUALITY_CLUSTER_ID:
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
        case ANALOG_OUTPUT_CLUSTER_ID: // lumi.curtain
        case WINDOW_COVERING_CLUSTER_ID:  // FIXME ubisys J1 is not a light
        // Danalock support. In nodeEvent() in de_web_plugin.cpp, whitelist DOOR_LOCK_CLUSTER_ID to call updateLightNode()
        case DOOR_LOCK_CLUSTER_ID:
        {
            updateLightNode(event);
        }
        break;
        case FAN_CONTROL_CLUSTER_ID:
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

    if (task.lightNode->state() != LightNode::StateNormal)
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
    \param ind the APS level data indication containing the ZCL packet
    \param zclFrame the actual ZCL frame which holds the groups cluster reponse
 */
void DeRestPluginPrivate::handleGroupClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
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
    \param ind the APS level data indication containing the ZCL packet
    \param zclFrame the actual ZCL frame which holds the scene cluster reponse
 */
void DeRestPluginPrivate::handleSceneClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    if (zclFrame.isDefaultResponse())
    {
    }
    else if (zclFrame.commandId() == 0x06 && zclFrame.frameControl() & deCONZ::ZclFCDirectionServerToClient) // Get scene membership response
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
    else if (zclFrame.commandId() == 0x02 && zclFrame.frameControl() & deCONZ::ZclFCDirectionServerToClient) // Remove scene response
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
    else if ((zclFrame.commandId() == 0x00 || zclFrame.commandId() == 0x40) && zclFrame.frameControl() & deCONZ::ZclFCDirectionServerToClient) // Add scene response | Enhanced add scene response
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
    else if ((zclFrame.commandId() == 0x01 || zclFrame.commandId() == 0x41) && zclFrame.frameControl() & deCONZ::ZclFCDirectionServerToClient) // View scene response || Enhanced view scene response
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
        else if (status == 0x8b && zclFrame.payload().size() >= 4) // scene not found
        {
            uint16_t groupId;
            uint8_t sceneId;

            stream >> groupId;
            stream >> sceneId;

            Group *group = getGroupForId(groupId);
            Scene *scene = group->getScene(sceneId);

            if (!group || !scene)
            {
                return;
            }

            auto *ls = scene->getLightState(lightNode->id());
            if (ls)
            {
                ls->setNeedRead(false); // move to add scene
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
                s.name = tr("Scene %u").arg(sceneId);
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
                    if (gwPermitJoinDuration == 0)
                    {
                        queuePollNode(lightNode);
                    }

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
    \param ind the APS level data indication containing the ZCL packet
    \param zclFrame the actual ZCL frame which holds the scene cluster reponse
 */
void DeRestPluginPrivate::handleOnOffClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    if (zclFrame.isDefaultResponse())
    {
        return;
    }

    bool dark = true;
    Group *group = nullptr;

    if (ind.dstAddressMode() == deCONZ::ApsGroupAddress)
    {
        group = getGroupForId(ind.dstAddress().group());
    }

    if (zclFrame.commandId() == ONOFF_COMMAND_ON_WITH_TIMED_OFF)
    {
        for (Sensor &s : sensors)
        {
            if (s.deletedState() != Sensor::StateNormal)
            {
                continue;
            }

            if ((s.address().hasExt() && s.address().ext() == ind.srcAddress().ext()) ||
                (s.address().hasNwk() && s.address().nwk() == ind.srcAddress().nwk()))
            {
                if (!s.type().endsWith(QLatin1String("Presence")))
                {
                     continue;
                }
                ResourceItem *item;
                qint64 delay = 0;

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

                    pushZclValueDb(s.address().ext(), s.fingerPrint().endpoint, OCCUPANCY_SENSING_CLUSTER_ID, 0x0000, 1);
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

/*! Handle packets related to the Philips 0xFC00 cluster.
    \param ind the APS level data indication containing the ZCL packet
    \param zclFrame the actual ZCL frame which holds the scene cluster reponse
 */
void DeRestPluginPrivate::handlePhilipsClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    if (zclFrame.isDefaultResponse() || zclFrame.manufacturerCode() != VENDOR_PHILIPS || zclFrame.commandId() != 0x00)
    {
        return;
    }

    Sensor *sensorNode = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), ind.srcEndpoint());

    if (!sensorNode)
    {
        return;
    }

    /* Philips Hue dimmer switch and Lutron Aurora Friends-of-Hue dimmer send following payload:
       For buttonevents: 0b00 00 30 0e 21 dddd
       For rotaryevents: 1400 01 30 0e 29 rrrr 21 dddd 29 rrrr 21 dddd 29 rrrr 21 dddd
       Where b is the button; e is the event; dddd is the duration and rrrr is the rotation.
     */

    if (zclFrame.payload().size() >= 5)
    {
        QDataStream stream(zclFrame.payload());
        stream.setByteOrder(QDataStream::LittleEndian);

        uint16_t button;
        uint8_t buttonType;
        uint8_t dataType;
        uint8_t event = 0xFF;
        int16_t rotation = -0x7FFF;
        uint16_t duration = 0xFFFF;

        stream >> button;
        stream >> buttonType;
        stream >> dataType;
        if (dataType == deCONZ::Zcl8BitEnum)
        {
            stream >> event;
            while (!stream.atEnd())
            {
                stream >> dataType;
                if      (dataType == deCONZ::Zcl16BitInt)  stream >> rotation;
                else if (dataType == deCONZ::Zcl16BitUint) stream >> duration;
                else                                       break;
            }
            if (buttonType == 0 && event != 0xFF && duration != 0xFFFF)
            {
                button *= 1000;
                button += event;

                DBG_Printf(DBG_INFO, "[INFO] - Button %u - %s endpoint: 0x%02X cluster: PHILIPS_SPECIFIC (0x%04X)\n", button,
                           qPrintable(sensorNode->modelId()), ind.srcEndpoint(), ind.clusterId());

                ResourceItem *item = sensorNode->item(RStateButtonEvent);
                if (item)
                {
                    updateSensorEtag(sensorNode);
                    sensorNode->updateStateTimestamp();
                    item->setValue(button);
                    Event e(RSensors, RStateButtonEvent, sensorNode->id(), item);
                    enqueueEvent(e);
                    ResourceItem *item = sensorNode->item(RStateEventDuration);
                    if (item)
                    {
                        item->setValue(duration);
                        Event e(RSensors, RStateEventDuration, sensorNode->id(), item);
                        enqueueEvent(e);
                    }
                    enqueueEvent(Event(RSensors, RStateLastUpdated, sensorNode->id()));
                }
            }
            else if (buttonType == 1 && event != 0xFF && rotation != -0x7FFF && duration != 0xFFFF)
            {
                DBG_Printf(DBG_INFO_L2, "%s: Philips cluster command: rotaryevent: %d, expectedrotation: %d, expectedeventduration: %d\n", qPrintable(sensorNode->address().toStringExt()), event, rotation, duration);
            }
            else
            {
                DBG_Printf(DBG_INFO_L2, "%s: Philips cluster command: %s\n", qPrintable(sensorNode->address().toStringExt()), qPrintable(zclFrame.payload()));
            }
        }
    }
}

/*! Handle packets related to the ZCL Commissioning cluster.
    \param ind the APS level data indication containing the ZCL packet
    \param zclFrame the actual ZCL frame which holds the Commissioning cluster reponse
 */
void DeRestPluginPrivate::handleCommissioningClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    uint8_t ep = ind.srcEndpoint();
    Sensor *sensorNode = getSensorNodeForAddressAndEndpoint(ind.srcAddress(), ind.srcEndpoint());
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

        while (!stream.atEnd() && epIter < count)
        {
            stream >> groupId;
            stream >> type;

            if (groupId == 0)
            {
                continue;
            }

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
        if (lightNode.state() != LightNode::StateNormal)
        {
            continue;
        }

        if (ind.srcAddress().hasExt() && ind.srcAddress().ext() != lightNode.address().ext())
        {
            continue;
        }

        if (ind.srcAddress().hasNwk() && ind.srcAddress().nwk() != lightNode.address().nwk())
        {
            continue;
        }

        lightNode.rx();

        ResourceItem *item = lightNode.item(RStateReachable);
        if (item && !item->toBool())
        {
            item->setValue(true);
            Event e(RLights, RStateReachable, lightNode.id(), item);
            enqueueEvent(e);
        }

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
                if (s.deletedState() != Sensor::StateNormal)
                {
                    continue;
                }

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
        if (i->state() != LightNode::StateNormal)
        {
            continue;
        }

        if (i->address().ext() == ext)
        {
            i->rx();
            i->setValue(RAttrLastAnnounced, i->lastRx().toUTC());

            // clear to speedup polling
            for (NodeValue &val : i->zclValues())
            {
                val.timestamp = QDateTime();
                val.timestampLastReport = QDateTime();
                val.timestampLastConfigured = QDateTime();
            }

            i->setLastAttributeReportBind(0);

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

            deCONZ::Node *node = i->node();
            if (node && node->endpoints().end() == std::find(node->endpoints().begin(),
                                                     node->endpoints().end(),
                                                     i->haEndpoint().endpoint()))
            {
                continue; // not a active endpoint
            }

            ResourceItem *item = i->item(RStateReachable);

            if (item)
            {
                item->setValue(true); // refresh timestamp after device announce
                if (i->state() == LightNode::StateNormal)
                {
                    Event e(i->prefix(), RStateReachable, i->id(), item);
                    enqueueEvent(e);
                }

                updateEtag(gwConfigEtag);
            }

            DBG_Printf(DBG_INFO, "DeviceAnnce of LightNode: %s Permit Join: %i\n", qPrintable(i->address().toStringExt()), gwPermitJoinDuration);

            // force reading attributes
            i->enableRead(READ_GROUPS | READ_SCENES);

            // bring to front to force next polling
            const PollNodeItem pollItem(i->uniqueId(), i->prefix());
            pollNodes.push_front(pollItem);

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

                // clear to speedup polling
                for (NodeValue &val : si->zclValues())
                {
                    val.timestamp = QDateTime();
                    val.timestampLastReport = QDateTime();
                    val.timestampLastConfigured = QDateTime();
                }

                addSensorNode(si->node()); // check if somethings needs to be updated
            }

            if (si->type() == QLatin1String("ZHATime"))
            {
                if (!si->mustRead(READ_TIME))
                {
                    DBG_Printf(DBG_INFO, "  >>> %s sensor %s: set READ_TIME from handleDeviceAnnceIndication()\n", qPrintable(si->type()), qPrintable(si->name()));
                    si->enableRead(READ_TIME);
                    si->setLastRead(READ_TIME, idleTotalCounter);
                    si->setNextReadTime(READ_TIME, queryTime);
                    queryTime = queryTime.addSecs(1);
                }
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

/*! Handle node descriptor response.
    \param ind a ZDP NodeDescriptor_rsp
 */
void DeRestPluginPrivate::handleNodeDescriptorResponseIndication(const deCONZ::ApsDataIndication &ind)
{
    patchNodeDescriptor(ind);
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
            if (isLightNodeInGroup(lightNode, task.req.dstAddress().group()))
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
            if (task.onOff && item && item->toBool() != (task.level > 0)) // FIXME abuse of taks.onOff
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
            {
                updateLightEtag(lightNode);
                lightNode->setColorLoopActive(task.colorLoop);
                setAttributeColorLoopActive(lightNode);
            }
            break;

        default:
            break;
        }
    }
}

/*! Patch invalid node descriptors resulting from mac capabilities of 0x00.
    \param ind a ZDP NodeDescriptor_rsp
 */
void DeRestPluginPrivate::patchNodeDescriptor(const deCONZ::ApsDataIndication &ind)
{
            int i = 0;
            quint16 nwk;
            const deCONZ::Node *node;
            deCONZ::NodeDescriptor nd;

            {
                quint8 seq;
                quint8 status;

                QDataStream stream(ind.asdu());
                stream.setByteOrder(QDataStream::LittleEndian);

                stream >> seq;
                stream >> status;
                stream >> nwk;

                nd.readFromStream(stream);
            }

            while (apsCtrl->getNode(i, &node) == 0)
            {
                if(nwk == node->address().nwk() && node->address().nwk() != 0)      // Skip the coordinator
                {
                    DBG_Printf(DBG_INFO_L2, "[ND] NWK: 0x%04X\n", node->address().nwk());
                    DBG_Printf(DBG_INFO_L2, "[ND] Ext: %s\n", qPrintable(node->address().toStringExt()));
                    DBG_Printf(DBG_INFO_L2, "[ND] Current node descriptor: 0x%s\n", qPrintable(node->nodeDescriptor().toByteArray().toHex()));
                    DBG_Printf(DBG_INFO_L2, "[ND] Checking node...\n");

                    if(node->nodeDescriptor().isNull() || node->nodeDescriptor().toByteArray() != nd.toByteArray())
                    {
                        deCONZ::Node *patchableNode = const_cast<deCONZ::Node*>(&*node);
                        deCONZ::NodeDescriptor &patchableNd = const_cast<deCONZ::NodeDescriptor&>(nd);

                        if(node->nodeDescriptor().isNull())                 // Check current node descriptor
                        {
                            DBG_Printf(DBG_INFO_L2, "[ND] Current node descriptor is NULL...\n");
                            DBG_Printf(DBG_INFO_L2, "[ND] Checking validity of received node descriptor...\n");
                        }
                        else if(node->nodeDescriptor().toByteArray() != nd.toByteArray())
                        {
                            DBG_Printf(DBG_INFO_L2, "[ND] Current node descriptor deviates from the received...\n");
                        }
                        DBG_Printf(DBG_INFO_L2, "[ND] Received node descriptor: 0x%s\n", qPrintable(nd.toByteArray().toHex()));

                        if(qPrintable(nd.toByteArray().toHex()) == 0)       // Check received node descriptor
                        {
                            // Sanity check, do nothing
                        }
                        else
                        {
                            int j = 0;

                            if(nd.macCapabilities() == 0)       // This already results in an invalid node descriptor
                            {
                                DBG_Printf(DBG_INFO_L2, "[ND] Received node descriptor is invalid due to mac capabilities being 0\n");
                                DBG_Printf(DBG_INFO_L2, "[ND] Updating mac capabilities to 0x80...\n");
                                patchableNd.setMacCapabilities(deCONZ::MacCapability::MacAllocateAddress);
                                j++;
                            }

                            // Also fix incorrect manufacturer code for older Develco devices
                            if(node->address().toStringExt().mid(2).startsWith("0015bc", Qt::CaseInsensitive) && nd.manufacturerCode() == 0x0000)
                            {
                                patchableNd.setManufacturerCode(4117);  // MFC: 0x1015
                                j++;
                            }

                            if(j > 0)
                            {
                                if(!patchableNd.isNull() && node->nodeDescriptor().toByteArray() == patchableNd.toByteArray())
                                {
                                    DBG_Printf(DBG_INFO_L2, "[ND] Node descriptor seems to have been already patched. All good, nothing to do...\n");
                                }
                                else if(!patchableNd.isNull())
                                {
                                    DBG_Printf(DBG_INFO_L2, "[ND] Node descriptor is now valid, patching 0x%s\n", qPrintable(patchableNd.toByteArray().toHex()));
                                    patchableNode->setNodeDescriptor(patchableNd);
                                    DBG_Printf(DBG_INFO_L2, "[ND] Updating node cache and database\n");
                                    apsCtrl->updateNode(*patchableNode);
                                    DBG_Printf(DBG_INFO_L2, "[ND] new MFC: 0x%004X\n", node->nodeDescriptor().manufacturerCode());
                                    pushZdpDescriptorDb(node->address().ext(), ZDO_ENDPOINT, ZDP_NODE_DESCRIPTOR_CLID, node->nodeDescriptor().toByteArray());
                                }
                                else
                                {
                                    DBG_Printf(DBG_INFO_L2, "[ND] Node descriptor patch skipped due to invalid new node descriptor\n");
                                }
                            }
                            else
                            {
                                DBG_Printf(DBG_INFO_L2, "[ND] Node descriptor patch skipped due to other reasons\n", node->nodeDescriptor().manufacturerCode());
                            }
                        }
                    }
                    else
                    {
                        DBG_Printf(DBG_INFO_L2, "[ND] All good, nothing to do...\n");

                    }
                }
                i++;
            }
}

/*! Speed up discovery of end devices.
 */
void DeRestPluginPrivate::delayedFastEnddeviceProbe(const deCONZ::NodeEvent *event)
{
    if (!apsCtrl)
    {
        return;
    }

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
    if (macPoll)
    {
        if (event->node() && event->node()->address().ext() != sc->address.ext())
        {
            return;
        }

        DBG_Printf(DBG_INFO, "MAC poll fastEnddeviceProbe() 0x%016llX\n", sc->address.ext());
    }
#else
//    bool macPoll = false;
#endif

    {
        Sensor *sensor = getSensorNodeForAddress(sc->address);
        const deCONZ::Node *node = sensor ? sensor->node() : nullptr;

        if (sensor && sensor->deletedState() != Sensor::StateNormal)
        {
            DBG_Printf(DBG_INFO, "don't use deleted sensor and node 0x%016llX as candidate\n", sc->address.ext());
            sensor = nullptr;
            node = nullptr;
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

        if (sc->timeout.isValid() && sc->timeout.elapsed() < 9000)
        {
            DBG_Printf(DBG_INFO, "wait response fastEnddeviceProbe() 0x%016llX, elapsed %d ms\n", sc->address.ext(), sc->timeout.elapsed());
            return;
        }

        bool hasNodeDescriptor = false;
        bool hasActiveEndpoints = false;

        for (auto const &ind : fastProbeIndications)
        {
            if      (ind.clusterId() == ZDP_NODE_DESCRIPTOR_RSP_CLID) { hasNodeDescriptor = true; }
            else if (ind.clusterId() == ZDP_ACTIVE_ENDPOINTS_RSP_CLID) { hasActiveEndpoints = true; }
        }

        if (!hasNodeDescriptor)
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
            apsReq.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);

            QDataStream stream(&apsReq.asdu(), QIODevice::WriteOnly);
            stream.setByteOrder(QDataStream::LittleEndian);

            stream << zclSeq++;
            stream << sc->address.nwk();

            deCONZ::ApsController *apsCtrl = deCONZ::ApsController::instance();

            if (apsCtrl && apsCtrl->apsdeDataRequest(apsReq) == deCONZ::Success)
            {
                queryTime = queryTime.addSecs(5);
                sc->timeout.restart();
                sc->waitIndicationClusterId = ZDP_NODE_DESCRIPTOR_RSP_CLID;
            }
            return;
        }

        sc->endpoints = node->endpoints();

        if (!hasActiveEndpoints)
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
            apsReq.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);

            QDataStream stream(&apsReq.asdu(), QIODevice::WriteOnly);
            stream.setByteOrder(QDataStream::LittleEndian);

            stream << zclSeq++;
            stream << sc->address.nwk();

            deCONZ::ApsController *apsCtrl = deCONZ::ApsController::instance();

            if (apsCtrl && apsCtrl->apsdeDataRequest(apsReq) == deCONZ::Success)
            {
                queryTime = queryTime.addSecs(5);
                sc->timeout.restart();
                sc->waitIndicationClusterId = ZDP_ACTIVE_ENDPOINTS_RSP_CLID;
            }
            return;
        }

        // simple descriptor for endpoint 0x01
        {
            quint8 ep = 0;

            for (size_t i = 0; i < node->endpoints().size(); i++)
            {
                ep = node->endpoints()[i]; // search

                for (int j = 0; j < node->simpleDescriptors().size(); j++)
                {
                    const deCONZ::SimpleDescriptor &sd = node->simpleDescriptors()[j];
                    if (sd.endpoint() == ep && sd.deviceId() != 0xffff)
                    {
                        ep = 0;
                        break;
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
                    apsReq.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);

                    QDataStream stream(&apsReq.asdu(), QIODevice::WriteOnly);
                    stream.setByteOrder(QDataStream::LittleEndian);

                    stream << zclSeq++;
                    stream << sc->address.nwk();
                    stream << ep;

                    deCONZ::ApsController *apsCtrl = deCONZ::ApsController::instance();

                    if (apsCtrl && apsCtrl->apsdeDataRequest(apsReq) == deCONZ::Success)
                    {
                        queryTime = queryTime.addSecs(1);
                        sc->timeout.restart();
                        sc->waitIndicationClusterId = ZDP_SIMPLE_DESCRIPTOR_RSP_CLID;
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
        bool dateCodeAvailable = false;
        quint8 thermostatClusterEndpoint = 0;

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
                            dateCodeAvailable = attr.isAvailable(); // might become false after first read
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
                        else if (attr.lastRead() != static_cast<time_t>(-1) && attr.dataType() == deCONZ::ZclCharacterString && attr.toString().isEmpty())
                        {
                            // e.g. some Trust devices return empty strings
                            unavailBasicAttr.push_back(attr.id());
                        }
                    }
                    else if (cl.id() == IAS_ZONE_CLUSTER_ID)
                    {
                        if (attr.id() == 0x0001 && attr.numericValue().u64 != 0) // Zone type
                        {
                            DBG_Assert(attr.numericValue().u64 <= UINT16_MAX);
                            iasZoneType = static_cast<quint16>(attr.numericValue().u64);
                        }
                    }
                    else if (cl.id() == THERMOSTAT_CLUSTER_ID)
                    {
                        thermostatClusterEndpoint = sd.endpoint();
                    }
                }
            }

            if ((sd.deviceId() == DEV_ID_IAS_ZONE || sd.deviceId() == DEV_ID_IAS_WARNING_DEVICE) && iasZoneType == 0)
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
                    sc->timeout.restart();
                    sc->waitIndicationClusterId = apsReq.clusterId();
                }
                return;
            }
        }

        if (sensor && sensor->deletedState() != Sensor::StateNormal)
        {
            sensor = nullptr; // force query
        }

        if (node->nodeDescriptor().manufacturerCode() == VENDOR_SAMJIN)
        {
            swBuildIdAvailable = false; // empty string
            dateCodeAvailable = false; // unsupported attribute
        }
        else if (modelId == QLatin1String("HG06323")) // LIDL Remote Control
        {
            swBuildIdAvailable = false; // unsupported attribute
            dateCodeAvailable = false; // empty string
        }

        // manufacturer, model id, sw build id
        if (!sensor || modelId.isEmpty() || manufacturer.isEmpty() || (swBuildId.isEmpty() && dateCode.isEmpty() && (dateCodeAvailable || swBuildIdAvailable)))
        {
            if (!modelId.isEmpty() && !isDeviceSupported(node, modelId))
            {
                return;
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

            bool skip = false;

            if (thermostatClusterEndpoint > 0) // e.g. Eurotronic SPZB0001 thermostat
            {  }
            else if (iasZoneType > 0) // IAS motion and contact sensors
            {  }
            else if (modelId.startsWith(QLatin1String("lumi.")))
            {
                skip = true; // Xiaomi Mija devices won't respond to ZCL read
                DBG_Printf(DBG_INFO, "[4] Skipping additional attribute read - Model starts with 'lumi.'\n");
            }
            else if ((checkMacAndVendor(node, VENDOR_JENNIC) || checkMacAndVendor(node, VENDOR_ADUROLIGHT))
                     && node->simpleDescriptors().size() == 2
                     && node->simpleDescriptors().front().deviceId() == DEV_ID_ZLL_NON_COLOR_CONTROLLER)
            {
                skip = true; // e.g. Trust remote (ZYCT-202)
                DBG_Printf(DBG_INFO, "[4] Skipping additional attribute read -  Assumed Trust remote (ZYCT-202)\n");
            }

            if (skip)
            {
                // don't read these (Xiaomi, Trust, ...)
                // response is empty or no response at all
            }
            else if (manufacturer.isEmpty())
            {
                DBG_Printf(DBG_INFO, "[4.1] Get manufacturer code\n");
                attributes.push_back(0x0004); // manufacturer
            }
            else if (modelId.isEmpty())
            {
                DBG_Printf(DBG_INFO, "[4.1] Get model ID\n");
                attributes.push_back(0x0005); // model id
            }
            else if (swBuildId.isEmpty() && dateCode.isEmpty())
            {
                if ((sc->address.ext() & macPrefixMask) == tiMacPrefix ||
                    existDevicesWithVendorCodeForMacPrefix(sc->address, VENDOR_UBISYS) ||
                    modelId == QLatin1String("Motion Sensor-A") || // OSRAM motion sensor
                    manufacturer.startsWith(QLatin1String("Climax")) ||
                    modelId.startsWith(QLatin1String("lumi")) ||
                    node->nodeDescriptor().manufacturerCode() == VENDOR_CENTRALITE ||
                    !swBuildIdAvailable)
                {
                    DBG_Printf(DBG_INFO, "[4.1] Get date code\n");
                    attributes.push_back(0x0006); // date code
                }
                else
                {
                    DBG_Printf(DBG_INFO, "[4.1] Get sw build id\n");
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
                    DBG_Printf(DBG_INFO, "[4.2] get basic cluster attr 0x%04X for 0x%016llx\n", attrId, sc->address.ext());
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
                    sc->timeout.restart();
                    sc->waitIndicationClusterId = apsReq.clusterId();
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
        else if (sensor->modelId().startsWith(QLatin1String("RWL02")) || // Hue dimmer switch
                 sensor->modelId().startsWith(QLatin1String("ROM00")) || // Hue smart button
                 sensor->modelId().startsWith(QLatin1String("Z3-1BRL"))) // Lutron Aurora Friends-of-Hue dimmer switch

        {
            NodeValue val = sensor->getZclValue(VENDOR_CLUSTER_ID, 0x0000);
            if (!val.isValid())
            {
                if (checkSensorBindingsForAttributeReporting(sensor))
                {
                    return;
                }
            }

            // Stop the Hue dimmer from touchlinking when holding the On button.
            deCONZ::ZclAttribute attr(0x0031, deCONZ::Zcl16BitBitMap, "mode", deCONZ::ZclReadWrite, false);
            attr.setBitmap(0x000b);

            val = sensor->getZclValue(BASIC_CLUSTER_ID, 0x0031);

            if (val.isValid()) // already done
            {
            }
            else if (writeAttribute(sensor, sensor->fingerPrint().endpoint, BASIC_CLUSTER_ID, attr, VENDOR_PHILIPS))
            {
                queryTime = queryTime.addSecs(1);

                // mark done
                deCONZ::NumericUnion touchLink;
                touchLink.u64 = 0x000b;
                sensor->setZclValue(NodeValue::UpdateByZclRead, sensor->fingerPrint().endpoint, BASIC_CLUSTER_ID, 0x0031, touchLink);
                return;
            }

            checkSensorGroup(sensor);
            checkSensorBindingsForClientClusters(sensor);
        }
        else if (sensor->modelId().startsWith(QLatin1String("RDM00"))) // Hue wall switch module
        {
            NodeValue val = sensor->getZclValue(VENDOR_CLUSTER_ID, 0x0000);
            if (!val.isValid())
            {
                if (checkSensorBindingsForAttributeReporting(sensor))
                {
                }
            }

            // Stop the Hue dimmer from touchlinking when holding the On button.
            deCONZ::ZclAttribute attr(0x0031, deCONZ::Zcl16BitBitMap, "mode", deCONZ::ZclReadWrite, false);
            attr.setBitmap(0x000b);

            val = sensor->getZclValue(BASIC_CLUSTER_ID, 0x0031);

            if (val.isValid()) // already done
            {
            }
            else if (writeAttribute(sensor, sensor->fingerPrint().endpoint, BASIC_CLUSTER_ID, attr, VENDOR_PHILIPS))
            {
                queryTime = queryTime.addSecs(1);

                // mark done
                deCONZ::NumericUnion touchLink;
                touchLink.u64 = 0x000b;
                sensor->setZclValue(NodeValue::UpdateByZclRead, sensor->fingerPrint().endpoint, BASIC_CLUSTER_ID, 0x0031, touchLink);
            }

            std::vector<uint16_t> attributes;
            const NodeValue &devicemode = sensor->getZclValue(BASIC_CLUSTER_ID, 0x0034);
            if (!devicemode.timestamp.isValid())
            {
                attributes.push_back(0x0034); // devicemode
            }

            if (!attributes.empty() &&
                readAttributes(sensor, sensor->fingerPrint().endpoint, BASIC_CLUSTER_ID, attributes, VENDOR_PHILIPS))
            {
                queryTime = queryTime.addSecs(1);
            }
        }
        else if (sensor->modelId().startsWith(QLatin1String("SML00"))) // Hue motion sensor
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
        else if (sensor->modelId() == QLatin1String("TRADFRI wireless dimmer") && // IKEA dimmer
                 sensor->fingerPrint().profileId == ZLL_PROFILE_ID) // old ZLL firmware
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
        else if (sensor->modelId() == QLatin1String("TRADFRI remote control") && // IKEA remote
                 sensor->fingerPrint().profileId == ZLL_PROFILE_ID) // old ZLL firmware
        {
        }
        else if (sensor->modelId().startsWith(QLatin1String("TRADFRI on/off switch")) ||
                 sensor->modelId().startsWith(QLatin1String("TRADFRI SHORTCUT Button")) ||
                 sensor->modelId().startsWith(QLatin1String("TRADFRI open/close remote")) ||
                 sensor->modelId().startsWith(QLatin1String("TRADFRI remote control")) ||
                 sensor->modelId().startsWith(QLatin1String("TRADFRI wireless dimmer")) ||
                 sensor->modelId().startsWith(QLatin1String("TRADFRI motion sensor")))
        {
            checkSensorGroup(sensor);

            if (sensor->lastAttributeReportBind() < (idleTotalCounter - IDLE_ATTR_REPORT_BIND_LIMIT_SHORT))
            {
                if (checkSensorBindingsForClientClusters(sensor))
                {
                    sensor->setLastAttributeReportBind(idleTotalCounter);

                }
            }
        }
        else if (sensor->modelId().startsWith(QLatin1String("SYMFONISK")))
        {
            ResourceItem *item = sensor->item(RStateButtonEvent);

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
        else if (sensor->modelId() == QLatin1String("Remote switch") || // Legrand switch
                 sensor->modelId() == QLatin1String("Double gangs remote switch") || // Legrand switch double
                 sensor->modelId() == QLatin1String("Remote toggle switch") || // Legrand switch module
                 sensor->modelId() == QLatin1String("Remote motion sensor") || // Legrand motion sensor
                 sensor->modelId() == QLatin1String("ZBT-CCTSwitch-D0001") || // LDS Remote
                 sensor->modelId() == QLatin1String("Shutters central remote switch")) // Legrand shutter switch
        {
            checkSensorGroup(sensor);
            checkSensorBindingsForClientClusters(sensor);
        }
        else if (sensor->modelId().startsWith(QLatin1String("RC 110"))) // innr Remote
        {
            ResourceItem *item = sensor->item(RConfigGroup);
            if (!item)
            {
                item = sensor->addItem(DataTypeString, RConfigGroup);
                QStringList gids;
                QString gid;
                Group *group;

                for (quint8 endpoint = 0x01; endpoint <= 0x08; endpoint++)
                {
                    if (endpoint == 0x02)
                    {
                        // No client clusters on endpoint 0x02.
                        continue;
                    }

                    group = addGroup();
                    gid = group->id();
                    gids << gid;
                    group->setName(sensor->name());
                    DBG_Printf(DBG_INFO, "create group %s for sensor %s\n", qPrintable(gid), qPrintable(sensor->id()));
                    ResourceItem *item2 = group->addItem(DataTypeString, RAttrUniqueId);
                    DBG_Assert(item2);
                    if (item2)
                    {
                        const QString uid = generateUniqueId(sensor->address().ext(), endpoint, 0);
                        item2->setValue(uid);
                    }
                    if (group->addDeviceMembership(sensor->id()))
                    {
                    }

                    if (endpoint == 0x01)
                    {
                        // Binding of client clusters doesn't work for endpoint 0x01.
                        // Need to add the group to the server Groups cluster instead.
                        TaskItem task;

                        // set destination parameters
                        task.req.dstAddress() = sensor->address();
                        task.req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
                        task.req.setDstEndpoint(sensor->fingerPrint().endpoint);
                        task.req.setSrcEndpoint(getSrcEndpoint(sensor, task.req));
                        task.req.setDstAddressMode(deCONZ::ApsExtAddress);

                        addTaskAddToGroup(task, group->id().toInt());
                    }
                }
                item->setValue(gids.join(","));
                sensor->setNeedSaveDatabase(true);
                queSaveDb(DB_SENSORS, DB_SHORT_SAVE_DELAY);
                Event e(RSensors, RConfigGroup, sensor->id(), item);
                enqueueEvent(e);
            }
            checkSensorBindingsForClientClusters(sensor);
        }
        else if (sensor->modelId().startsWith(QLatin1String("ICZB-RM")) || // icasa remote
                 sensor->modelId().startsWith(QLatin1String("ZGR904-S")) || // Envilar remote
                 sensor->modelId().startsWith(QLatin1String("ZGRC-KEY")))  // Sunricher remote
        {
            quint8 lastEndpoint;

            if (sensor->modelId() == QLatin1String("ZGRC-KEY-012")) { lastEndpoint = 0x05; }
            else { lastEndpoint = 0x04; }

            ResourceItem *item = sensor->item(RConfigGroup);
            if (!item)
            {
                item = sensor->addItem(DataTypeString, RConfigGroup);
                QStringList gids;
                QString gid;
                Group *group;

                for (quint8 endpoint = 0x01; endpoint <= lastEndpoint; endpoint++)
                {
                    group = addGroup();
                    gid = group->id();
                    gids << gid;
                    group->setName(sensor->name());
                    DBG_Printf(DBG_INFO, "create group %s for sensor %s\n", qPrintable(gid), qPrintable(sensor->id()));
                    ResourceItem *item2 = group->addItem(DataTypeString, RAttrUniqueId);
                    DBG_Assert(item2);
                    if (item2)
                    {
                        const QString uid = generateUniqueId(sensor->address().ext(), endpoint, 0);
                        item2->setValue(uid);
                    }
                    if (group->addDeviceMembership(sensor->id()))
                    {
                    }
                }
                item->setValue(gids.join(","));
                sensor->setNeedSaveDatabase(true);
                queSaveDb(DB_SENSORS, DB_SHORT_SAVE_DELAY);
                Event e(RSensors, RConfigGroup, sensor->id(), item);
                enqueueEvent(e);
            }
            checkSensorBindingsForClientClusters(sensor);
        }
        else if (sensor->modelId().startsWith(QLatin1String("902010/23"))) // Bitron remote control
        {
            checkSensorGroup(sensor);
            checkSensorBindingsForClientClusters(sensor);

            ResourceItem *item = sensor->item(RStateButtonEvent);

            if (!item || !item->lastSet().isValid())
            {
                // Remote uses NWK group multicast, which is not picked up by deCONZ, see #2503.
                // As workaround, add bindings to the coordinator.
                BindingTask bindingTask, bindingTask2;

                bindingTask.state = BindingTask::StateIdle;
                bindingTask.action = BindingTask::ActionBind;
                bindingTask.restNode = sensor;
                Binding &bnd = bindingTask.binding;
                bnd.srcAddress = sensor->address().ext();
                bnd.dstAddrMode = deCONZ::ApsExtAddress;
                bnd.srcEndpoint = sensor->fingerPrint().endpoint;
                bnd.clusterId = ONOFF_CLUSTER_ID;
                bnd.dstAddress.ext = apsCtrl->getParameter(deCONZ::ParamMacAddress);
                bnd.dstEndpoint = endpoint();

                bindingTask2.state = BindingTask::StateIdle;
                bindingTask2.action = BindingTask::ActionBind;
                bindingTask2.restNode = sensor;
                Binding &bnd2 = bindingTask2.binding;
                bnd2.srcAddress = sensor->address().ext();
                bnd2.dstAddrMode = deCONZ::ApsExtAddress;
                bnd2.srcEndpoint = sensor->fingerPrint().endpoint;
                bnd2.clusterId = LEVEL_CLUSTER_ID;
                bnd2.dstAddress.ext = apsCtrl->getParameter(deCONZ::ParamMacAddress);
                bnd2.dstEndpoint = endpoint();

                if (bnd.dstEndpoint > 0) // valid gateway endpoint?
                {
                    queueBindingTask(bindingTask);
                    queueBindingTask(bindingTask2);
                }
            }
        }
        else if (sensor->modelId() == QLatin1String("AIR") && node->nodeDescriptor().manufacturerCode() == VENDOR_BOSCH2) // Bosch Air quality sensor
        {
            // binding for manufacturer specific BOSCH_AIR_QUALITY_CLUSTER_ID
            BindingTask bindingTask;

            bindingTask.state = BindingTask::StateIdle;
            bindingTask.action = BindingTask::ActionBind;
            bindingTask.restNode = sensor;
            Binding &bnd = bindingTask.binding;
            bnd.srcAddress = sensor->address().ext();
            bnd.dstAddrMode = deCONZ::ApsExtAddress;
            bnd.srcEndpoint = 0x02;
            bnd.clusterId = BOSCH_AIR_QUALITY_CLUSTER_ID;
            bnd.dstAddress.ext = apsCtrl->getParameter(deCONZ::ParamMacAddress);
            bnd.dstEndpoint = endpoint();

            if (bnd.dstEndpoint > 0) // valid gateway endpoint?
            {
                queueBindingTask(bindingTask);
            }
        }
        else if (sensor->modelId().endsWith(QLatin1String("86opcn01")))  // Aqara Opple
        {
            auto *item = sensor->item(RConfigPending);
            if (item && item->toNumber() & R_PENDING_MODE)
            {
                DBG_Printf(DBG_INFO, "Write Aqara Opple switch 0x%016llX mode attribute 0x0009 = 1\n", sensor->address().ext());
                // send the magic word to the Aqara Opple switch
                deCONZ::ZclAttribute attr(0x0009, deCONZ::Zcl8BitUint, "mode", deCONZ::ZclReadWrite, false);
                attr.setValue(static_cast<quint64>(1));
                writeAttribute(sensor, sensor->fingerPrint().endpoint, XIAOMI_CLUSTER_ID, attr, VENDOR_XIAOMI);
                item->setValue(item->toNumber() & ~R_PENDING_MODE);
            }
        }
        else if (sensor->modelId() == QLatin1String("HG06323")) // LIDL Remote Control
        {
            ResourceItem *item = sensor->item(RConfigGroup);
            if (!item)
            {
                item = sensor->addItem(DataTypeString, RConfigGroup);
                Group *group = addGroup();
                QString gid = group->id();

                DBG_Printf(DBG_INFO, "create group %s for sensor %s\n", qPrintable(gid), qPrintable(sensor->id()));
                group->setName(sensor->name());
                ResourceItem *item2 = group->addItem(DataTypeString, RAttrUniqueId);
                DBG_Assert(item2);
                if (item2)
                {
                    const QString uid = generateUniqueId(sensor->address().ext(), 0x01, 0);
                    item2->setValue(uid);
                }
                if (group->addDeviceMembership(sensor->id()))
                {
                }

                // Binding of client clusters doesn't work for endpoint 0x01.
                // Need to add the group to the server Groups cluster instead.
                TaskItem task;

                // set destination parameters
                task.req.dstAddress() = sensor->address();
                task.req.setTxOptions(deCONZ::ApsTxAcknowledgedTransmission);
                task.req.setDstEndpoint(0x01);
                task.req.setSrcEndpoint(getSrcEndpoint(sensor, task.req));
                task.req.setDstAddressMode(deCONZ::ApsExtAddress);

                addTaskAddToGroup(task, group->id().toInt());

                item->setValue(gid);
                sensor->setNeedSaveDatabase(true);
                queSaveDb(DB_SENSORS, DB_SHORT_SAVE_DELAY);
                Event e(RSensors, RConfigGroup, sensor->id(), item);
                enqueueEvent(e);
            }
            checkSensorBindingsForClientClusters(sensor);
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

    m_idleTimer->start(IDLE_TIMER_INTERVAL);
    d->idleTimer.start();
}

/*! The plugin deconstructor.
 */
DeRestPlugin::~DeRestPlugin()
{
    d = nullptr;
    m_w = nullptr;
}

/*! Handle idle states.

    After IDLE_LIMIT seconds user inactivity this timer
    checks if nodes need to be refreshed. This is the case
    if a node was not refreshed for IDLE_READ_LIMIT seconds.
 */
void DeRestPlugin::idleTimerFired()
{
    // Use extra QElapsedTimer since QTimer seems sometimes to
    // fire in < 1 sec. intervals (after database write?).
    if (d->idleTimer.elapsed() < (IDLE_TIMER_INTERVAL - 50)) // -50 : don't be too strict
    {
        DBG_Printf(DBG_INFO, "Skip idle timer callback, too early: elapsed %lld msec\n", d->idleTimer.elapsed());
        return;
    }
    d->idleTimer.start();

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
            d->gwBridgeId = QString("%1").arg(static_cast<qulonglong>(d->gwDeviceAddress.ext()), 16, 16, QLatin1Char('0')).toUpper();
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
        // automatically try reconnect network
        if (d->networkState == DeRestPluginPrivate::MaintainNetwork && d->gwRfConnectedExpected)
        {
            d->networkConnectedBefore = d->gwRfConnectedExpected;
            d->startReconnectNetwork(RECONNECT_CHECK_DELAY);
        }
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

                if (!lightNode->isAvailable() || !lightNode->lastRx().isValid() || !lightNode->node() || lightNode->state() != LightNode::StateNormal)
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

                // workaround for lights and smart plugs with multiple endpoints but only one basic cluster
                if ((lightNode->manufacturerCode() == VENDOR_JENNIC || // mostly Xiaomi
                     lightNode->manufacturerCode() == VENDOR_XIAOMI || // Xiaomi
                     lightNode->manufacturerCode() == VENDOR_EMBER ||  // LIDL
                     (lightNode->address().ext() & macPrefixMask) == tiMacPrefix) // GLEDOPTO
                    && (lightNode->modelId().isEmpty() || lightNode->manufacturer().isEmpty() || lightNode->item(RAttrSwVersion)->toString().isEmpty()))
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

                        if ((lightNode->manufacturer().isEmpty() || lightNode->manufacturer() == QLatin1String("Unknown")) && lightNode->manufacturer() != l.manufacturer())
                        {
                            lightNode->setManufacturerName(l.manufacturer());
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

                if (d->gwPermitJoinDuration == 0)
                {
                    d->queuePollNode(lightNode);
                }

                if (lightNode->lastRx().secsTo(now) > (5 * 60))
                {
                    // let poll manager detect if node is available
                    continue;
                }

                if (processLights)
                {
                    break;
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
                if (d->otauLastBusyTimeDelta() > OTA_LOW_PRIORITY_TIME && !d->permitJoinFlag)
                {
                    if (lightNode->lastAttributeReportBind() < (d->idleTotalCounter - IDLE_ATTR_REPORT_BIND_LIMIT) || lightNode->lastAttributeReportBind() == 0)
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


                if (sensorNode->modelId().isEmpty())
                {
                    LightNode *lightNode = d->getLightNodeForAddress(sensorNode->address());
                    if (lightNode && !lightNode->modelId().isEmpty())
                    {
                        sensorNode->setModelId(lightNode->modelId());
                    }
                }

                if (d->gwPermitJoinDuration == 0)
                {
                    d->queuePollNode(sensorNode);
                }

                if (sensorNode->lastRx().secsTo(now) > (5 * 60))
                {
                    // let poll manager detect if node is available
                    continue;
                }

                if (processSensors)
                {
                    break;
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
                            if (sensorNode->modelId() == QLatin1String("Thermostat")) // eCozy
                            {
                                val = sensorNode->getZclValue(*ci, 0x0023); // Temperature Setpoint Hold
                            }
                            else
                            {
                                val = sensorNode->getZclValue(*ci, 0x0029); // heating operation state
                            }

                            if (sensorNode->modelId().startsWith(QLatin1String("SPZB")) ||      // Eurotronic Spirit
                                sensorNode->modelId().startsWith(QLatin1String("SLT2")) ||      // Hive Active Heating Thermostat
                                sensorNode->modelId().startsWith(QLatin1String("SLR2")) ||      // Hive Active Heating Receiver 2 channel
                                sensorNode->modelId().startsWith(QLatin1String("SLR1b")) ||     // Hive Active Heating Receiver 1 channel
                                sensorNode->modelId().startsWith(QLatin1String("TRV001")) ||    // Hive TRV
                                sensorNode->modelId().startsWith(QLatin1String("TH112")) ||     // Sinope devices
                                sensorNode->modelId().startsWith(QLatin1String("TH1300ZB")) ||  // Sinope devices
                                sensorNode->modelId().startsWith(QLatin1String("eTRV0100")) ||  // Danfoss Ally
                                sensorNode->modelId().startsWith(QLatin1String("0x8020")) ||    // Danfoss RT24V Display thermostat
                                sensorNode->modelId().startsWith(QLatin1String("0x8021")) ||    // Danfoss RT24V Display thermostat with floor sensor
                                sensorNode->modelId().startsWith(QLatin1String("0x8030")) ||    // Danfoss RTbattery Display thermostat
                                sensorNode->modelId().startsWith(QLatin1String("0x8031")) ||    // Danfoss RTbattery Display thermostat with infrared
                                sensorNode->modelId().startsWith(QLatin1String("0x8034")) ||    // Danfoss RTbattery Dial thermostat
                                sensorNode->modelId().startsWith(QLatin1String("0x8035")) ||    // Danfoss RTbattery Dial thermostat with infrared
                                sensorNode->modelId().startsWith(QLatin1String("Zen-01")) ||    // Zen
                                sensorNode->modelId().startsWith(QLatin1String("Super TR")) ||  // Elko
                                sensorNode->modelId().startsWith(QLatin1String("AC201")) ||     // Owon
                                sensorNode->modelId().startsWith(QLatin1String("SORB")) ||      // Stelpro Orleans
                                sensorNode->modelId().startsWith(QLatin1String("3157100")))     // Centralite pearl
                            {
                                // supports reporting, no need to read attributes
                            }
                            else if (!val.timestamp.isValid() || val.timestamp.secsTo(now) > 600)
                            {
                                sensorNode->enableRead(READ_THERMOSTAT_STATE);
                                sensorNode->setLastRead(READ_THERMOSTAT_STATE, d->idleTotalCounter);
                                sensorNode->setNextReadTime(READ_THERMOSTAT_STATE, d->queryTime);
                                d->queryTime = d->queryTime.addSecs(tSpacing);
                                processSensors = true;

                                if (sensorNode->modelId() == QLatin1String("Thermostat"))
                                {
                                    sensorNode->enableRead(READ_THERMOSTAT_SCHEDULE);
                                    sensorNode->setLastRead(READ_THERMOSTAT_SCHEDULE, d->idleTotalCounter);
                                    sensorNode->setNextReadTime(READ_THERMOSTAT_SCHEDULE, d->queryTime);
                                    d->queryTime = d->queryTime.addSecs(tSpacing);
                                }
                            }
                        }

                        if (*ci == TIME_CLUSTER_ID)
                        {
                            if (!sensorNode->mustRead(READ_TIME))
                            {
                                val = sensorNode->getZclValue(*ci, 0x0000); // Time
                                if (!val.timestamp.isValid() || val.timestamp.secsTo(now) >= 6 * 3600)
                                {
                                    DBG_Printf(DBG_INFO, "  >>> %s sensor %s: set READ_TIME from idleTimerFired()\n", qPrintable(sensorNode->type()), qPrintable(sensorNode->name()));
                                    sensorNode->enableRead(READ_TIME);
                                    sensorNode->setLastRead(READ_TIME, d->idleTotalCounter);
                                    sensorNode->setNextReadTime(READ_TIME, d->queryTime);
                                    d->queryTime = d->queryTime.addSecs(tSpacing);
                                    processSensors = true;
                                }
                            }
                        }

                    }

                    DBG_Printf(DBG_INFO_L2, "Force read attributes for %s SensorNode %s\n", qPrintable(sensorNode->type()), qPrintable(sensorNode->name()));
                    //break;
                }

                if ((d->otauLastBusyTimeDelta() > OTA_LOW_PRIORITY_TIME) && (sensorNode->lastAttributeReportBind() < (d->idleTotalCounter - IDLE_ATTR_REPORT_BIND_LIMIT)))
                {
                    if (d->checkSensorBindingsForAttributeReporting(sensorNode))
                    {
                        sensorNode->setLastAttributeReportBind(d->idleTotalCounter);
                    }

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
                if (i->isAvailable() && i->manufacturerCode() == VENDOR_ATMEL && i->modelId().startsWith("FLS")) // old FLS devices
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
        d->saveDatabaseItems |= (DB_SENSORS | DB_RULES | DB_LIGHTS);
        d->openDb();
        d->saveDb();
        d->ttlDataBaseConnection = 0;
        d->closeDb();

        d->apsCtrl = nullptr;
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
        m_w = new DeRestWidget(nullptr, this);
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
    d->pushClientForClose(sock, 60, hdr);

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
    req.mode = d->gwHueMode ? ApiModeHue : ApiModeNormal;

    ApiResponse rsp;
    rsp.httpStatus = HttpStatusNotFound;
    rsp.contentType = HttpContentHtml;

    int ret = REQ_NOT_HANDLED;

    d->authorise(req, rsp);

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
        bool resourceExist = true;

        if ((req.path.size() >= 2) && (req.auth == ApiAuthFull || req.auth == ApiAuthInternal))
        {
            // GET /api/<apikey>
            if ((req.path.size() == 2) && (req.hdr.method() == QLatin1String("GET")))
            {
                ret = d->getFullState(req, rsp);
            }
            else if (path[2] == QLatin1String("devices"))
            {
                ret = d->restDevices->handleApi(req, rsp);
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
                ret = d->handleConfigFullApi(req, rsp);
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
        }
        else
        {
            ret = d->handleConfigBasicApi(req, rsp);
        }

        if ((ret == REQ_NOT_HANDLED) && (req.auth == ApiAuthLocal || req.auth == ApiAuthInternal || req.auth == ApiAuthFull))
        {
            ret = d->handleConfigLocalApi(req, rsp);
        }

        if (ret == REQ_NOT_HANDLED)
        {
            const QStringList ls = req.path.mid(2);
            const QString resource = "/" + ls.join('/');
            if (req.auth == ApiAuthFull || req.auth == ApiAuthInternal)
            {
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
            else
            {
                rsp.httpStatus = HttpStatusForbidden;
                rsp.list.append(d->errorToMap(ERR_UNAUTHORIZED_USER, resource, "unauthorized user"));
                if (req.sock)
                {
                    DBG_Printf(DBG_HTTP, "\thost: %s\n", qPrintable(req.sock->peerAddress().toString()));
                }
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
    if (rsp.httpStatus != HttpStatusOk && req.mode != ApiModeNormal)
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

bool DeRestPlugin::dbSaveAllowed() const
{
    return (d->saveDatabaseItems & DB_NOSAVE) == 0;
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

            DBG_Assert(i->sock != nullptr);

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

    std::vector<TcpClient>::iterator i = openClients.begin();
    std::vector<TcpClient>::iterator end = openClients.end();

    for ( ; i != end; ++i)
    {
        if (i->sock == obj)
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
    union _a
    {
        quint8 bytes[8];
        quint64 mac;
    } a;
    a.mac = extAddress;
    int ret = -1;
    char buf[64];

    if (clusterId != 0 && endpoint != 0xf2)
    {
        ret = snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x-%02x-%04x",
                    a.bytes[7], a.bytes[6], a.bytes[5], a.bytes[4],
                    a.bytes[3], a.bytes[2], a.bytes[1], a.bytes[0],
                    endpoint, clusterId);

    }
    else if (endpoint != 0)
    {
        ret = snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x-%02x",
                    a.bytes[7], a.bytes[6], a.bytes[5], a.bytes[4],
                    a.bytes[3], a.bytes[2], a.bytes[1], a.bytes[0],
                    endpoint);
    }
    else
    {
        ret = snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
                    a.bytes[7], a.bytes[6], a.bytes[5], a.bytes[4],
                    a.bytes[3], a.bytes[2], a.bytes[1], a.bytes[0]);
    }
    Q_ASSERT(ret > 0);
    Q_ASSERT(static_cast<size_t>(ret) < sizeof(buf));

    if (ret < 0 || static_cast<size_t>(ret) >= sizeof(buf))
    {
        DBG_Printf(DBG_ERROR, "failed to generate uuid, buffer too small\n");
        Q_ASSERT(0);
        return QString();
    }

    return QString::fromLatin1(buf);
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
    if (!apsCtrl)
    {
        return false;
    }

    if (!isInNetwork())
    {
        DBG_Printf(DBG_ERROR, "backup: failed to export - ZigBee network is down\n");
        return false;
    }

    ttlDataBaseConnection = 0;
    closeDb();

    if (db)
    {
        DBG_Printf(DBG_ERROR, "backup: failed to export - database busy\n");
        return false; // might be busy
    }

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

    bool ok = true;
    QString saveString = Json::serialize(map, ok);

    DBG_Assert(ok);
    if (!ok)
    {
        return false;
    }

    const QString path = deCONZ::getStorageLocation(deCONZ::ApplicationsDataLocation);

    // cleanup older files
    const std::vector<const char*> files1 = { "/deCONZ.conf", "/deCONZ.tar", "/deCONZ.tar.gz" };
    for (const char *f : files1)
    {
        const QString filePath = path + f;
        if (QFile::exists(filePath))
        {
            if (QFile::remove(filePath))
            {
                DBG_Printf(DBG_INFO, "backup: removed temporary file %s\n", qPrintable(filePath));
            }
            else
            {
                DBG_Printf(DBG_ERROR, "backup: failed to remove temporary file %s\n", qPrintable(filePath));
                return false;
            }
        }
    }

    { // put config as JSON object in file
        QFile configFile(path + "/deCONZ.conf");
        if (configFile.open(QIODevice::ReadWrite))
        {
            QTextStream stream(&configFile);
            stream << saveString << endl;
            configFile.close();
        }
    }

    if (QFile::exists(path + "/deCONZ.conf"))
    {
        //create .tar
        QProcess *archProcess = new QProcess(this);

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
        // clean up old homebridge backup files
        QStringList filters;
        filters << "AccessoryInfo*";
        filters << "IdentifierCache*";

         QDir appDir(path);
         QStringList files = appDir.entryList(filters);

         for (QString f : files)
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

        archProcess->start("tar -cf " + path + "/deCONZ.tar -C " + path + " deCONZ.conf zll.db session.default " + FirstFileName + " " + SecondFileName + " " + logfilesDirectories);
#endif
        archProcess->waitForFinished(EXT_PROCESS_TIMEOUT);
        DBG_Printf(DBG_INFO, "%s\n", qPrintable(archProcess->readAllStandardOutput()));
        archProcess->deleteLater();
        archProcess = nullptr;

        //create .tar.gz
        QProcess *zipProcess = new QProcess(this);
#ifdef Q_OS_WIN

        cmd = appPath + "/7za.exe";
        args.clear();
        args.append("a");
        args.append(path + "/deCONZ.tar.gz");
        args.append(path + "/deCONZ.tar");
        zipProcess->start(cmd, args);
#endif
#ifdef Q_OS_LINUX
        zipProcess->start("gzip -k -f " + path + "/deCONZ.tar");
#endif
        zipProcess->waitForFinished(EXT_PROCESS_TIMEOUT);
        DBG_Printf(DBG_INFO, "%s\n", qPrintable(zipProcess->readAllStandardOutput()));
        zipProcess->deleteLater();
        zipProcess = nullptr;
    }

    //cleanup
    const std::vector<const char*> files2 = { "/deCONZ.conf", "/deCONZ.tar" };
    for (const char *f : files2)
    {
        const QString filePath = path + f;
        if (QFile::exists(filePath))
        {
            if (QFile::remove(filePath))
            {
                DBG_Printf(DBG_INFO, "backup: removed temporary file %s\n", qPrintable(filePath));
            }
            else
            {
                DBG_Printf(DBG_ERROR, "backup: failed to remove temporary file %s\n", qPrintable(filePath));
            }
        }
        else
        {
            DBG_Printf(DBG_ERROR, "backup: temporary file %s doesn't exist\n", qPrintable(filePath));
            ok = false; // these files must exist
        }
    }

    return ok;
}

/*! Import the deCONZ network settings from a file.
 */
bool DeRestPluginPrivate::importConfiguration()
{
    if (!apsCtrl)
    {
        return false;
    }

    // prevent overwrite database with content of current memory
    // will be reset after application soft restart
    ttlDataBaseConnection = 0;
    saveDatabaseItems |= DB_NOSAVE;
    closeDb();

    if (db)
    {
        DBG_Printf(DBG_ERROR, "backup: failed to import - database busy\n");
        return false; // database might be busy
    }

    const QString path = deCONZ::getStorageLocation(deCONZ::ApplicationsDataLocation);

    // cleanup old files
    const std::vector<const char*> files1 = { "/deCONZ.conf", "/deCONZ.tar" };
    for (const char *f : files1)
    {
        const QString filePath = path + f;
        if (QFile::exists(filePath))
        {
            if (QFile::remove(filePath))
            {
                DBG_Printf(DBG_INFO, "backup: removed temporary file %s\n", qPrintable(filePath));
            }
            else
            {
                DBG_Printf(DBG_ERROR, "backup: failed to remove temporary file %s\n", qPrintable(filePath));
                return false;
            }
        }
    }

#ifdef Q_OS_LINUX
    // clean up old homebridge backup files
    QStringList filters;
    filters << "AccessoryInfo*";
    filters << "IdentifierCache*";

     QDir appDir(path);
     QStringList files = appDir.entryList(filters);

     for (QString f : files)
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
 #endif

    if (QFile::exists(path + QLatin1String("/deCONZ.tar.gz")))
    {
        // decompress .tar.gz
        QProcess *archProcess = new QProcess(this);

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
        archProcess = nullptr;
    }

    if (QFile::exists(path + QLatin1String("/deCONZ.tar")))
    {
        // unpack .tar
        QProcess *zipProcess = new QProcess(this);
#ifdef Q_OS_WIN
        QString appPath = qApp->applicationDirPath();
        QString cmd = appPath + "/7za.exe";
        QStringList args;
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
        zipProcess = nullptr;
    }

    bool ok = false;
    QVariantMap map;
    QFile file(path + QLatin1String("/deCONZ.conf"));
    if (file.open(QIODevice::ReadOnly))
    {
        const QString json = file.readAll();
        QVariant var = Json::parse(json, ok);
        if (ok)
        {
            map = var.toMap();
        }
    }

    const std::vector<const char*> requiredFields = {
        "deviceType", "panId", "extPanId", "apsUseExtPanId", "macAddress", "staticNwkAddress",
        "nwkAddress", "apsAck", "curChannel", "tcAddress", "securityMode", "networkKey", "tcLinkKey",
        "nwkUpdateId"
    };

    for (const auto *key : requiredFields)
    {
        if (!map.contains(QLatin1String(key)))
        {
            ok = false;
            break;
        }
    }

    if (ok) // all fields present
    {
        uint8_t deviceType = ok ? map["deviceType"].toUInt(&ok) : 0;
        if (ok && deviceType != deCONZ::Coordinator) { ok = false; } // only coordinator supported currently

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

        if (ok) // TODO as alternative load network configuration from zll.db file
        {
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

            // HA endpoint
            QVariantMap endpoint1;
            endpoint1["endpoint"] = QLatin1String("0x01");
            endpoint1["profileId"] = QLatin1String("0x0104");
            endpoint1["deviceId"] = QLatin1String("0x05");
            endpoint1["deviceVersion"] = QLatin1String("0x01");
            endpoint1["inClusters"] = QVariantList({ "0x0019", "0x000A"});
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

            if (gwZigbeeChannel != curChannel)
            {
                gwZigbeeChannel = curChannel;
                saveDatabaseItems |= DB_CONFIG;
            }
        }
    }

    //cleanup
    const std::vector<const char*> files2 = { "/deCONZ.conf", "/deCONZ.tar", "/deCONZ.targ.gz" };
    for (const char *f : files2)
    {
        const QString filePath = path + f;
        if (QFile::exists(filePath))
        {
            if (QFile::remove(filePath))
            {
                DBG_Printf(DBG_INFO, "backup: removed temporary file %s\n", qPrintable(filePath));
            }
            else
            {
                DBG_Printf(DBG_ERROR, "backup: failed to remove temporary file %s\n", qPrintable(filePath));
            }
        }
    }

    return ok;
}

/*! Reset the deCONZ network settings and/or delete database.
 */
bool DeRestPluginPrivate::resetConfiguration(bool resetGW, bool deleteDB)
{
    if (!apsCtrl)
    {
        return false;
    }

    // prevent overwrite database with content of current memory
    // will be reset after application soft restart
    ttlDataBaseConnection = 0;
    saveDatabaseItems |= DB_NOSAVE;
    closeDb();

    if (db)
    {
        DBG_Printf(DBG_ERROR, "backup: failed to import - database busy\n");
        return false; // database might be busy
    }

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

        if (macAddress == 0)
        {
            return false;
        }

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
    // deCONZ will be restarted after reconnect
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
    if (!sensor || sensor->deletedState() != Sensor::StateNormal || sensor->type().endsWith(QLatin1String("Battery")))
    {
        return;
    }

    Q_Q(DeRestPlugin);

    if (sensor->modelId().startsWith(QLatin1String("FLS-NB")))
    { } // use name from light
    else if (sensor->modelId().startsWith(QLatin1String("D1")) || sensor->modelId().startsWith(QLatin1String("S1")) ||
             sensor->modelId().startsWith(QLatin1String("S2")) ||sensor->modelId().startsWith(QLatin1String("lumi.ctrl_")))
    { } // use name from light
    else if (sensor->type() == QLatin1String("ZHAConsumption") || sensor->type() == QLatin1String("ZHAPower"))
    { } // use name from light
    else if (sensor->modelId().startsWith(QLatin1String("SML00")) && sensor->type() != QLatin1String("ZHAPresence"))
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
    DBG_Assert(apsCtrl != nullptr);

    if (!apsCtrl)
    {
        return;
    }

    if (pollManager->hasItems())
    {
        return;
    }

    if (q_ptr && !q_ptr->pluginActive())
    {
        return;
    }

    RestNodeBase *restNode = nullptr;

    while (!pollNodes.empty())
    {
        const auto pollItem = pollNodes.front();
        pollNodes.pop_front();

        if (pollItem.resourceType == RLights)
        {
            restNode = getLightNodeForId(pollItem.uuid);
        }
        else if (pollItem.resourceType == RSensors)
        {
            restNode = getSensorNodeForUniqueId(pollItem.uuid);
        }

        DBG_Assert(restNode);
        if (restNode && restNode->isAvailable())
        {
            break;
        }
        restNode = nullptr;
    }

    if (pollNodes.empty()) // TODO iter based
    {
        for (LightNode &l : nodes)
        {
            if (l.isAvailable() && l.address().ext() != gwDeviceAddress.ext() && l.state() == LightNode::StateNormal)
            {
                const PollNodeItem pollItem(l.uniqueId(), RLights);
                pollNodes.push_back(pollItem);
            }
        }

        for (Sensor &s : sensors)
        {
            if (s.isAvailable() && s.node() && s.node()->nodeDescriptor().receiverOnWhenIdle() && s.deletedState() == Sensor::StateNormal)
            {
                const PollNodeItem pollItem(s.uniqueId(), RSensors);
                pollNodes.push_back(pollItem);
            }
        }
    }

    if (restNode && restNode->isAvailable())
    {
        DBG_Printf(DBG_INFO_L2, "poll node %s\n", qPrintable(restNode->uniqueId()));
        pollManager->poll(restNode);
    }
}

/*! Request to disconnect from network.
 */
void DeRestPluginPrivate::genericDisconnectNetwork()
{
    DBG_Assert(apsCtrl != nullptr);

    if (!apsCtrl)
    {
        return;
    }

    networkDisconnectAttempts = NETWORK_ATTEMPS;
    networkConnectedBefore = gwRfConnectedExpected;
    networkState = DisconnectingNetwork;
    DBG_Printf(DBG_INFO_L2, "networkState: DisconnectingNetwork\n");

    apsCtrl->setNetworkState(deCONZ::NotInNetwork);

    startReconnectNetwork(RECONNECT_CHECK_DELAY);
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
            DBG_Assert(apsCtrl != nullptr);
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
    if (!reconnectTimer)
    {
        reconnectTimer = new QTimer(this);
        reconnectTimer->setSingleShot(true);
        connect(reconnectTimer, SIGNAL(timeout()),
                this, SLOT(reconnectTimerFired()));
    }

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
        // restart deCONZ to apply changes and reload database
        if (reconnectTimer)
        {
            reconnectTimer->stop();
        }

        if (needRestartApp)
        {
            qApp->exit(APP_RET_RESTART_APP);
        }
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
        DBG_Printf(DBG_INFO, "reconnect network failed, try later\n");
        networkState = MaintainNetwork;
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
