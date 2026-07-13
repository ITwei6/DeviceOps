cmake_minimum_required(VERSION 3.16)

project(DeviceOpsQtClient VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 QUIET COMPONENTS Widgets Network)
if(Qt6_FOUND)
    set(QT_WIDGETS_TARGET Qt6::Widgets)
    set(QT_NETWORK_TARGET Qt6::Network)
else()
    find_package(Qt5 REQUIRED COMPONENTS Widgets Network)
    set(QT_WIDGETS_TARGET Qt5::Widgets)
    set(QT_NETWORK_TARGET Qt5::Network)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_executable(deviceops_qt_client
    src/main.cpp
    src/app_config.cpp
    src/app_config.h
    src/http_client.cpp
    src/http_client.h
    src/json_utils.cpp
    src/json_utils.h
    src/main_window.cpp
    src/main_window.h
)

target_link_libraries(deviceops_qt_client
    PRIVATE
        ${QT_WIDGETS_TARGET}
        ${QT_NETWORK_TARGET}
)

target_compile_definitions(deviceops_qt_client
    PRIVATE
        QT_NO_KEYWORDS
)

add_custom_command(TARGET deviceops_qt_client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/config.json"
        "$<TARGET_FILE_DIR:deviceops_qt_client>/config.json")
