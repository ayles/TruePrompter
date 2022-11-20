#pragma once

#include <cstddef>
#include <memory>


namespace NTruePrompter::NAudioSource {

class IAudioSource {
public:
    IAudioSource(const IAudioSource&) = delete;
    IAudioSource(IAudioSource&&) noexcept = delete;
    IAudioSource& operator=(const IAudioSource&) = delete;
    IAudioSource& operator=(IAudioSource&&) noexcept = delete;

    IAudioSource() = default;

    virtual int32_t GetSampleRate() const = 0;
    virtual size_t Read(float* data, size_t dataSize) = 0;
    virtual ~IAudioSource() = default;
};

std::shared_ptr<IAudioSource> MakeMicrophoneAudioSource(int32_t sampleRate);

} // NTruePrompter::NAudioSource
