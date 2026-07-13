add_executable(rabbitmq_queue_probe
    main.cc)

target_link_libraries(rabbitmq_queue_probe
    PRIVATE
        tew_scaffold)
