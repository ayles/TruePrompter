#include "audio_codec.hpp"


namespace {


class TPCMEncoder : public NTruePrompter::NAudioCodec::IAudioEncoder {
public:
    TPCMEncoder(const NTruePrompter::NAudioCodec::NProto::TAudioMeta& meta)
        : Meta_(meta)
    {}

    void Encode(const float* data, size_t size) override {
        Callback(reinterpret_cast<const uint8_t*>(data), size * sizeof(float));
    }

    void Finalize() override {}

    const NTruePrompter::NAudioCodec::NProto::TAudioMeta& GetMeta() const override {
        return Meta_;
    }

private:
    NTruePrompter::NAudioCodec::NProto::TAudioMeta Meta_;
};


class TPCMDecoder : public NTruePrompter::NAudioCodec::IAudioDecoder {
public:
    TPCMDecoder(const NTruePrompter::NAudioCodec::NProto::TAudioMeta& meta)
        : Meta_(meta)
    {}

    void Decode(const uint8_t* data, size_t size) override {
        Callback(reinterpret_cast<const float*>(data), size / sizeof(float));
    }

    void Finalize() override {}

    const NTruePrompter::NAudioCodec::NProto::TAudioMeta& GetMeta() const override {
        return Meta_;
    }

private:
    NTruePrompter::NAudioCodec::NProto::TAudioMeta Meta_;
};


}


namespace NTruePrompter::NAudioCodec {


std::shared_ptr<IAudioEncoder> CreateEncoder(const NProto::TAudioMeta& meta) {
    if (meta.codec() == NProto::ECodec::RAW_PCM_F32LE) {
        return std::make_shared<TPCMEncoder>(meta);
    }
    return nullptr;
}

std::shared_ptr<IAudioDecoder> CreateDecoder(const NProto::TAudioMeta& meta) {
    if (meta.codec() == NProto::ECodec::RAW_PCM_F32LE) {
        return std::make_shared<TPCMDecoder>(meta);
    }
    return nullptr;
}


}
