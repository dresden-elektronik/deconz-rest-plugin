cmake_minimum_required(VERSION 3.14)

project(tests)

Include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v2.13.1)

FetchContent_MakeAvailable(Catch2)

# Compile the catch main() impl into a static lib
add_library(Catch2WithMain 000-catch2-main.cpp)
target_link_libraries(Catch2WithMain PUBLIC Catch2)
add_library(Catch2::Catch2WithMain ALIAS Catch2WithMain)
target_compile_definitions(Catch2WithMain PUBLIC CATCH_CONFIG_ENABLE_BENCHMARKING)

add_executable(001-device 001-device-1.cpp)
add_executable(101-resourceitem-dt-time 101-resourceitem-dt-time.cpp)
add_executable(201-device-js 201-device-js.cpp)
add_executable(301-utils-mappedval 301-utils-mappedval.cpp)
add_executable(302-http-header 302-http-header.cpp)
add_executable(303-timeref 303-timeref.cpp)

target_link_libraries(001-device
    PRIVATE device
    PRIVATE utils
    PRIVATE Catch2::Catch2
    PRIVATE Catch2::Catch2WithMain
)

target_link_libraries(101-resourceitem-dt-time
    PRIVATE resource
    PRIVATE Catch2::Catch2
    PRIVATE Catch2::Catch2WithMain
)

target_link_libraries(201-device-js
    PRIVATE device_js
    PRIVATE Catch2::Catch2
    PRIVATE Catch2::Catch2WithMain
)

target_link_libraries(301-utils-mappedval
    PRIVATE device
    PRIVATE utils
    PRIVATE Catch2::Catch2
    PRIVATE Catch2::Catch2WithMain
)

target_link_libraries(302-http-header
    PRIVATE device
    PRIVATE Catch2::Catch2
    PRIVATE Catch2::Catch2WithMain
)

target_link_libraries(303-timeref
    PRIVATE device
    PRIVATE Catch2::Catch2
    PRIVATE Catch2::Catch2WithMain
)


add_test(001-device 001-device)
add_test(101-resourceitem-dt-time 101-resourceitem-dt-time)
add_test(201-device-js 201-device-js)
add_test(301-utils-mappedval 301-utils-mappedval)
add_test(302-http-header 301-http-header)
add_test(303-timeref 303-timeref)
