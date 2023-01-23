#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>


namespace NTruePrompter::NRecognition {

class TKaldiModel;
class IRecognizerFactory;
class ITokenizerFactory;

std::shared_ptr<TKaldiModel> LoadKaldiModel(const std::filesystem::path& path);
std::shared_ptr<IRecognizerFactory> NewKaldiRecognizerFactory(std::unordered_map<std::string, std::shared_ptr<TKaldiModel>> models);
std::shared_ptr<ITokenizerFactory> NewKaldiTokenizerFactory(std::unordered_map<std::string, std::shared_ptr<TKaldiModel>> models);

} // namespace NTruePrompter::NRecognition

