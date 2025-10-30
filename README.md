# HOPSCOTCH ALGORITHM

**Implemented by:** Kyriakos Kalafatsis 1115202200058

## Introduction

The Hopscotch hash table algorithm was implemented to be used with different key and value as a template, and the whole implementation exists in the hopscotch.h file.
The other important file is the hopscotch_unit_tests.cpp, where all of the tests for the algorithms were written.

## Implementation
### Bitmaps - Neighborhoods

The main bucket for each cell of the table consists of the pair Key-Value for the data, as well as a **64-bit unsigned integer**, which represents the **neighborhood** of the cell.
Neighborhoods do not necessarily have to be 64 bits, however they cannot exceed that number (it would not be beneficial anyway, since we want them to fit within a cache line).

Each neighborhood bitmap uses the most significant bits (MSBs) to represent the closest cells to the home bucket.
For example, if the neighborhood of the cell with index 5 is 100100010, it means that there are elements whose hash is 5 stored in indices 5, 8, and 12.

This bit-based design allows for very fast lookup operations in the find() function, as bitwise operations are highly efficient.

### Iterator

An iterator class was implemented to allow for simple traversal of the table.
The iterator skips over empty buckets and only returns references to valid Key-Value pairs.
It supports prefix increment (++it) and dereferencing (*it and ->) operations, as well as comparison operators for equality and inequality.

### Hashing

A custom hash function was used, to make the program a bit more efficient. It is a hybrid between multiple hash functions, depending on the type.

### Constructors

The class provides three constructors for flexibility:

* **Default constructor:** Initializes a table with a default size (1024 buckets) and a default neighborhood length (32).

* **Size constructor:** Allows initialization with a given table size, using the default neighborhood length.

* **Size + Neighborhood constructor:** Provides full control over both parameters, with an automatic adjustment ensuring the neighborhood never exceeds 64 bits or the table size.

Internally, the constructor also adjusts the capacity to the next power of two to optimize hashing and bit masking operations. This is very important as we can use bit operations instead of the mod, which saves a lot of 
computing time.

### Emplace

The algorithm used for the emplace follows the exercise directly. Some interesting points include: 
* A function checking if the neighborhood is full (= checking if all the bits are 1)
* The main Hopscotching algorithm, in a while loop ensuring correct placement of the item in its neighborhood
* Rehashing in 3 spots, practically the load factor limit is never met, as the size is adjusted in the constructor if given

### Find

The find function uses the bitmap of the home bucket to efficiently locate elements.
Instead of iterating through all possible nearby buckets, it checks for the bits in a neighborhood, and only checks those with an item (set bits).

For each set bit, the algorithm computes the actual index, checks if the key matches, and returns an iterator if found.
This approach ensures that search operations remain extremely fast, often requiring only a few bitwise checks and one or two memory lookups.

### Rehash

Rehashing, as mentioned, occurs automatically when:

* The table exceeds the defined load factor limit

* The home bucket’s neighborhood becomes full during insertion.

* The Hopscotch algorithm cannot find any valid spots to move items


### Unit Tests

Comprehensive unit tests were written in the hopscotch_unit_tests.cpp file.
The tests cover:

* Basic insertions and lookups

* Rehashing behavior

* Iterator functionality

* Type checking for all possible types

* Stress Tests