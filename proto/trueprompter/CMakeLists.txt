protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS
    protocol.proto
)

add_library(TruePrompterProto ${PROTO_SRCS})
target_include_directories(TruePrompterProto PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/..)
target_link_libraries(TruePrompterProto PUBLIC protobuf::libprotobuf)