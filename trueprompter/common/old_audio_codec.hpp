#pragma once

// Too kek
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/samplefmt.h>
#include <libavformat/avformat.h>
}

#include <trueprompter/common/proto/audio_codec.pb.h>

#include <stdexcept>
#include <functional>
#include <vector>
#include <ostream>
#include <iostream>
#include <deque>

namespace NTruePrompter::NAudioCodec {


/*class IAudioEncoder {
private:
    enum class EState {
        Uninitialized,
        Initialized,
        Finalized,
    };

public:
    IAudioEncoder(const IAudioEncoder&) = delete;
    IAudioEncoder(IAudioEncoder&&) noexcept = delete;
    IAudioEncoder& operator=(const IAudioEncoder&) = delete;
    IAudioEncoder& operator=(IAudioEncoder&&) noexcept = delete;

    IAudioEncoder(const std::string& format, const std::string& codec) {
        // Codec stuff

        Codec_ = avcodec_find_encoder_by_name(codec.c_str());
        if (!Codec_) {
            throw std::runtime_error("Error finding codec");
        }

        CodecContext_ = avcodec_alloc_context3(Codec_);

        // Context_->bit_rate = 64000;
        CodecContext_->sample_fmt = AV_SAMPLE_FMT_FLTP;
        CodecContext_->thread_count = 1;
        CodecContext_->sample_rate = SelectSampleRate(Codec_);
        CodecContext_->ch_layout = (AVChannelLayout)AV_CHANNEL_LAYOUT_MONO;
        CodecContext_->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

        if (codec == "vorbis") {
            CodecContext_->channel_layout = AV_CH_LAYOUT_STEREO;
        }

        if (!CheckSampleFormat(Codec_, CodecContext_->sample_fmt)) {
            throw std::runtime_error("Unsupported sample format");
        }

        if (avcodec_open2(CodecContext_, Codec_, nullptr) < 0) {
            throw std::runtime_error("Can't open codec");
        }

        // Stream stuff

        constexpr size_t bufferSize = 32768;
        uint8_t* buffer = static_cast<uint8_t*>(av_malloc(bufferSize));
        IOContext_ = avio_alloc_context(buffer, bufferSize, 1, this, nullptr, &WriteCallback, nullptr);
        if (!IOContext_) {
            av_free(buffer);
            throw std::runtime_error("Error allocating av io context");
        }

        FormatContext_ = avformat_alloc_context();
        if (!FormatContext_) {
            throw std::runtime_error("Error allocating format context");
        }
        FormatContext_->pb = IOContext_;
        FormatContext_->pb->seekable = 0;
        FormatContext_->flags |= AVFMT_FLAG_CUSTOM_IO;
        FormatContext_->probesize = 0;
        FormatContext_->max_analyze_duration = 0;
        FormatContext_->oformat = av_guess_format(format.c_str(), nullptr, nullptr);
        if (!FormatContext_->oformat) {
            throw std::runtime_error("Muxer not found");
        }

        Stream_ = avformat_new_stream(FormatContext_, nullptr);
        if (!Stream_) {
            throw std::runtime_error("Error allocating stream");
        }

        avcodec_parameters_from_context(Stream_->codecpar, CodecContext_);
        Stream_->time_base = AVRational {
            .num = 1,
            .den = CodecContext_->sample_rate,
        };

        // Buffers stuff

        Frame_ = av_frame_alloc();
        Frame_->nb_samples = CodecContext_->frame_size ? CodecContext_->frame_size : 1024;
        Frame_->format = CodecContext_->sample_fmt;

        if (av_channel_layout_copy(&Frame_->ch_layout, &CodecContext_->ch_layout) < 0) {
            throw std::runtime_error("Error copying channel layout");
        }
        if (av_frame_get_buffer(Frame_, 0) < 0) {
            throw std::runtime_error("Error allocating frame buffer");
        }

        Packet_ = av_packet_alloc();
    }

    NProto::TAudioMeta GetMeta() const {
        NProto::TAudioMeta meta;

        switch (Codec_->id) {
            case AV_CODEC_ID_VORBIS:
                meta.set_codec(NProto::ECodec::VORBIS);
                break;
            case AV_CODEC_ID_OPUS:
                meta.set_codec(NProto::ECodec::OPUS);
                break;
            case AV_CODEC_ID_MP3:
                meta.set_codec(NProto::ECodec::MP3);
                break;
            default:
                throw std::runtime_error("Unknown codec");
        }

        meta.set_mime_type(FormatContext_->oformat->mime_type);

        if (FormatContext_->oformat->name == std::string_view("ogg")) {
            meta.set_format(NProto::EFormat::OGG);
        } else if (FormatContext_->oformat->name == std::string_view("webm")) {
            meta.set_format(NProto::EFormat::WEBM);
        } else if (FormatContext_->oformat->name == std::string_view("mpeg")) {
            meta.set_format(NProto::EFormat::MPEG);
        } else {
            throw std::runtime_error("Unknown format");
        }

        meta.set_sample_rate(CodecContext_->sample_rate);

        return meta;
    }

    void Encode(const float* data, size_t samples) {
        if (CurrentState_ == EState::Uninitialized) {
            if (avformat_write_header(FormatContext_, nullptr) < 0) {
                throw std::runtime_error("Can't write header");
            }
            CurrentState_ = EState::Initialized;
        }

        if (CurrentState_ == EState::Finalized) {
            throw std::runtime_error("Can't encode on finalized encoder");
        }

        while (samples) {
            if (!CurrentSamplesCount_) {
                if (av_frame_make_writable(Frame_) < 0) {
                    throw std::runtime_error("Error ensuring frame is writable");
                }
            }

            size_t samplesToWrite = std::min(samples, Frame_->nb_samples - CurrentSamplesCount_);
            std::memcpy(Frame_->data[0] + CurrentSamplesCount_ * sizeof(float), data, samplesToWrite * sizeof(float));
            CurrentSamplesCount_ += samplesToWrite;
            samples -= samplesToWrite;
            data += samplesToWrite;

            if (CurrentSamplesCount_ >= Frame_->nb_samples) {
                Encode(FormatContext_, CodecContext_, Frame_, Packet_);
                CurrentSamplesCount_ = 0;
            }
        }
    }

    void Finalize() {
        CurrentState_ = EState::Finalized;

        if (CurrentSamplesCount_) {
            Frame_->nb_samples = CurrentSamplesCount_;
            Encode(FormatContext_, CodecContext_, Frame_, Packet_);
            CurrentSamplesCount_ = 0;
        }

        Encode(FormatContext_, CodecContext_, nullptr, Packet_);
        av_write_trailer(FormatContext_);
    }

protected:
    virtual void Write(const uint8_t* data, size_t size) = 0;

private:
    static void Encode(AVFormatContext* formatContext, AVCodecContext* context, const AVFrame* frame, AVPacket* packet) {
        int e = avcodec_send_frame(context, frame);
        if (e < 0) {
            throw std::runtime_error("Error sending the frame to the encoder");
        }

        while (true) {
            int ret = avcodec_receive_packet(context, packet);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            }
            if (ret < 0) {
                throw std::runtime_error("Error encoding audio frame");
            }
            if (av_write_frame(formatContext, packet) < 0) {
                throw std::runtime_error("Error writing frame");
            }
            av_packet_unref(packet);
        }
    }

    static int WriteCallback(void* ptr, uint8_t* buf, int buf_size) {
        static_cast<IAudioEncoder*>(ptr)->Write(buf, buf_size);
        return buf_size;
    }

    static int SelectSampleRate(const AVCodec *codec) {
        constexpr int defaultSampleRate = 44100;
        int resSampleRate = 0;
        for (const int* sampleRate = codec->supported_samplerates; sampleRate && *sampleRate; ++sampleRate) {
            if (!resSampleRate || std::abs(defaultSampleRate - *sampleRate) < std::abs(defaultSampleRate - resSampleRate)) {
                resSampleRate = *sampleRate;
            }
        }
        return resSampleRate ? resSampleRate : defaultSampleRate;
    }

    static bool CheckSampleFormat(const AVCodec *codec, enum AVSampleFormat sample_fmt) {
        for (const AVSampleFormat* fmt = codec->sample_fmts; *fmt != AV_SAMPLE_FMT_NONE; ++fmt) {
            if (*fmt == sample_fmt) {
                return true;
            }
        }
        return false;
    }

private:
    EState CurrentState_ = EState::Uninitialized;

    AVIOContext* IOContext_ = nullptr;
    AVFormatContext* FormatContext_ = nullptr;
    const AVCodec* Codec_ = nullptr;
    AVStream* Stream_ = nullptr;
    AVCodecContext* CodecContext_ = nullptr;
    AVFrame* Frame_ = nullptr;
    AVPacket* Packet_ = nullptr;

    size_t CurrentSamplesCount_ = 0;
};


class TAudioEncoder : public IAudioEncoder {
public:
    using IAudioEncoder::IAudioEncoder;

    void Write(const uint8_t* data, size_t size) override {
        Frame_.mutable_data()->insert(Frame_.mutable_data()->begin(), data, data + size);
    }

    NProto::TAudioFrame Consume() {
        NProto::TAudioFrame frame;
        std::swap(Frame_, frame);
        return frame;
    }

private:
    NProto::TAudioFrame Frame_;
};


class IAudioDecoder {
private:
    enum class EState {
        Uninitialized,
        Initialized,
        Finalized,
    };

public:
    IAudioDecoder(const IAudioDecoder&) = delete;
    IAudioDecoder(IAudioDecoder&&) noexcept = delete;
    IAudioDecoder& operator=(const IAudioDecoder&) = delete;
    IAudioDecoder& operator=(IAudioDecoder&&) noexcept = delete;

    IAudioDecoder(const NProto::TAudioMeta* meta = nullptr) {
        // TODO use meta if provided

        // Stream stuff

        const size_t bufferSize = 32768;
        auto buffer = static_cast<uint8_t*>(av_malloc(bufferSize));
        IOContext_ = avio_alloc_context(buffer, bufferSize, 0, this, &ReadCallback, nullptr, nullptr);
        if (!IOContext_) {
            av_free(buffer);
            throw std::runtime_error("Error allocating av io context");
        }

        // Buffers stuff

        Packet_ = av_packet_alloc();
        Frame_ = av_frame_alloc();
    }

    int32_t GetSampleRate() const {
        return CodecContext_->sample_rate;
    }

    void Decode(const uint8_t* data, size_t size) {
        Buffer_.insert(Buffer_.end(), data, data + size);

        if (CurrentState_ == EState::Uninitialized) {
            constexpr size_t probeSize = 32768 / 2;
            constexpr size_t analyzeDuration = 32768 / 2;
            if (Buffer_.size() < probeSize + analyzeDuration) {
                return;
            }
            CurrentState_= EState::Initialized;

            FormatContext_ = avformat_alloc_context();
            if (!FormatContext_) {
                throw std::runtime_error("Error allocating format context");
            }
            FormatContext_->pb = IOContext_;
            FormatContext_->pb->seekable = 0;
            FormatContext_->probesize = probeSize;
            FormatContext_->max_analyze_duration = analyzeDuration;
            FormatContext_->flags |= AVFMT_FLAG_CUSTOM_IO;

            if (int err = avformat_open_input(&FormatContext_, nullptr, nullptr, nullptr); err < 0) {
                char buf[20] {};
                throw std::runtime_error(std::string("Error opening input: ") + av_make_error_string(buf, sizeof(buf), err));
            }
            if (int err = avformat_find_stream_info(FormatContext_, nullptr); err < 0) {
                if (err == AVERROR(EAGAIN)) {
                    return;
                }
                throw std::runtime_error("Error finding stream info");
            }

            Stream_ = FormatContext_->streams[av_find_best_stream(FormatContext_, AVMEDIA_TYPE_AUDIO, -1, -1, &Codec_, 0)];

            // Codec stuff

            CodecContext_ = avcodec_alloc_context3(Codec_);
            if (!CodecContext_) {
                throw std::runtime_error("Could not allocate audio context");
            }

            avcodec_parameters_to_context(CodecContext_, Stream_->codecpar);

            if (avcodec_open2(CodecContext_, Codec_, nullptr) < 0) {
                throw std::runtime_error("Can't open codec");
            }
        }

        while (true) {
            int ret = av_read_frame(FormatContext_, Packet_);
            if (ret == AVERROR_EOF) {
                break;
            }
            if (ret < 0) {
                throw std::runtime_error("Error decoding audio packet");
            }
            if (avcodec_send_packet(CodecContext_, Packet_) < 0) {
                throw std::runtime_error("Error submitting the packet to the decoder");
            }
            while (true) {
                int ret = avcodec_receive_frame(CodecContext_, Frame_);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                }
                if (ret < 0) {
                    throw std::runtime_error("Error decoding audio frame");
                }
                int size = av_get_bytes_per_sample(CodecContext_->sample_fmt);
                if (size < 0) {
                    throw std::runtime_error("Something went wrong with size");
                }
                Write(reinterpret_cast<float*>(Frame_->data[0]), Frame_->nb_samples);
            }
        }
    }

protected:
    virtual void Write(const float* data, size_t samples) = 0;

    static int ReadCallback(void* ptr, uint8_t* buffer, int bufferSize) {
        auto self = static_cast<IAudioDecoder*>(ptr);
        size_t toCopy = std::min<size_t>(bufferSize, self->Buffer_.size());
        if (toCopy) {
            for (size_t i = 0; i < toCopy; ++i) {
                buffer[i] = self->Buffer_[i];
            }
            self->Buffer_.erase(self->Buffer_.begin(), self->Buffer_.begin() + toCopy);
            return toCopy;
        }
        return AVERROR(EAGAIN);
    }

private:
    EState CurrentState_ = EState::Uninitialized;

    std::deque<uint8_t> Buffer_;

    AVIOContext* IOContext_ = nullptr;
    AVFormatContext* FormatContext_ = nullptr;
    const AVCodec* Codec_ = nullptr;
    AVStream* Stream_ = nullptr;
    AVCodecContext* CodecContext_ = nullptr;
    AVFrame* Frame_ = nullptr;
    AVPacket* Packet_ = nullptr;
};


class TAudioDecoder : public IAudioDecoder {
public:
    using IAudioDecoder::IAudioDecoder;

    void Write(const float* data, size_t samples) override {
        Data_.insert(Data_.begin(), data, data + samples);
    }

    std::vector<float> Consume() {
        std::vector<float> data;
        std::swap(Data_, data);
        return data;
    }

private:
    std::vector<float> Data_;
};*/


}