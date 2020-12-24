TARGET   = de_rest_plugin

# common configuration for deCONZ plugins

TARGET = $$qtLibraryTarget($$TARGET)

DEFINES += DECONZ_DLLSPEC=Q_DECL_IMPORT

QMAKE_CXXFLAGS += -Wno-attributes \
                  -Wno-psabi \
                  -Wall

CONFIG(debug, debug|release) {
    LIBS += -L../../debug
}

CONFIG(release, debug|release) {
    LIBS += -L../../release
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

unix:LIBS +=  -L../.. -ldeCONZ

unix:!macx {
    LIBS += -lcrypt
}

TEMPLATE        = lib
CONFIG         += plugin \
               += debug_and_release \
               += c++11 \
               -= qtquickcompiler

QT             += network

INCLUDEPATH    += ../.. \
                  ../../common

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
DEFINES += GW_SW_VERSION=\\\"2.09.00\\\"
DEFINES += GW_SW_DATE=$$GIT_COMMIT_DATE
DEFINES += GW_API_VERSION=\\\"1.16.0\\\"
DEFINES += GIT_COMMMIT=\\\"$$GIT_COMMIT\\\"

# Minimum version of the RaspBee firmware
# which shall be used in order to support all features for this software release (case sensitive)
DEFINES += GW_AUTO_UPDATE_AVR_FW_VERSION=0x260b0500
DEFINES += GW_AUTO_UPDATE_R21_FW_VERSION=0x26420700
DEFINES += GW_MIN_AVR_FW_VERSION=0x26390500
DEFINES += GW_MIN_R21_FW_VERSION=0x26660700

# Minimum version of the deRFusb23E0X firmware
# which shall be used in order to support all features for this software release
DEFINES += GW_MIN_DERFUSB23E0X_FW_VERSION=0x22030300

DEFINES += GW_DEFAULT_NAME=\\\"Phoscon-GW\\\"

HEADERS  = bindings.h \
           connectivity.h \
           colorspace.h \
           daylight.h \
           de_web_plugin.h \
           de_web_plugin_private.h \
           de_web_widget.h \
           event.h \
           gateway.h \
           gateway_scanner.h \
           green_power.h \
           group.h \
           group_info.h \
           json.h \
           light_node.h \
           poll_control.h \
           poll_manager.h \
           read_files.h \
           resource.h \
           resourcelinks.h \
           rest_devices.h \
           rest_node_base.h \
           rule.h \
           scene.h \
           sensor.h \
           tuya.h \
           websocket_server.h

SOURCES  = air_quality.cpp \
           authorisation.cpp \
           bindings.cpp \
           change_channel.cpp \
           connectivity.cpp \
           colorspace.cpp \
           database.cpp \
           daylight.cpp \
           device_setup.cpp \
           diagnostics.cpp \
           discovery.cpp \
           de_web_plugin.cpp \
           de_web_widget.cpp \
           de_otau.cpp \
           event.cpp \
           event_queue.cpp \
           fan_control.cpp \
           firmware_update.cpp \
           gateway.cpp \
           gateway_scanner.cpp \
           green_power.cpp \
           group.cpp \
           group_info.cpp \
           gw_uuid.cpp \
           ias_zone.cpp \
           identify.cpp \
           json.cpp \
           light_node.cpp \
           poll_control.cpp \
           poll_manager.cpp \
           read_files.cpp \
           resource.cpp \
           resourcelinks.cpp \
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
           thermostat_ui_configuration.cpp \
           upnp.cpp \
           permitJoin.cpp \
           scene.cpp \
           sensor.cpp \
           thermostat.cpp \
           time.cpp \
           tuya.cpp \
           basic.cpp \
           appliances.cpp \
           reset_device.cpp \
           rest_userparameter.cpp \
           zcl_tasks.cpp \
           window_covering.cpp \
           websocket_server.cpp \
           xmas.cpp

win32 {

    OPENSSL_PATH = E:/Qt/Tools/OpenSSL/Win_x86

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
         -L$${PWD}/../../../lib/sqlite-dll-win32-x86-3240000 \
         -ldeCONZ1 \
         -lsqlite3

    INCLUDEPATH += $${PWD}/../../../lib/sqlite-amalgamation-3240000
    CONFIG += dll
}

win32:DESTDIR  = ../../debug/plugins # TODO adjust
unix:DESTDIR  = ..

FORMS += \
    de_web_widget.ui
