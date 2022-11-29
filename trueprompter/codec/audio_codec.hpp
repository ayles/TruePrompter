#pragma once

#include <trueprompter/codec/proto/audio_codec.pb.h>

#include <stdexcept>
#include <functional>
#include <cstddef>


namespace NTruePrompter::NCodec {

class IAudioEncoder {
public:
    IAudioEncoder(const IAudioEncoder&) = delete;
    IAudioEncoder(IAudioEncoder&&) noexcept = delete;
    IAudioEncoder& operator=(const IAudioEncoder&) = delete;
    IAudioEncoder& operator=(IAudioEncoder&&) noexcept = delete;

    IAudioEncoder() = default;
    virtual ~IAudioEncoder() {}

    virtual void Encode(const float* data, size_t size) = 0;
    virtual void Finalize() = 0;
    virtual int32_t GetSampleRate() const = 0;
    virtual NProto::TAudioMeta GetMeta() const {
        throw std::runtime_error("Not implemented");
    }

    void SetCallback(const std::function<void(const uint8_t*, size_t)>& callback) {
        Callback_ = callback;
    }

protected:
    void Callback(const uint8_t* data, size_t size) {
        if (!Callback_) {
            throw std::runtime_error("Callback should be provided");
        }
        Callback_(data, size);
    }

private:
    std::function<void(const uint8_t* data, size_t size)> Callback_;
};

class IAudioDecoder {
public:
    IAudioDecoder(const IAudioDecoder&) = delete;
    IAudioDecoder(IAudioDecoder&&) noexcept = delete;
    IAudioDecoder& operator=(const IAudioDecoder&) = delete;
    IAudioDecoder& operator=(IAudioDecoder&&) noexcept = delete;

    IAudioDecoder() = default;
    virtual ~IAudioDecoder() {}

    virtual void Decode(const uint8_t* data, size_t size) = 0;
    virtual void Finalize() = 0;
    virtual int32_t GetSampleRate() const = 0;
    virtual NProto::TAudioMeta GetMeta() const {
        throw std::runtime_error("Not implemented");
    }

    void SetCallback(const std::function<void(const float*, size_t)>& callback) {
        Callback_ = callback;
    }

protected:
    void Callback(const float* data, size_t size) {
        if (!Callback_) {
            throw std::runtime_error("Callback should be provided");
        }
        Callback_(data, size);
    }

private:
    std::function<void(const float* data, size_t size)> Callback_;
};

bool IsMetaEquivalent(const NProto::TAudioMeta& l, const NProto::TAudioMeta& r);

std::shared_ptr<IAudioEncoder> CreateEncoder(const NProto::TAudioMeta& meta);
std::shared_ptr<IAudioDecoder> CreateDecoder(const NProto::TAudioMeta& meta);

} // NTruePrompter::NCodec
