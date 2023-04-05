
#pragma once

#include <trueprompter/recognition/matcher.hpp>

#include <vector>

namespace NTruePrompter::NRecognition {

// TODO maybe move this piece of logic it into recognizer?
class TOnlineMatcher : public IMatcher {
public:
    TOnlineMatcher(std::shared_ptr<IMatcher> matcher, size_t contextMaxSize)
        : Matcher_(matcher)
        , ContextMaxSize_(contextMaxSize)
    {}

    std::span<const int64_t> Match(Eigen::Map<const Eigen::MatrixXf> emission, std::span<const int64_t> tokens) const override {
        const size_t currentSize = Context_.size() / emission.rows() + emission.cols();
        if (currentSize > ContextMaxSize_) {
            Context_.erase(Context_.begin(), Context_.begin() + (currentSize - ContextMaxSize_) * emission.rows());
        }
        Context_.insert(Context_.end(), emission.data(), emission.data() + emission.size());

        return Matcher_->Match(Eigen::Map<const Eigen::MatrixXf>(Context_.data(), emission.rows(), Context_.size() / emission.rows()), tokens);
    }

private:
    std::shared_ptr<IMatcher> Matcher_;
    size_t ContextMaxSize_;
    mutable std::vector<float> Context_;
};

} // namespace NTruePrompter::NRecognition

