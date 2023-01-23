#include "kaldi.hpp"
#include "model.hpp"

#include <trueprompter/recognition/tokenizer.hpp>

#include <utf8.h>

#include <filesystem>

namespace {

class TKaldiTokenizer : public NTruePrompter::NRecognition::ITokenizer {
public:
    TKaldiTokenizer(std::shared_ptr<NTruePrompter::NRecognition::TKaldiModel> model)
        : Model_(std::move(model))
    {}

    bool Apply(const std::string& text, std::vector<int64_t>* tokensOut, std::vector<size_t>* tokensOffsetsOut) override {
        if (!utf8::is_valid(text.begin(), text.end())) {
            throw std::runtime_error("Text is not valid utf-8 string");
        }

        tokensOut->clear();
        if (tokensOffsetsOut) {
            tokensOffsetsOut->clear();
        }

        auto isWordSymbol = [](uint32_t c) {
            // TODO proper spaces tracking
            return c > std::numeric_limits<unsigned char>::max() || !std::isspace((unsigned char)c);
        };

        auto flush = [this, &tokensOut, &tokensOffsetsOut](const std::string& s, size_t from, size_t to) {
            auto ret = Model_->Phoneticize(s);
            for (size_t j = 0; j < ret.size(); ++j) {
                tokensOut->emplace_back(ret[j]);
                if (tokensOffsetsOut) {
                    tokensOffsetsOut->emplace_back(std::min<size_t>(to - from - 1, (to - from) * j / ret.size()) + from);
                }
            }
        };

        size_t wordPos = 0;
        size_t currentPos = 0;
        auto wordIt = text.begin();
        auto currentIt = text.begin();
        while (currentIt != text.end()) {
            uint32_t c = utf8::peek_next(currentIt, text.end());
            if (isWordSymbol(c)) {
                utf8::next(currentIt, text.end());
                currentPos++;
            } else {
                flush(std::string(wordIt, currentIt), wordPos, currentPos);
                wordPos = currentPos;
                wordIt = currentIt;
                while (currentIt != text.end()) {
                    currentPos++;
                    if (isWordSymbol(utf8::next(currentIt, text.end()))) {
                        break;
                    }
                    wordPos = currentPos;
                    wordIt = currentIt;
                }
            }
        }
        flush(std::string(wordIt, currentIt), wordPos, currentPos);

        return true;
    }

private:
    std::shared_ptr<NTruePrompter::NRecognition::TKaldiModel> Model_;
};

class TKaldiTokenizerFactory : public NTruePrompter::NRecognition::ITokenizerFactory {
public:
    TKaldiTokenizerFactory(std::unordered_map<std::string, std::shared_ptr<NTruePrompter::NRecognition::TKaldiModel>> models) 
    {
        for (auto& [modelName, model] : models) {
            Tokenizers_[modelName] = std::make_shared<TKaldiTokenizer>(std::move(model));
        }
    }

    std::shared_ptr<NTruePrompter::NRecognition::ITokenizer> New(const std::string& modelName) const {
        return Tokenizers_.at(modelName);
    }

private:
    std::unordered_map<std::string, std::shared_ptr<TKaldiTokenizer>> Tokenizers_;
};

} // namespace

namespace NTruePrompter::NRecognition {

std::shared_ptr<ITokenizerFactory> NewKaldiTokenizerFactory(std::unordered_map<std::string, std::shared_ptr<TKaldiModel>> models) {
    return std::make_shared<TKaldiTokenizerFactory>(std::move(models));
}

} // namespace NTruePrompter::NRecognition

