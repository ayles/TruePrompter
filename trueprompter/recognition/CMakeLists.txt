add_library(trueprompter_recognition_cxx17
    onnx/onnx_model.cpp
)

set_property(TARGET trueprompter_recognition_cxx17 PROPERTY CXX_STANDARD 17)

target_link_libraries(trueprompter_recognition_cxx17
    nlohmann_json::nlohmann_json
    onnxruntime::onnxruntime
    phonetisaurus
)

target_include_directories(trueprompter_recognition_cxx17
    PUBLIC ${CMAKE_SOURCE_DIR}
)

add_library(trueprompter_recognition
    matcher.hpp
    recognizer.hpp
    tokenizer.hpp
    impl/online_recognizer.hpp
    impl/viterbi_matcher.hpp
    onnx/onnx_model.hpp
    onnx/onnx_recognizer.cpp
    onnx/onnx_recognizer.hpp
    onnx/onnx_tokenizer.cpp
    onnx/onnx_tokenizer.hpp
)

target_include_directories(trueprompter_recognition
    PUBLIC ${CMAKE_SOURCE_DIR}
)

target_link_libraries(trueprompter_recognition
    Eigen3::Eigen
    Matplot++::matplot
    nlohmann_json::nlohmann_json
    onnxruntime::onnxruntime
    trueprompter_recognition_cxx17
    utf8cpp
)

