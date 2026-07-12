add_executable(device_gateway
    src/gateway_config.cc
    src/gateway_server.cc
    src/main.cc)

target_include_directories(device_gateway
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(device_gateway
    PRIVATE
        tew_scaffold
        deviceops_proto)
