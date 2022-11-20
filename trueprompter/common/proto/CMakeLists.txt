add_proto_library(trueprompter_common_proto
    protocol.proto
)

target_link_libraries(trueprompter_common_proto
    trueprompter_codec_proto
)
