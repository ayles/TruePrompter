#pragma once

#include "onnx_model.hpp"

#include <trueprompter/recognition/tokenizer.hpp>

#include <stdexcept>

namespace NTruePrompter::NRecognition {

class TOnnxTokenizer : public ITokenizer {
public:
    TOnnxTokenizer(std::shared_ptr<TOnnxModel> model)
        : Model_(std::move(model))
    {}

    void Tokenize(const std::string& text, std::vector<int64_t>* tokensOut, std::vector<size_t>* mappingOut) override;

    std::string Lookup(int64_t token) const override {
        return Model_->GetVocab().at(token);
    }

    int64_t GetBlankToken() const override {
        return Model_->GetBlankToken();
    }

    int64_t GetSpaceToken() const override {
        return Model_->GetSpaceToken();
    }

private:
    std::shared_ptr<TOnnxModel> Model_;
};

} // namespace NTruePrompter::NRecognition

