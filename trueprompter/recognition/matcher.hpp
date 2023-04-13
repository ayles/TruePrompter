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
    using TTrack = std::vector<Eigen::Vector2i>;
    using TTokensRange = std::span<const int64_t>;

    virtual std::tuple<TTrack, TTokensRange> Match(Eigen::Map<const Eigen::MatrixXf> emission, TTokensRange tokens) const = 0;
    virtual ~IMatcher() = default;
};

} // namespace NTruePrompter::NRecognition

