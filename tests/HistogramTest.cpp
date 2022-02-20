#pragma once

#include "pch.h"
#include "Histogram.h"

TEST(histogram, calculates_histogram) {
    int n = 100;
    auto seed = time(nullptr);
    std::cout << "Random seed: " << seed << std::endl;
    std::srand((unsigned int) seed);

    for (int i = 0; i < n; ++i) {
        int size = (std::rand() % 4096) + 1;
        int numMetrics = (std::rand() % 10) + 1;
        
        int minVal = INT_MAX, maxVal = 0;
        std::vector<MetricRecorder> recorders;
        for (int i = 0; i < numMetrics; ++i) {
            MetricRecorder m(size);
            for (int i = 0; i < size; ++i) {
                auto v = std::rand();
                minVal = std::min(v, minVal);
                maxVal = std::max(v, maxVal);
                m.Add(v);
            }
            recorders.push_back(m);
        }

        Histogram hgm(recorders);
        auto ps = hgm.GetPercentiles(std::vector<double> {0, .5, .9, .99, 1});
        EXPECT_EQ(minVal, ps.front());
        EXPECT_EQ(maxVal, ps.back());

        int lastP = minVal;
        for (auto p : ps) {
            if (p < lastP) {
                assert(false); // should have percentiles increasing
            }
        }
        // and no assertion happened in histogram getpercentiles.
    }
}

TEST(histogram, unit_test1) {
    int size = 1000;
    MetricRecorder m(size);
    for (int i = 0; i < size; ++i) {
        m.Add(i + 1);
    }

    // can't pass temporary obj to Histogram as that keeps a reference
    // and compiler can't see that.. Really need Rust
    auto recorders = std::vector<MetricRecorder>{ m };
    Histogram hgm(recorders);
    auto ps = hgm.GetPercentiles(std::vector<double> {0, .5, .9, .99, 1});
    EXPECT_EQ(1, ps[0]);
    EXPECT_EQ(500, ps[1]);
    EXPECT_EQ(900, ps[2]);
    EXPECT_EQ(990, ps[3]);
    EXPECT_EQ(1000, ps[4]);
}
