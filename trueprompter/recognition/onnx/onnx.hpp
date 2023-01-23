#pragma once

#include <trueprompter/recognition/recognizer.hpp>

#include <onnxruntime/core/session/onnxruntime_cxx_api.h>

#include <filesystem>
#include <memory>
#include <iostream>
#include <array>


namespace NTruePrompter::NRecognition {

class TOnnxRecognizerFactory : public IRecognizerFactory {
public:
    static constexpr std::array<const char*, 1> InputName = { "input" };
    static constexpr std::array<const char*, 1> OutputName = { "output" };

    class TRecognizer : public IRecognizer {
    public:
        TRecognizer(const TOnnxRecognizerFactory& parent) 
            : Parent_(parent)
        {}

        bool Update(const float* data, size_t dataSize, int32_t sampleRate, std::vector<int64_t>* tokensOut) override {
            double mean = 0.0;
            double variance = 0.0;
            for (size_t i = 0; i < dataSize; ++i) {
                mean += data[i];
            }
            mean /= dataSize;
            for (size_t i = 0; i < dataSize; ++i) {
                variance += std::pow(data[i] - mean, 2.0);
            }
            variance /= dataSize;
            const double stdDeviation = std::sqrt(variance + 1e-5);
            
            std::vector<float> newData(dataSize);
            for (size_t i = 0; i < dataSize; ++i) {
                newData[i] = (data[i] - mean) / stdDeviation;
            }

            std::array<int64_t, 2> dims { 1, (int64_t)dataSize };
            auto tensor = Ort::Value::CreateTensor<float>(Parent_.MemInfo_, newData.data(), dataSize, dims.data(), dims.size());
            auto outTensors = Parent_.Session_->Run(Ort::RunOptions { nullptr }, InputName.data(), &tensor, 1, OutputName.data(), 1);
            auto dimsOut = outTensors.front().GetTensorTypeAndShapeInfo().GetShape();
            std::vector<int64_t> location { 0, 0, 0 };
            for (location[1] = 0; location[1] < dimsOut[1]; ++location[1]) {
                float mx = std::numeric_limits<float>::lowest();
                size_t mxIndex = 0;
                for (location[2] = 0; location[2] < dimsOut[2]; ++location[2]) {
                    float m = outTensors.front().At<float>(location);
                    if (m >= mx) {
                        mx = m;
                        mxIndex = location[2];
                    }
                }
                auto r = std::array<std::string, 32> {{
                    "<pad>",
                    "<s>",
                    "</s>",
                    "<unk>",
                    "|",
                    "E",
                    "T",
                    "A",
                    "O",
                    "N",
                    "I",
                    "H",
                    "S",
                    "R",
                    "D",
                    "L",
                    "U",
                    "M",
                    "W",
                    "C",
                    "F",
                    "G",
                    "Y",
                    "P",
                    "B",
                    "V",
                    "K",
                    "'",
                    "X",
                    "J",
                    "Q",
                    "Z",
                }}[mxIndex];
                if (r != "<pad>") {
                    std::cout << r << " " << std::flush;
                }
            }
            return true;
        }

        void Reset() override {

        }

    private:
        const TOnnxRecognizerFactory& Parent_;
    };

    TOnnxRecognizerFactory(const std::filesystem::path& path)
        : Env_(ORT_LOGGING_LEVEL_WARNING, "TOnnxRecognizerFactory")
        , Opts_()
        , MemInfo_(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault))
    {
        Opts_.SetIntraOpNumThreads(1);
        Opts_.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
        Session_ = std::make_unique<Ort::Session>(Env_, path.c_str(), Opts_);
    }

    std::shared_ptr<IRecognizer> New() const {
        return std::make_shared<TRecognizer>(*this);
    }

    
private:
    Ort::Env Env_;
    Ort::SessionOptions Opts_;
    Ort::MemoryInfo MemInfo_;
    std::unique_ptr<Ort::Session> Session_;
};

} // NTruePrompter::NRecognition

