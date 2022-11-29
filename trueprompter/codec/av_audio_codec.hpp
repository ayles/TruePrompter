#pragma once

#include "audio_codec.hpp"

#include <av.h>
#include <avutils.h>
#include <codeccontext.h>
#include <formatcontext.h>
#include <audioresampler.h>

#include <thread>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <unordered_map>


namespace NPrivate {

bool DoInitialize() {
    av::init();
    av::set_logging_level(AV_LOG_DEBUG);
    return true;
}

void Initialize() {
    static bool initialized = DoInitialize();
}

std::unordered_map<NTruePrompter::NCodec::NProto::EFormat, std::string> FormatMap {
    { NTruePrompter::NCodec::NProto::EFormat::OGG, "ogg" },
    { NTruePrompter::NCodec::NProto::EFormat::MPEG, "mpeg" },
};

std::unordered_map<NTruePrompter::NCodec::NProto::ECodec, AVCodecID> CodecMap {
    { NTruePrompter::NCodec::NProto::ECodec::VORBIS, AV_CODEC_ID_VORBIS },
    { NTruePrompter::NCodec::NProto::ECodec::OPUS, AV_CODEC_ID_OPUS },
    { NTruePrompter::NCodec::NProto::ECodec::MP3, AV_CODEC_ID_MP3 },
};

} // namespace

namespace NTruePrompter::NCodec {

class TAvAudioEncoder : public IAudioEncoder, av::CustomIO {
private:
    enum class EState {
        Uninitialized,
        Initialized,
        Finalized,
    };
public:
    explicit TAvAudioEncoder(NProto::EFormat format, NProto::ECodec codec, int32_t inputSampleRate, int inputChannels = 1, int32_t outputSampleRate = 0, int outputChannels = 1) {
        NPrivate::Initialize();

        Meta_.set_format(format);
        Meta_.set_codec(codec);
        Meta_.set_sample_rate(inputSampleRate);

        OutputFormat_ = av::guessOutputFormat(NPrivate::FormatMap.at(format));
        FormatContext_.setFormat(OutputFormat_);
        Codec_ = av::findEncodingCodec(NPrivate::CodecMap.at(codec));
        Stream_ = FormatContext_.addStream(Codec_);

        constexpr bool newApi = true;
        if constexpr (newApi) {
            Context_ = av::AudioEncoderContext(Codec_);
        } else {
            Context_ = av::AudioEncoderContext(Stream_);
        }
        Context_.setSampleFormat(AV_SAMPLE_FMT_FLTP);
        Context_.setSampleRate(SelectSampleRate(Codec_, outputSampleRate));
        Context_.setChannels(outputChannels);
        Context_.setStrict(FF_COMPLIANCE_EXPERIMENTAL);
        Context_.setTimeBase(av::Rational(1, Context_.sampleRate()));
        Context_.open();
        if constexpr (newApi) {
            avcodec_parameters_from_context(Stream_.raw()->codecpar, Context_.raw());
        }

        FormatContext_.openOutput(this);

        Resampler_.init(
            Context_.channelLayout(),
            Context_.sampleRate(),
            Context_.sampleFormat(),
            av_get_default_channel_layout(inputChannels),
            inputSampleRate,
            AV_SAMPLE_FMT_FLT
        );
    }

    void Encode(const float *data, size_t size) override {
        if (State_ == EState::Finalized) {
            throw std::runtime_error("Can't encode on finalized encoder");
        }
        if (State_ == EState::Uninitialized) {
            // Idk is it is necessary
            // FormatContext_.dump()
            FormatContext_.writeHeader();
            FormatContext_.flush();
            State_ = EState::Initialized;
        }

        if (data) {
            av::AudioSamples inputSamples(
                reinterpret_cast<const uint8_t*>(data),
                sizeof(*data) * size,
                Resampler_.srcSampleFormat(),
                size,
                Resampler_.srcChannelLayout(),
                Resampler_.srcSampleRate()
            );

            Resampler_.push(inputSamples);
        }

        while (true) {
            // TODO can it be moved outside loop?
            av::AudioSamples outputSamples(
                Context_.sampleFormat(),
                Context_.frameSize(),
                Context_.channelLayout(),
                Context_.sampleRate()
            );

            if (!Resampler_.pop(outputSamples, !data)) {
                break;
            }

            outputSamples.setStreamIndex(0);
            outputSamples.setTimeBase(Context_.timeBase());

            av::Packet outputPacket = Context_.encode(outputSamples);

            if (!outputPacket) {
                continue;
            }

            outputPacket.setStreamIndex(0);
            FormatContext_.writePacket(outputPacket);
        }
    }

    void Finalize() override {
        if (State_ == EState::Finalized) {
            return;
        }
        if (State_ == EState::Initialized) {
            Encode(nullptr, 0);

            while (true) {
                av::AudioSamples inputSamples(nullptr);
                av::Packet outputPacket = Context_.encode(inputSamples);
                if (!outputPacket) {
                    break;
                }
                outputPacket.setStreamIndex(0);
                FormatContext_.writePacket(outputPacket);
            }

            FormatContext_.flush();
            FormatContext_.writeTrailer();
        }
        State_ = EState::Finalized;
    }

    int32_t GetSampleRate() const override {
        return Resampler_.srcSampleRate();
    }

    NProto::TAudioMeta GetMeta() const override {
        return Meta_;
    }

private:
    int write(const uint8_t *data, size_t size) override {
        Callback(data, size);
        return size;
    }

    static int SelectSampleRate(const av::Codec& codec, int proposedSampleRate) {
        proposedSampleRate = proposedSampleRate > 0 ? proposedSampleRate : 44100;
        int resultSampleRate = 0;
        for (int sampleRate : codec.supportedSamplerates()) {
            if (!resultSampleRate || std::abs(proposedSampleRate - sampleRate) < std::abs(proposedSampleRate - resultSampleRate)) {
                resultSampleRate = sampleRate;
            }
        }
        return resultSampleRate;
    }

private:
    NProto::TAudioMeta Meta_;

    av::OutputFormat OutputFormat_;
    av::FormatContext FormatContext_;
    av::Codec Codec_;
    av::Stream Stream_;
    av::AudioEncoderContext Context_;
    av::AudioResampler Resampler_;
    EState State_ = EState::Uninitialized;
};

// TODO rewrite to better interfaces or fibers
class TAvAudioDecoderQueue {
public:
    // ======== Producer methods ========

    size_t Produce(const uint8_t* data, size_t size) {
        {
            std::lock_guard guard(Mutex_);
            if (BufferSize_) {
                throw std::runtime_error("Size is not 0");
            }
            if (Finalized_) {
                throw std::runtime_error("Queue is finalized");
            }
            Buffer_ = data;
            BufferSize_ = size;
        }
        ConditionVariable_.notify_one();
        std::unique_lock lock(Mutex_);
        ConditionVariable_.wait(lock, [this] { return BufferSize_ == 0; });
        if (Exception_) {
            std::rethrow_exception(Exception_);
        }
        return Buffer_ - data;
    }

    void Finalize() {
        {
            std::lock_guard guard(Mutex_);
            Finalized_ = true;
        }
        ConditionVariable_.notify_one();
    }

    bool IsFinalized() const {
        return Finalized_;
    }

    // ======== Consumer methods ========
    size_t Consume(uint8_t* data, size_t size) {
        std::unique_lock lock(Mutex_);
        ConditionVariable_.wait(lock, [this] { return BufferSize_ > 0 || Finalized_; });
        if (Finalized_) {
            return 0;
        }
        size_t toOutput = std::min<size_t>(size, BufferSize_);
        std::memcpy(data, Buffer_, toOutput);
        BufferSize_ -= toOutput;
        Buffer_ += toOutput;
        lock.unlock();
        if (!BufferSize_) {
            ConditionVariable_.notify_one();
        }
        return toOutput;
    }

    void Yield() {
        {
            std::lock_guard guard(Mutex_);
            BufferSize_ = 0;
        }
        ConditionVariable_.notify_one();
    }

    void SetException(std::exception_ptr&& exception) {
        {
            std::lock_guard guard(Mutex_);
            BufferSize_ = 0;
            Exception_ = std::move(exception);
            Finalized_ = true;
        }
        ConditionVariable_.notify_one();
    }
private:
    std::mutex Mutex_;
    std::condition_variable ConditionVariable_;
    const uint8_t* Buffer_ = nullptr;
    size_t BufferSize_ = 0;
    bool Finalized_ = false;
    std::exception_ptr Exception_;
};

class TAvAudioDecoder : public IAudioDecoder, av::CustomIO {
public:
    explicit TAvAudioDecoder(NProto::EFormat format, NProto::ECodec codec, int32_t inputSampleRate, int inputChannels = 1, int32_t outputSampleRate = 0, int outputChannels = 1)
        : Worker_(&TAvAudioDecoder::DoWork, this, format, codec, inputSampleRate, inputChannels, outputSampleRate, outputChannels)
    {
        Meta_.set_format(format);
        Meta_.set_codec(codec);
        Meta_.set_sample_rate(inputSampleRate);
    }

    void Decode(const uint8_t* data, size_t size) override {
        while (size) {
            size_t consumed = Queue_.Produce(data, size);
            size -= consumed;
            data += consumed;

            while (Resampler_.isValid()) {
                av::AudioSamples outputSamples(
                    Resampler_.dstSampleFormat(),
                    512,
                    Resampler_.dstChannelLayout(),
                    Resampler_.dstSampleRate()
                );

                if (!Resampler_.pop(outputSamples, Queue_.IsFinalized())) {
                    break;
                }

                Callback(reinterpret_cast<const float*>(outputSamples.data()), outputSamples.samplesCount());
            }
        }
    }

    void Finalize() override {
        Queue_.Finalize();
    }

    int32_t GetSampleRate() const override {
        return Resampler_.dstSampleRate();
    }

    NProto::TAudioMeta GetMeta() const override {
        return Meta_;
    }

    ~TAvAudioDecoder() override {
        Finalize();
        if (Worker_.joinable()) {
            Worker_.join();
        }
    }

private:
    int read(uint8_t *data, size_t size) override {
        return (int)Queue_.Consume(data, size);
    }

    void DoWork(NProto::EFormat format, NProto::ECodec codec, int32_t inputSampleRate, int inputChannels, int32_t outputSampleRate, int outputChannels) {
        try {
            NPrivate::Initialize();

            FormatContext_.openInput(this, av::InputFormat(NPrivate::FormatMap.at(format)));
            FormatContext_.findStreamInfo();
            for (size_t i = 0; i < FormatContext_.streamsCount(); ++i) {
                av::Stream stream = FormatContext_.stream(i);
                if (stream.isAudio()) {
                    Stream_ = std::move(stream);
                    break;
                }
            }

            if (Stream_.isNull()) {
                throw std::runtime_error("Stream is not found");
            }
            if (!Stream_.isValid()) {
                throw std::runtime_error("Stream is not valid");
            }
            if (Stream_.raw()->codecpar->codec_id != NPrivate::CodecMap.at(codec)) {
                throw std::runtime_error("Wrong codec");
            }
            if (Stream_.raw()->codecpar->sample_rate != inputSampleRate) {
                throw std::runtime_error("Wrong sample rate");
            }
            if (Stream_.raw()->codecpar->channels != inputChannels) {
                throw std::runtime_error("Wrong channels count");
            }

            Context_ = av::AudioDecoderContext(Stream_);
            Context_.open();

            Resampler_.init(
                av_get_default_channel_layout(outputChannels),
                outputSampleRate > 0 ? outputSampleRate : Context_.sampleRate(),
                AV_SAMPLE_FMT_FLT,
                Context_.channelLayout(),
                Context_.sampleRate(),
                Context_.sampleFormat()
            );

            while (true) {
                av::Packet packet = FormatContext_.readPacket();
                if (packet.streamIndex() != Stream_.index()) {
                    continue;
                }

                av::AudioSamples inputSamples = Context_.decode(packet);

                if (inputSamples) {
                    Resampler_.push(inputSamples);
                }

                Queue_.Yield();

                if (!packet && !inputSamples) {
                    break;
                }
            }
        } catch (...) {
            Queue_.SetException(std::current_exception());
        }
    }

private:
    NProto::TAudioMeta Meta_;

    av::FormatContext FormatContext_;
    av::Stream Stream_;
    av::AudioDecoderContext Context_;
    av::AudioResampler Resampler_;
    std::thread Worker_;
    TAvAudioDecoderQueue Queue_;
};

} // NTruePrompter::NCodec
