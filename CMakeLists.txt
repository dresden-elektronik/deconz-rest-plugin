cmake_minimum_required(VERSION 3.14)

project(rest_api LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(DECONZ_INCLUDE_DIR ../../../../deconz-lib/include)

find_package(QT NAMES Qt5 COMPONENTS Core Gui SerialPort Widgets Qml REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Gui SerialPort Widgets Qml REQUIRED)

add_compile_definitions(
    DECONZ_DLLSPEC=
)

if(QT_VERSION VERSION_LESS "5.15.0")
    add_compile_definitions( SKIP_EMPTY_PARTS=QString::SkipEmptyParts )
else()
    add_compile_definitions( SKIP_EMPTY_PARTS=Qt::SkipEmptyParts )
endif()

add_library(deconz_common INTERFACE)
target_include_directories(deconz_common INTERFACE ${DECONZ_INCLUDE_DIR})

target_link_libraries(deconz_common
        INTERFACE Qt${QT_VERSION_MAJOR}::Core
        INTERFACE Qt${QT_VERSION_MAJOR}::Gui
        INTERFACE Qt${QT_VERSION_MAJOR}::Widgets
        INTERFACE Qt${QT_VERSION_MAJOR}::Qml
        -ldeCONZ
)

add_subdirectory (resource)
add_subdirectory (device_js)

enable_testing()

add_subdirectory (tests)
