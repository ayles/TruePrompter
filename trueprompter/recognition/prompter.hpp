#pragma once

#include "matcher.hpp"
#include "recognizer.hpp"
#include "tokenizer.hpp"

namespace NTruePrompter::NRecognition {

class TPrompter {
public:
    TPrompter(std::shared_ptr<IRecognizer> recognizer, std::shared_ptr<ITokenizer> tokenizer, std::shared_ptr<IMatcher> matcher)
        : Recognizer_(std::move(recognizer))
        , Tokenizer_(std::move(tokenizer))
        , Matcher_(std::move(matcher))
    {

    }

    void Update(std::span<const float> data) {
        auto emissions = Recognizer_->Update(data, &RecognizerBuf_);
        if (!emissions.size()) {
            return;
        }
        Eigen::Map<const Eigen::MatrixXf> constEmissions(emissions.data(), emissions.rows(), emissions.cols());

        size_t currentCursor = TokenCursor_;
        while (currentCursor + MinChunkTokens_ <= std::min(TokenCursor_ + MinLookAheadTokens_, Tokens_.size())) {
            size_t nextCursor = currentCursor;
            while (nextCursor < Tokens_.size() && (nextCursor - currentCursor < MinChunkTokens_ || Tokens_.at(nextCursor) != Tokenizer_->GetSpaceToken())) {
                nextCursor++;
            }

            auto [range, prob] = Matcher_->Update(constEmissions, { Tokens_.data() + currentCursor, Tokens_.data() + nextCursor }, Tokenizer_->GetBlankToken());
            // We can skip space token
            currentCursor = nextCursor + 1;

            if (prob > 0.35) {
                TokenCursor_ = currentCursor;
                std::cout << prob << " ";
                for (auto token : range) {
                    std::cout << Tokenizer_->Lookup(token);
                }
                std::cout << std::endl;
            }
        }
    }

    const std::string& GetText() const {
        return Text_;
    }

    void SetText(std::string text) {
        Text_ = std::move(text);
        Tokenizer_->Apply(Text_, &Tokens_, &TokenOffsets_);
        SetCursor(0);
    }

    size_t GetCursor() const {
        if (TokenCursor_ < TokenOffsets_.size()) {
            return std::clamp<size_t>(TokenOffsets_.at(TokenCursor_), 0, Text_.size());
        }
        return Text_.size();
    }

    void SetCursor(size_t cursor) {
        auto it = std::lower_bound(TokenOffsets_.rbegin(), TokenOffsets_.rend(), cursor).base();
        TokenCursor_ = it - TokenOffsets_.end();
    }

    auto GetRecognizer() const {
        return Recognizer_;
    }

    auto GetTokenizer() const {
        return Tokenizer_;
    }

    auto GetMatcher() const {
        return Matcher_;
    }

private:
    size_t MinChunkTokens_ = 5;
    size_t MinLookAheadTokens_ = 25;
    std::shared_ptr<IRecognizer> Recognizer_;
    std::shared_ptr<ITokenizer> Tokenizer_;
    std::shared_ptr<IMatcher> Matcher_;
    std::string Text_;
    std::vector<int64_t> Tokens_;
    std::vector<size_t> TokenOffsets_;
    size_t TokenCursor_;
    std::vector<float> RecognizerBuf_;
};

} // namespace NTruePrompter::NRecognition

