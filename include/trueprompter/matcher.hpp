#pragma once

#include "distance_helpers.hpp"
#include "model.hpp"
#include "recognizer.hpp"

#include <tcb/span.hpp>

#include <vector>
#include <string>
#include <memory>

namespace NTruePrompter {

class TPhonemesMatcher {
public:
    explicit TPhonemesMatcher(std::vector<int64_t> phonemes)
        : Phonemes_(std::move(phonemes))
    {
    }

    // Returns range of matched phonemes
    tcb::span<const int64_t> Match(const tcb::span<const int64_t>& phonemes) {
        auto ret = SmithWaterman<const int64_t, double>(
            phonemes,
            tcb::span<const int64_t>(Phonemes_.data() + CommittedPhonemesPos_, Phonemes_.size() - CommittedPhonemesPos_),
            [](const int64_t&) {
                return -1.0;
            },
            [](const int64_t&) {
                return -1.0;
            },
            [](const int64_t& a, const int64_t& b) {
                return a == b ? 1.0 : -1.0;
            }
        );

        if (ret.empty() || std::get<2>(ret.back()) < 3.0) {
            return {};
        }

        auto it = find_if(ret.rbegin(), ret.rend(), [](const auto& x) {
            return std::get<1>(x) != -1;
        });

        if (it != ret.rend()) {
            CurrentPhonemesPos_ = std::max<size_t>(CurrentPhonemesPos_, CommittedPhonemesPos_ + std::get<1>(*it) + 1);
            auto firstIt = find_if(ret.begin(), ret.end(), [](const auto& x) {
                return std::get<0>(x) != -1;
            });
            auto lastIt = find_if(ret.rbegin(), ret.rend(), [](const auto& x) {
                return std::get<0>(x) != -1;
            });
            return tcb::span<const int64_t>(phonemes.data() + std::get<0>(*firstIt), phonemes.data() + std::get<0>(*lastIt) + 1);
        }

        return {};
    }

    size_t GetCurrentPos() const {
        return CurrentPhonemesPos_;
    }

    void Commit() {
        CommittedPhonemesPos_ = CurrentPhonemesPos_;
    }

    void SetCurrentPos(size_t pos) {
        CommittedPhonemesPos_ = std::min<size_t>(pos, Phonemes_.size());
        CurrentPhonemesPos_ = CommittedPhonemesPos_;
    }

private:
    std::vector<int64_t> Phonemes_;
    size_t CurrentPhonemesPos_ = 0;
    size_t CommittedPhonemesPos_ = 0;
};

class TWordsMatcher {
public:
    TWordsMatcher(const tcb::span<const std::string>& words, const std::shared_ptr<TModel>& model)
        : Model_(model)
        , Recognizer_(std::make_unique<TRecognizer>(Model_))
    {
        auto [phones, phoneToWord] = Model_->GetPhonetizer().Phoneticize(words);

        PhonemesMatcher_ = std::make_unique<TPhonemesMatcher>(std::move(phones));
        PhonemeIndexToWordIndex_ = std::move(phoneToWord);
    }

    void AcceptWaveform(const tcb::span<const float>& data, float sampleRate) {
        bool shouldCommit = Recognizer_->AcceptWaveform(data, sampleRate);
        auto phones = Recognizer_->GetPhones();
        SpeechPhonemes_.resize(CommittedSpeechPhonemesPos_);
        SpeechPhonemes_.insert(SpeechPhonemes_.end(), phones.begin(), phones.end());

        size_t window = std::min<size_t>({ SpeechPhonemes_.size() - CurrentSpeechPhonemesPos_, 20 });
        auto matchedRange = PhonemesMatcher_->Match(tcb::span(SpeechPhonemes_.data() + SpeechPhonemes_.size() - window, window));

        if (shouldCommit) {
            CommittedSpeechPhonemesPos_ = SpeechPhonemes_.size();
            PhonemesMatcher_->Commit();

            if (!matchedRange.empty()) {
                CurrentSpeechPhonemesPos_ = matchedRange.data() + matchedRange.size() - SpeechPhonemes_.data();
            }
        }
    }

    void SetCurrentPos(const std::pair<size_t, float>& pos) {
        if (!PhonemeIndexToWordIndex_.empty() && PhonemeIndexToWordIndex_.back().first < pos.first) {
            PhonemesMatcher_->SetCurrentPos(PhonemeIndexToWordIndex_.size());
            Recognizer_->Reset();
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
        Recognizer_->Reset();
    }

    std::pair<size_t, float> GetCurrentPos() const {
        size_t pos = PhonemesMatcher_->GetCurrentPos();
        if (pos >= PhonemeIndexToWordIndex_.size()) {
            return { PhonemeIndexToWordIndex_.back().first + 1, 0.0f };
        }
        return PhonemeIndexToWordIndex_[pos];
    }

private:
    std::shared_ptr<TModel> Model_;
    std::unique_ptr<TRecognizer> Recognizer_;

    std::vector<int64_t> SpeechPhonemes_;
    size_t CurrentSpeechPhonemesPos_ = 0;
    size_t CommittedSpeechPhonemesPos_ = 0;

    std::unique_ptr<TPhonemesMatcher> PhonemesMatcher_;
    std::vector<std::pair<size_t, float>> PhonemeIndexToWordIndex_;
};

}
