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
    static constexpr float LOAD_FACTOR_LIMIT = 0.9;
    static constexpr size_t DEFAULT_NEIGHBORHOOD_LENGTH = 32;
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
    size_t HashFunction(const Key &key) const
    {
        if constexpr (std::is_integral_v<Key>)
        {
            /* simple multiplicative mix, works faster than FNV-1a for ints */
            uint64_t x = static_cast<uint64_t>(key);
            x = (x ^ (x >> 33)) * 0xff51afd7ed558ccdULL;
            x = (x ^ (x >> 33)) * 0xc4ceb9fe1a85ec53ULL;
            x = x ^ (x >> 33);
            return x & (table_capacity - 1);
        }
        else if constexpr (std::is_floating_point_v<Key>)
        {
            uint64_t raw;
            static_assert(sizeof(raw) == sizeof(key));
            std::memcpy(&raw, &key, sizeof(key));
            raw ^= raw >> 33;
            raw *= 0xff51afd7ed558ccdULL;
            raw ^= raw >> 33;
            raw *= 0xc4ceb9fe1a85ec53ULL;
            raw ^= raw >> 33;
            return raw & (table_capacity - 1);
        }
        else if constexpr (std::is_same_v<Key, std::string>)
        {
            /* FNV-1a 64-bit */
            uint64_t hash = 14695981039346656037ULL;
            for (unsigned char c : key)
            {
                hash ^= c;
                hash *= 1099511628211ULL;
            }
            return hash & (table_capacity - 1);
        }
        else
        {
            /* Fallback to std::hash for any other type */
            return std::hash<Key>{}(key) & (table_capacity - 1);
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

    void print_bits(const char *message, size_t index)
    {
        std::cout << message << std::bitset<64>(table[index].neighborhood) << "\n";
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
        : table_capacity(next_power_of_2(static_cast<size_t>(table_size / LOAD_FACTOR_LIMIT) + 1)), neighborhood_size(set_neighborhood_size(table_capacity, neighborhood_length)), table(table_capacity) {}
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
        const size_t table_mask = table_capacity - 1;
        if ((table_capacity & table_mask) != 0)
        {
            return {end(), false};
        }

        /* Hash the key and find the index */
        // size_t hash_value = std::hash<Key>{}(key);
        // size_t index = hash_value & table_mask;
        size_t index = HashFunction(key);

        /* Check if neighborhood is full */
        if (is_neighborhood_full(table[index].neighborhood, neighborhood_size))
        {
            rehash();
            return emplace(key, value);
        }

        /* Find first free slot (linear probe) */
        size_t free_idx = index;
        while (table[free_idx].data.has_value())
        {
            free_idx = (free_idx + 1) & table_mask;
        }

        /* Hopscotch Step */
        bool found = false;
        size_t y_idx, k_idx, k_y_dist, k_free_dist;
        while (((free_idx - index) & table_mask) >= neighborhood_size)
        {
            found = false;
            for (size_t y_offset = neighborhood_size - 1; y_offset > 0; --y_offset)
            {
                y_idx = (free_idx - y_offset) & table_mask;
                auto &y_bucket = table[y_idx];

                if (!y_bucket.data.has_value())
                    continue;

                // size_t k_hash = std::hash<Key>{}(y_bucket.data->first);
                // size_t k_idx = k_hash & table_mask;
                k_idx = HashFunction(y_bucket.data->first);

                if (((free_idx - k_idx) & table_mask) < neighborhood_size)
                {
                    found = true;
                    table[free_idx].data = std::move(table[y_idx].data);
                    table[y_idx].data.reset();
                    /* Remove old position of y */
                    k_y_dist = (y_idx - k_idx) & table_mask;
                    table[k_idx].neighborhood &= ~(1ULL << (64 - 1 - k_y_dist));
                    /* Add new position of y*/
                    k_free_dist = (free_idx - k_idx) & table_mask;
                    table[k_idx].neighborhood |= (1ULL << (64 - 1 - k_free_dist));
                    /* Update the free index to the new free spot */
                    free_idx = y_idx;
                    break;
                }
            }

            /* Could not find such a spot, table is full */
            if (!found)
            {
                rehash();
                return emplace(key, value);
            }
        }

        /* Finally insert element at the new free spot*/
        table[free_idx].data = std::make_pair(key, value);
        /* Update the neighborhood bitmap */
        size_t distance = (free_idx - index) & table_mask;
        table[index].neighborhood |= (1ULL << (64 - 1 - distance));

        ++current_table_size;

        return {Iterator(&table, free_idx), true};
    }

    /* Find */
    Iterator find(const Key &key)
    {
        if (table.empty())
            return end();

        /* Get the index of the key */
        const size_t mask = table_capacity - 1;
        size_t index = HashFunction(key);

        /* move the neighborhood bits to the right side, putting the redundant zeros at the beginning, if neighborhood is smaller*/
        uint64_t top = table[index].neighborhood >> (64 - neighborhood_size);
        while (top)
        {
            /* Get the first set bit of the neighborhood (has a hashed element) */
            int highest_bit = 63 - __builtin_clzll(top); 
            /* Find the correct index */
            size_t offset = (neighborhood_size - 1) - highest_bit;
            size_t idx = (index + offset) & mask;
            auto &bucket = table[idx];
            if (bucket.data.has_value() && bucket.data->first == key)
                return Iterator(&table, idx);
            /* If we did not find it, clear it and continue */
            top &= ~(1ULL << highest_bit);
        }
        return end();
    }

    /* Begin */
    Iterator begin() { return Iterator(&table, 0); }

    /* End */
    Iterator end() { return Iterator(&table, table.size()); }
};