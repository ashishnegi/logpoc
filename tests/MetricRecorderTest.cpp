#pragma once

#include "pch.h"
#include "MetricRecorder.h"

TEST(metric_recorder, can_add_get_values) {
    int n = 100;
    for (int i = 0; i < n; ++i) {
        int size = (std::rand() % 4096) + 1;
        MetricRecorder m(size);
        for (int i = 0; i < size; ++i) {
            m.Add(i);
        }

        try
        {
            m.Add(10); // should throw
            EXPECT_TRUE(false) << std::string("Failed to work with size: " + size);
        }
        catch (std::runtime_error &) {
        }

        auto it = m.GetIterator();
        int expected = 0;
        while (it.HasNext()) {
            EXPECT_EQ(expected, it.GetNext()) << std::string("Failed to work with size: " + size);
            expected += 1;
        }
    }
}