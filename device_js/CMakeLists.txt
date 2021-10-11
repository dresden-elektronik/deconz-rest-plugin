
add_library (device_js
    device_js.h
    device_js.cpp
    device_js_wrappers.h
    device_js_wrappers.cpp
)

target_link_libraries(device_js
    PUBLIC deconz_common
    PUBLIC resource
)

target_include_directories (device_js PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
