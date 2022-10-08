#include <trueprompter/model.hpp>
#include <trueprompter/matcher.hpp>

#include <trueprompter/protocol.pb.h>

#include <websocketpp/server.hpp>
#include <websocketpp/config/asio.hpp>

#include <feat/wave-reader.h>

#include <iostream>
#include <vector>
#include <memory>
#include <map>
#include <cstdlib>

class TTruePrompterServer {
public:
    using TWebSocketServer = websocketpp::server<websocketpp::config::asio>;

    TTruePrompterServer(const std::shared_ptr<NTruePrompter::TModel>& model)
        : Model_(model)
    {}

    void Run(uint16_t port) {
        TWebSocketServer server;

        try {
            server.clear_access_channels(websocketpp::log::alevel::all);
            server.init_asio();

            server.set_open_handler([this](websocketpp::connection_hdl hdl) {
                State_.emplace(hdl, std::unique_ptr<NTruePrompter::TWordsMatcher>());
                std::cerr << "connected" << std::endl;
            });

            server.set_close_handler([this](websocketpp::connection_hdl hdl) {
                State_.erase(hdl);
                std::cerr << "disconnected" << std::endl;
            });

            server.set_message_handler([&server, this](websocketpp::connection_hdl hdl, TWebSocketServer::message_ptr msg) {
                std::cerr << "message" << std::endl;
                if (msg->get_opcode() != websocketpp::frame::opcode::binary) {
                    return;
                }

                auto& matcher = State_.at(hdl);

                NTruePrompter::TRequest request;
                request.ParseFromString(msg->get_payload());

                if (!request.words().empty()) {
                    std::cerr << "updating words" << std::endl;

                    std::vector<std::string> tmp(request.words().begin(), request.words().end());
                    matcher = std::make_unique<NTruePrompter::TWordsMatcher>(tmp, Model_);
                }

                if (!matcher) {
                    return;
                }

                if (request.has_position()) {
                    std::cerr << "updating position to " << request.position().word_index() << " + " << request.position().word_fraction() << std::endl;
                    matcher->SetCurrentPos({ request.position().word_index(), request.position().word_fraction() });
                }

                if (request.has_look_ahead()) {
                    std::cerr << "updating look ahead to " << request.look_ahead() << std::endl;
                    matcher->SetLookAhead(request.look_ahead());
                }

                if (!request.audio().empty() && request.sample_rate() > 0.0f) {
                    std::cerr << "updating audio" << std::endl;

                    matcher->AcceptWaveform(tcb::span(request.audio().begin(), request.audio().end()), request.sample_rate());
                    auto p = matcher->GetCurrentPos();
                    NTruePrompter::TResponse response;
                    auto* pos = response.mutable_position();
                    pos->set_word_index(p.first);
                    pos->set_word_fraction(p.second);
                    std::string data;
                    if (response.SerializeToString(&data)) {
                        std::cerr << "sending position " << p.first << " + " << p.second << std::endl;
                        server.send(hdl, data, websocketpp::frame::opcode::binary);
                    }
                }
            });

            server.listen(port);
            server.start_accept();
            server.run();
        } catch (websocketpp::exception const & e) {
            std::cerr << e.what() << std::endl;
        } catch (...) {
            std::cerr << "other exception" << std::endl;
        }
    }

private:
    std::map<websocketpp::connection_hdl, std::unique_ptr<NTruePrompter::TWordsMatcher>, std::owner_less<websocketpp::connection_hdl>> State_;
    std::shared_ptr<NTruePrompter::TModel> Model_;
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Expected <port> <model_path>" << std::endl;
        return -1;
    }
    std::cerr << "Initializing.." << std::endl;
    TTruePrompterServer server(std::make_shared<NTruePrompter::TModel>(argv[2]));
    std::cerr << "Started" << std::endl;
    server.Run(std::atoi(argv[1]));
}


/*int main(int argc, char* argv[]) {
    kaldi::WaveHolder h;
    std::ifstream f("../../wave.wav");
    h.Read(f);
    kaldi::Vector<kaldi::BaseFloat> input(h.Value().Data().NumCols());
    input.CopyRowFromMat(h.Value().Data(), 1);

    auto model = std::make_shared<NTruePrompter::TModel>(argv[2]);
    auto matcher = std::make_unique<NTruePrompter::TWordsMatcher>(std::vector<std::string>({ "арбуз" }), model);

    float ff = 0;
    for (int i = 0; i < input.Dim(); ++i) {
        if (std::abs(input.Data()[i]) > ff) {
            ff = std::abs(input.Data()[i]);
        }
    }
    std::cout << ff << std::endl;

    matcher->AcceptWaveform(tcb::span<const float>(input.Data(), input.Dim()), h.Value().SampFreq());

    auto p = matcher->GetCurrentPos();
    std::cout << p.first << " " << p.second << std::endl;
}*/
