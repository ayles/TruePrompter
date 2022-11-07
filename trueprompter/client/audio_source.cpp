#include "audio_source.hpp"

#include <portaudio.h>
#include <pa_ringbuffer.h>

#include <vector>


namespace {


class TMicAudioSource : public NTruePrompter::NAudioSource::IAudioSource {
public:
    static constexpr size_t RingBufferElementsCount = 65536;

    TMicAudioSource(int32_t sampleRate)
        : SampleRate_(sampleRate)
    {
        RingBufferData_.resize(RingBufferElementsCount);
        ring_buffer_size_t rbs = PaUtil_InitializeRingBuffer(&RingBuffer_, sizeof(decltype(RingBufferData_)::value_type), RingBufferElementsCount, RingBufferData_.data());
        if (rbs != 0) {
            throw std::runtime_error("PortAudio ring buffer initialization error");
        }
        if (Pa_Initialize() != paNoError) {
            throw std::runtime_error("PortAudio initialization error");
        }
        if (Pa_OpenDefaultStream(&Stream_, 1, 0, paFloat32, sampleRate, 0, Callback, this) != paNoError) {
            throw std::runtime_error("PortAudio failed to open the default stream");
        }
    }

    ~TMicAudioSource() override {
        if (Pa_IsStreamStopped(Stream_) == 0) {
            Pa_StopStream(Stream_);
        }
        if (Stream_) {
            Pa_CloseStream(Stream_);
            Pa_Terminate();
        }
    }

    int32_t GetSampleRate() const override {
        return SampleRate_;
    }

    size_t Read(float *data, size_t dataSize) override {
        if (Pa_IsStreamActive(Stream_) == 0) {
            if (Pa_StartStream(Stream_) != paNoError) {
                throw std::runtime_error("PortAudio failed to start stream");
            }
        }

        while (true) {
            ring_buffer_size_t dataAvailable = PaUtil_GetRingBufferReadAvailable(&RingBuffer_);
            if (dataAvailable >= (ssize_t)dataSize) {
                break;
            }
            Pa_Sleep(2);
        }

        if (PaUtil_ReadRingBuffer(&RingBuffer_, data, dataSize) != (ssize_t)dataSize) {
            throw std::runtime_error("PortAudio failed to read proper number of samples");
        }

        return dataSize;
    }

private:
    static int Callback(
        const void* in,
        [[maybe_unused]] void* out,
        long unsigned frames,
        [[maybe_unused]] const PaStreamCallbackTimeInfo* timeInfo,
        [[maybe_unused]] PaStreamCallbackFlags statusFlags,
        void* userData)
    {
        auto micAudioSource = reinterpret_cast<TMicAudioSource*>(userData);
        PaUtil_WriteRingBuffer(&micAudioSource->RingBuffer_, in, frames);
        return paContinue;
    }

private:
    int32_t SampleRate_;
    std::vector<float> RingBufferData_;
    PaUtilRingBuffer RingBuffer_;
    PaStream* Stream_;
};


}

namespace NTruePrompter::NAudioSource {


std::shared_ptr<IAudioSource> MakeMicrophoneAudioSource(int32_t sampleRate) {
    return std::make_shared<TMicAudioSource>(sampleRate);
}


} // NTruePrompter::NClient
