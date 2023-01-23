#pragma once

#include "smith_waterman.hpp"
#include "recognizer.hpp"
#include "tokenizer.hpp"

#include <vector>
#include <string>
#include <memory>
#include <optional>


namespace NTruePrompter::NRecognition {

class TSpeechPhonemesBuffer {
public:
    explicit TSpeechPhonemesBuffer(size_t fitThreshold = 150)
        : FitThreshold_(fitThreshold)
    {}

    void Update(const std::span<const int64_t>& phonemes) {
        Phonemes_.resize(CommittedPos_);
        Phonemes_.insert(Phonemes_.end(), phonemes.begin(), phonemes.end());
        MatchedPos_ = std::min<size_t>(MatchedPos_, Phonemes_.size());
    }

    void Commit() {
        CommittedPos_ = Phonemes_.size();
        Fit();
    }

    std::span<const int64_t> GetUnmatched() const {
        return std::span<const int64_t>(Phonemes_.data() + MatchedPos_, Phonemes_.size() - MatchedPos_);
    }

    void Match(size_t count) {
        MatchedPos_ = std::min<size_t>(MatchedPos_ + count, Phonemes_.size());
        Fit();
    }

    void Reset() {
        Phonemes_.clear();
        CommittedPos_ = 0;
        MatchedPos_ = 0;
    }

private:
    void Fit() {
        size_t count = std::min<size_t>(CommittedPos_, MatchedPos_);
        if (count < FitThreshold_) {
            return;
        }
        Phonemes_.erase(Phonemes_.begin(), Phonemes_.begin() + count);
        CommittedPos_ -= count;
        MatchedPos_ -= count;
    }

private:
    const size_t FitThreshold_;
    std::vector<int64_t> Phonemes_;
    size_t CommittedPos_ = 0;
    size_t MatchedPos_ = 0;
};

class TPhonemesMatcher {
public:
    struct TMatchParameters {
        std::optional<size_t> LookAhead;
        double FadeOverLookAhead = 0.3;
        double SimilarScore = 1.0;
        double DifferentScore = -1.0;
        double SourceSkipWeight = -1.0;
        double TargetSkipWeight = -1.0;
        double MinMatchWeight = 3.0;
    };

    explicit TPhonemesMatcher(std::vector<int64_t> phonemes)
        : Phonemes_(std::move(phonemes))
    {
    }

    void Match(TSpeechPhonemesBuffer& speechPhonemesBuffer, const TMatchParameters& matchParameters) {
        auto speechPhonemes = speechPhonemesBuffer.GetUnmatched();
        auto phonemes = std::span<const int64_t>(Phonemes_.data() + CurrentPos_, std::min<size_t>(Phonemes_.size() - CurrentPos_, matchParameters.LookAhead.value_or((size_t)-1)));

        // Slightly decrease weight at the end of lookahead window - prioritize closer matches
        auto weightMultiplier = [&phonemes, &matchParameters](const int64_t* targetPos) {
            return 1.0 - matchParameters.FadeOverLookAhead * (double)(targetPos - phonemes.data()) / (double)phonemes.size();
        };

        // TODO match last N speech phonemes
        const auto [speechPhonemesMatched, phonemesMatched, score] = SmithWaterman<const int64_t, double>(
            speechPhonemes,
            phonemes,
            [&weightMultiplier, &matchParameters](const int64_t*, const int64_t* targetPos) {
                return matchParameters.SourceSkipWeight * weightMultiplier(targetPos);
            },
            [&weightMultiplier, &matchParameters](const int64_t*, const int64_t* targetPos) {
                return matchParameters.TargetSkipWeight * weightMultiplier(targetPos);
            },
            [&weightMultiplier, &matchParameters](const int64_t* sourcePos, const int64_t* targetPos) {
                return (*sourcePos == *targetPos ? matchParameters.SimilarScore : matchParameters.DifferentScore) * weightMultiplier(targetPos);
            }
        );

        if (score < matchParameters.MinMatchWeight) {
            return;
        }

        speechPhonemesBuffer.Match(speechPhonemesMatched.end() - speechPhonemes.begin());
        CurrentPos_ += phonemesMatched.end() - phonemes.begin();
    }

    size_t GetCurrentPos() const {
        return CurrentPos_;
    }

    void SetCurrentPos(size_t pos) {
        CurrentPos_ = std::min<size_t>(pos, Phonemes_.size());
    }

private:
    std::vector<int64_t> Phonemes_;
    size_t CurrentPos_ = 0;
};

class TWordsMatcher {
public:
    TWordsMatcher(const std::string& text, std::shared_ptr<IRecognizer> recognizer, std::shared_ptr<ITokenizer> tokenizer)
        : Recognizer_(std::move(recognizer))
        , Tokenizer_(std::move(tokenizer))
        , SpeechPhonemesBuffer_(std::make_unique<TSpeechPhonemesBuffer>())
    {
        std::vector<int64_t> phones;
        std::vector<size_t> phoneToWord;
        Tokenizer_->Apply(text, &phones, &phoneToWord);

        PhonemesMatcher_ = std::make_unique<TPhonemesMatcher>(std::move(phones));
        PhonemeIndexToTextIndex_ = std::move(phoneToWord);
    }

    void AcceptWaveform(const float* data, size_t dataSize, int32_t sampleRate) {
        std::vector<int64_t> phonemes;
        bool shouldCommit = Recognizer_->Update(data, dataSize, sampleRate, &phonemes);
        SpeechPhonemesBuffer_->Update(phonemes);

        TPhonemesMatcher::TMatchParameters matchParameters = MatchParameters_;
        if (matchParameters.LookAhead) {
            size_t currentPos = PhonemesMatcher_->GetCurrentPos();
            if (currentPos < PhonemeIndexToTextIndex_.size()) {
                auto it = std::upper_bound(
                    PhonemeIndexToTextIndex_.begin() + currentPos,
                    PhonemeIndexToTextIndex_.end(),
                    PhonemeIndexToTextIndex_[currentPos] + matchParameters.LookAhead.value()
                );
                if (it != PhonemeIndexToTextIndex_.end()) {
                    // Convert N-characters to N-phonemes lookahead
                    matchParameters.LookAhead = it - (PhonemeIndexToTextIndex_.begin() + currentPos);
                }
            }
        }

        PhonemesMatcher_->Match(*SpeechPhonemesBuffer_, matchParameters);

        if (shouldCommit) {
            SpeechPhonemesBuffer_->Commit();
        }
    }

    void SetCurrentPos(size_t pos) {
        Recognizer_->Reset();
        SpeechPhonemesBuffer_->Reset();

        if (PhonemeIndexToTextIndex_.empty() || PhonemeIndexToTextIndex_.back() < pos) {
            PhonemesMatcher_->SetCurrentPos(PhonemeIndexToTextIndex_.size());
            return;
        }

        auto range = std::equal_range(PhonemeIndexToTextIndex_.begin(), PhonemeIndexToTextIndex_.end(), pos, [](auto& w, auto& p) {
            return w < p;
        });
        if (range.first == range.second) {
            return;
        }

        auto it = std::lower_bound(range.first, range.second, pos);
        if (it == range.second) {
            return;
        }

        PhonemesMatcher_->SetCurrentPos(it - PhonemeIndexToTextIndex_.begin());
    }

    size_t GetCurrentPos() const {
        if (PhonemeIndexToTextIndex_.empty()) {
            return 0;
        }
        size_t pos = PhonemesMatcher_->GetCurrentPos();
        if (pos >= PhonemeIndexToTextIndex_.size()) {
            return PhonemeIndexToTextIndex_.back() + 1;
        }
        return PhonemeIndexToTextIndex_[pos];
    }

    void SetMatchParameters(const TPhonemesMatcher::TMatchParameters& matchParameters) {
        MatchParameters_ = matchParameters;
    }

    const TPhonemesMatcher::TMatchParameters& GetMatchParameters() const {
        return MatchParameters_;
    }

private:
    TPhonemesMatcher::TMatchParameters MatchParameters_;

    std::shared_ptr<IRecognizer> Recognizer_;
    std::shared_ptr<ITokenizer> Tokenizer_;

    std::unique_ptr<TSpeechPhonemesBuffer> SpeechPhonemesBuffer_;
    std::unique_ptr<TPhonemesMatcher> PhonemesMatcher_;
    std::vector<size_t> PhonemeIndexToTextIndex_;
};

} // namespace NTruePrompter::NRecognition
