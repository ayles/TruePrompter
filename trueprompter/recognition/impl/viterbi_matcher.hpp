#pragma once

#include <trueprompter/recognition/matcher.hpp>

#include <matplot/matplot.h>

#include <numeric>
#include <unordered_set>

namespace NTruePrompter::NRecognition {

namespace NPrivate {

struct TContext {
    int MatchLength;
    float MatchMinWeight;
    Eigen::Map<const Eigen::MatrixXf> Emission;
    Eigen::Map<const Eigen::MatrixXi> Backtrack;
    std::span<const int64_t> Tokens;

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
        : Head_(pos)
        , Tail_(pos)
        , WeightSum_(0.0)
    {}

    int Length() const {
        return Tail_(0) - Head_(0);
    }

    float Weight() const {
        int length = Length();
        return length ? std::exp(WeightSum_ / length) : 0.0;
    }

    bool IsFinished() const {
        return Head_(0) < 0 || Head_(1) < 0;
    }

    void Advance(const TContext& ctx) {
        while (!IsFinished()) {
            auto prev = ctx.GetPrev(Head_);
            auto emission = ctx.GetEmission(Head_);
            std::swap(Head_, prev);
            if (Head_(0) != prev(0)) {
                WeightSum_ += emission;
                break;
            }
        }

        while (Tail_(1) > Head_(1) + 1) {
            auto prev = ctx.GetPrev(Tail_);
            if (Tail_(0) != prev(0)) {
                if (Length() <= ctx.MatchLength) {
                    break;
                }
                WeightSum_ -= ctx.GetEmission(Tail_);
            }
            Tail_ = prev;
        }
    }

    IMatcher::TTrack Track(const TContext& ctx) const {
        std::vector<Eigen::Vector2i> res;
        auto pos = Tail_;
        while (pos != Head_) {
            res.emplace_back(pos);
            pos = ctx.GetPrev(pos);
        }
        std::reverse(res.begin(), res.end());
        return res;
    }

    uint64_t Id() const {
        return ((uint64_t)Tail_(0) << 32) | (uint64_t)Tail_(1);
    }

    const Eigen::Vector2i& GetHead() const {
        return Head_;
    }

    const Eigen::Vector2i& GetTail() const {
        return Tail_;
    }

private:
    Eigen::Vector2i Head_;
    Eigen::Vector2i Tail_;
    double WeightSum_;
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
    TPath bestPath(Eigen::Vector2i(-1, -1));

    std::unordered_set<uint64_t> seen;

    auto processPath = [&](TPath path) {
        while (!path.IsFinished()) {
            path.Advance(ctx);
            if (path.Length() >= ctx.MatchLength) {
                auto [_, ok] = seen.emplace(path.Id());
                if (!ok) {
                    break;
                }
                // Try to choose first match in tokens sequence, not match with greater weight, to avoid jumps
                if (path.Weight() >= ctx.MatchMinWeight && (bestPath.GetTail()(0) < 0 || path.GetTail()(0) < bestPath.GetTail()(0))) {
                    bestPath = path;
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

    return bestPath;
}

} // namespace NPrivate

class TViterbiMatcher : public IMatcher {
public:
    TViterbiMatcher(int64_t blankToken, int matchLength = 0, float matchMinWeight = 0.0)
        : BlankToken_(blankToken)
        , MatchLength_(matchLength)
        , MatchMinWeight_(matchMinWeight)
    {}

    std::tuple<TTrack, TTokensRange> Match(Eigen::Map<const Eigen::MatrixXf> emission, TTokensRange tokens) const override {
        NPrivate::ValidateTokens(emission, tokens);
        auto [trellis, backtrack] = NPrivate::BuildTrellis(emission, tokens, BlankToken_);

        NPrivate::TContext ctx {
            .MatchLength = MatchLength_,
            .MatchMinWeight = MatchMinWeight_,
            .Emission = emission,
            .Backtrack = {backtrack.data(), backtrack.rows(), backtrack.cols()},
            .Tokens = tokens,
        };

        auto path = NPrivate::BacktrackNew(ctx);

        std::cout << path.Weight() << std::endl;
        if (path.Weight() < ctx.MatchMinWeight) {
            return {};
        }

        auto track = path.Track(ctx);

        if (track.empty()) {
            return {};
        }

        return {
            std::move(track),
            TTokensRange { tokens.data() + track.front()(0), tokens.data() + track.back()(0) + 1 },
        };
    }

private:
    const int64_t BlankToken_;
    const int MatchLength_;
    const float MatchMinWeight_;
};

} // namespace NTruePrompter::NRecognition

