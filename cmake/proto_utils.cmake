function(add_proto_library TARGET)
    cmake_parse_arguments(add_proto_library "" "" "" "${ARGN}")

    set(protobuf_generate_GENERATE_EXTENSIONS .pb.h .pb.cc)

    foreach(_proto ${add_proto_library_UNPARSED_ARGUMENTS})
        get_filename_component(_abs_file ${_proto} ABSOLUTE)
        get_filename_component(_abs_dir ${_abs_file} DIRECTORY)
        get_filename_component(_basename ${_proto} NAME_WLE)
        file(RELATIVE_PATH _rel_dir ${CMAKE_SOURCE_DIR} ${_abs_dir})
        set(_generated_srcs)
        foreach(_ext ${protobuf_generate_GENERATE_EXTENSIONS})
            list(APPEND _generated_srcs "${CMAKE_BINARY_DIR}/${_rel_dir}/${_basename}${_ext}")
        endforeach()

        add_custom_command(
            OUTPUT ${_generated_srcs}
            COMMAND protobuf::protoc
            ARGS --cpp_out ${CMAKE_BINARY_DIR} -I ${CMAKE_SOURCE_DIR} ${_abs_file}
            DEPENDS ${_abs_file} protobuf::protoc
            COMMENT "Running cpp protocol buffer compiler on ${_proto}"
            VERBATIM
        )
    endforeach()

    add_library(${TARGET} ${_generated_srcs})
    target_include_directories(${TARGET} PUBLIC ${CMAKE_BINARY_DIR})
    target_link_libraries(${TARGET} protobuf::libprotobuf)
endfunction()
