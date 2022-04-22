#include <portaudio.h>
#include <online/online-audio-source.h>

#include "recognizer.h"

#include <iostream>
#include <algorithm>

static constexpr size_t SampleRate = 48000;

int main() {
    kaldi::OnlinePaSource pa(0, SampleRate, SampleRate * 10, 0);
    kaldi::Vector<kaldi::BaseFloat> vec;
    vec.Resize(SampleRate / 2);

    /*auto paCheck = [](PaError err) {
        if (err != paNoError) {
            throw std::runtime_error(Pa_GetErrorText(err));
        }
    };

    paCheck(Pa_Initialize());

    PaStreamParameters inputParams {
        .device = Pa_GetDefaultInputDevice(),
        .channelCount = 1,
        .sampleFormat = paFloat32,
        .suggestedLatency = Pa_GetDeviceInfo(Pa_GetDefaultInputDevice())->defaultLowInputLatency,
        .hostApiSpecificStreamInfo = nullptr,
    };

    PaStream* stream;
    paCheck(Pa_OpenStream(&stream, &inputParams, nullptr, SampleRate, FramesPerBuffer, paClipOff, RecordCallback, nullptr));
    paCheck(Pa_StartStream(stream));*/

    Recognizer r(new Model("../small_model"), SampleRate);
    r.SetPartialWords(true);

    while (true) {
        if (!pa.Read(&vec)) {
            continue;
        }
        std::stringstream ss;
        if (r.AcceptWaveform(vec)) {
            ss << r.Result() << std::endl;
        } else {
            ss << r.PartialResult() << std::endl;
        }
        //std::cout << ss.str();
    }

    /*paCheck(Pa_CloseStream(stream));

    paCheck(Pa_Terminate());*/

    return 0;
}
