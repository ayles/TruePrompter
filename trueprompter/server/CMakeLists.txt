add_executable(trueprompter_server
    client_context.hpp
    main.cpp
)

target_link_libraries(trueprompter_server
    trueprompter_recognition
    trueprompter_common
    websocketpp_complete
    spdlog
)

install(TARGETS trueprompter_server DESTINATION bin)

