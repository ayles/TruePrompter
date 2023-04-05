#pragma once

#include <Eigen/Dense>

#include <span>
#include <vector>
#include <memory>

namespace NTruePrompter::NRecognition {

class IRecognizer {
public:
    // Each matrix column corresponds to one frame, each time frame is exactly GetFrameSize() samples
    virtual Eigen::Map<Eigen::MatrixXf> Update(std::span<const float> data, std::vector<float>* buf) = 0;
    virtual void Reset() = 0;
    virtual int64_t GetSampleRate() const = 0;
    virtual int64_t GetFrameSize() const = 0;
    virtual ~IRecognizer() = default;
};

} // NTruePrompter::NRecognition

