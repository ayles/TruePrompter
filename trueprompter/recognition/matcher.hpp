#pragma once

#include <Eigen/Dense>

#include <iostream>
#include <memory>
#include <span>
#include <vector>

namespace NTruePrompter::NRecognition {

class IMatcher {
public:
    using TPoint = std::tuple<Eigen::Index, Eigen::Index>;
    using TPath = std::vector<TPoint>;

    virtual std::tuple<std::span<const int64_t>, float> Update(Eigen::Map<const Eigen::MatrixXf> emissions, std::span<const int64_t> tokens, int64_t blankTokenIndex) = 0;
    virtual ~IMatcher() = default;
};

class TDefaultMatcher : public IMatcher {
public:
    enum class EMode {
        Default,
        WeightedPhonemes,
    };

    TDefaultMatcher(EMode mode)
        : Mode_(mode)
    {}

    std::tuple<std::span<const int64_t>, float> Update(Eigen::Map<const Eigen::MatrixXf> emissions, std::span<const int64_t> tokens, int64_t blankTokenIndex) override {
        auto [trellis, backtrack] = ConstructTrellis(emissions, tokens, blankTokenIndex);
        auto path = DoBacktrack(trellis, backtrack, tokens, blankTokenIndex);

        double sum = 0.0;
        size_t count = 0;
        double prevValue = 0.0;

        if (Mode_ == EMode::Default) {
            for (auto& [row, col] : path) {
                double currValue = trellis(row + 1, col + 1);
                sum += currValue - prevValue;
                prevValue = currValue;
                count++;
            }
        } else if (Mode_ == EMode::WeightedPhonemes) {
            auto it = path.begin();
            while (it != path.end()) {
                auto nextIt = std::find_if(it, path.end(), [&](const auto& point) {
                    return std::get<0>(point) != std::get<0>(*it);
                });
                auto beforeIt = std::prev(nextIt);
                double currentValue = trellis(std::get<0>(*beforeIt) + 1, std::get<1>(*beforeIt) + 1);
                sum += (currentValue - prevValue) / (nextIt - it);
                count++;
                prevValue = currentValue;
                it = nextIt;
            }
        } else {
            throw std::runtime_error("Unsupported mode");
        }

        return { tokens, std::exp(sum / count) };
    }

public:
    // https://pytorch.org/audio/stable/tutorials/forced_alignment_tutorial.html
    static std::tuple<Eigen::MatrixXf, Eigen::MatrixXi> ConstructTrellis(const Eigen::Map<const Eigen::MatrixXf>& emission, std::span<const int64_t> tokens, int64_t blankTokenIndex) {
        for (int64_t token : tokens) {
            if (token < 0 || token >= emission.rows()) {
                throw std::runtime_error("Invalid token (token: " + std::to_string(token) + ")");
            }
        }

        const size_t framesCount = emission.cols();
        const size_t tokensCount = tokens.size();

        // Zero row is for start of sentence acc
        // Zero column is for simplification of the code
        Eigen::MatrixXf trellis = Eigen::MatrixXf::Zero(tokensCount + 1, framesCount + 1);
        Eigen::MatrixXi backtrack = Eigen::MatrixXi::Zero(tokensCount + 1, framesCount + 1);

        trellis.block(1, 0, tokensCount, 1).setConstant(-std::numeric_limits<float>::infinity());
        trellis.block(0, trellis.cols() - tokensCount, 1, tokensCount).setConstant(std::numeric_limits<float>::infinity());

        for (size_t t = 0; t < framesCount; ++t) {
            for (size_t i = 0; i < tokensCount; ++i) {
                float stayScore = trellis(i + 1, t) + std::log(std::exp(emission(tokens[i], t)) + std::exp(emission(blankTokenIndex, t)));
                float changeScore = trellis(i, t) + emission(tokens[i], t);

                if (stayScore > changeScore) {
                    trellis(i + 1, t + 1) = stayScore;
                    backtrack(i + 1, t + 1) = 0;
                } else {
                    trellis(i + 1, t + 1) = changeScore;
                    backtrack(i + 1, t + 1) = -1;
                }
            }
        }

        return { trellis, backtrack };
    }

    static TPath DoBacktrack(const Eigen::MatrixXf& trellis, const Eigen::MatrixXi& backtrack, std::span<const int64_t> tokens, int64_t blankTokenIndex) {
        Eigen::Index selectedCol;
        trellis.row(trellis.rows() - 1).maxCoeff(&selectedCol);
        TPoint pos = { trellis.rows() - 1, selectedCol };

        TPath res;
        for (auto& [row, col] = pos; row != 0 && col != 0; pos = { row + backtrack(row, col), col - 1 }) {
            res.emplace_back(row - 1, col - 1);
        }

        std::reverse(res.begin(), res.end());
        return res;
    }

private:
    EMode Mode_;
};

class TOnlineMatcher : public IMatcher {
public:
    TOnlineMatcher(std::shared_ptr<IMatcher> matcher, size_t contextMaxSize)
        : Matcher_(matcher)
        , ContextMaxSize_(contextMaxSize)
    {}

    std::tuple<std::span<const int64_t>, float> Update(Eigen::Map<const Eigen::MatrixXf> emissions, std::span<const int64_t> tokens, int64_t blankTokenIndex) override {
        const size_t currentSize = Context_.size() / emissions.rows() + emissions.cols();
        if (currentSize > ContextMaxSize_) {
            Context_.erase(Context_.begin(), Context_.begin() + (currentSize - ContextMaxSize_) * emissions.rows());
        }
        Context_.insert(Context_.end(), emissions.data(), emissions.data() + emissions.size());

        return Matcher_->Update(Eigen::Map<const Eigen::MatrixXf>(Context_.data(), emissions.rows(), Context_.size() / emissions.rows()), tokens, blankTokenIndex);
    }

private:
    std::shared_ptr<IMatcher> Matcher_;
    size_t ContextMaxSize_;
    std::vector<float> Context_;
};

} // namespace NTruePrompter::NRecognition

