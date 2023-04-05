#include <trueprompter/recognition/matcher.hpp>
#include <trueprompter/recognition/impl/viterbi_matcher.hpp>
#include <trueprompter/recognition/onnx/onnx_recognizer.hpp>
#include <trueprompter/recognition/onnx/onnx_tokenizer.hpp>

#include <onnxruntime/core/session/onnxruntime_cxx_api.h>

#include <Eigen/Dense>
#include <unsupported/Eigen/MatrixFunctions>

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <numeric>
#include <vector>

int main() {
    std::ifstream f("audio2.raw", std::ios::binary);
    std::vector<char> b((std::istreambuf_iterator<char>(f)), (std::istreambuf_iterator<char>()));
    std::vector<float> buffer(b.size() / sizeof(float));
    std::memcpy(buffer.data(), b.data(), b.size());
    std::cout << buffer.size() << std::endl;

    auto model = std::make_shared<NTruePrompter::NRecognition::TOnnxModel>("model.onnx");
    auto recognizer = std::make_shared<NTruePrompter::NRecognition::TOnnxRecognizer>(model);
    auto tokenizer = std::make_shared<NTruePrompter::NRecognition::TOnnxTokenizer>();

    std::string transcript = { "ONE|TWO|THREE|FOUR|FIVE|SIX|SEVEN|EIGHT|NINE|TEN|ELEVEN|TWELVE" };
    std::vector<int64_t> tokens;
    tokenizer->Tokenize(transcript, &tokens, nullptr);

    std::vector<float> buf;
    auto mat = recognizer->Update(buffer, &buf);

    for (const auto& sample : mat.colwise()) {
        size_t maxIndex = std::max_element(sample.begin(), sample.end()) - sample.begin();
        if (maxIndex == 0) {
            continue;
        }
        std::cout << NTruePrompter::NRecognition::TOnnxTokenizer::Tokens[maxIndex] << " ";
    }
    std::cout << std::endl;

    NTruePrompter::NRecognition::TViterbiMatcher matcher(tokenizer->GetBlankToken(), 5, 0.0);
    auto match = matcher.Match(Eigen::Map<const Eigen::MatrixXf>(mat.data(), mat.rows(), mat.cols()), tokens);

    for (auto token : match) {
        std::cout << tokenizer->Lookup(token) << "|";
    }
}

