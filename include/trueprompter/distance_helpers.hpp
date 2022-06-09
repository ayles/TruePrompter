#pragma once

#include <tcb/span.hpp>

#include <functional>
#include <vector>
#include <algorithm>
#include <array>
#include <tuple>

namespace NTruePrompter {

template<typename T, typename V>
using TUnaryCost = std::function<V(const T*)>;

template<typename T, typename V>
using TBinaryCost = std::function<V(const T*, const T*)>;

template<typename T, typename V>
V EditDistance(
    const tcb::span<T>& source,
    const tcb::span<T>& target,
    const TUnaryCost<T, V>& insertCost,
    const TUnaryCost<T, V>& deleteCost,
    const TBinaryCost<T, V>& replaceCost)
{
    if (source.size() > target.size()) {
        return EditDistance(target, source, deleteCost, insertCost, replaceCost);
    }

    std::vector<V> dist(source.size() + 1);
    for (size_t i = 1; i <= source.size(); ++i) {
        dist[i] = dist[i - 1] + deleteCost(source.data() + i - 1);
    }

    for (size_t j = 1; j <= target.size(); ++j) {
        V prevDiag = dist[0];
        V prevDiagSave;

        dist[0] += insertCost(target.data() + j - 1);

        for (size_t i = 1; i <= source.size(); ++i) {
            prevDiagSave = dist[i];
            if (source[i - 1] == target[j - 1]) {
                dist[i] = prevDiag;
            } else {
                dist[i] = std::min(std::min(dist[i - 1] + deleteCost(source.data() + i - 1), dist[i] + insertCost(target.data() + j - 1)), prevDiag + replaceCost(source.data() + i - 1, target.data() + i - 1));
            }
            prevDiag = prevDiagSave;
        }
    }

    return dist[source.size()];
}

template<typename T, typename V>
std::tuple<tcb::span<T>, tcb::span<T>, V> SmithWaterman(
    const tcb::span<T>& source,
    const tcb::span<T>& target,
    const TBinaryCost<T, V>& sourceSkip,
    const TBinaryCost<T, V>& targetSkip,
    const TBinaryCost<T, V>& similarityScore)
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
        tcb::span<T>(source.begin() + prev.first, source.begin() + maxElem.first + 1),
        tcb::span<T>(target.begin() + prev.second, target.begin() + maxElem.second + 1),
        score[maxElem.first][maxElem.second],
    };
}

}
