add_executable(trueprompter_client
    main.cpp
    audio_source.cpp
    audio_source.hpp
)

target_include_directories(trueprompter_client PUBLIC ${CMAKE_SOURCE_DIR})

target_link_libraries(trueprompter_client
    trueprompter_common
    trueprompter_common_proto
    PortAudio
    websocketpp::websocketpp
)
