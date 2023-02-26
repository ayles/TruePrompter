add_library(trueprompter_recognition
    onnx/onnx_recognizer.cpp
    onnx/onnx_recognizer.hpp
    onnx/onnx_tokenizer.hpp
    matcher.hpp
    prompter.hpp
    recognizer.hpp
    tokenizer.hpp
)

target_include_directories(trueprompter_recognition
    PUBLIC ${CMAKE_SOURCE_DIR}
)

target_link_libraries(trueprompter_recognition
    Eigen3::Eigen
    onnxruntime::onnxruntime
)

