add_library(trueprompter_recognition_cxx17
    kaldi/kaldi.hpp
    kaldi/model.cpp
    kaldi/model.hpp
    kaldi/recognizer.cpp
    kaldi/tokenizer.cpp
)

target_include_directories(trueprompter_recognition_cxx17
    PUBLIC ${CMAKE_SOURCE_DIR}
    PRIVATE ${openfst_SOURCE_DIR}/src/include
)

target_link_libraries(trueprompter_recognition_cxx17
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
    utf8::cpp
)

set_property(TARGET trueprompter_recognition_cxx17 PROPERTY CXX_STANDARD 17)

add_library(trueprompter_recognition
    matcher.cpp
    matcher.hpp
    recognizer.hpp
    smith_waterman.hpp
    tokenizer.hpp
    onnx/onnx.hpp
    onnx/onnx.cpp
)

target_include_directories(trueprompter_recognition PUBLIC ${CMAKE_SOURCE_DIR})

target_link_libraries(trueprompter_recognition
    trueprompter_recognition_cxx17
    onnxruntime
)

target_include_directories(trueprompter_recognition PUBLIC ${CMAKE_SOURCE_DIR}/deps/onnxruntime/include)

