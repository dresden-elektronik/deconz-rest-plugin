cmake_minimum_required(VERSION 3.13)

include(FetchContent)

if(${CMAKE_VERSION} VERSION_GREATER "3.23.0")
    cmake_policy(SET CMP0135 NEW)
endif()

FetchContent_GetProperties(sqlite3)
if (NOT sqlite3_POPULATED)
    FetchContent_Populate(sqlite3
        URL "https://www.sqlite.org/2023/sqlite-amalgamation-3420000.zip"
        URL_HASH MD5=eb9a6e56044bc518e6705521a1a929ed)
endif()

add_library(sqlite3 SHARED ${sqlite3_SOURCE_DIR}/sqlite3.c)
add_library(SQLite::SQLite3 ALIAS sqlite3)

target_include_directories(sqlite3 PUBLIC ${sqlite3_SOURCE_DIR})
