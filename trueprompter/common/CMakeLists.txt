add_subdirectory(proto)

add_library(trueprompter_common INTERFACE)
target_include_directories(trueprompter_common INTERFACE ${CMAKE_BINARY_DIR})
target_link_libraries(trueprompter_common
    INTERFACE trueprompter_common_proto
)
