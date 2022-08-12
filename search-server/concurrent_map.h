#pragma once

#include <algorithm>
#include <cstdlib>
#include <future>
#include <map>
#include <numeric>
#include <random>
#include <string>
#include <vector>
#include <mutex>
#include <iterator>

using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap {
    // подсловари, для каждого подсловаря свой mutex
    struct SubMap {
        std::mutex mtx;
        std::map<Key, Value> concurrent_map;
    };
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);

    struct Access {
        Access(SubMap& sum_map, const Key key)
            : guard(sum_map.mtx)
            , ref_to_value(sum_map.concurrent_map[key]) {
        }

        void operator+=(const Value& other) {
            ref_to_value += other;
        }

        std::lock_guard<std::mutex> guard;
        Value& ref_to_value;
    };

    explicit ConcurrentMap(size_t bucket_count)
        : sub_maps_(bucket_count) {
    }

    Access operator[](const Key& key) {
        SubMap& one_sub_map = sub_maps_[static_cast<uint64_t>(key) % sub_maps_.size()];
        return { one_sub_map, key };
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result;
        for (auto& [mtx, concurrent_map] : sub_maps_) {
            std::lock_guard guard(mtx);
            result.insert(concurrent_map.begin(), concurrent_map.end());
        }
        return result;
    }

    void Erase(const Key& key) {
        sub_maps_[static_cast<uint64_t>(key) % sub_maps_.size()].concurrent_map.erase(key);
    }

private:
    std::vector<SubMap> sub_maps_;
};