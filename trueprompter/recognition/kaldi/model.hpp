#pragma once

#include <fst/extensions/ngram/ngram-fst.h>
#include <fst/fst.h>
#include <fst/register.h>
#include <fst/symbol-table.h>

#include <decoder/lattice-incremental-decoder.h>
#include <hmm/transition-model.h>
#include <lat/phone-align-lattice.h>
#include <lat/sausages.h>
#include <nnet3/am-nnet-simple.h>
#include <nnet3/decodable-simple-looped.h>
#include <nnet3/nnet-utils.h>
#include <online2/online-endpoint.h>
#include <online2/online-nnet2-feature-pipeline.h>
#include <online2/online-nnet3-incremental-decoding.h>

#include <filesystem>
#include <optional>
#include <unordered_map>


class PhonetisaurusScript;

namespace NTruePrompter::NRecognition {

class TKaldiModel {
public:
    TKaldiModel(const std::filesystem::path& path);
    virtual ~TKaldiModel() = default;

    PhonetisaurusScript& GetPhonetisaurusDecoder() const {
        return *PhonetisaurusDecoder_;
    }

    const fst::Fst<fst::StdArc>* GetFst() const {
        return HCLG_.get();
    }

    const kaldi::TransitionModel* GetTransitionModel() const {
        return TransitionModel_.get();
    }

    const kaldi::OnlineEndpointConfig& GetEndpointConfig() const {
        return EndpointConfig_;
    }

    std::optional<int64_t> RemapPhone(int64_t phone) const {
        auto it = KaldiToPhonetisaurusPhoneMapping_.find(phone);
        if (it != KaldiToPhonetisaurusPhoneMapping_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    virtual std::vector<int64_t> Phoneticize(const std::string& word) const;

    std::unique_ptr<kaldi::OnlineSilenceWeighting> CreateSilenceWeighting() const {
        return std::make_unique<kaldi::OnlineSilenceWeighting>(*TransitionModel_, FeatureInfo_.silence_weighting_config, 3);
    }

    auto CreateFeaturePipelineAndDecoder() const {
        auto featurePipeline = std::make_unique<kaldi::OnlineNnet2FeaturePipeline>(FeatureInfo_);
        auto decoder = std::make_unique<kaldi::SingleUtteranceNnet3IncrementalDecoder>(
            DecodingConfig_,
            *TransitionModel_,
            *DecodableInfo_,
            *HCLG_,
            featurePipeline.get()
        );
        return std::tuple(std::move(featurePipeline), std::move(decoder));
    }

private:
    static std::unordered_map<int64_t, int64_t> MakeKaldiToPhonetisaurusPhoneMapping(const fst::SymbolTable& kaldiSymbols, const fst::SymbolTable& phonetisaurusSymbols) {
        std::unordered_map<int64_t, int64_t> mapping;

        for (fst::SymbolTableIterator it(kaldiSymbols); !it.Done(); it.Next()) {
            std::string symbol = it.Symbol();
            auto underscorePos = symbol.find_first_of('_');
            if (underscorePos == std::string::npos) {
                continue;
            }
            symbol.resize(underscorePos);
            auto phonetisaurusPhone = phonetisaurusSymbols.Find(symbol);
            if (phonetisaurusPhone != fst::kNoSymbol) {
                mapping[it.Value()] = phonetisaurusPhone;
            }
        }

        return mapping;
    }

private:
    std::unique_ptr<PhonetisaurusScript> PhonetisaurusDecoder_;

    std::unordered_map<int64_t, int64_t> KaldiToPhonetisaurusPhoneMapping_;

    std::unique_ptr<kaldi::TransitionModel> TransitionModel_;
    std::unique_ptr<kaldi::nnet3::AmNnetSimple> NNet_;

    kaldi::LatticeIncrementalDecoderConfig DecodingConfig_;
    kaldi::OnlineEndpointConfig EndpointConfig_;
    kaldi::nnet3::NnetSimpleLoopedComputationOptions DecodableOpts_;
    kaldi::OnlineNnet2FeaturePipelineInfo FeatureInfo_;

    std::unique_ptr<kaldi::nnet3::DecodableNnetSimpleLoopedInfo> DecodableInfo_;

    std::unique_ptr<fst::Fst<fst::StdArc>> HCL_;
    std::unique_ptr<fst::Fst<fst::StdArc>> G_;
    std::unique_ptr<fst::Fst<fst::StdArc>> HCLG_;

    std::vector<int32_t> Disambig_;

    std::unique_ptr<fst::SymbolTable> PhoneSyms_;
};

} // NTruePrompter::NRecognition

