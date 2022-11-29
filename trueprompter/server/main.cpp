#include <trueprompter/recognition/matcher.hpp>
#include <trueprompter/codec/audio_codec.hpp>
#include <trueprompter/common/proto/protocol.pb.h>

#include <websocketpp/server.hpp>
#include <websocketpp/config/asio.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include <memory>
#include <map>
#include <cstdlib>
#include <functional>


class TClientContext {
public:
    TClientContext(const TClientContext&) = delete;
    TClientContext(TClientContext&&) noexcept = delete;
    TClientContext& operator=(const TClientContext&) = delete;
    TClientContext& operator=(TClientContext&&) noexcept = delete;

    TClientContext(std::shared_ptr<NTruePrompter::NRecognition::IRecognizer> recognizer, const std::string& clientId)
        : ClientId_(clientId)
        , Recognizer_(std::move(recognizer))
        , Matcher_(std::make_shared<NTruePrompter::NRecognition::TWordsMatcher>("", Recognizer_))
    {
        SPDLOG_INFO("Client connected (client_id: \"{}\")", ClientId_);
    }

    ~TClientContext() {
        SPDLOG_INFO("Client disconnected (client_id: \"{}\")", ClientId_);
    }

    std::optional<NTruePrompter::NCommon::NProto::TResponse> HandleMessage(const NTruePrompter::NCommon::NProto::TRequest& request) {
        SPDLOG_DEBUG("Client message received (client_id: \"{}\")", ClientId_);

        if (!Initialized_) {
            if (request.has_handshake()) {
                ClientName_ = request.handshake().client_name();
                Initialized_ = true;
                SPDLOG_INFO("Client initialized with handshake (client_id: \"{}\", handshake: {{ {} }})", ClientId_, request.handshake().ShortDebugString());
            } else {
                SPDLOG_WARN("Client message contains no handshake (client_id: \"{}\")", ClientId_);
                throw std::runtime_error("No handshake provided");
            }
        }

        if (request.has_text_data()) {
            // TODO do not recreate matcher and do not reset recognizer
            Recognizer_->Reset();
            auto params = Matcher_->GetMatchParameters();
            Matcher_ = std::make_shared<NTruePrompter::NRecognition::TWordsMatcher>(request.text_data().text(), Recognizer_);
            Matcher_->SetCurrentPos(request.text_data().text_pos());
            Matcher_->SetMatchParameters(params);
            SPDLOG_DEBUG("Client text data provided (client_id: \"{}\", text_data: {{ {} }})", ClientId_, request.text_data().ShortDebugString());
        }

        if (request.has_matcher_params()) {
            // TODO rework parsing
            NTruePrompter::NRecognition::TPhonemesMatcher::TMatchParameters params;
            if (request.matcher_params().has_look_ahead()) {
                params.LookAhead = request.matcher_params().look_ahead().value();
            }
            if (request.matcher_params().has_fade_over_look_ahead()) {
                params.FadeOverLookAhead = request.matcher_params().fade_over_look_ahead().value();
            }
            if (request.matcher_params().has_similar_score()) {
                params.SimilarScore = request.matcher_params().similar_score().value();
            }
            if (request.matcher_params().has_different_score()) {
                params.DifferentScore = request.matcher_params().different_score().value();
            }
            if (request.matcher_params().has_source_skip_weight()) {
                params.SourceSkipWeight = request.matcher_params().source_skip_weight().value();
            }
            if (request.matcher_params().has_target_skip_weight()) {
                params.TargetSkipWeight = request.matcher_params().target_skip_weight().value();
            }
            if (request.matcher_params().has_min_match_weight()) {
                params.MinMatchWeight = request.matcher_params().min_match_weight().value();
            }
            Matcher_->SetMatchParameters(params);
            SPDLOG_DEBUG("Client matcher parameters changed (client_id: \"{}\", matcher_parameters: {{ {} }})", ClientId_, request.matcher_params().ShortDebugString());
        }

        if (request.has_audio_data()) {
            if (request.audio_data().has_meta()) {
                SPDLOG_DEBUG("Client audio meta set (client_id: \"{}\", audio_meta: {{ {} }})", ClientId_, request.audio_data().meta().ShortDebugString());
                if (!Decoder_ || !NTruePrompter::NCodec::IsMetaEquivalent(request.audio_data().meta(), Decoder_->GetMeta())) {
                    Decoder_ = NTruePrompter::NCodec::CreateDecoder(request.audio_data().meta());
                    Decoder_->SetCallback([this](const float* data, size_t size) {
                        if (!data || !size) {
                            return;
                        }
                        Matcher_->AcceptWaveform(data, size, Decoder_->GetSampleRate());
                        SPDLOG_DEBUG("Client audio decoded (client_id: \"{}\", samples: [{}, ...])", ClientId_, *data);
                    });
                    SPDLOG_DEBUG("Client decoder reset (client_id: \"{}\")", ClientId_);
                }
            }
            if (!request.audio_data().data().empty()) {
                if (!Decoder_) {
                    SPDLOG_WARN("Client message contains audio data, but no meta provided (client_id: \"{}\")", ClientId_);
                    throw std::runtime_error("No audio meta provided");
                }
                SPDLOG_DEBUG("Client audio data provided (client_id: \"{}\", audio_data: binary)", ClientId_);
                Decoder_->Decode(reinterpret_cast<const uint8_t*>(request.audio_data().data().data()), request.audio_data().data().size());
                // TODO async
                NTruePrompter::NCommon::NProto::TResponse response;
                response.mutable_recognition_result()->set_text_pos(Matcher_->GetCurrentPos());
                SPDLOG_DEBUG("Client sending recognition result (client_id: \"{}\", recognition_result: {{ {} }})", ClientId_, response.recognition_result().ShortDebugString());
                return response;
            }
        }

        return std::nullopt;
    }

private:
    bool Initialized_ = false;
    std::string ClientId_;
    std::string ClientName_;
    std::shared_ptr<NTruePrompter::NRecognition::IRecognizer> Recognizer_;
    std::shared_ptr<NTruePrompter::NRecognition::TWordsMatcher> Matcher_;
    std::shared_ptr<NTruePrompter::NCodec::IAudioDecoder> Decoder_;
};

class TTruePrompterServer {
public:
    using TWebSocketServer = websocketpp::server<websocketpp::config::asio>;

    TTruePrompterServer(const std::shared_ptr<NTruePrompter::NRecognition::IRecognizerFactory>& recognizerFactory)
        : RecognizerFactory_(recognizerFactory)
    {}

    void Run(uint16_t port) {
        TWebSocketServer server;

        try {
            server.clear_access_channels(websocketpp::log::alevel::all);
            server.clear_error_channels(websocketpp::log::elevel::all);
            server.init_asio();

            server.set_open_handler([&server, this](websocketpp::connection_hdl hdl) {
                Clients_.emplace(hdl, std::make_shared<TClientContext>(RecognizerFactory_->Create(), server.get_con_from_hdl(hdl)->get_remote_endpoint()));
            });

            server.set_close_handler([this](websocketpp::connection_hdl hdl) {
                Clients_.erase(hdl);
            });

            server.set_message_handler([&server, this](websocketpp::connection_hdl hdl, TWebSocketServer::message_ptr msg) {
                std::optional<NTruePrompter::NCommon::NProto::TResponse> res;
                bool shouldClose = false;

                try {
                    if (msg->get_opcode() != websocketpp::frame::opcode::binary) {
                        SPDLOG_WARN("Server non-binary message received (client_id: \"{}\")", server.get_con_from_hdl(hdl)->get_remote_endpoint());
                        throw std::runtime_error("Non-binary message received");
                    }

                    std::shared_ptr<TClientContext>& clientContext = Clients_.at(hdl);

                    NTruePrompter::NCommon::NProto::TRequest request;
                    if (!request.ParseFromString(msg->get_payload())) {
                        SPDLOG_WARN("Server broken message received (client_id: \"{}\")", server.get_con_from_hdl(hdl)->get_remote_endpoint());
                        throw std::runtime_error("Broken message received");
                    }

                    res = Clients_.at(hdl)->HandleMessage(request);
                } catch (const std::exception& e) {
                    res.emplace();
                    res->mutable_error()->set_what(e.what());
                    shouldClose = true;
                } catch (...) {
                    res.emplace();
                    res->mutable_error()->set_what("generic error");
                    shouldClose = true;
                }

                try {
                    if (res.has_value()) {
                        server.send(hdl, res->SerializeAsString(), websocketpp::frame::opcode::binary);
                    }
                } catch (...) {
                    shouldClose = true;
                }

                if (shouldClose) {
                    server.close(hdl, res.has_value() ? res->error().code() : -1, res.has_value() ? res->error().what() : "");
                }
            });

            server.listen(port);
            server.start_accept();
            server.run();
        } catch (const std::exception& e) {
            SPDLOG_ERROR("Server received exception (error: \"{}\")", e.what());
        }
    }

private:
    std::shared_ptr<NTruePrompter::NRecognition::IRecognizerFactory> RecognizerFactory_;
    std::map<websocketpp::connection_hdl, std::shared_ptr<TClientContext>, std::owner_less<websocketpp::connection_hdl>> Clients_;
};

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Expected <port> <model_path> [<info_log_file> [<debug_log_file>]]" << std::endl;
        return -1;
    }

    {
        std::vector<spdlog::sink_ptr> sinks;

        auto consoleSink = std::make_shared<spdlog::sinks::stderr_sink_mt>();
        consoleSink->set_level(spdlog::level::info);
        sinks.emplace_back(std::move(consoleSink));

        if (argc >= 4) {
            auto infoSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(argv[3], 10 * 1048576, 2);
            infoSink->set_level(spdlog::level::info);
            sinks.emplace_back(std::move(infoSink));
        }

        if (argc >= 5) {
            auto debugSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(argv[4], 10 * 1048576, 2);
            debugSink->set_level(spdlog::level::debug);
            sinks.emplace_back(std::move(debugSink));
        }

        auto logger = std::make_shared<spdlog::logger>("logger", sinks.begin(), sinks.end());
        logger->set_level(spdlog::level::trace);
        logger->flush_on(spdlog::level::info);
        spdlog::set_default_logger(std::move(logger));
    }

    SPDLOG_INFO("Initializing..");
    TTruePrompterServer server(NTruePrompter::NRecognition::CreateRecognizerFactory(argv[2]));
    SPDLOG_INFO("Started");
    server.Run(std::atoi(argv[1]));
}
