#pragma once

#include <trueprompter/recognition/matcher.hpp>

#include <matplot/matplot.h>

#include <numeric>
#include <unordered_set>

namespace NTruePrompter::NRecognition {

namespace NPrivate {

struct TContext {
    Eigen::Map<const Eigen::MatrixXf> Emission;
    Eigen::Map<const Eigen::MatrixXi> Backtrack;
    std::span<const int64_t> Tokens;
    int MaxMatchLength;

    float GetEmission(const Eigen::Vector2i& pos) const {
        return Emission(Tokens[pos(0)], pos(1));
    }

    Eigen::Vector2i GetPrev(const Eigen::Vector2i& pos) const {
        return Eigen::Vector2i(pos(0) + Backtrack(pos(0), pos(1)), pos(1) - 1);
    }
};

class TPath {
public:
    TPath(const Eigen::Vector2i& pos)
        : Head(pos)
        , Tail(pos)
        , WeightSum(0.0)
    {}

    int Length() const {
        return Tail(0) - Head(0);
    }

    float Weight() const {
        int length = Length();
        return length ? std::exp(WeightSum / length) : 0.0;
    }

    bool IsFinished() const {
        return Head(0) < 0 || Head(1) < 0;
    }

    void Advance(const TContext& ctx) {
        while (!IsFinished()) {
            auto prev = ctx.GetPrev(Head);
            auto emission = ctx.GetEmission(Head);
            std::swap(Head, prev);
            if (Head(0) != prev(0)) {
                WeightSum += emission;
                break;
            }
        }

        while (Tail(1) > Head(1) + 1) {
            auto prev = ctx.GetPrev(Tail);
            if (Tail(0) != prev(0)) {
                if (Length() <= ctx.MaxMatchLength) {
                    break;
                }
                WeightSum -= ctx.GetEmission(Tail);
            }
            Tail = prev;
        }
    }

    std::vector<Eigen::Vector2i> Track(const TContext& ctx) const {
        std::vector<Eigen::Vector2i> res;
        auto pos = Tail;
        while (pos != Head) {
            res.emplace_back(pos);
            pos = ctx.GetPrev(pos);
        }
        return res;
    }

    uint64_t Id() const {
        return ((uint64_t)Tail(0) << 32) | (uint64_t)Tail(1);
    }

private:
    Eigen::Vector2i Head;
    Eigen::Vector2i Tail;
    double WeightSum;
};

static void ValidateTokens(Eigen::Map<const Eigen::MatrixXf> emission, std::span<const int64_t> tokens) {
    for (int64_t token : tokens) {
        if (token < 0 || token >= emission.rows()) {
            throw std::runtime_error("Invalid token (token: " + std::to_string(token) + ")");
        }
    }
}

// Modified version of https://pytorch.org/audio/stable/tutorials/forced_alignment_tutorial.html
static std::tuple<Eigen::MatrixXf, Eigen::MatrixXi> BuildTrellis(Eigen::Map<const Eigen::MatrixXf> emission, std::span<const int64_t> tokens, int64_t blankTokenIndex) {
    const size_t framesCount = emission.cols();
    const size_t tokensCount = tokens.size();

    Eigen::MatrixXf trellis = Eigen::MatrixXf::Zero(tokensCount, framesCount);
    Eigen::MatrixXi backtrack = Eigen::MatrixXi::Zero(tokensCount, framesCount);

    // TODO fill first row and column to remove branching
    for (size_t t = 0; t < framesCount; ++t) {
        auto prevCol = trellis.col(t ? t - 1 : 0);
        for (size_t i = 0; i < tokensCount; ++i) {
            float stayScore = prevCol.coeff(i) + emission(blankTokenIndex, t);
            float changeScore = prevCol.coeff(i ? i - 1 : 0) + emission(tokens[i], t);

            if (stayScore > changeScore) {
                trellis(i, t) = stayScore;
                backtrack(i, t) = 0;
            } else {
                trellis(i, t) = changeScore;
                backtrack(i, t) = -1;
            }
        }
    }

    return { trellis, backtrack };
}

static TPath BacktrackNew(const TContext& ctx) {
    TPath maxPath(Eigen::Vector2i(-1, -1));

    std::unordered_set<uint64_t> seen;

    auto processPath = [&](TPath path) {
        while (!path.IsFinished()) {
            path.Advance(ctx);
            if (path.Length() >= ctx.MaxMatchLength) {
                auto [_, ok] = seen.emplace(path.Id());
                if (!ok) {
                    break;
                }
                if (path.Weight() > maxPath.Weight()) {
                    maxPath = path;
                }
            }
        }
    };

    // Process rightmost paths
    for (Eigen::Vector2i pos(ctx.Backtrack.rows() - 1, ctx.Backtrack.cols() - 1); pos(0) >= 0; --pos(0)) {
        processPath(TPath(pos));
    }


    // Process all intermediate paths
    for (Eigen::Vector2i pos(ctx.Backtrack.rows() - 1, ctx.Backtrack.cols() - 1); pos(1) > 0; --pos(1)) {
        bool wasTransition = false;
        for ( ; pos(0) >= 0; --pos(0)) {
            bool transition = ctx.GetPrev(pos)(0) != pos(0);
            if (transition && !wasTransition) {
                processPath(TPath(Eigen::Vector2i(pos(0), pos(1) - 1)));
            }
            wasTransition = transition;
        }
    }

    return maxPath;
}

} // namespace NPrivate

class TViterbiMatcher : public IMatcher {
public:
    TViterbiMatcher(int64_t blankToken, int matchSize = 0, float matchStopWeight = 0.0)
        : BlankToken_(blankToken)
        , MatchSize_(matchSize)
        , MatchStopWeight_(matchStopWeight)
    {}

    std::span<const int64_t> Match(Eigen::Map<const Eigen::MatrixXf> emission, std::span<const int64_t> tokens) const override {
        NPrivate::ValidateTokens(emission, tokens);
        auto [trellis, backtrack] = NPrivate::BuildTrellis(emission, tokens, BlankToken_);

        NPrivate::TContext ctx {
            .Emission = emission,
            .Backtrack = {backtrack.data(), backtrack.rows(), backtrack.cols()},
            .Tokens = tokens,
            .MaxMatchLength = MatchSize_,
        };

        auto path = NPrivate::BacktrackNew(ctx);

        std::cout << path.Weight() << std::endl;
        if (path.Weight() < MatchStopWeight_) {
            return {};
        }

        auto track = path.Track(ctx);

        if (false) {
            std::vector<std::vector<double>> v(trellis.rows(), std::vector<double>(trellis.cols()));
            for (size_t row = 0; row < trellis.rows(); ++row) {
                for (size_t col = 0; col < trellis.cols(); ++col) {
                    v[row][col] = trellis(row, col);
                }
            }

            for (auto t : track) {
                v[t(0)][t(1)] = -std::numeric_limits<float>::infinity();
            }

            matplot::image(v, true);
            matplot::colorbar();
            matplot::axes()->y_axis().reverse(false);
            matplot::show();
        }

        if (track.empty()) {
            return {};
        }

        return { tokens.data() + track.back()(0), tokens.data() + track.front()(0) + 1 };
    }

private:
    const int64_t BlankToken_;
    const int MatchSize_;
    const float MatchStopWeight_;
};

} // namespace NTruePrompter::NRecognition

