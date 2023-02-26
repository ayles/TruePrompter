add_executable(trueprompter_server_test
    client_context.hpp
    main.cpp
)

target_link_libraries(trueprompter_server_test
    trueprompter_recognition
    trueprompter_common
    websocketpp_complete
    spdlog
)

