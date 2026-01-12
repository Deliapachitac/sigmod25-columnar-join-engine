#ifndef SLAB_ALLOCATOR_H
#define SLAB_ALLOCATOR_H

#include <cstdlib>
#include <vector>

#define DEFAULT_TUPLE_SIZE 24                         /* Default size of a tuple in bytes (key, hash and value) */
#define LARGE_CHUNK_SIZE (DEFAULT_TUPLE_SIZE * 10000) /* Allocate in chunks of 10k tuples */
#define SMALL_CHUNK_SIZE (DEFAULT_TUPLE_SIZE * 1000)  /* Allocate in chunks of 1k tuples */

class SlabAllocator
{
public:

    /* Constructor, initializes the slab allocator with a specified tuple size */
    SlabAllocator(size_t tuple_size = DEFAULT_TUPLE_SIZE)
        : tuple_size_(tuple_size), current_chunk_offset_(0)
    {
        /* Allocate the initial large chunk */
        allocate_new_chunk(LARGE_CHUNK_SIZE);
    }

    /* Allocate memory for a new tuple, checking if a new chunk is needed */
    void *allocate()
    {
        if (current_chunk_offset_ + tuple_size_ > current_chunk_size_)
        {
            allocate_new_chunk(SMALL_CHUNK_SIZE);
        }
        void *ptr = static_cast<char *>(current_chunk_) + current_chunk_offset_;
        current_chunk_offset_ += tuple_size_;
        return ptr;
    }

private:

    /* Allocate a new chunk of memory */
    void allocate_new_chunk(size_t chunk_size)
    {
        current_chunk_ = malloc(chunk_size);
        if (!current_chunk_)
        {
            throw std::bad_alloc();
        }
        chunks_.push_back(current_chunk_);
        current_chunk_size_ = chunk_size;
        current_chunk_offset_ = 0;
    }
    size_t tuple_size_;
    void *current_chunk_;
    size_t current_chunk_size_;
    size_t current_chunk_offset_;
    std::vector<void *> chunks_;
};

#endif // SLAB_ALLOCATOR_H
