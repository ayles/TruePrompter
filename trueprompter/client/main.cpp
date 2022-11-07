#include "audio_source.hpp"

#include <trueprompter/common/audio_codec.hpp>
#include <trueprompter/common/proto/protocol.pb.h>

#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

#include <cstddef>
#include <vector>
#include <iostream>
#include <thread>
#include <fstream>
#include <mutex>


int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: " << argv[0] << " <uri> [text_file]" << std::endl;
        return -1;
    }

    std::stringstream buffer;
    if (argc == 2) {
        buffer << std::cin.rdbuf();
    } else {
        buffer << std::ifstream(argv[2]).rdbuf();
    }

    std::string uri = argv[1];
    std::string text = buffer.str();

    // Init encoder
    NTruePrompter::NAudioCodec::NProto::TAudioMeta meta;
    meta.set_codec(NTruePrompter::NAudioCodec::NProto::ECodec::RAW_PCM_F32LE);
    meta.set_sample_rate(48000);
    auto encoder = NTruePrompter::NAudioCodec::CreateEncoder(meta);

    // Init audio input
    auto audioSource = NTruePrompter::NAudioSource::MakeMicrophoneAudioSource(encoder->GetMeta().sample_rate());

    websocketpp::client<websocketpp::config::asio_client> client;
    client.clear_access_channels(websocketpp::log::alevel::all);
    client.init_asio();

    std::mutex lock;
    std::optional<std::thread> thread;

    client.set_open_handler([&](websocketpp::connection_hdl hdl) {
        thread.emplace([&, hdl]() {
            {
                std::scoped_lock guard(lock);
                NTruePrompter::NProtocol::NProto::TRequest initialRequest;
                initialRequest.set_text(text);
                initialRequest.set_text_offset(0);
                initialRequest.set_look_ahead(100);
                *initialRequest.mutable_audio_meta() = encoder->GetMeta();
                client.send(hdl, initialRequest.SerializeAsString(), websocketpp::frame::opcode::value::binary);
            }

            std::vector<float> audioBuffer(8192);

            while (!hdl.expired()) {
                std::scoped_lock guard(lock);

                NTruePrompter::NProtocol::NProto::TRequest request;
                size_t samples = audioSource->Read(audioBuffer.data(), audioBuffer.size());
                encoder->SetCallback([&request](const uint8_t* data, size_t size) {
                    request.mutable_audio_data()->insert(request.mutable_audio_data()->begin(), data, data + size);
                });
                encoder->Encode(audioBuffer.data(), samples);

                client.send(hdl, request.SerializeAsString(), websocketpp::frame::opcode::value::binary);
            }
        });
    });

    client.set_message_handler([&, currentPosition = (size_t)0](websocketpp::connection_hdl, websocketpp::config::asio_client::message_type::ptr message) mutable {
        if (message->get_opcode() != websocketpp::frame::opcode::value::binary) {
            throw std::runtime_error("Error");
        }

        NTruePrompter::NProtocol::NProto::TResponse response;
        response.ParseFromString(message->get_payload());
        size_t newPosition = std::clamp<size_t>(response.text_offset(), currentPosition, text.size());
        std::cout << text.substr(currentPosition, newPosition - currentPosition) << std::flush;
        currentPosition = newPosition;
    });

    client.set_close_handler([&](websocketpp::connection_hdl) {
        std::scoped_lock guard(lock);
    });

    client.set_fail_handler([&](websocketpp::connection_hdl) {
        std::scoped_lock guard(lock);
    });

    websocketpp::lib::error_code ec;
    auto con = client.get_connection(uri, ec);
    if (ec) {
        std::cerr << "Could not create connection, error: " << ec.message() << std::endl;
        return 0;
    }

    std::cout << "> " << text << std::endl;

    client.connect(con);
    client.run();
    if (thread) {
        thread->join();
    }
}
