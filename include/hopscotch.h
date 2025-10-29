#pragma once
#include <vector>
#include <optional>
#include <functional>
#include <utility>
#include <cstring>
#include <iostream>
#include <bitset>

#define FNV_offset32 ((uint32_t) 2166136261U)
#define FNV_offset64 ((uint64_t) 14695981039346656037ULL)
#define FNV_prime32  ((uint32_t) 16777619U)
#define FNV_prime64  ((uint64_t) 1099511628211ULL)

using std::cout;
using std::endl;

template <typename Key, typename Value>
class HopscotchMap
{
private:
    /* Constant variables */
    static constexpr float LOAD_FACTOR_LIMIT = 0.9;
    static constexpr size_t DEFAULT_NEIGHBORHOOD_LENGTH = 16;
    static constexpr size_t DEFAULT_TABLE_SIZE = 1024;

    /* Bucket structure*/
    struct Bucket
    {
        std::optional<std::pair<Key, Value>> data;
        uint64_t neighborhood = 0;
    };
    size_t table_capacity;
    size_t neighborhood_size;
    std::vector<Bucket> table;
    size_t current_table_size = 0;

    /* Hash Functions */
    uint32_t FNV1a_32(const int32_t key){
        uint32_t hash = FNV_offset32;
        for(size_t i = 0; i < 4; i++){
            uint8_t byte = (key >> (i*8)) & 0xFF;
            hash ^= byte;
            hash *= FNV_prime32;
        }
        return hash;
    }
    uint64_t FNV1a_64(const int64_t key){
        uint64_t hash = FNV_offset64;
        for(size_t i = 0; i < 8; i++){
            uint8_t byte = (key >> (i*8)) & 0xFF;
            hash ^= byte;
            hash *= FNV_prime64;
        }
        return hash;
    }
    uint64_t FNV1a_str(const std::string& key){
        uint64_t hash = FNV_offset64;
        for(char byte: key){
            hash^=static_cast<uint8_t>(byte);
            hash*=FNV_prime64;
        }
        return hash;
    }
    size_t HashFunction(Key key) {
        if constexpr (std::is_same_v<Key, int32_t>) {
            return FNV1a_32(key) & (table_capacity-1);
        } else if constexpr (std::is_same_v<Key, int64_t>) {
            return FNV1a_64(key) & (table_capacity-1);
        } else if constexpr (std::is_same_v<Key, double>) {
            return FNV1a_64(*reinterpret_cast<int64_t*>(&key)) & (table_capacity-1);
        }else if constexpr(std::is_same_v<Key,std::string>){
            return FNV1a_str(key) & (table_capacity-1);
        }
    }

    /* Rehash */
    void rehash()
    {

        /* Save old table */
        std::vector<Bucket> old_table = std::move(table);

        /* Double the capacity */
        table_capacity *= 2;
        table.clear();
        table.resize(table_capacity);
        current_table_size = 0;

        /* Re-insert all elements */
        for (auto &bucket : old_table)
        {
            if (bucket.data.has_value())
            {
                const auto &[key, value] = bucket.data.value();
                emplace(key, value);
            }
        }
    }

    /* Helper Functions */
    constexpr size_t next_power_of_2(size_t n)
    {
        if (n == 0)
            return 1;
        --n;
        n |= n >> 1;
        n |= n >> 2;
        n |= n >> 4;
        n |= n >> 8;
        n |= n >> 16;
        if constexpr (sizeof(size_t) == 8) // 64-bit
            n |= n >> 32;
        return n + 1;
    }

    float current_load_factor()
    {
        return static_cast<float>(current_table_size) / table_capacity;
    }

    void print_bits(const char* message, size_t index)
    {
        std::cout << message << std::bitset<64>(table[index].neighborhood)<< "\n";
    }

    /* Neighborhood functions */

    size_t set_neighborhood_size(size_t table_size, size_t given_size)
    {
        size_t actual_size = given_size;
        if (given_size > 64)
            actual_size = 64;
        if (given_size > table_size)
            actual_size = (table_size > 64) ? 64 : table_size;
        return actual_size;
    }

    bool is_neighborhood_full(uint64_t neighborhood, size_t neighborhood_size)
    {
        if (neighborhood_size == 0)
            return false;

        uint64_t mask;
        if (neighborhood_size == 64)
            mask = ~0ULL;
        else
            mask = (1ULL << neighborhood_size) - 1;

        /* Extract only the bits needed */
        uint64_t top_bits = neighborhood >> (64 - neighborhood_size);

        /* If they are all 1,  */
        return (top_bits & mask) == mask;
    }

public:
    /* Iterator */
    struct Iterator
    {
        using BucketVec = std::vector<Bucket>;

        Iterator(BucketVec *table_ptr, size_t index)
            : table(table_ptr), idx(index)
        {
            /* Go to the first occupied bucket */
            advance_to_valid();
        }

        std::pair<Key, Value> &operator*() const
        {
            return table->at(idx).data.value();
        }

        std::pair<Key, Value> *operator->() const
        {
            return &table->at(idx).data.value();
        }

        Iterator &operator++()
        {
            ++idx;
            advance_to_valid();
            return *this;
        }

        bool operator!=(const Iterator &other) const
        {
            return idx != other.idx || table != other.table;
        }

        bool operator==(const Iterator &other) const
        {
            return idx == other.idx && table == other.table;
        }

    private:
        BucketVec *table;
        size_t idx;

        void advance_to_valid()
        {
            while (idx < table->size() && !(*table)[idx].data.has_value())
            {
                ++idx;
            }
        }
    };
    /* Constructor */
    /* Size + Neighborhood Length */
    HopscotchMap(size_t table_size, size_t neighborhood_length)
        : table_capacity(next_power_of_2(static_cast<size_t>(table_size / LOAD_FACTOR_LIMIT)+1)), neighborhood_size(set_neighborhood_size(table_capacity, neighborhood_length)), table(table_capacity) {}
    /* Size */
    HopscotchMap(size_t table_size)
        : HopscotchMap(table_size, DEFAULT_NEIGHBORHOOD_LENGTH) {}
    /* No parameters */
    HopscotchMap()
        : HopscotchMap(DEFAULT_TABLE_SIZE, DEFAULT_NEIGHBORHOOD_LENGTH) {}
    /* Emplace */
    std::pair<Iterator, bool> emplace(const Key &key, const Value &value)
    {

        /* Check for the load factor */
        if (current_load_factor() >= LOAD_FACTOR_LIMIT)
        {
            cout << "Load factor reached, rehashing!" << endl;
            rehash();
            return emplace(key, value);
        }

        /* Check if table capacity is a power of 2 (for the & (capacity -1 ) to work) */
        if ((table_capacity & (table_capacity - 1)) != 0)
        {
            return {end(), false};
        }

        /* Hash the key and find the index */
        //size_t hash_value = std::hash<Key>{}(key);
        //size_t index = hash_value & (table_capacity - 1);
        size_t index = HashFunction(key);

        /* Check if neighborhood is full */
        if (is_neighborhood_full(table[index].neighborhood, neighborhood_size))
        {
            cout << "Neighborhood full, rehashing!" << endl;
            rehash();
            return emplace(key, value);
        }

        /* Find first free slot (linear probe) */
        size_t free_idx = index;
        while (table[free_idx].data.has_value())
        {
            free_idx = (free_idx + 1) & (table_capacity - 1);
        }

        /* Hopscotch Step */
        while (((free_idx - index) & (table_capacity - 1))>= neighborhood_size)
        {
            bool found = false;
            for (size_t y_offset = neighborhood_size - 1; y_offset > 0; --y_offset)
            {
                size_t y_idx = (free_idx - y_offset) & (table_capacity - 1);
                auto &y_bucket = table[y_idx];

                if (!y_bucket.data.has_value()) continue;

                //size_t k_hash = std::hash<Key>{}(y_bucket.data->first);
                //size_t k_idx = k_hash & (table_capacity - 1);
                size_t k_idx = HashFunction(y_bucket.data->first);

                if (((free_idx - k_idx) & (table_capacity - 1)) < neighborhood_size)
                {
                    found = true;
                    table[free_idx].data = std::move(table[y_idx].data);
                    table[y_idx].data.reset();
                    /* Remove old position of y */
                    size_t k_y_dist   = (y_idx - k_idx) & (table_capacity - 1);
                    table[k_idx].neighborhood &= ~(1ULL << (64 - 1 - k_y_dist));
                    /* Add new position of y*/
                    size_t k_free_dist = (free_idx - k_idx) & (table_capacity - 1);
                    table[k_idx].neighborhood |= (1ULL << (64 - 1 - k_free_dist));
                    free_idx = y_idx;
                    break;
                }
            }

            /* Could not find such a spot, table is full */
            if (!found)
            {
                cout << "Could not find spot, rehashing!" << endl;
                rehash();
                return emplace(key, value);
            }
        }

        /* Finally insert element at the new free spot*/
        table[free_idx].data = std::make_pair(key, value);
        /* Update the neighborhood bitmap */
        size_t distance = (free_idx - index) & (table_capacity - 1);
        table[index].neighborhood |= (1ULL << (64 - 1 - distance));

        ++current_table_size;

        return {Iterator(&table, free_idx), true};
    }

    /* Find */
    Iterator find(const Key &key)
    {
        if (table.empty())
            return end();

        //size_t hash_value = std::hash<Key>{}(key);
        //size_t index = hash_value & (table_capacity - 1);
        size_t index = HashFunction(key);

        /* Scan all neighborhood bits (big-endian order) */
        for (size_t offset = 0; offset < neighborhood_size; ++offset)
        {
            /* Efficiently check if any value of the neighborhood bitmap is 1*/
            if (table[index].neighborhood & (1ULL << (64 - 1 - offset)))
            {
                size_t idx = (index + offset) & (table_capacity - 1);
                auto &bucket = table[idx];
                if (bucket.data.has_value() && bucket.data->first == key)
                    return Iterator(&table, idx);
            }
        }

        return end();
    }

    /* End */
    Iterator end() { return Iterator(&table, table.size()); }
};
