#include <trueprompter/model.hpp>

#include <include/PhonetisaurusScript.h>

#include <fst/register.h>
#include <fst/extensions/ngram/ngram-fst.h>

namespace {

class TPhonetisaurusPhonetizer : public NTruePrompter::TPhonetizer {
public:
    TPhonetisaurusPhonetizer(const std::string& fstPath)
        : PhonetisaurusDecoder_(fstPath)
    {}

    virtual const fst::SymbolTable& GetSymbolTable() const {
        return *PhonetisaurusDecoder_.osyms_;
    }

    virtual std::tuple<std::vector<int64_t>, std::vector<std::pair<size_t, float>>> Phoneticize(const tcb::span<const std::string>& words) const {
        std::vector<int64_t> phones;
        std::vector<std::pair<size_t, float>> phoneIndexToWordIndex;

        for (size_t i = 0; i < words.size(); ++i) {
            auto ret = PhonetisaurusDecoder_.Phoneticize(words[i]);
            for (size_t j = 0; j < ret[0].Uniques.size(); ++j) {
                phones.emplace_back(ret[0].Uniques[j]);
                phoneIndexToWordIndex.emplace_back(i, (float)j / (float)ret[0].Uniques.size());
            }
        }

        return { phones, phoneIndexToWordIndex };
    }

private:
    mutable PhonetisaurusScript PhonetisaurusDecoder_;
};

}

std::unique_ptr<NTruePrompter::TPhonetizer> NTruePrompter::TModel::MakePhonetizer(const std::string& fstPath) {
    return std::make_unique<TPhonetisaurusPhonetizer>(fstPath);
}

namespace fst {

static FstRegisterer<StdOLabelLookAheadFst> OLabelLookAheadFst_StdArc_registerer;
static FstRegisterer<NGramFst<StdArc>> NGramFst_StdArc_registerer;
static FstRegisterer<ConstFst<StdArc>> ConstFst_StdArc_registerer;
static FstRegisterer<VectorFst<StdArc>> VectorFst_StdArc_registerer;

}
