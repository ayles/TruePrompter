#include "client_context.hpp"

#include <trueprompter/common/proto/protocol.pb.h>
#include <trueprompter/recognition/impl/online_matcher.hpp>
#include <trueprompter/recognition/impl/online_recognizer.hpp>
#include <trueprompter/recognition/impl/viterbi_matcher.hpp>
#include <trueprompter/recognition/onnx/onnx_recognizer.hpp>
#include <trueprompter/recognition/onnx/onnx_tokenizer.hpp>
#include <trueprompter/recognition/matcher.hpp>

#include <websocketpp/config/asio.hpp>
#include <websocketpp/server.hpp>

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <functional>
#include <map>
#include <memory>
#include <optional>

namespace NTruePrompter::NServer {

class TTruePrompterServer {
public:
    using TWebSocketServer = websocketpp::server<websocketpp::config::asio>;

    TTruePrompterServer(const std::filesystem::path& modelPath)
        : Model_(std::make_shared<NRecognition::TOnnxModel>(modelPath / "config.json", modelPath / "model.onnx", modelPath / "model.fst"))
    {}

    void Run(uint16_t port) {
        TWebSocketServer server;

        try {
            server.clear_access_channels(websocketpp::log::alevel::all);
            server.clear_error_channels(websocketpp::log::elevel::all);
            server.init_asio();

            server.set_open_handler([&server, this](websocketpp::connection_hdl hdl) {
                auto recognizer = std::make_shared<NRecognition::TOnlineRecognizer>(
                    std::make_shared<NRecognition::TOnnxRecognizer>(Model_),
                    1.0,
                    0.2,
                    0.2
                );
                auto tokenizer = std::make_shared<NRecognition::TOnnxTokenizer>(Model_);
                auto matcher = std::make_shared<NRecognition::TOnlineMatcher>(
                    std::make_shared<NRecognition::TViterbiMatcher>(tokenizer->GetBlankToken(), 5, 0.9),
                    1.0 * recognizer->GetSampleRate() / recognizer->GetFrameSize()
                );
                Clients_.emplace(
                    hdl,
                    std::make_shared<TClientContext>(
                        server.get_con_from_hdl(hdl)->get_remote_endpoint(),
                        std::make_shared<NRecognition::TPrompter>(
                            recognizer,
                            tokenizer,
                            matcher
                        )
                    )
                );
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
    std::shared_ptr<NRecognition::TOnnxModel> Model_;
    std::map<websocketpp::connection_hdl, std::shared_ptr<TClientContext>, std::owner_less<websocketpp::connection_hdl>> Clients_;
};

} // namespace NTruePrompter::NServer

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

    NTruePrompter::NServer::TTruePrompterServer server(argv[2]);
    SPDLOG_INFO("Started");
    server.Run(std::atoi(argv[1]));
}

