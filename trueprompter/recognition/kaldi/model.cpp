#include "kaldi.hpp"
#include "model.hpp"

#include <include/PhonetisaurusScript.h>

namespace NTruePrompter::NRecognition {

TKaldiModel::TKaldiModel(const std::filesystem::path& path)
    : PhonetisaurusDecoder_(std::make_unique<PhonetisaurusScript>(path / "g2p.fst"))
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

    PhoneSyms_ = std::unique_ptr<fst::SymbolTable>(fst::SymbolTable::ReadText(path / "phones.txt"));

    KaldiToPhonetisaurusPhoneMapping_ = MakeKaldiToPhonetisaurusPhoneMapping(*PhoneSyms_, *PhonetisaurusDecoder_->osyms_);
}

std::vector<int64_t> TKaldiModel::Phoneticize(const std::string& word) const {
    std::vector<int64_t> res;
    auto ret = PhonetisaurusDecoder_->Phoneticize(word);
    for (size_t j = 0; j < ret[0].Uniques.size(); ++j) {
        res.emplace_back(ret[0].Uniques[j]);
    }
    return res;
}

class TKazakhKaldiModel : public NTruePrompter::NRecognition::TKaldiModel {
public:
    using NTruePrompter::NRecognition::TKaldiModel::TKaldiModel;

    std::vector<int64_t> Phoneticize(const std::string& word) const override {
        static const std::unordered_map<std::string, std::string> mapping {
            { "Ә", "А", },
            { "ә", "а", },
            { "Ғ", "Г", },
            { "ғ", "г", },
            { "Қ", "К", },
            { "қ", "к", },
            { "Ң", "Н", },
            { "ң", "н", },
            { "Ө", "О", },
            { "ө", "о", },
            { "Ұ", "У", },
            { "ұ", "у", },
            { "Ү", "У", },
            { "ү", "у", },
            { "Һ", "Х", },
            { "һ", "х", },
            { "І", "И", },
            { "і", "и", },
        };

        std::string newWord;

        auto it = word.begin();
        while (it != word.end()) {
            auto prev = it;
            utf8::next(it, word.end());
            std::string ch(prev, it);
            auto mit = mapping.find(ch);
            if (mit != mapping.end()) {
                newWord += mit->second;
            } else {
                newWord += ch;
            }
        }

        return NTruePrompter::NRecognition::TKaldiModel::Phoneticize(newWord);
    } 
};

std::shared_ptr<TKaldiModel> LoadKaldiModel(const std::filesystem::path& path) {
    if (path.filename() == "ru+kz") {
        return std::make_shared<TKazakhKaldiModel>(path.parent_path() / "ru");
    }
    return std::make_shared<TKaldiModel>(path);
}

}

namespace fst {

static FstRegisterer<StdOLabelLookAheadFst> OLabelLookAheadFst_StdArc_registerer;
static FstRegisterer<NGramFst<StdArc>> NGramFst_StdArc_registerer;

}

