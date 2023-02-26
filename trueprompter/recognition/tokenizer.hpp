#pragma once

#include <string>
#include <vector>

namespace NTruePrompter::NRecognition {

class ITokenizer {
public:
    virtual bool Apply(const std::string& text, std::vector<int64_t>* tokensOut, std::vector<size_t>* tokensOffsetsOut = nullptr) = 0;
    virtual std::string Lookup(int64_t token) const = 0;
    virtual int64_t GetBlankToken() const = 0;
    virtual int64_t GetSpaceToken() const = 0;
    virtual ~ITokenizer() = default;
};

} // NTruePrompter::NRecognition

