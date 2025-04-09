cmake_minimum_required(VERSION 3.13)

project(de_rest_plugin VERSION 2.29.03 LANGUAGES C;CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 COMPONENTS Core Widgets Network WebSockets REQUIRED)
find_package(Git REQUIRED)

if (UNIX)
    include(FindPkgConfig)
    pkg_check_modules (OPENSSL openssl)
else()
    find_package(OpenSSL REQUIRED)
endif()

if (NOT DECONZ_FULL_BUILD)
    # standalone build
    add_subdirectory(sqlite3)

#--- deCONZ library ---------------------------------------------------

include(FetchContent)

FetchContent_Declare(
  deconzlib
  GIT_REPOSITORY https://github.com/dresden-elektronik/deconz-lib.git
  GIT_TAG        main
)
# FetchContent_MakeAvailable(deconzlib)
# FetchContent_MakeAvailable requires CMake 3.14, but Debian Buster has only 3.13
FetchContent_GetProperties(deconzlib)
if (NOT deconzlib_POPULATED)
    FetchContent_Populate(deconzlib)
    add_subdirectory(${deconzlib_SOURCE_DIR} ${deconzlib_BINARY_DIR})
endif()

endif()

#----------------------------------------------------------------------

set(PLUGIN_INCLUDE_FILES
    air_quality.h
    alarm_system.h
    alarm_system_device_table.h
    alarm_system_event_handler.h
    aps_controller_wrapper.h
    backup.h
    bindings.h
    button_maps.h
    colorspace.h
    crypto/mmohash.h
    crypto/password.h
    crypto/random.h
    crypto/scrypt.h
    database.h
    daylight.h
    de_web_plugin.h
    de_web_plugin_private.h
    de_web_widget.h
    device.h
    device_access_fn.h
    device_compat.h
    device_ddf_bundle.h
    device_ddf_init.h
    device_descriptions.h
    device_tick.h
    device_js/device_js.h
    event.h
    event_emitter.h
    fan_control.h
    gateway.h
    gateway_scanner.h
    green_power.h
    group.h
    group_info.h
    ias_ace.h
    ias_zone.h
    json.h
    light_node.h
    poll_control.h
    poll_manager.h
    product_match.h
    read_files.h
    resource.h
    resourcelinks.h
    rest_alarmsystems.h
    rest_api.h
    rest_ddf.h
    rest_devices.h
    rest_node_base.h
    rule.h
    scene.h
    sensor.h
    simple_metering.h
    state_change.h
    thermostat.h
    thermostat_ui_configuration.h
    tuya.h
    ui/ddf_bindingeditor.h
    ui/ddf_editor.h
    ui/ddf_itemeditor.h
    ui/ddf_itemlist.h
    ui/ddf_treeview.h
    ui/device_widget.h
    ui/text_lineedit.h
    utils/ArduinoJson.h
    utils/ArduinoJson-v6.19.4.h
    utils/bufstring.h
    utils/scratchmem.h
    utils/stringcache.h
    utils/utils.h
    websocket_server.h
    xiaomi.h
    zcl/zcl.h
    zdp/zdp.h
    zdp/zdp_handlers.h
    )

add_library(${PROJECT_NAME} SHARED
    ${PLUGIN_INCLUDE_FILES}

    de_web_widget.ui
    ui/ddf_editor.ui
    ui/device_widget.ui

    air_quality.cpp
    alarm_system.cpp
    alarm_system_device_table.cpp
    alarm_system_event_handler.cpp
    appliances.cpp
    aps_controller_wrapper.cpp
    authorisation.cpp
    backup.cpp
    basic.cpp
    bindings.cpp
    button_maps.cpp
    change_channel.cpp
    colorspace.cpp
    crypto/mmohash.cpp
    crypto/password.cpp
    crypto/random.cpp
    crypto/scrypt.cpp
    cj/cj_all.c
    database.cpp
    daylight.cpp
    de_otau.cpp
    device_access_fn.cpp
    device_compat.cpp
    device.cpp
    device_ddf_bundle.cpp
    device_ddf_init.cpp
    device_descriptions.cpp
    device_js/duktape.c
    device_js/device_js_duktape.cpp
    device_tick.cpp
    de_web_plugin.cpp
    de_web_widget.cpp
    discovery.cpp
    electrical_measurement.cpp
    event.cpp
    event_emitter.cpp
    event_queue.cpp
    fan_control.cpp
    firmware_update.cpp
    gateway.cpp
    gateway_scanner.cpp
    green_power.cpp
    group.cpp
    group_info.cpp
    gw_uuid.cpp
    hue.cpp
    ias_ace.cpp
    ias_zone.cpp
    identify.cpp
    json.cpp
    light_node.cpp
    occupancy_sensing.cpp
    permitJoin.cpp
    poll_control.cpp
    poll_manager.cpp
    power_configuration.cpp
    product_match.cpp
    read_files.cpp
    reset_device.cpp
    resource.cpp
    resourcelinks.cpp
    rest_alarmsystems.cpp
    rest_api.cpp
    rest_capabilities.cpp
    rest_configuration.cpp
    rest_ddf.cpp
    rest_devices.cpp
    rest_gateways.cpp
    rest_groups.cpp
    rest_info.cpp
    rest_lights.cpp
    rest_node_base.cpp
    rest_resourcelinks.cpp
    rest_rules.cpp
    rest_scenes.cpp
    rest_schedules.cpp
    rest_sensors.cpp
    rest_touchlink.cpp
    rest_userparameter.cpp
    rule.cpp
    scene.cpp
    sensor.cpp
    simple_metering.cpp
    state_change.cpp
    thermostat.cpp
    thermostat_ui_configuration.cpp
    time.cpp
    tuya.cpp
    ui/ddf_bindingeditor.cpp
    ui/ddf_editor.cpp
    ui/ddf_itemeditor.cpp
    ui/ddf_itemlist.cpp
    ui/ddf_treeview.cpp
    ui/device_widget.cpp
    ui/text_lineedit.cpp
    upnp.cpp
    utils/bufstring.cpp
    utils/scratchmem.cpp
    utils/stringcache.cpp
    utils/utils.cpp
    websocket_server.cpp
    window_covering.cpp
    xiaomi.cpp
    xmas.cpp
    zcl_tasks.cpp
    zcl/zcl.cpp
    zdp/zdp.cpp
    zdp/zdp_handlers.cpp
    )

if (Git_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} log -1 --format=%at
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        OUTPUT_VARIABLE GIT_DATE
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-list HEAD --max-count=1
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        OUTPUT_VARIABLE SRC_GIT_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
    USE_WEBSOCKETS=1
    USE_DUKTAPE_JS_ENGINE=1
    USE_ULIB_SHARED=1

    GW_SW_VERSION="\"\"${PROJECT_VERSION}\"\""
    GW_SW_DATE=${GIT_DATE}
    GW_API_VERSION="\"\"1.16.0\"\""
    GIT_COMMMIT="\"\"${SRC_GIT_COMMIT}\"\""

    # Minimum version of the RaspBee firmware
    # which shall be used in order to support all features for this software release (case sensitive)
    GW_AUTO_UPDATE_AVR_FW_VERSION=0x260b0500
    GW_AUTO_UPDATE_R21_FW_VERSION=0x26420700
    GW_MIN_AVR_FW_VERSION=0x26400500
    GW_MIN_R21_FW_VERSION=0x26720700

    # Minimum version of the deRFusb23E0X firmware
    # which shall be used in order to support all features for this software release
    GW_MIN_DERFUSB23E0X_FW_VERSION=0x22030300
    GW_DEFAULT_NAME="\"\"Phoscon-GW\"\""
)

target_include_directories(${PROJECT_NAME} PRIVATE cj)

if (OPENSSL_FOUND)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAS_OPENSSL=1)
    if (APPLE)
        target_include_directories(${PROJECT_NAME} PRIVATE ${OPENSSL_INCLUDE_DIRS})
    else()
        target_include_directories(${PROJECT_NAME} PRIVATE ${OPENSSL_INCLUDE_DIR})
    endif()
endif()

if (Qt5Core_VERSION_STRING VERSION_LESS "5.15.0")
    target_compile_definitions(${PROJECT_NAME} PRIVATE SKIP_EMPTY_PARTS=QString::SkipEmptyParts )
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE SKIP_EMPTY_PARTS=Qt::SkipEmptyParts )
endif()

target_link_libraries(${PROJECT_NAME}
    PRIVATE Qt5::Core
    PRIVATE Qt5::Gui
    PRIVATE Qt5::Widgets
    PRIVATE Qt5::Network
    PRIVATE Qt5::WebSockets
    PRIVATE SQLite::SQLite3
    PRIVATE deCONZLib
)

#--------------------------------------------------------------
include(GNUInstallDirs)

if (UNIX)
    if (APPLE)
        set_target_properties(${PROJECT_NAME} PROPERTIES
            INSTALL_RPATH @loader_path)
    else()
        set_target_properties(${PROJECT_NAME} PROPERTIES
            INSTALL_RPATH "${CMAKE_INSTALL_FULL_LIBDIR}")
    endif()
endif()


if (${CMAKE_HOST_SYSTEM_NAME} MATCHES "Linux")
    install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/deCONZ/plugins"
    )

    # following applies only when build as part of deCONZ package
    if (DECONZ_FULL_BUILD)
        install(FILES "general.xml" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/deCONZ/zcl")
        install(DIRECTORY devices DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/deCONZ")
        install(FILES "button_maps.json" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/deCONZ/devices")

        install(DIRECTORY "linux/deCONZ/lib/systemd" DESTINATION "/lib")
        install(
            DIRECTORY "linux/deCONZ/usr/bin/"
            DESTINATION ${CMAKE_INSTALL_BINDIR}
            FILES_MATCHING
            PATTERN "*.sh"
        )
    endif()
endif()

#add_library(deconz_common INTERFACE)
#target_include_directories(deconz_common INTERFACE ${DECONZ_INCLUDE_DIR})
#target_link_directories(deconz_common INTERFACE /usr/local/lib)
#target_link_libraries(deconz_common
 #       INTERFACE Qt${QT_VERSION_MAJOR}::Core
#        INTERFACE Qt${QT_VERSION_MAJOR}::Gui
#        INTERFACE Qt${QT_VERSION_MAJOR}::Widgets
#        INTERFACE Qt${QT_VERSION_MAJOR}::Qml
#        deCONZ.so
#)

#add_subdirectory (resource)
#add_subdirectory (device_js)
#add_subdirectory (device)
#add_subdirectory (utils)
#add_subdirectory (zcl)
#add_subdirectory (zdp)

#enable_testing()

#add_subdirectory (tests)
