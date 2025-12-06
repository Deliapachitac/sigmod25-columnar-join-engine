// unchained_hashtable.cpp
#include "unchained_hashtable.h"
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <immintrin.h> // for _mm_crc32_u32, _mm_crc32_u64
#include <algorithm>
#include <random>
#include <bitset>

/* -------------------------------------------------------------
   Initialization of the tags used in Bloom filters,
   randomly and uniformly generated.
--------------------------------------------------------------*/
void unchained_ht::init_tags() {
    std::vector<uint16_t> patterns;
    patterns.reserve(1820);

    for (int a = 0; a < 16; a++)
    for (int b = a+1; b < 16; b++)
    for (int c = b+1; c < 16; c++)
    for (int d = c+1; d < 16; d++)
        patterns.push_back((1u << a) | (1u << b) | (1u << c) | (1u << d));

    // Shuffle with a fixed seed for determinism
    std::mt19937 rng(123456);
    std::shuffle(patterns.begin(), patterns.end(), rng);

    // Fill the tags table (pad to 2048)
    for (int i = 0; i < 2048; i++)
        tags[i] = patterns[i % patterns.size()];
}

/* -------------------------------------------------------------
   Hash function
   Follows the paper (crc32 + mixing constant)
--------------------------------------------------------------*/
uint64_t unchained_ht::hash_key(int32_t key) const
{
    uint32_t crc = _mm_crc32_u32(seed1, uint32_t(key));

    // multiply by 64-bit mixing constant (same pattern as paper)
    uint64_t mix = 0x8648DBDBULL;
    return (uint64_t)crc * ((mix << 32) + 1);
}

/* -------------------------------------------------------------
   Insert into temporary build buffer
--------------------------------------------------------------*/
void unchained_ht::build_insert(int32_t key, size_t value)
{
    if (isBuilt)
        throw std::runtime_error("Hash table already finalized.");

    Tuple t;
    t.key   = key;
    t.value = value;
    t.hash  = hash_key(key);

    build_buffer.push_back(t);
}

/* -------------------------------------------------------------
   Finalize build: pack tuples into contiguous array ordered
   by hash prefix; build directory.
--------------------------------------------------------------*/
void unchained_ht::finalize_build()
{
    if (isBuilt) return;

    tuple_count = build_buffer.size();
    if (tuple_count == 0) {
        isBuilt = true;
        return;
    }

    array = new Tuple[tuple_count];

    // ----------------------------------------------------
    // STEP 1: Count tuples & OR bloom filters (bottom 16 bits)
    // ----------------------------------------------------
    for (const Tuple& t : build_buffer) {
        uint64_t slot = t.hash >> shift;         // 0 .. directory_size-1

        uint64_t count = (directory[slot] >> 16) + 1;   // increment count
        uint16_t bloom = uint16_t(directory[slot]) | compute_tag(t.hash);

        directory[slot] = (count << 16) | bloom;
    }

    // ----------------------------------------------------
    // STEP 2: Prefix sum on counts → convert to start offsets
    // After this: directory[i] = (start_offset << 16) | bloom
    // ----------------------------------------------------
    size_t running = 0;

    for (size_t i = 0; i < directory_size; i++) {
        uint64_t count = directory[i] >> 16;
        uint16_t bloom = uint16_t(directory[i]);

        directory[i] = (running << 16) | bloom;   // store start offset
        running += count;
    }

    // directory[-1] points to start of tuple storage (index 0)
    directory[-1] = 0;

    // Sentinel entry: holds one past the end (no bloom bits)
    directory[directory_size] = (running << 16);

    // ----------------------------------------------------
    // STEP 3: Copy tuples AND increment directory offsets
    // This converts starts → ends
    //
    // After this loop:
    //   directory[slot-1] >> 16 = start index
    //   directory[slot]   >> 16 = end   index
    // ----------------------------------------------------
    for (const Tuple& t : build_buffer) {
        uint64_t slot = t.hash >> shift;

        // directory[slot] currently holds the *next free position* (tuple index)
        uint64_t pos = directory[slot] >> 16;

        array[pos] = t;

        // increment end pointer (keeping bloom untouched)
        uint16_t bloom = uint16_t(directory[slot]);
        directory[slot] = ((pos + 1) << 16) | bloom;
    }

    isBuilt = true;
}

/* -------------------------------------------------------------
   Lookup: return pointer range [start, end)
--------------------------------------------------------------*/
std::pair<const unchained_ht::Tuple*, const unchained_ht::Tuple*>
unchained_ht::lookup(int32_t key) const
{
    if (!isBuilt)
        throw std::runtime_error("Hash table not built.");

    if (tuple_count == 0)
        return {nullptr, nullptr};

    uint64_t h    = hash_key(key);
    uint64_t slot = h >> shift;  // 0 .. directory_size-1

    uint64_t entry = directory[slot];
    uint16_t bloom = uint16_t(entry);

    if (!could_contain(bloom, h))
        return {nullptr, nullptr};

    uint64_t start_off = directory[slot - 1] >> 16;
    uint64_t end_off   = directory[slot]     >> 16;

    const Tuple* begin = array + start_off;
    const Tuple* end   = array + end_off;

    // find first
    const Tuple* first = nullptr;
    for (auto p = begin; p < end; ++p)
        if (p->hash == h && p->key == key) { first = p; break; }

    if (!first)
        return {nullptr, nullptr};

    // find last (one past last match)
    const Tuple* last = first + 1;
    for (auto p = last; p < end; ++p)
        if (p->hash == h && p->key == key) last = p + 1;

    return {first, last};
}
