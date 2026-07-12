add_executable(telemetry_service
    src/main.cc
    src/telemetry_repository.cc
    src/telemetry_service_impl.cc)

target_include_directories(telemetry_service
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(telemetry_service
    PRIVATE
        tew_scaffold
        deviceops_proto)
