#pragma once

#include <trueprompter/codec/resampler.hpp>
#include <trueprompter/common/proto/protocol.pb.h>
#include <trueprompter/recognition/prompter.hpp>

#include <spdlog/spdlog.h>

#include <optional>

namespace NTruePrompter::NServer {

class TClientContext {
public:
    TClientContext(const TClientContext&) = delete;
    TClientContext(TClientContext&&) noexcept = delete;
    TClientContext& operator=(const TClientContext&) = delete;
    TClientContext& operator=(TClientContext&&) noexcept = delete;

    TClientContext(const std::string& clientId, std::shared_ptr<NRecognition::TPrompter> prompter)
        : ClientId_(clientId)
        , Prompter_(std::move(prompter))
    {
        SPDLOG_INFO("Client connected (client_id: \"{}\")", ClientId_);
    }

    ~TClientContext() {
        SPDLOG_INFO("Client disconnected (client_id: \"{}\")", ClientId_);
    }

    void Initialize(const NCommon::NProto::TRequest& request) {
        if (Initialized_) {
            return;
        }

        if (request.has_handshake()) {
            ClientName_ = request.handshake().client_name();
            Initialized_ = true;
            SPDLOG_INFO("Client initialized with handshake (client_id: \"{}\", handshake: {{ {} }})", ClientId_, request.handshake().ShortDebugString());
        } else {
            SPDLOG_WARN("Client message contains no handshake (client_id: \"{}\")", ClientId_);
            throw std::runtime_error("No handshake provided");
        }
    }

    std::optional<NCommon::NProto::TResponse> HandleMessage(const NCommon::NProto::TRequest& request) {
        SPDLOG_DEBUG("Client message received (client_id: \"{}\")", ClientId_);

        Initialize(request);

        std::optional<NCommon::NProto::TResponse> response;

        if (request.has_text_data()) {
            Prompter_->SetText(request.text_data().text());
            Prompter_->SetCursor(request.text_data().text_pos());
            SPDLOG_DEBUG("Client text data provided (client_id: \"{}\", text_data: {{ {} }})", ClientId_, request.text_data().ShortDebugString());
        }

        if (request.has_audio_data()) {
            if (!request.audio_data().data().empty()) {
                SPDLOG_DEBUG("Client audio data provided (client_id: \"{}\", audio_data: binary)", ClientId_);
                std::vector<float> data(request.audio_data().data().size() / sizeof(float));
                std::memcpy(data.data(), request.audio_data().data().data(), request.audio_data().data().size());
                std::vector<float> resampled;
                Resampler_.Resample(data, request.audio_data().meta().sample_rate(), resampled, Prompter_->GetRecognizer()->GetSampleRate());
                Prompter_->Update(resampled);

                if (!response) {
                    response.emplace();
                }

                response->mutable_recognition_result()->set_text_pos(Prompter_->GetCursor());
                SPDLOG_DEBUG("Client sending recognition result (client_id: \"{}\", recognition_result: {{ {} }})", ClientId_, response->recognition_result().ShortDebugString());
            }
        }

        if (response.has_value()) {
            response->set_user_data(request.user_data());
        }

        return response;
    }

private:
    bool Initialized_ = false;
    std::string ClientId_;
    std::string ClientName_;
    std::shared_ptr<NRecognition::TPrompter> Prompter_;
    NCodec::TResampler Resampler_;
};

} // namespace NTruePrompter::NServer

