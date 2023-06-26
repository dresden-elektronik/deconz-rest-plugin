TARGET   = de_rest_plugin

# common configuration for deCONZ plugins

TARGET = $$qtLibraryTarget($$TARGET)

DEFINES += DECONZ_DLLSPEC=Q_DECL_IMPORT

# select Javascript engine
#DEFINES += USE_QT_JS_ENGINE
DEFINES += USE_DUKTAPE_JS_ENGINE

QMAKE_CXXFLAGS += -Wno-attributes \
                  -Wno-psabi \
                  -Wall

CONFIG(debug, debug|release) {
    LIBS += -L../../debug
    DEFINES += DECONZ_DEBUG_BUILD
}

CONFIG(release, debug|release) {
    LIBS += -L../../release
}

equals(QT_MAJOR_VERSION, 5):lessThan(QT_MINOR_VERSION, 15) {
    DEFINES += SKIP_EMPTY_PARTS=QString::SkipEmptyParts
} else {
    DEFINES += SKIP_EMPTY_PARTS=Qt::SkipEmptyParts
}

greaterThan(QT_MAJOR_VERSION, 4) {
    QT += core gui widgets serialport

    greaterThan(QT_MINOR_VERSION, 2) {
        DEFINES += USE_WEBSOCKETS
        QT += websockets
    }
}

QMAKE_SPEC_T = $$[QMAKE_SPEC]

contains(QMAKE_SPEC_T,.*linux.*) {
    CONFIG += link_pkgconfig
    packagesExist(sqlite3) {
        DEFINES += HAS_SQLITE3
        PKGCONFIG += sqlite3
    }

    packagesExist(openssl) {
        DEFINES += HAS_OPENSSL
        #PKGCONFIG += openssl
    }
}

macx {
    DEFINES += QT_NO_DEPRECATED_WARNINGS
    CONFIG+=sdk_no_version_check

    LIBS += -lsqlite3
    DEFINES += HAS_SQLITE3
}

unix:LIBS +=  -L../.. -ldeCONZ

unix:!macx {
    LIBS += -lcrypt
}

TEMPLATE        = lib
CONFIG         += plugin \
               += debug_and_release \
               += c++14 \
               -= qtquickcompiler

QT             += network
#QT             += qml

INCLUDEPATH    += ../../lib

# TAG is specified by auto build system
# this is needed since non head versions which are checkedout and build
# will have a revision different to HEAD
GIT_TAG=$$TAG

isEmpty(GIT_TAG) {
    GIT_TAG=HEAD # default
}

GIT_COMMIT = $$system("git rev-list $$GIT_TAG --max-count=1")
GIT_COMMIT_DATE = $$system("git show -s --format=%ct $$GIT_TAG")

# Version Major.Minor.Build
# Important: don't change the format of this line since it's parsed by scripts!
DEFINES += GW_SW_VERSION=\\\"2.22.02\\\"
DEFINES += GW_SW_DATE=$$GIT_COMMIT_DATE
DEFINES += GW_API_VERSION=\\\"1.16.0\\\"
DEFINES += GIT_COMMMIT=\\\"$$GIT_COMMIT\\\"

# Minimum version of the RaspBee firmware
# which shall be used in order to support all features for this software release (case sensitive)
DEFINES += GW_AUTO_UPDATE_AVR_FW_VERSION=0x260b0500
DEFINES += GW_AUTO_UPDATE_R21_FW_VERSION=0x26420700
DEFINES += GW_MIN_AVR_FW_VERSION=0x26400500
DEFINES += GW_MIN_R21_FW_VERSION=0x26720700

# Minimum version of the deRFusb23E0X firmware
# which shall be used in order to support all features for this software release
DEFINES += GW_MIN_DERFUSB23E0X_FW_VERSION=0x22030300

DEFINES += GW_DEFAULT_NAME=\\\"Phoscon-GW\\\"

HEADERS  = bindings.h \
           air_quality.h \
           alarm_system.h \
           alarm_system_device_table.h \
           alarm_system_event_handler.h \
           aps_controller_wrapper.h \
           backup.h \
           button_maps.h \
           connectivity.h \
           colorspace.h \
           crypto/mmohash.h \
           crypto/random.h \
           crypto/scrypt.h \
           database.h \
           daylight.h \
           de_web_plugin.h \
           de_web_plugin_private.h \
           de_web_widget.h \
           device.h \
           device_access_fn.h \
           device_compat.h \
           device_ddf_init.h \
           device_descriptions.h \
           device_js/device_js.h \
           device_js/device_js_duktape.h \
           device_js/device_js_wrappers.h \
           device_tick.h \
           event.h \
           event_emitter.h \
           fan_control.h \
           gateway.h \
           gateway_scanner.h \
           green_power.h \
           group.h \
           group_info.h \
           json.h \
           ias_ace.h \
           ias_zone.h \
           light_node.h \
           poll_control.h \
           poll_manager.h \
           product_match.h \
           read_files.h \
           resource.h \
           resourcelinks.h \
           rest_alarmsystems.h \
           rest_devices.h \
           rest_node_base.h \
           rule.h \
           scene.h \
           sensor.h \
           state_change.h \
           simple_metering.h \
           thermostat.h \
           thermostat_ui_configuration.h \
           tuya.h \
           ui/ddf_bindingeditor.h \
           ui/ddf_editor.h \
           ui/ddf_itemeditor.h \
           ui/ddf_itemlist.h \
           ui/ddf_treeview.h \
           ui/device_widget.h \
           ui/text_lineedit.h \
           utils/bufstring.h \
           utils/stringcache.h \
           utils/utils.h \
           websocket_server.h \
           xiaomi.h \
           zcl/zcl.h \
           zdp/zdp.h \
           zdp/zdp_handlers.h

SOURCES  = air_quality.cpp \
           alarm_system.cpp \
           alarm_system_device_table.cpp \
           alarm_system_event_handler.cpp \
           aps_controller_wrapper.cpp \
           authorisation.cpp \
           backup.cpp \
           bindings.cpp \
           button_maps.cpp \
           change_channel.cpp \
           connectivity.cpp \
           colorspace.cpp \
           crypto/mmohash.cpp \
           crypto/random.cpp \
           crypto/scrypt.cpp \
           database.cpp \
           daylight.cpp \
           device.cpp \
           device_access_fn.cpp \
           device_compat.cpp \
           device_ddf_init.cpp \
           device_descriptions.cpp \
           device_js/device_js.cpp \
           device_js/device_js_duktape.cpp \
           device_js/device_js_wrappers.cpp \
           device_js/duktape.c \
           device_setup.cpp \
           device_tick.cpp \
           diagnostics.cpp \
           discovery.cpp \
           de_web_plugin.cpp \
           de_web_widget.cpp \
           de_otau.cpp \
           electrical_measurement.cpp \
           event.cpp \
           event_emitter.cpp \
           event_queue.cpp \
           fan_control.cpp \
           firmware_update.cpp \
           gateway.cpp \
           gateway_scanner.cpp \
           green_power.cpp \
           group.cpp \
           group_info.cpp \
           gw_uuid.cpp \
           hue.cpp \
           ias_ace.cpp \
           ias_zone.cpp \
           identify.cpp \
           json.cpp \
           light_node.cpp \
           occupancy_sensing.cpp \
           poll_control.cpp \
           poll_manager.cpp \
           power_configuration.cpp \
           product_match.cpp \
           read_files.cpp \
           resource.cpp \
           resourcelinks.cpp \
           rest_alarmsystems.cpp \
           rest_configuration.cpp \
           rest_devices.cpp \
           rest_gateways.cpp \
           rest_groups.cpp \
           rest_lights.cpp \
           rest_node_base.cpp \
           rest_resourcelinks.cpp \
           rest_rules.cpp \
           rest_sensors.cpp \
           rest_schedules.cpp \
           rest_touchlink.cpp \
           rest_scenes.cpp \
           rest_info.cpp \
           rest_capabilities.cpp \
           rule.cpp \
           state_change.cpp \
           thermostat_ui_configuration.cpp \
           ui/ddf_bindingeditor.cpp \
           ui/ddf_editor.cpp \
           ui/ddf_itemeditor.cpp \
           ui/ddf_itemlist.cpp \
           ui/ddf_treeview.cpp \
           ui/device_widget.cpp \
           ui/text_lineedit.cpp \
           upnp.cpp \
           permitJoin.cpp \
           scene.cpp \
           sensor.cpp \
           simple_metering.cpp \
           thermostat.cpp \
           time.cpp \
           tuya.cpp \
           basic.cpp \
           appliances.cpp \
           reset_device.cpp \
           rest_userparameter.cpp \
           utils/bufstring.cpp \
           utils/stringcache.cpp \
           utils/utils.cpp \
           xiaomi.cpp \
           window_covering.cpp \
           websocket_server.cpp \
           xmas.cpp \
           zcl/zcl.cpp \
           zcl_tasks.cpp \
           zdp/zdp.cpp \
           zdp/zdp_handlers.cpp

win32 {

    OPENSSL_PATH = C:/Qt/Tools/OpenSSL/Win_x86

    exists($$OPENSSL_PATH) {
        message(OpenSLL detected $$OPENSSL_PATH)

        #LIBS += -L$$OPENSSL_PATH/bin \
        #     -llibcrypto-1_1 \
        #     -llibssl-1_1
        INCLUDEPATH += $$OPENSSL_PATH/include
        DEFINES += HAS_OPENSSL
    }

    LIBS += \
         -L../.. \
         -L$${PWD}/../../../lib/sqlite-dll-win32-x86-3270200 \
         -ldeCONZ1 \
         -lsqlite3

    INCLUDEPATH += $${PWD}/../../../lib/sqlite-amalgamation-3270200
    CONFIG += dll
}

win32:DESTDIR  = ../../debug/plugins # TODO adjust
unix:DESTDIR  = ..

FORMS += \
    de_web_widget.ui \
    ui/ddf_editor.ui \
    ui/device_widget.ui
