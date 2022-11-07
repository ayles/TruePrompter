#pragma once

#include <cstddef>
#include <vector>
#include <tuple>
#include <filesystem>
#include <string>
#include <memory>


namespace NTruePrompter::NRecognition {


class IRecognizer {
public:
    virtual bool AcceptWaveform(const float* data, size_t dataSize, float sampleRate) = 0;
    virtual void Reset() = 0;
    virtual std::vector<int64_t> GetPhones() const = 0;
    virtual std::tuple<std::vector<int64_t>, std::vector<size_t>> MapToPhones(const std::string& text) const = 0;
};


class IRecognizerFactory {
public:
    virtual std::shared_ptr<IRecognizer> Create() const = 0;
};


std::shared_ptr<IRecognizerFactory> CreateRecognizerFactory(const std::filesystem::path& path);


} // namespace NTruePrompter::NServer
