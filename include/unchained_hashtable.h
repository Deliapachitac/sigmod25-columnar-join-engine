// unchained_hashtable.h
#ifndef UNCHAINED_HASHTABLE_H
#define UNCHAINED_HASHTABLE_H

#include <cstdint>
#include <vector>
#include <iostream>
#include <cstring> // for memset

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

    /* Directory size (power of 2) */
    size_t directory_size;

    /* How many bits to shift hash >> shift to get slot index */
    uint64_t shift;

    /* Temporary buffer (before finalization) */
    std::vector<Tuple> build_buffer;

    /* Final contiguous tuple storage */
    Tuple *array = nullptr;
    size_t tuple_count = 0;

    /*
     * Directory:
     *   upper 48 bits = index into array (tuple index, not bytes)
     *   lower 16 bits = Bloom filter
     *
     * We allocate directory_raw[directory_size + 2] and let
     * directory = directory_raw + 1 so that directory[-1] is valid.
     */
    uint64_t *directory_raw = nullptr;
    uint64_t *directory     = nullptr;

    /* Precomputed tags used in Bloom filters (rounded to 2048) */
    uint16_t tags[2048];

    /* Hash seeds (crc32-based hashing needs two 32-bit seeds) */
    uint32_t seed1 = 2200058;
    uint32_t seed2 = 89101112;

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

    /* Bloom filter tag computation */
    inline uint16_t compute_tag(uint64_t hash) const
    {
        return tags[(uint32_t)hash >> (32 - 11)];
    }

    /* Quick filter check (ANDN logic from paper) */
    inline bool could_contain(uint16_t filter, uint64_t hash) const
    {
        uint16_t tag = compute_tag(hash);
        return !(tag & ~filter);
    }

    /* Hash function (crc32) */
    uint64_t hash_key(int32_t key) const;

    /* Initialize the tags */
    void init_tags();

public:
    /* Constructor for size */
    explicit unchained_ht(size_t size)
        : directory_size(next_power_of_2(size)),
          array(nullptr),
          tuple_count(0)
    {
        /* Set the minimum table size */
        if (directory_size < 2)
            directory_size = 2;

        /* shift = 64 - log2(directory_size) (use upper bits) */
        shift = 64 - __builtin_ctzll(directory_size);

        /* allocate directory_raw with two extra entries:
           - one before (for directory[-1])
           - one after (sentinel at directory[directory_size]) */
        directory_raw = new uint64_t[directory_size + 2];
        std::memset(directory_raw, 0, sizeof(uint64_t) * (directory_size + 2));

        /* shift logical directory pointer by one */
        directory = directory_raw + 1;

        /* directory[-1] is the sentinel for slot 0 start -> index 0 */
        directory[-1] = 0;

        /* initialize Bloom tags */
        init_tags();
    }

    ~unchained_ht()
    {
        delete[] array;
        delete[] directory_raw;
    }

    unchained_ht(const unchained_ht&)            = delete;
    unchained_ht& operator=(const unchained_ht&) = delete;
    unchained_ht(unchained_ht&&)                 = delete;
    unchained_ht& operator=(unchained_ht&&)      = delete;

    /* Insert tuple during build (store in temporary buffer) */
    void build_insert(int32_t key, size_t value);

    /* Finalize: build directory + pack tuples into contiguous array */
    void finalize_build();

    /* Probe: return range [start, end) of matching tuples */
    std::pair<const Tuple *, const Tuple *> lookup(int32_t key) const;

    /* Test functions for the hashtable */
    size_t get_directory_size() const { return directory_size; }
    size_t get_tuple_count()   const { return tuple_count;    }
};

#endif // UNCHAINED_HASHTABLE_H
