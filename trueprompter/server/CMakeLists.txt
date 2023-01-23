add_executable(trueprompter_server
    main.cpp
)

target_link_libraries(trueprompter_server
    trueprompter_recognition
    trueprompter_common
    trueprompter_codec
    websocketpp_complete
    spdlog
)

