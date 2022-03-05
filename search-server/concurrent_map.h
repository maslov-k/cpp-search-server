#pragma once
#include <future>
#include <mutex>
#include <map>
#include <vector>

template <typename Key, typename Value>
class ConcurrentMap
{
private:
    struct Bucket
    {
        std::mutex bucket_mutex;
        std::map<Key, Value> bucket_map;
    };

    std::vector<Bucket> buckets_;

public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");

    struct Access
    {
        std::lock_guard<std::mutex> guard;
        Value& ref_to_value;

        Access(const Key& key, Bucket& bucket)
            : guard(bucket.bucket_mutex), ref_to_value(bucket.bucket_map[key])
        {
        }
    };

    explicit ConcurrentMap(size_t bucket_count)
        : buckets_(bucket_count)
    {
    }

    Access operator[](const Key& key)
    {
        int index = static_cast<uint64_t>(key) % buckets_.size();
        return Access{ key, buckets_[index] };
    }

    std::map<Key, Value> BuildOrdinaryMap()
    {
        std::map<Key, Value> result;
        for (auto& [m, bucket] : buckets_)
        {
            std::lock_guard guard(m);
            result.insert(bucket.begin(), bucket.end());
        }
        return result;
    }
};