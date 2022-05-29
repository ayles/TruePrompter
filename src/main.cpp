#include <trueprompter/model.hpp>
#include <trueprompter/matcher.hpp>

#include <trueprompter/protocol.pb.h>

#include <websocketpp/server.hpp>
#include <websocketpp/config/asio.hpp>

//#include <online/online-audio-source.h>

#include <iostream>
#include <vector>
#include <memory>
#include <map>

class TTruePrompterServer {
public:
    using TWebSocketServer = websocketpp::server<websocketpp::config::asio>;

    TTruePrompterServer(const std::shared_ptr<NTruePrompter::TModel>& model)
        : Model_(model)
    {}

    void Run() {
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
                    std::cerr << "updating position" << std::endl;
                    matcher->SetCurrentPos({ request.position().word_index(), request.position().word_fraction() });
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
                        std::cerr << "sending position" << std::endl;
                        server.send(hdl, data, websocketpp::frame::opcode::binary);
                    }
                }
            });

            server.listen(8080);
            server.start_accept();
            server.run();
        } catch (websocketpp::exception const & e) {
            std::cout << e.what() << std::endl;
        } catch (...) {
            std::cout << "other exception" << std::endl;
        }
    }

private:
    std::map<websocketpp::connection_hdl, std::unique_ptr<NTruePrompter::TWordsMatcher>, std::owner_less<websocketpp::connection_hdl>> State_;
    std::shared_ptr<NTruePrompter::TModel> Model_;
};

int main() {
    std::cerr << "Initializing.." << std::endl;
    TTruePrompterServer server(std::make_shared<NTruePrompter::TModel>("../../small_model"));
    std::cerr << "Started" << std::endl;
    server.Run();
}


/*int main() {
    // Needs ICU or smth alike to make Lower()
    std::vector<std::string> words1 = { "карл", "у", "клары", "украл", "кораллы", "клара", "у", "карла", "украла", "кларнет" };
    std::vector<std::string> words = { "раз", "два", "три", "четыре", "пять" };

    std::cerr << "Loading model..." << std::endl;
    auto model = std::make_shared<NTruePrompter::TModel>("../../small_model");
    std::cerr << "Phoneticizing..." << std::endl;
    auto matcher = std::make_shared<NTruePrompter::TWordsMatcher>(words, model);
    std::cerr << "Done" << std::endl;

    constexpr float sampleRate = 48000;
    kaldi::OnlinePaSource pa(0, sampleRate, sampleRate * 10, 0);
    kaldi::Vector<kaldi::BaseFloat> vec;
    vec.Resize(sampleRate / 2);

    while (true) {
        if (!pa.Read(&vec)) {
            continue;
        }

        matcher->AcceptWaveform(tcb::span<float>(vec.Data(), vec.Dim()), sampleRate);

        std::cout << tcb::span<float>(vec.Data(), vec.Dim())[0] << std::endl;

        std::cout << "\r";
        for (size_t i = 0; i < words.size(); ++i) {
            if (i < matcher->GetCurrentPos().first) {
                std::cout << "\x1B[31m" << words[i] << "\x1B[0m";
            } else {
                std::cout << words[i];
            }
            std::cout << " ";
        }
    }

    return 0;
}*/
