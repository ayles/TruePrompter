#include <trueprompter/recognition/matcher.hpp>
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
    std::ifstream f("audio2.raw", std::ios::binary);
    std::vector<char> b((std::istreambuf_iterator<char>(f)), (std::istreambuf_iterator<char>()));
    std::vector<float> buffer(b.size() / sizeof(float));
    std::memcpy(buffer.data(), b.data(), b.size());
    std::cout << buffer.size() << std::endl;

    auto model = std::make_shared<NTruePrompter::NRecognition::TOnnxModel>("/home/ayles/Projects/TruePrompterModel/out/exported/config.json", "/home/ayles/Projects/TruePrompterModel/out/exported/model.onnx", "/home/ayles/Projects/TruePrompterModel/pout/model.fst");
    auto recognizer = std::make_shared<NTruePrompter::NRecognition::TOnnxRecognizer>(model);
    auto tokenizer = std::make_shared<NTruePrompter::NRecognition::TOnnxTokenizer>(model);

    std::string transcript = { "one two three four five six seven" };
    std::vector<int64_t> tokens;
    tokenizer->Tokenize(transcript, &tokens, nullptr);
    for (auto token : tokens) {
        std::cout << tokenizer->Lookup(token);
    }
    std::cout << std::endl;

    std::vector<float> buf;
    auto mat = recognizer->Update(buffer, &buf);

    if (true) {
        std::vector<std::vector<double>> v(mat.rows(), std::vector<double>(mat.cols()));
        for (size_t row = 0; row < mat.rows(); ++row) {
            for (size_t col = 0; col < mat.cols(); ++col) {
                v[row][col] = mat(row, col);
            }
        }

        matplot::image(v, true);
        matplot::colorbar();
        matplot::axes()->y_axis().reverse(false);
        matplot::show();
    }

    for (const auto& sample : mat.colwise()) {
        size_t maxIndex = std::max_element(sample.begin(), sample.end()) - sample.begin();
        if (maxIndex == 0) {
            continue;
        }
        std::cout << tokenizer->Lookup(maxIndex) << " ";
    }
    std::cout << std::endl;

    NTruePrompter::NRecognition::TViterbiMatcher matcher(tokenizer->GetBlankToken(), 5, 0.0);
    auto match = matcher.Match(Eigen::Map<const Eigen::MatrixXf>(mat.data(), mat.rows(), mat.cols()), tokens);

    for (auto token : match) {
        std::cout << tokenizer->Lookup(token);
    }
    std::cout << std::endl;
}

