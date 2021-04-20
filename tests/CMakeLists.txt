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

add_executable(001-device 001-device-1.cpp)
add_executable(101-resourceitem-dt-time 101-resourceitem-dt-time.cpp)
add_executable(201-device-js 201-device-js.cpp)

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

add_test(001-device 001-device)
add_test(101-resourceitem-dt-time 101-resourceitem-dt-time)
add_test(201-device-js 201-device-js)
