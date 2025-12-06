// unchained_hashtable.cpp
#include "unchained_hashtable.h"
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <immintrin.h> // for _mm_crc32_u32, _mm_crc32_u64
#include <algorithm>
#include <random>
#include <bitset>


/* Constructor */
unchained_ht::unchained_ht()
{
    /* Initialize the tuple count */
    tuple_count = 0;

    /* Set up the precomputed tags */
    init_tags();
}


/* Initialize the precomputed tag matrix */
void unchained_ht::init_tags()
{   
    /* Initialize the pattern vector */
    std::vector<uint16_t> patterns;
    patterns.reserve(1820);
    
    /* Set the masks */
    for (int a = 0; a < 16; a++)
        for (int b = a + 1; b < 16; b++)
            for (int c = b + 1; c < 16; c++)
                for (int d = c + 1; d < 16; d++)
                    patterns.push_back((1u << a) | (1u << b) | (1u << c) | (1u << d));


    /* Suffle the patterns */
    std::mt19937 rng(2200058);
    std::shuffle(patterns.begin(), patterns.end(), rng);

    /* Fill the tags, with the padding to 2048 */
    for (int i = 0; i < 2048; i++)
        tags[i] = patterns[i % patterns.size()];
}

/* CRC32 hash function */
uint64_t unchained_ht::hash_key(int32_t key) const
{
    uint32_t crc = _mm_crc32_u32(2200058, uint32_t(key));

    uint64_t mix = 0x8648DBDBULL;
    return (uint64_t)crc * ((mix << 32) + 1);
}

/* Insert the tuples and store them in temporary array */
void unchained_ht::build_insert(int32_t key, size_t value)
{   
    /* Cannot insert after building */
    if (isBuilt)
        throw std::runtime_error("Hash table already finalized.");

    /* Store the tuple correctly */
    Tuple t;
    t.key = key;
    t.value = value;
    t.hash = hash_key(key);

    /* Put it in the buffer */
    build_buffer.push_back(t);
}

/* Finalize the build, to get ready to probe */
void unchained_ht::finalize_build()
{
    /* If the table is already built, ignore */
    if (isBuilt)
        return;

    /* Check the number of tuples,  */
    tuple_count = build_buffer.size();
    if (tuple_count == 0)
    {
        isBuilt = true;
        return;
    }


    /* Allocate the directory */
    directory_size = next_power_of_2(static_cast<size_t>(tuple_count / load_factor) + 1);

    /* Calculate the shift */
    shift = 64 - __builtin_ctzll(directory_size);

    /* Set a directory with one extra slot, so that we get the starting bucket to be [dir[-1], dir[0]] */
    directory_raw = new uint64_t[directory_size + 1];
    std::memset(directory_raw, 0, sizeof(uint64_t) * (directory_size + 1));

    /* Set the new directory one element ahead, to have access to the dir[-1] */
    directory = directory_raw + 1;
    directory[-1] = 0;

    // Allocate final contiguous storage
    array = new Tuple[tuple_count];

    // 1) Count tuples per slot and build Bloom filters
    for (const Tuple &t : build_buffer)
    {
        uint64_t slot = t.hash >> shift;

        uint64_t count = (directory[slot] >> 16) + 1; // increment tuple count
        uint16_t bloom = static_cast<uint16_t>(directory[slot]) | tags[(uint32_t)t.hash >> (32 - 11)];

        directory[slot] = (count << 16) | bloom;
    }

    // 2) Exclusive prefix sum over counts to get starting indices
    size_t running = 0;
    for (size_t i = 0; i < directory_size; i++)
    {
        uint64_t count = directory[i] >> 16;
        uint16_t bloom = static_cast<uint16_t>(directory[i]);

        directory[i] = (running << 16) | bloom; // store start index
        running += count;
    }

    // Start index for slot 0 is 0
    directory[-1] = 0;

    // 3) Scatter tuples into their final positions, updating ends
    for (const Tuple &t : build_buffer)
    {
        uint64_t slot = t.hash >> shift;

        uint64_t pos = directory[slot] >> 16; // current write position

        array[pos] = t;

        uint16_t bloom = static_cast<uint16_t>(directory[slot]);
        directory[slot] = ((pos + 1) << 16) | bloom; // advance end pointer
    }

    isBuilt = true;
}


std::vector<const unchained_ht::Tuple*>
unchained_ht::probe(int32_t key) const
{
    if (!isBuilt)
        throw std::runtime_error("Hash table not built.");

    std::vector<const Tuple*> result;

    if (tuple_count == 0)
        return result;

    uint64_t h = hash_key(key);
    uint64_t slot = h >> shift;

    uint64_t entry = directory[slot];
    uint16_t bloom = static_cast<uint16_t>(entry);

    /* Bloom filter check -- reject early if impossible match */
    if (!could_contain(bloom, h))
    {
        return result;
    }
    
    // Range for this hash-prefix slot
    uint64_t start_off = directory[slot - 1] >> 16;
    uint64_t end_off   = directory[slot]     >> 16;

    const Tuple* begin = array + start_off;
    const Tuple* end   = array + end_off;

    // Collect only matching tuples
    for (auto p = begin; p < end; ++p)
        if (p->key == key)
        {
            result.push_back(p);
        }
            

    return result;
}
