#pragma once

#include <functional>
#include <vector>
#include <algorithm>
#include <array>
#include <tuple>
#include <span>


namespace NTruePrompter::NRecognition {


template<typename T, typename V>
std::tuple<std::span<T>, std::span<T>, V> SmithWaterman(
    std::span<T> source,
    std::span<T> target,
    const std::function<V(const T*, const T*)>& sourceSkip,
    const std::function<V(const T*, const T*)>& targetSkip,
    const std::function<V(const T*, const T*)>& similarityScore)
{
    std::vector<std::vector<V>> score(source.size() + 1, std::vector<V>(target.size() + 1));
    std::vector<std::vector<std::pair<size_t, size_t>>> traceback(source.size() + 1, std::vector<std::pair<size_t, size_t>>(target.size() + 1));

    for (size_t i = 1; i <= source.size(); ++i) {
        for(size_t j = 1; j <= target.size(); ++j) {
            std::array<std::tuple<size_t, size_t, V>, 4> alts {{
                { 1, 1, score[i - 1][j - 1] + similarityScore(source.data() + i, target.data() + j) },
                { 1, 0, score[i - 1][j] + sourceSkip(source.data() + i, target.data() + j) },
                { 0, 1, score[i][j - 1] + targetSkip(source.data() + i, target.data() + j) },
                { 0, 0, V() },
            }};

            auto it = std::max_element(alts.begin(), alts.end(), [](const auto& a, const auto& b) {
                return std::get<2>(a) < std::get<2>(b);
            });

            auto& [is, js, v] = *it;

            score[i][j] = v;
            traceback[i][j] = { i - is, j - js };
        }
    }

    std::pair<size_t, size_t> maxElem;
    for (size_t i = 1; i <= source.size(); ++i) {
        for (size_t j = 1; j <= target.size(); ++j) {
            if (score[i][j] > score[maxElem.first][maxElem.second]) {
                maxElem = { i, j };
            }
        }
    }

    std::pair<size_t, size_t> prev = { maxElem.first + 1, maxElem.second + 1 };
    std::pair<size_t, size_t> curr = maxElem;

    while (score[curr.first][curr.second] != V()) {
        prev = curr;
        curr = traceback[curr.first][curr.second];
    }

    return {
        std::span<T>(source.data() + prev.first, source.data() + maxElem.first + 1),
        std::span<T>(target.data() + prev.second, target.data() + maxElem.second + 1),
        score[maxElem.first][maxElem.second],
    };
}


} // namespace NTruePrompter::NServer
