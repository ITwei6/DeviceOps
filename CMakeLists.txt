cmake_minimum_required(VERSION 3.16)

project(DeviceOps VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(CPP_MICROSERVICE_KIT_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/cpp-microservice-kit"
    ABSOLUTE)

add_subdirectory(
    "${CPP_MICROSERVICE_KIT_DIR}"
    "${CMAKE_BINARY_DIR}/third_party/cpp-microservice-kit")

add_subdirectory(services/device_gateway)
