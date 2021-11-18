
add_library (zdp
    zdp.h
    zdp.cpp
)

target_link_libraries(zdp PUBLIC deconz_common)

target_include_directories (zdp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
