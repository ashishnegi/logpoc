#pragma once

#include <vector>

/**
* Fixed size data list which partitions data so that we don't have to allocate large continuous memory.
* We don't want to allocate memory during Adding data to not cause any latency spike because of this data structure.
* We are not using std::map/unordered_map as they are slow.
*/
class MetricRecorder {
public:
    MetricRecorder(int size) : MetricRecorder(size, 1024) {
    }

    MetricRecorder(int size, int bucketSize) : size(size) {
        if (size <= 0) {
            throw std::runtime_error("invalid size");
        }

        int numBuckets = ((size - 1) / bucketSize) + 1;
        buckets = std::vector<std::vector<int>>(numBuckets);

        for (auto & b : buckets) {
            b = std::vector<int>(bucketSize, 0);
        }

        curBucketIndex = 0;
        curSlotIndex = 0;
        lastBucketSize = size % bucketSize;
        if (lastBucketSize == 0) {
            lastBucketSize = bucketSize;
        }
    }

    void Add(int data) {
        CheckSpace(false);
        buckets[curBucketIndex][curSlotIndex] = data;
        IncrementIndex();
    }

    class MetricRecorderIterator;

    MetricRecorderIterator GetIterator() const {
        if (CheckSpace(true)) { // should not have any space left - writes should finish.
            throw std::runtime_error("Iterator can only be called after filling all slots.");
        }
        return MetricRecorderIterator(*this);
    }

    class MetricRecorderIterator {
    public:
        MetricRecorderIterator(const MetricRecorder & list) : list(list), index(0) { }
        int GetNext() {
            return list.Get(index++);
        }

        bool HasNext() {
            return index < list.size;
        }
    private:
        int index;
        const MetricRecorder & list;
    };

private:
    bool CheckSpace(bool noThrow) const {
        if (curBucketIndex < (buckets.size() - 1)) {
            return true;
        }

        if (curSlotIndex >= lastBucketSize) {
            if (noThrow) {
                return false;
            }
            else {
                throw std::runtime_error("Out of index access");
            }
        }

        return true;
    }

    void IncrementIndex() {
        curSlotIndex = (curSlotIndex + 1) % buckets[0].size();
        if (curSlotIndex == 0) {
            curBucketIndex += 1;
        }
    }

    int Get(int index) const {
        size_t bucketIndex = index / buckets[0].size();
        size_t slotIndex = index % buckets[0].size();
        return buckets[bucketIndex][slotIndex];
    }

    std::vector<std::vector<int>> buckets;
    int curBucketIndex;
    int curSlotIndex;
    int size;
    int lastBucketSize;
};