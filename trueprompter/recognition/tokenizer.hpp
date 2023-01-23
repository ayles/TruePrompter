#pragma once

#include <memory>
#include <span>
#include <string>
#include <vector>


namespace NTruePrompter::NRecognition {

class ITokenizer {
public:
    virtual bool Apply(const std::string& text, std::vector<int64_t>* tokensOut, std::vector<size_t>* tokensOffsetsOut = nullptr) = 0;
};

class ITokenizerFactory {
public:
    virtual std::shared_ptr<ITokenizer> New() const = 0;
};

} // NTruePrompter::NRecognition

