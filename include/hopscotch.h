#pragma once
#include <vector>
#include <optional>
#include <functional>
#include <utility>
#include <cstring>
#include <iostream>
#include <bitset>

using std::cout;
using std::endl;

template <typename Key, typename Value>
class HopscotchMap
{
private:
    /* Constant variables */
    static constexpr float LOAD_FACTOR_LIMIT = 0.95;
    static constexpr size_t DEFAULT_NEIGHBORHOOD_LENGTH = 8;
    static constexpr size_t DEFAULT_TABLE_SIZE = 32;

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

        // cout << "Rehashing, new capacity is: " << table_capacity << "\n";

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
            mask = ~0ULL; // all bits set
        else
            mask = (1ULL << neighborhood_size) - 1;

        // Extract only the top `neighborhood_size` bits
        uint64_t top_bits = neighborhood >> (64 - neighborhood_size);

        // Check if all bits are 1
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
        : table_capacity(next_power_of_2(table_size)), neighborhood_size(set_neighborhood_size(table_capacity, neighborhood_length)), table(table_capacity) {}
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
            rehash();
            return emplace(key, value);
        }

        /* Check if table capacity is a power of 2 (for the & (capacity -1 ) to work) */
        if ((table_capacity & (table_capacity - 1)) != 0)
        {
            return {end(), false};
        }

        /* Hash the key and find the index */
        size_t hash_value = std::hash<Key>{}(key);
        size_t index = hash_value & (table_capacity - 1);

        /* cout << "Hashed Index: " << index << endl;
        cout << "Table capacity: " << table_capacity << endl;
        cout << "Neighborhood Size: " << neighborhood_size << endl; */

        /* Check if key already exists in neighborhood */
        for (size_t offset = 0; offset < neighborhood_size; ++offset)
        {
            size_t idx = (index + offset) & (table_capacity - 1);
            auto &bucket = table[idx];
            if (bucket.data.has_value() && bucket.data->first == key)
            {
                bucket.data->second = value; /* update the value */
                return {Iterator(&table, idx), true};
            }
        }

        /* uint64_t top_bits = table[index].neighborhood >> (64 - neighborhood_size);
        std::bitset<64> bits(top_bits);
        std::cout << "Neighborhood Start: " << bits.to_string().substr(64 - neighborhood_size) << "\n\n"; */

        /* Since the key is not in the neighborhood, check if it is full */
        if (is_neighborhood_full(table[index].neighborhood, neighborhood_size))
        {
            rehash();
            return emplace(key, value);
        }

        /* Find first free slot (linear probe) */
        size_t free_idx = index;
        while (table[free_idx].data.has_value())
        {
            free_idx = (free_idx + 1) & (table_capacity - 1);
        }

        /* Insert the value in the correct index */
        table[free_idx].data = std::make_pair(key, value);

        /* Compute the distanse from the starting point */
        size_t distance = (free_idx + table_capacity - index) & (table_capacity - 1);
        if (distance < neighborhood_size)
        {
            /* Write the MSBs first, with max being 64 */
            table[index].neighborhood |= (1ULL << (64 - 1 - distance));
        }
        else
        {
            /* Fallback: simple rehash if insertion cannot be within neighborhood */
            rehash();
            return emplace(key, value);
        }

        /* uint64_t final_top_bits = table[index].neighborhood >> (64 - neighborhood_size);
        std::bitset<64> final_bits(final_top_bits);
        std::cout << "Neighborhood Final: " << final_bits.to_string().substr(64 - neighborhood_size) << "\n\n"; */

        ++current_table_size;

        return {Iterator(&table, free_idx), true};
    }

    /* Find */
    Iterator find(const Key &key)
    {
        if (table.empty())
            return end();

        size_t hash_value = std::hash<Key>{}(key);
        size_t index = hash_value & (table_capacity - 1);

        uint64_t neighborhood_mask = table[index].neighborhood;

        /* Scan all neighborhood bits (big-endian order) */
        for (size_t offset = 0; offset < neighborhood_size; ++offset)
        {
            /* For big-endian, bit 0 corresponds to the MSB (closest slot) */
            if (neighborhood_mask & (1ULL << (64 - 1 - offset)))
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
