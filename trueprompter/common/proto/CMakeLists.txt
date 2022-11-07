protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS
    protocol.proto
    audio_codec.proto
)

add_library(trueprompter_common_proto
    ${PROTO_SRCS}
)

target_include_directories(trueprompter_common_proto PUBLIC ${CMAKE_BINARY_DIR})

target_link_libraries(trueprompter_common_proto
    PUBLIC protobuf::libprotobuf
)
