#include "audio_codec.hpp"
#include "av_audio_codec.hpp"

#include <tuple>


namespace {

class TPCMEncoder : public NTruePrompter::NCodec::IAudioEncoder {
public:
    TPCMEncoder(int32_t sampleRate)
        : SampleRate_(sampleRate)
    {}

    void Encode(const float* data, size_t size) override {
        Callback(reinterpret_cast<const uint8_t*>(data), size * sizeof(float));
    }

    void Finalize() override {}

    int32_t GetSampleRate() const {
        return SampleRate_;
    }

    NTruePrompter::NCodec::NProto::TAudioMeta GetMeta() const override {
        NTruePrompter::NCodec::NProto::TAudioMeta meta;
        meta.set_sample_rate(GetSampleRate());
        meta.set_format(NTruePrompter::NCodec::NProto::EFormat::RAW);
        meta.set_codec(NTruePrompter::NCodec::NProto::ECodec::PCM_F32LE);
        return meta;
    }

private:
    int32_t SampleRate_;
};

class TPCMDecoder : public NTruePrompter::NCodec::IAudioDecoder {
public:
    TPCMDecoder(int32_t sampleRate)
        : SampleRate_(sampleRate)
    {}

    void Decode(const uint8_t* data, size_t size) override {
        Callback(reinterpret_cast<const float*>(data), size / sizeof(float));
    }

    void Finalize() override {}

    int32_t GetSampleRate() const {
        return SampleRate_;
    }

    NTruePrompter::NCodec::NProto::TAudioMeta GetMeta() const override {
        NTruePrompter::NCodec::NProto::TAudioMeta meta;
        meta.set_sample_rate(GetSampleRate());
        meta.set_format(NTruePrompter::NCodec::NProto::EFormat::RAW);
        meta.set_codec(NTruePrompter::NCodec::NProto::ECodec::PCM_F32LE);
        return meta;
    }

private:
    int32_t SampleRate_;
};

} // namespace

namespace NTruePrompter::NCodec {

bool IsMetaEquivalent(const NProto::TAudioMeta& l, const NProto::TAudioMeta& r) {
    return std::tuple(l.codec(), l.format(), l.sample_rate()) == std::tuple(r.codec(), r.format(), r.sample_rate());
}

std::shared_ptr<IAudioEncoder> CreateEncoder(const NProto::TAudioMeta& meta) {
    if (meta.format() == NProto::EFormat::RAW) {
        switch (meta.codec()) {
            case NProto::ECodec::PCM_F32LE:
                return std::make_shared<TPCMEncoder>(meta.sample_rate());
        }
    }
    try {
        return std::make_shared<TAvAudioEncoder>(meta.format(), meta.codec(), meta.sample_rate());
    } catch (...) {
        return nullptr;
    }
}

std::shared_ptr<IAudioDecoder> CreateDecoder(const NProto::TAudioMeta& meta) {
    if (meta.format() == NProto::EFormat::RAW) {
        switch (meta.codec()) {
            case NProto::ECodec::PCM_F32LE:
                return std::make_shared<TPCMDecoder>(meta.sample_rate());
        }
    }
    try {
        return std::make_shared<TAvAudioDecoder>(meta.format(), meta.codec(), meta.sample_rate());
    } catch (...) {
        return nullptr;
    }
}

} // namespace NTruePrompter::NCodec
