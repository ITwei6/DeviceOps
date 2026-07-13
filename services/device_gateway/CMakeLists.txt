add_executable(device_gateway
    src/downstream_forwarder.cc
    src/gateway_config.cc
    src/gateway_rpc_service.cc
    src/gateway_server.cc
    src/main.cc)

target_include_directories(device_gateway
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(device_gateway
    PRIVATE
        tew_scaffold
        deviceops_proto)
