add_subdirectory(proto)

add_library(trueprompter_codec
    audio_codec.cpp
    audio_codec.hpp
    av_audio_codec.hpp
    resampler.hpp
)

target_include_directories(trueprompter_codec PUBLIC ${CMAKE_SOURCE_DIR})

target_link_libraries(trueprompter_codec
    SampleRate::samplerate
    avcpp::avcpp-static
    trueprompter_codec_proto
)
