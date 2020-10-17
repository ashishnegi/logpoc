#pragma once

#include "MetricRecorder.h"
#include <unordered_map>
#include <set>

class Histogram {
public:
    Histogram(const std::vector<MetricRecorder> & recorders) : recorders(recorders) {}

    std::vector<int> GetPercentiles(std::vector<double> percentileAt) {
        int count = 0;
        std::unordered_map<int, int> latencyCount;
        std::set<int> keys;

        // Merge data for each recorder
        for (auto & recorder : recorders) {
            auto recorderIt = recorder.GetIterator();
            while (recorderIt.HasNext()) {
                auto val = recorderIt.GetNext();
                auto latencyIt = latencyCount.insert(std::make_pair(val, 1));
                if (!latencyIt.second) {
                    latencyIt.first->second += 1;
                }

                count += 1;

                keys.insert(val);
            }
        }

        std::vector<int> percentileCount;
        for (auto pAt : percentileAt) {
            if (pAt > 1.0) {
                throw std::runtime_error("Invalid input");
            }

            percentileCount.push_back(pAt * count);
        }

        int tmpCount = 0;
        int index = 0;

        std::vector<int> percentiles(percentileCount.size());
        int lastKey = INT_MIN;
        for (auto k : keys) {
            if (lastKey >= k) {
                assert(false); // should see keys in increasing order.
            }
            lastKey = k;

            tmpCount += latencyCount[k];

            for (int i = index; i < percentileCount.size(); ++i) {
                if (tmpCount >= percentileCount[i]) {
                    percentiles[i] = k;
                    index = i + 1;
                }
            }

            if (index >= percentileCount.size()) {
                break;
            }
        }

        if (index != percentiles.size()) {
            assert(false); // should be able to calculate all percentiles.
        }

        return percentiles;
    }

private:
    const std::vector<MetricRecorder> & recorders;
};