#pragma once

#include <Eigen/Dense>

#include <iostream>
#include <memory>
#include <span>
#include <tuple>
#include <vector>

namespace NTruePrompter::NRecognition {

class IMatcher {
public:
    virtual std::span<const int64_t> Match(Eigen::Map<const Eigen::MatrixXf> emission, std::span<const int64_t> tokens) const = 0;
    virtual ~IMatcher() = default;
};

} // namespace NTruePrompter::NRecognition

