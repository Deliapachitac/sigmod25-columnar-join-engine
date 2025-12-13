// unchained_hashtable.h
#ifndef UNCHAINED_HASHTABLE_H
#define UNCHAINED_HASHTABLE_H

#include <cstdint>
#include <vector>
#include <iostream>
#include <cstring> /* for memset */

class unchained_ht
{
private:
    /* Tuple Layout, for the contiguous array */
    struct Tuple
    {
        int32_t  key;
        uint64_t hash;  /* Full hash to save time on rehashing */
        size_t   value;
    };

    /* Build-state flag */
    bool isBuilt = false;

    /* Load Factor */
    float load_factor = 0.65;

    /* Directory size (power of 2) */
    size_t directory_size;

    /* How many bits to shift hash >> shift to get slot index */
    uint64_t shift;

    /* Temporary buffer (before finalization) */
    std::vector<Tuple> build_buffer;

    /* Final contiguous tuple storage */
    Tuple *array = nullptr;
    size_t tuple_count = 0;

    /* Directory storage */
    uint64_t *directory_raw = nullptr;
    uint64_t *directory     = nullptr;

    /* Precomputed tags used in Bloom filters (rounded to 2048) */
    uint16_t tags[2048];

    /* --------- Helper: round size to next power of 2 --------- */
    static constexpr size_t next_power_of_2(size_t n)
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

    /* Quick filter check (ANDN logic from paper) */
    inline bool could_contain(uint16_t filter, uint64_t hash) const
    {
        return !(tags[(uint32_t)hash >> (32 - 11)] & ~filter);
    }

    /* Hash function (crc32) */
    uint64_t hash_key(int32_t key) const;

    /* Initialize the tags */
    void init_tags();

public:
    /* Constructor for size */
    unchained_ht();

    /* Insert tuple during build (store in temporary buffer) */
    void build_insert(int32_t key, size_t value);

    /* Finalize: build directory + pack tuples into contiguous array */
    void finalize_build();

    /* Probe: return vector of matching values for a key */
    std::vector<size_t> probe(int32_t key) const;

    /* Test functions for the hashtable */
    size_t get_directory_size() const { return directory_size; }
    size_t get_tuple_count()   const { return tuple_count;    }
    bool built() const { return isBuilt; }
    size_t get_build_buffer_size() const { return build_buffer.size(); }
};

#endif // UNCHAINED_HASHTABLE_H
