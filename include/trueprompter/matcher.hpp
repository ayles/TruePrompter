#pragma once

#include "distance_helpers.hpp"
#include "model.hpp"
#include "recognizer.hpp"

#include <tcb/span.hpp>

#include <vector>
#include <string>
#include <memory>
#include <optional>

namespace NTruePrompter {

class TSpeechPhonemesBuffer {
public:
    explicit TSpeechPhonemesBuffer(size_t fitThreshold = 150)
        : FitThreshold_(fitThreshold)
    {}

    void Update(const tcb::span<const int64_t>& phonemes) {
        Phonemes_.resize(CommittedPos_);
        Phonemes_.insert(Phonemes_.end(), phonemes.begin(), phonemes.end());
        MatchedPos_ = std::min<size_t>(MatchedPos_, Phonemes_.size());
    }

    void Commit() {
        CommittedPos_ = Phonemes_.size();
        Fit();
    }

    tcb::span<const int64_t> GetUnmatched() const {
        return tcb::span<const int64_t>(Phonemes_.data() + MatchedPos_, Phonemes_.size() - MatchedPos_);
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
    explicit TPhonemesMatcher(std::vector<int64_t> phonemes)
        : Phonemes_(std::move(phonemes))
    {
    }

    void Match(TSpeechPhonemesBuffer& speechPhonemesBuffer, size_t lookAhead = (size_t)-1) {
        auto speechPhonemes = speechPhonemesBuffer.GetUnmatched();
        auto phonemes = tcb::span<const int64_t>(Phonemes_.data() + CurrentPos_, std::min<size_t>(Phonemes_.size() - CurrentPos_, lookAhead));

        // Slightly decrease weight at the end of lookahead window - prioritize closer matches
        auto weightMultiplier = [&phonemes](const int64_t* targetPos) {
            return 1.0 - 0.3 * (double)(targetPos - phonemes.data()) / (double)phonemes.size();
        };

        // TODO match last N speech phonemes
        const auto [speechPhonemesMatched, phonemesMatched, score] = SmithWaterman<const int64_t, double>(
            speechPhonemes,
            phonemes,
            [&weightMultiplier](const int64_t*, const int64_t* targetPos) {
                return -1.0 * weightMultiplier(targetPos);
            },
            [&weightMultiplier](const int64_t*, const int64_t* targetPos) {
                return -1.0 * weightMultiplier(targetPos);
            },
            [&weightMultiplier](const int64_t* sourcePos, const int64_t* targetPos) {
                return (*sourcePos == *targetPos ? 1.0 : -1.0) * weightMultiplier(targetPos);
            }
        );

        if (score < 3.0) {
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
    TWordsMatcher(const tcb::span<const std::string>& words, const std::shared_ptr<TModel>& model)
        : Model_(model)
        , Recognizer_(std::make_unique<TRecognizer>(Model_))
        , SpeechPhonemesBuffer_(std::make_unique<TSpeechPhonemesBuffer>())
    {
        auto [phones, phoneToWord] = Model_->GetPhonetizer().Phoneticize(words);

        PhonemesMatcher_ = std::make_unique<TPhonemesMatcher>(std::move(phones));
        PhonemeIndexToWordIndex_ = std::move(phoneToWord);
    }

    void AcceptWaveform(const tcb::span<const float>& data, float sampleRate) {
        bool shouldCommit = Recognizer_->AcceptWaveform(data, sampleRate);
        SpeechPhonemesBuffer_->Update(Recognizer_->GetPhones());

        size_t lookAheadPhonemes = (size_t)-1;
        if (LookAhead_ != (size_t)-1) {
            size_t currentPos = PhonemesMatcher_->GetCurrentPos();
            if (currentPos < PhonemeIndexToWordIndex_.size()) {
                auto it = std::upper_bound(
                    PhonemeIndexToWordIndex_.begin() + currentPos,
                    PhonemeIndexToWordIndex_.end(),
                    std::pair<size_t, float>(PhonemeIndexToWordIndex_[currentPos].first + LookAhead_, 0.0f)
                );
                if (it != PhonemeIndexToWordIndex_.end()) {
                    lookAheadPhonemes = it - (PhonemeIndexToWordIndex_.begin() + currentPos);
                }
            }
        }

        PhonemesMatcher_->Match(*SpeechPhonemesBuffer_, lookAheadPhonemes);

        if (shouldCommit) {
            SpeechPhonemesBuffer_->Commit();
        }
    }

    void SetCurrentPos(const std::pair<size_t, float>& pos) {
        Recognizer_->Reset();
        SpeechPhonemesBuffer_->Reset();

        if (PhonemeIndexToWordIndex_.empty() || PhonemeIndexToWordIndex_.back().first < pos.first) {
            PhonemesMatcher_->SetCurrentPos(PhonemeIndexToWordIndex_.size());
            return;
        }

        auto range = std::equal_range(PhonemeIndexToWordIndex_.begin(), PhonemeIndexToWordIndex_.end(), pos, [](auto& w, auto& p) {
            return w.first < p.first;
        });
        if (range.first == range.second) {
            return;
        }

        auto it = std::lower_bound(range.first, range.second, pos);
        if (it == range.second) {
            return;
        }

        PhonemesMatcher_->SetCurrentPos(it - PhonemeIndexToWordIndex_.begin());
    }

    std::pair<size_t, float> GetCurrentPos() const {
        size_t pos = PhonemesMatcher_->GetCurrentPos();
        if (pos >= PhonemeIndexToWordIndex_.size()) {
            return { PhonemeIndexToWordIndex_.back().first + 1, 0.0f };
        }
        return PhonemeIndexToWordIndex_[pos];
    }

    void SetLookAhead(size_t lookAhead) {
        LookAhead_ = lookAhead;
    }

    size_t GetLookAhead() const {
        return LookAhead_;
    }

private:
    std::shared_ptr<TModel> Model_;
    std::unique_ptr<TRecognizer> Recognizer_;
    size_t LookAhead_ = (size_t)-1;

    std::unique_ptr<TSpeechPhonemesBuffer> SpeechPhonemesBuffer_;
    std::unique_ptr<TPhonemesMatcher> PhonemesMatcher_;
    std::vector<std::pair<size_t, float>> PhonemeIndexToWordIndex_;
};

}
