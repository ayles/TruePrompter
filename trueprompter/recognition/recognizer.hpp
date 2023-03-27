#pragma once

#include <Eigen/Dense>

#include <span>
#include <vector>
#include <memory>

namespace NTruePrompter::NRecognition {

class IRecognizer {
public:
    // Should return matrix with columnsN = time slices count, rowsN = phones count, values are log probabilites
    virtual Eigen::Map<Eigen::MatrixXf> Update(std::span<const float> data, std::vector<float>* buf) = 0;
    virtual int64_t GetSampleRate() const = 0;
    virtual int64_t GetFrameSize() const = 0;
    virtual ~IRecognizer() = default;
};

class TContextualRecognizer : public IRecognizer {
public:
    TContextualRecognizer(std::shared_ptr<IRecognizer> recognizer, float chunkLength, float leftStrideLength, float rightStrideLength)
        : Recognizer_(std::move(recognizer))
        , ChunkLength_((int64_t)std::round(chunkLength * Recognizer_->GetSampleRate() / Recognizer_->GetFrameSize()) * Recognizer_->GetFrameSize())
        , LeftStrideLength_((int64_t)std::round(leftStrideLength * Recognizer_->GetSampleRate() / Recognizer_->GetFrameSize()) * Recognizer_->GetFrameSize())
        , RightStrideLength_((int64_t)std::round(rightStrideLength * Recognizer_->GetSampleRate() / Recognizer_->GetFrameSize()) * Recognizer_->GetFrameSize())
    {
        if (ChunkLength_ < LeftStrideLength_ + RightStrideLength_) {
            throw std::runtime_error("Chunk length must be superior to stride length");
        }
    }

    // TODO do not forget that to finish recognition we should do recognition without right context
    Eigen::Map<Eigen::MatrixXf> Update(std::span<const float> data, std::vector<float>* buf) override {
        size_t lastRows = 0;
        buf->clear();

        while (DataContext_.size() + data.size() >= ChunkLength_) {
            if (DataContext_.size() > ChunkLength_) {
                throw std::runtime_error("Should never happen");
            }
            const int64_t currentLeftStride = std::min<int64_t>(DataContext_.size(), LeftStrideLength_);
            const size_t toConsume = ChunkLength_ - DataContext_.size();
            DataContext_.insert(DataContext_.end(), data.begin(), data.begin() + toConsume);
            data = { data.data() + toConsume, data.size() - toConsume };
            auto emissions = Recognizer_->Update(DataContext_, &BufContext_);
            auto begin = emissions.data() + currentLeftStride / Recognizer_->GetFrameSize() * emissions.rows();
            auto end = emissions.data() + emissions.size() - RightStrideLength_ / Recognizer_->GetFrameSize() * emissions.rows();
            buf->insert(buf->end(), begin < end ? begin : end, end);
            lastRows = emissions.rows();

            DataContext_.erase(DataContext_.begin(), DataContext_.end() - LeftStrideLength_ - RightStrideLength_);
        }
        DataContext_.insert(DataContext_.end(), data.begin(), data.end());

        if (lastRows) {
            return Eigen::Map<Eigen::MatrixXf>(buf->data(), lastRows, buf->size() / lastRows);
        } else {
            return Eigen::Map<Eigen::MatrixXf>(nullptr, 0, 0);
        }
    }

    int64_t GetSampleRate() const override {
        return Recognizer_->GetSampleRate();
    }

    int64_t GetFrameSize() const override {
        return Recognizer_->GetFrameSize();
    }

private:
    std::shared_ptr<IRecognizer> Recognizer_;
    int64_t ChunkLength_;
    int64_t LeftStrideLength_;
    int64_t RightStrideLength_;
    std::vector<float> DataContext_;
    std::vector<float> BufContext_;
};

} // NTruePrompter::NRecognition

