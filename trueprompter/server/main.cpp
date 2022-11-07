#include <trueprompter/server/matcher.hpp>

#include <trueprompter/common/audio_codec.hpp>
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

    TClientContext(std::shared_ptr<NTruePrompter::NRecognition::IRecognizer> recognizer, const std::string& clientIdentity)
        : Recognizer_(std::move(recognizer))
        , ClientIdentity_(clientIdentity)
    {
        SPDLOG_INFO("Client connected (client: \"{}\")", ClientIdentity_);
    }

    ~TClientContext() {
        SPDLOG_INFO("Client disconnected (client: \"{}\")", ClientIdentity_);
    }

    void HandleMessage(
        const NTruePrompter::NProtocol::NProto::TRequest& request,
        const std::function<void(const NTruePrompter::NProtocol::NProto::TResponse&)>& callback)
    {
        SPDLOG_DEBUG("Client message received (client: \"{}\")", ClientIdentity_);

        if (request.has_text()) {
            SPDLOG_DEBUG("Client text properties changed (client: \"{}\", text: \"{}\")", ClientIdentity_, request.text());
            Recognizer_->Reset();
            Matcher_ = std::make_shared<NTruePrompter::NRecognition::TWordsMatcher>(request.text(), Recognizer_);
        }

        if (Matcher_) {
            if (request.has_text_offset()) {
                SPDLOG_DEBUG("Client text properties changed (client: \"{}\", text_offset: {})", ClientIdentity_, request.text_offset());
                Matcher_->SetCurrentPos(request.text_offset());
            }
            if (request.has_look_ahead()) {
                SPDLOG_DEBUG("Client text properties changed (client: \"{}\", look_ahead: {})", ClientIdentity_, request.look_ahead());
                Matcher_->SetLookAhead(request.look_ahead());
            }
        }

        if (request.has_audio_meta()) {
            SPDLOG_DEBUG("Client audio properties changed (client: \"{}\", audio_meta: {{ {} }})", ClientIdentity_, request.audio_meta().ShortDebugString());
            Decoder_ = NTruePrompter::NAudioCodec::CreateDecoder(request.audio_meta());
            Decoder_->SetCallback([this](const float* data, size_t size) {
                if (!data || !size) {
                    return;
                }
                SPDLOG_DEBUG("Client audio decoded (client: \"{}\", samples: [{}, ...])", ClientIdentity_, *data);
                if (Matcher_) {
                    Matcher_->AcceptWaveform(data, size, Decoder_->GetMeta().sample_rate());
                }
            });
        }

        if (Decoder_) {
            if (request.has_audio_data() && !request.audio_data().empty()) {
                SPDLOG_DEBUG("Client audio properties changed (client: \"{}\", audio_data: binary)", ClientIdentity_);
                Decoder_->Decode(reinterpret_cast<const uint8_t*>(request.audio_data().data()), request.audio_data().size());
                if (Matcher_) {
                    // TODO async
                    NTruePrompter::NProtocol::NProto::TResponse response;
                    response.set_text_offset(Matcher_->GetCurrentPos());
                    callback(response);
                }
            }
        }
    }

private:
    std::shared_ptr<NTruePrompter::NRecognition::IRecognizer> Recognizer_;
    std::shared_ptr<NTruePrompter::NRecognition::TWordsMatcher> Matcher_;
    std::shared_ptr<NTruePrompter::NAudioCodec::IAudioDecoder> Decoder_;
    std::string ClientIdentity_;
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
                if (msg->get_opcode() != websocketpp::frame::opcode::binary) {
                    SPDLOG_WARN("Server non-binary message received (client: \"{}\")", server.get_con_from_hdl(hdl)->get_remote_endpoint());
                    return;
                }

                std::shared_ptr<TClientContext>& clientContext = Clients_.at(hdl);

                NTruePrompter::NProtocol::NProto::TRequest request;
                if (!request.ParseFromString(msg->get_payload())) {
                    SPDLOG_WARN("Server broken message received (client: \"{}\")", server.get_con_from_hdl(hdl)->get_remote_endpoint());
                    return;
                }

                Clients_.at(hdl)->HandleMessage(request, [&server, &hdl](const NTruePrompter::NProtocol::NProto::TResponse& response) {
                    server.send(hdl, response.SerializeAsString(), websocketpp::frame::opcode::binary);
                });
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
