add_executable(event_service
    src/event_repository.cc
    src/event_service_impl.cc
    src/main.cc)

target_include_directories(event_service
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(event_service
    PRIVATE
        deviceops_db
        deviceops_mq
        tew_scaffold
        deviceops_proto)
