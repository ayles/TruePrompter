#include <trueprompter/recognition/recognizer.hpp>

namespace NTruePrompter::NRecognition {

class TOnlineRecognizer : public IRecognizer {
public:
    TOnlineRecognizer(std::shared_ptr<IRecognizer> recognizer, float chunkLength, float leftStrideLength, float rightStrideLength)
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
            const size_t toConsume = ChunkLength_ - DataContext_.size();
            DataContext_.insert(DataContext_.end(), data.begin(), data.begin() + toConsume);
            data = { data.data() + toConsume, data.size() - toConsume };
            auto emission = Recognizer_->Update(DataContext_, &BufContext_);
            auto begin = emission.data() + LeftStrideLength_ / Recognizer_->GetFrameSize() * emission.rows();
            auto end = emission.data() + emission.size() - RightStrideLength_ / Recognizer_->GetFrameSize() * emission.rows();
            buf->insert(buf->end(), begin < end ? begin : end, end);
            lastRows = emission.rows();

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

    void Reset() override {
        Recognizer_->Reset();
        DataContext_.clear();
    }

private:
    std::shared_ptr<IRecognizer> Recognizer_;
    int64_t ChunkLength_;
    int64_t LeftStrideLength_;
    int64_t RightStrideLength_;
    std::vector<float> DataContext_;
    std::vector<float> BufContext_;
};

} // namespace NTruePrompter::NRecognition

