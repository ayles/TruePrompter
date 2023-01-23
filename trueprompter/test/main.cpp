#include <trueprompter/recognition/onnx/onnx.hpp>

#include <fstream>
#include <iostream>
#include <vector>
#include <iterator>

int main() {
    std::ifstream f("../audio.raw", std::ios::binary);
    std::vector<char> b((std::istreambuf_iterator<char>(f)), (std::istreambuf_iterator<char>()));
    std::vector<float> buffer(b.size() / sizeof(float));
    std::memcpy(buffer.data(), b.data(), b.size());
    std::cout << buffer.front() << std::endl;

    NTruePrompter::NRecognition::TOnnxRecognizerFactory factory("../wav2vec2.onnx");
    auto recognizer = factory.New();
    recognizer->Update(buffer.data(), buffer.size(), 16000, nullptr);
}
