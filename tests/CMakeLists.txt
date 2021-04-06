cmake_minimum_required(VERSION 3.14)

project(qt_cmake1 LANGUAGES CXX)

Include(FetchContent)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(DECONZ_INCLUDE_DIR ../../../../../deconz-lib/include)

find_package(QT NAMES Qt5 COMPONENTS Core Gui SerialPort Widgets REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Gui SerialPort Widgets REQUIRED)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v2.13.1)

FetchContent_MakeAvailable(Catch2)

add_compile_definitions(
    DECONZ_DLLSPEC=
)

# Hacky support for compiling the impl into a static lib
add_library(Catch2WithMain 000-catch2-main.cpp)
target_link_libraries(Catch2WithMain PUBLIC Catch2)
add_library(Catch2::Catch2WithMain ALIAS Catch2WithMain)

# These examples use the standard separate compilation
add_executable( 001-device
    001-device-1.cpp
    ../resource.cpp
    ../device.cpp
    ../zdp.cpp
)

#string( REPLACE ".cpp" "" BASENAMES_TEST_CASES "${SOURCES_TEST_CASES}" )
#set( TARGETS_TEST_CASES ${BASENAMES_TEST_CASES} )

set( TARGETS_TEST_CASES 001-device )

foreach( name ${TARGETS_TEST_CASES} )
    target_link_libraries( ${name}
        PRIVATE Qt${QT_VERSION_MAJOR}::Core
        PRIVATE Qt${QT_VERSION_MAJOR}::Gui
        PRIVATE Qt${QT_VERSION_MAJOR}::Widgets
        PRIVATE Catch2::Catch2
        PRIVATE Catch2::Catch2WithMain
        -ldeCONZ
    )
    target_include_directories(${name} PRIVATE ${DECONZ_INCLUDE_DIR})
endforeach()
