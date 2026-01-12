#ifndef SLAB_ALLOCATOR_H
#define SLAB_ALLOCATOR_H

#include <cstdlib>
#include <vector>
#include <stdexcept>

#define LARGE_CHUNK_SIZE (1024 * 1024) /* Allocate in chunks of 1MB */
#define SMALL_CHUNK_SIZE (64 * 1024)   /* Allocate small chunks of 64KB */

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

    char *current_large_chunk_; /* Current large chunk */
    size_t large_chunk_offset_; /* Offset inside the large chunk */

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

    struct Chunk
    {
        char *begin; /* Start of small chunk */
        char *end;   /* End of written tuples */
    };

    std::vector<Chunk> chunks_; /* All chunks owned by this allocator */

    char *current_chunk_;   /* Current small chunk */
    size_t current_offset_; /* Offset inside the small chunk */

    size_t tuple_size_;

public:
    TupleAllocator(ThreadAllocator &thread_allocator, size_t tuple_size)
        : thread_allocator_(thread_allocator),
          current_chunk_(nullptr),
          current_offset_(SMALL_CHUNK_SIZE)
    {
        tuple_size_ =
            (tuple_size + alignof(std::max_align_t) - 1) &
            ~(alignof(std::max_align_t) - 1);
    }

    /* Allocate memory for a single tuple */
    void *allocate_tuple()
    {
        /* Allocate a new small chunk if needed */
        if (current_offset_ + tuple_size_ > SMALL_CHUNK_SIZE)
        {
            current_chunk_ =
                static_cast<char *>(thread_allocator_.allocate_small_chunk());

            chunks_.push_back(
                Chunk{current_chunk_, current_chunk_});

            current_offset_ = 0;
        }

        /* Bump allocation for one tuple */
        char *ptr = current_chunk_ + current_offset_;
        current_offset_ += tuple_size_;

        /* Advance end pointer of current chunk */
        chunks_.back().end = current_chunk_ + current_offset_;

        return ptr;
    }

    /* Function to iterate over all allocated tuples and apply a function */
    template <typename Fn>
    void for_each_tuple(Fn &&fn) const
    {
        for (const Chunk &chunk : chunks_)
        {
            const char *cur = chunk.begin;
            const char *end = chunk.end;

            while (cur < end)
            {
                fn(cur); // pass raw pointer
                cur += tuple_size_;
            }
        }
    }
};

#endif // SLAB_ALLOCATOR_H
