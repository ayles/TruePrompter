#pragma once

#include <samplerate.h>

#include <cmath>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace NTruePrompter::NCodec {

class TResampler {
public:
    void Resample(std::span<const float> input, int64_t inputSampleRate, std::vector<float>& output, int64_t outputSampleRate) const {
        if (inputSampleRate < 8000 || inputSampleRate > 100000 || outputSampleRate < 8000 || outputSampleRate > 100000) {
            throw std::runtime_error("Invalid sample rate");
        }
        double ratio = (double)outputSampleRate / inputSampleRate;
        output.resize(std::ceil(input.size() * ratio));
        SRC_DATA srcData {
            .data_in = input.data(),
            .data_out = output.data(),
            .input_frames = (long)input.size(),
            .output_frames = (long)output.size(),
            .src_ratio = ratio,
        };
        int err = src_simple(&srcData, SRC_LINEAR, 1);
        if (err != 0) {
            throw std::runtime_error(src_strerror(err));
        }
        output.resize(srcData.output_frames_gen);
    }
};

} // namespace NTruePrompter::NCodec

