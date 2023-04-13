#pragma once

#include <onnxruntime/core/session/experimental_onnxruntime_cxx_api.h>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <memory>

class PhonetisaurusScript;

namespace NTruePrompter::NRecognition {

class TOnnxModel {
public:
    friend class TOnnxRecognizer;

    TOnnxModel(const std::filesystem::path& configPath, const std::filesystem::path& modelPath, const std::filesystem::path& fstPath);
    ~TOnnxModel();

    const std::unordered_map<int64_t, std::string>& GetVocab() const {
        return Vocab_;
    }

    int64_t GetBlankToken() const {
        return BlankToken_;
    }

    int64_t GetSpaceToken() const {
        return SpaceToken_;
    }

    std::vector<int64_t> Phoneticize(const std::string& word) const;

    PhonetisaurusScript& GetPhonetisaurusScript() const {
        return *PhonetisaurusScript_;
    }

private:
    nlohmann::json Config_;
    std::unordered_map<int64_t, std::string> Vocab_;
    int64_t BlankToken_ = 0;
    int64_t SpaceToken_ = 0;
    Ort::Env Env_;
    Ort::SessionOptions Opts_;
    Ort::MemoryInfo MemInfo_;
    std::unique_ptr<Ort::Session> Session_;
    std::unique_ptr<PhonetisaurusScript> PhonetisaurusScript_;
    std::unordered_map<int64_t, int64_t> Remap_;
    int64_t SampleRate_;
    int64_t FrameSize_;
    int64_t FeaturesCount_;
    std::string InputName_;
    std::string OutputName_;
};

} // namespace NTruePrompter::NRecognition

