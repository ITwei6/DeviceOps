add_executable(diagnosis_service
    src/diagnosis_rag_client.cc
    src/diagnosis_repository.cc
    src/diagnosis_service_impl.cc
    src/main.cc)

target_include_directories(diagnosis_service
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(diagnosis_service
    PRIVATE
        deviceops_db
        deviceops_mq
        tew_scaffold
        deviceops_proto)
