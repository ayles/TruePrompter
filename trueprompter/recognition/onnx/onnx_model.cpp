#include "onnx_model.hpp"

#include <include/PhonetisaurusScript.h>

#include <fst/symbol-table.h>

#include <memory>

namespace NTruePrompter::NRecognition {

TOnnxModel::TOnnxModel(const std::filesystem::path& configPath, const std::filesystem::path& modelPath, const std::filesystem::path& fstPath)
    : Config_(nlohmann::json::parse(std::fstream(configPath)))
    , Env_(ORT_LOGGING_LEVEL_WARNING, "TOnnxModel")
    , Opts_()
    , MemInfo_(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault))
    , PhonetisaurusScript_(std::make_unique<PhonetisaurusScript>(fstPath))
{
    SampleRate_ = Config_.at("sampling_rate").get<int64_t>();
    FrameSize_ = Config_.at("inputs_to_logits_ratio").get<int64_t>();
    for (auto& [k, v] : Config_.at("vocab").items()) {
        Vocab_[v.get<int64_t>()] = k;
        if (k == "<pad>") {
            BlankToken_ = v.get<int64_t>();
        }
        if (k == "|" || k == " ") {
            SpaceToken_ = v.get<int64_t>();
        }
    }

    Opts_.SetIntraOpNumThreads(1);
    Opts_.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
    Session_ = std::make_unique<Ort::Session>(Env_, modelPath.c_str(), Opts_);

    if (Session_->GetInputCount() != 1) {
        throw std::runtime_error("Expected single input model");
    }

    if (Session_->GetOutputCount() != 1) {
        throw std::runtime_error("Expected single output model");
    }

    auto inputShape = Session_->GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
    if (inputShape.size() != 2 || inputShape[0] != -1 || inputShape[1] != -1) {
        throw std::runtime_error("Unexpected input shape");
    }

    auto outputShape = Session_->GetOutputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
    if (outputShape.size() != 3 || outputShape[0] != -1 || outputShape[1] != -1 || outputShape[2] <= 0) {
        throw std::runtime_error("Unexpected output shape");
    }

    FeaturesCount_ = outputShape[2];

    Ort::AllocatorWithDefaultOptions alloc;
    InputName_ = Session_->GetInputNameAllocated(0, alloc).get();
    OutputName_ = Session_->GetOutputNameAllocated(0, alloc).get();

    for (auto& [k, v] : Vocab_) {
        int64_t pk = PhonetisaurusScript_->FindOsym(v);
        PhonetisaurusRemap_[pk] = k;
    }
}

TOnnxModel::~TOnnxModel() = default;

std::vector<int64_t> TOnnxModel::Phoneticize(const std::string& word) const {
    std::vector<int64_t> res;
    auto ret = PhonetisaurusScript_->Phoneticize(word);
    for (size_t j = 0; j < ret[0].Uniques.size(); ++j) {
        res.emplace_back(PhonetisaurusRemap_.at(ret[0].Uniques[j]));
    }
    return res;
}

} // NTruePrompter::NRecognition

