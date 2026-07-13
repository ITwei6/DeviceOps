cmake_minimum_required(VERSION 3.16)

project(DeviceOps VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Protobuf REQUIRED)
find_program(ODB_COMPILER odb REQUIRED)

get_filename_component(CPP_MICROSERVICE_KIT_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/cpp-microservice-kit"
    ABSOLUTE)

add_subdirectory(
    "${CPP_MICROSERVICE_KIT_DIR}"
    "${CMAKE_BINARY_DIR}/third_party/cpp-microservice-kit")

set(DEVICEOPS_PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/proto")
set(DEVICEOPS_PROTO_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/proto")

set(DEVICEOPS_PROTO_FILES
    "${DEVICEOPS_PROTO_DIR}/common.proto"
    "${DEVICEOPS_PROTO_DIR}/device_gateway.proto"
    "${DEVICEOPS_PROTO_DIR}/device.proto"
    "${DEVICEOPS_PROTO_DIR}/telemetry.proto"
    "${DEVICEOPS_PROTO_DIR}/event.proto"
    "${DEVICEOPS_PROTO_DIR}/log.proto"
    "${DEVICEOPS_PROTO_DIR}/diagnosis.proto"
    "${DEVICEOPS_PROTO_DIR}/knowledge.proto")

set(DEVICEOPS_PROTO_SRCS)
set(DEVICEOPS_PROTO_HDRS)

foreach(PROTO_FILE ${DEVICEOPS_PROTO_FILES})
    get_filename_component(PROTO_NAME "${PROTO_FILE}" NAME_WE)
    set(GENERATED_SRC "${DEVICEOPS_PROTO_GENERATED_DIR}/${PROTO_NAME}.pb.cc")
    set(GENERATED_HDR "${DEVICEOPS_PROTO_GENERATED_DIR}/${PROTO_NAME}.pb.h")

    add_custom_command(
        OUTPUT "${GENERATED_SRC}" "${GENERATED_HDR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DEVICEOPS_PROTO_GENERATED_DIR}"
        COMMAND ${Protobuf_PROTOC_EXECUTABLE}
            --proto_path=${DEVICEOPS_PROTO_DIR}
            --cpp_out=${DEVICEOPS_PROTO_GENERATED_DIR}
            "${PROTO_FILE}"
        DEPENDS ${DEVICEOPS_PROTO_FILES}
        COMMENT "Generating C++ protobuf source for ${PROTO_NAME}.proto"
        VERBATIM)

    list(APPEND DEVICEOPS_PROTO_SRCS "${GENERATED_SRC}")
    list(APPEND DEVICEOPS_PROTO_HDRS "${GENERATED_HDR}")
endforeach()

add_library(deviceops_proto STATIC
    ${DEVICEOPS_PROTO_SRCS}
    ${DEVICEOPS_PROTO_HDRS})

target_include_directories(deviceops_proto
    PUBLIC
        "${DEVICEOPS_PROTO_GENERATED_DIR}"
        ${Protobuf_INCLUDE_DIRS})

target_link_libraries(deviceops_proto
    PUBLIC
        ${Protobuf_LIBRARIES})

set(DEVICEOPS_ODB_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/odb")
set(DEVICEOPS_ODB_ENTITY_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/common/include/deviceops/db/device_entity.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/common/include/deviceops/db/event_entity.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/common/include/deviceops/db/knowledge_entity.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/common/include/deviceops/db/diagnosis_entity.h")

set(DEVICEOPS_ODB_SRCS)
set(DEVICEOPS_ODB_HDRS)

foreach(ODB_ENTITY_FILE ${DEVICEOPS_ODB_ENTITY_FILES})
    get_filename_component(ODB_ENTITY_NAME "${ODB_ENTITY_FILE}" NAME_WE)
    set(ODB_GENERATED_SRC "${DEVICEOPS_ODB_GENERATED_DIR}/${ODB_ENTITY_NAME}-odb.cxx")
    set(ODB_GENERATED_HDR "${DEVICEOPS_ODB_GENERATED_DIR}/${ODB_ENTITY_NAME}-odb.hxx")
    set(ODB_GENERATED_IXX "${DEVICEOPS_ODB_GENERATED_DIR}/${ODB_ENTITY_NAME}-odb.ixx")

    add_custom_command(
        OUTPUT "${ODB_GENERATED_SRC}" "${ODB_GENERATED_HDR}" "${ODB_GENERATED_IXX}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DEVICEOPS_ODB_GENERATED_DIR}"
        COMMAND ${ODB_COMPILER}
            -d mysql
            --std c++17
            --generate-query
            --generate-schema
            --schema-format embedded
            --output-dir "${DEVICEOPS_ODB_GENERATED_DIR}"
            -I "${CMAKE_CURRENT_SOURCE_DIR}/common/include"
            "${ODB_ENTITY_FILE}"
        DEPENDS "${ODB_ENTITY_FILE}"
        COMMENT "Generating ODB mapping for ${ODB_ENTITY_NAME}"
        VERBATIM)

    list(APPEND DEVICEOPS_ODB_SRCS "${ODB_GENERATED_SRC}")
    list(APPEND DEVICEOPS_ODB_HDRS "${ODB_GENERATED_HDR}")
endforeach()

add_library(deviceops_db STATIC
    common/src/odb_database.cc
    ${DEVICEOPS_ODB_SRCS}
    ${DEVICEOPS_ODB_HDRS})

target_include_directories(deviceops_db
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/common/include
        ${CMAKE_CURRENT_SOURCE_DIR}/common/include/deviceops/db
        "${DEVICEOPS_ODB_GENERATED_DIR}")

target_link_libraries(deviceops_db
    PUBLIC
        tew_scaffold)

add_subdirectory(services/device_gateway)
add_subdirectory(services/device_service)
add_subdirectory(services/telemetry_service)
add_subdirectory(services/event_service)
add_subdirectory(services/log_service)
add_subdirectory(services/knowledge_service)
add_subdirectory(services/diagnosis_service)
add_subdirectory(simulator)
