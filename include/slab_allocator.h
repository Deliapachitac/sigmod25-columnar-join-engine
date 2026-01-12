#ifndef SLAB_ALLOCATOR_H
#define SLAB_ALLOCATOR_H

#include <cstdlib>
#include <vector>
#include <stdexcept>

#define DEFAULT_TUPLE_SIZE 24                         /* Default size of a tuple in bytes (key, hash and value) */
#define LARGE_CHUNK_SIZE (DEFAULT_TUPLE_SIZE * 10000) /* Allocate in chunks of 10k tuples */
#define SMALL_CHUNK_SIZE (DEFAULT_TUPLE_SIZE * 1000)  /* Allocate in chunks of 1k tuples */

/* LEVEL 1: The GlobalAllocator allocates large chunks of memory and manages them efficiently */
class GlobalAllocator
{
private:
    std::vector<void *> chunks_; /* Vector to hold allocated chunks */
public:
    /* Allocate a new Large chunk of memory */
    void *allocate()
    {
        void *ptr = std::malloc(LARGE_CHUNK_SIZE);
        if (!ptr)
        {
            throw std::bad_alloc();
        }
        chunks_.push_back(ptr);
        return ptr;
    }

    /* Destructor to free all allocated chunks */
    ~GlobalAllocator()
    {
        for (void *chunk : chunks_)
        {
            std::free(chunk);
        }
    }
};

/* LEVEL 2: The ThreadAllocator allocates smaller chunks of memory for each thread from the GlobalAllocator */
class ThreadAllocator
{
private:
    GlobalAllocator &global_allocator_; /* Reference to the global allocator */

    char *current_large_chunk_;         /* Current large chunk */
    size_t large_chunk_offset_;         /* Offset inside the large chunk */

public:
    ThreadAllocator(GlobalAllocator &global_allocator)
        : global_allocator_(global_allocator),
          current_large_chunk_(nullptr),
          large_chunk_offset_(LARGE_CHUNK_SIZE) {}

    /* Allocate a new Small chunk of memory from the GlobalAllocator */
    void *allocate_small_chunk()
    {
        /* Allocate a new large chunk if needed */
        if (large_chunk_offset_ + SMALL_CHUNK_SIZE > LARGE_CHUNK_SIZE)
        {
            current_large_chunk_ =
                static_cast<char *>(global_allocator_.allocate());
            large_chunk_offset_ = 0;
        }

        /* Carve out a small chunk */
        void *chunk = current_large_chunk_ + large_chunk_offset_;
        large_chunk_offset_ += SMALL_CHUNK_SIZE;
        return chunk;
    }
};

/* LEVEL 3: The TupleAllocator allocates memory for individual tuples from the ThreadAllocator */
class TupleAllocator
{
private:
    ThreadAllocator &thread_allocator_; /* Reference to the thread allocator */

    char *current_chunk_;               /* Current small chunk */
    size_t current_offset_;             /* Offset inside the small chunk */

public:
    TupleAllocator(ThreadAllocator &thread_allocator)
        : thread_allocator_(thread_allocator),
          current_chunk_(nullptr),
          current_offset_(SMALL_CHUNK_SIZE) {}

    /* Allocate memory for a single tuple */
    void *allocate_tuple()
    {
        /* Allocate a new small chunk if needed */
        if (current_offset_ + DEFAULT_TUPLE_SIZE > SMALL_CHUNK_SIZE)
        {
            current_chunk_ =
                static_cast<char *>(thread_allocator_.allocate_small_chunk());
            current_offset_ = 0;
        }

        /* Bump allocation for one tuple */
        void *ptr = current_chunk_ + current_offset_;
        current_offset_ += DEFAULT_TUPLE_SIZE;
        return ptr;
    }
};

#endif // SLAB_ALLOCATOR_H
