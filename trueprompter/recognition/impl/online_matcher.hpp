
#pragma once

#include <trueprompter/recognition/matcher.hpp>

#include <vector>

namespace NTruePrompter::NRecognition {

// TODO maybe move this piece of logic it into recognizer?
class TOnlineMatcher : public IMatcher {
public:
    TOnlineMatcher(std::shared_ptr<IMatcher> matcher, size_t contextMaxSize, size_t strideSize)
        : Matcher_(matcher)
        , ContextMaxSize_(contextMaxSize)
        , StrideSize_(strideSize)
    {
        if (!StrideSize_ || StrideSize_ > ContextMaxSize_) {
            throw std::runtime_error("Invalid stride size");
        }
    }

    std::span<const int64_t> Match(Eigen::Map<const Eigen::MatrixXf> emission, std::span<const int64_t> tokens) const override {
        std::span<const int64_t> resMatch;
        size_t currentFrame = 0;

        while (Context_.size() / emission.rows() + emission.cols() - currentFrame >= ContextMaxSize_) {
            if (Context_.size() / emission.rows() > ContextMaxSize_) {
                throw std::runtime_error("Should never happen");
            }
            const size_t toConsume = ContextMaxSize_ - Context_.size() / emission.rows();
            Context_.insert(Context_.end(), emission.data() + currentFrame * emission.rows(), emission.data() + toConsume * emission.rows());
            currentFrame += toConsume;
            auto match = Matcher_->Match(Eigen::Map<const Eigen::MatrixXf>(Context_.data(), emission.rows(), Context_.size() / emission.rows()), tokens);
            if (!match.empty()) {
                resMatch = match;
            }
            Context_.erase(Context_.begin(), Context_.begin() + StrideSize_ * emission.rows());
        }
        Context_.insert(Context_.end(), emission.data() + currentFrame * emission.rows(), emission.data() + emission.size());

        return resMatch;
    }

private:
    std::shared_ptr<IMatcher> Matcher_;
    size_t ContextMaxSize_;
    size_t StrideSize_;
    mutable std::vector<float> Context_;
};

} // namespace NTruePrompter::NRecognition

