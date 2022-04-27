#pragma once

#include <decoder/lattice-incremental-decoder.h>
#include <hmm/transition-model.h>
#include <nnet3/am-nnet-simple.h>
#include <nnet3/decodable-simple-looped.h>
#include <nnet3/nnet-utils.h>
#include <online2/online-nnet2-feature-pipeline.h>
#include <online2/online-endpoint.h>
#include <fst/fst.h>

#include <string>
#include <memory>
#include <vector>

class TModel {
public:
    friend class TRecognizer;

    TModel(const std::string& path)
        : TransitionModel_(std::make_unique<kaldi::TransitionModel>())
        , NNet_(std::make_unique<kaldi::nnet3::AmNnetSimple>())
    {
        {
            bool binary;
            kaldi::Input ki(path + "/am/final.mdl", &binary);
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
            po.ReadConfigFile(path + "/conf/model.conf");
        }

        DecodingConfig_.determinize_max_delay = 20;
        DecodingConfig_.determinize_min_chunk_size = 10;
        kaldi::ReadConfigFromFile(path + "/conf/mfcc.conf", &FeatureInfo_.mfcc_opts);
        FeatureInfo_.feature_type = "mfcc";
        FeatureInfo_.mfcc_opts.frame_opts.allow_downsample = true;
        FeatureInfo_.silence_weighting_config.silence_weight = 1e-3;
        FeatureInfo_.silence_weighting_config.silence_phones_str = EndpointConfig_.silence_phones;

        {
            kaldi::OnlineIvectorExtractionConfig opts;
            opts.splice_config_rxfilename = path + "/ivector/splice.conf";
            opts.cmvn_config_rxfilename = path + "/ivector/online_cmvn.conf";
            opts.lda_mat_rxfilename = path + "/ivector/final.mat";
            opts.global_cmvn_stats_rxfilename = path + "/ivector/global_cmvn.stats";
            opts.diag_ubm_rxfilename = path + "/ivector/final.dubm";
            opts.ivector_extractor_rxfilename = path + "/ivector/final.ie";
            opts.max_count = 100;

            FeatureInfo_.use_ivectors = true;
            FeatureInfo_.ivector_extractor_info.Init(opts);
        }

        DecodableInfo_ = std::make_unique<kaldi::nnet3::DecodableNnetSimpleLoopedInfo>(DecodableOpts_, NNet_.get());

        HCL_ = std::unique_ptr<fst::Fst<fst::StdArc>>(fst::StdFst::Read(path + "/graph/HCLr.fst"));
        G_ = std::unique_ptr<fst::Fst<fst::StdArc>>(fst::StdFst::Read(path + "/graph/Gr.fst"));
        kaldi::ReadIntegerVectorSimple(path + "/graph/disambig_tid.int", &Disambig_);

        PhoneSyms_ = std::unique_ptr<fst::SymbolTable>(fst::SymbolTable::ReadText(path + "/graph/phones.txt"));
    }

    std::string GetPhoneSym(int32_t index) {
        return PhoneSyms_->Find(index);
    }

    std::string GetWordSym(int32_t index) {
        return G_->OutputSymbols()->Find(index);
    }

private:
    std::unique_ptr<kaldi::TransitionModel> TransitionModel_;
    std::unique_ptr<kaldi::nnet3::AmNnetSimple> NNet_;

    kaldi::LatticeIncrementalDecoderConfig DecodingConfig_;
    kaldi::OnlineEndpointConfig EndpointConfig_;
    kaldi::nnet3::NnetSimpleLoopedComputationOptions DecodableOpts_;
    kaldi::OnlineNnet2FeaturePipelineInfo FeatureInfo_;


    std::unique_ptr<kaldi::nnet3::DecodableNnetSimpleLoopedInfo> DecodableInfo_;

    std::unique_ptr<fst::Fst<fst::StdArc>> HCL_;
    std::unique_ptr<fst::Fst<fst::StdArc>> G_;
    std::vector<int32_t> Disambig_;

    std::unique_ptr<fst::SymbolTable> PhoneSyms_;
};
