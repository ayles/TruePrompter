#include <online/online-audio-source.h>

#include <include/PhonetisaurusScript.h>

#include "new_model.h"
#include "new_recognizer.h"

#include <iostream>

static constexpr size_t SampleRate = 48000;

int main() {
    auto model = std::make_shared<TModel>("../small_model");
    std::cout << "loaded" << std::endl;

    TRecognizer recognizer(model, SampleRate);

    PhonetisaurusScript phonetisaurusDecoder("../small_model/ru.fst");
    auto a = phonetisaurusDecoder.Phoneticize("обзалупнянческий");

    for (auto x : a[0].OLabels) {
        std::cout << phonetisaurusDecoder.osyms_->Find(x) << " ";
    }
    std::cout << std::endl;

    kaldi::OnlinePaSource pa(0, SampleRate, SampleRate * 10, 0);
    kaldi::Vector<kaldi::BaseFloat> vec;
    vec.Resize(SampleRate / 2);

    while (true) {
        if (!pa.Read(&vec)) {
            continue;
        }
        std::stringstream ss;
        recognizer.AcceptWaveform(vec.Data(), vec.Dim());

        for (int32_t p : recognizer.GetPhones()) {
            std::cout << model->GetPhoneSym(p) << " ";
        }
        std::cout << std::endl;
    }

    return 0;
}
