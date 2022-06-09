add_executable(TruePrompter main.cpp model.cpp)
target_include_directories(TruePrompter PUBLIC ${openfst_SOURCE_DIR}/src/include)
target_include_directories(TruePrompter PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(TruePrompter
    span
    fst
    fstngram
    kaldi-fstext
    kaldi-nnet3
    kaldi-online
    kaldi-online2
    kaldi-rnnlm
    phonetisaurus
    OpenSSL::SSL
    websocketpp::websocketpp
    Boost::system
    TruePrompterProto
    BLAS::BLAS
    LAPACK::LAPACK
)
