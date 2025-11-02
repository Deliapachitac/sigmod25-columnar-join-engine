## Project Part 1
Cuckoo Hashing

Name: Delia-Maria
Surname: Pachitac
ID: 1115202200125

# Implementation Details
The implementation of cuckoo hashing contains a struct Data:
- kv: a pair that contains the key and the value.
- occupied: a variable indicating whether the bucket is occupied.

The buckets are stored in two vectors, T1 and T2 (two arrays with the same capacity).

# Hashing
For hashing, we have two functions: h1 and h2.
These functions are based on the FNV-1a algorithm for different key types (int32, int64, double, string).
The table size is a power of two, allowing the use of bitmasking instead of modulo, which increases speed.

# Emplace
There are two tables, T1 and T2. For each key, there are two hash functions, h1 and h2, giving two possible positions.

During insertion, we try to place the (key, value) pair in the position given by h1 in T1.
If the position is empty, we insert it.

If it is occupied, we first check for a duplicate key. If it is not a duplicate, we swap the existing element and try to place it in T2 at the position given by h2.
If that position is also occupied, we continue performing swaps until an empty bucket is found or a cycle is detected.

If a cycle is detected or if the number of entries becomes too large, a rehash occurs:
the capacity is doubled, and all elements are reinserted into the new table.

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
