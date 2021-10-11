
add_library (zcl
    zcl.h
    zcl.cpp
)

target_link_libraries(zcl PUBLIC deconz_common)

target_include_directories (zcl PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
