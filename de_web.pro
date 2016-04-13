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
}

win32:LIBS +=  -L../.. -ldeCONZ1
unix:LIBS +=  -L../.. -ldeCONZ
win32:CONFIG += dll

unix:!macx {
    LIBS += -lcrypt
}

TEMPLATE        = lib
CONFIG         += plugin \
               += debug_and_release

QT             += network

INCLUDEPATH    += ../.. \
                  ../../common
                  
# Version Major.Minor.Build
# Important: don't change the format of this line since it's parsed by scripts!
DEFINES += GW_SW_VERSION=\\\"2.03.25\\\"

# Minimum version of the RaspBee firmware
# which shall be used in order to support all features for this software release (case sensitive)
DEFINES += GW_MIN_RPI_FW_VERSION=0x26050500

# Minimum version of the deRFusb23E0X firmware
# which shall be used in order to support all features for this software release
DEFINES += GW_MIN_DERFUSB23E0X_FW_VERSION=0x22030300

unix:contains(QMAKE_HOST.arch, armv6l) {
    DEFINES += GW_DEFAULT_NAME=\\\"RaspBee-GW\\\"
}
else {
    DEFINES += GW_DEFAULT_NAME=\\\"deCONZ-GW\\\"
}

QMAKE_CXXFLAGS += -Wno-attributes

HEADERS  = bindings.h \
           de_web_plugin.h \
           de_web_widget.h \
           connectivity.h \
           json.h \
           colorspace.h \
           sqlite3.h \
           de_web_plugin_private.h \
           rest_node_base.h \
           light_node.h \
           group.h \
           group_info.h \
           rule.h \
           scene.h \
           sensor.h

SOURCES  = authentification.cpp \
           bindings.cpp \
           change_channel.cpp \
           connectivity.cpp \
           database.cpp \
           discovery.cpp \
           de_web_plugin.cpp \
           de_web_widget.cpp \
           de_otau.cpp \
           firmware_update.cpp \
           json.cpp \
           colorspace.cpp \
           sqlite3.c \
           rest_lights.cpp \
           rest_configuration.cpp \
           rest_groups.cpp \
           rest_rules.cpp \
           rest_sensors.cpp \
           rest_schedules.cpp \
           rest_touchlink.cpp \
           rule.cpp \
           upnp.cpp \
           zcl_tasks.cpp \
           gw_uuid.cpp \
           permitJoin.cpp \
           rest_node_base.cpp \
           light_node.cpp \
           group.cpp \
           group_info.cpp \
           scene.cpp \
           sensor.cpp \
           atmel_wsndemo_sensor.cpp \
           reset_device.cpp

win32:DESTDIR  = ../../debug/plugins # TODO adjust
unix:DESTDIR  = ..

FORMS += \
    de_web_widget.ui
