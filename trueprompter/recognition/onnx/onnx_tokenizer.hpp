#pragma once

#include <trueprompter/recognition/tokenizer.hpp>

#include <array>
#include <stdexcept>
#include <string_view>

namespace NTruePrompter::NRecognition {

class TOnnxTokenizer : public ITokenizer {
public:
    static constexpr std::array<std::string_view, 32> Tokens {{
        "<pad>",
        "<s>",
        "</s>",
        "<unk>",
        "|",
        "E",
        "T",
        "A",
        "O",
        "N",
        "I",
        "H",
        "S",
        "R",
        "D",
        "L",
        "U",
        "M",
        "W",
        "C",
        "F",
        "G",
        "Y",
        "P",
        "B",
        "V",
        "K",
        "'",
        "X",
        "J",
        "Q",
        "Z",
    }};

    bool Apply(const std::string& text, std::vector<int64_t>* tokensOut, std::vector<size_t>* tokensOffsetsOut) override {
        tokensOut->clear();
        if (tokensOffsetsOut) {
            tokensOffsetsOut->clear();
        }

        for (char c : text) {
            auto it = std::find(Tokens.begin(), Tokens.end(), std::string_view(&c, 1));
            if (it == Tokens.end()) {
                throw std::runtime_error("Unknown char " + std::to_string((int)c));
            }
            tokensOut->emplace_back(it - Tokens.begin());
            if (tokensOffsetsOut) {
                tokensOffsetsOut->emplace_back(tokensOffsetsOut->size());
            }
        }

        return true;
    }

    std::string Lookup(int64_t token) const override {
        return std::string(Tokens.at(token));
    }

    int64_t GetBlankToken() const override {
        return 0;
    }

    int64_t GetSpaceToken() const override {
        return 4;
    }
};

} // namespace NTruePrompter::NRecognition

