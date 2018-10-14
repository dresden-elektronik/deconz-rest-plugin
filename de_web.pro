TARGET   = de_rest_plugin

# common configuration for deCONZ plugins

TARGET = $$qtLibraryTarget($$TARGET)

DEFINES += DECONZ_DLLSPEC=Q_DECL_IMPORT

unix:contains(QMAKE_HOST.arch, armv6l) {
    DEFINES += ARCH_ARM ARCH_ARMV6
}
unix:contains(QMAKE_HOST.arch, armv7l) {
    DEFINES += ARCH_ARM ARCH_ARMV7
}

QMAKE_CXXFLAGS += -Wno-attributes \
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

GIT_COMMIT = $$system("git rev-list HEAD --max-count=1")

# Version Major.Minor.Build
# Important: don't change the format of this line since it's parsed by scripts!
DEFINES += GW_SW_VERSION=\\\"2.05.40\\\"
DEFINES += GW_API_VERSION=\\\"1.16.0\\\"
DEFINES += GIT_COMMMIT=\\\"$$GIT_COMMIT\\\" \

# Minimum version of the RaspBee firmware
# which shall be used in order to support all features for this software release (case sensitive)
DEFINES += GW_AUTO_UPDATE_FW_VERSION=0x260b0500
DEFINES += GW_MIN_RPI_FW_VERSION=0x26240500

# Minimum version of the deRFusb23E0X firmware
# which shall be used in order to support all features for this software release
DEFINES += GW_MIN_DERFUSB23E0X_FW_VERSION=0x22030300

DEFINES += GW_DEFAULT_NAME=\\\"Phoscon-GW\\\"

QMAKE_CXXFLAGS += -Wno-attributes

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
           group.h \
           group_info.h \
           json.h \
           light_node.h \
           poll_manager.h \
           resource.h \
           resourcelinks.h \
           rest_node_base.h \
           rule.h \
           scene.h \
           sensor.h \
           websocket_server.h

SOURCES  = authentification.cpp \
           atmel_wsndemo_sensor.cpp \
           bindings.cpp \
           change_channel.cpp \
           connectivity.cpp \
           colorspace.cpp \
           database.cpp \
           daylight.cpp \
           discovery.cpp \
           de_web_plugin.cpp \
           de_web_widget.cpp \
           de_otau.cpp \
           event.cpp \
           event_queue.cpp \
           firmware_update.cpp \
           gateway.cpp \
           gateway_scanner.cpp \
           group.cpp \
           group_info.cpp \
           gw_uuid.cpp \
           ias_zone.cpp \
           json.cpp \
           light_node.cpp \
           poll_manager.cpp \
           resource.cpp \
           resourcelinks.cpp \
           rest_configuration.cpp \
           rest_gateways.cpp \
           rest_groups.cpp \
           rest_lights.cpp \
           rest_node_base.cpp \
           rest_resourcelinks.cpp \
           rest_rules.cpp \
           rest_sensors.cpp \
           rest_schedules.cpp \
           rest_touchlink.cpp \
           rule.cpp \
           upnp.cpp \
           permitJoin.cpp \
           scene.cpp \
           sensor.cpp \
           time.cpp \
           reset_device.cpp \
           rest_userparameter.cpp \
           zcl_tasks.cpp \
           window_covering.cpp \
           websocket_server.cpp

win32 {

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
