#include "recognizer.hpp"

#include <include/PhonetisaurusScript.h>

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

#include <unordered_map>


namespace {


class TPhonetizer {
public:
    TPhonetizer(const std::filesystem::path& path)
        : PhonetisaurusDecoder_(path)
    {}

    const fst::SymbolTable& GetSymbolTable() const {
        return *PhonetisaurusDecoder_.osyms_;
    }

    std::tuple<std::vector<int64_t>, std::vector<size_t>> Phoneticize(const std::string& text) const {
        if (!utf8::is_valid(text.begin(), text.end())) {
            throw std::runtime_error("Text is not valid utf-8 string");
        }

        std::vector<int64_t> phones;
        std::vector<size_t> mapping;

        auto isWordSymbol = [](uint32_t c) {
            // TODO proper spaces tracking
            return c > std::numeric_limits<unsigned char>::max() || !std::isspace((unsigned char)c);
        };

        auto flush = [this, &text, &phones, &mapping](const std::string::const_iterator& begin, const std::string::const_iterator& end) {
            auto ret = PhonetisaurusDecoder_.Phoneticize(std::string(begin, end));
            auto it = begin;
            float textFraction = 0.0f;
            for (size_t j = 0; j < ret[0].Uniques.size(); ++j) {
                phones.emplace_back(ret[0].Uniques[j]);
                const float phoneFraction = float(j) / ret[0].Uniques.size();
                while (textFraction < phoneFraction && it != end) {
                    utf8::next(it, end);
                    textFraction = float(std::distance(begin, it)) / std::distance(begin, end);
                }
                mapping.emplace_back(it - text.begin());
            }
        };

        auto wordStartIt = text.begin();
        auto it = text.begin();
        while (it != text.end()) {
            uint32_t c = utf8::peek_next(it, text.end());
            if (isWordSymbol(c)) {
                utf8::next(it, text.end());
            } else {
                flush(wordStartIt, it);
                do {
                    wordStartIt = it;
                } while (it != text.end() && !isWordSymbol(utf8::next(it, text.end())));
            }
        }
        flush(wordStartIt, it);

        return { phones, mapping };
    }

private:
    mutable PhonetisaurusScript PhonetisaurusDecoder_;
};


class TModel {
public:
    TModel(const std::filesystem::path& path)
        : Phonetizer_(path / "ru.fst")
        , TransitionModel_(std::make_unique<kaldi::TransitionModel>())
        , NNet_(std::make_unique<kaldi::nnet3::AmNnetSimple>())
    {
        {
            bool binary;
            kaldi::Input ki(path / "am/final.mdl", &binary);
            TransitionModel_->Read(ki.Stream(), binary);
            NNet_->Read(ki.Stream(), binary);
            SetBatchnormTestMode(true, &NNet_->GetNnet());
            SetDropoutTestMode(true, &NNet_->GetNnet());
            kaldi::nnet3::CollapseModel({}, &NNet_->GetNnet());
        }

        {
            kaldi::ParseOptions po("");
            DecodingConfig_.Register(&po);
            EndpointConfig_.Register(&po);
            DecodableOpts_.Register(&po);
            po.ReadConfigFile(path / "conf/model.conf");
        }

        DecodingConfig_.determinize_max_delay = 20;
        DecodingConfig_.determinize_min_chunk_size = 10;
        kaldi::ReadConfigFromFile(path / "conf/mfcc.conf", &FeatureInfo_.mfcc_opts);
        FeatureInfo_.feature_type = "mfcc";
        FeatureInfo_.mfcc_opts.frame_opts.allow_downsample = true;
        FeatureInfo_.silence_weighting_config.silence_weight = 1e-3;
        FeatureInfo_.silence_weighting_config.silence_phones_str = EndpointConfig_.silence_phones;

        {
            kaldi::OnlineIvectorExtractionConfig opts;
            opts.splice_config_rxfilename = path / "ivector/splice.conf";
            opts.cmvn_config_rxfilename = path / "ivector/online_cmvn.conf";
            opts.lda_mat_rxfilename = path / "ivector/final.mat";
            opts.global_cmvn_stats_rxfilename = path / "ivector/global_cmvn.stats";
            opts.diag_ubm_rxfilename = path / "ivector/final.dubm";
            opts.ivector_extractor_rxfilename = path / "ivector/final.ie";
            opts.max_count = 100;

            FeatureInfo_.use_ivectors = true;
            FeatureInfo_.ivector_extractor_info.Init(opts);
        }

        DecodableInfo_ = std::make_unique<kaldi::nnet3::DecodableNnetSimpleLoopedInfo>(DecodableOpts_, NNet_.get());

        HCL_ = std::unique_ptr<fst::Fst<fst::StdArc>>(fst::StdFst::Read(path / "graph/HCLr.fst"));
        G_ = std::unique_ptr<fst::Fst<fst::StdArc>>(fst::StdFst::Read(path / "graph/Gr.fst"));
        kaldi::ReadIntegerVectorSimple(path / "graph/disambig_tid.int", &Disambig_);
        HCLG_.reset(fst::LookaheadComposeFst(*HCL_, *G_, Disambig_));

        PhoneSyms_ = std::unique_ptr<fst::SymbolTable>(fst::SymbolTable::ReadText(path / "graph/phones.txt"));

        KaldiToPhonetisaurusPhoneMapping_ = MakeKaldiToPhonetisaurusPhoneMapping(*PhoneSyms_, Phonetizer_.GetSymbolTable());
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

    const TPhonetizer& GetPhonetizer() const {
        return Phonetizer_;
    }

    std::optional<int64_t> RemapPhone(int64_t phone) const {
        auto it = KaldiToPhonetisaurusPhoneMapping_.find(phone);
        if (it != KaldiToPhonetisaurusPhoneMapping_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

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
    TPhonetizer Phonetizer_;
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


class TRecognizer : public NTruePrompter::NRecognition::IRecognizer {
public:
    TRecognizer(std::shared_ptr<TModel> model)
        : Model_(std::move(model))
    {
        std::tie(FeaturePipeline_, Decoder_) = Model_->CreateFeaturePipelineAndDecoder();
    }

    bool AcceptWaveform(const float* data, size_t dataSize, float sampleRate) override {
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

        return false;
    }

    void Reset() override {
        Decoder_->FinalizeDecoding();
        FrameOffset_ += Decoder_->NumFramesDecoded();
        Decoder_->InitDecoding(FrameOffset_);
        SilenceWeighting_.reset();
    }

    std::vector<int64_t> GetPhones() const override {
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

    std::tuple<std::vector<int64_t>, std::vector<size_t>> MapToPhones(const std::string& text) const override {
        return Model_->GetPhonetizer().Phoneticize(text);
    }

private:
    std::shared_ptr<TModel> Model_;

    std::unique_ptr<kaldi::OnlineNnet2FeaturePipeline> FeaturePipeline_;
    std::unique_ptr<kaldi::OnlineSilenceWeighting> SilenceWeighting_;
    std::unique_ptr<kaldi::SingleUtteranceNnet3IncrementalDecoder> Decoder_;
    int32_t FrameOffset_ = 0;
};


class TRecognizerFactory : public NTruePrompter::NRecognition::IRecognizerFactory {
public:
    TRecognizerFactory(const std::filesystem::path& path)
        : Model_(std::make_shared<TModel>(path))
    {}

    std::shared_ptr<NTruePrompter::NRecognition::IRecognizer> Create() const override {
        return std::make_shared<TRecognizer>(Model_);
    }

private:
    std::shared_ptr<TModel> Model_;
};

}


std::shared_ptr<NTruePrompter::NRecognition::IRecognizerFactory> NTruePrompter::NRecognition::CreateRecognizerFactory(const std::filesystem::path& path) {
    return std::make_shared<TRecognizerFactory>(path);
}

namespace fst {

static FstRegisterer<StdOLabelLookAheadFst> OLabelLookAheadFst_StdArc_registerer;
static FstRegisterer<NGramFst<StdArc>> NGramFst_StdArc_registerer;

}
