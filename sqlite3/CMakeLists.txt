cmake_minimum_required(VERSION 3.13)

include(FetchContent)

FetchContent_Declare(sqlite3 URL "https://www.sqlite.org/2023/sqlite-amalgamation-3420000.zip")
if (NOT sqlite3_POPULATED)
    FetchContent_Populate(sqlite3)
endif()

add_library(sqlite3 SHARED ${sqlite3_SOURCE_DIR}/sqlite3.c)
add_library(SQLite::SQLite3 ALIAS sqlite3)

target_include_directories(sqlite3 PUBLIC ${sqlite3_SOURCE_DIR})


