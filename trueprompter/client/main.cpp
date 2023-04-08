#include "audio_source.hpp"

#include <trueprompter/common/proto/protocol.pb.h>

#include <utf8.h>

#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

#include <cstddef>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>


int main(int argc, char* argv[]) {
    if (argc < 3 || argc > 4) {
        std::cerr << "Usage: " << argv[0] << " <uri> <language> [text_file]" << std::endl;
        return -1;
    }

    std::stringstream buffer;
    if (argc == 3) {
        buffer << std::cin.rdbuf();
    } else {
        buffer << std::ifstream(argv[3]).rdbuf();
    }

    std::string uri = argv[1];
    std::string language = argv[2];
    std::string text = buffer.str();

    // Init encoder
    NTruePrompter::NCodec::NProto::TAudioMeta meta;
    meta.set_format(NTruePrompter::NCodec::NProto::EFormat::RAW);
    meta.set_codec(NTruePrompter::NCodec::NProto::ECodec::PCM_F32LE);
    meta.set_sample_rate(16000);

    // Init audio input
    auto audioSource = NTruePrompter::NAudioSource::MakeMicrophoneAudioSource(16000);

    /*{
        std::ofstream f("out.ogg");
        for (size_t i = 0; i < 10; ++i) {
            std::vector<float> audioBuffer(8192);
            size_t samples = audioSource->Read(audioBuffer.data(), audioBuffer.size());
            encoder->SetCallback([&f](const uint8_t* data, size_t size) {
                f.write(reinterpret_cast<const char*>(data), size);
            });
            encoder->Encode(audioBuffer.data(), samples);
        }
        encoder->Finalize();
        f.flush();
        f.close();
        return 0;
    }*/

    websocketpp::client<websocketpp::config::asio_client> client;
    client.clear_access_channels(websocketpp::log::alevel::all);
    client.init_asio();

    std::mutex lock;
    std::optional<std::thread> thread;

    client.set_open_handler([&](websocketpp::connection_hdl hdl) {
        thread.emplace([&, hdl]() {
            {
                std::scoped_lock guard(lock);

                NTruePrompter::NCommon::NProto::TRequest initialMessage;
                initialMessage.mutable_handshake()->set_client_name("trueprompter_client");
                initialMessage.mutable_text_data()->set_text(text);
                initialMessage.mutable_text_data()->set_language(language);
                *initialMessage.mutable_audio_data()->mutable_meta() = meta;
                initialMessage.mutable_matcher_params()->mutable_look_ahead()->set_value(100);
                client.send(hdl, initialMessage.SerializeAsString(), websocketpp::frame::opcode::value::binary);
            }

            std::vector<float> audioBuffer(8192);

            while (!hdl.expired()) {
                std::scoped_lock guard(lock);

                NTruePrompter::NCommon::NProto::TRequest request;
                size_t samples = audioSource->Read(audioBuffer.data(), audioBuffer.size());
                request.mutable_audio_data()->mutable_data()->insert(request.mutable_audio_data()->mutable_data()->end(), reinterpret_cast<char*>(audioBuffer.data()), reinterpret_cast<char*>(audioBuffer.data()) + samples * sizeof(float));

                client.send(hdl, request.SerializeAsString(), websocketpp::frame::opcode::value::binary);
            }
        });
    });

    client.set_message_handler([&, currentPosition = text.begin()](websocketpp::connection_hdl, websocketpp::config::asio_client::message_type::ptr message) mutable {
        if (message->get_opcode() != websocketpp::frame::opcode::value::binary) {
            throw std::runtime_error("Error");
        }

        NTruePrompter::NCommon::NProto::TResponse response;
        response.ParseFromString(message->get_payload());

        if (response.msg_case() == NTruePrompter::NCommon::NProto::TResponse::kRecognitionResult) {
            auto it = text.begin();
            utf8::advance(it, response.recognition_result().text_pos(), text.end());
            it = it > currentPosition ? it : currentPosition;
            std::cout << std::string(currentPosition, it) << std::flush;
            currentPosition = it;
        } else if (response.msg_case() == NTruePrompter::NCommon::NProto::TResponse::kError) {
            std::cerr << "Error (code: " << response.error().code() << "): " << response.error().what() << std::endl;
        } else {
            std::cerr << "Unknown response message type" << std::endl;
        }
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
