add_library(trueprompter_server_cxx17_lib
    recognizer.hpp
    recognizer.cpp
)

target_include_directories(trueprompter_server_cxx17_lib
    PUBLIC ${openfst_SOURCE_DIR}/src/include
    PUBLIC ${CMAKE_SOURCE_DIR}
)

target_link_libraries(trueprompter_server_cxx17_lib
    fst
    fstngram
    kaldi-fstext
    kaldi-nnet3
    kaldi-online
    kaldi-online2
    kaldi-rnnlm
    phonetisaurus
    BLAS::BLAS
    LAPACK::LAPACK
)

set_property(TARGET trueprompter_server_cxx17_lib PROPERTY CXX_STANDARD 17)

add_executable(trueprompter_server
    main.cpp
    matcher.hpp
    smith_waterman.hpp
)

target_include_directories(trueprompter_server PUBLIC ${CMAKE_SOURCE_DIR})

target_link_libraries(trueprompter_server
    trueprompter_server_cxx17_lib
    trueprompter_common
    trueprompter_common_proto
    OpenSSL::SSL
    websocketpp::websocketpp
    Boost::system
    spdlog
)
