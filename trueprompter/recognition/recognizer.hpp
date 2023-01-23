#pragma once

#include <memory>
#include <string>
#include <vector>
#include <version>


namespace NTruePrompter::NRecognition {

class IRecognizer {
public:
    virtual bool Update(const float* data, size_t dataSize, int32_t sampleRate, std::vector<int64_t>* tokensOut) = 0;
    virtual void Reset() = 0;
};

class IRecognizerFactory {
public:
    virtual std::shared_ptr<IRecognizer> New(const std::string& modelName) const = 0;
};

} // NTruePrompter::NRecognition

