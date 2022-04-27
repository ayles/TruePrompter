#pragma once

#include <lat/phone-align-lattice.h>
#include <lat/sausages.h>
#include <online2/online-nnet3-incremental-decoding.h>

#include "new_model.h"

#include <memory>

class TRecognizer {
public:
    TRecognizer(std::shared_ptr<TModel> model, float samplingRate)
        : Model_(model)
        , SamplingRate_(samplingRate)
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

    bool AcceptWaveform(const float* data, size_t size) {
        size_t chunkSize = SamplingRate_ * 0.2f;

        kaldi::Vector<kaldi::BaseFloat> vec;
        vec.Resize(chunkSize, kaldi::kUndefined);

        for (size_t i = 0; i < size; i += chunkSize) {
            size_t currentChunkSize = 0;
            for ( ; currentChunkSize < chunkSize && i + currentChunkSize < size; ++currentChunkSize) {
                vec(currentChunkSize) = data[i + currentChunkSize];
            }
            vec.Resize(currentChunkSize, kaldi::MatrixResizeType::kCopyData);

            FeaturePipeline_->AcceptWaveform(SamplingRate_, vec);
            //UpdateSilenceWeights();
            Decoder_->AdvanceDecoding();
        }

        if (Decoder_->EndpointDetected(Model_->EndpointConfig_)) {
            return true;
        }

        return false;
    }

    std::vector<int32_t> GetWords() const {
        return {};
    }

    std::vector<int32_t> GetPhones() const {
        if (!Decoder_->NumFramesInLattice()) {
            return {};
        }

        const kaldi::CompactLattice& compactLattice = Decoder_->GetLattice(Decoder_->NumFramesInLattice(), false);

        kaldi::CompactLattice phoneAlignedLattice;
        kaldi::PhoneAlignLatticeOptions opts;
        opts.replace_output_symbols = true;

        kaldi::PhoneAlignLattice(compactLattice, *Model_->TransitionModel_, opts, &phoneAlignedLattice);
        fst::ScaleLattice(fst::GraphLatticeScale(0.0), &phoneAlignedLattice);

        kaldi::MinimumBayesRisk mbr(phoneAlignedLattice);
        return mbr.GetOneBest();
    }

private:
    std::shared_ptr<TModel> Model_;
    const float SamplingRate_;

    std::unique_ptr<fst::LookaheadFst<fst::StdArc, int32>> FST_;
    std::unique_ptr<kaldi::OnlineNnet2FeaturePipeline> FeaturePipeline_;
    std::unique_ptr<kaldi::SingleUtteranceNnet3IncrementalDecoder> Decoder_;
};
