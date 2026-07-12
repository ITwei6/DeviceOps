add_executable(device_service
    src/device_repository.cc
    src/device_service_impl.cc
    src/main.cc)

target_include_directories(device_service
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(device_service
    PRIVATE
        tew_scaffold
        deviceops_proto)
