// unchained_hashtable.cpp
#include "unchained_hashtable.h"
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <immintrin.h> /* for _mm_crc32_u32, _mm_crc32_u64 */
#include <algorithm>
#include <random>
#include <list>
#include <thread>
#include <bitset>

/* Constructor */
unchained_ht::unchained_ht(): directory_raw(nullptr), array(nullptr), directory(nullptr)
{
    /* Initialize the tuple count */
    tuple_count = 0;

    /* Initialize the number of threads and partitions */
    num_threads = std::thread::hardware_concurrency();
    num_partitions = std::thread::hardware_concurrency();
    if(num_threads == 0){
        num_threads = 8;
        num_partitions = 8;
    }
    previous_counts = new size_t[num_partitions]{};
    thread_states.reserve(num_threads);

    /* Set up the per-thread build states */
    for (size_t t = 0; t < num_threads; ++t)
    {
        thread_states.emplace_back(
            ThreadBuildState{
                ThreadAllocator(global_allocator),
                {}});

        auto &partitions = thread_states.back().partitions;
        partitions.reserve(num_partitions);

        for (size_t p = 0; p < num_partitions; ++p)
        {
            partitions.emplace_back(
                PartitionBuffer{
                    TupleAllocator(thread_states.back().allocator, sizeof(Tuple)),
                    0});
        }
    }

    /* Set up the precomputed tags */
    init_tags();
}
unchained_ht::~unchained_ht(){
    delete[] previous_counts;
    delete[] directory_raw;
    delete[] array;
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
void unchained_ht::build_insert(int32_t key, size_t value, size_t thread_id)
{
    /* If the table is already built, ignore */
    if (isBuilt)
        throw std::runtime_error("Hash table already built.");

    /* Get the Hash */
    uint64_t hash = hash_key(key);

    /* Get the partition */
    size_t partition = hash >> (64 - __builtin_ctzll(num_partitions));

    /* Allocate the tuple in the appropriate thread/partition buffer */
    PartitionBuffer &pb = thread_states[thread_id].partitions[partition];
    Tuple *t = static_cast<Tuple *>(pb.allocator.allocate_tuple());

    *t = {key, hash, value};
    pb.count++;
}
/* We prepare the hashtable to be finalized. This part needs to happen by one thread and that is why we have seperated the 2 functions*/
bool unchained_ht::prepare_build()
{
    /* If the table is already built, ignore */
   if (isBuilt) return false;

    // 1. Reset counts
    std::memset(previous_counts, 0, sizeof(size_t) * num_partitions);
    std::vector<size_t> partition_totals(num_partitions, 0);

    // 2. Aggregate counts across threads
    for (size_t t = 0; t < num_threads; ++t) {
        for (size_t p = 0; p < num_partitions; ++p) {
            partition_totals[p] += thread_states[t].partitions[p].count;
        }
    }

    // 3. Prefix sum for global array offsets
    size_t total = 0;
    for (size_t p = 0; p < num_partitions; ++p) {
        previous_counts[p] = total;
        total += partition_totals[p];
    }
    tuple_count = total;

    if (tuple_count == 0) {
        isBuilt = true;
        return false;
    }

    /* Allocate the directory */
    directory_size = next_power_of_2(static_cast<size_t>(tuple_count / load_factor) + 1);
    if(directory_size <= num_partitions) directory_size = next_power_of_2(num_partitions + 1);
    /* Calculate the shift */
    shift = 64 - __builtin_ctzll(directory_size);

    /* Set a directory with one extra slot, so that we get the starting bucket to be [dir[-1], dir[0]] */
    directory_raw = new uint64_t[directory_size + 1];
    std::memset(directory_raw, 0, sizeof(uint64_t) * (directory_size + 1));

    /* Set the new directory one element ahead, to have access to the dir[-1] */
    directory = directory_raw + 1;
    directory[-1] = 0;

    /* Allocate final contiguous storage */
    array = new Tuple[tuple_count];

    prepared = true;

    return true;
}

void unchained_ht::post_process_build(size_t tid, size_t partition){
    if(isBuilt && tuple_count == 0) return;
    if(!prepared) throw std::runtime_error("Hashtable was not prepared for build!");
    size_t prev_count = previous_counts[partition];
    size_t tuple_size = thread_states[0].partitions[0].allocator.get_tuple_size();
    /* We combine the partition tuples of all threads to 1 linked list, this is done so each thread processes its own partition without the need of synchronization */
    std::list<Chunk> connected_chunks{};
    for(size_t t = 0; t < num_threads; t++){
        thread_states[t].partitions[partition].allocator.connect_list(connected_chunks);
    }

    /* Step 1. Count tuples per slot and build Bloom filters */

    for(Chunk chunk : connected_chunks){
        const char *cur = chunk.begin;
        const char *end = chunk.end;
        while (cur < end)
        {
            const Tuple& tup = *reinterpret_cast<const Tuple*>(cur);
            uint64_t slot = tup.hash >> shift;
            directory[slot] += (1ULL << 16);

            directory[slot] |= compute_tag(tup.hash);
            cur += tuple_size;
        } 
    }

    /* Step 2. Exclusive prefix sum over counts to get starting indices */
    size_t running = prev_count;
    size_t k = 64 - shift;
    size_t start = (partition << k) / num_partitions;
    size_t end = ((partition + 1) << k) / num_partitions;
    for (size_t i = start; i < end; i++)
    {
       
        uint64_t count = directory[i] >> 16;                  /* number of tuples in this slot */
        uint16_t bloom = static_cast<uint16_t>(directory[i]); /* bloom filter */ 
        
        directory[i] = (running << 16) | bloom; /* store start index */
        running += count;                       /* update running total */ 
    }

    /* Step 3. Scatter tuples into their final positions, updating ends */

    for(Chunk chunk : connected_chunks){
        const char *cur = chunk.begin;
        const char *end = chunk.end;
        while (cur < end)
        {
            const Tuple& tup = *reinterpret_cast<const Tuple*>(cur);

            uint64_t slot = tup.hash >> shift;
            uint64_t pos  = directory[slot] >> 16;

            array[pos] = tup;
            
            directory[slot] += (1ULL << 16);
            cur += tuple_size;
        } 
    }

}
void unchained_ht::finalize_build(){
    isBuilt = true;
}
/* Probe function, returns vector of matching values */
std::vector<size_t>
unchained_ht::probe(int32_t key) const
{
    if (!isBuilt) throw std::runtime_error("Hash table not built.");
    if (tuple_count == 0 || directory == nullptr) return {}; // Complete safety

    std::vector<size_t> result;

    if (tuple_count == 0)
        return result;

    /* Compute hash and slot */
    uint64_t h = hash_key(key);
    uint64_t slot = h >> shift;

    /* Load directory entry */
    uint64_t entry = directory[slot];
    uint16_t bloom = static_cast<uint16_t>(entry);

    /* Bloom filter check, reject early if impossible match */
    if (!could_contain(bloom, h))
    {
        return result;
    }

    /* Range for this hash-prefix slot */
    uint64_t start_off = directory[slot - 1] >> 16;
    uint64_t end_off = directory[slot] >> 16;

    /* Get pointers to the beginning and end of the range */
    const Tuple *begin = array + start_off;
    const Tuple *end = array + end_off;

    /* Collect only matching tuples */
    for (auto p = begin; p < end; ++p)
        if (p->key == key)
        {
            result.push_back(p->value);
        }

    return result;
}
