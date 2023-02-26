#pragma once

#include <trueprompter/recognition/recognizer.hpp>

#include <onnxruntime/core/session/experimental_onnxruntime_cxx_api.h>

#include <filesystem>

namespace NTruePrompter::NRecognition {

class TOnnxModel {
public:
    friend class TOnnxRecognizer;

    TOnnxModel(const std::filesystem::path& path)
        : Env_(ORT_LOGGING_LEVEL_WARNING, "TOnnxModel")
        , Opts_()
        , MemInfo_(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault))
    {
        Opts_.SetIntraOpNumThreads(1);
        Opts_.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
        Session_ = std::make_unique<Ort::Session>(Env_, path.c_str(), Opts_);

        if (Session_->GetInputCount() != 1) {
            throw std::runtime_error("Expected single input model");
        }

        if (Session_->GetOutputCount() != 1) {
            throw std::runtime_error("Expected single output model");
        }

        auto inputShape = Session_->GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
        if (inputShape.size() != 2 || inputShape[0] != -1 || inputShape[1] != -1) {
            throw std::runtime_error("Unexpected input shape");
        }

        auto outputShape = Session_->GetOutputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
        if (outputShape.size() != 3 || outputShape[0] != -1 || outputShape[1] != -1 || outputShape[2] <= 0) {
            throw std::runtime_error("Unexpected output shape");
        }

        // TODO load meta
        SampleRate_ = 16000;
        FrameSize_ = 320;
        FeaturesCount_ = outputShape[2];

        Ort::AllocatorWithDefaultOptions alloc;
        InputName_ = Session_->GetInputNameAllocated(0, alloc).get();
        OutputName_ = Session_->GetOutputNameAllocated(0, alloc).get();
    }

private:
    Ort::Env Env_;
    Ort::SessionOptions Opts_;
    Ort::MemoryInfo MemInfo_;
    std::unique_ptr<Ort::Session> Session_;
    int64_t SampleRate_;
    int64_t FrameSize_;
    int64_t FeaturesCount_;
    std::string InputName_;
    std::string OutputName_;
};

class TOnnxRecognizer : public IRecognizer {
public:
    TOnnxRecognizer(std::shared_ptr<TOnnxModel> model)
        : Model_(std::move(model))
    {}

    Eigen::Map<Eigen::MatrixXf> Update(std::span<const float> data, std::vector<float>* buf) override {
        Eigen::Map<const Eigen::ArrayXf> dataAsArray(data.data(), data.size());
        const float mean = dataAsArray.mean();
        const float stdDeviation = std::sqrt((dataAsArray - mean).square().sum() / data.size());
        // TODO do not allocate
        Eigen::ArrayXf transformedData = (dataAsArray - mean) / stdDeviation;

        std::array<int64_t, 2> dims { (int64_t)1, (int64_t)data.size() };
        std::array<const char*, 1> inputNames { Model_->InputName_.c_str() };
        std::array<const char*, 1> outputNames { Model_->OutputName_.c_str() };

        auto tensor = Ort::Value::CreateTensor<float>(Model_->MemInfo_, transformedData.data(), transformedData.size(), dims.data(), dims.size());
        auto outTensors = Model_->Session_->Run(Ort::RunOptions { nullptr }, inputNames.data(), &tensor, 1, outputNames.data(), 1);
        const float* outData = outTensors.front().GetTensorData<float>();
        const auto outShapeInfo = outTensors.front().GetTensorTypeAndShapeInfo();

        const size_t rows = outShapeInfo.GetShape()[2];
        const size_t cols = outShapeInfo.GetShape()[1];
        buf->resize(rows * cols);
        Eigen::Map<Eigen::MatrixXf> emissions(buf->data(), rows, cols);
        std::copy(outData, outData + rows * cols, buf->data());

        // LogSoftmax
        emissions.rowwise() -= emissions.array().exp().colwise().sum().log().matrix();

        return emissions;
    }

    int64_t GetSampleRate() const override {
        return Model_->SampleRate_;
    }

    int64_t GetFrameSize() const override {
        return Model_->FrameSize_;
    }

private:
    std::shared_ptr<TOnnxModel> Model_;
};

} // namespace NTruePrompter::NRecognition

