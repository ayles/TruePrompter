#include <trueprompter/recognition/matcher.hpp>
#include <trueprompter/recognition/impl/online_recognizer.hpp>
#include <trueprompter/recognition/impl/online_matcher.hpp>
#include <trueprompter/recognition/impl/viterbi_matcher.hpp>
#include <trueprompter/recognition/onnx/onnx_model.hpp>
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
    std::ifstream f("kek.raw", std::ios::binary);
    std::vector<char> b((std::istreambuf_iterator<char>(f)), (std::istreambuf_iterator<char>()));
    std::vector<float> buffer(b.size() / sizeof(float));
    std::memcpy(buffer.data(), b.data(), b.size());
    std::cout << buffer.size() << std::endl;

    auto model = std::make_shared<NTruePrompter::NRecognition::TOnnxModel>("/Users/ayles/Projects/TruePrompter/kek/config.json", "/Users/ayles/Projects/TruePrompter/kek/model.onnx", "/Users/ayles/Projects/TruePrompter/kek/model.fst");
    auto recognizer = std::make_shared<NTruePrompter::NRecognition::TOnlineRecognizer>(
        std::make_shared<NTruePrompter::NRecognition::TOnnxRecognizer>(model),
        2.0,
        0.5,
        0.5
    );
    auto tokenizer = std::make_shared<NTruePrompter::NRecognition::TOnnxTokenizer>(model);
    auto matcher = std::make_shared<NTruePrompter::NRecognition::TOnlineMatcher>(
        std::make_shared<NTruePrompter::NRecognition::TViterbiMatcher>(tokenizer->GetBlankToken(), 5, 0.5),
        1.5 * recognizer->GetSampleRate() / recognizer->GetFrameSize(),
        0.5 * recognizer->GetSampleRate() / recognizer->GetFrameSize()
    );

    std::string transcript = { "один два три четыре пять" };
    std::vector<int64_t> tokens;
    tokenizer->Tokenize(transcript, &tokens, nullptr);
    for (auto token : tokens) {
        std::cout << tokenizer->Lookup(token);
    }
    std::cout << std::endl;

    std::vector<float> buf;
    auto mat = recognizer->Update(buffer, &buf);

    for (const auto& sample : mat.colwise()) {
        size_t maxIndex = std::max_element(sample.begin(), sample.end()) - sample.begin();
        if (tokenizer->GetBlankToken() == maxIndex) {
            continue;
        }
        std::cout << tokenizer->Lookup(maxIndex);
    }
    std::cout << std::endl;

    auto match = matcher->Match(Eigen::Map<const Eigen::MatrixXf>(mat.data(), mat.rows(), mat.cols()), tokens);

    for (auto token : match) {
        std::cout << tokenizer->Lookup(token);
    }
    std::cout << std::endl;
}

