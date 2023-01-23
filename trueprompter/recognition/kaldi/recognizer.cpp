#include "kaldi.hpp"
#include "model.hpp"

#include <trueprompter/recognition/recognizer.hpp>


namespace {

class TKaldiRecognizer : public NTruePrompter::NRecognition::IRecognizer {
public:
    TKaldiRecognizer(std::shared_ptr<NTruePrompter::NRecognition::TKaldiModel> model)
        : Model_(std::move(model))
    {
        std::tie(FeaturePipeline_, Decoder_) = Model_->CreateFeaturePipelineAndDecoder();
    }

    bool Update(const float* data, size_t dataSize, int32_t sampleRate, std::vector<int64_t>* tokensOut) override {
        if (!SilenceWeighting_) {
            SilenceWeighting_ = Model_->CreateSilenceWeighting();
        }

        size_t chunkSize = sampleRate * 0.2f;

        kaldi::Vector<kaldi::BaseFloat> vec;
        vec.Resize(chunkSize, kaldi::kUndefined);

        for (size_t i = 0; i < dataSize; i += chunkSize) {
            size_t currentChunkSize = 0;
            for ( ; currentChunkSize < chunkSize && i + currentChunkSize < dataSize; ++currentChunkSize) {
                vec(currentChunkSize) = data[i + currentChunkSize] * 32767.0f; // Kaldi wants it
            }
            vec.Resize(currentChunkSize, kaldi::MatrixResizeType::kCopyData);

            FeaturePipeline_->AcceptWaveform(sampleRate, vec);

            if (SilenceWeighting_->Active() && FeaturePipeline_->NumFramesReady() > 0
                && FeaturePipeline_->IvectorFeature() != nullptr)
            {
                std::vector<std::pair<int32_t, kaldi::BaseFloat>> deltaWeights;
                SilenceWeighting_->ComputeCurrentTraceback(Decoder_->Decoder());
                SilenceWeighting_->GetDeltaWeights(FeaturePipeline_->NumFramesReady(), FrameOffset_ * 3, &deltaWeights);
                FeaturePipeline_->UpdateFrameWeights(deltaWeights);
            }

            Decoder_->AdvanceDecoding();
        }

        if (Decoder_->EndpointDetected(Model_->GetEndpointConfig())) {
            Reset();
            return true;
        }

        // TODO
        *tokensOut = GetPhones();

        return false;
    }

    void Reset() override {
        Decoder_->FinalizeDecoding();
        FrameOffset_ += Decoder_->NumFramesDecoded();
        Decoder_->InitDecoding(FrameOffset_);
        SilenceWeighting_.reset();
    }

    std::vector<int64_t> GetPhones() const {
        if (!Decoder_->NumFramesInLattice()) {
            return {};
        }

        const kaldi::CompactLattice& compactLattice = Decoder_->GetLattice(Decoder_->NumFramesInLattice(), false);

        kaldi::CompactLattice phoneAlignedLattice;
        kaldi::PhoneAlignLatticeOptions opts;
        opts.replace_output_symbols = true;

        fst::ScaleLattice(fst::GraphLatticeScale(0.0), &phoneAlignedLattice);
        kaldi::PhoneAlignLattice(compactLattice, *Model_->GetTransitionModel(), opts, &phoneAlignedLattice);

        kaldi::MinimumBayesRisk mbr(phoneAlignedLattice);

        auto rawRes = mbr.GetOneBest();

        std::vector<int64_t> res;
        res.reserve(rawRes.size());

        for (auto phone : rawRes) {
            auto remappedPhone = Model_->RemapPhone(phone);
            if (remappedPhone) {
                res.emplace_back(*remappedPhone);
            }
        }

        return res;
    }

private:
    std::shared_ptr<NTruePrompter::NRecognition::TKaldiModel> Model_;

    std::unique_ptr<kaldi::OnlineNnet2FeaturePipeline> FeaturePipeline_;
    std::unique_ptr<kaldi::OnlineSilenceWeighting> SilenceWeighting_;
    std::unique_ptr<kaldi::SingleUtteranceNnet3IncrementalDecoder> Decoder_;
    int32_t FrameOffset_ = 0;
};

class TKaldiRecognizerFactory : public NTruePrompter::NRecognition::IRecognizerFactory {
public:
    TKaldiRecognizerFactory(std::unordered_map<std::string, std::shared_ptr<NTruePrompter::NRecognition::TKaldiModel>> models)
        : Models_(std::move(models))
    {}

    std::shared_ptr<NTruePrompter::NRecognition::IRecognizer> New(const std::string& modelName) const override {
        return std::make_shared<TKaldiRecognizer>(Models_.at(modelName));
    }

private:
    std::unordered_map<std::string, std::shared_ptr<NTruePrompter::NRecognition::TKaldiModel>> Models_;
};

} // namespace

namespace NTruePrompter::NRecognition {

std::shared_ptr<IRecognizerFactory> NewKaldiRecognizerFactory(std::unordered_map<std::string, std::shared_ptr<TKaldiModel>> models) {
    return std::make_shared<TKaldiRecognizerFactory>(std::move(models));
}

} // NTruePrompter::NRecognition

