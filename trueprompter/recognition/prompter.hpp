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

        auto [_, match] = Matcher_->Match(constEmissions, { Tokens_.data() + TokenCursor_, std::min(LookAheadTokens_, Tokens_.size() - TokenCursor_) });

        if (!match.empty()) {
            for (auto t : match) {
                std::cout << Tokenizer_->Lookup(t);
            }
            std::cout << std::endl;
            TokenCursor_ = std::max<size_t>(TokenCursor_, match.data() + match.size() - Tokens_.data());
        }
    }

    const std::string& GetText() const {
        return Text_;
    }

    void SetText(std::string text) {
        Text_ = std::move(text);
        Tokenizer_->Tokenize(Text_, &Tokens_, &Mapping_);
        for (auto t : Tokens_) {
            std::cout << Tokenizer_->Lookup(t);
        }
        std::cout << std::endl;
        SetCursor(0);
    }

    size_t GetCursor() const {
        if (TokenCursor_ < Mapping_.size()) {
            return std::clamp<size_t>(Mapping_.at(TokenCursor_), 0, Text_.size());
        }
        return Text_.size();
    }

    void SetCursor(size_t cursor) {
        auto it = std::lower_bound(Mapping_.rbegin(), Mapping_.rend(), cursor).base();
        TokenCursor_ = it - Mapping_.end();
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
    const size_t LookAheadTokens_ = 40;
    const std::shared_ptr<IRecognizer> Recognizer_;
    const std::shared_ptr<ITokenizer> Tokenizer_;
    const std::shared_ptr<IMatcher> Matcher_;
    std::string Text_;
    std::vector<int64_t> Tokens_;
    std::vector<size_t> Mapping_;
    std::vector<float> RecognizerBuf_;
    size_t TokenCursor_;
};

} // namespace NTruePrompter::NRecognition

