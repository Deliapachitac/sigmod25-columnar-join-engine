# Project Part 1

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

### Optimization testing

After running tests to see where the hashtable spends the most time, I saw that 99% of the time was spent at find, while a small fraction was spent in insert. This is to be expected as only a small fraction of the operations performed were inserts and most operations where finds. So I focused more on trying to make find as effecient as possible. Adding early stopping improved performance and I made sure to compute all variables before the main loop to avoid unecessary pointer dereferencing, and also passed the key as a reference to avoid the item being copied which would have been costly. A big factor in optimizing find would be the hash function. A good hash function was necessary to improve on the performance. Since most time was speant on search, ideally we want a hash function that has the best statistical properties, not necessarily the fastest one to compute, since we don't call it often and the program spends so little time, that it can safely be ignored. Using std::hash proved to be catastrophic because runtime would skyrocket to 10-20x the time it took to run the program with unordered map. This happens because std::hash does not perform good for open addressing, and finds end up taking O(n) due to bad spread of the entries. I chose to use FNV1a hash, 32 and 64 bit versions because it was very simple to implement and performed very well in terms of distribution. After running the program I found out that it ran marginally better than the unordered map, faster that cuckoo hashing and a bit slower that Hopscotch hashing, which was to be expected. Hopscotch was expected to be the fastest, since find, which takes up most of the time, only needs to search at the neighbourhood the item is hashed at, which is small so it is about O(H) worst case. While Robin hood for smaller hashtables may perform better, the fact that searches in Hopscotch don't get slower while the hashtable has more entries make hopscotch a better fit for a program that is as intensive in searches as this one. Also hopscotch benefits from better cache locality. Cuckoo hashing has constant time find operations at O(1), so while it would be expected to be the fastest, in fact it is very 

