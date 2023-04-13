add_executable(trueprompter_server
    client_context.hpp
    main.cpp
)

target_link_libraries(trueprompter_server
    trueprompter_codec
    trueprompter_common
    trueprompter_recognition
    websocketpp_complete
    spdlog
)

install(TARGETS trueprompter_server DESTINATION bin)

