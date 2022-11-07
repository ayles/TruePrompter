add_subdirectory(proto)

add_library(trueprompter_common
    audio_codec.cpp
    audio_codec.hpp
    old_audio_codec.hpp
)

target_include_directories(trueprompter_common PUBLIC ${CMAKE_SOURCE_DIR})

target_link_libraries(trueprompter_common
    trueprompter_common_proto
    PkgConfig::LIBAV
)
