add_executable(knowledge_service
    src/knowledge_repository.cc
    src/knowledge_service_impl.cc
    src/rag_client.cc
    src/main.cc)

target_include_directories(knowledge_service
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(knowledge_service
    PRIVATE
        deviceops_db
        deviceops_mq
        tew_scaffold
        deviceops_proto)
