#include "onnx_tokenizer.hpp"

#include <utf8.h>

namespace NTruePrompter::NRecognition {

void TOnnxTokenizer::Tokenize(const std::string& text, std::vector<int64_t>* tokensOut, std::vector<size_t>* mappingOut) {
    if (!utf8::is_valid(text.begin(), text.end())) {
        throw std::runtime_error("Text is not valid utf-8 string");
    }

    tokensOut->clear();
    if (mappingOut) {
        mappingOut->clear();
    }

    auto isWordSymbol = [](uint32_t c) {
        // TODO proper spaces tracking
        return c > std::numeric_limits<unsigned char>::max() || !std::isspace((unsigned char)c);
    };

    auto flush = [&](const std::string& s, size_t from, size_t to) {
        auto ret = Model_->Phoneticize(s);
        for (size_t j = 0; j < ret.size(); ++j) {
            tokensOut->emplace_back(ret[j]);
            if (mappingOut) {
                mappingOut->emplace_back(std::min<size_t>(to - from - 1, (to - from) * j / ret.size()) + from);
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
            tokensOut->emplace_back(GetSpaceToken());
            if (mappingOut) {
                mappingOut->emplace_back(currentPos);
            }
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
}

} // NTruePrompter::NRecognition

