add_executable(robot_device_simulator
    src/main.cc
    src/robot_device_simulator.cc)

target_include_directories(robot_device_simulator
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(robot_device_simulator
    PRIVATE
        tew_scaffold)
