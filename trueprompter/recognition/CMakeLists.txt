add_library(trueprompter_recognition
    matcher.hpp
    recognizer.hpp
    tokenizer.hpp
    impl/online_recognizer.hpp
    impl/viterbi_matcher.hpp
    onnx/onnx_recognizer.cpp
    onnx/onnx_recognizer.hpp
    onnx/onnx_tokenizer.hpp
)

target_include_directories(trueprompter_recognition
    PUBLIC ${CMAKE_SOURCE_DIR}
)

target_link_libraries(trueprompter_recognition
    Eigen3::Eigen
    onnxruntime::onnxruntime
    Matplot++::matplot
)

