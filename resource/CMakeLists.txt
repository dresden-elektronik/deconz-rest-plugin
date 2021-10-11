
add_library (resource
    ../resource.h
    ../resource.cpp
    ../state_change.h
    ../state_change.cpp
)

target_link_libraries(resource PUBLIC deconz_common)

target_include_directories (resource PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
