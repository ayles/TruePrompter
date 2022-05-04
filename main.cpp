#include "model.h"
#include "recognizer.h"
#include "distance_helpers.h"

#include <online/online-audio-source.h>
#include <include/PhonetisaurusScript.h>

#include <iostream>

class TPhonemesMatcher {
public:
    explicit TPhonemesMatcher(std::vector<int32_t> phonemes)
        : Phonemes_(std::move(phonemes))
    {
    }

    // Returns range of matched phonemes
    std::pair<size_t, size_t> Match(const tcb::span<int32_t>& phonemes) {
        auto ret = SmithWaterman<int32_t, double>(
            phonemes,
            tcb::span(Phonemes_.data() + CommittedPhonemesPos_, Phonemes_.size() - CommittedPhonemesPos_),
            [](const int32_t&) {
                return -1.0;
            },
            [](const int32_t&) {
                return -1.0;
            },
            [](const int32_t& a, const int32_t& b) {
                return a == b ? 1.0 : -1.0;
            }
        );

        if (ret.empty() || std::get<2>(ret.back()) < 3.0) {
            return { 0, 0 };
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
            return { std::get<0>(*firstIt), std::get<0>(*lastIt) - std::get<0>(*firstIt) + 1 };
        }

        return { 0, 0 };
    }

    size_t GetCurrentPos() const {
        return CurrentPhonemesPos_;
    }

    void Commit() {
        CommittedPhonemesPos_ = CurrentPhonemesPos_;
    }

private:
    std::vector<int32_t> Phonemes_;
    size_t CurrentPhonemesPos_ = 0;
    size_t CommittedPhonemesPos_ = 0;
};

class TTextMatcher {
public:
    explicit TTextMatcher(const std::string& text, const std::string& modelPath)
        : Model_(std::make_shared<TModel>(modelPath))
        , Recognizer_(std::make_unique<TRecognizer>(Model_, 48000))
        , PhonetisaurusDecoder_(std::make_unique<PhonetisaurusScript>(modelPath + "/ru.fst"))
    {
        std::unordered_set<char> skipTokens = { ' ', ',', '.', '?', '!' };
        for (size_t i = 0; i < text.size(); ++i) {
            if (skipTokens.count(text[i])) {
                continue;
            }
            auto it = std::find_first_of(text.begin() + i, text.end(), skipTokens.begin(), skipTokens.end());
            Words_.emplace_back(text.substr(i, it - text.begin() - i));
            i = it - text.begin();
        }
        PhonetisaurusScript phonetisaurusDecoder(modelPath + "/ru.fst");
        std::vector<int32_t> phonemes;
        for (size_t i = 0; i < Words_.size(); ++i) {
            auto dec = phonetisaurusDecoder.Phoneticize(Words_[i]);
            for (auto p : dec[0].Uniques) {
                phonemes.emplace_back(p);
                PhonemeIndexToWordIndex_.emplace_back(i);
            }
        }

        PhonemesMatcher_ = std::make_unique<TPhonemesMatcher>(std::move(phonemes));
    }

    void AcceptWaveform(const float* data, size_t size) {
        bool shouldCommit = Recognizer_->AcceptWaveform(data, size);
        SpeechPhonemes_.resize(CommittedSpeechPhonemesPos_);
        auto phones = RemapPhones(Recognizer_->GetPhones());
        SpeechPhonemes_.insert(SpeechPhonemes_.end(), phones.begin(), phones.end());

        size_t window = std::min<size_t>({ SpeechPhonemes_.size() - CurrentSpeechPhonemesPos_, 20 });
        auto matchedRange = PhonemesMatcher_->Match(tcb::span(SpeechPhonemes_.data() + SpeechPhonemes_.size() - window, window));

        if (shouldCommit) {
            CommittedSpeechPhonemesPos_ = SpeechPhonemes_.size();
            PhonemesMatcher_->Commit();

            if (matchedRange.second) {
                CurrentSpeechPhonemesPos_ = SpeechPhonemes_.size() - window + matchedRange.first + matchedRange.second;
            }
        }
    }

    size_t GetCurrentPos() const {
        size_t pos = PhonemesMatcher_->GetCurrentPos();
        if (pos >= PhonemeIndexToWordIndex_.size()) {
            return PhonemeIndexToWordIndex_.back() + 1;
        }
        return PhonemeIndexToWordIndex_[pos];
    }

    const std::vector<std::string>& GetWords() const {
        return Words_;
    }

private:
    std::vector<int32_t> RemapPhones(std::vector<int32_t> phones) {
        size_t newPos = 0;
        for (int32_t& phone : phones) {
            std::string phoneSym = Model_->GetPhoneSym(phone);
            auto underscorePos = phoneSym.find_first_of('_');
            if (underscorePos == std::string::npos) {
                continue;
            }
            phoneSym.resize(underscorePos);
            auto newPhone = PhonetisaurusDecoder_->osyms_->Find(phoneSym);
            if (newPhone != kNoSymbol) {
                phones[newPos++] = newPhone;
            }
        }
        phones.resize(newPos);
        return phones;
    }

private:
    std::shared_ptr<TModel> Model_;
    std::unique_ptr<TRecognizer> Recognizer_;
    std::unique_ptr<PhonetisaurusScript> PhonetisaurusDecoder_;

    std::vector<int32_t> SpeechPhonemes_;
    size_t CurrentSpeechPhonemesPos_ = 0;
    size_t CommittedSpeechPhonemesPos_ = 0;

    std::unique_ptr<TPhonemesMatcher> PhonemesMatcher_;
    std::vector<size_t> PhonemeIndexToWordIndex_;
    std::vector<std::string> Words_;
};

static constexpr size_t SampleRate = 48000;

int main() {
    // Needs ICU or smth alike to make Lower()
    std::string text = "карл у клары украл кораллы, клара у карла украла кларнет";

    std::cerr << "Loading..." << std::endl;
    TTextMatcher textMatcher(text, "../small_model");
    std::cerr << "Loaded" << std::endl;

    kaldi::OnlinePaSource pa(0, SampleRate, SampleRate * 10, 0);
    kaldi::Vector<kaldi::BaseFloat> vec;
    vec.Resize(SampleRate / 2);

    while (true) {
        if (!pa.Read(&vec)) {
            continue;
        }

        textMatcher.AcceptWaveform(vec.Data(), vec.Dim());

        std::cout << "\r";
        for (size_t i = 0; i < textMatcher.GetWords().size(); ++i) {
            if (i < textMatcher.GetCurrentPos()) {
                std::cout << "\x1B[31m" << textMatcher.GetWords()[i] << "\x1B[0m";
            } else {
                std::cout << textMatcher.GetWords()[i];
            }
            std::cout << " ";
        }
    }

    return 0;
}
