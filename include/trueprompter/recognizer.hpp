#pragma once

#include "model.hpp"

#include <lat/phone-align-lattice.h>
#include <lat/sausages.h>
#include <online2/online-nnet3-incremental-decoding.h>

#include <tcb/span.hpp>

#include <memory>

namespace NTruePrompter {

class TRecognizer {
public:
    TRecognizer(std::shared_ptr<TModel> model)
        : Model_(std::move(model))
        , FeaturePipeline_(std::make_unique<kaldi::OnlineNnet2FeaturePipeline>(Model_->FeatureInfo_))
        , FST_(fst::LookaheadComposeFst(*Model_->HCL_, *Model_->G_, Model_->Disambig_))
    {
        Decoder_ = std::make_unique<kaldi::SingleUtteranceNnet3IncrementalDecoder>(
            Model_->DecodingConfig_,
            *Model_->TransitionModel_,
            *Model_->DecodableInfo_,
            *FST_,
            FeaturePipeline_.get());
    }

    bool AcceptWaveform(const tcb::span<const float> data, float sampleRate) {
        if (!SilenceWeighting_) {
            SilenceWeighting_ = std::make_unique<kaldi::OnlineSilenceWeighting>(
                *Model_->TransitionModel_,
                Model_->FeatureInfo_.silence_weighting_config, 3);
        }

        size_t chunkSize = sampleRate * 0.2f;

        kaldi::Vector<kaldi::BaseFloat> vec;
        vec.Resize(chunkSize, kaldi::kUndefined);

        for (size_t i = 0; i < data.size(); i += chunkSize) {
            size_t currentChunkSize = 0;
            for ( ; currentChunkSize < chunkSize && i + currentChunkSize < data.size(); ++currentChunkSize) {
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

        if (Decoder_->EndpointDetected(Model_->EndpointConfig_)) {
            Reset();
            return true;
        }

        return false;
    }

    void Reset() {
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
        kaldi::PhoneAlignLattice(compactLattice, *Model_->TransitionModel_, opts, &phoneAlignedLattice);

        kaldi::MinimumBayesRisk mbr(phoneAlignedLattice);

        auto rawRes = mbr.GetOneBest();

        std::vector<int64_t> res;
        res.reserve(rawRes.size());

        for (auto phone : rawRes) {
            auto it = Model_->KaldiToPhonetisaurusPhoneMapping_.find(phone);
            if (it != Model_->KaldiToPhonetisaurusPhoneMapping_.end()) {
                res.emplace_back(it->second);
            }
        }

        return res;
    }

private:
    std::shared_ptr<TModel> Model_;

    std::unique_ptr<fst::LookaheadFst<fst::StdArc, int32>> FST_;
    std::unique_ptr<kaldi::OnlineNnet2FeaturePipeline> FeaturePipeline_;
    std::unique_ptr<kaldi::OnlineSilenceWeighting> SilenceWeighting_;
    std::unique_ptr<kaldi::SingleUtteranceNnet3IncrementalDecoder> Decoder_;
    int32_t FrameOffset_ = 0;
};

}
