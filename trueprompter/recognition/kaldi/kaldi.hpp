#pragma once

#include <filesystem>
#include <memory>


namespace NTruePrompter::NRecognition {

class TKaldiModel;
class IRecognizerFactory;
class ITokenizerFactory;

std::shared_ptr<TKaldiModel> LoadKaldiModel(const std::filesystem::path& path);
std::shared_ptr<IRecognizerFactory> NewKaldiRecognizerFactory(const std::shared_ptr<TKaldiModel>& model);
std::shared_ptr<ITokenizerFactory> NewKaldiTokenizerFactory(const std::shared_ptr<TKaldiModel>& model);

} // namespace NTruePrompter::NRecognition

