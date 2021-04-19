
add_library (event
    ../event.h
    ../event.cpp
)

target_link_libraries(event
    PRIVATE resource
)

target_include_directories (event PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)


add_library (device
    ../device.h
    ../device.cpp
    ../device_access_fn.h
    ../device_access_fn.cpp
    ../device_descriptions.h
    ../device_descriptions.cpp
    ../device_ddf_init.h
    ../device_ddf_init.cpp
)

target_link_libraries(device
    PUBLIC deconz_common
    PUBLIC device_js
    PUBLIC event
    PUBLIC resource
    PUBLIC zcl
    PUBLIC zdp
)

target_include_directories (device PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
