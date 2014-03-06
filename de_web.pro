TARGET   = de_rest_plugin

# common configuration for deCONZ plugins

TARGET = $$qtLibraryTarget($$TARGET)

DEFINES += DECONZ_DLLSPEC=Q_DECL_IMPORT

unix:contains(QMAKE_HOST.arch, armv6l) {
    DEFINES += ARCH_ARM ARCH_ARMV6
}

win32:LIBS+=  -L../.. -ldeCONZ1
unix:LIBS+=  -L../.. -ldeCONZ -lcrypt
win32:CONFIG += dll

TEMPLATE        = lib
CONFIG         += plugin \
               += debug_and_release

QT             += network

INCLUDEPATH    += ../.. \
                  ../../common
                  
unix:INCLUDEPATH += /usr/include

# Version Major.Minor.Build
# Important: don't change the format of this line since it's parsed by scripts!
DEFINES += GW_SW_VERSION=\\\"1.13.00\\\"

# Minimum version of the RaspBee firmware
# which shall be used in order to support all features for this software release (case sensitive)
DEFINES += GW_MIN_RPI_FW_VERSION=0x1a030500

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

HEADERS  = de_web_plugin.h \
           de_web_widget.h \
           json.h \
           colorspace.h \
           sqlite3.h \
           de_web_plugin_private.h \
           rest_node_base.h \
           light_node.h \
           group.h \
           group_info.h \
           scene.h

SOURCES  = authentification.cpp \
           change_channel.cpp \
           database.cpp \
           discovery.cpp \
           de_web_plugin.cpp \
           de_web_widget.cpp \
           de_otau.cpp \
           json.cpp \
           colorspace.cpp \
           sqlite3.c \
           rest_lights.cpp \
           rest_configuration.cpp \
           rest_groups.cpp \
           rest_schedules.cpp \
           rest_touchlink.cpp \
           upnp.cpp \
           zcl_tasks.cpp \
           gw_uuid.cpp \
           permitJoin.cpp \
           rest_node_base.cpp \
           light_node.cpp \
           group.cpp \
           group_info.cpp \
           scene.cpp

win32:DESTDIR  = ../../debug/plugins # TODO adjust
unix:DESTDIR  = ..

FORMS += \
    de_web_widget.ui
