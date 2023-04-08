#pragma once

#include "onnx_model.hpp"

#include <trueprompter/recognition/recognizer.hpp>

#include <onnxruntime/core/session/experimental_onnxruntime_cxx_api.h>

namespace NTruePrompter::NRecognition {

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

    void Reset() override {
        // It has no state, so reset is no-op
    }

private:
    std::shared_ptr<TOnnxModel> Model_;
};

} // namespace NTruePrompter::NRecognition

