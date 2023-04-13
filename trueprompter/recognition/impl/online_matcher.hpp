
#pragma once

#include <trueprompter/recognition/matcher.hpp>

#include <vector>

namespace NTruePrompter::NRecognition {

// TODO maybe move this piece of logic it into recognizer?
class TOnlineMatcher : public IMatcher {
public:
    TOnlineMatcher(std::shared_ptr<IMatcher> matcher, size_t contextMaxSize, size_t overlapSize)
        : Matcher_(matcher)
        , ContextMaxSize_(contextMaxSize)
        , OverlapSize_(overlapSize)
        , Context_(0, 0)
        , ContextSize_(0)
    {
        if (OverlapSize_ >= ContextMaxSize_) {
            throw std::runtime_error("Overlap size should be less than context size");
        }
    }

    std::tuple<TTrack, TTokensRange> Match(Eigen::Map<const Eigen::MatrixXf> emission, TTokensRange tokens) const override {
        TTokensRange bestMatch;
        size_t currentCol = 0;

        if (!Context_.cols()) {
            Context_.conservativeResize(emission.rows(), ContextMaxSize_);
            ContextSize_ = 0;
        } else if (Context_.rows() != emission.rows()) {
            throw std::runtime_error("Should never happen");
        }

        while (true) {
            size_t toConsume = std::min<size_t>(emission.cols() - currentCol, ContextMaxSize_ - ContextSize_);
            if (!toConsume) {
                break;
            }
            Context_.middleCols(ContextSize_, toConsume) = emission.middleCols(currentCol, toConsume);
            ContextSize_ += toConsume;

            auto [track, match] = Matcher_->Match(Eigen::Map<const Eigen::MatrixXf>(emission.data() + emission.rows() * currentCol, emission.rows(), emission.cols() - currentCol), tokens);
            if (!track.empty() && !match.empty()) {
                size_t posAfterMatch = std::min<size_t>(track.back()(1) + 1, ContextSize_);
                if (ContextSize_ > posAfterMatch) {
                    Context_.leftCols(ContextSize_ - posAfterMatch) = Context_.middleCols(posAfterMatch, ContextSize_ - posAfterMatch);
                }
                ContextSize_ = ContextSize_ - posAfterMatch;
                tokens = { match.end(), tokens.end() };
                bestMatch = match;
            }

            currentCol += toConsume;
            if (ContextSize_ > OverlapSize_) {
                Context_.leftCols(OverlapSize_) = Context_.middleCols(ContextSize_ - OverlapSize_, OverlapSize_);
                ContextSize_ = OverlapSize_;
            }
        }

        return {
            {},
            bestMatch,
        };
    }

private:
    std::shared_ptr<IMatcher> Matcher_;
    size_t ContextMaxSize_;
    size_t OverlapSize_;
    mutable Eigen::MatrixXf Context_;
    mutable size_t ContextSize_;
};

} // namespace NTruePrompter::NRecognition

