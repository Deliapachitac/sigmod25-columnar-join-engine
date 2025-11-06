# Project Part 1

# Optimization testing
After running tests to see where the hashtable spends the most time, I saw that 99% of the time was spent at find, while a small fraction was spent in insert. This is to be expected as only a small fraction of the operations performed were inserts and most operations where finds. So I focused more on trying to make find as effecient as possible. Adding early stopping improved performance and I made sure to compute all variables before the main loop to avoid unecessary pointer dereferencing, and also passed the key as a reference to avoid the item being copied which would have been costly. A big factor in optimizing find would be the hash function. A good hash function was necessary to improve on the performance. Since most time was speant on search, ideally we want a hash function that has the best statistical properties, not necessarily the fastest one to compute, since we don't call it often and the program spends so little time, that it can safely be ignored. Using std::hash proved to be catastrophic because runtime would skyrocket to 10-20x the time it took to run the program with unordered map. This happens because std::hash does not perform good for open addressing, and finds end up taking O(n) due to bad spread of the entries. I chose to use FNV1a hash, 32 and 64 bit versions because it was very simple to implement and performed very well in terms of distribution. After running the program I found out that it ran marginally better than the unordered map, faster that cuckoo hashing and a bit slower that Hopscotch hashing, which was to be expected. Hopscotch was expected to be the fastest, since find, which takes up most of the time, only needs to search at the neighbourhood the item is hashed at, which is small so it is about O(H) worst case. While Robin hood for smaller hashtables may perform better, the fact that searches in Hopscotch don't get slower while the hashtable has more entries make hopscotch a better fit for a program that is as intensive in searches as this one. Also hopscotch benefits from better cache locality. Cuckoo hashing has constant time find operations at O(1), so while it would be expected to be the fastest, in fact it is very 

## Robin Hood Hashing
*Implemented by Iasonas Karaprodromidis (sdi2200064)*

---

### Implementation Details

#### Data Storage

The hash map stores each bucket in a `struct` called `data`, which contains all the information needed for the Robin Hood algorithm. Each `data` struct includes:

- `kv`: a `std::pair` holding the key and value. Using a pair simplifies iterator implementation, allowing them to return elements directly as key-value pairs.  
- `psl`: the probe sequence length.
- `is_empty`: a boolean flag indicating whether the bucket is currently occupied.

All buckets are stored in a `std::vector<data>`, allowing easy resizing during rehashing.

#### Hashing
For hashing, I implemented the `FNV-1a` algorithm in both its 32-bit and 64-bit variants. I chose `FNV-1a` because it provides an excellent balance between speed and hash value distribution, effectively minimizing collisions. Since performance was a primary goal of this project, it was a natural choice.

Additionally, the hash table size is always a power of two rather than a prime number. This simplifies resizing—by simply doubling the capacity—and improves performance, as it allows the use of a bitmask instead of the slower modulo operation for computing indices.

#### Iterator
I implemented two iterators—a mutable one and a constant one—to keep the design consistent with the STL. This ensures that functions returning iterators can be used flexibly in both mutable and read-only contexts.

#### Insert / Emplace
I implemented both `insert` and `emplace` (since the program uses `emplace`). The key difference is that `emplace` constructs the object in place, while `insert` first searches for the key.  

- If the key already exists, it returns an iterator to the existing element and `false`.  
- Otherwise, it checks the load factor and triggers a rehash if necessary.  

The insertion follows the Robin Hood strategy: it attempts to place the key at its ideal position (`PSL = 0`), displacing existing keys with smaller probe sequence lengths until an empty bucket is found. Finally, the function inserts the key and returns an iterator to the new element along with `true`.

#### Find

The `find` function searches for a key in the hash map using Robin Hood linear probing. It starts at the key's ideal bucket (computed by the hash function) and increments the probe sequence length (PSL) while scanning.  

- If an empty bucket is encountered or the current bucket's PSL is less than the search PSL, the search stops early.  
- If the key is found, it returns an iterator to that element; otherwise, it returns `end()`.

#### Unit Tests

I implemented comprehensive unit tests covering all core functionalities of the hashmap. These tests verify that basic operations—such as insertion, emplace, search, iteration, and clearing—work correctly under normal conditions.  

Additionally, the tests include stress scenarios with a large number of insertions to ensure proper handling of collisions, automatic rehashing, and maintenance of the Robin Hood property. Special cases, such as using 64 bit integers, string and double keys, as well as const iterators, are also validated to ensure the hashmap behaves correctly and consistently in all expected usage patterns.

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


# Cuckoo Hashing

Name: Delia-Maria
Surname: Pachitac
ID: 1115202200125

# Implementation Details
The implementation of cuckoo hashing contains a struct Data:
- kv: a pair that contains the key and the value.
- occupied: a variable indicating whether the bucket is occupied.

The buckets are stored in two vectors, T1 and T2 (two arrays with the two different capacities).
I choose to change the shared capacity into two dofferent ones for each table because th rehash was triggered based on the combined load factor of both tables. For example :
Imagine the following situation:
    -T1 has 16 elements and reaches 100% of its capacity
    -T2 is  empty having 0 elements 
In the old design the total load factory for both tables would be at 50% and we would double the size of both tables and reinsertall the elements . But in the T2 is still has plenty of free space and we may  pay an expensive cost for doubling T2. 


# Hashing
For hashing, we have two functions: h1 and h2.
These functions are based on the FNV-1a algorithm for different key types (int32, int64, double, string).
The table size is a power of two, allowing the use of bitmasking instead of modulo, which increases speed.

# Emplace
1. Check before inserting each table separetly if they have reached the load factory and rehash 

2. Compute h1 and h2 for the key

3. Try inserting (key, value) in T1[h1(key)]      
    - If empty -> place it there        
    - If occupied -> check for duplicates

4. If it is a duplicate  , else  swap the existing element and try to move it to the second table based on the h2

5. Continue swapping until either an empty spot is found or a cycle is detected.

6. If a cycle occurs -> rehash the table (double capacity for both tables and reinsert all elements).

# Find
The find function checks the two possible positions:
T1[h1(key)] or T2[h2(key)].
If the key is found, it returns an iterator to that position; otherwise, it returns end().

# Test Coverage Summary
- Constructors  (Default constructorand size constructor)
- Insertion with collisions
- Insertion with cycle Detection and rehashing
- Duplicate Key Handling
- Find Function
- Clear Function
- Iterators
- Multiple Key Types (int32, int64, double, std::string)

# Complexity 
After implementing and executing the 3 hashing algorithms, we realized that in the cuckoo hash when a  cycle occurs or the tables have reached the load factor, several swaps plus possible rehashing are required. This extra movement makes insertions slower compared to algorithms like robin hood or hopscotch. Also the cuckoo hashing accesses two separate tables, which requires more memory access and  it slows down the program. On the other side the other two algorithms operate in a single contiguous array making them faster.
