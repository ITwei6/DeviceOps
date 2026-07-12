cmake_minimum_required(VERSION 3.16)

project(DeviceOps VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Protobuf REQUIRED)

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

add_subdirectory(services/device_gateway)
add_subdirectory(services/device_service)
add_subdirectory(services/telemetry_service)
add_subdirectory(simulator)
