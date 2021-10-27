
add_library (utils
    utils.h
    utils.cpp
)

target_link_libraries(utils PUBLIC deconz_common)
