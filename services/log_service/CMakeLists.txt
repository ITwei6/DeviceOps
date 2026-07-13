add_executable(log_service
    src/log_repository.cc
    src/log_service_impl.cc
    src/main.cc)

target_include_directories(log_service
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(log_service
    PRIVATE
        tew_scaffold
        deviceops_proto)
