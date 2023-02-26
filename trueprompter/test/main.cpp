#include <trueprompter/recognition/onnx/onnx_recognizer.hpp>
#include <trueprompter/recognition/onnx/onnx_tokenizer.hpp>
#include <trueprompter/recognition/matcher.hpp>

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

/*std::tuple<bool, std::pair<size_t, size_t>> FindMaxAverageSubarray(const std::vector<float>& v, size_t k) {
    auto check = [&](double mid) -> std::tuple<bool, std::pair<size_t, size_t>> {
        double sum = std::accumulate(v.begin(), v.begin() + k, 0.0) - mid * k;
        double prev = 0;
        double minSum = 0;
        size_t minSumIndex = 0;

        if (sum >= 0) {
            return { true, { minSumIndex, k } };
        }

        for (size_t i = k; i < v.size(); ++i) {
            sum += v[i] - mid;
            prev += v[i - k] - mid;
            if (prev < minSum) {
                minSum = prev;
                minSumIndex = i - k;
            }
            if (sum >= minSum) {
                return { true, { minSumIndex, i } };
            }
        }
        return { false, { 0, 0 } };
    };

    std::tuple<bool, std::pair<size_t, size_t>> last;
    double mx = *std::max_element(v.begin(), v.end());
    double mn = *std::min_element(v.begin(), v.end());

    double prevMid = mx;
    double error = std::numeric_limits<double>::infinity();

    while (error > 0.000000001) {
        std::cout << mx << " " << mn << std::endl;
        double mid = (mx + mn) * 0.5;
        auto current = check(mid);
        if (std::get<bool>(current)) {
            mn = mid;
            last = current;
        } else {
            mx = mid;
        }
        error = std::abs(prevMid - mid);
        prevMid = mid;
    }
    return last;
}*/

int main() {
    std::ifstream f("audio2.raw", std::ios::binary);
    std::vector<char> b((std::istreambuf_iterator<char>(f)), (std::istreambuf_iterator<char>()));
    std::vector<float> buffer(b.size() / sizeof(float));
    std::memcpy(buffer.data(), b.data(), b.size());
    std::cout << buffer.size() << std::endl;

    auto model = std::make_shared<NTruePrompter::NRecognition::TOnnxModel>("model.onnx");
    auto recognizer = std::make_shared<NTruePrompter::NRecognition::TOnnxRecognizer>(model);
    auto contextualRecognizer = std::make_shared<NTruePrompter::NRecognition::TContextualRecognizer>(recognizer, 2.0, 0.5, 0.5);
    auto tokenizer = std::make_shared<NTruePrompter::NRecognition::TOnnxTokenizer>();
    //auto contextualRecognizer = recognizer;

    std::string transcript = { "FIVE" };
    std::vector<int64_t> tokens;
    tokenizer->Apply(transcript, &tokens, nullptr);

    std::vector<float> buf;
    auto mat = contextualRecognizer->Update(buffer, &buf);

    for (const auto& sample : mat.colwise()) {
        size_t maxIndex = std::max_element(sample.begin(), sample.end()) - sample.begin();
        if (maxIndex == 0) {
            continue;
        }
        std::cout << NTruePrompter::NRecognition::TOnnxTokenizer::Tokens[maxIndex] << " ";
    }
    std::cout << std::endl;

    {
        NTruePrompter::NRecognition::TDefaultMatcher matcher(NTruePrompter::NRecognition::TDefaultMatcher::EMode::Default);
        matcher.Update(Eigen::Map<const Eigen::MatrixXf>(mat.data(), mat.rows(), mat.cols()), tokens, 0);
    }

    {
        NTruePrompter::NRecognition::TDefaultMatcher matcher(NTruePrompter::NRecognition::TDefaultMatcher::EMode::WeightedPhonemes);
        matcher.Update(Eigen::Map<const Eigen::MatrixXf>(mat.data(), mat.rows(), mat.cols()), tokens, 0);
    }
}

