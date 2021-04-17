
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
)

target_link_libraries(device
    PUBLIC deconz_common
    PUBLIC device_js
    PUBLIC event
    PUBLIC resource
    PUBLIC zdp
)

target_include_directories (device PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
